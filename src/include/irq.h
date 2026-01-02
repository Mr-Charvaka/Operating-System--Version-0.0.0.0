#ifndef IRQ_H
#define IRQ_H

#include "../include/isr.h"

// void register_interrupt_handler(uint8_t n, isr_t handler); // Moved to isr.h
#ifdef __cplusplus
extern "C" {
#endif

void irq_install();
void irq_handler(registers_t *regs);

extern void isr32();
extern void isr33();
extern void isr34();
extern void isr35();
extern void isr36();
extern void isr37();
extern void isr38();
extern void isr39();
extern void isr40();
extern void isr41();
extern void isr42();
extern void isr43();
extern void isr44();
extern void isr45();
extern void isr46();
extern void isr47();

#ifdef __cplusplus
}
#endif

#endif
