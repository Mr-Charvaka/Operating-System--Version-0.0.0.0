#ifndef TITAN_UI_H
#define TITAN_UI_H

#include "../drivers/graphics.h"
#include "../include/Std/Types.h"
#include "../include/string.h"

// Forward declaration for launch_app used by AppLaunchButton
extern "C" void launch_app(const char *name);

namespace TitanUI {

enum class DisplayType { Block, FlexRow, FlexColumn };
enum class Orientation { Horizontal, Vertical };
enum class EventType { MouseMove, MouseClick, KeyPress };
enum class WidgetType {
  Component,
  Label,
  Button,
  ProgressBar,
  Separator,
  BoxLayout,
  TextBox,
  AssetView,
  CheckBox,
  RadioButton,
  GroupBox,
  ListView,
  ScrollBar,
  Slider,
  Spinner,
  StatusBar,
  TabWidget,
  TableView,
  TreeView,
  AnalogClock,
  IconView,
  Calendar,
  ColorPicker,
  Link,
  ComboBox,
  Image,
  TerminalWidget
};

struct Style {
  uint32_t backgroundColor = 0x00000000;
  uint32_t borderColor = 0xFF000000;
  int borderRadius = 0;
  int padding = 0;
  int margin = 0;
  bool visible = true;
  float opacity = 1.0f;
  bool glassmorphism = false;
  bool neoPop = false;
};

class Component {
public:
  static const uint32_t MAGIC_VALUE = 0xDEADBEEF;
  uint32_t magic;
  int x, y, width, height;
  Style style;
  Component *parent;

  static const int MAX_CHILDREN = 32;
  Component *children[MAX_CHILDREN];
  int childCount;

  Component();
  virtual ~Component();

  virtual WidgetType type() const;
  virtual void render();
  virtual void handle_event(EventType type, int arg1, int arg2);

  void add_child(Component *child);
  int get_absolute_x();
  int get_absolute_y();

protected:
  void render_glass(int x, int y, int w, int h, int r);
  void render_neopop(int x, int y, int w, int h, uint32_t color,
                     bool pressed = false);
};

class Label : public Component {
public:
  char text[64];
  uint32_t color;
  int scale;
  Label(const char *t);
  WidgetType type() const override;
  void render() override;
};

class Button : public Component {
public:
  bool hovered;
  bool isPressed;
  char text[64];
  void (*onClick)();

  Button();
  WidgetType type() const override;
  void render() override;
  void handle_event(EventType type, int mx, int my) override;
};

class AppLaunchButton : public Button {
public:
  char target[32];
  WidgetType type() const override;
  void handle_event(EventType type, int mx, int my) override;
};

class VectorIcon : public Component {
public:
  int iconType;
  VectorIcon(int type, int w, int h);
  WidgetType type() const override;
  void render() override;
};

class ProgressBar : public Component {
public:
  int value = 0;
  int max = 100;
  uint32_t progressColor = 0xFF00FF00;
  WidgetType type() const override;
  void render() override;
};

class Separator : public Component {
public:
  Orientation orientation = Orientation::Horizontal;
  WidgetType type() const override;
  void render() override;
};

class BoxLayout : public Component {
public:
  Orientation orientation;
  int spacing = 5;
  BoxLayout(Orientation o);
  WidgetType type() const override;
  void perform_layout();
  void render() override;
};

class VerticalBoxLayout : public BoxLayout {
public:
  VerticalBoxLayout();
};

class HorizontalBoxLayout : public BoxLayout {
public:
  HorizontalBoxLayout();
};

class CheckBox : public Component {
public:
  bool checked = false;
  char label[64];
  CheckBox(const char *l);
  WidgetType type() const override;
  void render() override;
  void handle_event(EventType type, int arg1, int arg2) override;
};

class RadioButton : public Component {
public:
  bool selected = false;
  char label[64];
  RadioButton(const char *l);
  WidgetType type() const override;
  void render() override;
};

class GroupBox : public Component {
public:
  char title[64];
  GroupBox(const char *t);
  WidgetType type() const override;
  void render() override;
};

class ListView : public Component {
public:
  char items[10][32];
  int itemCount = 0;
  int selectedIndex = -1;
  ListView(); // Added
  WidgetType type() const override;
  void add_item(const char *s);
  void render() override;
};

class ScrollBar : public Component {
public:
  int value = 0;
  int max = 100;
  Orientation orientation = Orientation::Vertical;
  ScrollBar(); // Added
  WidgetType type() const override;
  void render() override;
};

class Slider : public Component {
public:
  int value = 50;
  int min = 0;
  int max = 100;
  Slider(); // Added
  WidgetType type() const override;
  void render() override;
};

class Spinner : public Component {
public:
  int value = 0;
  Spinner();
  WidgetType type() const override;
  void render() override;
};

class StatusBar : public Component {
public:
  char text[64];
  StatusBar();
  WidgetType type() const override;
  void render() override;
};

class TabWidget : public Component {
public:
  char tabLabels[4][32];
  int tabCount = 0;
  int activeTab = 0;
  TabWidget(); // Added
  void add_tab(const char *label);
  WidgetType type() const override;
  void render() override;
};

class TableView : public Component {
public:
  TableView(); // Added
  WidgetType type() const override;
  void render() override;
};

class TreeView : public Component {
public:
  TreeView(); // Added
  WidgetType type() const override;
  void render() override;
};

class AnalogClock : public Component {
public:
  AnalogClock(); // Added
  WidgetType type() const override;
  void render() override;
};

class IconView : public Component {
public:
  struct Icon {
    char label[32];
    const uint32_t *data;
  };
  Icon icons[8];
  int iconCount = 0;
  IconView(); // Added
  void add_icon(const char *label, const uint32_t *data);
  WidgetType type() const override;
  void render() override;
};

class Calendar : public Component {
public:
  Calendar(); // Added
  WidgetType type() const override;
  void render() override;
};

class ColorPicker : public Component {
public:
  ColorPicker(); // Added
  WidgetType type() const override;
  void render() override;
};

class Link : public Component {
public:
  char url[64];
  char text[64];
  Link(); // Added
  WidgetType type() const override;
  void render() override;
};

class ComboBox : public Component {
public:
  char selectedItem[32];
  ComboBox(); // Added
  WidgetType type() const override;
  void render() override;
};

class Image : public Component {
public:
  const uint32_t *data = nullptr;
  Image(); // Added
  WidgetType type() const override;
  void render() override;
};

class TerminalWidget : public Component {
public:
  TerminalWidget(); // Added
  WidgetType type() const override;
  void render() override;
};

class TextBox : public Component {
public:
  char text[128];
  TextBox();
  WidgetType type() const override;
  void render() override;
};

Component *parse_gml(const char *gml);

} // namespace TitanUI

#endif
