//===================================================================
// Kernel.cpp – core kernel entry point
//===================================================================

#include "../drivers/acpi.h"
#include "../drivers/bga.h"
#include "../drivers/fat16.h"
#include "../drivers/graphics.h"
#include "../drivers/hpet.h"
#include "../drivers/keyboard.h"
#include "../drivers/mouse.h"
#include "../drivers/pci.h"
#include "../drivers/rtc.h"
#include "../drivers/serial.h"
#include "../drivers/timer.h"
#include "../drivers/vga.h"

// From src/include
#include "idt.h"
#include "io.h"
#include "irq.h"
#include "isr.h"
#include "string.h"
#include "types.h"
#include "vfs.h"

// From src/kernel
#include "gdt.h"
#include "gui.h"
#include "heap.h"
#include "memory.h"
#include "paging.h"
#include "process.h"
#include "socket.h"
#include "syscall.h"
#include "tsc.h"
#include "tty.h"

// Memory & Paging headers must be early
#include "memory.h"
#include "paging.h"
#include "pmm.h"

extern "C" void cpp_kernel_entry();

extern u32 _kernel_end;

//-------------------------------------------------------------------
// Helper: enable the x87 FPU (prevents #NM – Coprocessor Segment Overrun)
//-------------------------------------------------------------------
static void enable_fpu(void) {
  // Clear TS flag (task‑switched)
  asm volatile("clts");
  // Clear EM and MP bits in CR0
  uint32_t cr0;
  asm volatile("mov %%cr0, %0" : "=r"(cr0));
  cr0 &= ~0x2;      // EM  (bit 1)
  cr0 &= ~0x200000; // MP  (bit 21)
  asm volatile("mov %0, %%cr0" ::"r"(cr0));
  // Initialise the FPU
  asm volatile("fninit");
}

//-------------------------------------------------------------------
// Double Fault handler
//-------------------------------------------------------------------
void isr8_handler(registers_t *regs) {
  (void)regs;
  serial_log("FATAL: DOUBLE FAULT!");
  // Check for stack overflow or recursion
  for (;;)
    ;
}

//-------------------------------------------------------------------
// FPU handler
//-------------------------------------------------------------------
void isr9_handler(registers_t *regs) {
  (void)regs;
  serial_log("EXCEPTION: #NM (FPU not available) – enabling now");
  enable_fpu();
}

//-------------------------------------------------------------------
// Test thread (unchanged from earlier)
//-------------------------------------------------------------------
void test_thread() {
  u32 cs;
  asm volatile("mov %%cs, %0" : "=r"(cs));
  serial_log_hex("THREAD: Before CS: ", cs);

  enter_user_mode();

  asm volatile("mov %%cs, %0" : "=r"(cs));
  serial_log_hex("THREAD: After CS: ", cs);

  serial_log("THREAD: Triggering Syscall 0 (Print)...");
  const char *msg = "Hello from Ring 3 via Syscall!";
  asm volatile("mov $0, %%eax; mov %0, %%ebx; int $0x80" ::"r"(msg)
               : "eax", "ebx");
  serial_log("THREAD: Syscall returned.");

  while (1) {
    for (int i = 0; i < 1000000; i++)
      ;
  }
}

//===================================================================
// Kernel entry point
//===================================================================
int main() {
  init_serial();
  serial_log("KERNEL: Booting Custom OS...");

  // --------------------------------------------------------------
  // Basic CPU/interrupt setup
  // --------------------------------------------------------------
  init_gdt();
  isr_install(); // installs ISRs 0-31
  irq_install(); // installs IRQs 32-47, remaps PIC/APIC

  // Register critical handlers
  register_interrupt_handler(8, (isr_t)isr8_handler);
  register_interrupt_handler(9, (isr_t)isr9_handler);

  init_syscalls();
  serial_log("KERNEL: Interrupts & Syscalls & GDT Initialized.");

  // Enable interrupts for the rest of boot
  asm volatile("sti");

  // Drivers
  init_keyboard();
  init_mouse();
  hpet_init();
  tsc_calibrate();
  serial_log("KERNEL: Drivers & Timers Initialized.");

  // --------------------------------------------------------------
  enable_fpu();

  // ... (previous includes)

  // --------------------------------------------------------------
  // Early placement heap (2 MiB) – needed for kmalloc before paging
  // --------------------------------------------------------------
  init_memory(0x200000); // 2 MiB placement heap
  kmalloc_align_page();

  // --------------------------------------------------------------
  // PMM Initialization
  // --------------------------------------------------------------
  // We place the bitmap at 3MB to be safe (after placement heap usage)
  // Total Memory: 512MB (as set in QEMU).
  uint32_t mem_size = 512 * 1024 * 1024;
  uint32_t *bitmap_addr = (uint32_t *)0x00300000;
  pmm_init(mem_size, bitmap_addr);

  // Initially, ALL memory is FREE after pmm_init.
  // Now mark specific regions as USED:

  // 1. Low Memory (0-1MB): BIOS, IVT, real mode stuff
  pmm_mark_region_used(0x0, 0x100000);

  // 2. Kernel Code/Data (1MB-2MB)
  pmm_mark_region_used(0x100000, 0x100000);

  // 3. Placement Heap (2MB-3MB)
  pmm_mark_region_used(0x200000, 0x100000);

  // 4. PMM Bitmap (3MB + 16KB)
  // Size = 512MB / 4KB / 8 = 16KB
  pmm_mark_region_used((uint32_t)bitmap_addr, 16384);

  // 5. Kernel Heap (16MB-272MB = 256MB)
  // This is a LARGE region owned by kmalloc
  pmm_mark_region_used(0x01000000, 0x10000000);

  // Memory layout summary:
  // 0x00000000 - 0x00100000 (1MB): Low memory (USED)
  // 0x00100000 - 0x00200000 (1MB): Kernel (USED)
  // 0x00200000 - 0x00300000 (1MB): Placement heap (USED)
  // 0x00300000 - 0x00304000 (16KB): PMM Bitmap (USED)
  // 0x00304000 - 0x01000000 (~13MB): FREE for PMM
  // 0x01000000 - 0x11000000 (256MB): Kernel Heap (USED)
  // 0x11000000 - 0x20000000 (240MB): FREE for PMM (user processes)

  pmm_print_stats(); // Show initial memory state

  // --------------------------------------------------------------
  // Verify C++ runtime (calls constructors, etc.)
  // --------------------------------------------------------------
  serial_log("KERNEL: Testing C++ Runtime...");
  cpp_kernel_entry();

  // --------------------------------------------------------------
  // Enable paging (identity‑map 0‑256 MiB)
  // --------------------------------------------------------------
  init_paging();

  // --------------------------------------------------------------
  // Heap Init
  // --------------------------------------------------------------
  serial_log("KERNEL: Initializing Heap (16MB to 272MB)...");
  init_kheap(0x01000000, 0x11000000, 0x11000000);
  set_heap_status(1); // Crucial!

// Enable Slab
#include "slab.h"
  slab_init();
  extern int slab_is_initialized;
  slab_is_initialized = 1;

  serial_log("KERNEL: Heap enabled.");

  // --------------------------------------------------------------
  // Initialise the FAT16 filesystem (required for later work)
  // --------------------------------------------------------------
  fat16_init();
  vfs_root = fat16_vfs_init();
  vfs_dev = devfs_init();
  fat16_list_root();
  socket_init();
  tty_init();

  // --------------------------------------------------------------
  // BGA detection – we keep the detection but skip all graphics usage
  // --------------------------------------------------------------
  serial_log("KERNEL: Scanning PCI for BGA...");
  u32 fb_addr = pci_get_bga_bar0();
  serial_log_hex("KERNEL: BGA LFB Address: ", fb_addr);
  // --------------------------------------------------------------
  // Full GUI and Multitasking System
  // --------------------------------------------------------------
  if (fb_addr != 0) {
    // 5. Init Graphics
    serial_log("KERNEL: Setting Mode 1024x768x32...");
    bga_set_video_mode(1024, 768, 32);

    // Map VRAM
    serial_log("KERNEL: Mapping VRAM...");
    // Map enough for 1024x768x32 (~3MB)
    for (int i = 0; i < 1024; i++) {
      paging_map(fb_addr + (i * 4096), fb_addr + (i * 4096), 3);
    }

    // Initialize graphics (allocates backbuffer)
    init_graphics(fb_addr);

    serial_log("KERNEL: Starting GUI...");
    gui_init();
    serial_log("KERNEL: GUI Init complete.");

    // 7. Enable Multitasking & Schedule User Process
    serial_log("KERNEL: Enabling Multitasking...");
    init_multitasking();
    serial_log("KERNEL: Multitasking Enabled.");

    create_user_process("INIT.ELF");
    serial_log("KERNEL: User Process Created.");

    // create_kernel_thread(test_thread); // Disabled - test_thread uses broken
    // enter_user_mode() serial_log("KERNEL: Kernel Thread Created.");

    serial_log("KERNEL: Enabling Timer...");
    init_timer(50); // Restore to 50Hz
    serial_log("KERNEL: Timer Enabled.");
  } else {
    serial_log("KERNEL: FATAL - BGA Not Found!");
  }

  // --------------------------------------------------------------
  // Kernel is now stable – hang forever
  // --------------------------------------------------------------
  while (1) {
  }
  return 0; // never reached
}