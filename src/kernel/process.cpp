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

process_t *current_process = 0;
process_t *ready_queue = 0;
uint32_t next_pid = 1;

extern "C" void switch_task(uint32_t *old_esp, uint32_t new_esp,
                            uint32_t new_cr3);
extern "C" void fork_child_return();

void init_multitasking() {
  // CLI should be called before this
  serial_log("SCHED: Initializing...");

  // Create 'process zero' for the running kernel
  current_process = (process_t *)kmalloc(sizeof(process_t));
  current_process->id = 0;
  current_process->state = PROCESS_RUNNING;
  current_process->parent = 0;
  current_process->exit_code = 0;
  current_process->page_directory = kernel_directory;
  current_process->kernel_stack_top = 0x9000;

  // Clear FD table
  for (int i = 0; i < MAX_PROCESS_FILES; i++) {
    current_process->fd_table[i] = 0;
  }

  // Initialize priority scheduling fields
  current_process->priority = DEFAULT_PRIORITY;
  current_process->time_slice = DEFAULT_TIME_SLICE;
  current_process->time_remaining = DEFAULT_TIME_SLICE;
  current_process->sleep_until = 0;
  strcpy(current_process->cwd, "/");

  // Make circular list
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
  new_proc->page_directory = kernel_directory;
  new_proc->heap_end = 0;

  // Initialize priority scheduling
  new_proc->priority = DEFAULT_PRIORITY;
  new_proc->time_slice = DEFAULT_TIME_SLICE;
  new_proc->time_remaining = DEFAULT_TIME_SLICE;
  new_proc->sleep_until = 0;

  // Allocate Kernel Stack
  uint32_t *stack = (uint32_t *)kmalloc(4096);
  uint32_t *top = stack + 1024;

  // Setup Stack Frame
  *(--top) = (uint32_t)fn;
  *(--top) = 0;      // EBX
  *(--top) = 0;      // ESI
  *(--top) = 0;      // EDI
  *(--top) = 0;      // EBP
  *(--top) = 0x0202; // EFLAGS

  new_proc->esp = (uint32_t)top;
  new_proc->kernel_stack_top = (uint32_t)stack + 4096;

  // Add to list
  new_proc->next = current_process->next;
  current_process->next = new_proc;
}

void user_mode_entry(uint32_t entry, uint32_t utop) {
  serial_log_hex("SCHED: Entering User Mode PID ", current_process->id);
  serial_log_hex("  Entry: ", entry);
  serial_log_hex("  Utop:  ", utop);

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

#include "vm.h" // Added

void create_user_process(const char *filename) {
  // 1. Create New Page Directory
  uint32_t *new_pd = pd_create();
  if (!new_pd) {
    serial_log("SCHED ERROR: Failed to create Page Directory.");
    return;
  }

  // 2. Switch to New PD temporarily to load ELF
  // We need to save the current CR3 to switch back
  uint32_t *kernel_pd =
      current_process ? current_process->page_directory : kernel_directory;
  pd_switch(new_pd);

  // 3. Load ELF (Now writes to new_pd's virtual memory)
  // This will dynamically allocate pages at 0x40000000+, separate from Kernel
  // Heap
  uint32_t top_addr = 0;
  uint32_t entry = load_elf(filename, &top_addr);

  // 4. Switch Back
  pd_switch(kernel_pd);

  if (entry == 0) {
    // Failed. TODO: destroy pd
    return;
  }

  process_t *new_proc = (process_t *)kmalloc(sizeof(process_t));
  new_proc->id = next_pid++;
  new_proc->state = PROCESS_READY;
  new_proc->parent = current_process;
  new_proc->exit_code = 0;
  new_proc->page_directory = new_pd; // Assign the new isolated directory
  new_proc->heap_end = top_addr;     // Initialize Heap End!

  // Clear FD table
  for (int i = 0; i < MAX_PROCESS_FILES; i++) {
    new_proc->fd_table[i] = 0;
  }

  // Pre-open /dev/tty for FD 0, 1, 2
  vfs_node_t *tty = finddir_vfs(vfs_dev, "tty");
  if (tty) {
    new_proc->fd_table[0] = tty;
    new_proc->fd_table[1] = tty;
    new_proc->fd_table[2] = tty;
  }

  if (current_process) {
    strcpy(new_proc->cwd, current_process->cwd);
  } else {
    strcpy(new_proc->cwd, "/");
  }

  // 5. Allocate Kernel Stack (This lives in Kernel Heap, which is SHARED
  // mapped)
  uint32_t *kstack = (uint32_t *)kmalloc(4096);
  uint32_t *ktop = kstack + 1024;

  // 6. Allocate User Stack
  // We want a fixed virtual address for User Stack (e.g., 0xB0000000)
  // But for now, to keep it simple, we can use kmalloc_a IF we map it?
  // Problem: kmalloc_a returns Kernel Virtual Address.
  // If we want isolation, we should allocate a page and map it to 0xB0000000 in
  // new_pd.

  // Let's do it properly:
  uint32_t stack_phys = (uint32_t)pmm_alloc_block();
  uint32_t user_stack_virt = 0xB0000000;

  // Map it in the new PD
  pd_switch(new_pd);
  vm_map_page(stack_phys, user_stack_virt, 7); // User|RW|Present
  // Map additional pages for stack safety
  vm_map_page((uint32_t)pmm_alloc_block(), user_stack_virt - 0x1000,
              7); // Below
  vm_map_page((uint32_t)pmm_alloc_block(), user_stack_virt + 0x1000,
              7); // Above (for any accesses at stack top)
  pd_switch(kernel_pd);

  new_proc->entry_point = entry;
  new_proc->user_stack_top =
      user_stack_virt + 4096; // Top of the allocated page

  // 7. Setup Kernel Stack for switch_task
  // We want switch_task to return to user_mode_entry
  // Since user_mode_entry takes arguments, we push them as well.

  *(--ktop) = (uint32_t)(uintptr_t)new_proc->user_stack_top; // Arg 2 (utop)
  *(--ktop) = (uint32_t)(uintptr_t)entry;                    // Arg 1 (entry)
  *(--ktop) = 0; // Dummy return address for user_mode_entry
  *(--ktop) = (uint32_t)(uintptr_t)user_mode_entry; // EIP for switch_task

  *(--ktop) = 0;      // EBX
  *(--ktop) = 0;      // ESI
  *(--ktop) = 0;      // EDI
  *(--ktop) = 0;      // EBP
  *(--ktop) = 0x0202; // EFLAGS

  new_proc->esp = (uint32_t)(uintptr_t)ktop;
  new_proc->kernel_stack_top = (uint32_t)(uintptr_t)kstack + 4096;

  // Add to list
  new_proc->next = current_process->next;
  current_process->next = new_proc;

  serial_log("SCHED: Created User Process (Isolated).");
}

void schedule() {
  if (!current_process)
    return;

  // Decrement time remaining for current process
  if (current_process->state == PROCESS_RUNNING) {
    current_process->time_remaining--;

    // If time slice not expired and no other higher priority, continue
    if (current_process->time_remaining > 0) {
      // Check if there's a higher priority ready process
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
        return; // Continue current process
    }
  }

  // Find highest priority ready process
  process_t *old = current_process;
  process_t *best = 0;
  process_t *p = old->next;

  // First pass: find highest priority ready process
  do {
    if (p->state == PROCESS_READY ||
        (p == old && old->state == PROCESS_RUNNING)) {
      if (!best || p->priority < best->priority) {
        best = p;
      }
    }
    p = p->next;
  } while (p != old->next);

  if (!best) {
    // No ready process found - stay on current if possible
    if (old->state == PROCESS_RUNNING)
      return;
    serial_log("SCHED ERROR: No ready processes!");
    return;
  }

  if (best == old && old->state == PROCESS_RUNNING) {
    // Continue current, reset time slice
    current_process->time_remaining = current_process->time_slice;
    return;
  }

  // Context switch
  if (old->state == PROCESS_RUNNING) {
    old->state = PROCESS_READY;
  }

  current_process = best;
  current_process->state = PROCESS_RUNNING;
  current_process->time_remaining = current_process->time_slice;

  // Set TSS kernel stack
  set_kernel_stack(current_process->kernel_stack_top);

  switch_task(&old->esp, current_process->esp,
              (uint32_t)(uintptr_t)current_process->page_directory);
}

void enter_user_mode() {
  // This is the "jump" to Ring 3 using IRET
  // 0x23 is User Data, 0x1B is User Code
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

int get_pid() {
  if (current_process)
    return current_process->id;
  return -1;
}

int fork_process(registers_t *parent_regs) {
  asm volatile("cli");

  serial_log("FORK: Starting fork...");

  // 1. Clone Page Directory (deep copy of user pages)
  uint32_t *new_pd = pd_clone(current_process->page_directory);
  if (!new_pd) {
    serial_log("FORK: Failed to clone page directory");
    asm volatile("sti");
    return -1;
  }

  // 2. Create new process structure
  process_t *child = (process_t *)kmalloc(sizeof(process_t));
  if (!child) {
    serial_log("FORK: Failed to allocate process struct");
    pd_destroy(new_pd);
    asm volatile("sti");
    return -1;
  }

  child->id = next_pid++;
  child->state = PROCESS_READY;
  child->parent = current_process;
  child->exit_code = 0;
  child->page_directory = new_pd;
  child->entry_point = current_process->entry_point;
  child->user_stack_top = current_process->user_stack_top;
  child->heap_end = current_process->heap_end;
  strcpy(child->cwd, current_process->cwd);

  // Clone FD table (shallow copy - both reference same vnodes)
  // Increment reference counts!
  for (int i = 0; i < MAX_PROCESS_FILES; i++) {
    child->fd_table[i] = current_process->fd_table[i];
    if (child->fd_table[i]) {
      child->fd_table[i]->ref_count++;
    }
  }

  // 3. Allocate NEW kernel stack for child
  uint32_t *child_kstack = (uint32_t *)kmalloc(4096);
  if (!child_kstack) {
    serial_log("FORK: Failed to allocate kernel stack");
    pd_destroy(new_pd);
    kfree(child);
    asm volatile("sti");
    return -1;
  }
  child->kernel_stack_top = (uint32_t)(uintptr_t)child_kstack + 4096;

  // 4. Set up child's kernel stack for first scheduling
  // We need to set up a stack that:
  //   a) Has a registers_t frame at the top (copy of parent's, with EAX=0)
  //   b) Below that, has the switch_task frame pointing to fork_child_return

  // fork_child_return is declared at the top of the file

  uint32_t *stack_ptr = (uint32_t *)(child->kernel_stack_top);

  // Copy the parent's interrupt frame to child's stack
  // The interrupt frame consists of:
  //   - iret frame: SS, ESP, EFLAGS, CS, EIP (pushed by CPU on interrupt from
  //   ring 3)
  //   - error_code, int_no (pushed by ISR stub)
  //   - pusha: EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI
  //   - DS (pushed as uint32_t)

  // Push iret frame (what iret will pop to return to user mode)
  *(--stack_ptr) = parent_regs->ss;             // SS
  *(--stack_ptr) = parent_regs->useresp;        // User ESP
  *(--stack_ptr) = parent_regs->eflags | 0x200; // EFLAGS with IF set
  *(--stack_ptr) = parent_regs->cs;             // CS (user code segment)
  *(--stack_ptr) = parent_regs->eip; // EIP (instruction after syscall)

  // Push error code and interrupt number (will be skipped by add esp, 8)
  *(--stack_ptr) = parent_regs->err_code;
  *(--stack_ptr) = parent_regs->int_no;

  // Push general registers (popa will restore these)
  *(--stack_ptr) = 0; // EAX = 0 (fork returns 0 to child!)
  *(--stack_ptr) = parent_regs->ecx;
  *(--stack_ptr) = parent_regs->edx;
  *(--stack_ptr) = parent_regs->ebx;
  *(--stack_ptr) = parent_regs->esp; // Saved ESP (ignored by popa)
  *(--stack_ptr) = parent_regs->ebp;
  *(--stack_ptr) = parent_regs->esi;
  *(--stack_ptr) = parent_regs->edi;

  // Push DS (fork_child_return will pop this first)
  *(--stack_ptr) = parent_regs->ds;

  // Now push the switch_task frame
  // switch_task expects: EFLAGS, EBP, EDI, ESI, EBX, then RET addr at the end
  *(--stack_ptr) =
      (uint32_t)(uintptr_t)fork_child_return; // Return address for switch_task
  *(--stack_ptr) = 0;                         // EBX
  *(--stack_ptr) = 0;                         // ESI
  *(--stack_ptr) = 0;                         // EDI
  *(--stack_ptr) = 0;                         // EBP
  *(--stack_ptr) = 0x0202;                    // EFLAGS

  child->esp = (uint32_t)(uintptr_t)stack_ptr;

  // 5. Add child to scheduler queue
  child->next = current_process->next;
  current_process->next = child;

  asm volatile("sti");

  // Parent returns child's PID
  return child->id;
}

void exit_process(int status) {
  asm volatile("cli");
  serial_log_hex("EXIT: Process ", current_process->id);
  serial_log_hex("  Status: ", status);

  current_process->state = PROCESS_ZOMBIE;
  current_process->exit_code = (uint32_t)status;

  // Cleanup File Descriptors
  for (int i = 0; i < MAX_PROCESS_FILES; i++) {
    if (current_process->fd_table[i]) {
      close_vfs(current_process->fd_table[i]);
      current_process->fd_table[i] = 0;
    }
  }

  // Send SIGCHLD to parent
  if (current_process->parent) {
    sys_kill(current_process->parent->id, SIGCHLD);
  }

  // We should also handle "orphans" - give them to init (PID 0 or 1)
  // For now, we skip this to keep it simple.

  // Switch to next task - this will never return
  schedule();
}

int wait_process(int *status) {
  serial_log_hex("WAIT: Process ", current_process->id);

  while (true) {
    asm volatile("cli");
    process_t *child = 0;
    process_t *prev = 0;

    // Scan all processes for a child that is a zombie
    // Note: this is O(n), but fine for now.
    process_t *p = ready_queue;
    do {
      if (p->parent == current_process && p->state == PROCESS_ZOMBIE) {
        child = p;
        break;
      }
      prev = p;
      p = p->next;
    } while (p != ready_queue);

    if (child) {
      uint32_t pid = child->id;
      if (status) {
        *status = child->exit_code;
      }

      // Cleanup child
      // Remove from circular ready_queue list safely
      if (child->next == child) {
        ready_queue = 0;
      } else {
        process_t *curr = ready_queue;
        while (curr->next != child) {
          curr = curr->next;
        }
        curr->next = child->next;
        if (ready_queue == child) {
          ready_queue = child->next;
        }
      }

      // Free child's resources
      kfree((void *)(child->kernel_stack_top - 4096));
      pd_destroy(child->page_directory);
      kfree(child);

      asm volatile("sti");
      return (int)pid;
    }

    // No zombie children found. Check if we even have children.
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

    // Have children but none finished. Block.
    current_process->state = PROCESS_WAITING;
    asm volatile("sti");
    schedule();
    // After waking up, loop and check again
  }
}

int exec_process(registers_t *regs, const char *path, char *const argv[],
                 char *const envp[]) {
  serial_log("EXEC: Loading program...");
  serial_log(path);

  // 1. Clear current user mappings
  vm_clear_user_mappings();

  // 2. Load new ELF binary
  uint32_t top_addr = 0;
  uint32_t entry = load_elf(path, &top_addr);

  if (entry == 0) {
    serial_log("EXEC ERROR: Failed to load ELF");
    return -1;
  }

  // 3. Setup User Stack
  uint32_t user_stack_virt = 0xB0000000;
  vm_map_page((uint32_t)pmm_alloc_block(), user_stack_virt,
              7); // User|RW|Present
  vm_map_page((uint32_t)pmm_alloc_block(), user_stack_virt - 0x1000, 7);
  vm_map_page((uint32_t)pmm_alloc_block(), user_stack_virt + 0x1000, 7);

  uint32_t stack_ptr = user_stack_virt + 4096;

  // 4. Copy argv/envp to stack (Simplified for now - just push pointers if they
  // exist)
  // TODO: Implement proper copying of strings to userspace stack.
  // For now, we assume simple binaries that don't need complex argv.

  // 5. Update process info
  current_process->entry_point = entry;
  current_process->user_stack_top = user_stack_virt + 4096;
  current_process->heap_end = top_addr;

  // 6. Update registers for iret
  regs->eip = entry;
  regs->useresp = stack_ptr;

  serial_log("EXEC: Success. Returning to new entry.");
  return 0;
}
