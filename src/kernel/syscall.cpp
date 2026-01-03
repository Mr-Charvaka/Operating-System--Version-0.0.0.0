#include "../drivers/rtc.h"
#include "../drivers/serial.h"
#include "../include/errno.h"
#include "../include/signal.h"
#include "../include/string.h"
#include "../include/vfs.h"
#include "heap.h"
#include "memory.h"
#include "paging.h"
#include "pipe.h"
#include "pmm.h"
#include "process.h"
#include "shm.h"
#include "socket.h"
#include "tty.h"
#include "vm.h"

#define AT_FDCWD -100

// External FAT16 function
extern "C" int fat16_create_file(const char *filename);
extern "C" void fat16_get_stats_bytes(uint32_t *total_bytes,
                                      uint32_t *free_bytes);

// External tick counter from timer
extern uint32_t tick;

// System hostname
static char system_hostname[64] = "retro-os";

// System info
static const char *sysname = "Retro-OS";
static const char *release = "1.0.11";
static const char *version = "#1 SMP";
static const char *machine = "i686";

static bool validate_user_pointer(const void *ptr, uint32_t size) {
  uintptr_t p = (uintptr_t)ptr;
  if (p < 0x20000000)
    return false;
  if (p + size < p)
    return false;
  return true;
}

static bool check_unveil(const char *path, uint32_t needed_perms) {
  if (!current_process->unveils)
    return true;

  auto u = current_process->unveils;
  while (u) {
    if (strncmp(path, u->path, strlen(u->path)) == 0) {
      if ((u->permissions & needed_perms) == needed_perms)
        return true;
    }
    u = u->next;
  }
  return false;
}

static void syscall_handler(registers_t *regs);

// individual syscall implementations
typedef int (*syscall_ptr)(registers_t *);

int sys_print_serial(registers_t *regs) {
  if (!(current_process->pledges & PLEDGE_STDIO))
    return -EPERM;
  const char *str = (const char *)(uintptr_t)regs->ebx;
  serial_log_hex("  String at: ", (uint32_t)(uintptr_t)str);
  serial_log(str);
  tty_t *tty = tty_get_console();
  if (tty && str) {
    int len = 0;
    while (str[len])
      len++;
    tty_write(tty, str, len);
  }
  return 0;
}

int sys_get_pid(registers_t *regs) { return current_process->id; }

int sys_open(registers_t *regs) {
  char *path = (char *)(uintptr_t)regs->ebx;
  uint32_t flags = regs->ecx;

  uint32_t needed = 0;
  if (flags & O_CREAT)
    needed |= PLEDGE_CPATH;
  else if ((flags & 3) == O_RDONLY)
    needed |= PLEDGE_RPATH;
  else
    needed |= PLEDGE_WPATH;

  if (!check_unveil(path,
                    (flags & O_CREAT) ? 8 : ((flags & 3) == O_RDONLY ? 4 : 2)))
    return -EACCES;

  if ((current_process->pledges & needed) == 0)
    return -EPERM;

  vfs_node_t *node = vfs_resolve_path(path);
  if (node) {
    for (int j = 0; j < MAX_PROCESS_FILES; j++) {
      if (current_process->fd_table[j] == 0) {
        current_process->fd_table[j] = node;
        open_vfs(node, (flags & 3) != O_WRONLY, (flags & 3) != O_RDONLY);
        return j;
      }
    }
    return -EMFILE;
  }
  return -ENOENT;
}

int sys_openat(registers_t *regs) {
  int dirfd = (int)regs->ebx;
  const char *path = (const char *)regs->ecx;
  uint32_t flags = regs->edx;

  vfs_node_t *dir_node = vfs_root;
  if (dirfd != AT_FDCWD) {
    if (dirfd < 0 || dirfd >= MAX_PROCESS_FILES ||
        !current_process->fd_table[dirfd])
      return -EBADF;
    dir_node = current_process->fd_table[dirfd];
  }

  vfs_node_t *node = vfs_resolve_path_relative(dir_node, path);
  if (node) {
    for (int j = 0; j < MAX_PROCESS_FILES; j++) {
      if (current_process->fd_table[j] == 0) {
        current_process->fd_table[j] = node;
        open_vfs(node, (flags & 3) != O_WRONLY, (flags & 3) != O_RDONLY);
        return j;
      }
    }
    return -EMFILE;
  }
  return -ENOENT;
}

int sys_read(registers_t *regs) {
  int fd = (int)regs->ebx;
  uint8_t *buf = (uint8_t *)regs->ecx;
  uint32_t size = (uint32_t)regs->edx;
  if (validate_user_pointer(buf, size) && fd >= 0 && fd < MAX_PROCESS_FILES &&
      current_process->fd_table[fd]) {
    return read_vfs(current_process->fd_table[fd], 0, size, buf);
  }
  return -EBADF;
}

int sys_readv(registers_t *regs) {
  int fd = (int)regs->ebx;
  const struct iovec *iov = (const struct iovec *)regs->ecx;
  int iovcnt = (int)regs->edx;

  if (fd < 0 || fd >= MAX_PROCESS_FILES || !current_process->fd_table[fd])
    return -EBADF;
  if (!iov || iovcnt <= 0)
    return -EINVAL;

  vfs_node_t *node = current_process->fd_table[fd];
  ssize_t total_read = 0;
  for (int i = 0; i < iovcnt; i++) {
    uint32_t n = read_vfs(node, 0, iov[i].iov_len, (uint8_t *)iov[i].iov_base);
    total_read += n;
    if (n < iov[i].iov_len)
      break;
  }
  return (int)total_read;
}

int sys_write(registers_t *regs) {
  int fd = (int)regs->ebx;
  uint8_t *buf = (uint8_t *)regs->ecx;
  uint32_t size = (uint32_t)regs->edx;
  if (validate_user_pointer(buf, size) && fd >= 0 && fd < MAX_PROCESS_FILES &&
      current_process->fd_table[fd]) {
    return write_vfs(current_process->fd_table[fd], 0, size, buf);
  }
  return -EBADF;
}

int sys_writev(registers_t *regs) {
  int fd = (int)regs->ebx;
  const struct iovec *iov = (const struct iovec *)regs->ecx;
  int iovcnt = (int)regs->edx;

  if (fd < 0 || fd >= MAX_PROCESS_FILES || !current_process->fd_table[fd])
    return -EBADF;
  if (!iov || iovcnt <= 0)
    return -EINVAL;

  vfs_node_t *node = current_process->fd_table[fd];
  ssize_t total_written = 0;
  for (int i = 0; i < iovcnt; i++) {
    uint32_t n = write_vfs(node, 0, iov[i].iov_len, (uint8_t *)iov[i].iov_base);
    total_written += n;
    if (n < iov[i].iov_len)
      break;
  }
  return (int)total_written;
}

int sys_close(registers_t *regs) {
  int fd = (int)regs->ebx;
  if (fd >= 0 && fd < MAX_PROCESS_FILES && current_process->fd_table[fd]) {
    close_vfs(current_process->fd_table[fd]);
    current_process->fd_table[fd] = 0;
    return 0;
  }
  return -EBADF;
}

int sys_sbrk(registers_t *regs) {
  intptr_t increment = (intptr_t)regs->ebx;
  uint32_t old_brk = current_process->heap_end;
  uint32_t new_brk = old_brk + increment;

  uint32_t old_page_top = (old_brk + 0xFFF) & 0xFFFFF000;
  uint32_t new_page_top = (new_brk + 0xFFF) & 0xFFFFF000;

  if (new_page_top > old_page_top) {
    for (uint32_t page = old_page_top; page < new_page_top; page += 4096) {
      void *phys = pmm_alloc_block();
      if (!phys)
        return -ENOMEM;
      vm_map_page((uint32_t)phys, page, 7);
      memset((void *)page, 0, 4096);
    }
  }
  current_process->heap_end = new_brk;
  return old_brk;
}

int sys_mmap(registers_t *regs) {
  uint32_t addr = regs->ebx;
  uint32_t length = regs->ecx;
  if (addr == 0) {
    static uint32_t next_mmap_addr = 0x80000000;
    addr = next_mmap_addr;
    next_mmap_addr += (length + 0xFFF) & 0xFFFFF000;
  }
  uint32_t num_pages = (length + 0xFFF) / 4096;
  for (uint32_t i = 0; i < num_pages; i++) {
    uint32_t virt = addr + i * 4096;
    void *phys = pmm_alloc_block();
    if (!phys)
      return -ENOMEM;
    vm_map_page((uint32_t)phys, virt, 7);
    memset((void *)virt, 0, 4096);
  }
  return addr;
}

int sys_munmap(registers_t *regs) {
  uint32_t addr = regs->ebx;
  uint32_t length = regs->ecx;
  uint32_t num_pages = (length + 0xFFF) / 4096;
  for (uint32_t i = 0; i < num_pages; i++) {
    vm_unmap_page(addr + i * 4096);
  }
  return 0;
}

int sys_fork(registers_t *regs) {
  if (!(current_process->pledges & PLEDGE_PROC))
    return -EPERM;
  return fork_process(regs);
}

int sys_execve(registers_t *regs) {
  if (!(current_process->pledges & PLEDGE_EXEC))
    return -EPERM;
  const char *path = (const char *)regs->ebx;
  char **argv = (char **)regs->ecx;
  char **envp = (char **)regs->edx;
  return exec_process(regs, path, argv, envp);
}

int sys_wait(registers_t *regs) {
  int *status = (int *)regs->ebx;
  return wait_process(status);
}

int sys_exit(registers_t *regs) {
  exit_process(regs->ebx);
  return 0; // Should not reach
}

int sys_pipe_call(registers_t *regs) {
  uint32_t *filedes = (uint32_t *)regs->ebx;
  return sys_pipe(filedes);
}

int sys_socket_call(registers_t *regs) {
  if (!(current_process->pledges & PLEDGE_INET))
    return -EPERM;
  return sys_socket(regs->ebx, regs->ecx, regs->edx);
}

int sys_bind_call(registers_t *regs) {
  return sys_bind(regs->ebx, (const char *)regs->ecx);
}

int sys_connect_call(registers_t *regs) {
  return sys_connect(regs->ebx, (const char *)regs->ecx);
}

int sys_accept_call(registers_t *regs) { return sys_accept(regs->ebx); }

int sys_signal_call(registers_t *regs) {
  return sys_signal(regs->ebx, (sighandler_t)regs->ecx);
}

int sys_kill_call(registers_t *regs) {
  if (!(current_process->pledges & PLEDGE_PROC))
    return -EPERM;
  return sys_kill(regs->ebx, regs->ecx);
}

int sys_sigreturn_call(registers_t *regs) { return kernel_sigreturn(regs); }

int sys_readdir_call(registers_t *regs) {
  int fd = (int)regs->ebx;
  uint32_t index = (uint32_t)regs->ecx;
  struct dirent *de = (struct dirent *)regs->edx;
  if (fd >= 0 && fd < MAX_PROCESS_FILES && current_process->fd_table[fd]) {
    struct dirent *res = readdir_vfs(current_process->fd_table[fd], index);
    if (res) {
      memcpy(de, res, sizeof(struct dirent));
      return 0;
    }
    return -ENOENT;
  }
  return -EBADF;
}

int sys_statfs_call(registers_t *regs) {
  uint32_t *total = (uint32_t *)regs->ebx;
  uint32_t *free = (uint32_t *)regs->ecx;
  uint32_t *block_size = (uint32_t *)regs->edx;
  if (block_size)
    *block_size = 1;
  fat16_get_stats_bytes(total, free);
  return 0;
}

int sys_chdir_call(registers_t *regs) {
  const char *path = (const char *)regs->ebx;
  vfs_node_t *node = vfs_resolve_path(path);
  if (node && (node->flags & 0x7) == VFS_DIRECTORY) {
    if (path[0] == '/') {
      strcpy(current_process->cwd, path);
    } else {
      strcat(current_process->cwd, "/");
      strcat(current_process->cwd, path);
    }
    return 0;
  }
  return -ENOENT;
}

int sys_unlink_call(registers_t *regs) {
  return unlink_vfs(vfs_root, (const char *)regs->ebx);
}

int sys_mkdir_call(registers_t *regs) {
  return mkdir_vfs(vfs_root, (const char *)regs->ebx, 0);
}

int sys_getcwd_call(registers_t *regs) {
  char *buf = (char *)regs->ebx;
  uint32_t size = (uint32_t)regs->ecx;
  if (!buf || size < strlen(current_process->cwd) + 1)
    return -ERANGE;
  strcpy(buf, current_process->cwd);
  return 0;
}

int sys_stat_call(registers_t *regs) {
  const char *path = (const char *)regs->ebx;
  struct stat *st = (struct stat *)regs->ecx;
  vfs_node_t *node = vfs_resolve_path(path);
  if (node && st) {
    st->st_dev = 0;
    st->st_ino = node->inode;
    st->st_mode = node->flags;
    st->st_nlink = 1;
    st->st_uid = node->uid;
    st->st_gid = node->gid;
    st->st_size = node->length;
    st->st_atime = 0;
    st->st_mtime = 0;
    st->st_ctime = 0;
    return 0;
  }
  return -ENOENT;
}

int sys_fstat_call(registers_t *regs) {
  int fd = (int)regs->ebx;
  struct stat *st = (struct stat *)regs->ecx;
  if (fd >= 0 && fd < MAX_PROCESS_FILES && current_process->fd_table[fd] &&
      st) {
    vfs_node_t *node = current_process->fd_table[fd];
    st->st_dev = 0;
    st->st_ino = node->inode;
    st->st_mode = node->flags;
    st->st_nlink = 1;
    st->st_uid = node->uid;
    st->st_gid = node->gid;
    st->st_size = node->length;
    st->st_atime = 0;
    st->st_mtime = 0;
    st->st_ctime = 0;
    return 0;
  }
  return -EBADF;
}

int sys_fstatat_call(registers_t *regs) {
  int dirfd = (int)regs->ebx;
  const char *path = (const char *)regs->ecx;
  struct stat *st = (struct stat *)regs->edx;

  vfs_node_t *dir_node = vfs_root;
  if (dirfd != AT_FDCWD) {
    if (dirfd < 0 || dirfd >= MAX_PROCESS_FILES ||
        !current_process->fd_table[dirfd])
      return -EBADF;
    dir_node = current_process->fd_table[dirfd];
  }

  vfs_node_t *node = vfs_resolve_path_relative(dir_node, path);
  if (node && st) {
    st->st_dev = 0;
    st->st_ino = node->inode;
    st->st_mode = node->flags;
    st->st_nlink = 1;
    st->st_uid = node->uid;
    st->st_gid = node->gid;
    st->st_size = node->length;
    st->st_atime = 0;
    st->st_mtime = 0;
    st->st_ctime = 0;
    return 0;
  }
  return -ENOENT;
}

int sys_seek(registers_t *regs) { return 0; }

int sys_dup_call(registers_t *regs) {
  int oldfd = (int)regs->ebx;
  if (oldfd >= 0 && oldfd < MAX_PROCESS_FILES &&
      current_process->fd_table[oldfd]) {
    for (int j = 0; j < MAX_PROCESS_FILES; j++) {
      if (current_process->fd_table[j] == 0) {
        current_process->fd_table[j] = current_process->fd_table[oldfd];
        current_process->fd_table[j]->ref_count++;
        return j;
      }
    }
  }
  return -EMFILE;
}

int sys_dup2_call(registers_t *regs) {
  int oldfd = (int)regs->ebx;
  int newfd = (int)regs->ecx;
  if (oldfd >= 0 && oldfd < MAX_PROCESS_FILES && newfd >= 0 &&
      newfd < MAX_PROCESS_FILES && current_process->fd_table[oldfd]) {
    if (current_process->fd_table[newfd])
      close_vfs(current_process->fd_table[newfd]);
    current_process->fd_table[newfd] = current_process->fd_table[oldfd];
    current_process->fd_table[newfd]->ref_count++;
    return newfd;
  }
  return -EBADF;
}

int sys_gettime_call(registers_t *regs) {
  rtc_time_t *time = (rtc_time_t *)regs->ebx;
  if (time) {
    rtc_read(time);
    return 0;
  }
  return -EFAULT;
}

int sys_uptime_call(registers_t *regs) { return (int)tick; }

int sys_sleep_call(registers_t *regs) {
  uint32_t ticks = regs->ebx;
  current_process->sleep_until = tick + ticks;
  current_process->state = PROCESS_SLEEPING;
  schedule();
  return 0;
}

int sys_uname_call(registers_t *regs) {
  struct uname_buf {
    char sysname[32];
    char nodename[32];
    char release[32];
    char version[32];
    char machine[32];
  } *buf = (struct uname_buf *)regs->ebx;
  if (buf) {
    strcpy(buf->sysname, sysname);
    strcpy(buf->nodename, system_hostname);
    strcpy(buf->release, release);
    strcpy(buf->version, version);
    strcpy(buf->machine, machine);
    return 0;
  }
  return -EFAULT;
}

int sys_getuid_call(registers_t *regs) { return 0; }
int sys_geteuid_call(registers_t *regs) { return 0; }
int sys_getppid_call(registers_t *regs) {
  return current_process->parent ? current_process->parent->id : 0;
}

int sys_access_call(registers_t *regs) {
  vfs_node_t *node = vfs_resolve_path((const char *)regs->ebx);
  return node ? 0 : -ENOENT;
}
int sys_chmod_call(registers_t *regs) { return 0; }
int sys_ioctl_call(registers_t *regs) { return 0; }
int sys_alarm_call(registers_t *regs);

int sys_hostname_call(registers_t *regs) {
  char *name = (char *)regs->ebx;
  uint32_t len = regs->ecx;
  if (name && len > 0) {
    uint32_t i = 0;
    while (i < len - 1 && system_hostname[i]) {
      name[i] = system_hostname[i];
      i++;
    }
    name[i] = 0;
    return 0;
  }
  return -EFAULT;
}

int sys_meminfo_call(registers_t *regs) {
  struct mem_info {
    uint32_t total, free, used;
  } *info = (struct mem_info *)regs->ebx;
  if (info) {
    uint32_t total_blocks = pmm_get_block_count();
    uint32_t free_blocks = pmm_get_free_block_count();
    info->total = total_blocks * 4096;
    info->free = free_blocks * 4096;
    info->used = (total_blocks - free_blocks) * 4096;
    return 0;
  }
  return -EFAULT;
}

int sys_getpgrp_call(registers_t *regs) { return current_process->pgid; }

int sys_setpgrp_call(registers_t *regs) {
  current_process->pgid = current_process->id;
  return 0;
}

int sys_setpgid_call(registers_t *regs) {
  uint32_t pid = regs->ebx;
  uint32_t pgid = regs->ecx;

  if (pid == 0)
    pid = current_process->id;
  if (pgid == 0)
    pgid = pid;

  if (pid == current_process->id) {
    current_process->pgid = pgid;
    return 0;
  }
  return -ENOSYS;
}

int sys_getpgid_call(registers_t *regs) {
  uint32_t pid = regs->ebx;
  if (pid == 0)
    return current_process->pgid;
  return current_process->pgid;
}

int sys_setsid_call(registers_t *regs) {
  if (current_process->pgid == current_process->id)
    return -EPERM;
  current_process->sid = current_process->id;
  current_process->pgid = current_process->id;
  return current_process->sid;
}

int sys_getsid_call(registers_t *regs) {
  uint32_t pid = regs->ebx;
  if (pid == 0)
    return current_process->sid;
  return current_process->sid;
}

int sys_sync_call(registers_t *regs) { return 0; }

int sys_rmdir_call(registers_t *regs) {
  if (!(current_process->pledges & PLEDGE_CPATH))
    return -EPERM;
  return rmdir_vfs(vfs_root, (const char *)regs->ebx);
}

int sys_pledge_call(registers_t *regs) {
  current_process->pledges &= (uint32_t)regs->ebx;
  return 0;
}

int sys_unveil_call(registers_t *regs) {
  const char *path = (const char *)regs->ebx;
  uint32_t perms = (uint32_t)regs->ecx;
  auto node = (struct process::unveil_node *)kmalloc(
      sizeof(struct process::unveil_node));
  strcpy(node->path, path);
  node->permissions = perms;
  node->next = current_process->unveils;
  current_process->unveils = node;
  return 0;
}

int sys_shmget_call(registers_t *regs) {
  return sys_shmget(regs->ebx, regs->ecx, regs->edx);
}
int sys_shmat_call(registers_t *regs) {
  return (uint32_t)(uintptr_t)sys_shmat(regs->ebx);
}
int sys_shmdt_call(registers_t *regs) { return sys_shmdt((void *)regs->ebx); }

// Forward declarations for new syscalls
int sys_waitpid_call(registers_t *regs);
int sys_waitid_call(registers_t *regs);
int sys__exit_call(registers_t *regs);
int sys_sigaction_call(registers_t *regs);
int sys_sigprocmask_call(registers_t *regs);
int sys_sigpending_call(registers_t *regs);
int sys_sigsuspend_call(registers_t *regs);
int sys_sigwait_call(registers_t *regs);
int sys_pause_call(registers_t *regs);
int sys_abort_call(registers_t *regs);
int sys_posix_spawn_call(registers_t *regs);
int sys_nanosleep_call(registers_t *regs);
int sys_clock_gettime_call(registers_t *regs);
int sys_gettimeofday_call(registers_t *regs);
int sys_pread_call(registers_t *regs);
int sys_pwrite_call(registers_t *regs);
int sys_lseek_call(registers_t *regs);
int sys_truncate_call(registers_t *regs);
int sys_ftruncate_call(registers_t *regs);
int sys_link_call(registers_t *regs);
int sys_symlink_call(registers_t *regs);
int sys_readlink_call(registers_t *regs);
int sys_lstat_call(registers_t *regs);
int sys_mkdirat_call(registers_t *regs);
int sys_unlinkat_call(registers_t *regs);
int sys_renameat_call(registers_t *regs);
int sys_rename_call(registers_t *regs);
int sys_fchmod_call(registers_t *regs);
int sys_chown_call(registers_t *regs);
int sys_fchown_call(registers_t *regs);
int sys_fcntl_call(registers_t *regs);
int sys_fchdir_call(registers_t *regs);
int sys_getdents_call(registers_t *regs);
int sys_times_call(registers_t *regs);
// Phase 11-12
int sys_mprotect_call(registers_t *regs);
int sys_msync_call(registers_t *regs);
int sys_mlock_call(registers_t *regs);
int sys_munlock_call(registers_t *regs);
int sys_posix_madvise_call(registers_t *regs);
int sys_posix_memalign_call(registers_t *regs);
int sys_sysconf_call(registers_t *regs);
int sys_pathconf_call(registers_t *regs);
int sys_fpathconf_call(registers_t *regs);
int sys_confstr_call(registers_t *regs);
int sys_getrlimit_call(registers_t *regs);
int sys_setrlimit_call(registers_t *regs);

// The Syscall Jump Table
// ============================================================================
// New POSIX Syscall Implementations (Phases 1-12)
// ============================================================================

// ----------------------------------------------------------------------------
// Phase 1: Process Management & Signals
// ----------------------------------------------------------------------------

// These call existing kernel functions from process.cpp/signal.cpp
int sys_waitpid_call(registers_t *regs) {
  return sys_waitpid((int)regs->ebx, (int *)regs->ecx, (int)regs->edx);
}

int sys_waitid_call(registers_t *regs) {
  return sys_waitid((int)regs->ebx, (int)regs->ecx, (void *)regs->edx,
                    (int)regs->esi);
}

int sys__exit_call(registers_t *regs) {
  sys__exit((int)regs->ebx);
  return 0;
}

int sys_posix_spawn_call(registers_t *regs) {
  return sys_posix_spawn((int *)regs->ebx, (const char *)regs->ecx,
                         (void *)regs->edx, (void *)regs->esi,
                         (char *const *)regs->edi, 0);
}

int sys_sigaction_call(registers_t *regs) {
  return sys_sigaction((int)regs->ebx, (const struct sigaction *)regs->ecx,
                       (struct sigaction *)regs->edx);
}

int sys_sigprocmask_call(registers_t *regs) {
  return sys_sigprocmask((int)regs->ebx, (const sigset_t *)regs->ecx,
                         (sigset_t *)regs->edx);
}

int sys_sigpending_call(registers_t *regs) {
  return sys_sigpending((sigset_t *)regs->ebx);
}

int sys_sigsuspend_call(registers_t *regs) {
  return sys_sigsuspend((const sigset_t *)regs->ebx);
}

int sys_sigwait_call(registers_t *regs) {
  return sys_sigwait((const sigset_t *)regs->ebx, (int *)regs->ecx);
}

// pause/abort from process.cpp
int sys_pause_call(registers_t *regs) {
  (void)regs;
  return -1; /* stub */
}
int sys_abort_call(registers_t *regs) {
  (void)regs;
  return -1; /* stub */
}

int sys_alarm_call(registers_t *regs) {
  return sys_alarm((unsigned int)regs->ebx);
}

// ----------------------------------------------------------------------------
// Phase 2: File & Directory Operations
// These use file_ops.cpp functions (without sys_ prefix)
// ----------------------------------------------------------------------------
extern "C" int pread(int fd, void *buf, size_t count, int offset);
int sys_pread_call(registers_t *regs) {
  return pread((int)regs->ebx, (void *)regs->ecx, (size_t)regs->edx,
               (int)regs->esi);
}

extern "C" int pwrite(int fd, const void *buf, size_t count, int offset);
int sys_pwrite_call(registers_t *regs) {
  return pwrite((int)regs->ebx, (const void *)regs->ecx, (size_t)regs->edx,
                (int)regs->esi);
}

extern "C" int lseek(int fd, int offset, int whence);
int sys_lseek_call(registers_t *regs) {
  return lseek((int)regs->ebx, (int)regs->ecx, (int)regs->edx);
}

extern "C" int truncate(const char *path, int length);
int sys_truncate_call(registers_t *regs) {
  return truncate((const char *)regs->ebx, (int)regs->ecx);
}

extern "C" int ftruncate(int fd, int length);
int sys_ftruncate_call(registers_t *regs) {
  return ftruncate((int)regs->ebx, (int)regs->ecx);
}

extern "C" int link(const char *oldpath, const char *newpath);
int sys_link_call(registers_t *regs) {
  return link((const char *)regs->ebx, (const char *)regs->ecx);
}

extern "C" int symlink(const char *target, const char *linkpath);
int sys_symlink_call(registers_t *regs) {
  return symlink((const char *)regs->ebx, (const char *)regs->ecx);
}

extern "C" int readlink(const char *pathname, char *buf, size_t bufsiz);
int sys_readlink_call(registers_t *regs) {
  return readlink((const char *)regs->ebx, (char *)regs->ecx,
                  (size_t)regs->edx);
}

extern "C" int lstat(const char *pathname, void *statbuf);
int sys_lstat_call(registers_t *regs) {
  return lstat((const char *)regs->ebx, (void *)regs->ecx);
}

extern "C" int mkdirat(int dirfd, const char *pathname, int mode);
int sys_mkdirat_call(registers_t *regs) {
  return mkdirat((int)regs->ebx, (const char *)regs->ecx, (int)regs->edx);
}

extern "C" int unlinkat(int dirfd, const char *pathname, int flags);
int sys_unlinkat_call(registers_t *regs) {
  return unlinkat((int)regs->ebx, (const char *)regs->ecx, (int)regs->edx);
}

extern "C" int renameat(int olddirfd, const char *oldpath, int newdirfd,
                        const char *newpath);
int sys_renameat_call(registers_t *regs) {
  return renameat((int)regs->ebx, (const char *)regs->ecx, (int)regs->edx,
                  (const char *)regs->esi);
}

extern "C" int rename(const char *oldpath, const char *newpath);
int sys_rename_call(registers_t *regs) {
  return rename((const char *)regs->ebx, (const char *)regs->ecx);
}

extern "C" int fchmod(int fd, int mode);
int sys_fchmod_call(registers_t *regs) {
  return fchmod((int)regs->ebx, (int)regs->ecx);
}

extern "C" int chown(const char *pathname, int owner, int group);
int sys_chown_call(registers_t *regs) {
  return chown((const char *)regs->ebx, (int)regs->ecx, (int)regs->edx);
}

extern "C" int fchown(int fd, int owner, int group);
int sys_fchown_call(registers_t *regs) {
  return fchown((int)regs->ebx, (int)regs->ecx, (int)regs->edx);
}

extern "C" int fcntl(int fd, int cmd, int arg);
int sys_fcntl_call(registers_t *regs) {
  return fcntl((int)regs->ebx, (int)regs->ecx, (int)regs->edx);
}

// ----------------------------------------------------------------------------
// Phase 3: Directory Management
// ----------------------------------------------------------------------------
extern "C" int fchdir(int fd);
int sys_fchdir_call(registers_t *regs) { return fchdir((int)regs->ebx); }

extern "C" int getdents(int fd, void *dirp, unsigned int count);
int sys_getdents_call(registers_t *regs) {
  return getdents((int)regs->ebx, (void *)regs->ecx, (unsigned int)regs->edx);
}

// ----------------------------------------------------------------------------
// Phase 4: Pthreads
// ----------------------------------------------------------------------------
extern "C" int pthread_create(void *thread, const void *attr,
                              void *(*start_routine)(void *), void *arg);
int sys_pthread_create_call(registers_t *regs) {
  return pthread_create((void *)regs->ebx, (const void *)regs->ecx,
                        (void *(*)(void *))regs->edx, (void *)regs->esi);
}

extern "C" void pthread_exit(void *retval);
int sys_pthread_exit_call(registers_t *regs) {
  pthread_exit((void *)regs->ebx);
  return 0;
}

extern "C" int pthread_join(unsigned int thread, void **retval);
int sys_pthread_join_call(registers_t *regs) {
  return pthread_join((unsigned int)regs->ebx, (void **)regs->ecx);
}

extern "C" int pthread_detach(unsigned int thread);
int sys_pthread_detach_call(registers_t *regs) {
  return pthread_detach((unsigned int)regs->ebx);
}

extern "C" int pthread_mutex_init(void *mutex, const void *attr);
int sys_pthread_mutex_init_call(registers_t *regs) {
  return pthread_mutex_init((void *)regs->ebx, (const void *)regs->ecx);
}

extern "C" int pthread_mutex_lock(void *mutex);
int sys_pthread_mutex_lock_call(registers_t *regs) {
  return pthread_mutex_lock((void *)regs->ebx);
}

extern "C" int pthread_mutex_unlock(void *mutex);
int sys_pthread_mutex_unlock_call(registers_t *regs) {
  return pthread_mutex_unlock((void *)regs->ebx);
}

extern "C" int pthread_cond_init(void *cond, const void *attr);
int sys_pthread_cond_init_call(registers_t *regs) {
  return pthread_cond_init((void *)regs->ebx, (const void *)regs->ecx);
}

extern "C" int pthread_cond_wait(void *cond, void *mutex);
int sys_pthread_cond_wait_call(registers_t *regs) {
  return pthread_cond_wait((void *)regs->ebx, (void *)regs->ecx);
}

extern "C" int pthread_cond_signal(void *cond);
int sys_pthread_cond_signal_call(registers_t *regs) {
  return pthread_cond_signal((void *)regs->ebx);
}

// ----------------------------------------------------------------------------
// Phase 5: IPC (Semaphores & Message Queues)
// ----------------------------------------------------------------------------
extern "C" int sem_open(const char *name, int oflag, unsigned int mode,
                        unsigned int value);
int sys_sem_open_call(registers_t *regs) {
  return sem_open((const char *)regs->ebx, (int)regs->ecx,
                  (unsigned int)regs->edx, (unsigned int)regs->esi);
}

extern "C" int sem_close(void *sem);
int sys_sem_close_call(registers_t *regs) {
  return sem_close((void *)regs->ebx);
}

extern "C" int sem_unlink(const char *name);
int sys_sem_unlink_call(registers_t *regs) {
  return sem_unlink((const char *)regs->ebx);
}

extern "C" int sem_wait(void *sem);
int sys_sem_wait_call(registers_t *regs) { return sem_wait((void *)regs->ebx); }

extern "C" int sem_post(void *sem);
int sys_sem_post_call(registers_t *regs) { return sem_post((void *)regs->ebx); }

extern "C" int mq_open(const char *name, int oflag);
int sys_mq_open_call(registers_t *regs) {
  return mq_open((const char *)regs->ebx, (int)regs->ecx);
}

extern "C" int mq_close(int mqdes);
int sys_mq_close_call(registers_t *regs) { return mq_close((int)regs->ebx); }

extern "C" int mq_unlink(const char *name);
int sys_mq_unlink_call(registers_t *regs) {
  return mq_unlink((const char *)regs->ebx);
}

extern "C" int mq_send(int mqdes, const char *msg_ptr, size_t msg_len,
                       unsigned int msg_prio);
int sys_mq_send_call(registers_t *regs) {
  return mq_send((int)regs->ebx, (const char *)regs->ecx, (size_t)regs->edx,
                 (unsigned int)regs->esi);
}

extern "C" int mq_receive(int mqdes, char *msg_ptr, size_t msg_len,
                          unsigned int *msg_prio);
int sys_mq_receive_call(registers_t *regs) {
  return mq_receive((int)regs->ebx, (char *)regs->ecx, (size_t)regs->edx,
                    (unsigned int *)regs->esi);
}

// ----------------------------------------------------------------------------
// Phase 6: Sockets
// ----------------------------------------------------------------------------
extern "C" int listen(int sockfd, int backlog);
int sys_listen_call(registers_t *regs) {
  return listen((int)regs->ebx, (int)regs->ecx);
}

extern "C" int send(int sockfd, const void *buf, size_t len, int flags);
int sys_send_call(registers_t *regs) {
  return send((int)regs->ebx, (const void *)regs->ecx, (size_t)regs->edx,
              (int)regs->esi);
}

extern "C" int recv(int sockfd, void *buf, size_t len, int flags);
int sys_recv_call(registers_t *regs) {
  return recv((int)regs->ebx, (void *)regs->ecx, (size_t)regs->edx,
              (int)regs->esi);
}

extern "C" int sendto(int sockfd, const void *buf, size_t len, int flags,
                      const void *dest_addr, unsigned int addrlen);
int sys_sendto_call(registers_t *regs) {
  return sendto((int)regs->ebx, (const void *)regs->ecx, (size_t)regs->edx,
                (int)regs->esi, (const void *)regs->edi, 0);
}

extern "C" int recvfrom(int sockfd, void *buf, size_t len, int flags,
                        void *src_addr, unsigned int *addrlen);
int sys_recvfrom_call(registers_t *regs) {
  return recvfrom((int)regs->ebx, (void *)regs->ecx, (size_t)regs->edx,
                  (int)regs->esi, (void *)regs->edi, 0);
}

extern "C" int getsockname(int sockfd, void *addr, unsigned int *addrlen);
int sys_getsockname_call(registers_t *regs) {
  return getsockname((int)regs->ebx, (void *)regs->ecx,
                     (unsigned int *)regs->edx);
}

extern "C" int getpeername(int sockfd, void *addr, unsigned int *addrlen);
int sys_getpeername_call(registers_t *regs) {
  return getpeername((int)regs->ebx, (void *)regs->ecx,
                     (unsigned int *)regs->edx);
}

extern "C" int setsockopt(int sockfd, int level, int optname,
                          const void *optval, unsigned int optlen);
int sys_setsockopt_call(registers_t *regs) {
  return setsockopt((int)regs->ebx, (int)regs->ecx, (int)regs->edx,
                    (const void *)regs->esi, (unsigned int)regs->edi);
}

extern "C" int getsockopt(int sockfd, int level, int optname, void *optval,
                          unsigned int *optlen);
int sys_getsockopt_call(registers_t *regs) {
  return getsockopt((int)regs->ebx, (int)regs->ecx, (int)regs->edx,
                    (void *)regs->esi, (unsigned int *)regs->edi);
}

extern "C" int shutdown(int sockfd, int how);
int sys_shutdown_call(registers_t *regs) {
  return shutdown((int)regs->ebx, (int)regs->ecx);
}

// ----------------------------------------------------------------------------
// Phase 7: Time
// ----------------------------------------------------------------------------
extern "C" int nanosleep(const void *req, void *rem);
int sys_nanosleep_call(registers_t *regs) {
  return nanosleep((const void *)regs->ebx, (void *)regs->ecx);
}

extern "C" int clock_gettime(int clk_id, void *tp);
int sys_clock_gettime_call(registers_t *regs) {
  return clock_gettime((int)regs->ebx, (void *)regs->ecx);
}

extern "C" int gettimeofday(void *tv, void *tz);
int sys_gettimeofday_call(registers_t *regs) {
  return gettimeofday((void *)regs->ebx, (void *)regs->ecx);
}

extern "C" long times(void *buf);
int sys_times_call(registers_t *regs) { return (int)times((void *)regs->ebx); }

// ----------------------------------------------------------------------------
// Phase 8: User Management
// ----------------------------------------------------------------------------
extern "C" int setuid(unsigned int uid);
int sys_setuid_call(registers_t *regs) {
  return setuid((unsigned int)regs->ebx);
}
extern "C" int setgid(unsigned int gid);
int sys_setgid_call(registers_t *regs) {
  return setgid((unsigned int)regs->ebx);
}
extern "C" int seteuid(unsigned int euid);
int sys_seteuid_call(registers_t *regs) {
  return seteuid((unsigned int)regs->ebx);
}
extern "C" int setegid(unsigned int egid);
int sys_setegid_call(registers_t *regs) {
  return setegid((unsigned int)regs->ebx);
}
extern "C" int setresuid(unsigned int ruid, unsigned int euid,
                         unsigned int suid);
int sys_setresuid_call(registers_t *regs) {
  return setresuid((unsigned int)regs->ebx, (unsigned int)regs->ecx,
                   (unsigned int)regs->edx);
}

// ----------------------------------------------------------------------------
// Phase 9-10: Termios/Select
// ----------------------------------------------------------------------------
extern "C" int tcgetattr(int fd, void *termios_p);
int sys_tcgetattr_call(registers_t *regs) {
  return tcgetattr((int)regs->ebx, (void *)regs->ecx);
}
extern "C" int tcsetattr(int fd, int optional_actions, const void *termios_p);
int sys_tcsetattr_call(registers_t *regs) {
  return tcsetattr((int)regs->ebx, (int)regs->ecx, (const void *)regs->edx);
}
extern "C" int select(int nfds, void *readfds, void *writefds, void *exceptfds,
                      void *timeout);
int sys_select_call(registers_t *regs) {
  return select((int)regs->ebx, (void *)regs->ecx, (void *)regs->edx,
                (void *)regs->esi, (void *)regs->edi);
}
extern "C" int poll(void *fds, unsigned int nfds, int timeout);
int sys_poll_call(registers_t *regs) {
  return poll((void *)regs->ebx, (unsigned int)regs->ecx, (int)regs->edx);
}

// ----------------------------------------------------------------------------
// Phase 11-12: Memory/Config
// ----------------------------------------------------------------------------
extern "C" int sys_mprotect(void *addr, size_t len, int prot);
int sys_mprotect_call(registers_t *regs) {
  return sys_mprotect((void *)regs->ebx, (size_t)regs->ecx, (int)regs->edx);
}
extern "C" int sys_msync(void *addr, size_t len, int flags);
int sys_msync_call(registers_t *regs) {
  return sys_msync((void *)regs->ebx, (size_t)regs->ecx, (int)regs->edx);
}
extern "C" int sys_mlock(const void *addr, size_t len);
int sys_mlock_call(registers_t *regs) {
  return sys_mlock((const void *)regs->ebx, (size_t)regs->ecx);
}
extern "C" int sys_munlock(const void *addr, size_t len);
int sys_munlock_call(registers_t *regs) {
  return sys_munlock((const void *)regs->ebx, (size_t)regs->ecx);
}
extern "C" int sys_posix_madvise(void *addr, size_t len, int advice);
int sys_posix_madvise_call(registers_t *regs) {
  return sys_posix_madvise((void *)regs->ebx, (size_t)regs->ecx,
                           (int)regs->edx);
}
extern "C" int sys_posix_memalign(void **memptr, size_t alignment, size_t size);
int sys_posix_memalign_call(registers_t *regs) {
  return sys_posix_memalign((void **)regs->ebx, (size_t)regs->ecx,
                            (size_t)regs->edx);
}

extern "C" long sysconf(int name);
int sys_sysconf_call(registers_t *regs) { return (int)sysconf((int)regs->ebx); }
extern "C" long pathconf(const char *path, int name);
int sys_pathconf_call(registers_t *regs) {
  return (int)pathconf((const char *)regs->ebx, (int)regs->ecx);
}
extern "C" long fpathconf(int fd, int name);
int sys_fpathconf_call(registers_t *regs) {
  return (int)fpathconf((int)regs->ebx, (int)regs->ecx);
}
extern "C" size_t confstr(int name, char *buf, size_t len);
int sys_confstr_call(registers_t *regs) {
  return (int)confstr((int)regs->ebx, (char *)regs->ecx, (size_t)regs->edx);
}
extern "C" int getrlimit(int resource, void *rlim);
int sys_getrlimit_call(registers_t *regs) {
  return getrlimit((int)regs->ebx, (void *)regs->ecx);
}
extern "C" int setrlimit(int resource, const void *rlim);
int sys_setrlimit_call(registers_t *regs) {
  return setrlimit((int)regs->ebx, (const struct rlimit *)regs->ecx);
}

// ============================================================================
// Syscall Table & Handler (Moved to end to see all declarations)
// ============================================================================
static syscall_ptr syscall_table[] = {
    sys_print_serial,   // 0
    sys_get_pid,        // 1
    sys_open,           // 2
    sys_read,           // 3
    sys_write,          // 4
    sys_close,          // 5
    sys_sbrk,           // 6
    sys_mmap,           // 7
    sys_munmap,         // 8
    sys_fork,           // 9
    sys_execve,         // 10
    sys_wait,           // 11
    sys_exit,           // 12
    sys_pipe_call,      // 13
    sys_socket_call,    // 14
    sys_bind_call,      // 15
    sys_connect_call,   // 16
    sys_accept_call,    // 17
    sys_signal_call,    // 18
    sys_kill_call,      // 19
    sys_sigreturn_call, // 20
    sys_readdir_call,   // 21
    sys_statfs_call,    // 22
    sys_chdir_call,     // 23
    sys_unlink_call,    // 24
    sys_mkdir_call,     // 25
    sys_getcwd_call,    // 26
    sys_stat_call,      // 27
    sys_fstat_call,     // 28
    sys_seek,           // 29
    sys_dup_call,       // 30
    sys_dup2_call,      // 31
    sys_gettime_call,   // 32
    sys_uptime_call,    // 33
    sys_sleep_call,     // 34
    sys_uname_call,     // 35
    sys_getuid_call,    // 36
    sys_geteuid_call,   // 37
    sys_getppid_call,   // 38
    sys_rename_call,    // 39
    sys_truncate_call,  // 40
    sys_access_call,    // 41
    sys_chmod_call,     // 42
    sys_ioctl_call,     // 43
    sys_alarm_call,     // 44
    sys_hostname_call,  // 45
    sys_meminfo_call,   // 46
    nullptr,            // 47 (procinfo)
    sys_symlink_call,   // 48
    sys_readlink_call,  // 49
    sys_getpgrp_call,   // 50
    sys_setpgid_call,   // 51
    sys_setsid_call,    // 52
    sys_getpgid_call,   // 53
    sys_getsid_call,    // 54
    sys_sync_call,      // 55
    sys_rmdir_call,     // 56
    sys_pledge_call,    // 57
    sys_unveil_call,    // 58
    sys_shmget_call,    // 59
    sys_shmat_call,     // 60
    sys_shmdt_call,     // 61
    sys_openat,         // 62
    sys_fstatat_call,   // 63
    sys_readv,          // 64
    sys_writev,         // 65
    // New POSIX syscalls (Phase 1-3)
    sys_waitpid_call,       // 66
    sys_waitid_call,        // 67
    sys__exit_call,         // 68
    sys_sigaction_call,     // 69
    sys_sigprocmask_call,   // 70
    sys_sigpending_call,    // 71
    sys_sigsuspend_call,    // 72
    sys_sigwait_call,       // 73
    sys_pause_call,         // 74
    sys_abort_call,         // 75
    sys_posix_spawn_call,   // 76
    sys_nanosleep_call,     // 77
    sys_clock_gettime_call, // 78
    sys_gettimeofday_call,  // 79
    sys_pread_call,         // 80
    sys_pwrite_call,        // 81
    sys_lseek_call,         // 82
    sys_ftruncate_call,     // 83
    sys_link_call,          // 84
    sys_lstat_call,         // 85
    sys_mkdirat_call,       // 86
    sys_unlinkat_call,      // 87
    sys_renameat_call,      // 88
    sys_fchmod_call,        // 89
    sys_chown_call,         // 90
    sys_fchown_call,        // 91
    sys_fcntl_call,         // 92
    sys_fchdir_call,        // 93
    sys_getdents_call,      // 94
    sys_times_call,         // 95
    // Phase 4: Pthreads
    sys_pthread_create_call,       // 96
    sys_pthread_exit_call,         // 97
    sys_pthread_join_call,         // 98
    sys_pthread_detach_call,       // 99
    sys_pthread_mutex_init_call,   // 100
    sys_pthread_mutex_lock_call,   // 101
    sys_pthread_mutex_unlock_call, // 102
    sys_pthread_cond_init_call,    // 103
    sys_pthread_cond_wait_call,    // 104
    sys_pthread_cond_signal_call,  // 105
    // Phase 5: IPC
    sys_sem_open_call,   // 106
    sys_sem_close_call,  // 107
    sys_sem_unlink_call, // 108
    sys_sem_wait_call,   // 109
    sys_sem_post_call,   // 110
    sys_mq_open_call,    // 111
    sys_mq_close_call,   // 112
    sys_mq_unlink_call,  // 113
    sys_mq_send_call,    // 114
    sys_mq_receive_call, // 115
    // Phase 6: Sockets
    sys_listen_call,      // 116
    sys_send_call,        // 117
    sys_recv_call,        // 118
    sys_sendto_call,      // 119
    sys_recvfrom_call,    // 120
    sys_getsockname_call, // 121
    sys_getpeername_call, // 122
    sys_setsockopt_call,  // 123
    sys_getsockopt_call,  // 124
    sys_shutdown_call,    // 125
    // Phase 8: User
    sys_setuid_call,    // 126
    sys_setgid_call,    // 127
    sys_seteuid_call,   // 128
    sys_setegid_call,   // 129
    sys_setresuid_call, // 130
    // Phase 9-10: Termios/Select
    sys_tcgetattr_call,                                   // 131
    sys_tcsetattr_call,                                   // 132
    sys_select_call,                                      // 133
    sys_poll_call,                                        // 134
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, // 135-140
    // Phase 11-12: Memory/Config
    sys_mprotect_call, // 141
    sys_msync_call,    // 142
    sys_mlock_call,    // 143
    sys_sysconf_call   // 144
};

static const int num_syscalls = sizeof(syscall_table) / sizeof(syscall_ptr);

void init_syscalls() { register_interrupt_handler(0x80, syscall_handler); }

void syscall_handler(registers_t *regs) {
  if (regs->eax < (uint32_t)num_syscalls && syscall_table[regs->eax]) {
    serial_log_hex("SYSCALL: Called ID ", regs->eax);
    regs->eax = syscall_table[regs->eax](regs);
  } else {
    serial_log_hex("SYSCALL: Unknown ID ", regs->eax);
    regs->eax = -ENOSYS;
  }
}
