#ifndef VM_H
#define VM_H

#include "../include/types.h"

// Create a new Page Directory with kernel mappings copied
uint32_t *pd_create();

// Clone a Page Directory (used for fork)
uint32_t *pd_clone(uint32_t *source_pd);

// Destroy a Page Directory (freeing its own structure, but NOT the shared
// kernel tables)
void pd_destroy(uint32_t *pd);

// Switch the current page directory (CR3)
void pd_switch(uint32_t *pd);

// Map a physical page to a virtual address in the CURRENT directory
// flags: 7 = User/RW/Present, 3 = Kernel/RW/Present
void vm_map_page(uint32_t phys, uint32_t virt, uint32_t flags);

// Unmap a page
void vm_unmap_page(uint32_t virt);

// Get physical address for virtual, returns 0 if not mapped
uint32_t vm_get_phys(uint32_t virt);

void vm_clear_user_mappings();

#endif
