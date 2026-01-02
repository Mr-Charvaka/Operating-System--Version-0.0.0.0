// Block Device - Implementation
#include "block_device.h"
#include "../include/string.h"
#include "heap.h"

extern "C" {

#define MAX_BLOCK_DEVICES 16
static block_device_t *block_devices[MAX_BLOCK_DEVICES];
static int num_block_devices = 0;

int register_block_device(block_device_t *dev) {
  if (num_block_devices >= MAX_BLOCK_DEVICES)
    return -1;
  block_devices[num_block_devices++] = dev;
  return 0;
}

block_device_t *get_block_device(const char *name) {
  for (int i = 0; i < num_block_devices; i++) {
    if (strcmp(block_devices[i]->name, name) == 0)
      return block_devices[i];
  }
  return 0;
}

int block_read(block_device_t *dev, uint32_t block, uint8_t *buffer) {
  if (!dev || !dev->read_block)
    return -1;
  return dev->read_block(dev, block, buffer);
}

int block_write(block_device_t *dev, uint32_t block, uint8_t *buffer) {
  if (!dev || !dev->write_block)
    return -1;
  return dev->write_block(dev, block, buffer);
}

} // extern "C"
