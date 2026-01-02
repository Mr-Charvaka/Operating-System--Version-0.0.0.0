#include "rtc.h"
#include "../include/io.h"

#define CMOS_ADDRESS 0x70
#define CMOS_DATA 0x71

uint8_t get_rtc_register(int reg) {
  outb(CMOS_ADDRESS, reg);
  return inb(CMOS_DATA);
}

// BCD to Binary
uint8_t from_bcd(uint8_t val) { return ((val / 16) * 10) + (val & 0xf); }

void rtc_read(rtc_time_t *time) {
  // Wait for "update in progress" flag to clear (Register A, bit 7)
  // Simplified: just read. In a real OS we should check this.

  // Read registers
  time->second = from_bcd(get_rtc_register(0x00));
  time->minute = from_bcd(get_rtc_register(0x02));
  time->hour = from_bcd(get_rtc_register(0x04));
  time->day = from_bcd(get_rtc_register(0x07));
  time->month = from_bcd(get_rtc_register(0x08));
  time->year = from_bcd(get_rtc_register(0x09));

  // Status Register B (0x0B). Bit 2 = DM (Binary Mode), Bit 1 = 24H Mode.
  // We assumed BCD above. Most BIOSes use BCD.
}
