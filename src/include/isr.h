#ifndef ISR_H
#define ISR_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Registers struct passed from assembly
typedef struct {
  uint32_t ds;                                     // Data segment selector
  uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax; // Pushed by pusha
  uint32_t int_no, err_code; // Interrupt number and error code
  uint32_t eip, cs, eflags, useresp,
      ss; // Pushed by the processor automatically
} registers_t;

void isr_handler(registers_t *regs);

// Assembly interrupt handler stubs
extern void isr0();
extern void isr1();
extern void isr2();
extern void isr3();
extern void isr4();
extern void isr5();
extern void isr6();
extern void isr7();
extern void isr8();
extern void isr9();
extern void isr10();
extern void isr11();
extern void isr12();
extern void isr13();
extern void isr14();
extern void isr15();
extern void isr16();
extern void isr17();
extern void isr18();
extern void isr19();
extern void isr20();
extern void isr21();
extern void isr22();
extern void isr23();
extern void isr24();
extern void isr25();
extern void isr26();
extern void isr27();
extern void isr28();
extern void isr29();
extern void isr30();
extern void isr31();
extern void isr128();
extern void isr255();

void isr_install();
typedef void (*isr_t)(registers_t *);
extern isr_t interrupt_handlers[256];
void register_interrupt_handler(uint8_t n, isr_t handler);

#ifdef __cplusplus
}
#endif

#endif
