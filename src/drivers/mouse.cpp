#include "../include/io.h"
#include "../include/irq.h"
#include "../include/types.h"
#include "../kernel/apic.h"
#include "../kernel/gui.h"
#include "serial.h"

uint8_t mouse_cycle = 0;
int8_t mouse_byte[3];

void mouse_wait(uint8_t type) {
  uint32_t _time_out = 100000;
  if (type == 0) {
    while (_time_out--) { // Data
      if ((inb(0x64) & 1) == 1) {
        return;
      }
    }
    return;
  } else {
    while (_time_out--) { // Signal
      if ((inb(0x64) & 2) == 0) {
        return;
      }
    }
    return;
  }
}

void mouse_write(uint8_t a_write) {
  // Wait to be able to send a command
  mouse_wait(1);
  // Tell the mouse we are sending a command
  outb(0x64, 0xD4);
  // Wait for the final part
  mouse_wait(1);
  // Finally write
  outb(0x60, a_write);
}

uint8_t mouse_read() {
  // Get response from mouse
  mouse_wait(0);
  return inb(0x60);
}

void mouse_callback(registers_t *regs) {
  uint8_t status = inb(0x64);
  if (!(status & 0x21)) { // Check bit 0 (data ready) and bit 5 (mouse data)
    return;
  }

  uint8_t data = inb(0x60);
  if (!(status & 0x20)) {
    // Not mouse data, might be keyboard?
    // Usually status register bit 5 indicates mouse
    return;
  }

  mouse_byte[mouse_cycle++] = data;

  if (mouse_cycle == 3) {
    mouse_cycle = 0;

    // Packet ready
    uint8_t state = mouse_byte[0];
    int8_t x_rel = (int8_t)mouse_byte[1];
    int8_t y_rel = (int8_t)mouse_byte[2];

    // Update GUI
    update_mouse_position(x_rel, y_rel, state);
  }
}

void init_mouse() {
  serial_log("PS/2: Initializing controller...");

  // 1. Enable Keyboard & Mouse interfaces
  mouse_wait(1);
  outb(0x64, 0xAE); // Enable Keyboard
  mouse_wait(1);
  outb(0x64, 0xA8); // Enable Mouse (Auxiliary)

  // 2. Set Controller Command Byte
  mouse_wait(1);
  outb(0x64, 0x20); // Read Command Byte
  mouse_wait(0);
  uint8_t status = inb(0x60);

  // Enable IRQ1 (bit 0), IRQ12 (bit 1), enable clock (bit 4, 5 clear)
  status |= 0x03;  // Enable interrupts
  status &= ~0x30; // Enable clocks (clear disable bits)

  mouse_wait(1);
  outb(0x64, 0x60); // Write Command Byte
  mouse_wait(1);
  outb(0x60, status);

  // 3. Initialize Mouse Device
  mouse_write(0xF6); // Set defaults
  mouse_read();      // ACK

  mouse_write(0xF4); // Enable data reporting
  mouse_read();      // ACK

  // 4. Register Handler
  register_interrupt_handler(44, mouse_callback); // IRQ12 = IDT 44
  ioapic_set_mask(12, false);

  serial_log("PS/2: Mouse/Keyboard interfaces enabled.");
}
