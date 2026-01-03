#include "vm.h"
#include "../drivers/serial.h"
#include "../include/string.h"
#include "paging.h"
#include "pmm.h"

extern uint32_t *kernel_directory;

uint32_t *pd_create() {
  uint32_t phys_pd = (uint32_t)pmm_alloc_block();
  uint32_t *pd = (uint32_t *)PHYS_TO_VIRT(phys_pd);
  memset(pd, 0, 4096);

  for (int i = 0; i < 1024; i++) {
    if (kernel_directory[i] & 1) {
      pd[i] = kernel_directory[i];
    }
  }

  return (uint32_t *)phys_pd; // Return physical address for CR3 usage
}

uint32_t *pd_clone(uint32_t *source_pd_phys) {
  uint32_t phys_new_pd = (uint32_t)pd_create();
  uint32_t *new_pd = (uint32_t *)PHYS_TO_VIRT(phys_new_pd);
  uint32_t *source_pd = (uint32_t *)PHYS_TO_VIRT(source_pd_phys);

  for (int i = 0; i < 768; i++) {
    if (!(source_pd[i] & 1))
      continue;
    if (source_pd[i] == kernel_directory[i])
      continue;

    uint32_t *src_pt = (uint32_t *)PHYS_TO_VIRT(source_pd[i] & 0xFFFFF000);
    uint32_t phys_dest_pt = (uint32_t)pmm_alloc_block();
    uint32_t *dest_pt = (uint32_t *)PHYS_TO_VIRT(phys_dest_pt);
    memset(dest_pt, 0, 4096);

    new_pd[i] = phys_dest_pt | (source_pd[i] & 0xFFF);

    for (int j = 0; j < 1024; j++) {
      if (src_pt[j] & 1) {
        uint32_t src_phys = src_pt[j] & 0xFFFFF000;
        uint32_t dest_phys = (uint32_t)pmm_alloc_block();

        memcpy((void *)PHYS_TO_VIRT(dest_phys), (void *)PHYS_TO_VIRT(src_phys),
               4096);
        dest_pt[j] = dest_phys | (src_pt[j] & 0xFFF);
      }
    }
  }
  return (uint32_t *)phys_new_pd;
}

void pd_destroy(uint32_t *pd_phys) {
  uint32_t *pd = (uint32_t *)PHYS_TO_VIRT(pd_phys);
  for (int i = 0; i < 1024; i++) {
    if (!(pd[i] & 1))
      continue;
    if (pd[i] == kernel_directory[i])
      continue;

    uint32_t *pt = (uint32_t *)PHYS_TO_VIRT(pd[i] & 0xFFFFF000);
    for (int j = 0; j < 1024; j++) {
      if (pt[j] & 1) {
        pmm_free_block((void *)(pt[j] & 0xFFFFF000));
      }
    }
    pmm_free_block((void *)VIRT_TO_PHYS(pt));
  }
  pmm_free_block((void *)pd_phys);
}

void pd_switch(uint32_t *pd_phys) {
  asm volatile("mov %0, %%cr3" ::"r"(pd_phys));
}

void vm_map_page(uint32_t phys, uint32_t virt, uint32_t flags) {
  uint32_t phys_pd;
  asm volatile("mov %%cr3, %0" : "=r"(phys_pd));
  uint32_t *pd = (uint32_t *)PHYS_TO_VIRT(phys_pd);

  uint32_t pd_index = virt >> 22;
  uint32_t pt_index = (virt >> 12) & 0x03FF;

  if (!(pd[pd_index] & 1)) {
    uint32_t phys_pt = (uint32_t)pmm_alloc_block();
    uint32_t *virt_pt = (uint32_t *)PHYS_TO_VIRT(phys_pt);
    memset(virt_pt, 0, 4096);
    pd[pd_index] = phys_pt | 7;
  }

  uint32_t *pt = (uint32_t *)PHYS_TO_VIRT(pd[pd_index] & 0xFFFFF000);
  pt[pt_index] = (phys & 0xFFFFF000) | flags;

  asm volatile("invlpg (%0)" ::"r"(virt) : "memory");
}

uint32_t vm_get_phys(uint32_t virt) {
  uint32_t phys_pd;
  asm volatile("mov %%cr3, %0" : "=r"(phys_pd));
  uint32_t *pd = (uint32_t *)PHYS_TO_VIRT(phys_pd);

  uint32_t pd_index = virt >> 22;
  uint32_t pt_index = (virt >> 12) & 0x03FF;

  if (!(pd[pd_index] & 1))
    return 0;
  uint32_t *pt = (uint32_t *)PHYS_TO_VIRT(pd[pd_index] & 0xFFFFF000);
  if (!(pt[pt_index] & 1))
    return 0;

  return (pt[pt_index] & 0xFFFFF000) + (virt & 0xFFF);
}

void vm_unmap_page(uint32_t virt) {
  uint32_t phys_pd;
  asm volatile("mov %%cr3, %0" : "=r"(phys_pd));
  uint32_t *pd = (uint32_t *)PHYS_TO_VIRT(phys_pd);

  uint32_t pd_index = virt >> 22;
  uint32_t pt_index = (virt >> 12) & 0x03FF;

  if (!(pd[pd_index] & 1))
    return;
  uint32_t *pt = (uint32_t *)PHYS_TO_VIRT(pd[pd_index] & 0xFFFFF000);
  if (pt[pt_index] & 1) {
    pmm_free_block((void *)(pt[pt_index] & 0xFFFFF000));
    pt[pt_index] = 0;
  }
  asm volatile("invlpg (%0)" ::"r"(virt) : "memory");
}

void vm_clear_user_mappings() {
  uint32_t phys_pd;
  asm volatile("mov %%cr3, %0" : "=r"(phys_pd));
  uint32_t *pd = (uint32_t *)PHYS_TO_VIRT(phys_pd);

  for (int i = 256; i < 768; i++) {
    if (pd[i] & 1) {
      uint32_t *pt = (uint32_t *)PHYS_TO_VIRT(pd[i] & 0xFFFFF000);
      for (int j = 0; j < 1024; j++) {
        if (pt[j] & 1) {
          pmm_free_block((void *)(pt[j] & 0xFFFFF000));
          pt[j] = 0;
        }
      }
      pmm_free_block((void *)(pd[i] & 0xFFFFF000));
      pd[i] = 0;
    }
  }
  asm volatile("mov %%cr3, %%eax; mov %%eax, %%cr3" ::: "eax");
}
