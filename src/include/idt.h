#ifndef IDT_H
#define IDT_H

#include "types.h"

// Segment selectors
#define KERNEL_CS 0x08

// IDT Entry
typedef struct {
  uint16_t low_offset;  // Lower 16 bits of handler address
  uint16_t sel;         // Kernel segment selector
  uint8_t always0;      // This must always be 0
  uint8_t flags;        // Type and attributes
  uint16_t high_offset; // Higher 16 bits of handler address
} __attribute__((packed)) idt_gate_t;

// A pointer to the array of interrupt handlers.
// Assembly instruction 'lidt' will read it
typedef struct {
  uint16_t limit;
  uint32_t base;
} __attribute__((packed)) idt_register_t;

// Functions
void set_idt_gate(int n, uint32_t handler);
void set_idt_gate_user(int n, uint32_t handler);
void set_idt();

#endif
