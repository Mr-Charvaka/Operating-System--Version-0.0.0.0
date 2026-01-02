#include "../drivers/mouse.h"
#include "../drivers/vga.h"
#include "../include/io.h"
#include "../include/irq.h"
#include "../include/types.h"
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
  if (!(status & 0x20)) {
    // Not mouse
    return;
  }

  mouse_byte[mouse_cycle++] = inb(0x60);

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
  uint8_t _status;

  // Enable the auxiliary mouse device
  mouse_wait(1);
  outb(0x64, 0xA8);

  // Enable the interrupts
  mouse_wait(1);
  outb(0x64, 0x20); // Command: Read Controller Command Byte
  mouse_wait(0);
  _status = (inb(0x60) | 2); // Set bit 1 (Enable IRQ12)
  mouse_wait(1);
  outb(0x64, 0x60); // Command: Write Controller Command Byte
  mouse_wait(1);
  outb(0x60, _status);

  // Tell the mouse to use default settings
  mouse_write(0xF6);
  mouse_read(); // Acknowledge

  // Enable the mouse
  mouse_write(0xF4);
  mouse_read(); // Acknowledge

  // Setup the ISR handler
  register_interrupt_handler(44, mouse_callback); // IRQ12 = IDT 44

  serial_log("MOUSE: Initialized and enabled IRQ12");
}
