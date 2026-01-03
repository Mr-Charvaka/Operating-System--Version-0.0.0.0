// ============================================================================
// posix_stubs.cpp - Stub implementations for missing POSIX functions
// ============================================================================

#include "../drivers/serial.h"
#include "../include/errno.h"
#include "process.h"
#include "vfs.h"

extern "C" {

// ============================================================================
// File Operations Stubs
// ============================================================================

int pread(int fd, void *buf, size_t count, int offset) {
  (void)fd;
  (void)buf;
  (void)count;
  (void)offset;
  serial_log("pread: stub called");
  return -ENOSYS;
}

int pwrite(int fd, const void *buf, size_t count, int offset) {
  (void)fd;
  (void)buf;
  (void)count;
  (void)offset;
  serial_log("pwrite: stub called");
  return -ENOSYS;
}

int lseek(int fd, int offset, int whence) {
  (void)fd;
  (void)offset;
  (void)whence;
  serial_log("lseek: stub called");
  return -ENOSYS;
}

int truncate(const char *path, int length) {
  (void)path;
  (void)length;
  serial_log("truncate: stub called");
  return -ENOSYS;
}

int ftruncate(int fd, int length) {
  (void)fd;
  (void)length;
  serial_log("ftruncate: stub called");
  return -ENOSYS;
}

int link(const char *oldpath, const char *newpath) {
  (void)oldpath;
  (void)newpath;
  serial_log("link: stub called");
  return -ENOSYS;
}

int symlink(const char *target, const char *linkpath) {
  (void)target;
  (void)linkpath;
  serial_log("symlink: stub called");
  return -ENOSYS;
}

int readlink(const char *pathname, char *buf, size_t bufsiz) {
  (void)pathname;
  (void)buf;
  (void)bufsiz;
  serial_log("readlink: stub called");
  return -ENOSYS;
}

int lstat(const char *pathname, void *statbuf) {
  (void)pathname;
  (void)statbuf;
  serial_log("lstat: stub called");
  return -ENOSYS;
}

int mkdirat(int dirfd, const char *pathname, int mode) {
  (void)dirfd;
  (void)pathname;
  (void)mode;
  serial_log("mkdirat: stub called");
  return -ENOSYS;
}

int unlinkat(int dirfd, const char *pathname, int flags) {
  (void)dirfd;
  (void)pathname;
  (void)flags;
  serial_log("unlinkat: stub called");
  return -ENOSYS;
}

int renameat(int olddirfd, const char *oldpath, int newdirfd,
             const char *newpath) {
  (void)olddirfd;
  (void)oldpath;
  (void)newdirfd;
  (void)newpath;
  serial_log("renameat: stub called");
  return -ENOSYS;
}

int rename(const char *oldpath, const char *newpath) {
  (void)oldpath;
  (void)newpath;
  serial_log("rename: stub called");
  return -ENOSYS;
}

int fchmod(int fd, int mode) {
  (void)fd;
  (void)mode;
  serial_log("fchmod: stub called");
  return -ENOSYS;
}

int chown(const char *pathname, int owner, int group) {
  (void)pathname;
  (void)owner;
  (void)group;
  serial_log("chown: stub called");
  return -ENOSYS;
}

int fchown(int fd, int owner, int group) {
  (void)fd;
  (void)owner;
  (void)group;
  serial_log("fchown: stub called");
  return -ENOSYS;
}

int fcntl(int fd, int cmd, int arg) {
  (void)fd;
  (void)cmd;
  (void)arg;
  serial_log("fcntl: stub called");
  return -ENOSYS;
}

// ============================================================================
// Directory Operations Stubs
// ============================================================================

int fchdir(int fd) {
  (void)fd;
  serial_log("fchdir: stub called");
  return -ENOSYS;
}

int getdents(int fd, void *dirp, unsigned int count) {
  (void)fd;
  (void)dirp;
  (void)count;
  serial_log("getdents: stub called");
  return -ENOSYS;
}

// ============================================================================
// Socket Operations Stubs
// ============================================================================

int listen(int sockfd, int backlog) {
  (void)sockfd;
  (void)backlog;
  serial_log("listen: stub called");
  return -ENOSYS;
}

int send(int sockfd, const void *buf, size_t len, int flags) {
  (void)sockfd;
  (void)buf;
  (void)len;
  (void)flags;
  serial_log("send: stub called");
  return -ENOSYS;
}

int recv(int sockfd, void *buf, size_t len, int flags) {
  (void)sockfd;
  (void)buf;
  (void)len;
  (void)flags;
  serial_log("recv: stub called");
  return -ENOSYS;
}

int sendto(int sockfd, const void *buf, size_t len, int flags,
           const void *dest_addr, unsigned int addrlen) {
  (void)sockfd;
  (void)buf;
  (void)len;
  (void)flags;
  (void)dest_addr;
  (void)addrlen;
  serial_log("sendto: stub called");
  return -ENOSYS;
}

int recvfrom(int sockfd, void *buf, size_t len, int flags, void *src_addr,
             unsigned int *addrlen) {
  (void)sockfd;
  (void)buf;
  (void)len;
  (void)flags;
  (void)src_addr;
  (void)addrlen;
  serial_log("recvfrom: stub called");
  return -ENOSYS;
}

int getsockname(int sockfd, void *addr, unsigned int *addrlen) {
  (void)sockfd;
  (void)addr;
  (void)addrlen;
  serial_log("getsockname: stub called");
  return -ENOSYS;
}

int getpeername(int sockfd, void *addr, unsigned int *addrlen) {
  (void)sockfd;
  (void)addr;
  (void)addrlen;
  serial_log("getpeername: stub called");
  return -ENOSYS;
}

int setsockopt(int sockfd, int level, int optname, const void *optval,
               unsigned int optlen) {
  (void)sockfd;
  (void)level;
  (void)optname;
  (void)optval;
  (void)optlen;
  serial_log("setsockopt: stub called");
  return -ENOSYS;
}

int getsockopt(int sockfd, int level, int optname, void *optval,
               unsigned int *optlen) {
  (void)sockfd;
  (void)level;
  (void)optname;
  (void)optval;
  (void)optlen;
  serial_log("getsockopt: stub called");
  return -ENOSYS;
}

int shutdown(int sockfd, int how) {
  (void)sockfd;
  (void)how;
  serial_log("shutdown: stub called");
  return -ENOSYS;
}

} // extern "C"
