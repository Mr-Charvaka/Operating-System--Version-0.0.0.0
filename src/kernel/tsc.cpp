#include "tsc.h"
#include "../drivers/hpet.h"
#include "../drivers/serial.h"

static uint64_t tsc_freq = 0;

uint64_t rdtsc() {
  uint32_t low, high;
  asm volatile("rdtsc" : "=a"(low), "=d"(high));
  return ((uint64_t)high << 32) | low;
}

void tsc_calibrate() {
  serial_log("TSC: Calibrating...");

  // Use HPET for calibration if available
  uint64_t h1 = hpet_read_counter();
  uint64_t t1 = rdtsc();

  // Busy wait for ~10ms (estimating by counter)
  // Actually, we should check HPET period/frequency
  // But for a simple estimate:
  for (int i = 0; i < 1000000; i++)
    asm volatile("" : : : "memory");

  uint64_t h2 = hpet_read_counter();
  uint64_t t2 = rdtsc();

  // Very rough estimate for now
  tsc_freq = (t2 - t1);
  serial_log_hex("TSC: Frequency estimate (ticks per sample): ", tsc_freq);
}

void nanosleep(uint64_t ns) {
  uint64_t start = rdtsc();
  // Use a simpler condition without division to avoid __udivdi3
  uint64_t ticks = ns; // Very crude placeholder
  while ((rdtsc() - start) < ticks)
    ;
}
