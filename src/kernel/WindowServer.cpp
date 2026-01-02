#include "../drivers/bmp.h"
#include "../drivers/graphics.h"
#include "../drivers/rtc.h"
#include "../drivers/serial.h"
#include "../include/Std/Types.h"
#include "../include/string.h"
#include "../include/vfs.h"
#include "../kernel/TitanUI.h"
#include "../kernel/gui.h"
#include "../kernel/memory.h"
#include "../kernel/process.h"
#include "TitanAssets.h"

using namespace TitanUI;

extern "C" int fat16_create_file(const char *filename);
extern "C" int fat16_write_file(const char *filename, uint8_t *data,
                                uint32_t size);
extern "C" int fat16_mkdir(const char *name);
extern "C" int fat16_delete_file(const char *filename);
extern "C" void fat16_get_stats_bytes(uint32_t *total, uint32_t *free);

// Forward declarations for C++ internal usage
void draw_desktop();
void swap_buffers();
void draw_context_menu();
void handle_menu_action(int action_id);
void draw_window(window_t *win);
void save_mouse_bg(int x, int y);
void restore_mouse_bg(int x, int y);
void draw_cursor_bitmap(int x, int y);
uint32_t get_pixel(int x, int y);
void draw_file_manager_content(window_t *win);
void draw_system_monitor_content(window_t *win);
void console_execute(window_t *win, char *cmd);
void launch_app(const char *app_name); // Forward declaration for launch_app
void load_file_list();
void show_context_menu(int x, int y, int context_type);
void draw_system_dashboard_content(window_t *win);
void draw_notifications();
void init_terminal_apps();

// Mouse State
int m_x = 160;
int m_y = 100;
uint32_t mouse_bg_buffer[12 * 18]; // Save background behind cursor
int mouse_left = 0;
int mouse_right = 0;

int drag_window_id = -1; // -1 = None
int drag_offset_x = 0;
int drag_offset_y = 0;
int focused_window_id = -1; // Window receiving keyboard input
int terminal_window_id = -1;

// PHASE 5: Clipboard
clipboard_t global_clipboard = {{0}, 0};

// PHASE 2: File Manager
typedef struct {
  char name[64];
  uint32_t size;
  uint8_t is_directory;
} file_entry_t;

file_entry_t file_list[50];
int file_count = 0;
int selected_file = -1;
int file_scroll_offset = 0;

// TitanUI Global State
Component *desktop_root = nullptr;
struct DockIconComponent : public Button {
  int appIndex;
  DockIconComponent(int idx) : appIndex(idx) {}
};
Component *dock_container = nullptr;

// PHASE 7: App launcher
int hovered_dock_icon = -1;

// Window System
// struct window_t is defined in gui.h

#define MAX_WINDOWS 10
window_t windows[MAX_WINDOWS];
int window_count = 0;

// Context Menu Global State
context_menu_t global_menu = {0, 0, 0, 0, 0, {{0, 0}}, 0, -1};

// Wallpaper
uint32_t *wallpaper_buffer = 0; // 32-bit ARGB buffer
int wallpaper_width = 0;
int wallpaper_height = 0;

// BMP Structures (Mini-def to avoid including bmp.h here if possible, or just
// include it) We'll just cast input to uint8_t and parse manually or include
// bmp.h
// #include "../drivers/bmp.h" (Moved to top)

void gui_set_wallpaper(uint8_t *bmp_data) {
  bmp_file_header_t *file = (bmp_file_header_t *)bmp_data;
  bmp_info_header_t *info =
      (bmp_info_header_t *)(bmp_data + sizeof(bmp_file_header_t));

  wallpaper_width = info->width_px;
  wallpaper_height = info->height_px;

  // Allocate 32-bit buffer (Full Screen Size for simplicity, or Image Size)
  // Use page-aligned allocation to reduce fragmentation
  // Dynamic allocation
  wallpaper_buffer = (uint32_t *)kmalloc(1024 * 768 * 4);
  serial_log_hex("GUI: Wallpaper Buffer allocated at ",
                 (uint32_t)wallpaper_buffer);
  if (!wallpaper_buffer) {
    serial_log("GUI: FAILED TO ALLOCATE WALLPAPER BUFFER!");
    return;
  }
  uint8_t *pixel_data = bmp_data + file->offset;
  int bpp = info->bits_per_pixel;
  int row_size = ((wallpaper_width * bpp + 31) / 32) * 4;

  // Clamp dimensions to screen size to avoid overflow
  int draw_width =
      wallpaper_width < SCREEN_WIDTH ? wallpaper_width : SCREEN_WIDTH;
  int draw_height =
      wallpaper_height < SCREEN_HEIGHT ? wallpaper_height : SCREEN_HEIGHT;

  for (int y = 0; y < draw_height; y++) {
    for (int x = 0; x < draw_width; x++) {
      int data_row =
          wallpaper_height - 1 - y; // use original height for source offset
      int offset = (data_row * row_size) + (x * (bpp / 8));

      u8 b = pixel_data[offset + 0];
      u8 g = pixel_data[offset + 1];
      u8 r = pixel_data[offset + 2];

      u32 color = (0xFF << 24) | (r << 16) | (g << 8) | b;

      // Store in linear buffer (clamped)
      wallpaper_buffer[y * SCREEN_WIDTH + x] = color;
    }
  }
}

void draw_window(window_t *win);

// Classic Arrow Cursor (11x18 approx)
// 0 = Transparent, 1 = Black, 2 = White
// We'll map this to palette colors.
// Simple hardcoded shape
// Chunky Retro Arrow Cursor (12x18)
static const uint8_t cursor_bitmap[18][12] = {
    {1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {1, 2, 2, 1, 1, 0, 0, 0, 0, 0, 0, 0},
    {1, 2, 2, 2, 2, 1, 1, 0, 0, 0, 0, 0}, {1, 2, 2, 2, 2, 2, 2, 1, 1, 0, 0, 0},
    {1, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 0}, {1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1},
    {1, 1, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1}, {0, 1, 1, 2, 2, 2, 2, 2, 1, 0, 0, 0},
    {0, 0, 1, 1, 2, 2, 2, 2, 1, 0, 0, 0}, {0, 0, 0, 1, 1, 2, 2, 2, 2, 1, 0, 0},
    {0, 0, 0, 0, 1, 1, 2, 2, 2, 1, 0, 0}, {0, 0, 0, 0, 0, 1, 1, 2, 2, 2, 1, 0},
    {0, 0, 0, 0, 0, 0, 1, 1, 2, 2, 1, 0}, {0, 0, 0, 0, 0, 0, 0, 1, 1, 2, 1, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}};

// Globals for Window Management
// (drag globals defined above)
int resize_window_id = -1;
int resize_start_w, resize_start_h, resize_start_mx, resize_start_my;

// forward decl
void save_mouse_bg(int x, int y);
void restore_mouse_bg(int x, int y);
void draw_cursor_bitmap(int x, int y);
uint32_t get_pixel(int x, int y);

struct AppLaunchButton : public Button {
  char target[32];
  void handle_event(EventType type, int mx, int my) override {
    if (type == EventType::MouseClick) {
      int absX = get_absolute_x();
      int absY = get_absolute_y();
      if (mx >= absX && mx < absX + width && my >= absY && my < absY + height) {
        launch_app(target);
      }
    }
    Button::handle_event(type, mx, my);
  }
};

// Procedural Icon Component
class VectorIcon : public Component {
public:
  int iconType;
  VectorIcon(int type, int w, int h) : iconType(type) {
    width = w;
    height = h;
    style.backgroundColor = 0; // Transparent container
  }

  void render() override {
    int ax = get_absolute_x();
    int ay = get_absolute_y();
    uint32_t border = 0xFF000000;

    if (iconType == 0) { // File Manager (NeoPop Blue)
      uint32_t blue = 0xFF3D5AFE;
      draw_rect(ax, ay + 2, width / 2, 10, blue);
      draw_rect(ax, ay + 2, width / 2, 1, border);
      draw_rect(ax, ay + 2, 1, 10, border);
      draw_rect(ax + width / 2, ay + 2, 1, 10, border);
      draw_rect(ax, ay + 8, width, height - 8, blue);
      draw_rect(ax, ay + 8, width, 1, border);
      draw_rect(ax, ay + height - 1, width, 1, border);
      draw_rect(ax, ay + 8, 1, height - 8, border);
      draw_rect(ax + width - 1, ay + 8, 1, height - 8, border);
      draw_rect(ax + 5, ay + 15, width - 10, height - 20, 0xFFFFFFFF);
    } else if (iconType == 1) { // Terminal
      draw_rect(ax, ay, width, height, 0xFF212121);
      draw_rect(ax, ay, width, 1, border);
      draw_rect(ax, ay + height - 1, width, 1, border);
      draw_rect(ax, ay, 1, height, border);
      draw_rect(ax + width - 1, ay, 1, height, border);
      draw_rect(ax, ay, width, 12, 0xFFEEEEEE);
      draw_rect(ax, ay + 11, width, 1, border);
      draw_rect(ax + 8, ay + 20, 12, 3, 0xFF00E676);
      draw_rect(ax + 22, ay + 20, 16, 3, 0xFF00E676);
    } else if (iconType == 2) { // Notepad
      draw_rect(ax, ay, width, height, 0xFFFFFFFF);
      draw_rect(ax, ay, width, 1, border);
      draw_rect(ax, ay + height - 1, width, 1, border);
      draw_rect(ax, ay, 1, height, border);
      draw_rect(ax + width - 1, ay, 1, height, border);
      draw_rect(ax, ay, width, 10, 0xFFFFEA00);
      draw_rect(ax, ay + 9, width, 1, border);
      for (int i = 20; i < height - 10; i += 8) {
        draw_rect(ax + 6, ay + i, width - 12, 1, 0xFFE0E0E0);
      }
    } else if (iconType == 3) { // System Monitor
      draw_rect(ax, ay, width, height, 0xFF333333);
      draw_rect(ax, ay, width, 1, border);
      draw_rect(ax, ay + height - 1, width, 1, border);
      draw_rect(ax, ay, 1, height, border);
      draw_rect(ax + width - 1, ay, 1, height, border);
      draw_rect(ax + 6, ay + 6, width - 12, 16, 0xFF000000);
      int gx = ax + 8;
      int gy = ay + 14;
      draw_line(gx, gy, gx + 5, gy, 0xFF00E676);
      draw_line(gx + 5, gy, gx + 8, gy - 5, 0xFF00E676);
      draw_line(gx + 8, gy - 5, gx + 12, gy + 5, 0xFF00E676);
      draw_line(gx + 12, gy + 5, gx + 15, gy, 0xFF00E676);
      draw_line(gx + 15, gy, ax + width - 8, gy, 0xFF00E676);
      uint32_t gold = 0xFFFFD700;
      for (int i = 8; i < width - 8; i += 8) {
        draw_rect(ax + i, ay + height - 4, 4, 4, gold);
        draw_rect(ax + i, ay, 4, 4, gold);
      }
    }
  }
};

void init_terminal_apps() {
  // Launch System Dashboard as the primary window
  create_window("SYSTEM DASHBOARD", 150, 80, 520, 420, WINDOW_TYPE_DASHBOARD);

  focused_window_id = window_count - 1;

  draw_desktop();
  save_mouse_bg(m_x, m_y);
  draw_cursor_bitmap(m_x, m_y);
}

void gui_init() {
  m_x = SCREEN_WIDTH / 2;
  m_y = SCREEN_HEIGHT / 2;

  serial_log("GUI_INIT: Starting...");
  // Initialize TitanUI Roots
  desktop_root = new Component();
  serial_log_hex("GUI_INIT: desktop_root = ", (uint32_t)desktop_root);
  desktop_root->width = SCREEN_WIDTH;
  desktop_root->height = SCREEN_HEIGHT;
  serial_log("GUI_INIT: desktop_root initialized.");

  // Modern Dock using NeoPop Style
  serial_log("GUI_INIT: Allocating Dock...");
  int dock_w = 460;
  int dock_h = 80;
  dock_container = new Component();
  serial_log_hex("GUI_INIT: Dock allocated at ", (uint32_t)dock_container);

  dock_container->x = (SCREEN_WIDTH - dock_w) / 2;
  dock_container->y = SCREEN_HEIGHT - 90;
  dock_container->width = dock_w;
  dock_container->height = dock_h;
  dock_container->style.neoPop = true; // Use NeoPop Plunk Rendering
  dock_container->style.backgroundColor = 0xFFFFFFFF; // White Dock Surface

  desktop_root->add_child(dock_container);
  serial_log("GUI_INIT: Dock added to desktop.");

  const char *appNames[] = {"File Manager", "Terminal", "Notepad",
                            "System Monitor"};

  // VectorIcon definition moved to global scope

  serial_log("GUI_INIT: VectorIcon class defined.");

  for (int i = 0; i < 4; i++) {
    serial_log_hex("GUI_INIT: Creating LaunchButton ", i);
    // Assuming AppLaunchButton is defined earlier in the file (it was seen in
    // previous steps) If not, we might need to use Button or define it. Based
    // on Step 1568 context, there was a class with handle_event that called
    // launch_app. I will call it AppLaunchButton to match the snippet I saw
    // using 'target'.

    // We need to define AppLaunchButton if it was inside the function too?
    // Step 1568 showed: class ... : public Button { char target[32]; ... }
    // It didn't start with "class AppLaunchButton". It started with "class
    // VectorIcon" AFTER that. The previous class (lines 160-171 in Step 1568)
    // WAS the button! But it was anonymous? Or named? "160: char target[32];"
    // -> It was inside a class definition. I suspect the class was named
    // `AppLaunchButton`. I will assume it is available. If not, I'll get a
    // compilation error, but better than syntax error.

    AppLaunchButton *launchBtn = new AppLaunchButton();
    serial_log_hex("GUI_INIT: LaunchBtn allocated ", (uint32_t)launchBtn);

    launchBtn->x = 25 + i * 110;
    launchBtn->y = 15;
    launchBtn->width = 50;
    launchBtn->height = 50;
    launchBtn->style.backgroundColor = 0;
    strcpy(launchBtn->target, appNames[i]);

    serial_log("GUI_INIT: Creating VectorIcon...");
    VectorIcon *icon = new VectorIcon(i, 50, 50);
    serial_log_hex("GUI_INIT: VectorIcon allocated ", (uint32_t)icon);

    launchBtn->add_child(icon);
    dock_container->add_child(launchBtn);
  }
  serial_log("GUI_INIT: Icons created.");

  // PHASE 1 & 7: Create initial windows...
  // (existing window initialization stays for now, but will migrate to TitanUI)
  init_terminal_apps();
}

void draw_taskbar() {
  int taskbar_y = SCREEN_HEIGHT - 40;
  // White Taskbar Background with 4px black top border
  draw_rect(0, taskbar_y, SCREEN_WIDTH, 40, PIXEL_WHITE);
  draw_rect(0, taskbar_y, SCREEN_WIDTH, 4, PIXEL_BLACK);

  // Yellow MENU Button
  draw_pixel_box(5, taskbar_y + 6, 80, 28, PIXEL_YELLOW);
  draw_string_scaled(15, taskbar_y + 10, "MENU", PIXEL_BLACK, 2);

  // Status Icons (Placeholders)
  // Speaker
  draw_string(SCREEN_WIDTH - 200, taskbar_y + 12, "vol", PIXEL_BLACK);
  // Battery
  draw_pixel_box(SCREEN_WIDTH - 150, taskbar_y + 10, 30, 20,
                 PIXEL_WHITE); // Battery body
  draw_rect(SCREEN_WIDTH - 120, taskbar_y + 15, 4, 10, PIXEL_BLACK); // Tip

  // Clock
  rtc_time_t t;
  rtc_read(&t);
  char time_str[6];
  time_str[0] = '0' + (t.hour / 10);
  time_str[1] = '0' + (t.hour % 10);
  time_str[2] = ':';
  time_str[3] = '0' + (t.minute / 10);
  time_str[4] = '0' + (t.minute % 10);
  time_str[5] = 0;
  draw_string_scaled(SCREEN_WIDTH - 90, taskbar_y + 10, time_str, PIXEL_BLACK,
                     2);
}

void draw_dock() {
  // Simply tell the TitanUI container to render itself
  if (dock_container) {
    dock_container->render();
  }
}

void draw_window(window_t *win) {
  // PHASE 1: Skip minimized windows
  if (win->minimized)
    return;

  // Shadow (Solid Pixel Shadow)
  draw_rect(win->x + 8, win->y + 8, win->width, win->height, 0x80000000);

  // Main Window Body (Chunky)
  draw_pixel_box(win->x, win->y, win->width, win->height, PIXEL_WHITE);

  // Title Bar Divider (Chunky line)
  int title_bar_height = 32;
  draw_rect(win->x, win->y + title_bar_height - 2, win->width, 4, PIXEL_BLACK);

  // Window Controls (Square with black borders)
  int btn_size = 20;
  int btn_y = win->y + 6;
  int right_x = win->x + win->width - 30;

  // Close (X)
  draw_pixel_box(right_x, btn_y, btn_size, btn_size,
                 (win->hovered_button == 0) ? PIXEL_RED : PIXEL_WHITE);
  draw_string(right_x + 6, btn_y + 4, "x", PIXEL_BLACK);

  // Maximize ([])
  draw_pixel_box(right_x - 24, btn_y, btn_size, btn_size,
                 (win->hovered_button == 2) ? PIXEL_BLUE : PIXEL_WHITE);
  draw_rect(right_x - 19, btn_y + 5, 10, 8, PIXEL_BLACK);

  // Minimize (-)
  draw_pixel_box(right_x - 48, btn_y, btn_size, btn_size,
                 (win->hovered_button == 1) ? PIXEL_YELLOW : PIXEL_WHITE);
  draw_line(right_x - 43, btn_y + 10, right_x - 33, btn_y + 10, PIXEL_BLACK);

  // Resize Handle (Bottom Right)
  int rx = win->x + win->width;
  int ry = win->y + win->height;
  draw_line(rx - 15, ry - 5, rx - 5, ry - 15, 0xFF000000);
  draw_line(rx - 10, ry - 5, rx - 5, ry - 10, 0xFF000000);

  // Title Text (Centered) - PHASE 26: Using scaled font
  int title_len = strlen(win->title) * 16; // Scale 2
  int title_x = win->x + (win->width - title_len) / 2;
  draw_string_scaled(title_x, win->y + 8, win->title, 0x00404040, 2);

  // PHASE 2: Draw file manager content if this is a file manager window
  if (win->type == WINDOW_TYPE_FILE_MANAGER) {
    draw_file_manager_content(win);
    return;
  }

  // PHASE 13: System Dashboard content
  if (win->type == WINDOW_TYPE_DASHBOARD) {
    draw_system_dashboard_content(win);
    return;
  }

  // Draw Content (Text) for normal windows
  int content_x = win->x + 8;
  int content_y = win->y + 40;
  int max_width = win->width - 16;
  int cur_x = 0;

  for (int i = 0; i < win->buffer_len; i++) {
    char c = win->buffer[i];
    if (c == '\n') {
      content_y += 20; // Line height for Scale 2
      cur_x = 0;
      continue;
    }
    if (content_y + 16 > win->y + win->height - 4)
      break;

    draw_char_scaled(content_x + cur_x, content_y, c, COLOR_BLACK, 2);
    cur_x += 16; // 8*2

    if (cur_x >= max_width) {
      content_y += 20;
      cur_x = 0;
    }
  }

  // Draw underscore cursor
  if (win == &windows[focused_window_id]) {
    draw_line(content_x + cur_x, content_y + 16, content_x + cur_x + 12,
              content_y + 16, COLOR_BLACK);
  }
}

void draw_desktop() {
  serial_log("DRAW: Desktop start.");
  // Retro Pixel Grid Background
  draw_pixel_grid(PIXEL_BLUE, 0xFF3D7AB8, 40); // Darker blue grid

  // Draw Bottom Taskbar
  serial_log("DRAW: Taskbar...");
  draw_taskbar();

  // Draw Windows (Back to Front)
  serial_log("DRAW: Windows...");
  for (int i = 0; i < window_count; i++) {
    draw_window(&windows[i]);
  }

  // Draw Dock (Always on top of windows at bottom)
  serial_log("DRAW: Dock...");
  draw_dock();

  // PHASE 11: Draw Context Menu on top of everything
  draw_context_menu();

  // PHASE 13: Draw Notifications
  draw_notifications();

  // Present
  serial_log("DRAW: Swap...");
  swap_buffers();
  serial_log("DRAW: Done.");
}

// Called by mouse driver
// Called by mouse driver
void update_mouse_position(int8_t dx, int8_t dy, uint8_t buttons) {
  // 1. Restore background at old position
  restore_mouse_bg(m_x, m_y);

  // 2. Update position
  m_x += dx;
  m_y -= dy; // Invert Y

  // Clamp
  if (m_x < 0)
    m_x = 0;
  if (m_x >= SCREEN_WIDTH - 12)
    m_x = SCREEN_WIDTH - 12;
  if (m_y < 0)
    m_y = 0;
  if (m_y >= SCREEN_HEIGHT - 18)
    m_y = SCREEN_HEIGHT - 18;

  // ========== PHASE 1 & 7: UPDATE HOVER STATES ==========
  // Update button hover states for all windows (always update on move)
  for (int i = 0; i < window_count; i++) {
    window_t *win = &windows[i];
    if (win->minimized)
      continue;

    win->hovered_button = -1;

    int btn_size = 20;
    int btn_y = win->y + 6;
    int right_x = win->x + win->width - 30;

    // Close
    if (m_x >= right_x && m_x < right_x + btn_size && m_y >= btn_y &&
        m_y < btn_y + btn_size) {
      win->hovered_button = 0;
    }
    // Maximize
    else if (m_x >= right_x - 24 && m_x < right_x - 24 + btn_size &&
             m_y >= btn_y && m_y < btn_y + btn_size) {
      win->hovered_button = 2;
    }
    // Minimize
    else if (m_x >= right_x - 48 && m_x < right_x - 48 + btn_size &&
             m_y >= btn_y && m_y < btn_y + btn_size) {
      win->hovered_button = 1;
    }
  }

  // Dispatch event to TitanUI Tree
  if (desktop_root) {
    desktop_root->handle_event(EventType::MouseMove, m_x, m_y);
  }

  // PHASE 11: Menu Hover Update
  if (global_menu.active) {
    if (m_x >= global_menu.x && m_x < global_menu.x + global_menu.width &&
        m_y >= global_menu.y && m_y < global_menu.y + global_menu.height) {
      int idx = (m_y - (global_menu.y + 5)) / 28;
      if (idx >= 0 && idx < global_menu.item_count) {
        if (global_menu.hovered_item != idx) {
          global_menu.hovered_item = idx;
          draw_desktop();
        }
      } else if (global_menu.hovered_item != -1) {
        global_menu.hovered_item = -1;
        draw_desktop();
      }
    } else if (global_menu.hovered_item != -1) {
      global_menu.hovered_item = -1;
      draw_desktop();
    }
  }

  // 3. Update Buttons
  int new_left = (buttons & 1);
  int new_right = (buttons & 2);

  // Handle Resizing
  if (resize_window_id != -1 && new_left) {
    window_t *win = &windows[resize_window_id];
    win->width = resize_start_w + (m_x - resize_start_mx);
    win->height = resize_start_h + (m_y - resize_start_my);
    if (win->width < 200)
      win->width = 200;
    if (win->height < 100)
      win->height = 100;
    draw_desktop();
  }
  // Handle Dragging (If already dragging)
  else if (new_left && drag_window_id != -1) {
    window_t *win = &windows[drag_window_id];
    if (win->x != m_x - drag_offset_x || win->y != m_y - drag_offset_y) {
      win->x = m_x - drag_offset_x;
      win->y = m_y - drag_offset_y;
      draw_desktop(); // Redraw everything under the cursor
    }
  }

  // ========== PHASE 1: WINDOW BUTTON CLICKS & FOCUS ==========
  if (new_left && !mouse_left) {
    // Just pressed
    int handled = 0;

    // PHASE 11: Menu Action Click
    if (global_menu.active) {
      if (m_x >= global_menu.x && m_x < global_menu.x + global_menu.width &&
          m_y >= global_menu.y && m_y < global_menu.y + global_menu.height) {
        int idx = (m_y - (global_menu.y + 5)) / 28;
        if (idx >= 0 && idx < global_menu.item_count) {
          handle_menu_action(global_menu.items[idx].action_id);
          handled = 1;
        }
      } else {
        global_menu.active = 0; // Click outside dismisses
        draw_desktop();
      }
    }

    // A. Check window title bar buttons
    for (int i = window_count - 1; i >= 0; i--) {
      window_t *win = &windows[i];
      if (win->minimized)
        continue;

      int btn_size = 20;
      int btn_y = win->y + 6;
      int right_x = win->x + win->width - 30;

      // Close
      if (m_x >= right_x && m_x < right_x + btn_size && m_y >= btn_y &&
          m_y < btn_y + btn_size) {
        close_window(i);
        handled = 1;
        break;
      }
      // Maximize
      if (m_x >= right_x - 24 && m_x < right_x - 24 + btn_size &&
          m_y >= btn_y && m_y < btn_y + btn_size) {
        maximize_window(i);
        handled = 1;
        break;
      }
      // Minimize
      if (m_x >= right_x - 48 && m_x < right_x - 48 + btn_size &&
          m_y >= btn_y && m_y < btn_y + btn_size) {
        minimize_window(i);
        handled = 1;
        break;
      }
    }

    if (!handled && desktop_root) {
      // Dispatch click to TitanUI Tree (Handles Dock Icons)
      desktop_root->handle_event(EventType::MouseClick, m_x, m_y);
      handled = 1; // Mark as handled if it hit a component (simplified)
    }

    if (!handled) {
      // C. Check file manager content
      for (int i = window_count - 1; i >= 0; i--) {
        window_t *win = &windows[i];
        if (win->type == WINDOW_TYPE_FILE_MANAGER && !win->minimized) {
          if (m_x >= win->x + 10 && m_x < win->x + win->width - 10 &&
              m_y >= win->y + 50 && m_y < win->y + win->height - 10) {
            int clicked_index = (m_y - (win->y + 50)) / 35 + file_scroll_offset;
            if (clicked_index >= 0 && clicked_index < file_count) {
              selected_file = clicked_index;
              draw_desktop();
              handled = 1;
              break;
            }
          }
        }
      }
    }

    if (!handled) {
      // D. Window Focus, Resize, Drag
      for (int i = window_count - 1; i >= 0; i--) {
        window_t *win = &windows[i];
        if (m_x >= win->x && m_x < win->x + win->width && m_y >= win->y &&
            m_y < win->y + win->height) {
          focused_window_id = i;

          // Check Resize Handle (Bottom Right 20x20)
          if (m_x >= win->x + win->width - 20 &&
              m_y >= win->y + win->height - 20) {
            resize_window_id = i;
            resize_start_w = win->width;
            resize_start_h = win->height;
            resize_start_mx = m_x;
            resize_start_my = m_y;
            handled = 1;
            break;
          }

          if (m_y < win->y + 28) { // Title bar
            drag_window_id = i;
            drag_offset_x = m_x - win->x;
            drag_offset_y = m_y - win->y;
          }
          draw_desktop();
          handled = 1;
          break;
        }
      }
    }
  }

  // Handle Release
  if (!new_left && mouse_left) {
    drag_window_id = -1;
    resize_window_id = -1; // Stop resizing
  }

  // Handle Right Click (Context Menu Trigger)
  if (new_right && !mouse_right) {
    serial_log("GUI: Right-click detected.");

    // Context detection
    int context = 0; // 0 = Desktop

    // Check if right-clicking a file in a file manager
    for (int i = window_count - 1; i >= 0; i--) {
      window_t *win = &windows[i];
      if (win->type == WINDOW_TYPE_FILE_MANAGER && !win->minimized) {
        if (m_x >= win->x + 150 &&
            m_x < win->x + win->width && // Sidebar is 150
            m_y >= win->y + 50 && m_y < win->y + win->height) {
          context = 1; // File context
          break;
        }
      }
    }

    show_context_menu(m_x, m_y, context);
  }

  mouse_left = new_left;
  mouse_right = new_right;

  // ALWAYS redraw cursor on move
  save_mouse_bg(m_x, m_y);
  draw_cursor_bitmap(m_x, m_y);
  swap_buffers();
}

// ============================================================================
// PHASE 1: WORKING WINDOW CONTROLS
// ============================================================================

void close_window(int id) {
  if (id < 0 || id >= window_count)
    return;

  serial_log_hex("GUI: Closing window ", id);

  for (int i = id; i < window_count - 1; i++) {
    windows[i] = windows[i + 1];
  }
  window_count--;

  if (focused_window_id == id) {
    focused_window_id = (window_count > 0) ? window_count - 1 : -1;
  } else if (focused_window_id > id) {
    focused_window_id--;
  }

  if (terminal_window_id == id) {
    terminal_window_id = -1;
  } else if (terminal_window_id > id) {
    terminal_window_id--;
  }

  draw_desktop();
}

void minimize_window(int id) {
  if (id < 0 || id >= window_count)
    return;

  windows[id].minimized = 1;
  serial_log_hex("GUI: Minimized window ", id);

  if (focused_window_id == id) {
    focused_window_id = -1;
    for (int i = 0; i < window_count; i++) {
      if (!windows[i].minimized) {
        focused_window_id = i;
        break;
      }
    }
  }

  draw_desktop();
}

void maximize_window(int id) {
  if (id < 0 || id >= window_count)
    return;

  window_t *win = &windows[id];

  if (win->maximized) {
    win->x = win->restore_x;
    win->y = win->restore_y;
    win->width = win->restore_w;
    win->height = win->restore_h;
    win->maximized = 0;
  } else {
    win->restore_x = win->x;
    win->restore_y = win->y;
    win->restore_w = win->width;
    win->restore_h = win->height;

    win->x = 0;
    win->y = 0;
    win->width = SCREEN_WIDTH;
    win->height = SCREEN_HEIGHT - 40; // taskbar is at bottom (40px)
    win->maximized = 1;
  }

  draw_desktop();
}

int create_window(const char *title, int x, int y, int w, int h, int type) {
  if (window_count >= MAX_WINDOWS)
    return -1;

  window_t *win = &windows[window_count];
  win->x = x;
  win->y = y;
  win->width = w;
  win->height = h;
  strcpy(win->title, title);
  win->buffer[0] = 0;
  win->buffer_len = 0;
  win->minimized = 0;
  win->maximized = 0;
  win->type = type;
  win->hovered_button = -1;

  int id = window_count;
  window_count++;
  focused_window_id = id;

  draw_desktop();
  return id;
}

// PHASE 11: Context Menu Implementation
void show_context_menu(int x, int y, int context_type) {
  global_menu.x = x;
  global_menu.y = y;
  global_menu.active = 1;
  global_menu.hovered_item = -1;
  global_menu.width = 160;

  if (context_type == 0) { // Desktop Context
    global_menu.item_count = 5;
    strcpy(global_menu.items[0].label, "New Folder");
    global_menu.items[0].action_id = ACTION_NEW_FOLDER;
    strcpy(global_menu.items[1].label, "New File");
    global_menu.items[1].action_id = ACTION_NEW_FILE;
    strcpy(global_menu.items[2].label, "Refresh");
    global_menu.items[2].action_id = ACTION_REFRESH;
    strcpy(global_menu.items[3].label, "Paste");
    global_menu.items[3].action_id = ACTION_PASTE;
    strcpy(global_menu.items[4].label, "Desktop Prefs...");
    global_menu.items[4].action_id = 0;
  } else if (context_type == 1) { // File/Folder Context
    global_menu.item_count = 5;
    strcpy(global_menu.items[0].label, "Open");
    global_menu.items[0].action_id = ACTION_OPEN;
    strcpy(global_menu.items[1].label, "Cut");
    global_menu.items[1].action_id = ACTION_CUT;
    strcpy(global_menu.items[2].label, "Copy");
    global_menu.items[2].action_id = ACTION_COPY;
    strcpy(global_menu.items[3].label, "Delete");
    global_menu.items[3].action_id = ACTION_DELETE;
    strcpy(global_menu.items[4].label, "Properties");
    global_menu.items[4].action_id = 0;
  }
  global_menu.height = global_menu.item_count * 28 + 10;
  draw_desktop();
}

void draw_context_menu() {
  if (!global_menu.active)
    return;

  // Modern shadow/border
  draw_rect(global_menu.x + 4, global_menu.y + 4, global_menu.width,
            global_menu.height, 0x40000000); // Shadow
  draw_rounded_rect(global_menu.x, global_menu.y, global_menu.width,
                    global_menu.height, 6, COLOR_WHITE);

  // Gradient Background (Subtle)
  draw_rect_gradient(global_menu.x + 2, global_menu.y + 2,
                     global_menu.width - 4, global_menu.height - 4, 0xFFF9F9F9,
                     0xFFEEEEEE);

  int item_y = global_menu.y + 5;
  for (int i = 0; i < global_menu.item_count; i++) {
    // Hover Highlight
    if (i == global_menu.hovered_item) {
      draw_rect(global_menu.x + 5, item_y, global_menu.width - 10, 24,
                0xFF0078D7); // Windows Blue
      draw_string(global_menu.x + 15, item_y + 4, global_menu.items[i].label,
                  COLOR_WHITE);
    } else {
      draw_string(global_menu.x + 15, item_y + 4, global_menu.items[i].label,
                  COLOR_BLACK);
    }

    // Separator line (before "Delete" or similar)
    if (global_menu.items[i].action_id == ACTION_DELETE ||
        global_menu.items[i].action_id == ACTION_REFRESH) {
      draw_line(global_menu.x + 10, item_y - 2,
                global_menu.x + global_menu.width - 10, item_y - 2, 0xFFDDDDDD);
    }

    item_y += 28;
  }
}

void handle_menu_action(int action_id) {
  global_menu.active = 0;

  switch (action_id) {
  case ACTION_NEW_FOLDER:
    serial_log("GUI: Creating New Folder...");
    fat16_mkdir("NEWFOLDER");
    load_file_list();
    break;
  case ACTION_NEW_FILE:
    serial_log("GUI: Creating New File...");
    fat16_create_file("NEWFILE.TXT");
    load_file_list();
    break;
  case ACTION_DELETE:
    if (selected_file != -1) {
      serial_log_hex("GUI: Deleting item ", selected_file);
      fat16_delete_file(file_list[selected_file].name);
      load_file_list();
      selected_file = -1;
    }
    break;
  case ACTION_REFRESH:
    load_file_list();
    break;
  case ACTION_COPY:
    if (selected_file != -1) {
      clipboard_set_text(file_list[selected_file].name);
      serial_log("GUI: Copied to clipboard.");
    }
    break;
  }
  draw_desktop();
}

// System Dashboard Implementation
void draw_dashboard_tile(int x, int y, int w, int h, const char *label,
                         uint32_t color, const char *symbol) {
  draw_pixel_box(x, y, w, h, color);
  draw_string_scaled(x + (w - strlen(label) * 16) / 2, y + h - 25, label,
                     PIXEL_BLACK, 2);
  // Center the symbol (usually one char or emoji-like placeholder)
  draw_string_scaled(x + (w - strlen(symbol) * 32) / 2, y + 20, symbol,
                     PIXEL_WHITE, 4);
}

void draw_system_dashboard_content(window_t *win) {
  int padding = 20;
  int tile_w = 140;
  int tile_h = 140;
  int start_x = win->x + 20;
  int start_y = win->y + 50;

  // Row 1
  draw_dashboard_tile(start_x, start_y, tile_w, tile_h, "WEATHER", PIXEL_ORANGE,
                      "S");
  draw_dashboard_tile(start_x + tile_w + padding, start_y, tile_w, tile_h,
                      "CALENDAR", PIXEL_GREEN, "C");
  draw_dashboard_tile(start_x + (tile_w + padding) * 2, start_y, tile_w, tile_h,
                      "MUSIC", PIXEL_PURPLE, "M");

  // Row 2
  draw_dashboard_tile(start_x, start_y + tile_h + padding, tile_w, tile_h,
                      "EDIT", PIXEL_RED, "E");
  draw_dashboard_tile(start_x + tile_w + padding, start_y + tile_h + padding,
                      tile_w, tile_h, "SETTINGS", PIXEL_BLUE, "S");
  draw_dashboard_tile(start_x + (tile_w + padding) * 2,
                      start_y + tile_h + padding, tile_w, tile_h, "BROWSER",
                      PIXEL_CYAN, "B");
}

// PHASE 2: FILE MANAGER
void load_file_list() {
  file_count = 0;
  if (vfs_root) {
    int i = 0;
    struct dirent *ent;
    while ((ent = readdir_vfs(vfs_root, i++)) != 0 && file_count < 50) {
      file_entry_t *entry = &file_list[file_count++];
      strcpy(entry->name, ent->name);

      // Try to find the actual node to get size and type
      vfs_node_t *node = finddir_vfs(vfs_root, ent->name);
      if (node) {
        entry->size = node->length;
        entry->is_directory = (node->flags & 0x7) == VFS_DIRECTORY;
      } else {
        entry->size = 0;
        entry->is_directory = 0;
      }
    }
  }
}

// Helper to draw a modern folder icon
void draw_folder_icon(int x, int y) {
  // Main folder body
  draw_rect(x + 2, y + 6, 28, 20, 0xFFFFD700); // Gold body
  // Folder tab/flap
  draw_rect(x + 2, y + 2, 12, 6, 0xFFFFD700);
  // Darker bottom edge for shadow
  draw_line(x + 2, y + 26, x + 30, y + 26, 0xFFB8860B);
}

// Helper to draw a modern file icon
void draw_file_icon(int x, int y) {
  // White paper body
  draw_rect(x + 4, y + 2, 22, 28, COLOR_WHITE);
  // Border
  draw_rect(x + 4, y + 2, 22, 1, 0xFFCCCCCC);
  draw_rect(x + 4, y + 2, 1, 28, 0xFFCCCCCC);
  draw_rect(x + 26, y + 2, 1, 28, 0xFFCCCCCC);
  draw_rect(x + 4, y + 30, 22, 1, 0xFFCCCCCC);

  // Folded corner (Top right)
  draw_rect(x + 20, y + 2, 6, 6, 0xFFEEEEEE);
  draw_line(x + 20, y + 2, x + 26, y + 8, 0xFFCCCCCC);
}

void draw_file_manager_content(window_t *win) {
  // 1. Draw Sidebar (Light Gray/Translucent look)
  int sidebar_width = 150;
  draw_rect(win->x, win->y + 28, sidebar_width, win->height - 28, 0xFFF6F6F6);
  draw_line(win->x + sidebar_width - 1, win->y + 28, win->x + sidebar_width - 1,
            win->y + win->height, 0xFFDDDDDD);

  // Sidebar Items
  const char *sidebar_items[] = {"Favorites",      "  AirDrop",   "  Recents",
                                 "  Applications", "  Documents", "Locations",
                                 "  ThisOS HD"};
  int sy = win->y + 50;
  for (int i = 0; i < 7; i++) {
    uint32_t color = (i == 0 || i == 5) ? 0xFF888888 : COLOR_BLACK;
    draw_string(win->x + 10, sy, sidebar_items[i], color);
    sy += 25;
  }

  // 2. Toolbar / Address Bar Area
  int toolbar_height = 45;
  draw_rect(win->x + sidebar_width, win->y + 28, win->width - sidebar_width,
            toolbar_height, COLOR_WHITE);
  draw_line(win->x + sidebar_width, win->y + 28 + toolbar_height - 1,
            win->x + win->width, win->y + 28 + toolbar_height - 1, 0xFFEEEEEE);

  // Back/Forward placeholder buttons
  draw_circle_filled(win->x + sidebar_width + 20, win->y + 50, 12, 0xFFF0F0F0);
  draw_string(win->x + sidebar_width + 16, win->y + 44, "<", 0xFF666666);
  draw_circle_filled(win->x + sidebar_width + 50, win->y + 50, 12, 0xFFF0F0F0);
  draw_string(win->x + sidebar_width + 46, win->y + 44, ">", 0xFF666666);

  // Breadcrumb / Address Bar
  int addr_x = win->x + sidebar_width + 80;
  int addr_w = win->width - sidebar_width - 100;
  draw_rect(addr_x, win->y + 38, addr_w, 24, 0xFFF1F1F1);
  draw_string(addr_x + 10, win->y + 44, "ThisOS HD > Root", 0xFF444444);

  // 3. Main File View
  int item_y = win->y + 28 + toolbar_height + 10;
  int item_height = 40;
  int content_x = win->x + sidebar_width + 10;
  int content_w = win->width - sidebar_width - 20;

  for (int i = file_scroll_offset;
       i < file_count && item_y < win->y + win->height - 20; i++) {
    file_entry_t *entry = &file_list[i];

    // Highlight selected
    if (i == selected_file) {
      draw_rect(content_x - 5, item_y - 2, content_w + 10, item_height,
                0xFFE8F1FF); // Light blue background
    }

    // Draw Icon
    if (entry->is_directory) {
      draw_folder_icon(content_x, item_y + 2);
    } else {
      draw_file_icon(content_x, item_y + 2);
    }

    // File Name and Details
    draw_string_scaled(content_x + 40, item_y + 10, entry->name, COLOR_BLACK,
                       2);

    // Size (if not directory)
    if (!entry->is_directory) {
      char size_str[16];
      if (entry->size < 1024) {
        strcpy(size_str, "1 KB");
      } else {
        // Very simple math for KB
        int kb = entry->size / 1024;
        // Since we don't have sprintf easily, just hardcode a few for now or
        // leave simple
        strcpy(size_str, "FILE");
      }
      draw_string(content_x + content_w - 60, item_y + 15, size_str,
                  0xFF999999);
    }

    item_y += item_height;
  }
}

void open_file_manager() {
  int id = create_window("File Manager", 150, 80, 600, 450,
                         WINDOW_TYPE_FILE_MANAGER);
  if (id >= 0) {
    load_file_list();
    selected_file = -1;
    file_scroll_offset = 0;
  }
}

// PHASE 5: CLIPBOARD
void clipboard_set_text(const char *text) {
  int len = 0;
  while (text[len] && len < 2047) {
    global_clipboard.data[len] = text[len];
    len++;
  }
  global_clipboard.data[len] = 0;
  global_clipboard.has_data = 1;
}

const char *clipboard_get_text() {
  if (global_clipboard.has_data) {
    return global_clipboard.data;
  }
  return "";
}

int clipboard_has_data() { return global_clipboard.has_data; }

// Helper for int to string
void int_to_str(int n, char *buf) {
  if (n == 0) {
    buf[0] = '0';
    buf[1] = 0;
    return;
  }
  int k = 0;
  while (n > 0) {
    buf[k++] = '0' + (n % 10);
    n /= 10;
  }
  buf[k] = 0;
  // Reverse
  for (int i = 0; i < k / 2; i++) {
    char t = buf[i];
    buf[i] = buf[k - 1 - i];
    buf[k - 1 - i] = t;
  }
}

void draw_system_monitor_content(window_t *win) {
  // Header
  draw_rect(win->x, win->y + 30, win->width, 24, 0xFFEEEEEE);
  draw_string(win->x + 10, win->y + 36, "PID", COLOR_BLACK);
  draw_string(win->x + 60, win->y + 36, "STATE", COLOR_BLACK);
  draw_string(win->x + 150, win->y + 36, "TYPE", COLOR_BLACK);
  // Line
  draw_line(win->x, win->y + 54, win->x + win->width, win->y + 54, 0xFFCCCCCC);

  int y = win->y + 60;
  process_t *p = ready_queue;
  if (!p)
    return;

  process_t *start = p;
  char buf[16];
  do {
    int_to_str(p->id, buf);
    draw_string(win->x + 10, y, buf, COLOR_BLACK);
    draw_string(win->x + 60, y, "RUNNING", 0xFF00AA00); // Mock state
    draw_string(win->x + 150, y, "Process", COLOR_BLACK);

    y += 20;
    p = p->next;
  } while (p != start && p != 0 && y < win->y + win->height - 20);
}

// PHASE 7: APPLICATION LAUNCHER
void launch_app(const char *app_name) {
  if (strcmp(app_name, "File Manager") == 0) {
    open_file_manager();
  } else if (strcmp(app_name, "Terminal") == 0) {
    create_window("Terminal", 100, 100, 600, 400, WINDOW_TYPE_TERMINAL);
    window_t *term = &windows[window_count - 1];
    strcpy(term->buffer, "Welcome to ThisOS Terminal\\n> ");
    term->buffer_len = strlen(term->buffer);
  } else if (strcmp(app_name, "Notepad") == 0) {
    create_window("Notepad", 200, 150, 500, 400, WINDOW_TYPE_NOTEPAD);
  } else if (strcmp(app_name, "System Monitor") == 0) {
    create_window("System Monitor", 150, 150, 400, 300,
                  WINDOW_TYPE_SYSTEM_MONITOR);
  } else if (strcmp(app_name, "Calculator") == 0) {
    create_window("Calculator", 300, 200, 300, 400, WINDOW_TYPE_CALCULATOR);
  }
}

void save_mouse_bg(int x, int y) {
  int idx = 0;
  for (int i = 0; i < 18; i++) {
    for (int j = 0; j < 12; j++) {
      if (x + j < SCREEN_WIDTH && y + i < SCREEN_HEIGHT)
        mouse_bg_buffer[idx++] = get_pixel(x + j, y + i);
      else
        mouse_bg_buffer[idx++] = 0;
    }
  }
}

void restore_mouse_bg(int x, int y) {
  int idx = 0;
  for (int i = 0; i < 18; i++) {
    for (int j = 0; j < 12; j++) {
      if (x + j < SCREEN_WIDTH && y + i < SCREEN_HEIGHT)
        put_pixel(x + j, y + i, mouse_bg_buffer[idx]);
      idx++;
    }
  }
}

void draw_cursor_bitmap(int x, int y) {
  for (int i = 0; i < 18; i++) {
    for (int j = 0; j < 12; j++) {
      uint8_t val = cursor_bitmap[i][j];
      if (val == 0)
        continue;

      uint32_t color = (val == 1) ? COLOR_BLACK : COLOR_WHITE;
      put_pixel(x + j, y + i, color);
    }
  }
}

uint32_t get_pixel(int x, int y) {
  if (x >= 0 && x < SCREEN_WIDTH && y >= 0 && y < SCREEN_HEIGHT) {
    return screen_buffer[y * SCREEN_WIDTH + x];
  }
  return 0;
}

void handle_key_press(char c) {
  if (focused_window_id == -1) {
    return;
  }
  window_t *win = &windows[focused_window_id];

  if (c == '\b') {
    if (win->buffer_len > 0) {
      win->buffer_len--;
      win->buffer[win->buffer_len] = 0;
      draw_desktop();
    }
  } else if (c == '\n') {
    if (focused_window_id == terminal_window_id) {
      int i = win->buffer_len - 1;
      while (i >= 0 && win->buffer[i] != '\n' && win->buffer[i] != '>')
        i--;

      char *raw_cmd = win->buffer + i + 1;
      while (*raw_cmd == ' ')
        raw_cmd++;

      char cmd_buf[64];
      int k = 0;
      while (k < 63 && *raw_cmd != 0 && *raw_cmd != '\n') {
        cmd_buf[k++] = *raw_cmd++;
      }
      cmd_buf[k] = 0;

      win->buffer[win->buffer_len++] = '\n';
      win->buffer[win->buffer_len] = 0;

      console_execute(win, cmd_buf);

      strcat(win->buffer, "> ");
      win->buffer_len = strlen(win->buffer);
    } else {
      if (win->buffer_len < 1023) {
        win->buffer[win->buffer_len++] = '\n';
        win->buffer[win->buffer_len] = 0;
      }
    }
    draw_desktop();
  } else if (win->buffer_len < 1023) {
    // Handle Ctrl+S (Simple: Save to SAVED.TXT)
    // Assuming 'c' is ASCII. Ctrl+S is usually 0x13
    if (c == 0x13) {
      serial_log("GUI: Saving Notepad content...");
      fat16_create_file("SAVED.TXT");
      fat16_write_file("SAVED.TXT", (uint8_t *)win->buffer, win->buffer_len);
      serial_log("GUI: Saved to SAVED.TXT");
    } else {
      win->buffer[win->buffer_len++] = c;
      win->buffer[win->buffer_len] = 0;
      draw_desktop();
    }
  }
}

void console_execute(window_t *win, char *cmd) {
  if (strcmp(cmd, "help") == 0) {
    strcat(win->buffer, "Available commands:\n  help - Show this list\n  cls  "
                        "- Clear screen\n  time - Show system time\n  ls   - "
                        "List files\n  df   - Disk usage\n  fm   - Open File "
                        "Manager\n  calc <exp> - Calculator\n");
  } else if (strcmp(cmd, "cls") == 0) {
    win->buffer[0] = 0;
    win->buffer_len = 0;
  } else if (strcmp(cmd, "time") == 0) {
    rtc_time_t t;
    rtc_read(&t);
    char tmp[32];
    tmp[0] = '0' + t.hour / 10;
    tmp[1] = '0' + t.hour % 10;
    tmp[2] = ':';
    tmp[3] = '0' + t.minute / 10;
    tmp[4] = '0' + t.minute % 10;
    tmp[5] = '\n';
    tmp[6] = 0;
    strcat(win->buffer, "Current Time: ");
    strcat(win->buffer, tmp);
  } else if (strcmp(cmd, "ls") == 0 || strcmp(cmd, "dir") == 0) {
    strcat(win->buffer, "Contents of /:\n");
    for (int i = 0; i < 50; i++) {
      struct dirent *de = readdir_vfs(vfs_root, i);
      if (de) {
        strcat(win->buffer, "  ");
        strcat(win->buffer, de->name);
        strcat(win->buffer, "\n");
      } else
        break;
    }
  } else if (strcmp(cmd, "df") == 0) {
    uint32_t total = 0, free = 0;
    fat16_get_stats_bytes(&total, &free);
    char buf[128];
    strcpy(buf, "Disk Usage:\n  Total: ");
    char tbuf[16];
    int_to_str(total / 1024, tbuf);
    strcat(buf, tbuf);
    strcat(buf, " KB\n  Free:  ");
    int_to_str(free / 1024, tbuf);
    strcat(buf, tbuf);
    strcat(buf, " KB\n");
    strcat(win->buffer, buf);
  } else if (strcmp(cmd, "fm") == 0) {
    open_file_manager();
    strcat(win->buffer, "Opening File Manager...\n");
  } else if (strncmp(cmd, "calc ", 5) == 0) {
    char *exp = cmd + 5;
    // Very simple 1 op parser: "val op val"
    int v1 = 0, v2 = 0;
    char op = 0;
    char *p = exp;
    while (*p && (*p < '0' || *p > '9'))
      p++;
    while (*p >= '0' && *p <= '9')
      v1 = v1 * 10 + (*p++ - '0');
    while (*p && *p == ' ')
      p++;
    if (*p)
      op = *p++;
    while (*p && (*p < '0' || *p > '9'))
      p++;
    while (*p >= '0' && *p <= '9')
      v2 = v2 * 10 + (*p++ - '0');

    int res = 0;
    if (op == '+')
      res = v1 + v2;
    else if (op == '-')
      res = v1 - v2;
    else if (op == '*')
      res = v1 * v2;
    else if (op == '/' && v2 != 0)
      res = v1 / v2;

    char buf[32];
    int_to_str(res, buf);
    strcat(win->buffer, "Result: ");
    strcat(win->buffer, buf);
    strcat(win->buffer, "\n");
  } else if (strlen(cmd) > 0) {
    strcat(win->buffer, "Unknown command.\n");
  }
  win->buffer_len = strlen(win->buffer);
}
// Retro Pixel Notifications
typedef struct {
  char from[32];
  char msg[64];
  int active;
  uint32_t start_time;
} notification_t;

notification_t global_notif = {"PIXELCHAT", "New Message from PIXELCHAT", 0, 0};

void add_notification(const char *from, const char *msg) {
  strcpy(global_notif.from, from);
  strcpy(global_notif.msg, msg);
  global_notif.active = 1;
  draw_desktop();
}

void draw_notifications() {
  if (!global_notif.active)
    return;

  int nw = 220;
  int nh = 80;
  int nx = SCREEN_WIDTH - nw - 20;
  int ny = 40;

  // Toast (Yellow Pixel Box)
  draw_pixel_box(nx, ny, nw, nh, PIXEL_YELLOW);

  draw_string(nx + 10, ny + 10, "New Message from", PIXEL_BLACK);
  draw_string(nx + 10, ny + 25, global_notif.from, PIXEL_BLACK);

  // DISMISS Button
  draw_pixel_box(nx + 40, ny + nh - 30, 140, 24,
                 PIXEL_YELLOW); // Actually black border yellow button
  draw_rect(nx + 40, ny + nh - 30, 140, 24,
            PIXEL_WHITE); // White interior for button look
  draw_rect(nx + 40, ny + nh - 30, 140, 4, PIXEL_BLACK);
  draw_rect(nx + 40, ny + nh - 10, 140, 4, PIXEL_BLACK);
  draw_rect(nx + 40, ny + nh - 30, 4, 24, PIXEL_BLACK);
  draw_rect(nx + 180, ny + nh - 30, 4, 24, PIXEL_BLACK);

  draw_string(nx + 65, ny + nh - 25, "DISMISS", PIXEL_BLACK);
}
