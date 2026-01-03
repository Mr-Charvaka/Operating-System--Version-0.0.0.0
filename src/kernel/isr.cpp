#include "../include/isr.h"
#include "../drivers/serial.h"
#include "../drivers/vga.h"
#include "../include/idt.h"
#include "../include/signal.h"
#include "../include/string.h"

extern "C" {
isr_t interrupt_handlers[256];
}

void spurious_handler(registers_t *regs) {
  // Spurious interrupts don't need EOI or handling
}

extern "C" void register_interrupt_handler(uint8_t n, isr_t handler) {
  interrupt_handlers[n] = handler;
}

extern "C" void isr_install() {
  set_idt_gate(0, (uint32_t)isr0);
  set_idt_gate(1, (uint32_t)isr1);
  set_idt_gate(2, (uint32_t)isr2);
  set_idt_gate(3, (uint32_t)isr3);
  set_idt_gate(4, (uint32_t)isr4);
  set_idt_gate(5, (uint32_t)isr5);
  set_idt_gate(6, (uint32_t)isr6);
  set_idt_gate(7, (uint32_t)isr7);
  set_idt_gate(8, (uint32_t)isr8);
  set_idt_gate(9, (uint32_t)isr9);
  set_idt_gate(10, (uint32_t)isr10);
  set_idt_gate(11, (uint32_t)isr11);
  set_idt_gate(12, (uint32_t)isr12);
  set_idt_gate(13, (uint32_t)isr13);
  set_idt_gate(14, (uint32_t)isr14);
  set_idt_gate(15, (uint32_t)isr15);
  set_idt_gate(16, (uint32_t)isr16);
  set_idt_gate(17, (uint32_t)isr17);
  set_idt_gate(18, (uint32_t)isr18);
  set_idt_gate(19, (uint32_t)isr19);
  set_idt_gate(20, (uint32_t)isr20);
  set_idt_gate(21, (uint32_t)isr21);
  set_idt_gate(22, (uint32_t)isr22);
  set_idt_gate(23, (uint32_t)isr23);
  set_idt_gate(24, (uint32_t)isr24);
  set_idt_gate(25, (uint32_t)isr25);
  set_idt_gate(26, (uint32_t)isr26);
  set_idt_gate(27, (uint32_t)isr27);
  set_idt_gate(28, (uint32_t)isr28);
  set_idt_gate(29, (uint32_t)isr29);
  set_idt_gate(30, (uint32_t)isr30);
  set_idt_gate(31, (uint32_t)isr31);

  set_idt_gate_user(128, (uint32_t)isr128); // Syscall

  set_idt_gate(255, (uint32_t)isr255); // Spurious APIC
  register_interrupt_handler(255, (isr_t)spurious_handler);

  set_idt(); // Load IDT pointer

  extern idt_register_t idt_reg;
  asm volatile("lidt (%0)" : : "r"(&idt_reg));
}

const char *exception_messages[] = {"Division By Zero",
                                    "Debug",
                                    "Non Maskable Interrupt",
                                    "Breakpoint",
                                    "Into Detected Overflow",
                                    "Out of Bounds",
                                    "Invalid Opcode",
                                    "No Coprocessor",

                                    "Double Fault",
                                    "Coprocessor Segment Overrun",
                                    "Bad TSS",
                                    "Segment Not Present",
                                    "Stack Fault",
                                    "General Protection Fault",
                                    "Page Fault",
                                    "Unknown Interrupt",

                                    "Coprocessor Fault",
                                    "Alignment Check",
                                    "Machine Check",
                                    "Reserved",
                                    "Reserved",
                                    "Reserved",
                                    "Reserved",
                                    "Reserved",

                                    "Reserved",
                                    "Reserved",
                                    "Reserved",
                                    "Reserved",
                                    "Reserved",
                                    "Reserved",
                                    "Reserved",
                                    "Reserved"};

extern "C" void isr_handler(registers_t *regs) {
  if (interrupt_handlers[regs->int_no] != 0) {
    isr_t handler = interrupt_handlers[regs->int_no];
    handler(regs);
    return;
  }

  serial_log("EXCEPTION: Unhandled Interrupt!");
  serial_log_hex("  ID: ", regs->int_no);
  serial_log_hex("  EIP: ", regs->eip);
  serial_log_hex("  CS: ", regs->cs);
  serial_log_hex("  EF: ", regs->eflags);

  if (regs->int_no < 32) {
    serial_log(exception_messages[regs->int_no]);
    if (regs->int_no == 14) {
      uint32_t cr2;
      asm volatile("mov %%cr2, %0" : "=r"(cr2));
      serial_log_hex("  CR2: ", cr2);
    }
  } else {
    serial_log("Unknown Exception/Interrupt");
  }

  // This is a hang for kernel panic/exception
  // BUT what if it's a user exception?
  // If we implement signals, we might want to kill the process instead of
  // hanging kernel. handle_signals will check pending bits. BUT the exception
  // MUST set the pending bit first? Paging fault handler sets it. For other
  // exceptions, we might want to map them to signals. For now, let's just hang
  // if unhandled kernel exception.

  // If user mode, try to handle signals (maybe we got a KILL)
  if ((regs->cs & 3) == 3) {
    handle_signals(regs);
    // If handle_signals returns, it means we handled it or ignored it.
    // But if we had an exception, we shouldn't just retry instruction execution
    // loop indefinitely? Unless the signal handler fixes it. For now, if it was
    // an exception (0-31), and we are user, we probably want to kill process if
    // signal didn't. But let's stick to the plan: call handle_signals.
    return;
  }

  // Hang if unhandled exception
  for (;;)
    ;
}
