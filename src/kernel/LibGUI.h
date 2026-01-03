/*
 * LibGUI - Retro-OS Modern Widget System
 * Inspired by SerenityOS LibGUI/GML
 */
#ifndef LIBGUI_H
#define LIBGUI_H

#include "../include/types.h"
#include "gui.h"

namespace TitanUI {

enum class Orientation { Horizontal, Vertical };

class Widget {
public:
  Widget();
  virtual ~Widget();

  virtual void draw(int ox, int oy) = 0;
  virtual void handle_mouse(int mx, int my, bool click) {}
  virtual void handle_key(char c) {}

  void set_x(int x) { m_x = x; }
  void set_y(int y) { m_y = y; }
  void set_width(int w) { m_width = w; }
  void set_height(int h) { m_height = h; }

  int x() const { return m_x; }
  int y() const { return m_y; }
  int width() const { return m_width; }
  int height() const { return m_height; }

  void add_child(Widget *child) {
    if (m_child_count < 16) {
      m_children[m_child_count++] = child;
      child->m_parent = this;
    }
  }

protected:
  int m_x{0}, m_y{0};
  int m_width{100}, m_height{20};
  Widget *m_parent{nullptr};
  Widget *m_children[16];
  int m_child_count{0};
};

class Label : public Widget {
public:
  Label(const char *text);
  void draw(int ox, int oy) override;
  void set_text(const char *text);

private:
  char m_text[64];
};

class Button : public Widget {
public:
  Button(const char *text);
  void draw(int ox, int oy) override;
  void handle_mouse(int mx, int my, bool click) override;
  void set_on_click(void (*callback)()) { m_on_click = callback; }

private:
  char m_text[32];
  bool m_hovered{false};
  void (*m_on_click)(){nullptr};
};

class TextBox : public Widget {
public:
  TextBox();
  void draw(int ox, int oy) override;
  void handle_key(char c) override;
  const char *text() const { return m_buffer; }

private:
  char m_buffer[128];
  int m_len{0};
  bool m_focused{false};
};

class ProgressBar : public Widget {
public:
  ProgressBar();
  void draw(int ox, int oy) override;
  void set_value(int v) {
    m_value = v;
    if (m_value > 100)
      m_value = 100;
  }

private:
  int m_value{0};
};

class BoxLayout : public Widget {
public:
  BoxLayout(Orientation orientation);
  void draw(int ox, int oy) override;
  void perform_layout();

private:
  Orientation m_orientation;
};

// GML Parser - Takes a GML string and returns a root Widget
Widget *parse_gml(const char *gml);

} // namespace TitanUI

#endif
