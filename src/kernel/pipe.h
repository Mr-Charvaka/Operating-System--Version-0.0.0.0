#ifndef PIPE_H
#define PIPE_H

#include "../include/types.h"
#include "../include/vfs.h"

#define PIPE_SIZE 4096

typedef struct {
  uint8_t *buffer;
  uint32_t head;
  uint32_t tail;
  uint32_t size;
  uint8_t read_closed;
  uint8_t write_closed;
  // We'll add wait queues here later if needed for advanced blocking
} pipe_t;

#ifdef __cplusplus
extern "C" {
#endif

void pipe_init();
int sys_pipe(uint32_t *filedes);

// VFS interface for pipes
uint32_t pipe_read(vfs_node_t *node, uint32_t offset, uint32_t size,
                   uint8_t *buffer);
uint32_t pipe_write(vfs_node_t *node, uint32_t offset, uint32_t size,
                    uint8_t *buffer);
void pipe_close(vfs_node_t *node);

#ifdef __cplusplus
}
#endif

#endif
