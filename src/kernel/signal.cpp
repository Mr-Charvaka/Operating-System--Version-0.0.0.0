// ============================================================================
// signal.cpp - POSIX Signal Implementation
// Entirely hand-crafted for Retro-OS kernel
// ============================================================================

#include "../include/signal.h"
#include "../drivers/serial.h"
#include "../include/errno.h"
#include "heap.h"
#include "process.h"


extern "C" {

// Global tick counter (from timer.cpp)
extern uint32_t tick;

// ============================================================================
// Signal Set Manipulation Functions
// ============================================================================

int sigemptyset(sigset_t *set) {
  if (!set)
    return -1;
  *set = 0;
  return 0;
}

int sigfillset(sigset_t *set) {
  if (!set)
    return -1;
  *set = ~((sigset_t)0);
  return 0;
}

int sigaddset(sigset_t *set, int signum) {
  if (!set || signum < 1 || signum >= NSIG)
    return -1;
  *set |= ((sigset_t)1 << signum);
  return 0;
}

int sigdelset(sigset_t *set, int signum) {
  if (!set || signum < 1 || signum >= NSIG)
    return -1;
  *set &= ~((sigset_t)1 << signum);
  return 0;
}

int sigismember(const sigset_t *set, int signum) {
  if (!set || signum < 1 || signum >= NSIG)
    return -1;
  return (*set & ((sigset_t)1 << signum)) ? 1 : 0;
}

// ============================================================================
// sys_signal - Simple signal handler registration (legacy)
// ============================================================================

int sys_signal(int signum, sighandler_t handler) {
  if (signum < 1 || signum >= NSIG)
    return -EINVAL;

  // SIGKILL and SIGSTOP cannot be caught or ignored
  if (signum == SIGKILL || signum == SIGSTOP)
    return -EINVAL;

  if (!current_process)
    return -ESRCH;

  // Store handler in process signal_actions array
  struct sigaction *sa = &current_process->signal_actions[signum];
  sa->sa_handler = handler;
  sa->sa_mask = 0;
  sa->sa_flags = 0;
  sa->sa_restorer = 0;

  return 0;
}

// ============================================================================
// sys_sigaction - Detailed signal management
// ============================================================================

int sys_sigaction(int signum, const struct sigaction *act,
                  struct sigaction *oldact) {
  if (signum < 1 || signum >= NSIG)
    return -EINVAL;

  // SIGKILL and SIGSTOP cannot have handlers
  if (signum == SIGKILL || signum == SIGSTOP)
    return -EINVAL;

  if (!current_process)
    return -ESRCH;

  struct sigaction *current_sa = &current_process->signal_actions[signum];

  // Return old action if requested
  if (oldact) {
    oldact->sa_handler = current_sa->sa_handler;
    oldact->sa_mask = current_sa->sa_mask;
    oldact->sa_flags = current_sa->sa_flags;
    oldact->sa_restorer = current_sa->sa_restorer;
  }

  // Set new action if provided
  if (act) {
    current_sa->sa_handler = act->sa_handler;
    current_sa->sa_mask = act->sa_mask;
    current_sa->sa_flags = act->sa_flags;
    current_sa->sa_restorer = act->sa_restorer;
  }

  return 0;
}

// ============================================================================
// sys_sigprocmask - Examine and change blocked signals
// ============================================================================

int sys_sigprocmask(int how, const sigset_t *set, sigset_t *oldset) {
  if (!current_process)
    return -ESRCH;

  // Return old mask if requested
  if (oldset)
    *oldset = current_process->signal_mask;

  // Modify mask if set provided
  if (set) {
    switch (how) {
    case SIG_BLOCK:
      current_process->signal_mask |= *set;
      break;
    case SIG_UNBLOCK:
      current_process->signal_mask &= ~(*set);
      break;
    case SIG_SETMASK:
      current_process->signal_mask = *set;
      break;
    default:
      return -EINVAL;
    }
    // SIGKILL and SIGSTOP cannot be blocked
    current_process->signal_mask &= ~((sigset_t)1 << SIGKILL);
    current_process->signal_mask &= ~((sigset_t)1 << SIGSTOP);
  }

  return 0;
}

// ============================================================================
// sys_sigpending - Examine pending signals
// ============================================================================

int sys_sigpending(sigset_t *set) {
  if (!set)
    return -EFAULT;

  if (!current_process)
    return -ESRCH;

  *set = current_process->pending_signals;
  return 0;
}

// ============================================================================
// sys_sigsuspend - Atomically change mask and wait for signal
// ============================================================================

int sys_sigsuspend(const sigset_t *mask) {
  if (!mask)
    return -EFAULT;

  if (!current_process)
    return -ESRCH;

  // Save current mask
  sigset_t old_mask = current_process->signal_mask;

  // Set temporary mask
  current_process->signal_mask = *mask;
  // SIGKILL and SIGSTOP cannot be blocked
  current_process->signal_mask &= ~((sigset_t)1 << SIGKILL);
  current_process->signal_mask &= ~((sigset_t)1 << SIGSTOP);

  // Wait for any unblocked signal
  while (!(current_process->pending_signals & ~current_process->signal_mask)) {
    current_process->state = PROCESS_WAITING;
    schedule();
  }

  // Restore original mask
  current_process->signal_mask = old_mask;

  // sigsuspend always returns -1 with EINTR
  return -EINTR;
}

// ============================================================================
// sys_sigwait - Synchronously wait for signals
// ============================================================================

int sys_sigwait(const sigset_t *set, int *sig) {
  if (!set || !sig)
    return -EFAULT;

  if (!current_process)
    return -ESRCH;

  // Wait for a signal in the set to become pending
  while (1) {
    sigset_t pending = current_process->pending_signals & *set;
    if (pending) {
      // Find first pending signal
      for (int i = 1; i < NSIG; i++) {
        if (pending & ((sigset_t)1 << i)) {
          // Remove from pending
          current_process->pending_signals &= ~((sigset_t)1 << i);
          *sig = i;
          return 0;
        }
      }
    }
    current_process->state = PROCESS_WAITING;
    schedule();
  }
}

// ============================================================================
// sys_sigtimedwait - Wait for signals with timeout
// ============================================================================

int sys_sigtimedwait(const sigset_t *set, siginfo_t *info,
                     const void *timeout) {
  if (!set)
    return -EFAULT;

  if (!current_process)
    return -ESRCH;

  // Parse timeout (simplified - expects uint32_t ticks)
  uint32_t timeout_ticks = 0;
  if (timeout) {
    timeout_ticks = *((const uint32_t *)timeout);
  }

  uint32_t deadline = tick + timeout_ticks;

  while (1) {
    sigset_t pending = current_process->pending_signals & *set;
    if (pending) {
      // Find first pending signal
      for (int i = 1; i < NSIG; i++) {
        if (pending & ((sigset_t)1 << i)) {
          // Remove from pending
          current_process->pending_signals &= ~((sigset_t)1 << i);

          // Fill siginfo if provided
          if (info) {
            info->si_signo = i;
            info->si_errno = 0;
            info->si_code = 0;
            info->si_pid = 0;
            info->si_uid = 0;
          }
          return i;
        }
      }
    }

    // Check timeout
    if (timeout && tick >= deadline)
      return -EAGAIN;

    current_process->state = PROCESS_WAITING;
    schedule();
  }
}

// ============================================================================
// sys_kill - Send signal to a process
// ============================================================================

int sys_kill(int pid, int signum) {
  if (signum < 0 || signum >= NSIG)
    return -EINVAL;

  // Signal 0 is used to check if process exists (no signal sent)
  bool signal_zero = (signum == 0);

  // Handle special pid values
  if (pid == 0) {
    // Send to all processes in caller's process group
    pid = current_process->pgid;
    // Fall through to pgid handling
  }

  if (pid == -1) {
    // Send to all processes (except init and self)
    process_t *p = ready_queue;
    if (!p)
      return -ESRCH;

    bool found = false;
    process_t *start = p;
    do {
      if (p->id > 1 && p != current_process) {
        if (!signal_zero)
          p->pending_signals |= ((sigset_t)1 << signum);
        found = true;
        // Wake if waiting
        if (p->state == PROCESS_WAITING)
          p->state = PROCESS_READY;
      }
      p = p->next;
    } while (p && p != start);

    return found ? 0 : -ESRCH;
  }

  if (pid < -1) {
    // Send to all processes in process group |pid|
    int pgid = -pid;
    process_t *p = ready_queue;
    if (!p)
      return -ESRCH;

    bool found = false;
    process_t *start = p;
    do {
      if (p->pgid == (uint32_t)pgid) {
        if (!signal_zero)
          p->pending_signals |= ((sigset_t)1 << signum);
        found = true;
        if (p->state == PROCESS_WAITING)
          p->state = PROCESS_READY;
      }
      p = p->next;
    } while (p && p != start);

    return found ? 0 : -ESRCH;
  }

  // pid > 0: Send to specific process
  process_t *p = ready_queue;
  if (!p)
    return -ESRCH;

  process_t *start = p;
  do {
    if (p->id == (uint32_t)pid) {
      if (!signal_zero)
        p->pending_signals |= ((sigset_t)1 << signum);
      if (p->state == PROCESS_WAITING)
        p->state = PROCESS_READY;
      return 0;
    }
    p = p->next;
  } while (p && p != start);

  return -ESRCH;
}

// ============================================================================
// sys_pause - Suspend until signal
// ============================================================================

int sys_pause(void) {
  if (!current_process)
    return -ESRCH;

  // Wait until we receive any signal
  while (current_process->pending_signals == 0) {
    current_process->state = PROCESS_WAITING;
    schedule();
  }

  return -EINTR;
}

// ============================================================================
// sys_abort - Abort process (send SIGABRT to self)
// ============================================================================

int sys_abort(void) {
  serial_log("SIGNAL: Process aborting via abort()");
  sys_kill(current_process->id, SIGABRT);
  // If we get here, handler returned - forcefully exit
  exit_process(128 + SIGABRT);
  return 0; // Never reached
}

// ============================================================================
// kernel_sigreturn - Return from signal handler
// ============================================================================

int kernel_sigreturn(registers_t *regs) {
  if (!current_process || !current_process->in_signal_handler)
    return -EINVAL;

  // Restore saved context
  *regs = current_process->saved_context;
  current_process->in_signal_handler = 0;

  return 0; // Context switched, this value is from saved eax
}

// ============================================================================
// handle_signals - Called before returning to userspace
// ============================================================================

void handle_signals(registers_t *regs) {
  // Only handle signals when returning to user mode
  if ((regs->cs & 0x3) != 3)
    return;

  // Don't handle if already in a signal handler (prevent recursion)
  if (current_process->in_signal_handler)
    return;

  // Check for pending unblocked signals
  sigset_t deliverable =
      current_process->pending_signals & ~current_process->signal_mask;

  if (deliverable == 0)
    return;

  // Find highest-priority signal to deliver
  for (int sig = 1; sig < NSIG; sig++) {
    if (!(deliverable & ((sigset_t)1 << sig)))
      continue;

    // Clear pending bit
    current_process->pending_signals &= ~((sigset_t)1 << sig);

    struct sigaction *sa = &current_process->signal_actions[sig];
    sighandler_t handler = sa->sa_handler;

    // Check handler type
    if (handler == SIG_IGN) {
      // Ignored - continue to next signal
      continue;
    } else if (handler == SIG_DFL) {
      // Default action
      switch (sig) {
      case SIGCHLD:
      case SIGURG:
      case SIGWINCH:
      case SIGCONT:
        // Default: ignore
        continue;

      case SIGSTOP:
      case SIGTSTP:
      case SIGTTIN:
      case SIGTTOU:
        // Default: stop process
        current_process->state = PROCESS_WAITING;
        schedule();
        continue;

      default:
        // Default: terminate
        serial_log("SIGNAL: Terminating process due to unhandled signal");
        exit_process(128 + sig);
        return;
      }
    } else {
      // Custom handler - setup signal trampoline
      current_process->saved_context = *regs;
      current_process->in_signal_handler = 1;

      // Block signals during handler if SA_NODEFER not set
      if (!(sa->sa_flags & SA_NODEFER)) {
        current_process->signal_mask |= ((sigset_t)1 << sig);
      }
      current_process->signal_mask |= sa->sa_mask;

      // Setup user stack for signal handler call
      uint32_t *stack = (uint32_t *)(uintptr_t)regs->useresp;

      // Push return address (should call sigreturn)
      stack--;
      *stack = 0xDEADC0DE; // Indicates need for sigreturn syscall

      // Push signal number as argument
      stack--;
      *stack = sig;

      // Update stack pointer
      regs->useresp = (uint32_t)(uintptr_t)stack;

      // Jump to handler
      regs->eip = (uint32_t)(uintptr_t)handler;

      // Reset handler to default if SA_RESETHAND set
      if (sa->sa_flags & SA_RESETHAND)
        sa->sa_handler = SIG_DFL;

      // Only deliver one signal per interrupt
      return;
    }
  }
}

// ============================================================================
// signal_init - Initialize signal subsystem
// ============================================================================

void signal_init(void) { serial_log("SIGNAL: Subsystem initialized"); }

} // extern "C"
