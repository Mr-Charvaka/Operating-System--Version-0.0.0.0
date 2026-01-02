#include "../drivers/vga.h"
#include "../include/io.h"
#include "../include/irq.h"
#include "../include/string.h"
#include "../include/types.h"
#include "../kernel/process.h"
#include "serial.h"

uint32_t tick = 0;

static void timer_callback(registers_t *regs) {
  tick++;
  if ((tick % 100) == 0) {
    // serial_log("TIMER TICK");
  }
  schedule();
}

void init_timer(uint32_t frequency) {
  // Register timer handler
  register_interrupt_handler(32, timer_callback); // IRQ0 = IDT 32
  register_interrupt_handler(34,
                             timer_callback); // IRQ0 -> GSI 2 Override (IDT 34)

  // The value we send to the PIT is the value to divide it's input clock
  // (1193180 Hz) by, to get our required frequency.
  uint32_t divisor = 1193180 / frequency;

  // Send the command byte.
  outb(0x43, 0x36);

  // Divisor has to be sent byte-wise, so split here into upper/lower bytes.
  uint8_t l = (uint8_t)(divisor & 0xFF);
  uint8_t h = (uint8_t)((divisor >> 8) & 0xFF);

  // Send the frequency divisor.
  outb(0x40, l);
  outb(0x40, h);
}
