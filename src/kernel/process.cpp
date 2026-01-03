#include "process.h"
#include "../drivers/serial.h"
#include "../include/isr.h"
#include "../include/signal.h"
#include "../include/string.h"
#include "../kernel/memory.h"
#include "elf_loader.h"
#include "gdt.h"
#include "heap.h"
#include "paging.h"
#include "pmm.h"
#include "vm.h"

process_t *current_process = 0;
process_t *ready_queue = 0;
uint32_t next_pid = 1;

extern "C" uint32_t stack_top; // From kernel_entry.asm
extern "C" void switch_task(uint32_t *old_esp, uint32_t new_esp,
                            uint32_t new_cr3);
extern "C" void fork_child_return();

void init_multitasking() {
  serial_log("SCHED: Initializing Higher-Half Scheduler...");

  current_process = (process_t *)kmalloc(sizeof(process_t));
  current_process->id = 0;
  current_process->state = PROCESS_RUNNING;
  current_process->parent = 0;
  current_process->exit_code = 0;
  current_process->page_directory = (uint32_t *)VIRT_TO_PHYS(kernel_directory);
  current_process->kernel_stack_top = (uint32_t)&stack_top;

  for (int i = 0; i < MAX_PROCESS_FILES; i++)
    current_process->fd_table[i] = 0;

  current_process->priority = DEFAULT_PRIORITY;
  current_process->time_slice = DEFAULT_TIME_SLICE;
  current_process->time_remaining = DEFAULT_TIME_SLICE;
  current_process->sleep_until = 0;
  current_process->pledges = PLEDGE_ALL;
  strcpy(current_process->cwd, "/");

  current_process->next = current_process;
  ready_queue = current_process;

  serial_log("SCHED: Enabled.");
}

void create_kernel_thread(void (*fn)()) {
  process_t *new_proc = (process_t *)kmalloc(sizeof(process_t));
  new_proc->id = next_pid++;
  new_proc->state = PROCESS_READY;
  new_proc->parent = current_process;
  new_proc->exit_code = 0;
  new_proc->page_directory = (uint32_t *)VIRT_TO_PHYS(kernel_directory);
  new_proc->heap_end = 0;
  new_proc->pledges = PLEDGE_ALL;

  new_proc->priority = DEFAULT_PRIORITY;
  new_proc->time_slice = DEFAULT_TIME_SLICE;
  new_proc->time_remaining = DEFAULT_TIME_SLICE;
  new_proc->sleep_until = 0;

  uint32_t *stack = (uint32_t *)kmalloc(16384);
  uint32_t *top = stack + 4096;

  *(--top) = (uint32_t)fn;
  *(--top) = 0;
  *(--top) = 0;
  *(--top) = 0;
  *(--top) = 0;
  *(--top) = 0x0202;

  new_proc->esp = (uint32_t)top;
  new_proc->kernel_stack_top = (uint32_t)stack + 4096;

  new_proc->next = current_process->next;
  current_process->next = new_proc;
}

void user_mode_entry(uint32_t entry, uint32_t utop) {
  asm volatile("  \
        cli; \
        mov $0x23, %%ax; \
        mov %%ax, %%ds; \
        mov %%ax, %%es; \
        mov %%ax, %%fs; \
        mov %%ax, %%gs; \
        \
        pushl $0x23; \
        pushl %0; \
        pushf; \
        popl %%eax; \
        orl $0x200, %%eax; \
        pushl %%eax; \
        pushl $0x1B; \
        pushl %1; \
        iret; \
    " ::"r"(utop),
               "r"(entry)
               : "eax");
}

void create_user_process(const char *filename) {
  uint32_t phys_pd = (uint32_t)pd_create();
  if (!phys_pd)
    return;

  uint32_t phys_old_pd;
  asm volatile("mov %%cr3, %0" : "=r"(phys_old_pd));

  pd_switch((uint32_t *)phys_pd);

  uint32_t top_addr = 0;
  uint32_t entry = load_elf(filename, &top_addr);

  pd_switch((uint32_t *)phys_old_pd);

  if (entry == 0) {
    serial_log("PROC ERROR: Failed to load ELF");
    return;
  }

  serial_log_hex("PROC: Created user process from ", entry);

  process_t *new_proc = (process_t *)kmalloc(sizeof(process_t));
  new_proc->id = next_pid++;
  new_proc->state = PROCESS_READY;
  new_proc->parent = current_process;
  new_proc->exit_code = 0;
  new_proc->page_directory = (uint32_t *)phys_pd;
  new_proc->heap_end = top_addr;
  new_proc->pledges = PLEDGE_ALL;

  for (int i = 0; i < MAX_PROCESS_FILES; i++)
    new_proc->fd_table[i] = 0;

  vfs_node_t *tty = finddir_vfs(vfs_dev, "tty");
  if (tty) {
    new_proc->fd_table[0] = tty;
    new_proc->fd_table[1] = tty;
    new_proc->fd_table[2] = tty;
  }

  if (current_process)
    strcpy(new_proc->cwd, current_process->cwd);
  else
    strcpy(new_proc->cwd, "/");

  uint32_t *kstack = (uint32_t *)kmalloc(4096);
  uint32_t *ktop = kstack + 1024;

  uint32_t stack_phys = (uint32_t)pmm_alloc_block();
  uint32_t tsc_low;
  asm volatile("rdtsc" : "=a"(tsc_low) : : "edx");
  uint32_t aslr_offset = (tsc_low & 0xFF) * 0x1000;
  uint32_t user_stack_virt = 0xB0000000 - aslr_offset;

  new_proc->unveils = 0;

  pd_switch((uint32_t *)phys_pd);
  vm_map_page(stack_phys, user_stack_virt, 7);
  vm_map_page((uint32_t)pmm_alloc_block(), user_stack_virt - 0x1000, 7);
  vm_map_page((uint32_t)pmm_alloc_block(), user_stack_virt + 0x1000, 7);
  pd_switch((uint32_t *)phys_old_pd);

  new_proc->entry_point = entry;
  new_proc->user_stack_top = user_stack_virt + 4096;

  *(--ktop) = (uint32_t)new_proc->user_stack_top;
  *(--ktop) = (uint32_t)entry;
  *(--ktop) = 0;
  *(--ktop) = (uint32_t)user_mode_entry;

  *(--ktop) = 0;
  *(--ktop) = 0;
  *(--ktop) = 0;
  *(--ktop) = 0;
  *(--ktop) = 0x0202;

  new_proc->esp = (uint32_t)ktop;
  new_proc->kernel_stack_top = (uint32_t)kstack + 4096;

  new_proc->next = current_process->next;
  current_process->next = new_proc;

  serial_log("SCHED: Created User Process (Higher-Half Ready).");
}

void schedule() {
  if (!current_process)
    return;

  if (current_process->state == PROCESS_RUNNING) {
    current_process->time_remaining--;
    if (current_process->time_remaining > 0) {
      process_t *p = current_process->next;
      int found_higher = 0;
      while (p != current_process) {
        if (p->state == PROCESS_READY &&
            p->priority < current_process->priority) {
          found_higher = 1;
          break;
        }
        p = p->next;
      }
      if (!found_higher)
        return;
    }
  }

  process_t *old = current_process;
  process_t *best = 0;
  process_t *p = old->next;

  do {
    if (p->state == PROCESS_READY ||
        (p == old && old->state == PROCESS_RUNNING)) {
      if (!best || p->priority < best->priority)
        best = p;
    }
    p = p->next;
  } while (p != old->next);

  if (!best)
    return;
  if (best == old && old->state == PROCESS_RUNNING) {
    current_process->time_remaining = current_process->time_slice;
    return;
  }

  if (old->state == PROCESS_RUNNING)
    old->state = PROCESS_READY;

  current_process = best;
  current_process->state = PROCESS_RUNNING;
  current_process->time_remaining = current_process->time_slice;

  // Debug log
  if (current_process->id != old->id) {
    serial_log_hex("SCHED: Switching to PID ", current_process->id);
  }

  set_kernel_stack(current_process->kernel_stack_top);

  switch_task(&old->esp, current_process->esp,
              (uint32_t)current_process->page_directory);
}

void enter_user_mode() {
  asm volatile("  \
        cli; \
        mov $0x23, %ax; \
        mov %ax, %ds; \
        mov %ax, %es; \
        mov %ax, %fs; \
        mov %ax, %gs; \
        \
        mov %esp, %eax; \
        pushl $0x23; \
        pushl %eax; \
        pushf; \
        popl %eax; \
        orl $0x200, %eax; \
        pushl %eax; \
        pushl $0x1B; \
        pushl $.1; \
        iret; \
    .1: \
    ");
}

int get_pid() { return current_process ? current_process->id : -1; }

int fork_process(registers_t *parent_regs) {
  asm volatile("cli");

  uint32_t phys_new_pd = (uint32_t)pd_clone(current_process->page_directory);
  if (!phys_new_pd) {
    asm volatile("sti");
    return -1;
  }

  process_t *child = (process_t *)kmalloc(sizeof(process_t));
  child->id = next_pid++;
  child->state = PROCESS_READY;
  child->parent = current_process;
  child->exit_code = 0;
  child->page_directory = (uint32_t *)phys_new_pd;
  child->entry_point = current_process->entry_point;
  child->user_stack_top = current_process->user_stack_top;
  child->heap_end = current_process->heap_end;
  strcpy(child->cwd, current_process->cwd);
  child->pledges = current_process->pledges;

  for (int i = 0; i < MAX_PROCESS_FILES; i++) {
    child->fd_table[i] = current_process->fd_table[i];
    if (child->fd_table[i])
      child->fd_table[i]->ref_count++;
  }

  uint32_t *child_kstack = (uint32_t *)kmalloc(4096);
  child->kernel_stack_top = (uint32_t)child_kstack + 4096;

  uint32_t *stack_ptr = (uint32_t *)(child->kernel_stack_top);
  *(--stack_ptr) = parent_regs->ss;
  *(--stack_ptr) = parent_regs->useresp;
  *(--stack_ptr) = parent_regs->eflags | 0x200;
  *(--stack_ptr) = parent_regs->cs;
  *(--stack_ptr) = parent_regs->eip;

  *(--stack_ptr) = parent_regs->err_code;
  *(--stack_ptr) = parent_regs->int_no;

  *(--stack_ptr) = 0;
  *(--stack_ptr) = parent_regs->ecx;
  *(--stack_ptr) = parent_regs->edx;
  *(--stack_ptr) = parent_regs->ebx;
  *(--stack_ptr) = parent_regs->esp;
  *(--stack_ptr) = parent_regs->ebp;
  *(--stack_ptr) = parent_regs->esi;
  *(--stack_ptr) = parent_regs->edi;
  *(--stack_ptr) = parent_regs->ds;

  *(--stack_ptr) = (uint32_t)fork_child_return;
  *(--stack_ptr) = 0;
  *(--stack_ptr) = 0;
  *(--stack_ptr) = 0;
  *(--stack_ptr) = 0;
  *(--stack_ptr) = 0x0202;

  serial_log_hex("PROC: Forked child PID ", child->id);
  child->esp = (uint32_t)stack_ptr;
  child->next = current_process->next;
  current_process->next = child;

  asm volatile("sti");
  return child->id;
}

void exit_process(int status) {
  asm volatile("cli");
  current_process->state = PROCESS_ZOMBIE;
  current_process->exit_code = (uint32_t)status;
  for (int i = 0; i < MAX_PROCESS_FILES; i++) {
    if (current_process->fd_table[i]) {
      close_vfs(current_process->fd_table[i]);
      current_process->fd_table[i] = 0;
    }
  }
  if (current_process->parent)
    sys_kill(current_process->parent->id, SIGCHLD);
  schedule();
}

int wait_process(int *status) {
  while (true) {
    asm volatile("cli");
    process_t *child = 0;
    process_t *p = ready_queue;
    do {
      if (p->parent == current_process && p->state == PROCESS_ZOMBIE) {
        child = p;
        break;
      }
      p = p->next;
    } while (p != ready_queue);

    if (child) {
      uint32_t pid = child->id;
      if (status)
        *status = child->exit_code;
      if (child->next == child)
        ready_queue = 0;
      else {
        process_t *curr = ready_queue;
        while (curr->next != child)
          curr = curr->next;
        curr->next = child->next;
        if (ready_queue == child)
          ready_queue = child->next;
      }
      kfree((void *)(child->kernel_stack_top - 4096));
      pd_destroy(child->page_directory);
      kfree(child);
      asm volatile("sti");
      return (int)pid;
    }

    bool has_children = false;
    p = ready_queue;
    do {
      if (p->parent == current_process) {
        has_children = true;
        break;
      }
      p = p->next;
    } while (p != ready_queue);

    if (!has_children) {
      asm volatile("sti");
      return -1;
    }
    current_process->state = PROCESS_WAITING;
    asm volatile("sti");
    schedule();
  }
}

int exec_process(registers_t *regs, const char *path, char *const argv[],
                 char *const envp[]) {
  serial_log("EXEC: Attempting to execute...");
  serial_log(path);

  // Copy path to kernel space before clearing user mappings!
  char kernel_path[256];
  if (path) {
    strncpy(kernel_path, path, 255);
    kernel_path[255] = 0;
  } else {
    return -1;
  }

  vm_clear_user_mappings();
  uint32_t top_addr = 0;
  uint32_t entry = load_elf(kernel_path, &top_addr);
  if (entry == 0) {
    serial_log("EXEC: Failed to load ELF.");
    return -1;
  }

  serial_log_hex("EXEC: Entry point loaded at ", entry);

  uint32_t user_stack_virt = 0xB0000000;
  vm_map_page((uint32_t)pmm_alloc_block(), user_stack_virt, 7);
  vm_map_page((uint32_t)pmm_alloc_block(), user_stack_virt - 0x1000, 7);
  vm_map_page((uint32_t)pmm_alloc_block(), user_stack_virt + 0x1000, 7);

  current_process->entry_point = entry;
  current_process->user_stack_top = user_stack_virt + 4096;
  current_process->heap_end = top_addr;
  current_process->pledges = PLEDGE_ALL; // Reset pledges for new exec
  regs->eip = entry;
  regs->useresp = current_process->user_stack_top;

  serial_log("EXEC: Success. Jump to user mode.");
  return 0;
}

// ============================================================================
// sys_waitpid - Wait for specific process
// ============================================================================
// Options: WNOHANG (1), WUNTRACED (2), WCONTINUED (8)
#define WNOHANG 0x00000001
#define WUNTRACED 0x00000002
#define WCONTINUED 0x00000008

int sys_waitpid(int pid, int *status, int options) {
  bool nohang = (options & WNOHANG) != 0;

  while (true) {
    asm volatile("cli");
    process_t *found = 0;
    process_t *p = ready_queue;

    if (!p) {
      asm volatile("sti");
      return -1; // No processes
    }

    process_t *start = p;
    do {
      bool matches = false;

      if (pid == -1) {
        // Any child
        matches = (p->parent == current_process);
      } else if (pid == 0) {
        // Any child in same process group
        matches =
            (p->parent == current_process && p->pgid == current_process->pgid);
      } else if (pid < -1) {
        // Any child in process group |pid|
        matches = (p->parent == current_process && p->pgid == (uint32_t)(-pid));
      } else {
        // Specific child
        matches = (p->parent == current_process && p->id == (uint32_t)pid);
      }

      if (matches && p->state == PROCESS_ZOMBIE) {
        found = p;
        break;
      }

      p = p->next;
    } while (p != start);

    if (found) {
      uint32_t child_pid = found->id;

      // Return status: exit code in upper 8 bits
      if (status)
        *status = (found->exit_code << 8);

      // Accumulate child times
      current_process->cutime += found->utime + found->cutime;
      current_process->cstime += found->stime + found->cstime;

      // Remove from process list
      if (found->next == found) {
        ready_queue = 0;
      } else {
        process_t *curr = ready_queue;
        while (curr->next != found)
          curr = curr->next;
        curr->next = found->next;
        if (ready_queue == found)
          ready_queue = found->next;
      }

      // Free resources
      kfree((void *)(found->kernel_stack_top - 4096));
      pd_destroy(found->page_directory);
      kfree(found);

      asm volatile("sti");
      return (int)child_pid;
    }

    // Check if we have any matching children
    bool has_children = false;
    p = ready_queue;
    do {
      bool matches = false;
      if (pid == -1) {
        matches = (p->parent == current_process);
      } else if (pid == 0) {
        matches =
            (p->parent == current_process && p->pgid == current_process->pgid);
      } else if (pid < -1) {
        matches = (p->parent == current_process && p->pgid == (uint32_t)(-pid));
      } else {
        matches = (p->parent == current_process && p->id == (uint32_t)pid);
      }
      if (matches) {
        has_children = true;
        break;
      }
      p = p->next;
    } while (p != start);

    if (!has_children) {
      asm volatile("sti");
      return -10; // ECHILD
    }

    if (nohang) {
      asm volatile("sti");
      return 0; // No child exited yet
    }

    current_process->state = PROCESS_WAITING;
    asm volatile("sti");
    schedule();
  }
}

// ============================================================================
// sys_waitid - Wait for child process state change (POSIX)
// ============================================================================
// idtype: P_ALL(0), P_PID(1), P_PGID(2)
// options: WEXITED(4), WSTOPPED(2), WCONTINUED(8), WNOHANG(1),
// WNOWAIT(0x1000000)
#define P_ALL 0
#define P_PID 1
#define P_PGID 2
#define WEXITED 0x00000004

int sys_waitid(int idtype, int id, void *infop, int options) {
  // Simplified implementation - convert to waitpid semantics
  int pid;
  switch (idtype) {
  case P_ALL:
    pid = -1;
    break;
  case P_PID:
    pid = id;
    break;
  case P_PGID:
    pid = -id;
    break;
  default:
    return -22; // EINVAL
  }

  int status;
  int wpid_opts = 0;
  if (options & WNOHANG)
    wpid_opts |= WNOHANG;

  int result = sys_waitpid(pid, &status, wpid_opts);

  // Fill siginfo if provided (simplified)
  if (result > 0 && infop) {
    // Cast to uint32_t* for simple struct access
    uint32_t *info = (uint32_t *)infop;
    info[0] = SIGCHLD;              // si_signo
    info[1] = 0;                    // si_errno
    info[2] = 1;                    // si_code = CLD_EXITED
    info[3] = result;               // si_pid
    info[4] = 0;                    // si_uid
    info[5] = (status >> 8) & 0xFF; // si_status
  }

  return result;
}

// ============================================================================
// sys__exit - Immediate process termination (no cleanup)
// ============================================================================

void sys__exit(int status) {
  asm volatile("cli");

  current_process->state = PROCESS_ZOMBIE;
  current_process->exit_code = (uint32_t)status;

  // Send SIGCHLD to parent
  if (current_process->parent)
    sys_kill(current_process->parent->id, SIGCHLD);

  // No file descriptor cleanup - that's the difference from exit()
  schedule();

  // Should never reach here
  while (1) {
  }
}

// ============================================================================
// sys_alarm - Set an alarm clock
// Returns: Previous alarm remaining seconds (0 if none)
// ============================================================================

extern uint32_t tick;

uint32_t sys_alarm(uint32_t seconds) {
  if (!current_process)
    return 0;

  uint32_t old_alarm = current_process->alarm_time;
  uint32_t old_remaining = 0;

  // Calculate remaining time from old alarm
  if (old_alarm > 0 && old_alarm > tick) {
    old_remaining = (old_alarm - tick) / 100; // Assuming 100Hz timer
  }

  // Set new alarm
  if (seconds == 0) {
    current_process->alarm_time = 0; // Cancel alarm
  } else {
    current_process->alarm_time = tick + (seconds * 100); // 100Hz timer
  }

  return old_remaining;
}

// ============================================================================
// check_process_alarms - Called from timer interrupt
// ============================================================================

void check_process_alarms(void) {
  if (!ready_queue)
    return;

  process_t *p = ready_queue;
  process_t *start = p;
  do {
    if (p->alarm_time > 0 && tick >= p->alarm_time) {
      // Alarm expired - send SIGALRM
      p->pending_signals |= ((sigset_t)1 << SIGALRM);
      p->alarm_time = 0;

      // Wake up if sleeping
      if (p->state == PROCESS_WAITING || p->state == PROCESS_SLEEPING)
        p->state = PROCESS_READY;
    }
    p = p->next;
  } while (p && p != start);
}

// ============================================================================
// sys_posix_spawn - Spawn new process (simplified fork+exec)
// ============================================================================

int sys_posix_spawn(int *pid_out, const char *path, void *file_actions,
                    void *attrp, char *const argv[], char *const envp[]) {
  (void)file_actions; // Not implemented
  (void)attrp;        // Not implemented
  (void)argv;         // Not fully implemented
  (void)envp;         // Not fully implemented

  if (!path)
    return -22; // EINVAL

  // Create new process directly (more efficient than fork+exec)
  uint32_t phys_pd = (uint32_t)pd_create();
  if (!phys_pd)
    return -12; // ENOMEM

  uint32_t phys_old_pd;
  asm volatile("mov %%cr3, %0" : "=r"(phys_old_pd));

  pd_switch((uint32_t *)phys_pd);

  uint32_t top_addr = 0;
  uint32_t entry = load_elf(path, &top_addr);

  pd_switch((uint32_t *)phys_old_pd);

  if (entry == 0) {
    pd_destroy((uint32_t *)phys_pd);
    return -2; // ENOENT
  }

  process_t *new_proc = (process_t *)kmalloc(sizeof(process_t));
  if (!new_proc) {
    pd_destroy((uint32_t *)phys_pd);
    return -12; // ENOMEM
  }

  // Initialize new process
  new_proc->id = next_pid++;
  new_proc->state = PROCESS_READY;
  new_proc->parent = current_process;
  new_proc->exit_code = 0;
  new_proc->page_directory = (uint32_t *)phys_pd;
  new_proc->heap_end = top_addr;
  new_proc->pledges = PLEDGE_ALL;
  new_proc->pgid = current_process->pgid;
  new_proc->sid = current_process->sid;

  // Inherit uid/gid
  new_proc->uid = current_process->uid;
  new_proc->euid = current_process->euid;
  new_proc->suid = current_process->suid;
  new_proc->gid = current_process->gid;
  new_proc->egid = current_process->egid;
  new_proc->sgid = current_process->sgid;

  // Initialize file descriptors (inherit from parent)
  for (int i = 0; i < MAX_PROCESS_FILES; i++) {
    new_proc->fd_table[i] = current_process->fd_table[i];
    if (new_proc->fd_table[i])
      new_proc->fd_table[i]->ref_count++;
  }

  strcpy(new_proc->cwd, current_process->cwd);

  new_proc->priority = DEFAULT_PRIORITY;
  new_proc->time_slice = DEFAULT_TIME_SLICE;
  new_proc->time_remaining = DEFAULT_TIME_SLICE;
  new_proc->sleep_until = 0;
  new_proc->alarm_time = 0;

  // Initialize signal state
  new_proc->pending_signals = 0;
  new_proc->signal_mask = 0;
  new_proc->in_signal_handler = 0;

  // Resource usage
  new_proc->utime = 0;
  new_proc->stime = 0;
  new_proc->cutime = 0;
  new_proc->cstime = 0;
  new_proc->start_time = tick;
  new_proc->unveils = 0;

  // Allocate kernel stack
  uint32_t *kstack = (uint32_t *)kmalloc(4096);
  new_proc->kernel_stack_top = (uint32_t)kstack + 4096;

  // Allocate user stack
  pd_switch((uint32_t *)phys_pd);
  uint32_t user_stack_virt = 0xB0000000;
  vm_map_page((uint32_t)pmm_alloc_block(), user_stack_virt, 7);
  vm_map_page((uint32_t)pmm_alloc_block(), user_stack_virt - 0x1000, 7);
  vm_map_page((uint32_t)pmm_alloc_block(), user_stack_virt + 0x1000, 7);
  pd_switch((uint32_t *)phys_old_pd);

  new_proc->entry_point = entry;
  new_proc->user_stack_top = user_stack_virt + 4096;

  // Setup kernel stack for first context switch
  uint32_t *ktop = (uint32_t *)new_proc->kernel_stack_top;
  *(--ktop) = new_proc->user_stack_top;
  *(--ktop) = entry;
  *(--ktop) = 0;
  extern void user_mode_entry(uint32_t entry, uint32_t utop);
  *(--ktop) = (uint32_t)user_mode_entry;
  *(--ktop) = 0;      // ebp
  *(--ktop) = 0;      // ebx
  *(--ktop) = 0;      // esi
  *(--ktop) = 0;      // edi
  *(--ktop) = 0x0202; // flags

  new_proc->esp = (uint32_t)ktop;

  // Add to process list
  new_proc->next = current_process->next;
  current_process->next = new_proc;

  if (pid_out)
    *pid_out = new_proc->id;

  serial_log_hex("POSIX_SPAWN: Created process ", new_proc->id);
  return 0;
}
