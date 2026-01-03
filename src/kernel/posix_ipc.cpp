// ============================================================================
// posix_ipc.cpp - POSIX IPC Extensions
// Entirely hand-crafted for Retro-OS kernel
// ============================================================================

#include "../drivers/serial.h"
#include "../include/errno.h"
#include "../include/fcntl.h"
#include "../include/semaphore.h"
#include "../include/string.h"
#include "heap.h"
#include "memory.h"
#include "process.h"

extern "C" {

// ============================================================================
// Unnamed Semaphores
// ============================================================================

int sem_init(sem_t *sem, int pshared, unsigned int value) {
  (void)pshared; // Process sharing not implemented

  if (!sem)
    return -1;

  if (value > 0x7FFFFFFF) {
    // errno = EINVAL;
    return -1;
  }

  sem->value = value;
  sem->waiters = 0;

  return 0;
}

int sem_destroy(sem_t *sem) {
  if (!sem)
    return -1;

  if (sem->waiters > 0) {
    // errno = EBUSY;
    return -1;
  }

  return 0;
}

int sem_wait(sem_t *sem) {
  if (!sem)
    return -1;

  while (1) {
    asm volatile("cli");

    if (sem->value > 0) {
      sem->value--;
      asm volatile("sti");
      return 0;
    }

    sem->waiters++;
    asm volatile("sti");

    // Block until signaled
    current_process->state = PROCESS_WAITING;
    schedule();

    asm volatile("cli");
    sem->waiters--;
    asm volatile("sti");
  }
}

int sem_trywait(sem_t *sem) {
  if (!sem)
    return -1;

  asm volatile("cli");

  if (sem->value > 0) {
    sem->value--;
    asm volatile("sti");
    return 0;
  }

  asm volatile("sti");
  // errno = EAGAIN;
  return -1;
}

int sem_timedwait(sem_t *sem, const void *abs_timeout) {
  (void)abs_timeout; // Timed wait not fully implemented
  return sem_wait(sem);
}

int sem_post(sem_t *sem) {
  if (!sem)
    return -1;

  asm volatile("cli");

  if (sem->value == 0x7FFFFFFF) {
    asm volatile("sti");
    // errno = EOVERFLOW;
    return -1;
  }

  sem->value++;

  // Wake up one waiter
  if (sem->waiters > 0) {
    // In a real implementation, we'd wake a specific process
    process_t *p = ready_queue;
    if (p) {
      process_t *start = p;
      do {
        if (p->state == PROCESS_WAITING) {
          p->state = PROCESS_READY;
          break;
        }
        p = p->next;
      } while (p && p != start);
    }
  }

  asm volatile("sti");
  return 0;
}

int sem_getvalue(sem_t *sem, int *sval) {
  if (!sem || !sval)
    return -1;

  *sval = sem->value;
  return 0;
}

// ============================================================================
// Named Semaphores
// ============================================================================
#define MAX_NAMED_SEMS 32

struct named_sem {
  char name[64];
  sem_t sem;
  int ref_count;
  int unlinked;
};

static struct named_sem named_sems[MAX_NAMED_SEMS];
static int named_sems_initialized = 0;

static void init_named_sems(void) {
  if (named_sems_initialized)
    return;

  for (int i = 0; i < MAX_NAMED_SEMS; i++) {
    named_sems[i].name[0] = 0;
    named_sems[i].ref_count = 0;
    named_sems[i].unlinked = 0;
  }

  named_sems_initialized = 1;
}

sem_t *sem_open(const char *name, int oflag, ...) {
  init_named_sems();

  if (!name || name[0] != '/') {
    // errno = EINVAL;
    return SEM_FAILED;
  }

  // Look for existing
  for (int i = 0; i < MAX_NAMED_SEMS; i++) {
    if (named_sems[i].name[0] && strcmp(named_sems[i].name, name) == 0) {
      if ((oflag & O_CREAT) && (oflag & O_EXCL)) {
        // errno = EEXIST;
        return SEM_FAILED;
      }
      named_sems[i].ref_count++;
      return &named_sems[i].sem;
    }
  }

  // Create new
  if (!(oflag & O_CREAT)) {
    // errno = ENOENT;
    return SEM_FAILED;
  }

  for (int i = 0; i < MAX_NAMED_SEMS; i++) {
    if (!named_sems[i].name[0]) {
      strncpy(named_sems[i].name, name, 63);
      named_sems[i].name[63] = 0;
      named_sems[i].sem.value = 1; // Default value
      named_sems[i].sem.waiters = 0;
      named_sems[i].ref_count = 1;
      named_sems[i].unlinked = 0;
      return &named_sems[i].sem;
    }
  }

  // errno = ENFILE;
  return SEM_FAILED;
}

int sem_close(sem_t *sem) {
  if (!sem)
    return -1;

  for (int i = 0; i < MAX_NAMED_SEMS; i++) {
    if (&named_sems[i].sem == sem) {
      named_sems[i].ref_count--;
      if (named_sems[i].ref_count <= 0 && named_sems[i].unlinked) {
        named_sems[i].name[0] = 0;
      }
      return 0;
    }
  }

  return -1;
}

int sem_unlink(const char *name) {
  if (!name)
    return -1;

  for (int i = 0; i < MAX_NAMED_SEMS; i++) {
    if (strcmp(named_sems[i].name, name) == 0) {
      named_sems[i].unlinked = 1;
      if (named_sems[i].ref_count <= 0) {
        named_sems[i].name[0] = 0;
      }
      return 0;
    }
  }

  // errno = ENOENT;
  return -1;
}

// ============================================================================
// Message Queues
// ============================================================================
#define MAX_MESSAGE_QUEUES 16
#define MAX_MESSAGES 64
#define MAX_MSG_SIZE 256

struct message {
  char data[MAX_MSG_SIZE];
  size_t len;
  unsigned int priority;
  struct message *next;
};

struct message_queue {
  char name[64];
  struct mq_attr attr;
  struct message *head;
  struct message *tail;
  int ref_count;
  int unlinked;
};

static struct message_queue mqueues[MAX_MESSAGE_QUEUES];
static int mqueues_initialized = 0;

static void init_mqueues(void) {
  if (mqueues_initialized)
    return;

  for (int i = 0; i < MAX_MESSAGE_QUEUES; i++) {
    mqueues[i].name[0] = 0;
    mqueues[i].head = 0;
    mqueues[i].tail = 0;
    mqueues[i].ref_count = 0;
  }

  mqueues_initialized = 1;
}

mqd_t mq_open(const char *name, int oflag, ...) {
  init_mqueues();

  if (!name || name[0] != '/') {
    // errno = EINVAL;
    return -1;
  }

  // Look for existing
  for (int i = 0; i < MAX_MESSAGE_QUEUES; i++) {
    if (mqueues[i].name[0] && strcmp(mqueues[i].name, name) == 0) {
      if ((oflag & O_CREAT) && (oflag & O_EXCL)) {
        // errno = EEXIST;
        return -1;
      }
      mqueues[i].ref_count++;
      return i;
    }
  }

  // Create new
  if (!(oflag & O_CREAT)) {
    // errno = ENOENT;
    return -1;
  }

  for (int i = 0; i < MAX_MESSAGE_QUEUES; i++) {
    if (!mqueues[i].name[0]) {
      strncpy(mqueues[i].name, name, 63);
      mqueues[i].name[63] = 0;
      mqueues[i].attr.mq_flags = 0;
      mqueues[i].attr.mq_maxmsg = MAX_MESSAGES;
      mqueues[i].attr.mq_msgsize = MAX_MSG_SIZE;
      mqueues[i].attr.mq_curmsgs = 0;
      mqueues[i].head = 0;
      mqueues[i].tail = 0;
      mqueues[i].ref_count = 1;
      mqueues[i].unlinked = 0;
      return i;
    }
  }

  // errno = ENFILE;
  return -1;
}

int mq_close(mqd_t mqdes) {
  if (mqdes < 0 || mqdes >= MAX_MESSAGE_QUEUES)
    return -1;

  mqueues[mqdes].ref_count--;
  if (mqueues[mqdes].ref_count <= 0 && mqueues[mqdes].unlinked) {
    // Free all messages
    struct message *msg = mqueues[mqdes].head;
    while (msg) {
      struct message *next = msg->next;
      kfree(msg);
      msg = next;
    }
    mqueues[mqdes].name[0] = 0;
    mqueues[mqdes].head = 0;
    mqueues[mqdes].tail = 0;
  }

  return 0;
}

int mq_unlink(const char *name) {
  if (!name)
    return -1;

  for (int i = 0; i < MAX_MESSAGE_QUEUES; i++) {
    if (strcmp(mqueues[i].name, name) == 0) {
      mqueues[i].unlinked = 1;
      if (mqueues[i].ref_count <= 0) {
        // Free all messages
        struct message *msg = mqueues[i].head;
        while (msg) {
          struct message *next = msg->next;
          kfree(msg);
          msg = next;
        }
        mqueues[i].name[0] = 0;
        mqueues[i].head = 0;
        mqueues[i].tail = 0;
      }
      return 0;
    }
  }

  // errno = ENOENT;
  return -1;
}

int mq_send(mqd_t mqdes, const char *msg_ptr, size_t msg_len,
            unsigned int msg_prio) {
  if (mqdes < 0 || mqdes >= MAX_MESSAGE_QUEUES || !mqueues[mqdes].name[0])
    return -1;

  if (!msg_ptr || msg_len > MAX_MSG_SIZE)
    return -1;

  if (mqueues[mqdes].attr.mq_curmsgs >= mqueues[mqdes].attr.mq_maxmsg) {
    // Queue full - would block
    // For now, return error
    // errno = EAGAIN;
    return -1;
  }

  struct message *msg = (struct message *)kmalloc(sizeof(struct message));
  if (!msg)
    return -1;

  memcpy(msg->data, msg_ptr, msg_len);
  msg->len = msg_len;
  msg->priority = msg_prio;
  msg->next = 0;

  // Insert by priority (higher priority first)
  if (!mqueues[mqdes].head || msg_prio > mqueues[mqdes].head->priority) {
    msg->next = mqueues[mqdes].head;
    mqueues[mqdes].head = msg;
    if (!mqueues[mqdes].tail)
      mqueues[mqdes].tail = msg;
  } else {
    struct message *cur = mqueues[mqdes].head;
    while (cur->next && cur->next->priority >= msg_prio)
      cur = cur->next;
    msg->next = cur->next;
    cur->next = msg;
    if (!msg->next)
      mqueues[mqdes].tail = msg;
  }

  mqueues[mqdes].attr.mq_curmsgs++;

  return 0;
}

int mq_timedsend(mqd_t mqdes, const char *msg_ptr, size_t msg_len,
                 unsigned int msg_prio, const void *abs_timeout) {
  (void)abs_timeout;
  return mq_send(mqdes, msg_ptr, msg_len, msg_prio);
}

ssize_t mq_receive(mqd_t mqdes, char *msg_ptr, size_t msg_len,
                   unsigned int *msg_prio) {
  if (mqdes < 0 || mqdes >= MAX_MESSAGE_QUEUES || !mqueues[mqdes].name[0])
    return -1;

  if (!msg_ptr)
    return -1;

  if (!mqueues[mqdes].head) {
    // Queue empty - would block
    // errno = EAGAIN;
    return -1;
  }

  struct message *msg = mqueues[mqdes].head;
  mqueues[mqdes].head = msg->next;
  if (!mqueues[mqdes].head)
    mqueues[mqdes].tail = 0;

  mqueues[mqdes].attr.mq_curmsgs--;

  size_t copy_len = (msg->len < msg_len) ? msg->len : msg_len;
  memcpy(msg_ptr, msg->data, copy_len);

  if (msg_prio)
    *msg_prio = msg->priority;

  ssize_t result = (ssize_t)msg->len;
  kfree(msg);

  return result;
}

ssize_t mq_timedreceive(mqd_t mqdes, char *msg_ptr, size_t msg_len,
                        unsigned int *msg_prio, const void *abs_timeout) {
  (void)abs_timeout;
  return mq_receive(mqdes, msg_ptr, msg_len, msg_prio);
}

int mq_getattr(mqd_t mqdes, struct mq_attr *attr) {
  if (mqdes < 0 || mqdes >= MAX_MESSAGE_QUEUES || !mqueues[mqdes].name[0])
    return -1;

  if (!attr)
    return -1;

  *attr = mqueues[mqdes].attr;
  return 0;
}

int mq_setattr(mqd_t mqdes, const struct mq_attr *newattr,
               struct mq_attr *oldattr) {
  if (mqdes < 0 || mqdes >= MAX_MESSAGE_QUEUES || !mqueues[mqdes].name[0])
    return -1;

  if (oldattr)
    *oldattr = mqueues[mqdes].attr;

  if (newattr) {
    // Only mq_flags can be set
    mqueues[mqdes].attr.mq_flags = newattr->mq_flags;
  }

  return 0;
}

int mq_notify(mqd_t mqdes, const void *sevp) {
  (void)mqdes;
  (void)sevp;
  // Notification not implemented
  return -ENOSYS;
}

// ============================================================================
// POSIX Named Shared Memory
// ============================================================================
#define MAX_SHM_OBJECTS 16

struct shm_object {
  char name[64];
  int fd;
  size_t size;
  void *addr;
  int ref_count;
  int unlinked;
};

static struct shm_object shm_objects[MAX_SHM_OBJECTS];
static int shm_objects_initialized = 0;
static int next_shm_fd = 100;

static void init_shm_objects(void) {
  if (shm_objects_initialized)
    return;

  for (int i = 0; i < MAX_SHM_OBJECTS; i++) {
    shm_objects[i].name[0] = 0;
    shm_objects[i].ref_count = 0;
  }

  shm_objects_initialized = 1;
}

int shm_open(const char *name, int oflag, int mode) {
  (void)mode;
  init_shm_objects();

  if (!name || name[0] != '/') {
    // errno = EINVAL;
    return -1;
  }

  // Look for existing
  for (int i = 0; i < MAX_SHM_OBJECTS; i++) {
    if (shm_objects[i].name[0] && strcmp(shm_objects[i].name, name) == 0) {
      if ((oflag & O_CREAT) && (oflag & O_EXCL)) {
        // errno = EEXIST;
        return -1;
      }
      shm_objects[i].ref_count++;
      return shm_objects[i].fd;
    }
  }

  // Create new
  if (!(oflag & O_CREAT)) {
    // errno = ENOENT;
    return -1;
  }

  for (int i = 0; i < MAX_SHM_OBJECTS; i++) {
    if (!shm_objects[i].name[0]) {
      strncpy(shm_objects[i].name, name, 63);
      shm_objects[i].name[63] = 0;
      shm_objects[i].fd = next_shm_fd++;
      shm_objects[i].size = 0;
      shm_objects[i].addr = 0;
      shm_objects[i].ref_count = 1;
      shm_objects[i].unlinked = 0;
      return shm_objects[i].fd;
    }
  }

  // errno = ENFILE;
  return -1;
}

int shm_unlink(const char *name) {
  if (!name)
    return -1;

  for (int i = 0; i < MAX_SHM_OBJECTS; i++) {
    if (strcmp(shm_objects[i].name, name) == 0) {
      shm_objects[i].unlinked = 1;
      if (shm_objects[i].ref_count <= 0) {
        if (shm_objects[i].addr)
          kfree(shm_objects[i].addr);
        shm_objects[i].name[0] = 0;
      }
      return 0;
    }
  }

  // errno = ENOENT;
  return -1;
}

} // extern "C"
