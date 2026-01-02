#ifndef BMP_H
#define BMP_H

#include "../include/types.h"

#pragma pack(push, 1)

typedef struct {
  uint16_t type;      // Magic identifier: 0x4d42
  uint32_t size;      // File size in bytes
  uint16_t reserved1; // Not used
  uint16_t reserved2; // Not used
  uint32_t offset;    // Offset to image data in bytes from beginning of file
} bmp_file_header_t;

typedef struct {
  uint32_t size;             // Header size in bytes
  int32_t width_px;          // Width of the image
  int32_t height_px;         // Height of the image
  uint16_t planes;           // Number of color planes
  uint16_t bits_per_pixel;   // Bits per pixel
  uint32_t compression;      // Compression type
  uint32_t image_size;       // Image size in bytes
  int32_t x_resolution;      // Pixels per meter
  int32_t y_resolution;      // Pixels per meter
  uint32_t colors_used;      // Number of colors
  uint32_t colors_important; // Important colors
} bmp_info_header_t;

#pragma pack(pop)

void draw_bmp(uint8_t *data, int x, int y);

#endif
