#include "TitanUI.h"
#include "../include/string.h"
#include "TitanAssets.h"

extern "C" void serial_log(const char *msg);

namespace TitanUI {

// Component
Component::Component() {
  magic = MAGIC_VALUE;
  x = 0;
  y = 0;
  width = 0;
  height = 0;
  parent = nullptr;
  childCount = 0;
  for (int i = 0; i < MAX_CHILDREN; i++)
    children[i] = nullptr;
  // Initialize style with defaults (struct default ctor works)
}

Component::~Component() {
  magic = 0;
  for (int i = 0; i < childCount; i++) {
    if (children[i]) {
      delete children[i];
      children[i] = nullptr;
    }
  }
}

WidgetType Component::type() const { return WidgetType::Component; }

void Component::add_child(Component *child) {
  if (childCount < MAX_CHILDREN && child) {
    child->parent = this;
    children[childCount++] = child;
  }
}

int Component::get_absolute_x() {
  if (parent)
    return parent->get_absolute_x() + x;
  return x;
}

int Component::get_absolute_y() {
  if (parent)
    return parent->get_absolute_y() + y;
  return y;
}

void Component::render() {
  if (magic != MAGIC_VALUE) {
    serial_log("[TitanUI] Component corruption detected!");
    return;
  }
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

  for (int i = 0; i < childCount; i++) {
    if (children[i])
      children[i]->render();
  }
}

void Component::handle_event(EventType type, int arg1, int arg2) {
  if (magic != MAGIC_VALUE)
    return;
  for (int i = childCount - 1; i >= 0; i--) {
    if (children[i])
      children[i]->handle_event(type, arg1, arg2);
  }
}

void Component::render_glass(int x, int y, int w, int h, int r) {
  uint32_t glassFill = 0x40FFFFFF;
  draw_rounded_rect(x, y, w, h, r, glassFill);
}

void Component::render_neopop(int x, int y, int w, int h, uint32_t color,
                              bool pressed) {
  int offset = pressed ? 3 : 0;
  if (!pressed) {
    draw_rect(x + w, y + 3, 3, h, 0xFF000000);
    draw_rect(x + 3, y + h, w, 3, 0xFF000000);
    draw_rect(x + w, y + h, 3, 3, 0xFF000000);
  }
  int sx = x + offset;
  int sy = y + offset;
  draw_rect(sx, sy, w, h, color);
  uint32_t borderColor = 0xFF000000;
  draw_rect(sx, sy, w, 2, borderColor);         // Top
  draw_rect(sx, sy + h - 2, w, 2, borderColor); // Bottom
  draw_rect(sx, sy, 2, h, borderColor);         // Left
  draw_rect(sx + w - 2, sy, 2, h, borderColor); // Right
}

// Label
Label::Label(const char *t) : Component() {
  strcpy(text, t);
  color = 0xFF000000;
  scale = 1;
}
WidgetType Label::type() const { return WidgetType::Label; }
void Label::render() {
  if (!style.visible)
    return;
  Component::render();
  draw_string_scaled(get_absolute_x(), get_absolute_y(), text, color, scale);
}

// Button
Button::Button() : Component() {
  hovered = false;
  isPressed = false;
  onClick = nullptr;
  text[0] = 0;
}
WidgetType Button::type() const { return WidgetType::Button; }
void Button::render() {
  if (!style.visible)
    return;

  int absX = get_absolute_x();
  int absY = get_absolute_y();

  if (style.neoPop) {
    uint32_t bg = style.backgroundColor ? style.backgroundColor : 0xFFFFFFFF;
    render_neopop(absX, absY, width, height, bg, isPressed);
  } else {
    if (style.backgroundColor != 0) {
      uint32_t bgColor = hovered ? 0xFFE0E0E0 : style.backgroundColor;
      draw_rect(absX, absY, width, height, bgColor);
    }
  }
  if (text[0]) {
    draw_string(absX + 5, absY + 5, text, 0xFF000000);
  }
  for (int i = 0; i < childCount; i++) {
    if (children[i])
      children[i]->render();
  }
}
void Button::handle_event(EventType t, int mx, int my) {
  if (magic != MAGIC_VALUE)
    return;
  int absX = get_absolute_x();
  int absY = get_absolute_y();
  bool hit =
      (mx >= absX && mx < absX + width && my >= absY && my < absY + height);

  if (t == EventType::MouseMove) {
    hovered = hit;
  }
  if (t == EventType::MouseClick) {
    if (hit) {
      isPressed = true;
      if (onClick)
        onClick();
      // Reset pressed after short delay? Typically done by release event.
      // For now we just flash it or leave it.
    }
  }
  Component::handle_event(t, mx, my);
}

// AppLaunchButton
WidgetType AppLaunchButton::type() const { return WidgetType::Button; }
void AppLaunchButton::handle_event(EventType t, int mx, int my) {
  if (t == EventType::MouseClick) {
    int absX = get_absolute_x();
    int absY = get_absolute_y();
    if (mx >= absX && mx < absX + width && my >= absY && my < absY + height) {
      launch_app(target);
    }
  }
  Button::handle_event(t, mx, my);
}

// VectorIcon
VectorIcon::VectorIcon(int t, int w, int h) : Component(), iconType(t) {
  width = w;
  height = h;
  style.backgroundColor = 0;
}
WidgetType VectorIcon::type() const { return WidgetType::AssetView; }
void VectorIcon::render() {
  int ax = get_absolute_x();
  int ay = get_absolute_y();
  if (style.backgroundColor)
    draw_rect(ax, ay, width, height, style.backgroundColor);

  if (iconType == 0) { // File Manager
    uint32_t blue = 0xFF3D5AFE;
    draw_rect(ax, ay + 2, width / 2, 10, blue);
    draw_rect(ax, ay + 8, width, height - 8, blue);
    draw_rect(ax + 5, ay + 15, width - 10, height - 20, 0xFFFFFFFF);
  } else if (iconType == 1) { // Terminal
    draw_rect(ax, ay, width, height, 0xFF212121);
    draw_rect(ax, ay, width, 12, 0xFFEEEEEE);
  } else if (iconType == 2) { // Settings
    draw_circle(ax + width / 2, ay + height / 2, width / 2 - 2, 0xFF757575);
    draw_circle(ax + width / 2, ay + height / 2, width / 4, 0xFFFFFFFF);
  } else if (iconType == 3) { // Calculator
    uint32_t orange = 0xFFFF9800;
    draw_rect(ax, ay, width, height, orange);
    draw_rect(ax + 5, ay + 5, width - 10, 10, 0xFFFFFFFF);
    for (int i = 0; i < 3; i++)
      for (int j = 0; j < 2; j++)
        draw_rect(ax + 5 + i * 10, ay + 20 + j * 10, 8, 8, 0xFFFFFFFF);
  } else if (iconType == 4) { // Browser
    draw_circle(ax + width / 2, ay + height / 2, width / 2 - 2, 0xFF03A9F4);
    draw_line(ax + width / 2, ay + 2, ax + width / 2, ay + height - 2,
              0xFFFFFFFF);
    draw_line(ax + 2, ay + height / 2, ax + width - 2, ay + height / 2,
              0xFFFFFFFF);
  }
}

// ProgressBar
WidgetType ProgressBar::type() const { return WidgetType::ProgressBar; }
void ProgressBar::render() {
  if (!style.visible)
    return;
  int ax = get_absolute_x(), ay = get_absolute_y();
  draw_rect(ax, ay, width, height, 0xFF303030);
  draw_rect(ax, ay, width, 1, 0);
  draw_rect(ax, ay + height - 1, width, 1, 0);
  draw_rect(ax, ay, 1, height, 0);
  draw_rect(ax + width - 1, ay, 1, height, 0);
  if (max > 0) {
    int fw = (value * (width - 2)) / max;
    if (fw > 0)
      draw_rect(ax + 1, ay + 1, fw, height - 2, progressColor);
  }
}

// Separator
WidgetType Separator::type() const { return WidgetType::Separator; }
void Separator::render() {
  if (!style.visible)
    return;
  int ax = get_absolute_x(), ay = get_absolute_y();
  if (orientation == Orientation::Horizontal) {
    draw_rect(ax, ay, width, 1, 0xFF808080);
    draw_rect(ax, ay + 1, width, 1, 0xFFFFFFFF);
  } else {
    draw_rect(ax, ay, 1, height, 0xFF808080);
    draw_rect(ax + 1, ay, 1, height, 0xFFFFFFFF);
  }
}

// BoxLayout
BoxLayout::BoxLayout(Orientation o) : Component(), orientation(o), spacing(5) {}
WidgetType BoxLayout::type() const { return WidgetType::BoxLayout; }
void BoxLayout::perform_layout() {
  int cur = 0;
  for (int i = 0; i < childCount; i++) {
    if (!children[i])
      continue;
    if (orientation == Orientation::Vertical) {
      children[i]->y = cur;
      children[i]->x = 0;
      children[i]->width = width;
      cur += children[i]->height + spacing;
    } else {
      children[i]->x = cur;
      children[i]->y = 0;
      children[i]->height = height;
      cur += children[i]->width + spacing;
    }
    if (children[i]->type() == WidgetType::BoxLayout)
      ((BoxLayout *)children[i])->perform_layout();
  }
}
void BoxLayout::render() {
  perform_layout();
  Component::render();
}
VerticalBoxLayout::VerticalBoxLayout() : BoxLayout(Orientation::Vertical) {}
HorizontalBoxLayout::HorizontalBoxLayout()
    : BoxLayout(Orientation::Horizontal) {}

// CheckBox
CheckBox::CheckBox(const char *l) : Component() {
  strcpy(label, l);
  width = 120;
  height = 24;
  checked = false;
}
WidgetType CheckBox::type() const { return WidgetType::CheckBox; }
void CheckBox::render() {
  if (!style.visible)
    return;
  int ax = get_absolute_x(), ay = get_absolute_y();
  draw_rect(ax, ay + 2, 20, 20, 0xFFFFFFFF);
  draw_rect(ax, ay + 2, 20, 1, 0);
  draw_rect(ax, ay + 21, 20, 1, 0);
  draw_rect(ax, ay + 2, 1, 20, 0);
  draw_rect(ax + 19, ay + 2, 1, 20, 0);
  if (checked) {
    draw_line(ax + 4, ay + 6, ax + 16, ay + 18, 0);
    draw_line(ax + 16, ay + 6, ax + 4, ay + 18, 0);
  }
  draw_string(ax + 25, ay + 6, label, 0);
}
void CheckBox::handle_event(EventType t, int a1, int a2) {
  if (t == EventType::MouseClick) {
    int ax = get_absolute_x(), ay = get_absolute_y();
    if (a1 >= ax && a1 < ax + width && a2 >= ay && a2 < ay + height)
      checked = !checked;
  }
  Component::handle_event(t, a1, a2);
}

// RadioButton
RadioButton::RadioButton(const char *l) : Component() {
  strcpy(label, l);
  width = 120;
  height = 24;
  selected = false;
}
WidgetType RadioButton::type() const { return WidgetType::RadioButton; }
void RadioButton::render() {
  if (!style.visible)
    return;
  int ax = get_absolute_x(), ay = get_absolute_y();
  draw_rect(ax + 4, ay + 4, 12, 12, 0xFFFFFFFF);
  if (selected)
    draw_rect(ax + 8, ay + 8, 4, 4, 0);
  draw_string(ax + 25, ay + 6, label, 0);
}

// GroupBox
GroupBox::GroupBox(const char *t) : Component() { strcpy(title, t); }
WidgetType GroupBox::type() const { return WidgetType::GroupBox; }
void GroupBox::render() {
  int ax = get_absolute_x(), ay = get_absolute_y();
  draw_rect(ax, ay + 10, width, 1, 0);
  draw_rect(ax, ay + height - 1, width, 1, 0);
  draw_rect(ax, ay + 10, 1, height - 10, 0);
  draw_rect(ax + width - 1, ay + 10, 1, height - 10, 0);
  draw_string(ax + 10, ay, title, 0);
  Component::render();
}

// ListView
ListView::ListView() : Component() {
  itemCount = 0;
  selectedIndex = -1;
}
WidgetType ListView::type() const { return WidgetType::ListView; }
void ListView::add_item(const char *s) {
  if (itemCount < 10)
    strcpy(items[itemCount++], s);
}
void ListView::render() {
  Component::render();
  int ax = get_absolute_x(), ay = get_absolute_y();
  draw_rect(ax, ay, width, height, 0xFFFFFFFF);
  for (int i = 0; i < itemCount; i++) {
    if (i == selectedIndex)
      draw_rect(ax, ay + i * 22, width, 22, 0xFF0000FF);
    draw_string(ax + 4, ay + i * 22 + 4, items[i],
                (i == selectedIndex) ? 0xFFFFFFFF : 0);
  }
}

// ScrollBar
ScrollBar::ScrollBar() : Component() {}
WidgetType ScrollBar::type() const { return WidgetType::ScrollBar; }
void ScrollBar::render() {
  int ax = get_absolute_x(), ay = get_absolute_y();
  draw_rect(ax, ay, width, height, 0xFFC0C0C0);
}

// Slider
Slider::Slider() : Component() {}
WidgetType Slider::type() const { return WidgetType::Slider; }
void Slider::render() {
  int ax = get_absolute_x(), ay = get_absolute_y();
  draw_rect(ax, ay + height / 2 - 2, width, 4, 0xFF808080);
  int tx = ax + (value * (width - 12)) / max;
  draw_rect(tx, ay, 12, height, 0xFFE0E0E0);
}

// Spinner
Spinner::Spinner() : Component() {
  width = 60;
  height = 24;
  value = 0;
}
WidgetType Spinner::type() const { return WidgetType::Spinner; }
void Spinner::render() {
  int ax = get_absolute_x(), ay = get_absolute_y();
  draw_rect(ax, ay, width, height, 0xFFFFFFFF);
  char buf[16];
  itoa(value, buf, 10);
  draw_string(ax + 4, ay + 4, buf, 0);
}

// StatusBar
StatusBar::StatusBar() : Component() {
  height = 22;
  strcpy(text, "Ready");
}
WidgetType StatusBar::type() const { return WidgetType::StatusBar; }
void StatusBar::render() {
  int ax = get_absolute_x(), ay = get_absolute_y();
  draw_rect(ax, ay, width, height, 0xFFC0C0C0);
  draw_string(ax + 5, ay + 4, text, 0);
}

// TabWidget
TabWidget::TabWidget() : Component() {
  tabCount = 0;
  activeTab = 0;
}
void TabWidget::add_tab(const char *l) {
  if (tabCount < 4)
    strcpy(tabLabels[tabCount++], l);
}
WidgetType TabWidget::type() const { return WidgetType::TabWidget; }
void TabWidget::render() {
  int ax = get_absolute_x(), ay = get_absolute_y();
  for (int i = 0; i < tabCount; i++) {
    draw_rect(ax + i * 80, ay, 78, 24,
              (i == activeTab) ? 0xFFFFFFFF : 0xFFC0C0C0);
    draw_string(ax + i * 80 + 5, ay + 5, tabLabels[i], 0);
  }
  draw_rect(ax, ay + 24, width, height - 24, 0xFFFFFFFF);
}

// TableView & TreeView
TableView::TableView() : Component() {}
WidgetType TableView::type() const { return WidgetType::TableView; }
void TableView::render() {
  draw_rect(get_absolute_x(), get_absolute_y(), width, height, 0xFFFFFFFF);
}

TreeView::TreeView() : Component() {}
WidgetType TreeView::type() const { return WidgetType::TreeView; }
void TreeView::render() {
  draw_rect(get_absolute_x(), get_absolute_y(), width, height, 0xFFFFFFFF);
}

// AnalogClock
AnalogClock::AnalogClock() : Component() {}
WidgetType AnalogClock::type() const { return WidgetType::AnalogClock; }
void AnalogClock::render() {
  int ax = get_absolute_x(), ay = get_absolute_y();
  draw_circle(ax + width / 2, ay + height / 2, width / 2 - 5, 0xFFFFFFFF);
  draw_line(ax + width / 2, ay + height / 2, ax + width / 2, ay + 10, 0);
}

// IconView
IconView::IconView() : Component() { iconCount = 0; }
void IconView::add_icon(const char *l, const uint32_t *d) {
  if (iconCount < 8) {
    strcpy(icons[iconCount].label, l);
    icons[iconCount].data = d;
    iconCount++;
  }
}
WidgetType IconView::type() const { return WidgetType::IconView; }
void IconView::render() {
  int ax = get_absolute_x(), ay = get_absolute_y();
  for (int i = 0; i < iconCount; i++) {
    int ix = ax + (i % 4) * 80;
    int iy = ay + (i / 4) * 80;
    if (icons[i].data)
      draw_bitmap(ix + 16, iy + 10, 48, 48, icons[i].data);
    else
      draw_rect(ix + 16, iy + 10, 48, 48, 0xFFCCCCCC);
    draw_string(ix + 5, iy + 60, icons[i].label, 0xFFFFFFFF);
  }
}

// Calendar
Calendar::Calendar() : Component() {}
WidgetType Calendar::type() const { return WidgetType::Calendar; }
void Calendar::render() {
  draw_rect(get_absolute_x(), get_absolute_y(), width, height, 0xFFFFFFFF);
  draw_string(get_absolute_x(), get_absolute_y(), "Cal", 0);
}

// ColorPicker
ColorPicker::ColorPicker() : Component() {}
WidgetType ColorPicker::type() const { return WidgetType::ColorPicker; }
void ColorPicker::render() {
  int ax = get_absolute_x(), ay = get_absolute_y();
  for (int i = 0; i < 8; i++)
    draw_rect(ax + i * 20, ay, 18, 18, i * 0x222222);
}

// Link
Link::Link() : Component() {}
WidgetType Link::type() const { return WidgetType::Link; }
void Link::render() {
  draw_string(get_absolute_x(), get_absolute_y(), text, 0xFF0000FF);
}

// ComboBox
ComboBox::ComboBox() : Component() {}
WidgetType ComboBox::type() const { return WidgetType::ComboBox; }
void ComboBox::render() {
  int ax = get_absolute_x(), ay = get_absolute_y();
  draw_rect(ax, ay, width, height, 0xFFFFFFFF);
  draw_string(ax + 2, ay + 4, selectedItem, 0);
}

// Image
Image::Image() : Component() {}
WidgetType Image::type() const { return WidgetType::Image; }
void Image::render() {
  if (style.visible && data)
    draw_bitmap(get_absolute_x(), get_absolute_y(), width, height, data);
}

// TerminalWidget
TerminalWidget::TerminalWidget() : Component() {}
WidgetType TerminalWidget::type() const { return WidgetType::TerminalWidget; }
void TerminalWidget::render() {
  int ax = get_absolute_x(), ay = get_absolute_y();
  draw_rect(ax, ay, width, height, 0);
  draw_string(ax + 5, ay + 5, ">_", 0xFF00FF00);
}

// TextBox
TextBox::TextBox() : Component() { memset(text, 0, 128); }
WidgetType TextBox::type() const { return WidgetType::TextBox; }
void TextBox::render() {
  int ax = get_absolute_x(), ay = get_absolute_y();
  draw_rect(ax, ay, width, height, 0xFFFFFFFF);
  draw_string(ax + 4, ay + 4, text, 0);
}

} // namespace TitanUI
