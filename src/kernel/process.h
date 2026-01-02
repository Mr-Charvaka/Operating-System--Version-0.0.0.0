#ifndef PROCESS_H
#define PROCESS_H

#include "../include/isr.h"
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

  // Priority scheduling
  int priority;         // 0-139 (lower = higher priority)
  int time_slice;       // Time quantum in ticks
  int time_remaining;   // Remaining ticks before reschedule
  uint32_t sleep_until; // Tick count to wake up (for sleep)

  // Signal handling
  uint32_t signal_handlers[32]; // 32 signals
  uint32_t pending_signals;     // Bitmask
  uint32_t blocked_signals;     // Bitmask
  registers_t saved_context;    // Context before signal
  int in_signal_handler;        // Recursion protection
  char cwd[256];                // Current Working Directory

  struct process *next; // Next process in list
} process_t;

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
int exec_process(registers_t *regs, const char *path, char *const argv[],
                 char *const envp[]);

#endif
