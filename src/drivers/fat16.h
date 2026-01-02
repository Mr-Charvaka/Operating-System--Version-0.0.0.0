#ifndef FAT16_H
#define FAT16_H

#include "../include/Std/Types.h"

// FAT16 Boot Sector BPB
typedef struct {
  uint8_t jmp[3];
  char oem[8];
  uint16_t bytes_per_sector;
  uint8_t sectors_per_cluster;
  uint16_t reserved_sectors;
  uint8_t fats_count;
  uint16_t root_entries_count;
  uint16_t total_sectors_16;
  uint8_t media_type;
  uint16_t sectors_per_fat;
  uint16_t sectors_per_track;
  uint16_t heads_count;
  uint32_t hidden_sectors;
  uint32_t total_sectors_32;
  uint8_t drive_num;
  uint8_t reserved1;
  uint8_t boot_signature;
  uint32_t volume_id;
  char volume_label[11];
  char fs_type[8];
} __attribute__((packed)) fat16_bpb_t;

// Directory Entry (32 bytes)
typedef struct {
  char filename[8];
  char ext[3];
  uint8_t attributes;
  uint8_t reserved;
  uint8_t create_time_tenth;
  uint16_t create_time;
  uint16_t create_date;
  uint16_t last_access_date;
  uint16_t first_cluster_high; // FAT32 only, usually 0 in FAT16
  uint16_t write_time;
  uint16_t write_date;
  uint16_t first_cluster_low;
  uint32_t file_size;
} __attribute__((packed)) fat16_entry_t;

// Attributes
#define ATTR_READ_ONLY 0x01
#define ATTR_HIDDEN 0x02
#define ATTR_SYSTEM 0x04
#define ATTR_VOLUME_ID 0x08
#define ATTR_DIRECTORY 0x10
#define ATTR_ARCHIVE 0x20
#define ATTR_LFN 0x0F

#include "vfs.h"

#ifdef __cplusplus
extern "C" {
#endif

void fat16_init();
void fat16_list_root();
void fat16_create_test_file();
fat16_entry_t fat16_find_file(const char *filename);
void fat16_read_file(fat16_entry_t *entry, uint8_t *buffer);

vfs_node_t *fat16_vfs_init();
vfs_node_t *devfs_init();

#ifdef __cplusplus
}
#endif

#endif
