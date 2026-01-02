#include "pipe.h"
#include "../drivers/serial.h"
#include "../include/string.h"
#include "heap.h"
#include "memory.h"
#include "process.h"

void pipe_init() {
  // Future initialization if needed
}

extern "C" {

uint32_t pipe_read(vfs_node_t *node, uint32_t offset, uint32_t size,
                   uint8_t *buffer) {
  (void)offset;
  pipe_t *pipe = (pipe_t *)node->impl;
  if (!pipe || pipe->read_closed)
    return 0;

  uint32_t read_bytes = 0;
  while (read_bytes < size) {
    if (pipe->head == pipe->tail) {
      if (pipe->write_closed)
        break;
      if (read_bytes > 0)
        break; // Return what we have

      // Block (Wait for data)
      current_process->state = PROCESS_WAITING;
      schedule();
      continue;
    }

    buffer[read_bytes++] = pipe->buffer[pipe->head];
    pipe->head = (pipe->head + 1) % PIPE_SIZE;
  }

  // Wake up any waiting processes (potential writers waiting for space)
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

uint32_t pipe_write(vfs_node_t *node, uint32_t offset, uint32_t size,
                    uint8_t *buffer) {
  (void)offset;
  pipe_t *pipe = (pipe_t *)node->impl;
  if (!pipe || pipe->write_closed || pipe->read_closed)
    return 0;

  uint32_t written_bytes = 0;
  while (written_bytes < size) {
    uint32_t next_tail = (pipe->tail + 1) % PIPE_SIZE;
    if (next_tail == pipe->head) {
      if (written_bytes > 0)
        break; // Return what we have

      // Block (Wait for space)
      current_process->state = PROCESS_WAITING;
      schedule();
      continue;
    }

    pipe->buffer[pipe->tail] = buffer[written_bytes++];
    pipe->tail = next_tail;
  }

  // Wake up any waiting processes (potential readers waiting for data)
  process_t *p = ready_queue;
  if (p) {
    process_t *start = p;
    do {
      if (p->state == PROCESS_WAITING)
        p->state = PROCESS_READY;
      p = p->next;
    } while (p && p != start);
  }

  return written_bytes;
}

void pipe_close(vfs_node_t *node) {
  pipe_t *pipe = (pipe_t *)node->impl;
  if (!pipe)
    return;

  if (node->flags & 0x1)
    pipe->read_closed = 1;
  if (node->flags & 0x2)
    pipe->write_closed = 1;

  if (pipe->read_closed && pipe->write_closed) {
    kfree(pipe->buffer);
    kfree(pipe);
  }

  // Wake up others so they see the state change
  process_t *p = ready_queue;
  if (p) {
    process_t *start = p;
    do {
      if (p->state == PROCESS_WAITING)
        p->state = PROCESS_READY;
      p = p->next;
    } while (p && p != start);
  }
}

int sys_pipe(uint32_t *filedes) {
  if (!filedes)
    return -1;

  // Check if pointer is valid for writing (very basic check)
  // In a real OS we'd use vm_verify_pointer

  pipe_t *pipe = (pipe_t *)kmalloc(sizeof(pipe_t));
  if (!pipe)
    return -1;

  pipe->buffer = (uint8_t *)kmalloc(PIPE_SIZE);
  if (!pipe->buffer) {
    kfree(pipe);
    return -1;
  }

  pipe->head = 0;
  pipe->tail = 0;
  pipe->read_closed = 0;
  pipe->write_closed = 0;

  // Create VFS node for read end
  vfs_node_t *read_node = (vfs_node_t *)kmalloc(sizeof(vfs_node_t));
  memset(read_node, 0, sizeof(vfs_node_t));
  strcpy(read_node->name, "pipe_read");
  read_node->impl = (uint32_t)(uintptr_t)pipe;
  read_node->read = pipe_read;
  read_node->close = pipe_close;
  read_node->flags = 0x1; // READ side
  read_node->ref_count = 1;

  // Create VFS node for write end
  vfs_node_t *write_node = (vfs_node_t *)kmalloc(sizeof(vfs_node_t));
  memset(write_node, 0, sizeof(vfs_node_t));
  strcpy(write_node->name, "pipe_write");
  write_node->impl = (uint32_t)(uintptr_t)pipe;
  write_node->write = pipe_write;
  write_node->close = pipe_close;
  write_node->flags = 0x2; // WRITE side
  write_node->ref_count = 1;

  // Find slots in current process fd_table
  int f1 = -1, f2 = -1;
  for (int i = 0; i < MAX_PROCESS_FILES; i++) {
    if (current_process->fd_table[i] == 0) {
      if (f1 == -1)
        f1 = i;
      else if (f2 == -1) {
        f2 = i;
        break;
      }
    }
  }

  if (f1 == -1 || f2 == -1) {
    kfree(read_node);
    kfree(write_node);
    kfree(pipe->buffer);
    kfree(pipe);
    return -1;
  }

  current_process->fd_table[f1] = read_node;
  current_process->fd_table[f2] = write_node;

  filedes[0] = (uint32_t)f1;
  filedes[1] = (uint32_t)f2;

  return 0;
}

} // extern "C"
