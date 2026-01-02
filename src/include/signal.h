#ifndef SIGNAL_H
#define SIGNAL_H

#include "isr.h"
#include "types.h"

#define SIGHUP 1
#define SIGINT 2
#define SIGKILL 9
#define SIGSEGV 11
#define SIGTERM 15
#define SIGCHLD 17

typedef void (*sighandler_t)(int);
#define SIG_DFL ((sighandler_t)0)
#define SIG_IGN ((sighandler_t)1)
#define SIG_ERR ((sighandler_t) - 1)

typedef struct {
  sighandler_t sa_handler;
  uint32_t sa_flags;
  // blocked set...
} sigaction_t;

#ifdef __cplusplus
extern "C" {
#endif

int sys_signal(int signum, sighandler_t handler);
int sys_kill(int pid, int signum);
int sys_sigreturn();
int kernel_sigreturn(registers_t *regs);

// Kernel-side helper
void handle_signals(registers_t *regs);

#ifdef __cplusplus
}
#endif

#endif
