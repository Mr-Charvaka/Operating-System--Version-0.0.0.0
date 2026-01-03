#ifndef PROCESS_H
#define PROCESS_H

#include "../include/isr.h"
#include "../include/signal.h"
#include "../include/types.h"
#include "../include/vfs.h"
#include "paging.h"

#define MAX_PROCESS_FILES 16
#define DEFAULT_TIME_SLICE 10 // 10 timer ticks (~100ms at 100Hz)
#define DEFAULT_PRIORITY 120  // Linux-like, 0-139 range

typedef enum {
  PROCESS_RUNNING,
  PROCESS_READY,
  PROCESS_ZOMBIE,
  PROCESS_WAITING,
  PROCESS_SLEEPING // Sleeping on timer
} process_state_t;

typedef struct process {
  uint32_t id;               // Process ID
  uint32_t pgid;             // Process Group ID
  uint32_t sid;              // Session ID
  process_state_t state;     // Current state
  uint32_t exit_code;        // Exit code (for ZOMBIE state)
  struct process *parent;    // Parent process
  uint32_t esp;              // Stack Pointer (Kernel Stack)
  uint32_t kernel_stack_top; // Top of kernel stack for TSS
  uint32_t *page_directory;  // Page Directory (Physical Address)
  uint32_t entry_point;      // User mode entry point
  uint32_t user_stack_top;   // Top of user stack
  uint32_t heap_end;         // Current program break (end of heap)
  vfs_node_t *fd_table[MAX_PROCESS_FILES]; // File Descriptor Table

  // User/Group IDs
  uint32_t uid;  // Real user ID
  uint32_t euid; // Effective user ID
  uint32_t suid; // Saved user ID
  uint32_t gid;  // Real group ID
  uint32_t egid; // Effective group ID
  uint32_t sgid; // Saved group ID

  // Priority scheduling
  int priority;         // 0-139 (lower = higher priority)
  int time_slice;       // Time quantum in ticks
  int time_remaining;   // Remaining ticks before reschedule
  uint32_t sleep_until; // Tick count to wake up (for sleep)

  // Alarm timer
  uint32_t alarm_time; // Tick when SIGALRM should be sent (0 = disabled)

  // Signal handling
  struct sigaction signal_actions[64];
  sigset_t pending_signals;
  sigset_t signal_mask;
  registers_t saved_context; // Context before signal
  int in_signal_handler;     // Recursion protection

  // Resource usage (for times() syscall)
  uint32_t utime;      // User time in ticks
  uint32_t stime;      // System time in ticks
  uint32_t cutime;     // Children user time
  uint32_t cstime;     // Children system time
  uint32_t start_time; // Process start time (tick)

  char cwd[256];    // Current Working Directory
  uint32_t pledges; // Pledges (Bitmask of allowed actions)

  struct unveil_node {
    char path[128];
    uint32_t permissions; // 4=r, 2=w, 1=x, 8=create
    struct unveil_node *next;
  } *unveils;

  struct process *next; // Next process in list
} process_t;

// Pledge definitions
#define PLEDGE_STDIO 0x01
#define PLEDGE_RPATH 0x02 // Read files
#define PLEDGE_WPATH 0x04 // Write files
#define PLEDGE_CPATH 0x08 // Create files
#define PLEDGE_EXEC 0x10
#define PLEDGE_PROC 0x20 // Process ops (kill, etc)
#define PLEDGE_INET 0x40 // Network
#define PLEDGE_ALL 0xFFFFFFFF

extern process_t *current_process;
extern process_t *ready_queue;

void init_multitasking();
void create_kernel_thread(void (*fn)());
void create_user_process(const char *filename);
void schedule();
int get_pid();
void enter_user_mode();
int fork_process(registers_t *regs);
void exit_process(int status);
int wait_process(int *status);
int sys_waitpid(int pid, int *status, int options);
int sys_waitid(int idtype, int id, void *infop, int options);
int exec_process(registers_t *regs, const char *path, char *const argv[],
                 char *const envp[]);

// Immediate exit (no cleanup)
void sys__exit(int status);

// Alarm timer
uint32_t sys_alarm(uint32_t seconds);

// Check and deliver alarms (called from timer)
void check_process_alarms(void);

// posix_spawn (simplified fork+exec)
int sys_posix_spawn(int *pid, const char *path, void *file_actions, void *attrp,
                    char *const argv[], char *const envp[]);

#endif
