#include "../include/signal.h"
#include "../drivers/serial.h"
#include "../include/isr.h"
#include "process.h"

int sys_signal(int signum, sighandler_t handler) {
  if (signum < 0 || signum >= 32 || signum == SIGKILL)
    return -1;
  current_process->signal_handlers[signum] = (uint32_t)(uintptr_t)handler;
  return 0;
}

int sys_kill(int pid, int signum) {
  if (signum < 0 || signum >= 32)
    return -1;

  // Find process
  process_t *p = ready_queue;
  if (!p)
    return -1;

  // We iterate the circular list
  process_t *start = p;
  do {
    if (p->id == (uint32_t)pid) {
      // Found it
      p->pending_signals |= (1 << signum);

      // Wake it up if waiting
      if (p->state == PROCESS_WAITING) {
        p->state = PROCESS_READY;
      }
      return 0;
    }
    p = p->next;
  } while (p && p != start);

  return -1;
}

int kernel_sigreturn(registers_t *regs) {
  if (!current_process->in_signal_handler)
    return -1;

  // Restore context
  // We overwrite the *TRAP* frame with the saved context
  // The syscall handler passes 'regs' which points to the stack frame
  // that will be restored upon IRET.
  *regs = current_process->saved_context;

  current_process->in_signal_handler = 0;
  return 0; // Won't actually return 0, context switched
}

void handle_signals(registers_t *regs) {
  // Only handle if returning to user mode
  if ((regs->cs & 0x3) != 3)
    return;

  // Only handle if not already handling a signal (no recursion for now)
  if (current_process->in_signal_handler)
    return;

  if (current_process->pending_signals == 0)
    return;

  for (int sig = 1; sig < 32; sig++) {
    if (current_process->pending_signals & (1 << sig)) {
      // Check handler
      uint32_t handler = current_process->signal_handlers[sig];

      // Clear pending bit
      current_process->pending_signals &= ~(1 << sig);

      if (handler == (uint32_t)(uintptr_t)SIG_IGN) {
        continue;
      } else if (handler == (uint32_t)(uintptr_t)SIG_DFL) {
        // Default action
        if (sig == SIGCHLD)
          continue; // Ignore CHLD default

        // For others, terminate
        serial_log("SIGNAL: Terminating process due to unchecked signal.");
        exit_process(128 + sig);
        return;
      } else {
        // Custom handler
        // 1. Save context
        current_process->saved_context = *regs;
        current_process->in_signal_handler = 1;

        // 2. Setup user stack
        // We need to push 'signum' and a return address.
        // Assuming stack grows down.
        uint32_t *stack = (uint32_t *)(uintptr_t)regs->useresp;

        // Push return address (Dummy for now, expects explicit sigreturn)
        stack--;
        *stack = 0xDEADC0DE;

        // Push argument (signum)
        stack--;
        *stack = sig;

        // Update implementation ESP
        regs->useresp = (uint32_t)(uintptr_t)stack;

        // 3. Jump to handler
        regs->eip = handler;

        // Handle only one signal per interruption
        return;
      }
    }
  }
}
