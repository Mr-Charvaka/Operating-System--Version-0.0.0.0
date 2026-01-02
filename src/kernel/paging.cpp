#include "paging.h"
#include "../drivers/hpet.h"
#include "../drivers/serial.h"
#include "../include/signal.h"
#include "../include/string.h"
#include "../kernel/memory.h"
#include "apic.h"
#include "pmm.h"
#include "process.h"


// Page Directory: 1024 entries
// Each entry points to a Page Table
uint32_t *kernel_directory = 0;
uint32_t *current_directory = 0;

// Need a place to hold the actual Page Tables.
// For now, we only map the first 4MB (one page table).
// We might need more later.

void page_fault_handler(registers_t *regs) {
  // The faulting address is stored in the CR2 register.
  uint32_t faulting_address;
  asm volatile("mov %%cr2, %0" : "=r"(faulting_address));

  serial_log("PAGE FAULT! Address:");
  serial_log_hex("", faulting_address);
  serial_log_hex("  EIP: ", regs->eip);

  // Check error code
  int present = !(regs->err_code & 0x1); // Page not present
  int rw = regs->err_code & 0x2;         // Write operation?
  int us = regs->err_code & 0x4;         // Processor was in user-mode?
  int reserved =
      regs->err_code & 0x8; // Overwritten CPU-reserved bits of page entry?

  if (present)
    serial_log("  - Present");
  if (rw)
    serial_log("  - Write");
  if (us)
    serial_log("  - User");

  if (us) {
    serial_log("PAGE FAULT: Sending SIGSEGV to user process.");
    sys_kill(current_process->id, SIGSEGV);
    return;
  }

  serial_log("KERNEL PANIC: Page Fault");
  for (;;)
    ;
}

// Map a specific physical address to a virtual address
void paging_map(uint32_t phys, uint32_t virt, uint32_t flags) {
  uint32_t pd_index = virt >> 22;
  uint32_t pt_index = (virt >> 12) & 0x03FF;

  // Check if PDE exists
  if (!(kernel_directory[pd_index] & 1)) {
    // Allocate new PT via PMM
    uint32_t *new_pt = (uint32_t *)pmm_alloc_block();
    memset(new_pt, 0, 4096);
    kernel_directory[pd_index] = ((uint32_t)new_pt) | 7; // User, RW, Present
  }

  uint32_t *pt = (uint32_t *)(kernel_directory[pd_index] & 0xFFFFF000);
  pt[pt_index] = (phys & 0xFFFFF000) | flags;
}

void init_paging() {
  serial_log("PAGING: Initializing...");

  // 1. Allocate Page Directory via PMM
  kernel_directory = (uint32_t *)pmm_alloc_block();
  memset(kernel_directory, 0, 4096);

  // Identity map the first 512MB (128 page tables)
  // This covers the Kernel (0-16MB), Heap (16MB-272MB), and Stack (480MB)
  for (int j = 0; j < 128; j++) {
    uint32_t *pt = (uint32_t *)pmm_alloc_block();
    memset(pt, 0, 4096);
    for (int i = 0; i < 1024; i++) {
      pt[i] = (j * 1024 * 4096 + i * 4096) | 3;
    }
    kernel_directory[j] = (uint32_t)pt | 3;
  }

  // 5. Register Handler
  register_interrupt_handler(14, page_fault_handler);

  // 6. Map Hardware before switching (LAPIC, IO-APIC, HPET)
  apic_map_hardware();
  hpet_map_hardware();

  // 7. Enable Paging
  switch_page_directory(kernel_directory);

  serial_log("PAGING: Enabled.");
}

void switch_page_directory(uint32_t *dir) {
  current_directory = dir;
  asm volatile("mov %0, %%cr3" ::"r"(dir));
  uint32_t cr0;
  asm volatile("mov %%cr0, %0" : "=r"(cr0));
  cr0 |= 0x80000000; // Enable Paging
  asm volatile("mov %0, %%cr0" ::"r"(cr0));
}
