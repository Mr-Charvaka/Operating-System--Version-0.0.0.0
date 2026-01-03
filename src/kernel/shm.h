// Shared Memory - System V style shared memory
#ifndef SHM_H
#define SHM_H

#include "../include/types.h"

#define SHM_MAX_SEGMENTS 32
#define SHM_PAGE_SIZE 4096
#define IPC_PRIVATE 0

typedef struct shm_segment {
  uint32_t key;        // User-provided key
  uint32_t size;       // Size in bytes
  void *phys_addr;     // Physical address of pages
  uint32_t virt_start; // Known virtual start (for demand paging)
  int ref_count;       // Number of attachments
  int in_use;          // Is this slot used?
} shm_segment_t;

#ifdef __cplusplus
extern "C" {
#endif

// Initialize shared memory system
void shm_init();

// Create or get shared memory segment (returns segment ID or -1)
int sys_shmget(uint32_t key, uint32_t size, int flags);

// Attach shared memory to process address space (returns virtual addr or 0)
void *sys_shmat(int shmid);

// Detach shared memory from process
int sys_shmdt(void *addr);

// Look up segment by virtual address (for demand paging)
shm_segment_t *shm_get_segment(uint32_t virt_addr);
void shm_set_virt_range(int shmid, uint32_t virt_start);
void shm_free(int shmid);

#ifdef __cplusplus
}
#endif

#endif // SHM_H
