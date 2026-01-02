#ifndef ACPI_H
#define ACPI_H

#include "types.h"

typedef struct {
  char signature[4];
  uint32_t length;
  uint8_t revision;
  uint8_t checksum;
  char oem_id[6];
  char oem_table_id[8];
  uint32_t oem_revision;
  uint32_t creator_id;
  uint32_t creator_revision;
} __attribute__((packed)) acpi_sdt_header_t;

typedef struct {
  char signature[8];
  uint8_t checksum;
  char oem_id[6];
  uint8_t revision;
  uint32_t rsdt_address;
} __attribute__((packed)) acpi_rsdp_t;

typedef struct {
  acpi_sdt_header_t header;
  uint32_t entry[];
} __attribute__((packed)) acpi_rsdt_t;

typedef struct {
  acpi_sdt_header_t header;
  uint32_t lapic_addr;
  uint32_t flags;
  uint8_t entries[];
} __attribute__((packed)) acpi_madt_t;

typedef struct {
  uint8_t type;
  uint8_t length;
} __attribute__((packed)) acpi_madt_entry_t;

typedef struct {
  acpi_madt_entry_t header;
  uint8_t acpi_processor_id;
  uint8_t apic_id;
  uint32_t flags;
} __attribute__((packed)) acpi_madt_lapic_t;

typedef struct {
  acpi_madt_entry_t header;
  uint8_t io_apic_id;
  uint8_t reserved;
  uint32_t io_apic_addr;
  uint32_t global_system_interrupt_base;
} __attribute__((packed)) acpi_madt_io_apic_t;

typedef struct {
  acpi_madt_entry_t header;
  uint8_t bus_source;
  uint8_t irq_source;
  uint32_t global_system_interrupt;
  uint16_t flags;
} __attribute__((packed)) acpi_madt_iso_t;

typedef struct {
  acpi_sdt_header_t header;
  uint32_t event_timer_block_id;
  uint8_t address_space_id;
  uint8_t register_bit_width;
  uint8_t register_bit_offset;
  uint8_t reserved;
  uint64_t address;
  uint8_t hpet_number;
  uint16_t minimum_tick;
  uint8_t page_protection;
} __attribute__((packed)) acpi_hpet_t;

#ifdef __cplusplus
extern "C" {
#endif

void acpi_init();
void *acpi_find_table(const char *signature);

#ifdef __cplusplus
}
#endif

#endif
