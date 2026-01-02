// DevFS - Device Filesystem Implementation
// Provides /dev/null, /dev/zero, /dev/tty

#include "../include/string.h"
#include "../include/vfs.h"
#include "../kernel/heap.h"
#include "../kernel/memory.h"
#include "serial.h"

#include "../kernel/tty.h"
#include "serial.h"

extern "C" {
extern tty_t *tty_get_console();
extern int tty_read(tty_t *tty, char *buf, int len);
extern int tty_write(tty_t *tty, const char *buf, int len);

// Static device nodes - never freed
static vfs_node_t *devfs_root = 0;
static vfs_node_t *null_node = 0;
static vfs_node_t *zero_node = 0;
static vfs_node_t *tty_node = 0;

// ============================================================================
// /dev/null - Read returns EOF, Write discards
// ============================================================================
static uint32_t null_read(vfs_node_t *node, uint32_t offset, uint32_t size,
                          uint8_t *buffer) {
  (void)node;
  (void)offset;
  (void)size;
  (void)buffer;
  return 0; // EOF
}

static uint32_t null_write(vfs_node_t *node, uint32_t offset, uint32_t size,
                           uint8_t *buffer) {
  (void)node;
  (void)offset;
  (void)buffer;
  return size; // Accept everything, discard
}

// ============================================================================
// /dev/zero - Read returns zeros, Write discards
// ============================================================================
static uint32_t zero_read(vfs_node_t *node, uint32_t offset, uint32_t size,
                          uint8_t *buffer) {
  (void)node;
  (void)offset;
  memset(buffer, 0, size);
  return size;
}

static uint32_t zero_write(vfs_node_t *node, uint32_t offset, uint32_t size,
                           uint8_t *buffer) {
  (void)node;
  (void)offset;
  (void)buffer;
  return size; // Discard
}

// ============================================================================
// /dev/tty - Terminal placeholder
// ============================================================================
static uint32_t dev_tty_read(vfs_node_t *node, uint32_t offset, uint32_t size,
                             uint8_t *buffer) {
  (void)node;
  (void)offset;
  return tty_read(tty_get_console(), (char *)buffer, size);
}

static uint32_t dev_tty_write(vfs_node_t *node, uint32_t offset, uint32_t size,
                              uint8_t *buffer) {
  (void)node;
  (void)offset;
  return tty_write(tty_get_console(), (const char *)buffer, size);
}

// ============================================================================
// DevFS Directory Operations
// ============================================================================
static struct dirent devfs_dirent;

static struct dirent *devfs_readdir(vfs_node_t *node, uint32_t index) {
  (void)node;
  if (index == 0) {
    strcpy(devfs_dirent.name, "null");
    devfs_dirent.inode = 1;
    return &devfs_dirent;
  }
  if (index == 1) {
    strcpy(devfs_dirent.name, "zero");
    devfs_dirent.inode = 2;
    return &devfs_dirent;
  }
  if (index == 2) {
    strcpy(devfs_dirent.name, "tty");
    devfs_dirent.inode = 3;
    return &devfs_dirent;
  }
  return 0;
}

static vfs_node_t *devfs_finddir(vfs_node_t *node, const char *name) {
  (void)node;
  if (strcmp(name, "null") == 0)
    return null_node;
  if (strcmp(name, "zero") == 0)
    return zero_node;
  if (strcmp(name, "tty") == 0)
    return tty_node;
  return 0;
}

// ============================================================================
// DevFS Initialization
// ============================================================================
vfs_node_t *devfs_init() {
  serial_log("DEVFS: Initializing...");

  // Create root /dev directory
  devfs_root = (vfs_node_t *)kmalloc(sizeof(vfs_node_t));
  memset(devfs_root, 0, sizeof(vfs_node_t));
  strcpy(devfs_root->name, "dev");
  devfs_root->flags = VFS_DIRECTORY;
  devfs_root->readdir = devfs_readdir;
  devfs_root->finddir = devfs_finddir;
  devfs_root->ref_count = 0xFFFFFFFF; // Never free

  // Create /dev/null
  null_node = (vfs_node_t *)kmalloc(sizeof(vfs_node_t));
  memset(null_node, 0, sizeof(vfs_node_t));
  strcpy(null_node->name, "null");
  null_node->flags = VFS_CHARDEVICE;
  null_node->read = null_read;
  null_node->write = null_write;
  null_node->ref_count = 0xFFFFFFFF;

  // Create /dev/zero
  zero_node = (vfs_node_t *)kmalloc(sizeof(vfs_node_t));
  memset(zero_node, 0, sizeof(vfs_node_t));
  strcpy(zero_node->name, "zero");
  zero_node->flags = VFS_CHARDEVICE;
  zero_node->read = zero_read;
  zero_node->write = zero_write;
  zero_node->ref_count = 0xFFFFFFFF;

  // Create /dev/tty
  tty_node = (vfs_node_t *)kmalloc(sizeof(vfs_node_t));
  memset(tty_node, 0, sizeof(vfs_node_t));
  strcpy(tty_node->name, "tty");
  tty_node->flags = VFS_CHARDEVICE;
  tty_node->read = dev_tty_read;
  tty_node->write = dev_tty_write;
  tty_node->ref_count = 0xFFFFFFFF;

  serial_log("DEVFS: Initialized.");
  return devfs_root;
}

} // extern "C"
