#include "../include/idt.h"
#include "../include/string.h"

idt_gate_t idt[256];
idt_register_t idt_reg;

void set_idt_gate(int n, uint32_t handler) {
  idt[n].low_offset = (uint16_t)((handler) & 0xFFFF);
  idt[n].sel = KERNEL_CS;
  idt[n].always0 = 0;
  // flags: 1(Present) DPL(00 or 11) 0(Storage Segment) 1110(32-bit Interrupt
  // Gate)
  idt[n].flags = 0x8E;
  idt[n].high_offset = (uint16_t)(((handler) >> 16) & 0xFFFF);
}

void set_idt_gate_user(int n, uint32_t handler) {
  set_idt_gate(n, handler);
  idt[n].flags = 0xEE; // DPL 11 (Ring 3)
}

void set_idt() {
  idt_reg.base = (uint32_t)&idt;
  idt_reg.limit = 256 * sizeof(idt_gate_t) - 1;
  // Don't load it here, done in global install
  // __asm__ __volatile__("lidt (%0)" : : "r" (&idt_reg));
}
