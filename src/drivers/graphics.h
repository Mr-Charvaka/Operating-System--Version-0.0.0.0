#ifndef GRAPHICS_H
#define GRAPHICS_H

#include "../include/Std/Types.h"

#define SCREEN_WIDTH 1024
#define SCREEN_HEIGHT 768

// We will map this dynamically from PCI
extern uint32_t *screen_buffer;

// Colors 0xFFRRGGBB (Opaque)
#define COLOR_BLACK 0xFF000000
#define COLOR_WHITE 0xFFFFFFFF
#define COLOR_RED 0xFFFF0000
#define COLOR_BLUE 0xFF0000FF
#define COLOR_TEAL 0xFF008080
#define COLOR_LIGHT_GRAY 0xFFC0C0C0
#define COLOR_DARK_GRAY 0xFF808080

// Retro Pixel Palette
#define PIXEL_BLUE 0xFF4A90D9
#define PIXEL_YELLOW 0xFFFFD100
#define PIXEL_RED 0xFFFF4B4B
#define PIXEL_GREEN 0xFF8BC34A
#define PIXEL_PURPLE 0xFF9C27B0
#define PIXEL_CYAN 0xFF00BCD4
#define PIXEL_ORANGE 0xFFFF9800
#define PIXEL_WHITE 0xFFFFFFFF
#define PIXEL_BLACK 0xFF000000

void put_pixel(int x, int y, uint32_t color);
void draw_rect(int x, int y, int w, int h, uint32_t color);
void draw_line(int x1, int y1, int x2, int y2, uint32_t color);
void gfx_clear_screen(uint32_t color);
void draw_char(int x, int y, char c, uint32_t color);
void draw_char_scaled(int x, int y, char c, uint32_t color, int scale);
void draw_string(int x, int y, const char *str, uint32_t color);
void draw_string_scaled(int x, int y, const char *str, uint32_t color,
                        int scale);
void draw_rect_gradient(int x, int y, int w, int h, uint32_t c1, uint32_t c2);

// Double Buffering
void init_graphics(uint32_t lfb_address); // Allocates backbuffer
void swap_buffers();

// Primitives
void draw_circle(int x, int y, int radius, uint32_t color);
void draw_filled_circle(int x0, int y0, int radius, uint32_t color);
void draw_circle_filled(int x, int y, int radius,
                        uint32_t color); // Legacy alias? Or assume we replaced
                                         // it. I'll keep the new one.
void draw_rounded_rect(int x, int y, int w, int h, int r, uint32_t color);
void draw_rect_alpha(int x, int y, int w, int h, uint32_t color);

// Retro Pixel Primitives
void draw_pixel_grid(uint32_t bg_color, uint32_t grid_color, int spacing);
void draw_pixel_box(int x, int y, int w, int h, uint32_t bg_color);
void draw_thick_line(int x1, int y1, int x2, int y2, int thickness,
                     uint32_t color);

#endif
