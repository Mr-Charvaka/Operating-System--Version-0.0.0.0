#include "../drivers/vga.h"
#include "../include/io.h"
#include "../include/irq.h"
#include "../include/types.h"
#include "../kernel/gui.h"
#include "../kernel/process.h"
#include "../kernel/tty.h"
#include "serial.h"

// US Keyboard Layout (Normal)
char kbd_us[128] = {
    0,    27,   '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-',  '=',
    '\b', '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[',  ']',
    '\n', 0,    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0,    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,    '*',
    0,    ' ',  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,    0,
    0,    0,    0,   0,   0,   0,   0,   0,   0,   0,   '-', 0,   0,    0,
    0,    0,    0,   0,   0,   0,   0,   0,   0};

// US Keyboard Layout (Shifted)
char kbd_us_shifted[128] = {
    0,    27,   '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+',
    '\b', '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}',
    '\n', 0,    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0,    '|',  'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,   '*',
    0,    ' ',  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,    0,    0,   0,   0,   0,   0,   0,   0,   0,   '_', 0,   0,   0,
    0,    0,    0,   0,   0,   0,   0,   0,   0};

static int shift_pressed = 0;
static int ctrl_pressed = 0;
static int alt_pressed = 0;
static int caps_lock = 0;

static void keyboard_callback(registers_t *regs) {
  uint8_t scancode = inb(0x60);
  serial_log_hex("KEYBOARD: Scancode ", scancode);

  // Handle Modifier Keys (Press)
  if (scancode == 0x2A || scancode == 0x36) { // LShift, RShift
    shift_pressed = 1;
    return;
  }
  if (scancode == 0x1D) { // LCtrl
    ctrl_pressed = 1;
    return;
  }
  if (scancode == 0x38) { // LAlt
    alt_pressed = 1;
    return;
  }
  if (scancode == 0x3A) { // Caps Lock
    caps_lock = !caps_lock;
    return;
  }

  // Handle Modifier Keys (Release)
  if (scancode == 0xAA || scancode == 0xB6) { // LShift, RShift release
    shift_pressed = 0;
    return;
  }
  if (scancode == 0x9D) { // LCtrl release
    ctrl_pressed = 0;
    return;
  }
  if (scancode == 0xB8) { // LAlt release
    alt_pressed = 0;
    return;
  }

  // Handle normal keys
  if (!(scancode & 0x80)) {
    // Check for combinations
    if (ctrl_pressed && scancode == 0x2E) { // Ctrl+C
      serial_log("KEYBOARD: Ctrl+C detected. Sending SIGINT.");
      sys_kill(current_process->id, SIGINT);
      return;
    }

    // Determine character mapping
    char c = 0;
    int is_letter = (scancode >= 0x10 && scancode <= 0x19) || // q to p
                    (scancode >= 0x1E && scancode <= 0x26) || // a to l
                    (scancode >= 0x2C && scancode <= 0x32);   // z to m

    if (is_letter) {
      // Letters: Shift XOR Caps Lock
      if (shift_pressed ^ caps_lock) {
        c = kbd_us_shifted[scancode];
      } else {
        c = kbd_us[scancode];
      }
    } else {
      // Non-letters: Only Shift matters
      if (shift_pressed) {
        c = kbd_us_shifted[scancode];
      } else {
        c = kbd_us[scancode];
      }
    }

    // Special Keys (Scancode mapping for arrows and F keys)
    if (c == 0) {
      if (scancode == 0x48)
        c = 17; // Up
      if (scancode == 0x50)
        c = 18; // Down
      if (scancode == 0x4B)
        c = 19; // Left
      if (scancode == 0x4D)
        c = 20; // Right
      if (scancode == 0x3E)
        c = 14; // F4
    }

    // Handle Alt combinations
    if (alt_pressed && c == 14) { // Alt+F4
      handle_key_press(255);      // Send special "Close Current" event
      return;
    }

    if (c != 0) {
      handle_key_press(c);
    }
  }
}

#include "../kernel/apic.h"

void init_keyboard() {
  register_interrupt_handler(33, keyboard_callback); // IRQ1 = IDT 33
  ioapic_set_mask(1, false);
  serial_log("KEYBOARD: Initialized.");
}
