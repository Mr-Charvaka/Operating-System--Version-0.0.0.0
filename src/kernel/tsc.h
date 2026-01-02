#ifndef TSC_H
#define TSC_H

#include "../include/types.h"

uint64_t rdtsc();
void tsc_calibrate();
void nanosleep(uint64_t ns);

#endif
