#ifndef APIC_H
#define APIC_H

#include "../include/isr.h"
#include "../include/types.h"

// LAPIC Registers (Offset from base)
#define LAPIC_ID 0x20
#define LAPIC_VER 0x30
#define LAPIC_TPR 0x80
#define LAPIC_EOI 0x0B0
#define LAPIC_LDR 0x0D0
#define LAPIC_DFR 0x0E0
#define LAPIC_SPURIOUS 0x0F0
#define LAPIC_ESR 0x280
#define LAPIC_ICR_LOW 0x300
#define LAPIC_ICR_HIGH 0x310
#define LAPIC_LVT_TIMER 0x320
#define LAPIC_LVT_THERMAL 0x330
#define LAPIC_LVT_PERF 0x340
#define LAPIC_LVT_LINT0 0x350
#define LAPIC_LVT_LINT1 0x360
#define LAPIC_LVT_ERROR 0x370
#define LAPIC_TIC 0x380
#define LAPIC_TCC 0x390
#define LAPIC_TDCR 0x3E0

// IO-APIC Registers (Offsets for IOREGSEL)
#define IOAPIC_ID 0x00
#define IOAPIC_VER 0x01
#define IOAPIC_ARB 0x02
#define IOAPIC_REDTBL 0x10

#ifdef __cplusplus
extern "C" {
#endif

extern uint32_t lapic_base;
extern uint32_t ioapic_base;

void lapic_init();
void lapic_eoi();
void ioapic_init();
void ioapic_set_irq(uint8_t irq, uint64_t vector_data);
void apic_map_hardware();

#ifdef __cplusplus
}
#endif

#endif
