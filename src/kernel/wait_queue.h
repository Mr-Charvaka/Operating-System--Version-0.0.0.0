// Wait Queue - Sleep/Wake primitives for blocking I/O
#ifndef WAIT_QUEUE_H
#define WAIT_QUEUE_H

#include "../include/types.h"

struct process; // Forward declaration

typedef struct wait_queue_entry {
  struct process *proc;
  struct wait_queue_entry *next;
} wait_queue_entry_t;

typedef struct wait_queue {
  wait_queue_entry_t *head;
  wait_queue_entry_t *tail;
} wait_queue_t;

#define WAIT_QUEUE_INIT {0, 0}

#ifdef __cplusplus
extern "C" {
#endif

// Initialize a wait queue
void wait_queue_init(wait_queue_t *wq);

// Put current process to sleep on this wait queue
void sleep_on(wait_queue_t *wq);

// Wake up one process from the queue
void wake_up(wait_queue_t *wq);

// Wake up all processes from the queue
void wake_up_all(wait_queue_t *wq);

// Check if queue is empty
int wait_queue_empty(wait_queue_t *wq);

#ifdef __cplusplus
}
#endif

#endif // WAIT_QUEUE_H
