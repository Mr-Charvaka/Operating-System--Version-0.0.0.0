/*
 * syscall.h - System Call Interface for Retro-OS User Applications
 *
 * This header provides a clean interface to all OS syscalls for user-mode apps.
 */
#ifndef _SYSCALL_H
#define _SYSCALL_H

#include "types.h"
#include <stdint.h>

/* Syscall numbers */
#define SYS_PRINT 0
#define SYS_GETPID 1
#define SYS_OPEN 2
#define SYS_READ 3
#define SYS_WRITE 4
#define SYS_CLOSE 5
#define SYS_SBRK 6
#define SYS_MMAP 7
#define SYS_MUNMAP 8
#define SYS_FORK 9
#define SYS_EXECVE 10
#define SYS_WAIT 11
#define SYS_EXIT 12
#define SYS_PIPE 13
#define SYS_SOCKET 14
#define SYS_BIND 15
#define SYS_CONNECT 16
#define SYS_ACCEPT 17
#define SYS_SIGNAL 18
#define SYS_KILL 19
#define SYS_SIGRETURN 20
#define SYS_READDIR 21
#define SYS_STATFS 22
#define SYS_CHDIR 23
#define SYS_UNLINK 24
#define SYS_MKDIR 25
#define SYS_GETCWD 26
#define SYS_STAT 27
#define SYS_FSTAT 28
#define SYS_SEEK 29
#define SYS_DUP 30
#define SYS_DUP2 31
#define SYS_GETTIME 32
#define SYS_UPTIME 33
#define SYS_SLEEP 34
#define SYS_UNAME 35
#define SYS_GETUID 36
#define SYS_GETEUID 37
#define SYS_GETPPID 38
#define SYS_RENAME 39
#define SYS_TRUNCATE 40
#define SYS_ACCESS 41
#define SYS_CHMOD 42
#define SYS_IOCTL 43
#define SYS_ALARM 44
#define SYS_HOSTNAME 45
#define SYS_MEMINFO 46
#define SYS_PROCINFO 47
#define SYS_SYMLINK 48
#define SYS_READLINK 49
#define SYS_GETPGRP 50
#define SYS_SETPGRP 51
#define SYS_SETSID 52
#define SYS_ENV 53
#define SYS_SYNC 54
#define SYS_RMDIR 55
#define SYS_SHMGET 58
#define SYS_SHMAT 59
#define SYS_SHMDT 60
#define SYS_OPENAT 62
#define SYS_FSTATAT 63
#define SYS_READV 64
#define SYS_WRITEV 65
#define SYS_WAITPID 66
#define SYS_WAITID 67
#define SYS__EXIT 68
#define SYS_SIGACTION 69
#define SYS_SIGPROCMASK 70
#define SYS_SIGPENDING 71
#define SYS_SIGSUSPEND 72
#define SYS_SIGWAIT 73
#define SYS_PAUSE 74
#define SYS_ABORT 75
#define SYS_POSIX_SPAWN 76
#define SYS_NANOSLEEP 77
#define SYS_CLOCK_GETTIME 78
#define SYS_GETTIMEOFDAY 79
#define SYS_PREAD 80
#define SYS_PWRITE 81
#define SYS_LSEEK_POSIX 82
#define SYS_FTRUNCATE 83
#define SYS_LINK 84
#define SYS_LSTAT 85
#define SYS_MKDIRAT 86
#define SYS_UNLINKAT 87
#define SYS_RENAMEAT 88
#define SYS_FCHMOD 89
#define SYS_CHOWN 90
#define SYS_FCHOWN 91
#define SYS_FCNTL 92
#define SYS_FCHDIR 93
#define SYS_GETDENTS 94
#define SYS_TIMES 95

// Phase 4: Pthreads
#define SYS_PTHREAD_CREATE 96
#define SYS_PTHREAD_EXIT 97
#define SYS_PTHREAD_JOIN 98
#define SYS_PTHREAD_DETACH 99
#define SYS_PTHREAD_MUTEX_INIT 100
#define SYS_PTHREAD_MUTEX_LOCK 101
#define SYS_PTHREAD_MUTEX_UNLOCK 102
#define SYS_PTHREAD_COND_INIT 103
#define SYS_PTHREAD_COND_WAIT 104
#define SYS_PTHREAD_COND_SIGNAL 105

// Phase 5: IPC
#define SYS_SEM_OPEN 106
#define SYS_SEM_CLOSE 107
#define SYS_SEM_UNLINK 108
#define SYS_SEM_WAIT 109
#define SYS_SEM_POST 110
#define SYS_MQ_OPEN 111
#define SYS_MQ_CLOSE 112
#define SYS_MQ_UNLINK 113
#define SYS_MQ_SEND 114
#define SYS_MQ_RECEIVE 115

// Phase 6: Sockets
#define SYS_LISTEN 116
#define SYS_SEND 117
#define SYS_RECV 118
#define SYS_SENDTO 119
#define SYS_RECVFROM 120
#define SYS_GETSOCKNAME 121
#define SYS_GETPEERNAME 122
#define SYS_SETSOCKOPT 123
#define SYS_GETSOCKOPT 124
#define SYS_SHUTDOWN 125

// Phase 8: User
#define SYS_SETUID 126
#define SYS_SETGID 127
#define SYS_SETEUID 128
#define SYS_SETEGID 129
#define SYS_SETRESUID 130

// Phase 9-10: Termios/Select
#define SYS_TCGETATTR 131
#define SYS_TCSETATTR 132
#define SYS_SELECT 133
#define SYS_POLL 134

// Phase 11-12: Memory/Config
#define SYS_MPROTECT 141
#define SYS_MSYNC 142
#define SYS_MLOCK 143
#define SYS_SYSCONF 144

#define AF_UNIX 1
#define SOCK_STREAM 1

/* Open flags */
#define O_RDONLY 0x00
#define O_WRONLY 0x01
#define O_RDWR 0x02
#define O_CREAT 0x40
#define O_TRUNC 0x200
#define O_APPEND 0x400
#define O_EXCL 0x80

/* Seek origins */
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

/* File type flags */
#define S_IFMT 0170000
#define S_IFREG 0100000
#define S_IFDIR 0040000
#define S_IFCHR 0020000
#define S_IFBLK 0060000
#define S_IFIFO 0010000
#define S_IFLNK 0120000
#define S_IFSOCK 0140000

/* Signals */
#define SIGHUP 1
#define SIGINT 2
#define SIGQUIT 3
#define SIGILL 4
#define SIGTRAP 5
#define SIGABRT 6
#define SIGBUS 7
#define SIGFPE 8
#define SIGKILL 9
#define SIGUSR1 10
#define SIGSEGV 11
#define SIGUSR2 12
#define SIGPIPE 13
#define SIGALRM 14
#define SIGTERM 15
#define SIGCHLD 17
#define SIGCONT 18
#define SIGSTOP 19
#define SIGTSTP 20

// Redundant struct stat and dirent removed as they are in types.h

/* uname structure */
struct utsname {
  char sysname[32];
  char nodename[32];
  char release[32];
  char version[32];
  char machine[32];
};

/* Memory info structure */
struct meminfo {
  uint32_t total;
  uint32_t free;
  uint32_t used;
};

/* Process info structure */
struct procinfo {
  uint32_t pid;
  uint32_t ppid;
  uint32_t state;
  char name[64];
};

/* RTC time structure */
struct rtc_time {
  uint8_t second;
  uint8_t minute;
  uint8_t hour;
  uint8_t day;
  uint8_t month;
  uint8_t year;
};

/* Basic syscall - print string */
static inline void syscall_print(const char *str) {
  asm volatile("int $0x80" ::"a"(SYS_PRINT), "b"(str));
}

/* Get process ID */
static inline int syscall_getpid(void) {
  int res;
  asm volatile("int $0x80" : "=a"(res) : "a"(SYS_GETPID));
  return res;
}

/* Open file */
static inline int syscall_open(const char *path, int flags) {
  int res;
  asm volatile("int $0x80" : "=a"(res) : "a"(SYS_OPEN), "b"(path), "c"(flags));
  return res;
}

/* Read from file */
static inline int syscall_read(int fd, void *buf, uint32_t size) {
  int res;
  asm volatile("int $0x80"
               : "=a"(res)
               : "a"(SYS_READ), "b"(fd), "c"(buf), "d"(size));
  return res;
}

/* Write to file */
static inline int syscall_write(int fd, const void *buf, uint32_t size) {
  int res;
  asm volatile("int $0x80"
               : "=a"(res)
               : "a"(SYS_WRITE), "b"(fd), "c"(buf), "d"(size));
  return res;
}

/* Close file */
static inline int syscall_close(int fd) {
  int res;
  asm volatile("int $0x80" : "=a"(res) : "a"(SYS_CLOSE), "b"(fd));
  return res;
}

/* sbrk - Change data segment size */
static inline void *syscall_sbrk(intptr_t increment) {
  void *res;
  asm volatile("int $0x80" : "=a"(res) : "a"(SYS_SBRK), "b"(increment));
  return res;
}

/* Fork process */
static inline int syscall_fork(void) {
  int res;
  asm volatile("int $0x80" : "=a"(res) : "a"(SYS_FORK));
  return res;
}

/* Execute program */
static inline int syscall_execve(const char *path, char **argv, char **envp) {
  int res;
  asm volatile("int $0x80"
               : "=a"(res)
               : "a"(SYS_EXECVE), "b"(path), "c"(argv), "d"(envp));
  return res;
}

/* Wait for child */
static inline int syscall_wait(int *status) {
  int res;
  asm volatile("int $0x80" : "=a"(res) : "a"(SYS_WAIT), "b"(status));
  return res;
}

/* Exit process */
static inline void syscall_exit(int status) {
  asm volatile("int $0x80" : : "a"(SYS_EXIT), "b"(status));
  while (1)
    ;
}

/* Create pipe */
static inline int syscall_pipe(int *fds) {
  int res;
  asm volatile("int $0x80" : "=a"(res) : "a"(SYS_PIPE), "b"(fds));
  return res;
}

/* Send signal */
static inline int syscall_kill(int pid, int sig) {
  int res;
  asm volatile("int $0x80" : "=a"(res) : "a"(SYS_KILL), "b"(pid), "c"(sig));
  return res;
}

/* Read directory entry */
static inline int syscall_readdir(int fd, uint32_t index, struct dirent *de) {
  int res;
  asm volatile("int $0x80"
               : "=a"(res)
               : "a"(SYS_READDIR), "b"(fd), "c"(index), "d"(de));
  return res;
}

/* Get filesystem stats */
static inline int syscall_statfs(uint32_t *total, uint32_t *free,
                                 uint32_t *block_size) {
  int res;
  asm volatile("int $0x80"
               : "=a"(res)
               : "a"(SYS_STATFS), "b"(total), "c"(free), "d"(block_size));
  return res;
}

/* Change directory */
static inline int syscall_chdir(const char *path) {
  int res;
  asm volatile("int $0x80" : "=a"(res) : "a"(SYS_CHDIR), "b"(path));
  return res;
}

/* Unlink file */
static inline int syscall_unlink(const char *path) {
  int res;
  asm volatile("int $0x80" : "=a"(res) : "a"(SYS_UNLINK), "b"(path));
  return res;
}

/* Make directory */
static inline int syscall_mkdir(const char *path, uint32_t mode) {
  int res;
  asm volatile("int $0x80" : "=a"(res) : "a"(SYS_MKDIR), "b"(path));
  return res;
}

/* Get current working directory */
static inline int syscall_getcwd(char *buf, uint32_t size) {
  int res;
  asm volatile("int $0x80" : "=a"(res) : "a"(SYS_GETCWD), "b"(buf), "c"(size));
  return res;
}

/* Get file stat */
static inline int syscall_stat(const char *path, struct stat *st) {
  int res;
  asm volatile("int $0x80" : "=a"(res) : "a"(SYS_STAT), "b"(path), "c"(st));
  return res;
}

/* Get file stat by fd */
static inline int syscall_fstat(int fd, struct stat *st) {
  int res;
  asm volatile("int $0x80" : "=a"(res) : "a"(SYS_FSTAT), "b"(fd), "c"(st));
  return res;
}

/* Seek in file */
static inline int syscall_lseek(int fd, int offset, int whence) {
  int res;
  asm volatile("int $0x80"
               : "=a"(res)
               : "a"(SYS_SEEK), "b"(fd), "c"(offset), "d"(whence));
  return res;
}

/* Duplicate file descriptor */
static inline int syscall_dup(int oldfd) {
  int res;
  asm volatile("int $0x80" : "=a"(res) : "a"(SYS_DUP), "b"(oldfd));
  return res;
}

/* Duplicate file descriptor to specific fd */
static inline int syscall_dup2(int oldfd, int newfd) {
  int res;
  asm volatile("int $0x80" : "=a"(res) : "a"(SYS_DUP2), "b"(oldfd), "c"(newfd));
  return res;
}

/* Get RTC time */
static inline int syscall_gettime(struct rtc_time *time) {
  int res;
  asm volatile("int $0x80" : "=a"(res) : "a"(SYS_GETTIME), "b"(time));
  return res;
}

/* Get system uptime in ticks */
static inline uint32_t syscall_uptime(void) {
  uint32_t res;
  asm volatile("int $0x80" : "=a"(res) : "a"(SYS_UPTIME));
  return res;
}

/* Sleep for ticks */
static inline int syscall_sleep(uint32_t ticks) {
  int res;
  asm volatile("int $0x80" : "=a"(res) : "a"(SYS_SLEEP), "b"(ticks));
  return res;
}

/* Get uname */
static inline int syscall_uname(struct utsname *buf) {
  int res;
  asm volatile("int $0x80" : "=a"(res) : "a"(SYS_UNAME), "b"(buf));
  return res;
}

/* Get user ID */
static inline int syscall_getuid(void) {
  int res;
  asm volatile("int $0x80" : "=a"(res) : "a"(SYS_GETUID));
  return res;
}

/* Get effective user ID */
static inline int syscall_geteuid(void) {
  int res;
  asm volatile("int $0x80" : "=a"(res) : "a"(SYS_GETEUID));
  return res;
}

/* Get parent process ID */
static inline int syscall_getppid(void) {
  int res;
  asm volatile("int $0x80" : "=a"(res) : "a"(SYS_GETPPID));
  return res;
}

/* Rename file */
static inline int syscall_rename(const char *oldpath, const char *newpath) {
  int res;
  asm volatile("int $0x80"
               : "=a"(res)
               : "a"(SYS_RENAME), "b"(oldpath), "c"(newpath));
  return res;
}

/* Truncate file */
static inline int syscall_truncate(const char *path, uint32_t length) {
  int res;
  asm volatile("int $0x80"
               : "=a"(res)
               : "a"(SYS_TRUNCATE), "b"(path), "c"(length));
  return res;
}

/* Check access permissions */
static inline int syscall_access(const char *path, int mode) {
  int res;
  asm volatile("int $0x80" : "=a"(res) : "a"(SYS_ACCESS), "b"(path), "c"(mode));
  return res;
}

/* Get memory info */
static inline int syscall_meminfo(struct meminfo *info) {
  int res;
  asm volatile("int $0x80" : "=a"(res) : "a"(SYS_MEMINFO), "b"(info));
  return res;
}

/* Get process info */
static inline int syscall_procinfo(int pid, struct procinfo *info) {
  int res;
  asm volatile("int $0x80"
               : "=a"(res)
               : "a"(SYS_PROCINFO), "b"(pid), "c"(info));
  return res;
}

/* Get hostname */
static inline int syscall_gethostname(char *name, uint32_t len) {
  int res;
  asm volatile("int $0x80"
               : "=a"(res)
               : "a"(SYS_HOSTNAME), "b"(name), "c"(len));
  return res;
}

/* Remove directory */
static inline int syscall_rmdir(const char *path) {
  int res;
  asm volatile("int $0x80" : "=a"(res) : "a"(SYS_RMDIR), "b"(path));
  return res;
}

/* Sync filesystems */
static inline int syscall_sync(void) {
  int res;
  asm volatile("int $0x80" : "=a"(res) : "a"(SYS_SYNC));
  return res;
}

/* Set alarm */
static inline int syscall_alarm(uint32_t seconds) {
  int res;
  asm volatile("int $0x80" : "=a"(res) : "a"(SYS_ALARM), "b"(seconds));
  return res;
}

static inline int syscall_shmget(uint32_t key, uint32_t size, int flags) {
  int res;
  asm volatile("int $0x80"
               : "=a"(res)
               : "a"(SYS_SHMGET), "b"(key), "c"(size), "d"(flags));
  return res;
}

static inline void *syscall_shmat(int shmid) {
  void *res;
  asm volatile("int $0x80" : "=a"(res) : "a"(SYS_SHMAT), "b"(shmid));
  return res;
}

static inline int syscall_shmdt(void *addr) {
  int res;
  asm volatile("int $0x80" : "=a"(res) : "a"(SYS_SHMDT), "b"(addr));
  return res;
}

static inline int syscall_socket(int domain, int type, int protocol) {
  int res;
  asm volatile("int $0x80"
               : "=a"(res)
               : "a"(SYS_SOCKET), "b"(domain), "c"(type), "d"(protocol));
  return res;
}

static inline int syscall_connect(int sockfd, const char *path) {
  int res;
  asm volatile("int $0x80"
               : "=a"(res)
               : "a"(SYS_CONNECT), "b"(sockfd), "c"(path));
  return res;
}

static inline int syscall_sigaction(int sig, const struct sigaction *act,
                                    struct sigaction *oldact) {
  int res;
  asm volatile("int $0x80"
               : "=a"(res)
               : "a"(SYS_SIGACTION), "b"(sig), "c"(act), "d"(oldact));
  return res;
}

static inline int syscall_sigprocmask(int how, const void *set, void *oldset) {
  int res;
  asm volatile("int $0x80"
               : "=a"(res)
               : "a"(SYS_SIGPROCMASK), "b"(how), "c"(set), "d"(oldset));
  return res;
}

static inline int syscall_clock_gettime(clockid_t clk_id, struct timespec *tp) {
  int res;
  asm volatile("int $0x80"
               : "=a"(res)
               : "a"(SYS_CLOCK_GETTIME), "b"(clk_id), "c"(tp));
  return res;
}

static inline int syscall_nanosleep(const struct timespec *req,
                                    struct timespec *rem) {
  int res;
  asm volatile("int $0x80"
               : "=a"(res)
               : "a"(SYS_NANOSLEEP), "b"(req), "c"(rem));
  return res;
}

#endif /* _SYSCALL_H */
