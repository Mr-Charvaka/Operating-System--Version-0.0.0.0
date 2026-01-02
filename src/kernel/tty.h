// TTY - Terminal Device
#ifndef TTY_H
#define TTY_H

#include "../include/types.h"
#include "wait_queue.h"

#define TTY_BUFFER_SIZE 256

// TTY flags
#define TTY_ECHO 0x01  // Echo input characters
#define TTY_CANON 0x02 // Canonical mode (line buffering)
#define TTY_ISIG 0x04  // Enable signals (SIGINT, etc.)

typedef struct tty {
  char name[16];

  // Input buffer (for canonical mode)
  char input_buffer[TTY_BUFFER_SIZE];
  int input_head;
  int input_tail;
  int input_count;

  // Line buffer (for canonical mode)
  char line_buffer[TTY_BUFFER_SIZE];
  int line_len;

  // Flags
  uint32_t flags;

  // Wait queue for blocking reads
  wait_queue_t read_wait;

  // Foreground process group (for signals)
  int fg_pgid;
} tty_t;

#ifdef __cplusplus
extern "C" {
#endif

// Initialize TTY system
void tty_init();

// Get the console TTY
tty_t *tty_get_console();

// Write to TTY (output)
int tty_write(tty_t *tty, const char *buf, int len);

// Read from TTY (input)
int tty_read(tty_t *tty, char *buf, int len);

// Input character from keyboard
void tty_input_char(tty_t *tty, char c);

// Set TTY flags
void tty_set_flags(tty_t *tty, uint32_t flags);

#ifdef __cplusplus
}
#endif

#endif // TTY_H
