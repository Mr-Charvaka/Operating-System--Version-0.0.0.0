#include "apic.h"
#include "../drivers/acpi.h"
#include "../drivers/serial.h"
#include "../include/io.h"
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
  for (int i = 0; i < 16; i++) {
    uint32_t vector = 32 + i;
    // Deliver to cpu_lapic_id
    uint64_t entry = vector;
    entry |= ((uint64_t)cpu_lapic_id << 56);

    uint32_t target_irq = i;
    if (isos[i]) {
      target_irq = isos[i]->global_system_interrupt;
      // Handle flags (polarity/trigger mode) if needed
    }

    ioapic_set_irq(target_irq, entry);
  }
  serial_log("IO-APIC: Default IRQ routing set.");
}

void ioapic_set_irq(uint8_t irq, uint64_t vector_data) {
  uint32_t low = (uint32_t)vector_data;
  uint32_t high = (uint32_t)(vector_data >> 32);

  ioapic_write(IOAPIC_REDTBL + irq * 2, low);
  ioapic_write(IOAPIC_REDTBL + irq * 2 + 1, high);
}

void apic_map_hardware() {
  if (lapic_base)
    paging_map(lapic_base, lapic_base, 3);
  if (ioapic_base)
    paging_map(ioapic_base, ioapic_base, 3);
}

} // extern "C"
