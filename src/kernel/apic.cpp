#include "apic.h"
#include "../drivers/acpi.h"
#include "../drivers/serial.h"
#include "../include/io.h"
#include "../include/string.h"
#include "../include/types.h"
#include "paging.h"

extern "C" {

uint32_t lapic_base = 0;
uint32_t ioapic_base = 0;

static void lapic_write(uint32_t reg, uint32_t value) {
  *(volatile uint32_t *)(lapic_base + reg) = value;
}

static uint32_t lapic_read(uint32_t reg) {
  return *(volatile uint32_t *)(lapic_base + reg);
}

uint32_t cpu_lapic_id = 0;

void lapic_init() {
  acpi_madt_t *madt = (acpi_madt_t *)acpi_find_table("APIC");
  if (!madt) {
    serial_log("LAPIC: MADT not found!");
    return;
  }

  lapic_base = madt->lapic_addr;
  serial_log_hex("LAPIC: Base at ", lapic_base);

  // Read Local APIC ID (Register 0x20, bits 24-31)
  cpu_lapic_id = (lapic_read(LAPIC_ID) >> 24) & 0xFF;
  serial_log_hex("LAPIC: CPU APIC ID: ", cpu_lapic_id);

  // Spurious Interrupt Vector - Set bit 8 to enable APIC
  lapic_write(LAPIC_SPURIOUS, lapic_read(LAPIC_SPURIOUS) | 0x1FF);

  serial_log("LAPIC: Initialized and Enabled.");
}

void lapic_eoi() { lapic_write(LAPIC_EOI, 0); }

static void ioapic_write(uint32_t reg, uint32_t value) {
  *(volatile uint32_t *)(ioapic_base) = reg;
  *(volatile uint32_t *)(ioapic_base + 0x10) = value;
}

static uint32_t ioapic_read(uint32_t reg) {
  *(volatile uint32_t *)(ioapic_base) = reg;
  return *(volatile uint32_t *)(ioapic_base + 0x10);
}

static acpi_madt_iso_t *isos[16] = {0};

void ioapic_init() {
  acpi_madt_t *madt = (acpi_madt_t *)acpi_find_table("APIC");
  if (!madt)
    return;

  uint8_t *ptr = madt->entries;
  uint8_t *end = (uint8_t *)madt + madt->header.length;

  while (ptr < end) {
    acpi_madt_entry_t *entry = (acpi_madt_entry_t *)ptr;
    if (entry->type == 1) { // IO-APIC
      acpi_madt_io_apic_t *io = (acpi_madt_io_apic_t *)ptr;
      ioapic_base = io->io_apic_addr;
      serial_log_hex("IO-APIC: Base at ", ioapic_base);
    } else if (entry->type == 2) { // ISO
      acpi_madt_iso_t *iso = (acpi_madt_iso_t *)ptr;
      if (iso->irq_source < 16) {
        isos[iso->irq_source] = iso;
        serial_log_hex("IO-APIC: Found ISO for IRQ ", iso->irq_source);
      }
    }
    ptr += entry->length;
  }

  // Disable legacy PIC
  outb(0x21, 0xFF);
  outb(0xA1, 0xFF);
  serial_log("PIC: Disabled.");

  // Default redirection: Map 16 legacy IRQs to vectors 32-47
  // We want to ensure they are UNMASKED (bit 16 is 0)
  // Delivery mode: Fixed (000), Destination mode: Physical (0)
  for (int i = 0; i < 16; i++) {
    uint32_t vector = 32 + i;

    // Low 32 bits of entry:
    // bit 0-7: vector
    // bit 8-10: delivery mode (000 = Fixed)
    // bit 11: destination mode (0 = Physical)
    // bit 12: delivery status (RO)
    // bit 13: polarity (0 = Active High, 1 = Active Low)
    // bit 14: remote IRR (RO)
    // bit 15: trigger mode (0 = Edge, 1 = Level)
    // bit 16: mask (0 = Unmasked, 1 = Masked)
    uint32_t low_part =
        vector | 0x10000; // Masked by default, Edge, Active High, Fixed

    uint32_t target_gsi = i;
    if (isos[i]) {
      target_gsi = isos[i]->global_system_interrupt;
      // Many ISOs (like IRQ0 to GSI 2) are active high/edge
      // but let's be explicit if flags indicate otherwise
      // iso->flags: bit 0-1 (Polarity), bit 2-3 (Trigger Mode)
      // Polarity: 1=AH, 3=AL. Trigger: 1=Edge, 3=Level.
      if ((isos[i]->flags & 3) == 3)
        low_part |= (1 << 13); // Active Low
      if (((isos[i]->flags >> 2) & 3) == 3)
        low_part |= (1 << 15); // Level Triggered

      serial_log_hex("IO-APIC: Routing IRQ ", i);
      serial_log_hex("IO-APIC: To GSI ", target_gsi);
    }

    uint64_t entry = low_part;
    entry |= ((uint64_t)cpu_lapic_id << 56);

    ioapic_set_irq(target_gsi, entry);
  }
  serial_log("IO-APIC: Legacy IRQs 0-15 unmasked and routed.");
}

void ioapic_set_irq(uint8_t irq, uint64_t vector_data) {
  uint32_t low = (uint32_t)vector_data;
  uint32_t high = (uint32_t)(vector_data >> 32);

  ioapic_write(IOAPIC_REDTBL + irq * 2, low);
  ioapic_write(IOAPIC_REDTBL + irq * 2 + 1, high);
}

void ioapic_set_mask(uint8_t irq, bool masked) {
  if (irq >= 16)
    return;
  uint32_t gsi = irq;
  if (isos[irq]) {
    gsi = isos[irq]->global_system_interrupt;
  }

  uint32_t low = ioapic_read(IOAPIC_REDTBL + gsi * 2);
  if (masked)
    low |= (1 << 16);
  else
    low &= ~(1 << 16);
  ioapic_write(IOAPIC_REDTBL + gsi * 2, low);
}

void apic_map_hardware() {
  // Standard APIC addresses (mapped before ACPI discovery)
  // LAPIC is at 0xFEE00000, IO-APIC is at 0xFEC00000
  paging_map(0xFEE00000, 0xFEE00000, 3); // LAPIC
  paging_map(0xFEC00000, 0xFEC00000, 3); // IO-APIC
}

} // extern "C"
