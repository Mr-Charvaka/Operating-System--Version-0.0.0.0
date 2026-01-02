#ifndef SOCKET_H
#define SOCKET_H

#include "../include/types.h"
#include "../include/vfs.h"

#define AF_UNIX 1
#define SOCK_STREAM 1

typedef enum {
  SOCKET_FREE,
  SOCKET_BOUND,
  SOCKET_LISTENING,
  SOCKET_CONNECTING,
  SOCKET_CONNECTED,
  SOCKET_CLOSED
} socket_state_t;

typedef struct socket {
  int id;
  int type;
  int domain;
  socket_state_t state;

  char bind_path[128];
  struct socket *peer;
  struct socket *backlog[8];
  int backlog_count;

  uint8_t *buffer;
  uint32_t head;
  uint32_t tail;

  // We'll reuse the pipe-like ring buffer logic for simplicity
} socket_t;

#ifdef __cplusplus
extern "C" {
#endif

void socket_init();

int sys_socket(int domain, int type, int protocol);
int sys_bind(int sockfd, const char *path);
int sys_connect(int sockfd, const char *path);
int sys_accept(int sockfd);

// VFS interface for socket FDs
uint32_t socket_read(vfs_node_t *node, uint32_t offset, uint32_t size,
                     uint8_t *buffer);
uint32_t socket_write(vfs_node_t *node, uint32_t offset, uint32_t size,
                      uint8_t *buffer);
void socket_close(vfs_node_t *node);

#ifdef __cplusplus
}
#endif

#endif
