#ifndef HPET_H
#define HPET_H

#include "../include/types.h"

#define HPET_CAPABILITIES 0x00
#define HPET_CONFIGURATION 0x10
#define HPET_INTERRUPT_STATUS 0x20
#define HPET_MAIN_COUNTER 0xF0

#define HPET_TIMER_CONFIG(n) (0x100 + 0x20 * n)
#define HPET_TIMER_COMPARATOR(n) (0x108 + 0x20 * n)

#ifdef __cplusplus
extern "C" {
#endif

void hpet_init();
uint64_t hpet_read_counter();
void hpet_map_hardware();

#ifdef __cplusplus
}
#endif

#endif
