#ifndef TITAN_UI_H
#define TITAN_UI_H

#include "../drivers/graphics.h"
#include "../include/Std/Types.h"
#include "../include/string.h"
#include "TitanAssets.h"

namespace TitanUI {

enum class DisplayType { Block, FlexRow, FlexColumn };
enum class EventType { MouseMove, MouseClick, KeyPress };

struct Style {
  uint32_t backgroundColor = 0x00000000;
  uint32_t borderColor = 0xFF000000; // Black for NeoPop
  int borderRadius = 0;
  int padding = 0;
  int margin = 0;
  bool visible = true;
  float opacity = 1.0f;
  bool glassmorphism = false;
  bool neoPop = false; // Enable CRED NeoPop style (Plunk shadow)
};

class Component {
public:
  int x = 0, y = 0, width = 0, height = 0;
  Style style;
  Component *parent = nullptr;
  Component *children[16] = {nullptr};
  int childCount = 0;

  virtual ~Component() {}

  void add_child(Component *child) {
    if (childCount < 16) {
      child->parent = this;
      children[childCount++] = child;
    }
  }

  int get_absolute_x() { return (parent ? parent->get_absolute_x() : 0) + x; }
  int get_absolute_y() { return (parent ? parent->get_absolute_y() : 0) + y; }

  virtual void render() {
    if (!style.visible)
      return;

    int absX = get_absolute_x();
    int absY = get_absolute_y();

    if (style.neoPop) {
      render_neopop(absX, absY, width, height, style.backgroundColor);
    } else if (style.glassmorphism) {
      render_glass(absX, absY, width, height, style.borderRadius);
    } else if (style.backgroundColor != 0) {
      if (style.borderRadius > 0) {
        draw_rounded_rect(absX, absY, width, height, style.borderRadius,
                          style.backgroundColor);
      } else {
        draw_rect(absX, absY, width, height, style.backgroundColor);
      }
    }

    // Render Children
    for (int i = 0; i < childCount; i++) {
      if (children[i])
        children[i]->render();
    }
  }

  virtual void handle_event(EventType type, int arg1, int arg2) {
    for (int i = childCount - 1; i >= 0; i--) {
      if (children[i])
        children[i]->handle_event(type, arg1, arg2);
    }
  }

  void render_glass(int x, int y, int w, int h, int r) {
    uint32_t glassFill = 0x40FFFFFF;
    draw_rounded_rect(x, y, w, h, r, glassFill);
  }

  void render_neopop(int x, int y, int w, int h, uint32_t color,
                     bool pressed = false) {
    int offset = pressed ? 3 : 0;

    // NeoPop "Plunk" Effect (Shadow)
    if (!pressed) {
      // Shadow faces (Right and Bottom) for 3D effect
      // Right Face
      draw_rect(x + w, y + 3, 3, h, 0xFF000000);
      // Bottom Face
      draw_rect(x + 3, y + h, w, 3, 0xFF000000);
      // Corner Filler
      draw_rect(x + w, y + h, 3, 3, 0xFF000000);
    }

    // Main Surface (Translated if pressed)
    int sx = x + offset;
    int sy = y + offset;
    draw_rect(sx, sy, w, h, color);

    // Border - Solid Black 2px
    uint32_t borderColor = 0xFF000000;
    // Top
    draw_rect(sx, sy, w, 2, borderColor);
    // Bottom
    draw_rect(sx, sy + h - 2, w, 2, borderColor);
    // Left
    draw_rect(sx, sy, 2, h, borderColor);
    // Right
    draw_rect(sx + w - 2, sy, 2, h, borderColor);
  }
};

class Label : public Component {
public:
  char text[64];
  uint32_t color = COLOR_BLACK;
  int scale = 1;

  Label(const char *t) { strcpy(text, t); }

  void render() override {
    if (!style.visible)
      return;
    draw_string_scaled(get_absolute_x(), get_absolute_y(), text, color, scale);
  }
};

class Button : public Component {
public:
  bool hovered = false;
  bool isPressed = false;
  void (*onClick)() = nullptr;

  void render() override {
    if (!style.visible)
      return;

    int absX = get_absolute_x();
    int absY = get_absolute_y();

    if (style.neoPop) {
      uint32_t bg = style.backgroundColor ? style.backgroundColor : 0xFFFFFFFF;
      // Call parent's helper (now public/protected hopefully)
      // Since I'm editing the whole block, I'll make sure it's accessible.
      // Actually, the previous block made it private. I need to make sure I
      // REPLACE the private visibility label too if I can. Wait, replace target
      // includes "private:".

      ((Component *)this)
          ->render_neopop(absX, absY, width, height, bg, isPressed);

      // Render children (Naive: won't offset. Acceptable for now)
      for (int i = 0; i < childCount; i++) {
        if (children[i])
          children[i]->render();
      }
      return;
    }

    if (style.backgroundColor != 0) {
      uint32_t bgColor = hovered ? 0xFFE0E0E0 : style.backgroundColor;
      draw_rect(absX, absY, width, height, bgColor);
    }
    // Children
    for (int i = 0; i < childCount; i++) {
      if (children[i])
        children[i]->render();
    }
  }

  void handle_event(EventType type, int mx, int my) override {
    int absX = get_absolute_x();
    int absY = get_absolute_y();
    if (mx >= absX && mx < absX + width && my >= absY && my < absY + height) {
      if (type == EventType::MouseMove)
        hovered = true;
      if (type == EventType::MouseClick && onClick)
        onClick();
    } else {
      if (type == EventType::MouseMove)
        hovered = false;
    }
    Component::handle_event(type, mx, my);
  }
};

// New Premium Asset Component
class AssetView : public Component {
public:
  const uint32_t *data = nullptr;

  AssetView(const uint32_t *pixelData, int w, int h) {
    data = pixelData;
    width = w;
    height = h;
  }

  void render() override {
    if (!style.visible || !data)
      return;

    int absX = get_absolute_x();
    int absY = get_absolute_y();

    // Fast Pixel Blitting
    for (int i = 0; i < height; i++) {
      for (int j = 0; j < width; j++) {
        uint32_t color = data[i * width + j];
        // Simple Chroma Key for transparency (skip #000000 black if used as
        // background) In our Python script, we can improve this, but for now
        // just raw draw
        if (color != 0) { // Assuming 0 is transparent/black background
          put_pixel(absX + j, absY + i, color);
        }
      }
    }
  }
};

} // namespace TitanUI

#endif
