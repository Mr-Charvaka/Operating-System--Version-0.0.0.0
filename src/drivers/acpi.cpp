#include "acpi.h"
#include "../include/string.h"
#include "../kernel/memory.h"
#include "../kernel/paging.h"
#include "serial.h"

extern "C" {

static acpi_rsdp_t *rsdp = 0;
static acpi_rsdt_t *rsdt = 0;

static acpi_rsdp_t *find_rsdp() {
  // Scan 0x000E0000 to 0x000FFFFF
  for (uint32_t p = 0xE0000; p < 0x100000; p += 16) {
    if (memcmp((void *)p, "RSD PTR ", 8) == 0) {
      return (acpi_rsdp_t *)p;
    }
  }
  return 0;
}

void acpi_init() {
  rsdp = find_rsdp();
  if (!rsdp) {
    serial_log("ACPI: RSDP not found!");
    return;
  }
  serial_log_hex("ACPI: Found RSDP at ", (uint32_t)rsdp);

  rsdt = (acpi_rsdt_t *)rsdp->rsdt_address;
  serial_log_hex("ACPI: RSDT at ", (uint32_t)rsdt);

  // Map RSDT if paging is enabled (it should be soon after boot)
  // For now, assume physical = virtual if we are in early boot
}

void *acpi_find_table(const char *signature) {
  if (!rsdt)
    return 0;

  int entries = (rsdt->header.length - sizeof(acpi_sdt_header_t)) / 4;
  for (int i = 0; i < entries; i++) {
    acpi_sdt_header_t *header = (acpi_sdt_header_t *)rsdt->entry[i];
    if (memcmp(header->signature, signature, 4) == 0) {
      serial_log("ACPI: Found table ");
      serial_log(signature);
      serial_log_hex(" at ", (uint32_t)header);
      return header;
    }
  }
  return 0;
}

} // extern "C"
