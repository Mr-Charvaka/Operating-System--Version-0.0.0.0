// Block Device Interface - Abstract disk access
#ifndef BLOCK_DEVICE_H
#define BLOCK_DEVICE_H

#include "../include/types.h"

#define BLOCK_SIZE 512

struct block_device;

typedef int (*block_read_t)(struct block_device *dev, uint32_t block,
                            uint8_t *buffer);
typedef int (*block_write_t)(struct block_device *dev, uint32_t block,
                             uint8_t *buffer);

typedef struct block_device {
  char name[32];
  uint32_t block_size;   // Usually 512
  uint32_t total_blocks; // Total number of blocks
  void *private_data;    // Driver-specific data

  block_read_t read_block;
  block_write_t write_block;
} block_device_t;

#ifdef __cplusplus
extern "C" {
#endif

// Register a block device
int register_block_device(block_device_t *dev);

// Get block device by name
block_device_t *get_block_device(const char *name);

// High-level read/write
int block_read(block_device_t *dev, uint32_t block, uint8_t *buffer);
int block_write(block_device_t *dev, uint32_t block, uint8_t *buffer);

#ifdef __cplusplus
}
#endif

#endif // BLOCK_DEVICE_H
