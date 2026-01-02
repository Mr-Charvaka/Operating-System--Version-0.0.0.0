// Character Device Interface - Serial, keyboard, etc.
#ifndef CHAR_DEVICE_H
#define CHAR_DEVICE_H

#include "../include/types.h"

struct char_device;

typedef int (*char_read_t)(struct char_device *dev, uint8_t *buffer,
                           uint32_t size);
typedef int (*char_write_t)(struct char_device *dev, const uint8_t *buffer,
                            uint32_t size);
typedef int (*char_ioctl_t)(struct char_device *dev, uint32_t request,
                            void *arg);

typedef struct char_device {
  char name[32];
  uint32_t major;     // Device class (e.g., 4 = TTY)
  uint32_t minor;     // Device instance
  void *private_data; // Driver-specific data

  char_read_t read;
  char_write_t write;
  char_ioctl_t ioctl;
} char_device_t;

#ifdef __cplusplus
extern "C" {
#endif

// Register a character device
int register_char_device(char_device_t *dev);

// Get character device by major/minor
char_device_t *get_char_device(uint32_t major, uint32_t minor);

// Get character device by name
char_device_t *get_char_device_by_name(const char *name);

#ifdef __cplusplus
}
#endif

#endif // CHAR_DEVICE_H
