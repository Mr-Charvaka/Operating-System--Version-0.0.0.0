#include "fat16.h"
#include "../include/dirent.h"
#include "../include/string.h"
#include "../include/vfs.h"
#include "../kernel/heap.h"
#include "../kernel/memory.h"
#include "ata.h"
#include "serial.h"


// Forward declaration for DevFS
extern "C" vfs_node_t *devfs_init();
static vfs_node_t *devfs_node = 0;

extern "C" {
static struct dirent *fat16_readdir_vfs(vfs_node_t *node, uint32_t index);
extern void fat16_get_stats_bytes(uint32_t *total_bytes, uint32_t *free_bytes);

static fat16_bpb_t bpb;
static uint32_t root_dir_start_sector;
static uint32_t data_start_sector;

void fat16_init() {
  uint8_t sector[512];
  ata_read_sector(0, sector); // Read Boot Sector (LBA 0)
  memcpy(&bpb, sector, sizeof(fat16_bpb_t));

  root_dir_start_sector =
      bpb.reserved_sectors + (bpb.fats_count * bpb.sectors_per_fat);
  uint32_t root_dir_sectors = (bpb.root_entries_count * 32 + 511) / 512;
  data_start_sector = root_dir_start_sector + root_dir_sectors;

  serial_log("FAT16: Initialized.");
}

void fat16_list_root() {
  serial_log("FAT16: Listing Root Directory...");
  uint8_t buffer[512];
  ata_read_sector(root_dir_start_sector, buffer);
  fat16_entry_t *entries = (fat16_entry_t *)buffer;

  for (int i = 0; i < 16; i++) {
    if (entries[i].filename[0] == 0)
      break;
    if ((uint8_t)entries[i].filename[0] == 0xE5)
      continue;

    char name[13];
    int k = 0;
    for (int j = 0; j < 8; j++)
      if (entries[i].filename[j] != ' ')
        name[k++] = entries[i].filename[j];
    name[k++] = '.';
    for (int j = 0; j < 3; j++)
      if (entries[i].ext[j] != ' ')
        name[k++] = entries[i].ext[j];
    name[k] = 0;
    serial_log(name);
  }
}

fat16_entry_t fat16_find_file(const char *filename) {
  uint8_t buffer[512];
  uint32_t root_dir_sectors = (bpb.root_entries_count * 32 + 511) / 512;

  serial_log("FAT16: Searching for file:");
  serial_log(filename);

  for (uint32_t sector = 0; sector < root_dir_sectors; sector++) {
    ata_read_sector(root_dir_start_sector + sector, buffer);
    fat16_entry_t *entries = (fat16_entry_t *)buffer;

    for (int i = 0; i < 16; i++) {
      if ((uint8_t)entries[i].filename[0] == 0) {
        // End of entries
        serial_log("FAT16: End of entries reached.");
        goto not_found;
      }
      if ((uint8_t)entries[i].filename[0] == 0xE5) {
        // Deleted entry
        continue;
      }
      if (entries[i].attributes == 0x0F) {
        // LFN entry - ignore
        continue;
      }

      char name[13];
      int k = 0;
      for (int j = 0; j < 8; j++)
        if (entries[i].filename[j] != ' ')
          name[k++] = entries[i].filename[j];
      if (entries[i].ext[0] != ' ') {
        name[k++] = '.';
        for (int j = 0; j < 3; j++)
          if (entries[i].ext[j] != ' ')
            name[k++] = entries[i].ext[j];
      }
      name[k] = 0;

      serial_log("FAT16: Found entry:");
      serial_log(name);

      if (strcmp(name, filename) == 0) {
        serial_log("FAT16: Match found!");
        return entries[i];
      }
    }
  }

not_found:
  serial_log("FAT16: File not found.");
  fat16_entry_t empty;
  memset(&empty, 0, sizeof(fat16_entry_t));
  return empty;
}

void fat16_read_file(fat16_entry_t *entry, uint8_t *buffer) {
  uint32_t cluster = entry->first_cluster_low;
  uint32_t sector = data_start_sector + (cluster - 2) * bpb.sectors_per_cluster;
  uint32_t sectors_to_read = (entry->file_size + 511) / 512;

  serial_log_hex("FAT16: Reading file from cluster ", cluster);
  serial_log_hex("FAT16: Start sector ", sector);
  serial_log_hex("FAT16: Sectors to read ", sectors_to_read);

  for (uint32_t i = 0; i < sectors_to_read; i++) {
    ata_read_sector(sector + i, buffer + (i * 512));
  }
}

// ============================================================================
// FAT16 Write Support
// ============================================================================

// Get FAT start sector
static uint32_t fat16_get_fat_sector() { return bpb.reserved_sectors; }

// Read FAT entry for a cluster
static uint16_t fat16_get_fat_entry(uint16_t cluster) {
  uint32_t fat_offset = cluster * 2;
  uint32_t fat_sector = fat16_get_fat_sector() + (fat_offset / 512);
  uint32_t entry_offset = fat_offset % 512;

  uint8_t buffer[512];
  ata_read_sector(fat_sector, buffer);
  return *(uint16_t *)(buffer + entry_offset);
}

// Set FAT entry for a cluster
static void fat16_set_fat_entry(uint16_t cluster, uint16_t value) {
  uint32_t fat_offset = cluster * 2;
  uint32_t fat_sector = fat16_get_fat_sector() + (fat_offset / 512);
  uint32_t entry_offset = fat_offset % 512;

  uint8_t buffer[512];
  ata_read_sector(fat_sector, buffer);
  *(uint16_t *)(buffer + entry_offset) = value;
  ata_write_sector(fat_sector, buffer);

  // Also update backup FAT if exists
  if (bpb.fats_count > 1) {
    ata_write_sector(fat_sector + bpb.sectors_per_fat, buffer);
  }
}

// Allocate a free cluster
uint16_t fat16_alloc_cluster() {
  // Scan FAT for free cluster (0x0000 means free)
  for (uint16_t cluster = 2; cluster < 0xFFF0; cluster++) {
    if (fat16_get_fat_entry(cluster) == 0x0000) {
      fat16_set_fat_entry(cluster, 0xFFFF); // Mark as end of chain
      return cluster;
    }
  }
  return 0; // No free clusters
}

// Write data to a file (simple: overwrites from start)
int fat16_write_file(const char *filename, uint8_t *data, uint32_t size) {
  fat16_entry_t entry = fat16_find_file(filename);
  if (entry.filename[0] == 0)
    return -1; // File not found

  uint16_t cluster = entry.first_cluster_low;
  if (cluster < 2) {
    // Need to allocate first cluster
    cluster = fat16_alloc_cluster();
    if (cluster == 0)
      return -1;
    // Update entry's first cluster (needs to update dir entry too - simplified)
  }

  uint32_t sector = data_start_sector + (cluster - 2) * bpb.sectors_per_cluster;
  uint32_t sectors_to_write = (size + 511) / 512;

  // Write sectors
  uint8_t buffer[512];
  for (uint32_t i = 0; i < sectors_to_write; i++) {
    memset(buffer, 0, 512);
    uint32_t chunk = (size - i * 512 > 512) ? 512 : (size - i * 512);
    memcpy(buffer, data + i * 512, chunk);
    ata_write_sector(sector + i, buffer);
  }

  // Update file size in directory entry
  uint8_t dir_buf[512];
  ata_read_sector(root_dir_start_sector, dir_buf);
  fat16_entry_t *entries = (fat16_entry_t *)dir_buf;

  for (int i = 0; i < 16; i++) {
    if (memcmp(entries[i].filename, entry.filename, 8) == 0 &&
        memcmp(entries[i].ext, entry.ext, 3) == 0) {
      entries[i].file_size = size;
      entries[i].first_cluster_low = cluster;
      ata_write_sector(root_dir_start_sector, dir_buf);
      break;
    }
  }

  return size;
}

// Create a new file in root directory
int fat16_create_file(const char *filename) {
  // Check if file already exists
  fat16_entry_t existing = fat16_find_file(filename);
  if (existing.filename[0] != 0)
    return 0; // Already exists

  // Find free directory entry
  uint8_t buffer[512];
  ata_read_sector(root_dir_start_sector, buffer);
  fat16_entry_t *entries = (fat16_entry_t *)buffer;

  int free_slot = -1;
  for (int i = 0; i < 16; i++) {
    if (entries[i].filename[0] == 0 ||
        (uint8_t)entries[i].filename[0] == 0xE5) {
      free_slot = i;
      break;
    }
  }

  if (free_slot < 0)
    return -1; // No free slots

  // Parse filename (e.g., "TEST.TXT" -> "TEST    " + "TXT")
  memset(&entries[free_slot], 0, sizeof(fat16_entry_t));
  memset(entries[free_slot].filename, ' ', 8);
  memset(entries[free_slot].ext, ' ', 3);

  int fi = 0, ei = 0;
  int in_ext = 0;
  for (int i = 0; filename[i] && fi < 8; i++) {
    if (filename[i] == '.') {
      in_ext = 1;
      ei = 0;
    } else if (in_ext && ei < 3) {
      entries[free_slot].ext[ei++] = filename[i];
    } else if (!in_ext && fi < 8) {
      entries[free_slot].filename[fi++] = filename[i];
    }
  }

  entries[free_slot].attributes = 0x20; // Archive
  entries[free_slot].file_size = 0;
  entries[free_slot].first_cluster_low = 0;

  return 0;
}

// Delete a file
int fat16_delete_file(const char *filename) {
  fat16_entry_t entry = fat16_find_file(filename);
  if (entry.filename[0] == 0)
    return -1;

  // Free cluster chain
  uint16_t cluster = entry.first_cluster_low;
  while (cluster >= 2 && cluster < 0xFFF0) {
    uint16_t next = fat16_get_fat_entry(cluster);
    fat16_set_fat_entry(cluster, 0x0000);
    cluster = next;
  }

  // Mark directory entry as deleted
  uint8_t buffer[512];
  ata_read_sector(root_dir_start_sector, buffer);
  fat16_entry_t *entries = (fat16_entry_t *)buffer;

  for (int i = 0; i < 16; i++) {
    if (memcmp(entries[i].filename, entry.filename, 8) == 0 &&
        memcmp(entries[i].ext, entry.ext, 3) == 0) {
      entries[i].filename[0] = 0xE5; // Mark as deleted
      ata_write_sector(root_dir_start_sector, buffer);
      break;
    }
  }

  return 0;
}

static int fat16_unlink_vfs(vfs_node_t *node, const char *name) {
  (void)node;
  return fat16_delete_file(name);
}

// Simple mkdir (creates directory entry with ATTR_DIRECTORY)
int fat16_mkdir(const char *name) {
  // Check if already exists
  fat16_entry_t existing = fat16_find_file(name);
  if (existing.filename[0] != 0)
    return -1;

  // Find free directory entry
  uint8_t buffer[512];
  ata_read_sector(root_dir_start_sector, buffer);
  fat16_entry_t *entries = (fat16_entry_t *)buffer;

  int free_slot = -1;
  for (int i = 0; i < 16; i++) {
    if (entries[i].filename[0] == 0 ||
        (uint8_t)entries[i].filename[0] == 0xE5) {
      free_slot = i;
      break;
    }
  }

  if (free_slot < 0)
    return -1;

  memset(&entries[free_slot], 0, sizeof(fat16_entry_t));
  memset(entries[free_slot].filename, ' ', 8);
  memset(entries[free_slot].ext, ' ', 3);

  int fi = 0;
  for (int i = 0; name[i] && fi < 8; i++) {
    entries[free_slot].filename[fi++] = name[i];
  }

  entries[free_slot].attributes = 0x10; // ATTR_DIRECTORY
  entries[free_slot].file_size = 0;

  // Allocate a cluster for the directory
  uint16_t cluster = fat16_alloc_cluster();
  if (cluster == 0)
    return -1;
  entries[free_slot].first_cluster_low = cluster;

  // Clear the cluster
  uint8_t zero[512];
  memset(zero, 0, 512);
  uint32_t sector = data_start_sector + (cluster - 2) * bpb.sectors_per_cluster;
  for (int i = 0; i < bpb.sectors_per_cluster; i++) {
    ata_write_sector(sector + i, zero);
  }

  ata_write_sector(root_dir_start_sector, buffer);
  return 0;
}

static int fat16_mkdir_vfs(vfs_node_t *node, const char *name, uint32_t mask) {
  (void)node;
  (void)mask;
  return fat16_mkdir(name);
}

// VFS write wrapper
static uint32_t fat16_write_vfs(vfs_node_t *node, uint32_t offset,
                                uint32_t size, uint8_t *buffer) {
  (void)offset; // For now, always write from start
  int result = fat16_write_file(node->name, buffer, size);
  if (result < 0)
    return 0;
  node->length = size;
  return size;
}

// VFS wrappers
static uint32_t fat16_read_vfs(vfs_node_t *node, uint32_t offset, uint32_t size,
                               uint8_t *buffer) {
  fat16_entry_t entry = fat16_find_file(node->name);
  if (entry.filename[0] == 0)
    return 0;

  if (offset >= entry.file_size)
    return 0;
  if (offset + size > entry.file_size)
    size = entry.file_size - offset;

  // Allocate sector-aligned buffer for DMA/PIO reading
  uint32_t aligned_size = (entry.file_size + 511) & ~511;
  uint8_t *tmp = (uint8_t *)kmalloc(aligned_size);
  if (!tmp)
    return 0;

  fat16_read_file(&entry, tmp);
  memcpy(buffer, tmp + offset, size);
  kfree(tmp);
  return size;
}

static vfs_node_t *fat16_finddir_vfs(vfs_node_t *node, const char *name) {
  (void)node;

  // Check for DevFS mount point
  if (strcmp(name, "dev") == 0) {
    if (!devfs_node)
      devfs_node = devfs_init();
    return devfs_node;
  }

  fat16_entry_t entry = fat16_find_file(name);
  if (entry.filename[0] == 0)
    return 0;

  vfs_node_t *res = (vfs_node_t *)kmalloc(sizeof(vfs_node_t));
  memset(res, 0, sizeof(vfs_node_t));
  strcpy(res->name, name);
  res->length = entry.file_size;
  res->read = fat16_read_vfs;
  res->write = fat16_write_vfs;
  res->readdir = fat16_readdir_vfs;
  res->finddir = fat16_finddir_vfs; // Support subdirectories
  res->unlink = fat16_unlink_vfs;
  res->mkdir = fat16_mkdir_vfs;

  if (entry.attributes & 0x10) { // ATTR_DIRECTORY
    res->flags = VFS_DIRECTORY;
  } else {
    res->flags = VFS_FILE;
  }

  res->ref_count = 1;
  return res;
}

static struct dirent *fat16_readdir_vfs(vfs_node_t *node, uint32_t index) {
  uint32_t root_sectors = (bpb.root_entries_count * 32 + 511) / 512;
  uint8_t buffer[512];
  uint32_t count = 0;

  for (uint32_t s = 0; s < root_sectors; s++) {
    ata_read_sector(root_dir_start_sector + s, buffer);
    fat16_entry_t *entries = (fat16_entry_t *)buffer;

    for (int i = 0; i < 16; i++) {
      if (entries[i].filename[0] == 0)
        return 0;
      if ((uint8_t)entries[i].filename[0] == 0xE5)
        continue;
      if (entries[i].attributes & 0x08) // ATTR_VOLUME_ID
        continue;

      if (count == index) {
        static struct dirent d;
        // Clear previous data
        memset(d.d_name, 0, 256);
        d.d_ino = 0;

        int k = 0;
        for (int j = 0; j < 8; j++)
          if (entries[i].filename[j] != ' ')
            d.d_name[k++] = entries[i].filename[j];

        if (entries[i].ext[0] != ' ') {
          d.d_name[k++] = '.';
          for (int j = 0; j < 3; j++)
            if (entries[i].ext[j] != ' ')
              d.d_name[k++] = entries[i].ext[j];
        }
        d.d_name[k] = 0;
        d.d_ino = entries[i].first_cluster_low;
        d.d_off = index;
        d.d_reclen = sizeof(struct dirent);
        if (entries[i].attributes & 0x10)
          d.d_type = DT_DIR;
        else
          d.d_type = DT_REG;

        return &d;
      }
      count++;
    }
  }
  return 0;
}

void fat16_get_stats_bytes(uint32_t *total_bytes, uint32_t *free_bytes) {
  uint32_t total_sectors =
      bpb.total_sectors_16 != 0 ? bpb.total_sectors_16 : bpb.total_sectors_32;
  if (total_bytes)
    *total_bytes = total_sectors * 512;
  if (free_bytes) {
    // Return a reasonable placeholder or count clusters
    // For 32MB image, let's say 25MB is free
    *free_bytes = 25 * 1024 * 1024;
  }
}

vfs_node_t *fat16_vfs_init() {
  vfs_node_t *root = (vfs_node_t *)kmalloc(sizeof(vfs_node_t));
  memset(root, 0, sizeof(vfs_node_t));
  strcpy(root->name, "/");
  root->flags = VFS_DIRECTORY;
  root->readdir = fat16_readdir_vfs;
  root->finddir = fat16_finddir_vfs;
  root->ref_count = 0xFFFFFFFF; // Static root

  // Initialize and mount DevFS
  devfs_node = devfs_init();
  devfs_node->ptr = root;

  return root;
}

} // extern "C"
