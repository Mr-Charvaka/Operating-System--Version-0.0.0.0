#include "../include/vfs.h"
#include "../include/string.h"
#include "heap.h"
#include "memory.h"

extern "C" {

vfs_node_t *vfs_root = 0;
vfs_node_t *vfs_dev = 0;

uint32_t read_vfs(vfs_node_t *node, uint32_t offset, uint32_t size,
                  uint8_t *buffer) {
  if (node && node->read != 0)
    return node->read(node, offset, size, buffer);
  return 0;
}

uint32_t write_vfs(vfs_node_t *node, uint32_t offset, uint32_t size,
                   uint8_t *buffer) {
  if (node && node->write != 0)
    return node->write(node, offset, size, buffer);
  return 0;
}

void open_vfs(vfs_node_t *node, uint8_t read, uint8_t write) {
  if (node && node->open != 0)
    node->open(node);
}

void close_vfs(vfs_node_t *node) {
  if (!node)
    return;

  // Don't decrement/free static nodes (DevFS devices use 0xFFFFFFFF)
  if (node->ref_count == 0xFFFFFFFF)
    return;

  if (node->ref_count > 0)
    node->ref_count--;

  // Only close/free if ref_count reaches 0
  if (node->ref_count == 0) {
    if (node->close != 0)
      node->close(node);
    kfree(node);
  }
}

struct dirent *readdir_vfs(vfs_node_t *node, uint32_t index) {
  if (node && (node->flags & 0x7) == VFS_DIRECTORY && node->readdir != 0)
    return node->readdir(node, index);
  return 0;
}

// Simple path resolution: "/dev/stdout" or "HELLO.ELF"
vfs_node_t *finddir_vfs(vfs_node_t *node, const char *name) {
  if (!node || !name)
    return 0;

  // Handle "/dev/" prefix
  if (memcmp(name, "/dev/", 5) == 0) {
    if (vfs_dev && vfs_dev->finddir) {
      return vfs_dev->finddir(vfs_dev, name + 5);
    }
  }

  // Otherwise, dispatch to the provided node (e.g. FAT16 root)
  if ((node->flags & 0x7) == VFS_DIRECTORY && node->finddir != 0)
    return node->finddir(node, name);
  return 0;
}

vfs_node_t *vfs_resolve_path(const char *path) {
  if (!path)
    return 0;

  vfs_node_t *node = vfs_root;
  int i = 0;
  int len = strlen(path);

  // If relative, we should ideally start from CWD node.
  // For this OS, we assume single-rooted tree.
  // We'll handle relative paths by prepending CWD if path[0] != '/'.
  // But let's keep it simple for now: all paths starting from root or relative
  // from root.

  while (i < len && node) {
    while (i < len && path[i] == '/')
      i++;
    if (i >= len)
      break;

    char name[128];
    int start = i;
    while (i < len && path[i] != '/')
      i++;
    int clen = i - start;
    if (clen > 127)
      clen = 127;
    memcpy(name, path + start, clen);
    name[clen] = 0;

    node = finddir_vfs(node, name);
  }

  return node;
}

int mkdir_vfs(vfs_node_t *node, const char *name, uint32_t mask) {
  if (node && (node->flags & 0x7) == VFS_DIRECTORY && node->mkdir != 0)
    return node->mkdir(node, name, mask);
  return -1;
}

int unlink_vfs(vfs_node_t *node, const char *name) {
  if (node && (node->flags & 0x7) == VFS_DIRECTORY && node->unlink != 0)
    return node->unlink(node, name);
  return -1;
}

int rmdir_vfs(vfs_node_t *node, const char *name) {
  if (node && (node->flags & 0x7) == VFS_DIRECTORY && node->rmdir != 0)
    return node->rmdir(node, name);
  return -1;
}

} // extern "C"
