// Wait Queue - Implementation of sleep/wake primitives
#include "wait_queue.h"
#include "heap.h"
#include "memory.h"
#include "process.h"

extern "C" {

void wait_queue_init(wait_queue_t *wq) {
  wq->head = 0;
  wq->tail = 0;
}

void sleep_on(wait_queue_t *wq) {
  if (!current_process || !wq)
    return;

  // Disable interrupts to prevent race conditions
  asm volatile("cli");

  // Add current process to wait queue
  wait_queue_entry_t *entry =
      (wait_queue_entry_t *)kmalloc(sizeof(wait_queue_entry_t));
  if (!entry) {
    asm volatile("sti");
    return;
  }

  entry->proc = current_process;
  entry->next = 0;

  if (!wq->head) {
    wq->head = entry;
    wq->tail = entry;
  } else {
    wq->tail->next = entry;
    wq->tail = entry;
  }

  // Set process state to sleeping
  current_process->state = PROCESS_WAITING;

  // Re-enable interrupts and yield
  asm volatile("sti");

  // Yield to scheduler - when we return, we've been woken up
  schedule();
}

void wake_up(wait_queue_t *wq) {
  if (!wq || !wq->head)
    return;

  asm volatile("cli");

  // Remove first entry from queue
  wait_queue_entry_t *entry = wq->head;
  wq->head = entry->next;
  if (!wq->head)
    wq->tail = 0;

  // Wake up the process
  if (entry->proc) {
    entry->proc->state = PROCESS_READY;
  }

  kfree(entry);

  asm volatile("sti");
}

void wake_up_all(wait_queue_t *wq) {
  if (!wq)
    return;

  asm volatile("cli");

  while (wq->head) {
    wait_queue_entry_t *entry = wq->head;
    wq->head = entry->next;

    if (entry->proc) {
      entry->proc->state = PROCESS_READY;
    }

    kfree(entry);
  }

  wq->tail = 0;

  asm volatile("sti");
}

int wait_queue_empty(wait_queue_t *wq) { return wq ? (wq->head == 0) : 1; }

} // extern "C"
