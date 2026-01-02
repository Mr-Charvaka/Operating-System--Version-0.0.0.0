#include "bga.h"
#include "../include/io.h"

void bga_write_register(uint16_t index, uint16_t value) {
  outw(VBE_DISPI_IOPORT_INDEX, index);
  outw(VBE_DISPI_IOPORT_DATA, value);
}

void bga_set_video_mode(uint16_t width, uint16_t height, uint16_t bpp) {
  bga_write_register(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
  bga_write_register(VBE_DISPI_INDEX_XRES, width);
  bga_write_register(VBE_DISPI_INDEX_YRES, height);
  bga_write_register(VBE_DISPI_INDEX_BPP, bpp);
  bga_write_register(VBE_DISPI_INDEX_ENABLE,
                     VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED);
}
