#ifndef VFS_H
#define VFS_H

#include "types.h"

#define VFS_FILE 0x01
#define VFS_DIRECTORY 0x02
#define VFS_CHARDEVICE 0x03
#define VFS_BLOCKDEVICE 0x04
#define VFS_PIPE 0x05
#define VFS_SYMLINK 0x06
#define VFS_SOCKET 0x07
#define VFS_MOUNTPOINT 0x08

// Open flags
#define O_RDONLY 0x00
#define O_WRONLY 0x01
#define O_RDWR 0x02
#define O_CREAT 0x40
#define O_TRUNC 0x200

struct vfs_node;

typedef uint32_t (*read_type_t)(struct vfs_node *, uint32_t, uint32_t,
                                uint8_t *);
typedef uint32_t (*write_type_t)(struct vfs_node *, uint32_t, uint32_t,
                                 uint8_t *);
typedef void (*open_type_t)(struct vfs_node *);
typedef void (*close_type_t)(struct vfs_node *);
typedef struct dirent *(*readdir_type_t)(struct vfs_node *, uint32_t);
typedef struct vfs_node *(*finddir_type_t)(struct vfs_node *, const char *name);
typedef int (*mkdir_type_t)(struct vfs_node *, const char *name, uint32_t mask);
typedef int (*unlink_type_t)(struct vfs_node *, const char *name);
typedef int (*rmdir_type_t)(struct vfs_node *, const char *name);

typedef struct vfs_node {
  char name[128];
  uint32_t mask;
  uint32_t uid;
  uint32_t gid;
  uint32_t flags;
  uint32_t inode;
  uint32_t length;
  uint32_t impl;      // Implementation-specific value (e.g. cluster number)
  uint32_t ref_count; // Reference count for shared nodes (e.g. sockets/pipes)

  read_type_t read;
  write_type_t write;
  open_type_t open;
  close_type_t close;
  readdir_type_t readdir;
  finddir_type_t finddir;
  mkdir_type_t mkdir;
  unlink_type_t unlink;
  rmdir_type_t rmdir;

  struct vfs_node *ptr; // For mountpoints
} vfs_node_t;

struct dirent {
  char name[128];
  uint32_t inode;
};

#ifdef __cplusplus
extern "C" {
#endif

extern vfs_node_t *vfs_root;
extern vfs_node_t *vfs_dev;

// High-level API
uint32_t read_vfs(vfs_node_t *node, uint32_t offset, uint32_t size,
                  uint8_t *buffer);
uint32_t write_vfs(vfs_node_t *node, uint32_t offset, uint32_t size,
                   uint8_t *buffer);
void open_vfs(vfs_node_t *node, uint8_t read, uint8_t write);
void close_vfs(vfs_node_t *node);
struct dirent *readdir_vfs(vfs_node_t *node, uint32_t index);
vfs_node_t *finddir_vfs(vfs_node_t *node, const char *name);
vfs_node_t *vfs_resolve_path(const char *path);
int mkdir_vfs(vfs_node_t *node, const char *name, uint32_t mask);
int unlink_vfs(vfs_node_t *node, const char *name);
int rmdir_vfs(vfs_node_t *node, const char *name);

#ifdef __cplusplus
}
#endif

#endif
