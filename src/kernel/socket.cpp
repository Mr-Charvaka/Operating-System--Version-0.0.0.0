#include "socket.h"
#include "../drivers/serial.h"
#include "../include/string.h"
#include "heap.h"
#include "memory.h"
#include "process.h"

#define MAX_SOCKETS 64
static socket_t *sockets[MAX_SOCKETS];

void socket_init() {
  for (int i = 0; i < MAX_SOCKETS; i++)
    sockets[i] = 0;
}

static socket_t *alloc_socket() {
  for (int i = 0; i < MAX_SOCKETS; i++) {
    if (!sockets[i]) {
      sockets[i] = (socket_t *)kmalloc(sizeof(socket_t));
      if (!sockets[i]) {
        serial_log("SOCKET ERROR: OOM in alloc_socket");
        return 0;
      }
      memset(sockets[i], 0, sizeof(socket_t));
      sockets[i]->id = i;
      sockets[i]->buffer = (uint8_t *)kmalloc(4096);
      if (!sockets[i]->buffer) {
        serial_log("SOCKET ERROR: OOM for buffer in alloc_socket");
        kfree(sockets[i]);
        sockets[i] = 0;
        return 0;
      }
      return sockets[i];
    }
  }
  serial_log("SOCKET ERROR: Max sockets reached");
  return 0;
}

uint32_t socket_read(vfs_node_t *node, uint32_t offset, uint32_t size,
                     uint8_t *buffer) {
  (void)offset;
  socket_t *sock = (socket_t *)node->impl;
  if (!sock || sock->state != SOCKET_CONNECTED)
    return 0;

  uint32_t read_bytes = 0;
  while (read_bytes < size) {
    if (sock->head == sock->tail) {
      if (read_bytes > 0)
        break;
      // Block
      current_process->state = PROCESS_WAITING;
      schedule();
      continue;
    }
    buffer[read_bytes++] = sock->buffer[sock->head];
    sock->head = (sock->head + 1) % 4096;
  }

  // Wake up writers
  process_t *p = ready_queue;
  if (p) {
    process_t *start = p;
    do {
      if (p->state == PROCESS_WAITING)
        p->state = PROCESS_READY;
      p = p->next;
    } while (p && p != start);
  }

  return read_bytes;
}

uint32_t socket_write(vfs_node_t *node, uint32_t offset, uint32_t size,
                      uint8_t *buffer) {
  (void)offset;
  socket_t *sock = (socket_t *)node->impl;
  if (!sock || sock->state != SOCKET_CONNECTED || !sock->peer)
    return 0;

  socket_t *peer = sock->peer;
  uint32_t written = 0;
  while (written < size) {
    uint32_t next_tail = (peer->tail + 1) % 4096;
    if (next_tail == peer->head) {
      if (written > 0)
        break;
      // Block
      current_process->state = PROCESS_WAITING;
      schedule();
      continue;
    }
    peer->buffer[peer->tail] = buffer[written++];
    peer->tail = next_tail;
  }

  // Wake up readers on the peer side
  process_t *p = ready_queue;
  if (p) {
    process_t *start = p;
    do {
      if (p->state == PROCESS_WAITING)
        p->state = PROCESS_READY;
      p = p->next;
    } while (p && p != start);
  }

  return written;
}

void socket_close(vfs_node_t *node) {
  socket_t *sock = (socket_t *)(uintptr_t)node->impl;
  if (!sock)
    return;

  sock->state = SOCKET_CLOSED;

  // Remove from global array
  for (int i = 0; i < MAX_SOCKETS; i++) {
    if (sockets[i] == sock) {
      sockets[i] = 0;
      break;
    }
  }

  // Free resources
  if (sock->buffer) {
    kfree(sock->buffer);
  }
  kfree(sock);
}

int sys_socket(int domain, int type, int protocol) {
  if (domain != AF_UNIX || type != SOCK_STREAM)
    return -1;

  socket_t *sock = alloc_socket();
  if (!sock) {
    serial_log("SOCKET ERROR: alloc_socket failed");
    return -1;
  }

  sock->domain = domain;
  sock->type = type;
  sock->state = SOCKET_FREE;

  vfs_node_t *node = (vfs_node_t *)kmalloc(sizeof(vfs_node_t));
  if (!node) {
    serial_log("SOCKET ERROR: OOM for vfs_node");
    // TODO: free sock
    return -1;
  }
  memset(node, 0, sizeof(vfs_node_t));
  strcpy(node->name, "socket");
  node->impl = (uint32_t)(uintptr_t)sock;
  node->read = socket_read;
  node->write = socket_write;
  node->close = socket_close;
  node->flags = VFS_SOCKET;
  node->ref_count = 1;

  for (int i = 0; i < MAX_PROCESS_FILES; i++) {
    if (!current_process->fd_table[i]) {
      current_process->fd_table[i] = node;
      return i;
    }
  }

  serial_log("SOCKET ERROR: Process FD table full");
  return -1;
}

int sys_bind(int sockfd, const char *path) {
  if (sockfd < 0 || sockfd >= MAX_PROCESS_FILES)
    return -1;
  vfs_node_t *node = current_process->fd_table[sockfd];
  if (!node || node->flags != VFS_SOCKET)
    return -1;

  socket_t *sock = (socket_t *)node->impl;
  strcpy(sock->bind_path, path);
  sock->state = SOCKET_BOUND;
  serial_log("SOCKET: sys_bind bound socket to path:");
  serial_log(sock->bind_path);

  // In a real system we'd add this to VFS, but for now we'll just store it in
  // the array
  return 0;
}

int sys_connect(int sockfd, const char *path) {
  if (sockfd < 0 || sockfd >= MAX_PROCESS_FILES) {
    serial_log("SOCKET ERROR: Invalid sockfd");
    return -1;
  }
  vfs_node_t *node = current_process->fd_table[sockfd];
  if (!node) {
    serial_log("SOCKET ERROR: Node is null");
    return -1;
  }
  if (node->flags != VFS_SOCKET) {
    serial_log_hex("SOCKET ERROR: Node flags mismatch. Expected SOCKET, got: ",
                   node->flags);
    return -1;
  }
  socket_t *sock = (socket_t *)node->impl;

  serial_log("SOCKET: sys_connect looking for path:");
  serial_log(path);

  // Find the bound socket
  socket_t *server = 0;
  for (int i = 0; i < MAX_SOCKETS; i++) {
    if (sockets[i] && sockets[i]->state == SOCKET_BOUND) {
      serial_log("SOCKET: Checking bound socket:");
      serial_log(sockets[i]->bind_path);
      if (strcmp(sockets[i]->bind_path, path) == 0) {
        server = sockets[i];
        break;
      }
    }
  }

  if (!server)
    return -1;

  // Add to server's backlog
  if (server->backlog_count < 8) {
    server->backlog[server->backlog_count++] = sock;
    sock->state = SOCKET_CONNECTING;

    // Wake up the server! (It might be waiting in accept)
    process_t *p = ready_queue;
    if (p) {
      process_t *start = p;
      do {
        if (p->state == PROCESS_WAITING)
          p->state = PROCESS_READY;
        p = p->next;
      } while (p && p != start);
    }

    // Block until connected
    while (sock->state == SOCKET_CONNECTING) {
      current_process->state = PROCESS_WAITING;
      schedule();
    }
    return 0;
  }

  return -1;
}

int sys_accept(int sockfd) {
  if (sockfd < 0 || sockfd >= MAX_PROCESS_FILES)
    return -1;
  vfs_node_t *node = current_process->fd_table[sockfd];
  if (!node || node->flags != VFS_SOCKET)
    return -1;
  socket_t *server = (socket_t *)(uintptr_t)node->impl;

  while (server->backlog_count == 0) {
    current_process->state = PROCESS_WAITING;
    schedule();
  }

  socket_t *client = server->backlog[0];
  for (int i = 0; i < server->backlog_count - 1; i++)
    server->backlog[i] = server->backlog[i + 1];
  server->backlog_count--;

  // Create a new socket for the connection
  socket_t *conn = alloc_socket();
  conn->state = SOCKET_CONNECTED;
  conn->peer = client;
  client->peer = conn;
  client->state = SOCKET_CONNECTED;

  vfs_node_t *conn_node = (vfs_node_t *)kmalloc(sizeof(vfs_node_t));
  memset(conn_node, 0, sizeof(vfs_node_t));
  strcpy(conn_node->name, "socket_conn");
  conn_node->impl = (uint32_t)(uintptr_t)conn;
  conn_node->read = socket_read;
  conn_node->write = socket_write;
  conn_node->close = socket_close;
  conn_node->flags = VFS_SOCKET;
  conn_node->ref_count = 1;

  for (int i = 0; i < MAX_PROCESS_FILES; i++) {
    if (!current_process->fd_table[i]) {
      current_process->fd_table[i] = conn_node;

      // Wake up the client!
      process_t *p = ready_queue;
      if (p) {
        process_t *start = p;
        do {
          if (p->state == PROCESS_WAITING)
            p->state = PROCESS_READY;
          p = p->next;
        } while (p && p != start);
      }

      return i;
    }
  }

  return -1;
}
