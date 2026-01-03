// ============================================================================
// sysconf.cpp - System Configuration Implementation
// Entirely hand-crafted for Retro-OS kernel
// ============================================================================

#include "../drivers/serial.h"
#include "../include/errno.h"
#include "../include/time.h"
#include "../include/unistd.h"
#include "pmm.h"
#include "process.h"

extern "C" {

// Use definitions from unistd.h instead of duplicating here
// Extended _SC_* constants not in unistd.h
#define _SC_REALTIME_SIGNALS 15
#define _SC_PRIORITY_SCHEDULING 16
#define _SC_TIMERS 17
#define _SC_ASYNCHRONOUS_IO 18
#define _SC_PRIORITIZED_IO 19
#define _SC_SYNCHRONIZED_IO 20
#define _SC_FSYNC 21
#define _SC_MAPPED_FILES 22
#define _SC_MEMLOCK 23
#define _SC_MEMLOCK_RANGE 24
#define _SC_MEMORY_PROTECTION 25
#define _SC_MESSAGE_PASSING 26
#define _SC_SEMAPHORES 27
#define _SC_SHARED_MEMORY_OBJECTS 28
#define _SC_AIO_LISTIO_MAX 29
#define _SC_AIO_MAX 30
#define _SC_AIO_PRIO_DELTA_MAX 31
#define _SC_DELAYTIMER_MAX 32
#define _SC_MQ_OPEN_MAX 33
#define _SC_MQ_PRIO_MAX 34
#define _SC_RTSIG_MAX 35
#define _SC_SEM_NSEMS_MAX 36
#define _SC_SEM_VALUE_MAX 37
#define _SC_SIGQUEUE_MAX 38
#define _SC_TIMER_MAX 39
#define _SC_BC_BASE_MAX 40
#define _SC_BC_DIM_MAX 41
#define _SC_BC_SCALE_MAX 42
#define _SC_BC_STRING_MAX 43
#define _SC_COLL_WEIGHTS_MAX 44
#define _SC_EXPR_NEST_MAX 45
#define _SC_LINE_MAX 46
#define _SC_RE_DUP_MAX 47
#define _SC_2_VERSION 48
#define _SC_2_C_BIND 49
#define _SC_2_C_DEV 50
#define _SC_2_FORT_DEV 51
#define _SC_2_FORT_RUN 52
#define _SC_2_SW_DEV 53
#define _SC_2_LOCALEDEF 54

// ============================================================================
// pathconf Name Constants
// ============================================================================
#define _PC_LINK_MAX 0
#define _PC_MAX_CANON 1
#define _PC_MAX_INPUT 2
#define _PC_NAME_MAX 3
#define _PC_PATH_MAX 4
#define _PC_PIPE_BUF 5
#define _PC_CHOWN_RESTRICTED 6
#define _PC_NO_TRUNC 7
#define _PC_VDISABLE 8
#define _PC_SYNC_IO 9
#define _PC_ASYNC_IO 10
#define _PC_PRIO_IO 11
#define _PC_FILESIZEBITS 12
#define _PC_REC_INCR_XFER_SIZE 13
#define _PC_REC_MAX_XFER_SIZE 14
#define _PC_REC_MIN_XFER_SIZE 15
#define _PC_REC_XFER_ALIGN 16
#define _PC_SYMLINK_MAX 17

// ============================================================================
// sysconf - Get configuration settings
// ============================================================================
long sysconf(int name) {
  switch (name) {
  case _SC_ARG_MAX:
    return 131072; // 128KB for arguments

  case _SC_CHILD_MAX:
    return 64;

  case _SC_CLK_TCK:
    return 100; // 100Hz timer

  case _SC_NGROUPS_MAX:
    return 16;

  case _SC_OPEN_MAX:
    return MAX_PROCESS_FILES;

  case _SC_STREAM_MAX:
    return MAX_PROCESS_FILES;

  case _SC_TZNAME_MAX:
    return 6;

  case _SC_JOB_CONTROL:
    return 1; // Supported

  case _SC_SAVED_IDS:
    return 1; // Supported

  case _SC_REALTIME_SIGNALS:
    return 1;

  case _SC_VERSION:
    return 200809L; // POSIX.1-2008

  case _SC_PAGESIZE:
    return 4096;

  case _SC_NPROCESSORS_CONF:
  case _SC_NPROCESSORS_ONLN:
    return 1; // Single processor

  case _SC_PHYS_PAGES:
    return pmm_get_block_count();

  case _SC_AVPHYS_PAGES:
    return pmm_get_free_block_count();

  case _SC_MAPPED_FILES:
  case _SC_MEMLOCK:
  case _SC_MEMLOCK_RANGE:
  case _SC_MEMORY_PROTECTION:
  case _SC_MESSAGE_PASSING:
  case _SC_SEMAPHORES:
  case _SC_SHARED_MEMORY_OBJECTS:
  case _SC_TIMERS:
    return 1; // Supported

  case _SC_FSYNC:
  case _SC_SYNCHRONIZED_IO:
    return 1;

  case _SC_MQ_OPEN_MAX:
    return 16;

  case _SC_MQ_PRIO_MAX:
    return 32768;

  case _SC_SEM_NSEMS_MAX:
    return 32;

  case _SC_SEM_VALUE_MAX:
    return 32767;

  case _SC_SIGQUEUE_MAX:
    return 32;

  case _SC_TIMER_MAX:
    return 16;

  case _SC_LINE_MAX:
    return 2048;

  default:
    // errno = EINVAL;
    return -1;
  }
}

// ============================================================================
// pathconf - Get path configuration values
// ============================================================================
long pathconf(const char *path, int name) {
  (void)path;

  switch (name) {
  case _PC_LINK_MAX:
    return 127;

  case _PC_MAX_CANON:
    return 255;

  case _PC_MAX_INPUT:
    return 255;

  case _PC_NAME_MAX:
    return 255;

  case _PC_PATH_MAX:
    return 4096;

  case _PC_PIPE_BUF:
    return 4096;

  case _PC_CHOWN_RESTRICTED:
    return 1; // Chown restricted to root

  case _PC_NO_TRUNC:
    return 1; // Long names error (not truncated)

  case _PC_VDISABLE:
    return 0; // Disable character

  case _PC_FILESIZEBITS:
    return 32; // 32-bit file sizes

  case _PC_SYMLINK_MAX:
    return 255;

  default:
    // errno = EINVAL;
    return -1;
  }
}

long fpathconf(int fd, int name) {
  (void)fd;
  return pathconf("/", name);
}

// ============================================================================
// confstr - Get string configuration values
// ============================================================================
size_t confstr(int name, char *buf, size_t len) {
  const char *value = 0;

  switch (name) {
  case 0: // _CS_PATH
    value = "/bin:/usr/bin";
    break;
  default:
    // errno = EINVAL;
    return 0;
  }

  size_t value_len = 0;
  for (const char *p = value; *p; p++)
    value_len++;
  value_len++; // Include null terminator

  if (buf && len > 0) {
    size_t copy_len = (value_len < len) ? value_len : len - 1;
    for (size_t i = 0; i < copy_len; i++)
      buf[i] = value[i];
    buf[copy_len] = 0;
  }

  return value_len;
}

// ============================================================================
// getrlimit/setrlimit - Resource limits
// ============================================================================
#define RLIMIT_CPU 0         // CPU time in seconds
#define RLIMIT_FSIZE 1       // Maximum file size
#define RLIMIT_DATA 2        // Max data size
#define RLIMIT_STACK 3       // Max stack size
#define RLIMIT_CORE 4        // Max core file size
#define RLIMIT_RSS 5         // Max resident set size
#define RLIMIT_NPROC 6       // Max number of processes
#define RLIMIT_NOFILE 7      // Max number of open files
#define RLIMIT_MEMLOCK 8     // Max locked memory
#define RLIMIT_AS 9          // Max virtual memory
#define RLIMIT_LOCKS 10      // Max file locks
#define RLIMIT_SIGPENDING 11 // Max pending signals
#define RLIMIT_MSGQUEUE 12   // Max message queue bytes
#define RLIMIT_NICE 13       // Max nice value
#define RLIMIT_RTPRIO 14     // Max realtime priority
#define RLIMIT_RTTIME 15     // CPU time for realtime tasks
#define RLIM_NLIMITS 16

#define RLIM_INFINITY ((unsigned long)-1)

struct rlimit {
  unsigned long rlim_cur; // Soft limit
  unsigned long rlim_max; // Hard limit
};

static struct rlimit default_limits[RLIM_NLIMITS] = {
    [RLIMIT_CPU] = {RLIM_INFINITY, RLIM_INFINITY},
    [RLIMIT_FSIZE] = {RLIM_INFINITY, RLIM_INFINITY},
    [RLIMIT_DATA] = {RLIM_INFINITY, RLIM_INFINITY},
    [RLIMIT_STACK] = {8 * 1024 * 1024, RLIM_INFINITY}, // 8MB default stack
    [RLIMIT_CORE] = {0, RLIM_INFINITY},
    [RLIMIT_RSS] = {RLIM_INFINITY, RLIM_INFINITY},
    [RLIMIT_NPROC] = {64, 64},
    [RLIMIT_NOFILE] = {MAX_PROCESS_FILES, MAX_PROCESS_FILES},
    [RLIMIT_MEMLOCK] = {64 * 1024, 64 * 1024},
    [RLIMIT_AS] = {RLIM_INFINITY, RLIM_INFINITY},
    [RLIMIT_LOCKS] = {RLIM_INFINITY, RLIM_INFINITY},
    [RLIMIT_SIGPENDING] = {32, 32},
    [RLIMIT_MSGQUEUE] = {819200, 819200},
    [RLIMIT_NICE] = {0, 0},
    [RLIMIT_RTPRIO] = {0, 0},
    [RLIMIT_RTTIME] = {RLIM_INFINITY, RLIM_INFINITY},
};

int getrlimit(int resource, struct rlimit *rlim) {
  if (!rlim)
    return -EFAULT;

  if (resource < 0 || resource >= RLIM_NLIMITS)
    return -EINVAL;

  *rlim = default_limits[resource];
  return 0;
}

int setrlimit(int resource, const struct rlimit *rlim) {
  if (!rlim)
    return -EFAULT;

  if (resource < 0 || resource >= RLIM_NLIMITS)
    return -EINVAL;

  // Only root can raise limits above current hard limit
  if (current_process && current_process->euid != 0) {
    if (rlim->rlim_max > default_limits[resource].rlim_max)
      return -EPERM;
  }

  default_limits[resource] = *rlim;
  return 0;
}

int prlimit(int pid, int resource, const struct rlimit *new_limit,
            struct rlimit *old_limit) {
  (void)pid; // Only support current process

  if (old_limit) {
    if (getrlimit(resource, old_limit) < 0)
      return -1;
  }

  if (new_limit) {
    if (setrlimit(resource, new_limit) < 0)
      return -1;
  }

  return 0;
}

// ============================================================================
// getrusage - Get resource usage
// ============================================================================
#define RUSAGE_SELF 0
#define RUSAGE_CHILDREN (-1)
#define RUSAGE_THREAD 1

struct rusage {
  struct timeval ru_utime; // User time used
  struct timeval ru_stime; // System time used
  long ru_maxrss;          // Max resident set size
  long ru_ixrss;           // Integral shared memory size
  long ru_idrss;           // Integral unshared data size
  long ru_isrss;           // Integral unshared stack size
  long ru_minflt;          // Page reclaims (soft page faults)
  long ru_majflt;          // Page faults (hard page faults)
  long ru_nswap;           // Swaps
  long ru_inblock;         // Block input operations
  long ru_oublock;         // Block output operations
  long ru_msgsnd;          // Messages sent
  long ru_msgrcv;          // Messages received
  long ru_nsignals;        // Signals received
  long ru_nvcsw;           // Voluntary context switches
  long ru_nivcsw;          // Involuntary context switches
};

int getrusage(int who, struct rusage *usage) {
  if (!usage)
    return -EFAULT;

  // Zero out the structure
  for (size_t i = 0; i < sizeof(struct rusage); i++)
    ((char *)usage)[i] = 0;

  if (!current_process)
    return 0;

  switch (who) {
  case RUSAGE_SELF:
    usage->ru_utime.tv_sec = current_process->utime / 100;
    usage->ru_utime.tv_usec = (current_process->utime % 100) * 10000;
    usage->ru_stime.tv_sec = current_process->stime / 100;
    usage->ru_stime.tv_usec = (current_process->stime % 100) * 10000;
    break;

  case RUSAGE_CHILDREN:
    usage->ru_utime.tv_sec = current_process->cutime / 100;
    usage->ru_utime.tv_usec = (current_process->cutime % 100) * 10000;
    usage->ru_stime.tv_sec = current_process->cstime / 100;
    usage->ru_stime.tv_usec = (current_process->cstime % 100) * 10000;
    break;

  case RUSAGE_THREAD:
    // Same as self for now
    return getrusage(RUSAGE_SELF, usage);

  default:
    return -EINVAL;
  }

  return 0;
}

} // extern "C"
