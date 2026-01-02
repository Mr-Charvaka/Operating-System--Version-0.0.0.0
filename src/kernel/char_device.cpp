// Character Device - Implementation
#include "char_device.h"
#include "../include/string.h"
#include "heap.h"

extern "C" {

#define MAX_CHAR_DEVICES 32
static char_device_t *char_devices[MAX_CHAR_DEVICES];
static int num_char_devices = 0;

int register_char_device(char_device_t *dev) {
  if (num_char_devices >= MAX_CHAR_DEVICES)
    return -1;
  char_devices[num_char_devices++] = dev;
  return 0;
}

char_device_t *get_char_device(uint32_t major, uint32_t minor) {
  for (int i = 0; i < num_char_devices; i++) {
    if (char_devices[i]->major == major && char_devices[i]->minor == minor)
      return char_devices[i];
  }
  return 0;
}

char_device_t *get_char_device_by_name(const char *name) {
  for (int i = 0; i < num_char_devices; i++) {
    if (strcmp(char_devices[i]->name, name) == 0)
      return char_devices[i];
  }
  return 0;
}

} // extern "C"
