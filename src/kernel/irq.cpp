#include "../include/irq.h"
#include "../drivers/serial.h"
#include "../drivers/vga.h"
#include "../include/idt.h"
#include "../include/io.h"
#include "../include/signal.h"
#include "../include/string.h"
#include "../include/types.h"


#include "../drivers/acpi.h"
#include "apic.h"

// ISRs defined in isr.c or interrupt.asm need to be referenced here
// but actually we used macros, so let's reference the isr32-47 symbols
extern "C" {
void irq_remap() {
  outb(0x20, 0x11);
  outb(0xA0, 0x11);
  outb(0x21, 0x20); // Master offset 32 (0x20)
  outb(0xA1, 0x28); // Slave offset 40 (0x28)
  outb(0x21, 0x04);
  outb(0xA1, 0x02);
  outb(0x21, 0x01);
  outb(0xA1, 0x01);
  outb(0x21, 0x0);
  outb(0xA1, 0x0);
}

#include "../drivers/acpi.h"
#include "apic.h"

void irq_install() {
  acpi_init();
  void *madt = acpi_find_table("APIC");

  if (madt) {
    serial_log("IRQ: Using APIC mode.");
    lapic_init();
    ioapic_init();
  } else {
    serial_log("IRQ: APIC not found, falling back to PIC.");
    irq_remap();
  }

  set_idt_gate(32, (uint32_t)isr32);
  set_idt_gate(33, (uint32_t)isr33);
  set_idt_gate(34, (uint32_t)isr34);
  set_idt_gate(35, (uint32_t)isr35);
  set_idt_gate(36, (uint32_t)isr36);
  set_idt_gate(37, (uint32_t)isr37);
  set_idt_gate(38, (uint32_t)isr38);
  set_idt_gate(39, (uint32_t)isr39);
  set_idt_gate(40, (uint32_t)isr40);
  set_idt_gate(41, (uint32_t)isr41);
  set_idt_gate(42, (uint32_t)isr42);
  set_idt_gate(43, (uint32_t)isr43);
  set_idt_gate(44, (uint32_t)isr44);
  set_idt_gate(45, (uint32_t)isr45);
  set_idt_gate(46, (uint32_t)isr46);
  set_idt_gate(47, (uint32_t)isr47);
}

// Use a single handler for dispatch
#include "apic.h"

void irq_set_mask(uint8_t irq, bool masked) {
  if (lapic_base) {
    ioapic_set_mask(irq, masked);
  }
}

void irq_handler(registers_t *regs) {
  // Send EOI (End of Interrupt)
  if (lapic_base) {
    lapic_eoi();
  } else {
    if (regs->int_no >= 40) {
      outb(0xA0, 0x20); // Slave
    }
    outb(0x20, 0x20); // Master
  }

  if (interrupt_handlers[regs->int_no] != 0) {
    isr_t handler = interrupt_handlers[regs->int_no];
    handler(regs);
  }

  // Handle signals on return to user mode
  if ((regs->cs & 3) == 3) {
    handle_signals(regs);
  }
}
} // extern "C"
