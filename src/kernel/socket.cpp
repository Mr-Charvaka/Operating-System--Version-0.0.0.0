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
  strncpy(sock->bind_path, path, 127);
  sock->bind_path[127] = 0;
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
  serial_log_hex("SOCKET: Global sockets array at ", (uint32_t)sockets);
  for (int i = 0; i < MAX_SOCKETS; i++) {
    if (sockets[i]) {
      serial_log_hex("SOCKET: i=", i);
      serial_log_hex("  State: ", sockets[i]->state);
      serial_log("  Path: ");
      serial_log(sockets[i]->bind_path);
    }
  }

  char kpath[128];
  strncpy(kpath, path, 127);
  kpath[127] = 0;

  socket_t *sock = (socket_t *)node->impl;
  serial_log("SOCKET: sys_connect looking for path:");
  serial_log(kpath);

  // Find the bound socket
  socket_t *server = 0;
  for (int i = 0; i < MAX_SOCKETS; i++) {
    if (sockets[i] && sockets[i]->state == SOCKET_BOUND) {
      serial_log("SOCKET: Checking bound socket:");
      serial_log(sockets[i]->bind_path);
      if (strcmp(sockets[i]->bind_path, kpath) == 0) {
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

// ============================================================================
// Extended Socket Functions for POSIX Compliance
// ============================================================================

// Listen for connections
int sys_listen(int sockfd, int backlog) {
  if (sockfd < 0 || sockfd >= MAX_PROCESS_FILES)
    return -1;
  vfs_node_t *node = current_process->fd_table[sockfd];
  if (!node || node->flags != VFS_SOCKET)
    return -1;
  socket_t *sock = (socket_t *)(uintptr_t)node->impl;

  if (sock->state != SOCKET_BOUND)
    return -1; // Must be bound first

  sock->state = SOCKET_LISTENING;
  sock->max_backlog = (backlog > 0) ? backlog : 5;
  if (sock->max_backlog > MAX_BACKLOG)
    sock->max_backlog = MAX_BACKLOG;

  serial_log("SOCKET: Now listening");
  return 0;
}

// Send data (same as write but with flags)
ssize_t sys_send(int sockfd, const void *buf, size_t len, int flags) {
  (void)flags; // Flags not fully implemented

  if (sockfd < 0 || sockfd >= MAX_PROCESS_FILES || !buf)
    return -1;
  vfs_node_t *node = current_process->fd_table[sockfd];
  if (!node || node->flags != VFS_SOCKET)
    return -1;

  return socket_write(node, 0, len, (uint8_t *)buf);
}

// Receive data (same as read but with flags)
ssize_t sys_recv(int sockfd, void *buf, size_t len, int flags) {
  (void)flags; // Flags not fully implemented

  if (sockfd < 0 || sockfd >= MAX_PROCESS_FILES || !buf)
    return -1;
  vfs_node_t *node = current_process->fd_table[sockfd];
  if (!node || node->flags != VFS_SOCKET)
    return -1;

  return socket_read(node, 0, len, (uint8_t *)buf);
}

// Send datagram (for UDP-style sockets)
ssize_t sys_sendto(int sockfd, const void *buf, size_t len, int flags,
                   const void *dest_addr, uint32_t addrlen) {
  (void)dest_addr;
  (void)addrlen;
  // For UNIX domain sockets, sendto works like send
  return sys_send(sockfd, buf, len, flags);
}

// Receive datagram
ssize_t sys_recvfrom(int sockfd, void *buf, size_t len, int flags,
                     void *src_addr, uint32_t *addrlen) {
  (void)src_addr;
  (void)addrlen;
  // For UNIX domain sockets, recvfrom works like recv
  return sys_recv(sockfd, buf, len, flags);
}

// Get local socket name
int sys_getsockname(int sockfd, void *addr, uint32_t *addrlen) {
  if (sockfd < 0 || sockfd >= MAX_PROCESS_FILES)
    return -1;
  vfs_node_t *node = current_process->fd_table[sockfd];
  if (!node || node->flags != VFS_SOCKET)
    return -1;
  socket_t *sock = (socket_t *)(uintptr_t)node->impl;

  if (!addr || !addrlen)
    return -1;

  // Return socket address (simplified - just path for UNIX sockets)
  struct sockaddr_un {
    uint16_t sun_family;
    char sun_path[108];
  } *un_addr = (struct sockaddr_un *)addr;

  un_addr->sun_family = AF_UNIX;
  strncpy(un_addr->sun_path, sock->path, 107);
  un_addr->sun_path[107] = 0;

  *addrlen = sizeof(struct sockaddr_un);
  return 0;
}

// Get peer socket name
int sys_getpeername(int sockfd, void *addr, uint32_t *addrlen) {
  if (sockfd < 0 || sockfd >= MAX_PROCESS_FILES)
    return -1;
  vfs_node_t *node = current_process->fd_table[sockfd];
  if (!node || node->flags != VFS_SOCKET)
    return -1;
  socket_t *sock = (socket_t *)(uintptr_t)node->impl;

  if (!sock->peer || sock->state != SOCKET_CONNECTED)
    return -1; // Not connected

  if (!addr || !addrlen)
    return -1;

  // Return peer address
  struct sockaddr_un {
    uint16_t sun_family;
    char sun_path[108];
  } *un_addr = (struct sockaddr_un *)addr;

  un_addr->sun_family = AF_UNIX;
  strncpy(un_addr->sun_path, sock->peer->path, 107);
  un_addr->sun_path[107] = 0;

  *addrlen = sizeof(struct sockaddr_un);
  return 0;
}

// Socket option flags
#define SO_DEBUG 1
#define SO_REUSEADDR 2
#define SO_KEEPALIVE 9
#define SO_BROADCAST 6
#define SO_LINGER 13
#define SO_RCVBUF 8
#define SO_SNDBUF 7
#define SO_RCVTIMEO 20
#define SO_SNDTIMEO 21
#define SO_ERROR 4
#define SO_TYPE 3

#define SOL_SOCKET 1

// Set socket options
int sys_setsockopt(int sockfd, int level, int optname, const void *optval,
                   uint32_t optlen) {
  (void)level;
  (void)optval;
  (void)optlen;

  if (sockfd < 0 || sockfd >= MAX_PROCESS_FILES)
    return -1;
  vfs_node_t *node = current_process->fd_table[sockfd];
  if (!node || node->flags != VFS_SOCKET)
    return -1;

  // Socket options are not fully implemented
  // Just return success for common options
  switch (optname) {
  case SO_REUSEADDR:
  case SO_KEEPALIVE:
  case SO_BROADCAST:
  case SO_LINGER:
  case SO_RCVBUF:
  case SO_SNDBUF:
  case SO_RCVTIMEO:
  case SO_SNDTIMEO:
    return 0;
  default:
    return -1;
  }
}

// Get socket options
int sys_getsockopt(int sockfd, int level, int optname, void *optval,
                   uint32_t *optlen) {
  (void)level;

  if (sockfd < 0 || sockfd >= MAX_PROCESS_FILES)
    return -1;
  vfs_node_t *node = current_process->fd_table[sockfd];
  if (!node || node->flags != VFS_SOCKET)
    return -1;
  socket_t *sock = (socket_t *)(uintptr_t)node->impl;

  if (!optval || !optlen)
    return -1;

  switch (optname) {
  case SO_ERROR: {
    if (*optlen >= sizeof(int)) {
      *(int *)optval = 0; // No pending error
      *optlen = sizeof(int);
    }
    return 0;
  }
  case SO_TYPE: {
    if (*optlen >= sizeof(int)) {
      *(int *)optval = sock->type;
      *optlen = sizeof(int);
    }
    return 0;
  }
  case SO_RCVBUF:
  case SO_SNDBUF: {
    if (*optlen >= sizeof(int)) {
      *(int *)optval = 4096; // Our buffer size
      *optlen = sizeof(int);
    }
    return 0;
  }
  default:
    return -1;
  }
}

// Shutdown socket
#define SHUT_RD 0
#define SHUT_WR 1
#define SHUT_RDWR 2

int sys_shutdown(int sockfd, int how) {
  if (sockfd < 0 || sockfd >= MAX_PROCESS_FILES)
    return -1;
  vfs_node_t *node = current_process->fd_table[sockfd];
  if (!node || node->flags != VFS_SOCKET)
    return -1;
  socket_t *sock = (socket_t *)(uintptr_t)node->impl;

  switch (how) {
  case SHUT_RD:
    // Disable reading - mark buffer as full
    sock->head = sock->tail;
    break;
  case SHUT_WR:
    // Disable writing - disconnect peer awareness
    if (sock->peer)
      sock->peer->peer = 0;
    break;
  case SHUT_RDWR:
    sock->head = sock->tail;
    if (sock->peer)
      sock->peer->peer = 0;
    break;
  default:
    return -1;
  }

  return 0;
}

// Pair of connected sockets (for IPC)
int sys_socketpair(int domain, int type, int protocol, int sv[2]) {
  (void)domain;
  (void)type;
  (void)protocol;

  if (!sv)
    return -1;

  // Create two connected sockets
  socket_t *sock1 = alloc_socket();
  socket_t *sock2 = alloc_socket();

  if (!sock1 || !sock2) {
    if (sock1)
      sockets[sock1->id] = 0;
    if (sock2)
      sockets[sock2->id] = 0;
    return -1;
  }

  sock1->state = SOCKET_CONNECTED;
  sock2->state = SOCKET_CONNECTED;
  sock1->peer = sock2;
  sock2->peer = sock1;

  // Create VFS nodes
  vfs_node_t *node1 = (vfs_node_t *)kmalloc(sizeof(vfs_node_t));
  vfs_node_t *node2 = (vfs_node_t *)kmalloc(sizeof(vfs_node_t));

  if (!node1 || !node2) {
    if (node1)
      kfree(node1);
    if (node2)
      kfree(node2);
    sockets[sock1->id] = 0;
    sockets[sock2->id] = 0;
    return -1;
  }

  memset(node1, 0, sizeof(vfs_node_t));
  memset(node2, 0, sizeof(vfs_node_t));

  strcpy(node1->name, "socketpair");
  strcpy(node2->name, "socketpair");
  node1->impl = (uint32_t)(uintptr_t)sock1;
  node2->impl = (uint32_t)(uintptr_t)sock2;
  node1->read = socket_read;
  node2->read = socket_read;
  node1->write = socket_write;
  node2->write = socket_write;
  node1->close = socket_close;
  node2->close = socket_close;
  node1->flags = VFS_SOCKET;
  node2->flags = VFS_SOCKET;
  node1->ref_count = 1;
  node2->ref_count = 1;

  // Find two free file descriptors
  int fd1 = -1, fd2 = -1;
  for (int i = 0; i < MAX_PROCESS_FILES && (fd1 < 0 || fd2 < 0); i++) {
    if (!current_process->fd_table[i]) {
      if (fd1 < 0) {
        fd1 = i;
        current_process->fd_table[i] = node1;
      } else {
        fd2 = i;
        current_process->fd_table[i] = node2;
      }
    }
  }

  if (fd1 < 0 || fd2 < 0) {
    // Cleanup on failure
    if (fd1 >= 0)
      current_process->fd_table[fd1] = 0;
    kfree(node1);
    kfree(node2);
    sockets[sock1->id] = 0;
    sockets[sock2->id] = 0;
    return -1;
  }

  sv[0] = fd1;
  sv[1] = fd2;

  serial_log("SOCKET: Created socket pair");
  return 0;
}
