#include "bmp.h"
#include "../kernel/memory.h"
#include "graphics.h"
#include "serial.h"


void draw_bmp(uint8_t *data, int x, int y) {
  bmp_file_header_t *file_header = (bmp_file_header_t *)data;
  bmp_info_header_t *info_header =
      (bmp_info_header_t *)(data + sizeof(bmp_file_header_t));

  if (file_header->type != 0x4D42) { // 'BM'
    serial_log("BMP: Invalid Signature");
    return;
  }

  int width = info_header->width_px;
  int height = info_header->height_px;
  int bpp = info_header->bits_per_pixel;

  serial_log("BMP: Rendering Image...");
  // manual print for debugging
  // serial_log_num(width);
  // serial_log_num(height);

  if (bpp != 24 && bpp != 32) {
    serial_log("BMP: Unsupported BPP. Only 24/32 supported.");
    return;
  }

  uint8_t *pixel_data = data + file_header->offset;

  // BMP is usually stored bottom-up (height > 0)
  // Row padding: Each row is padded to a multiple of 4 bytes
  int row_size = ((width * bpp + 31) / 32) * 4;

  for (int row = 0; row < height; row++) {
    for (int col = 0; col < width; col++) {
      // Calculate source index
      // If height > 0, rows are stored bottom-to-top
      // So pixel(0,0) is at the bottom-left of the image
      // We want to draw it at x, y+(height-1-row) OR just flip the loop

      // Let's iterate visual y
      // Visual row 0 (top) corresponds to data row (height-1)

      int data_row = height - 1 - row;
      int offset = (data_row * row_size) + (col * (bpp / 8));

      uint8_t b = pixel_data[offset + 0];
      uint8_t g = pixel_data[offset + 1];
      uint8_t r = pixel_data[offset + 2];

      // put_pixel expects ARGB (0xAA RR GG BB)
      uint32_t color = (0xFF << 24) | (r << 16) | (g << 8) | b;

      put_pixel(x + col, y + row, color);
    }
  }
  serial_log("BMP: Render Complete.");
}
