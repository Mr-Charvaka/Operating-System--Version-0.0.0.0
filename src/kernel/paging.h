#ifndef PAGING_H
#define PAGING_H

#include "../include/isr.h"
#include "../include/types.h"

#define KERNEL_VIRTUAL_BASE 0xC0000000
#define KERNEL_PAGE_DIRECTORY_INDEX (KERNEL_VIRTUAL_BASE >> 22)

#define PHYS_TO_VIRT(addr) ((uint32_t)(addr) + KERNEL_VIRTUAL_BASE)
#define VIRT_TO_PHYS(addr) ((uint32_t)(addr) - KERNEL_VIRTUAL_BASE)

void init_paging();
void switch_page_directory(uint32_t *new_dir);
void paging_map(uint32_t phys, uint32_t virt, uint32_t flags);
extern uint32_t *kernel_directory;
extern uint32_t *current_directory;

#define PTE_PRESENT 0x1
#define PTE_RW 0x2
#define PTE_USER 0x4
#define PTE_WRITE PTE_RW
#define PTE_NX                                                                 \
  0x80000000 // Only valid if EFER.NXE is enabled, harmless if ignored in 32-bit
             // without PAE usually?
// Actually in legacy 32-bit without PAE, bit 63 doesn't exist. Flags are
// 32-bit. If we want NX, we assume these bits are ignored or handled. But wait,
// uint32_t flags in mprotect. Let's define PTE_NX as 0 for now to avoid issues
// if we are strict 32-bit. But mprotect.cpp uses it.

#undef PTE_NX
#define PTE_NX 0 // Disabled for now

uint32_t *paging_get_pte(uint32_t virt);

#endif
