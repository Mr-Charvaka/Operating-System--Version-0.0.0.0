#include "vm.h"
#include "../drivers/serial.h"
#include "../include/string.h"
#include "paging.h"
#include "pmm.h"

extern uint32_t *kernel_directory;

uint32_t *pd_create() {
  // 1. Allocate a new Page Directory (4KB)
  uint32_t *pd = (uint32_t *)pmm_alloc_block();
  memset(pd, 0, 4096);

  // 2. Copy Kernel Mappings
  // We must copy ALL allocations present in the Kernel Directory.
  // This includes Low Memory (Identity), Heap, AND High Memory (LAPIC, IOAPIC).
  for (int i = 0; i < 1024; i++) {
    if (kernel_directory[i] & 1) { // If present
      pd[i] = kernel_directory[i];
    }
  }

  // 3. Recursive Mapping? (Optional, specific technique)
  // For now we assume we can access 'pd' directly because it's in identity
  // space.

  return pd;
}

uint32_t *pd_clone(uint32_t *source_pd) {
  // 1. Create a new PD with kernel mappings already shared
  uint32_t *new_pd = pd_create();
  if (!new_pd)
    return nullptr;

  // 2. Clone ONLY User Mappings (skip kernel shared mappings)
  // User space typically starts at 0x40000000 (PDE index 256)
  // and goes up to 0xC0000000 (PDE index 768, which is kernel space start)
  // We must NOT deep-copy kernel identity mappings (indices 0-255).

  // Check each PDE: if it's the same as kernel_directory, it's shared - skip
  // it. Only deep-copy if it's different (i.e., a user-created mapping).
  for (int i = 0; i < 768; i++) {
    if (!(source_pd[i] & 1))
      continue; // Not present, skip

    // If this PDE matches the kernel's, it's a shared kernel mapping - skip
    if (source_pd[i] == kernel_directory[i])
      continue;

    // This is a user-private page table. Deep copy it.
    uint32_t *src_pt = (uint32_t *)(source_pd[i] & 0xFFFFF000);
    uint32_t *dest_pt = (uint32_t *)pmm_alloc_block();
    if (!dest_pt) {
      serial_log("pd_clone: OOM allocating page table");
      return nullptr;
    }
    memset(dest_pt, 0, 4096);

    // Link PT to new PD
    new_pd[i] = ((uint32_t)dest_pt) | (source_pd[i] & 0xFFF);

    for (int j = 0; j < 1024; j++) {
      if (src_pt[j] & 1) {
        // Deep copy the page itself
        uint32_t src_phys = src_pt[j] & 0xFFFFF000;
        void *dest_phys = pmm_alloc_block();
        if (!dest_phys) {
          serial_log("pd_clone: OOM allocating page");
          return nullptr;
        }

        // Copy using identity mapping (works because physical addresses < 512MB
        // are identity mapped)
        memcpy(dest_phys, (void *)src_phys, 4096);

        dest_pt[j] = ((uint32_t)dest_phys) | (src_pt[j] & 0xFFF);
      }
    }
  }

  return new_pd;
}

void pd_destroy(uint32_t *pd) {
  if (!pd)
    return;

  // We must free ONLY user-private tables and pages.
  // Check each PDE: if it's DIFFERENT from the kernel's, it's a user table.
  for (int i = 0; i < 1024; i++) {
    if (!(pd[i] & 1))
      continue; // Not present

    // If this PDE matches the kernel's, it's shared - DON'T free it or its
    // pages
    if (pd[i] == kernel_directory[i])
      continue;

    // This is a user-private page table.
    uint32_t *pt = (uint32_t *)(pd[i] & 0xFFFFF000);

    // Free all pages in this table
    for (int j = 0; j < 1024; j++) {
      if (pt[j] & 1) {
        pmm_free_block((void *)(pt[j] & 0xFFFFF000));
      }
    }

    // Free the page table itself
    pmm_free_block((void *)pt);
    pd[i] = 0;
  }

  // Free the Page Directory itself
  pmm_free_block((void *)pd);
}

void pd_switch(uint32_t *pd) { asm volatile("mov %0, %%cr3" ::"r"(pd)); }

void vm_map_page(uint32_t phys, uint32_t virt, uint32_t flags) {
  // Maps in the CURRENT directory (CR3)
  // Logic similar to paging_map but dynamic relative to CR3?
  // Actually, simpler to use the global 'current_directory' if generic,
  // but switch_pd in paging.cpp updates 'current_directory'.

  // We reuse the existing logic in paging.cpp but tailored or exposed.
  // Let's implement it cleanly here using CR3 reading implies we trust current
  // context.

  // BUT, we need to access page tables.
  // If CR3 points to 'pd', and 'pd' is identity mapped, we can read it.

  uint32_t *pd;
  asm volatile("mov %%cr3, %0" : "=r"(pd));

  uint32_t pd_index = virt >> 22;
  uint32_t pt_index = (virt >> 12) & 0x03FF;

  if (!(pd[pd_index] & 1)) {
    // Create new PT
    uint32_t *new_pt = (uint32_t *)pmm_alloc_block();
    memset(new_pt, 0, 4096);
    // Map as User/RW/Present (7)
    pd[pd_index] = ((uint32_t)new_pt) | 7;
  }

  uint32_t *pt = (uint32_t *)(pd[pd_index] & 0xFFFFF000);
  pt[pt_index] = (phys & 0xFFFFF000) | flags;

  // Invalidate TLB
  asm volatile("invlpg (%0)" ::"r"(virt) : "memory");
}

uint32_t vm_get_phys(uint32_t virt) {
  uint32_t *pd;
  asm volatile("mov %%cr3, %0" : "=r"(pd));

  uint32_t pd_index = virt >> 22;
  uint32_t pt_index = (virt >> 12) & 0x03FF;

  if (!(pd[pd_index] & 1))
    return 0;

  uint32_t *pt = (uint32_t *)(pd[pd_index] & 0xFFFFF000);
  if (!(pt[pt_index] & 1))
    return 0;

  return (pt[pt_index] & 0xFFFFF000) + (virt & 0xFFF);
}

void vm_unmap_page(uint32_t virt) {
  uint32_t *pd;
  asm volatile("mov %%cr3, %0" : "=r"(pd));

  uint32_t pd_index = virt >> 22;
  uint32_t pt_index = (virt >> 12) & 0x03FF;

  if (!(pd[pd_index] & 1))
    return;

  uint32_t *pt = (uint32_t *)(pd[pd_index] & 0xFFFFF000);
  if (pt[pt_index] & 1) {
    // We should free the physical block
    void *phys = (void *)(pt[pt_index] & 0xFFFFF000);
    pmm_free_block(phys);
    pt[pt_index] = 0;
  }

  // Invalidate TLB
  asm volatile("invlpg (%0)" ::"r"(virt) : "memory");
}

void vm_clear_user_mappings() {
  uint32_t *pd;
  asm volatile("mov %%cr3, %0" : "=r"(pd));

  // Clear all mappings between 0x40000000 and 0xC0000000
  // (PDE indices 256 to 768)
  for (int i = 256; i < 768; i++) {
    if (pd[i] & 1) {
      uint32_t *pt = (uint32_t *)(pd[i] & 0xFFFFF000);
      for (int j = 0; j < 1024; j++) {
        if (pt[j] & 1) {
          pmm_free_block((void *)(pt[j] & 0xFFFFF000));
          pt[j] = 0;
        }
      }
      pmm_free_block((void *)pd[i]);
      pd[i] = 0;
    }
  }

  // Flush TLB
  asm volatile("mov %%cr3, %%eax; mov %%eax, %%cr3" ::: "eax");
}
