#ifndef SOCKET_H
#define SOCKET_H

#include "../include/types.h"
#include "../include/vfs.h"

#define AF_UNIX 1
#define SOCK_STREAM 1
#define SOCK_DGRAM 2
#define MAX_BACKLOG 16

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
  char path[128]; // Address/path for getsockname/getpeername
  struct socket *peer;
  struct socket *backlog[MAX_BACKLOG];
  int backlog_count;
  int max_backlog; // Maximum backlog size

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
int sys_listen(int sockfd, int backlog);
ssize_t sys_send(int sockfd, const void *buf, size_t len, int flags);
ssize_t sys_recv(int sockfd, void *buf, size_t len, int flags);
ssize_t sys_sendto(int sockfd, const void *buf, size_t len, int flags,
                   const void *dest_addr, uint32_t addrlen);
ssize_t sys_recvfrom(int sockfd, void *buf, size_t len, int flags,
                     void *src_addr, uint32_t *addrlen);
int sys_getsockname(int sockfd, void *addr, uint32_t *addrlen);
int sys_getpeername(int sockfd, void *addr, uint32_t *addrlen);
int sys_setsockopt(int sockfd, int level, int optname, const void *optval,
                   uint32_t optlen);
int sys_getsockopt(int sockfd, int level, int optname, void *optval,
                   uint32_t *optlen);
int sys_shutdown(int sockfd, int how);
int sys_socketpair(int domain, int type, int protocol, int sv[2]);

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
