#include "hpet.h"
#include "../kernel/paging.h"
#include "acpi.h"
#include "serial.h"

extern "C" {

static uint32_t hpet_base = 0;

void hpet_init() {
  acpi_hpet_t *hpet = (acpi_hpet_t *)acpi_find_table("HPET");
  if (!hpet) {
    serial_log("HPET: Table not found!");
    return;
  }

  hpet_base = (uint32_t)hpet->address;
  serial_log_hex("HPET: Base at ", hpet_base);

  // Enable HPET (bit 0 of configuration register)
  *(volatile uint32_t *)(hpet_base + HPET_CONFIGURATION) |= 0x01;

  serial_log("HPET: Enabled.");
}

uint64_t hpet_read_counter() {
  if (!hpet_base)
    return 0;

  uint32_t low = *(volatile uint32_t *)(hpet_base + HPET_MAIN_COUNTER);
  uint32_t high = *(volatile uint32_t *)(hpet_base + HPET_MAIN_COUNTER + 4);

  return ((uint64_t)high << 32) | low;
}

void hpet_map_hardware() {
  if (hpet_base)
    paging_map(hpet_base, hpet_base, 3);
}

} // extern "C"
