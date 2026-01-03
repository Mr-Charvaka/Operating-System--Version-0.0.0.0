#include "../include/Std/Types.h"
#include "../include/string.h"
#include "TitanUI.h"

namespace TitanUI {

static bool isdigit(char c) { return c >= '0' && c <= '9'; }
static bool isalnum(char c) {
  return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
         (c >= 'A' && c <= 'Z');
}

class GMLParser {
public:
  const char *m_data;
  int m_pos{0};

  GMLParser(const char *data) : m_data(data) {}

  void skip_whitespace() {
    while (m_data[m_pos] && (m_data[m_pos] == ' ' || m_data[m_pos] == '\n' ||
                             m_data[m_pos] == '\t' || m_data[m_pos] == '\r')) {
      m_pos++;
    }
  }

  char peek() { return m_data[m_pos]; }
  char consume() { return m_data[m_pos++]; }

  bool consume_string(const char *str) {
    int len = strlen(str);
    if (strncmp(m_data + m_pos, str, len) == 0) {
      m_pos += len;
      return true;
    }
    return false;
  }

  void parse_identifier(char *buf) {
    int i = 0;
    while (isalnum(peek()) || peek() == '_' || peek() == ':') {
      buf[i++] = consume();
    }
    buf[i] = 0;
  }

  Component *parse_component() {
    skip_whitespace();
    if (consume() != '@')
      return nullptr;

    char name[64];
    parse_identifier(name);

    Component *comp = nullptr;
    if (strcmp(name, "VerticalBoxLayout") == 0)
      comp = new VerticalBoxLayout();
    else if (strcmp(name, "HorizontalBoxLayout") == 0)
      comp = new HorizontalBoxLayout();
    else if (strcmp(name, "Label") == 0)
      comp = new Label("");
    else if (strcmp(name, "Button") == 0)
      comp = new Button();
    else if (strcmp(name, "ProgressBar") == 0)
      comp = new ProgressBar();
    else if (strcmp(name, "TextBox") == 0)
      comp = new TextBox();
    else if (strcmp(name, "Separator") == 0)
      comp = new Separator();
    else if (strcmp(name, "CheckBox") == 0)
      comp = new CheckBox("");
    else if (strcmp(name, "RadioButton") == 0)
      comp = new RadioButton("");
    else if (strcmp(name, "GroupBox") == 0)
      comp = new GroupBox("");
    else if (strcmp(name, "ListView") == 0)
      comp = new ListView();
    else if (strcmp(name, "ScrollBar") == 0)
      comp = new ScrollBar();
    else if (strcmp(name, "Slider") == 0)
      comp = new Slider();
    else if (strcmp(name, "Spinner") == 0)
      comp = new Spinner();
    else if (strcmp(name, "StatusBar") == 0)
      comp = new StatusBar();
    else if (strcmp(name, "TabWidget") == 0)
      comp = new TabWidget();
    else if (strcmp(name, "TableView") == 0)
      comp = new TableView();
    else if (strcmp(name, "TreeView") == 0)
      comp = new TreeView();
    else if (strcmp(name, "AnalogClock") == 0)
      comp = new AnalogClock();
    else
      comp = new Component();

    skip_whitespace();
    if (consume() != '{')
      return comp;

    while (true) {
      if (peek() == '}') {
        consume();
        break;
      }
      if (peek() == '@') {
        Component *child = parse_component();
        if (child)
          comp->add_child(child);
        continue;
      }
      char prop[64];
      parse_identifier(prop);
      skip_whitespace();
      if (consume() != ':')
        break;
      skip_whitespace();

      if (peek() == '"') {
        consume();
        char value[128];
        int i = 0;
        while (peek() != '"')
          value[i++] = consume();
        value[i] = 0;
        consume();
        if (strcmp(prop, "text") == 0) {
          if (comp->type() == WidgetType::Label)
            strcpy(((Label *)comp)->text, value);
        } else if (strcmp(prop, "label") == 0) {
          if (comp->type() == WidgetType::CheckBox)
            strcpy(((CheckBox *)comp)->label, value);
          else if (comp->type() == WidgetType::RadioButton)
            strcpy(((RadioButton *)comp)->label, value);
        } else if (strcmp(prop, "title") == 0) {
          if (comp->type() == WidgetType::GroupBox)
            strcpy(((GroupBox *)comp)->title, value);
        } else if (strcmp(prop, "tab") == 0) {
          if (comp->type() == WidgetType::TabWidget)
            ((TabWidget *)comp)->add_tab(value);
        }
      } else {
        uint32_t val = 0;
        if (peek() == '0' && m_data[m_pos + 1] == 'x') {
          m_pos += 2;
          while (isalnum(peek())) {
            char c = consume();
            if (isdigit(c))
              val = val * 16 + (c - '0');
            else if (c >= 'A' && c <= 'F')
              val = val * 16 + (c - 'A' + 10);
            else if (c >= 'a' && c <= 'f')
              val = val * 16 + (c - 'a' + 10);
          }
        } else {
          while (isdigit(peek()))
            val = val * 10 + (consume() - '0');
        }
        if (strcmp(prop, "width") == 0)
          comp->width = val;
        else if (strcmp(prop, "height") == 0)
          comp->height = val;
        else if (strcmp(prop, "value") == 0) {
          if (comp->type() == WidgetType::ProgressBar)
            ((ProgressBar *)comp)->value = val;
          else if (comp->type() == WidgetType::Slider)
            ((Slider *)comp)->value = val;
          else if (comp->type() == WidgetType::Spinner)
            ((Spinner *)comp)->value = val;
          else if (comp->type() == WidgetType::ScrollBar)
            ((ScrollBar *)comp)->value = val;
        } else if (strcmp(prop, "min") == 0) {
          if (comp->type() == WidgetType::Slider)
            ((Slider *)comp)->min = val;
        } else if (strcmp(prop, "max") == 0) {
          if (comp->type() == WidgetType::ProgressBar)
            ((ProgressBar *)comp)->max = val;
          else if (comp->type() == WidgetType::Slider)
            ((Slider *)comp)->max = val;
          else if (comp->type() == WidgetType::ScrollBar)
            ((ScrollBar *)comp)->max = val;
        } else if (strcmp(prop, "spacing") == 0) {
          if (comp->type() == WidgetType::BoxLayout)
            ((BoxLayout *)comp)->spacing = val;
        } else if (strcmp(prop, "orientation") == 0) {
          Orientation o =
              (val == 0) ? Orientation::Horizontal : Orientation::Vertical;
          if (comp->type() == WidgetType::BoxLayout)
            ((BoxLayout *)comp)->orientation = o;
          else if (comp->type() == WidgetType::Separator)
            ((Separator *)comp)->orientation = o;
          else if (comp->type() == WidgetType::ScrollBar)
            ((ScrollBar *)comp)->orientation = o;
        } else if (strcmp(prop, "backgroundColor") == 0)
          comp->style.backgroundColor = val;
        else if (strcmp(prop, "color") == 0) {
          if (comp->type() == WidgetType::Label)
            ((Label *)comp)->color = val;
        }
      }
      skip_whitespace();
    }
    return comp;
  }
};

Component *parse_gml(const char *gml) {
  GMLParser parser(gml);
  return parser.parse_component();
}

} // namespace TitanUI
