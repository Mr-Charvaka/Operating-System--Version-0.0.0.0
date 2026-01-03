#include "paging.h"
#include "../drivers/hpet.h"
#include "../drivers/serial.h"
#include "../include/signal.h"
#include "../include/string.h"
#include "../kernel/memory.h"
#include "apic.h"
#include "pmm.h"
#include "process.h"

uint32_t *kernel_directory = 0;
uint32_t *current_directory = 0;

extern "C" uint32_t _text_start;
extern "C" uint32_t _text_end;
extern "C" uint32_t _rodata_start;
extern "C" uint32_t _rodata_end;

#include "shm.h"
#include "vm.h"

// Forward declaration to avoid circular include
typedef struct process process_t;
extern process_t *current_process;

void page_fault_handler(registers_t *regs);
bool handle_demand_paging(uint32_t faulting_address);

void page_fault_handler(registers_t *regs) {
  uint32_t faulting_address;
  asm volatile("mov %%cr2, %0" : "=r"(faulting_address));

  if (handle_demand_paging(faulting_address)) {
    return; // Fault fixed!
  }

  serial_log("PAGE FAULT! Address:");
  serial_log_hex("", faulting_address);
  serial_log_hex("  EIP: ", regs->eip);

  int us = regs->err_code & 0x4;
  if (us) {
    serial_log("PAGE FAULT: Killing user process.");
    sys_kill(current_process->id, SIGSEGV);
    return;
  }

  serial_log("KERNEL PANIC: Page Fault");
  for (;;)
    ;
}

bool handle_demand_paging(uint32_t addr) {
  if (!current_process)
    return false;

  // 1. Check SHM Range (0x70000000 to 0x80000000)
  if (addr >= 0x70000000 && addr < 0x80000000) {
    shm_segment_t *seg = shm_get_segment(addr);
    if (seg) {
      uint32_t page_base = addr & 0xFFFFF000;
      uint32_t phys_base =
          (uint32_t)(uintptr_t)seg->phys_addr + (page_base - seg->virt_start);

      serial_log_hex("DEMAND: Mapping SHM at ", addr);
      vm_map_page(phys_base, page_base, 7); // User|RW|Present
      return true;
    } else {
      serial_log_hex("DEMAND FAIL: SHM Segment not found for ", addr);
    }
  }

  // 2. Check User Heap Range (0x40000000 up to heap_end)
  if (addr >= 0x40000000 &&
      addr < current_process->heap_end + 0x200000) { // Increased grace to 2MB
    uint32_t page_base = addr & 0xFFFFF000;
    uint32_t phys = (uint32_t)pmm_alloc_block();
    serial_log_hex("DEMAND: Mapping HEAP at ", addr);
    vm_map_page(phys, page_base, 7);
    return true;
  }

  // 3. Check User Stack Range (near 0xB0000000)
  if (addr >= 0xAF000000 && addr <= 0xB0000000) {
    uint32_t page_base = addr & 0xFFFFF000;
    uint32_t phys = (uint32_t)pmm_alloc_block();
    serial_log_hex("DEMAND: Mapping STACK at ", addr);
    vm_map_page(phys, page_base, 7);
    return true;
  }

  return false;
}

void paging_map(uint32_t phys, uint32_t virt, uint32_t flags) {
  uint32_t pd_index = virt >> 22;
  uint32_t pt_index = (virt >> 12) & 0x03FF;

  if (!(kernel_directory[pd_index] & 1)) {
    uint32_t phys_pt = (uint32_t)pmm_alloc_block();
    uint32_t *virt_pt = (uint32_t *)PHYS_TO_VIRT(phys_pt);
    memset(virt_pt, 0, 4096);
    kernel_directory[pd_index] = phys_pt | 7;
  }

  uint32_t *pt =
      (uint32_t *)PHYS_TO_VIRT(kernel_directory[pd_index] & 0xFFFFF000);
  pt[pt_index] = (phys & 0xFFFFF000) | flags;
}

void init_paging() {
  serial_log("PAGING: Initializing Unified Memory Map...");

  uint32_t phys_pd = (uint32_t)pmm_alloc_block();
  kernel_directory = (uint32_t *)PHYS_TO_VIRT(phys_pd);
  memset(kernel_directory, 0, 4096);

  // Map 512MB
  for (int j = 0; j < 128; j++) {
    uint32_t phys_pt = (uint32_t)pmm_alloc_block();
    uint32_t *virt_pt = (uint32_t *)PHYS_TO_VIRT(phys_pt);
    memset(virt_pt, 0, 4096);

    for (int i = 0; i < 1024; i++) {
      uint32_t phys_addr = j * 4 * 1024 * 1024 + i * 4096;
      uint32_t virt_addr = phys_addr + KERNEL_VIRTUAL_BASE;

      uint32_t flags = 3;
      if (virt_addr >= (uint32_t)&_text_start &&
          virt_addr < (uint32_t)&_text_end) {
        flags = 1;
      } else if (virt_addr >= (uint32_t)&_rodata_start &&
                 virt_addr < (uint32_t)&_rodata_end) {
        flags = 1;
      }
      virt_pt[i] = phys_addr | flags;
    }

    // 1. Higher-Half Mapping (3GB+)
    kernel_directory[KERNEL_PAGE_DIRECTORY_INDEX + j] = phys_pt | 3;

    // 2. Identity Mapping (0-512MB) - Required for ACPI and legacy driver
    // initialization
    kernel_directory[j] = phys_pt | 3;
  }

  register_interrupt_handler(14, page_fault_handler);

  // Note: apic/hpet map uses paging_map which correctly handles virtual
  // pointers now
  apic_map_hardware();
  hpet_map_hardware();

  switch_page_directory(kernel_directory);
  serial_log("PAGING: Higher-Half & Identity Enabled.");
}

void switch_page_directory(uint32_t *dir) {
  current_directory = dir;
  uint32_t phys = VIRT_TO_PHYS(dir);
  asm volatile("mov %0, %%cr3" ::"r"(phys));
  uint32_t cr0;
  asm volatile("mov %%cr0, %0" : "=r"(cr0));
  cr0 |= 0x80010000;
  asm volatile("mov %0, %%cr0" ::"r"(cr0));
}

// Get page table entry for a virtual address
uint32_t *paging_get_pte(uint32_t virt) {
  uint32_t pd_index = virt >> 22;
  uint32_t pt_index = (virt >> 12) & 0x03FF;

  // Get the directory to use
  uint32_t *dir = current_directory ? current_directory : kernel_directory;
  if (!dir)
    return 0;

  // Check if page table exists
  if (!(dir[pd_index] & 1))
    return 0;

  // Get page table
  uint32_t *pt = (uint32_t *)PHYS_TO_VIRT(dir[pd_index] & 0xFFFFF000);
  return &pt[pt_index];
}
