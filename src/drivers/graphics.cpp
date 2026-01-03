#include "../drivers/graphics.h"
#include "../include/font.h"
#include "../include/string.h"

int abs(int n) { return n > 0 ? n : -n; }

#include "../drivers/serial.h" // Debug
#include "../kernel/memory.h"

uint32_t *frame_buffer = (uint32_t *)0; // Video Memory (LFB)
uint32_t *back_buffer = (uint32_t *)0;  // RAM

// Screen buffer currently points to what put_pixel writes to
// Initially NULL, set to back_buffer after init
uint32_t *screen_buffer = (uint32_t *)0;

// back_buffer dynamic allocation
void init_graphics(uint32_t lfb_address) {
  frame_buffer = (uint32_t *)lfb_address;

  // Dynamic allocation (Heap starts at high memory)
  back_buffer = (uint32_t *)kmalloc(SCREEN_WIDTH * SCREEN_HEIGHT * 4);
  if (!back_buffer) {
    serial_log("GRAPHICS: CRITICAL! Failed to allocate backbuffer.");
    return;
  }

  memset(back_buffer, 0, SCREEN_WIDTH * SCREEN_HEIGHT * 4);
  screen_buffer = back_buffer;
  serial_log("GRAPHICS: Using dynamic backbuffer.");
  serial_log_hex("GRAPHICS: Backbuffer Addr: ", (uint32_t)back_buffer);
}

void swap_buffers() {
  if (back_buffer && frame_buffer) {
    // Debug: Verify pointers haven't been corrupted
    if ((uint32_t)frame_buffer < 0xF0000000 ||
        (uint32_t)back_buffer < 0xC0000000) {
      serial_log("GRAPHICS: CRITICAL - Buffer pointer corruption!");
      serial_log_hex("  frame_buffer: ", (uint32_t)frame_buffer);
      serial_log_hex("  back_buffer:  ", (uint32_t)back_buffer);
      for (;;)
        ; // Halt
    }
    memcpy(frame_buffer, back_buffer, SCREEN_WIDTH * SCREEN_HEIGHT * 4);
  }
}

// Simple 8x8 Bitmap Font (First 128 ASCII chars)
// We'll define a minimal set or load a full one. For brevity, I'll include a
// small subset or logic to handle it. Actually, creating a full font locally is
// huge. I'll implement a VERY basic font (A-Z, 0-9) manually or use a
// procedural request if needed. For now, let's use a placeholder block for
// characters if I can't look up a font data easily. Wait, I can define a simple
// 8x8 font for 'A' and 'B' as test, but the user wants "No CLib raw". I will
// assume I can write a helper function to draw simple block chars or lines for
// text until I import a full font file. BETTER: I will include a small 8x8 font
// array for common characters.

// 8x8 font data for 'A' (Example)
// 0, 24, 60, 102, 102, 126, 102, 102 (binary representation)

// Let's use a very small subset font for now to save space in this file,
// or I can implement a "draw_char" that just draws a rectangle for now until I
// upload a font file? No, the user wants "Windows 95". I need text. I will
// implement a minimal font for "Windows 95" title.

// Minimal 5x7 font data for 'A'..'Z' is manageable.
// ... Actually, `vga.c` previously used the text mode hardware font. Now we are
// in graphics mode. We NEED a font. I will generate a font.h file in the next
// step. For now, I'll put the declarations.

// Helper for alpha blending
// color, bg are 0xAARRGGBB
uint32_t blend_colors(uint32_t fg, uint32_t bg) {
  uint32_t alpha = (fg >> 24) & 0xFF;
  if (alpha == 0)
    return bg;
  if (alpha == 255)
    return fg;

  uint32_t inv_alpha = 255 - alpha;

  uint32_t r =
      (((fg >> 16) & 0xFF) * alpha + ((bg >> 16) & 0xFF) * inv_alpha) / 255;
  uint32_t g =
      (((fg >> 8) & 0xFF) * alpha + ((bg >> 8) & 0xFF) * inv_alpha) / 255;
  uint32_t b = ((fg & 0xFF) * alpha + (bg & 0xFF) * inv_alpha) / 255;

  return 0xFF000000 | (r << 16) | (g << 8) | b;
}

void put_pixel(int x, int y, uint32_t color) {
  if (x >= 0 && x < SCREEN_WIDTH && y >= 0 && y < SCREEN_HEIGHT) {
    screen_buffer[y * SCREEN_WIDTH + x] = color;
  }
}

void blend_pixel(int x, int y, uint32_t color) {
  if (x >= 0 && x < SCREEN_WIDTH && y >= 0 && y < SCREEN_HEIGHT) {
    uint32_t bg = screen_buffer[y * SCREEN_WIDTH + x];
    screen_buffer[y * SCREEN_WIDTH + x] = blend_colors(color, bg);
  }
}

void draw_rect(int x, int y, int w, int h, uint32_t color) {
  for (int i = 0; i < h; i++) {
    for (int j = 0; j < w; j++) {
      put_pixel(x + j, y + i, color);
    }
  }
}

void draw_rect_alpha(int x, int y, int w, int h, uint32_t color) {
  for (int i = 0; i < h; i++) {
    for (int j = 0; j < w; j++) {
      blend_pixel(x + j, y + i, color);
    }
  }
}

void draw_line(int x1, int y1, int x2, int y2, uint32_t color) {
  int dx = abs(x2 - x1);
  int dy = -abs(y2 - y1);
  int sx = x1 < x2 ? 1 : -1;
  int sy = y1 < y2 ? 1 : -1;
  int err = dx + dy;

  while (1) {
    blend_pixel(x1, y1, color);
    if (x1 == x2 && y1 == y2)
      break;
    int e2 = 2 * err;
    if (e2 >= dy) {
      err += dy;
      x1 += sx;
    }
    if (e2 <= dx) {
      err += dx;
      y1 += sy;
    }
  }
}

void draw_circle(int x0, int y0, int radius, uint32_t color) {
  int x = radius;
  int y = 0;
  int err = 0;

  while (x >= y) {
    blend_pixel(x0 + x, y0 + y, color);
    blend_pixel(x0 + y, y0 + x, color);
    blend_pixel(x0 - y, y0 + x, color);
    blend_pixel(x0 - x, y0 + y, color);
    blend_pixel(x0 - x, y0 - y, color);
    blend_pixel(x0 - y, y0 - x, color);
    blend_pixel(x0 + y, y0 - x, color);
    blend_pixel(x0 + x, y0 - y, color);

    if (err <= 0) {
      y += 1;
      err += 2 * y + 1;
    }
    if (err > 0) {
      x -= 1;
      err -= 2 * x + 1;
    }
  }
}

void draw_filled_circle(int x0, int y0, int radius, uint32_t color) {
  int x = radius;
  int y = 0;
  int err = 0;

  while (x >= y) {
    // Draw horizontal lines to fill (using alpha blend)
    for (int i = x0 - x; i <= x0 + x; i++)
      blend_pixel(i, y0 + y, color);
    for (int i = x0 - x; i <= x0 + x; i++)
      blend_pixel(i, y0 - y, color);
    for (int i = x0 - y; i <= x0 + y; i++)
      blend_pixel(i, y0 + x, color);
    for (int i = x0 - y; i <= x0 + y; i++)
      blend_pixel(i, y0 - x, color);

    if (err <= 0) {
      y += 1;
      err += 2 * y + 1;
    }
    if (err > 0) {
      x -= 1;
      err -= 2 * x + 1;
    }
  }
}

void draw_rounded_rect(int x, int y, int w, int h, int r, uint32_t color) {
  // Ensure radius isn't too big
  if (r > w / 2)
    r = w / 2;
  if (r > h / 2)
    r = h / 2;

  // Draw centers
  draw_rect_alpha(x + r, y, w - 2 * r, h, color);
  draw_rect_alpha(x, y + r, r, h - 2 * r, color);
  draw_rect_alpha(x + w - r, y + r, r, h - 2 * r, color);

  // Draw corners
  int cx = r;
  int cy = 0;
  int err = 0;

  while (cx >= cy) {
    // Top Left
    for (int i = x + r - cx; i <= x + r + cx; i++)
      blend_pixel(i, y + r - cy, color); // Wrong fill logic for corners?
    // Actually, just drawing filled circles at corners is easiest if we already
    // have rects. But let's use the optimized filled circle quadrant approach.

    // Let's just use draw_filled_circle for corners?
    // Overdraw is fine for now on software render if it simplifies code.

    // But to be precise:
    // We need 4 filled quadrants.
    // Top-Left: center (x+r, y+r)
    // Top-Right: center (x+w-r, y+r)
    // Bottom-Right: center (x+w-r, y+h-r)
    // Bottom-Left: center (x+r, y+h-r)

    cy++;
  }

  // Simpler approach:
  draw_filled_circle(x + r, y + r, r, color);                 // TL
  draw_filled_circle(x + w - r - 1, y + r, r, color);         // TR
  draw_filled_circle(x + r, y + h - r - 1, r, color);         // BL
  draw_filled_circle(x + w - r - 1, y + h - r - 1, r, color); // BR

  // Fill core again to cover gaps?
  // Filled circle covers the square centered at it... wait.
  // draw_filled_circle draws a circle.
  // The rects above cover the cross shape.
  // The circles cover the corners.
  // Yes this works.
}

void gfx_clear_screen(uint32_t color) {
  // serial_log("GRAPHICS: Clearing Screen...");
  for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) {
    screen_buffer[i] = color;
  }
  // serial_log("GRAPHICS: Cleared.");
}

// Draw char using 8x8 font
void draw_char(int x, int y, char c, uint32_t color) {
  if (c < 0 || c > 127)
    return;

  // Get pointer to char data
  const uint8_t *char_data = font8x8_basic[(int)c];

  for (int col = 0; col < 8; col++) {
    for (int row = 0; row < 8; row++) {
      if (char_data[row] & (1 << (7 - col))) {
        put_pixel(x + col, y + row, color);
      }
    }
  }
}

// Scaled Char
void draw_char_scaled(int x, int y, char c, uint32_t color, int scale) {
  if (c < 0 || c > 127)
    return;

  const uint8_t *char_data = font8x8_basic[(int)c];

  for (int col = 0; col < 8; col++) {
    for (int row = 0; row < 8; row++) {
      if (char_data[row] & (1 << (7 - col))) {
        // Draw scale x scale rect instead of 1 pixel
        draw_rect(x + (col * scale), y + (row * scale), scale, scale, color);
      }
    }
  }
}

void draw_string(int x, int y, const char *str, uint32_t color) {
  draw_string_scaled(x, y, str, color, 1);
}

void draw_string_scaled(int x, int y, const char *str, uint32_t color,
                        int scale) {
  int cursor_x = x;
  int cursor_y = y;
  for (int i = 0; i < strlen(str); i++) {
    draw_char_scaled(cursor_x, cursor_y, str[i], color, scale);
    cursor_x += (8 * scale);
  }
}

// Gradient (Horizontal)
void draw_rect_gradient(int x, int y, int w, int h, uint32_t c1, uint32_t c2) {
  uint8_t r1 = (c1 >> 16) & 0xFF;
  uint8_t g1 = (c1 >> 8) & 0xFF;
  uint8_t b1 = c1 & 0xFF;

  uint8_t r2 = (c2 >> 16) & 0xFF;
  uint8_t g2 = (c2 >> 8) & 0xFF;
  uint8_t b2 = c2 & 0xFF;

  for (int i = 0; i < w; i++) {
    // Linear Interpolation
    uint8_t r = r1 + (r2 - r1) * i / w;
    uint8_t g = g1 + (g2 - g1) * i / w;
    uint8_t b = b1 + (b2 - b1) * i / w;

    uint32_t color = 0xFF000000 | (r << 16) | (g << 8) | b;

    // Draw vertical slice
    for (int j = 0; j < h; j++) {
      put_pixel(x + i, y + j, color);
    }
  }
}

void draw_circle_filled(int x0, int y0, int radius, uint32_t color) {
  int x = radius;
  int y = 0;
  int err = 0;

  while (x >= y) {
    // Draw horizontal lines to fill (Scanline algorithm)
    draw_line(x0 - x, y0 + y, x0 + x, y0 + y, color);
    draw_line(x0 - x, y0 - y, x0 + x, y0 - y, color);
    draw_line(x0 - y, y0 + x, x0 + y, y0 + x, color);
    draw_line(x0 - y, y0 - x, x0 + y, y0 - x, color);

    if (err <= 0) {
      y += 1;
      err += 2 * y + 1;
    }

    if (err > 0) {
      x -= 1;
      err -= 2 * x + 1;
    }
  }
}

// Retro Pixel Primitives
void draw_pixel_grid(uint32_t bg_color, uint32_t grid_color, int spacing) {
  for (int y = 0; y < SCREEN_HEIGHT; y++) {
    for (int x = 0; x < SCREEN_WIDTH; x++) {
      if (x % spacing == 0 || y % spacing == 0) {
        put_pixel(x, y, grid_color);
      } else {
        put_pixel(x, y, bg_color);
      }
    }
  }
}

void draw_pixel_box(int x, int y, int w, int h, uint32_t bg_color) {
  // Main background
  draw_rect(x, y, w, h, bg_color);
  // Chunky 4px Border
  // Top
  draw_rect(x, y, w, 4, PIXEL_BLACK);
  // Bottom
  draw_rect(x, y + h - 4, w, 4, PIXEL_BLACK);
  // Left
  draw_rect(x, y, 4, h, PIXEL_BLACK);
  // Right
  draw_rect(x + w - 4, y, 4, h, PIXEL_BLACK);
}

void draw_thick_line(int x1, int y1, int x2, int y2, int thickness,
                     uint32_t color) {
  for (int i = 0; i < thickness; i++) {
    draw_line(x1 + i, y1, x2 + i, y2, color);
    draw_line(x1, y1 + i, x2, y2 + i, color);
  }
}

void draw_bitmap(int x, int y, int w, int h, const uint32_t *data) {
  for (int i = 0; i < h; i++) {
    for (int j = 0; j < w; j++) {
      uint32_t color = data[i * w + j];
      if ((color >> 24) & 0xFF) { // Alpha check
        put_pixel(x + j, y + i, color);
      }
    }
  }
}
