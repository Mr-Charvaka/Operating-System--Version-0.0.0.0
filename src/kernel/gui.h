#ifndef GUI_H
#define GUI_H

#include "../include/types.h"

// PHASE 13: Action IDs for Context Menu
#define ACTION_NEW_FOLDER 1
#define ACTION_NEW_FILE 2
#define ACTION_DELETE 3
#define ACTION_CUT 4
#define ACTION_COPY 5
#define ACTION_PASTE 6
#define ACTION_OPEN 7
#define ACTION_REFRESH 8

// Window Types
#define WINDOW_TYPE_NORMAL 0
#define WINDOW_TYPE_FILE_MANAGER 1
#define WINDOW_TYPE_TERMINAL 2
#define WINDOW_TYPE_NOTEPAD 3
#define WINDOW_TYPE_SYSTEM_MONITOR 4
#define WINDOW_TYPE_CALCULATOR 5
#define WINDOW_TYPE_DASHBOARD 6

// PHASE 11: Context Menu system
typedef struct menu_item {
  char label[32];
  int action_id; // ID to identify the action in WindowServer
} menu_item_t;

typedef struct context_menu {
  int x, y;
  int width, height;
  int item_count;
  menu_item_t items[8];
  int active;
  int hovered_item;
} context_menu_t;

// Window Structure (PHASE 1: Enhanced with state tracking)
typedef struct {
  int x, y;
  int width, height;
  char title[64];
  char buffer[1024]; // Text content
  int buffer_len;

  // PHASE 11: Context Menu system
  typedef struct menu_item {
    char label[32];
    int action_id; // ID to identify the action in WindowServer
  } menu_item_t;

  typedef struct context_menu {
    int x, y;
    int width, height;
    int item_count;
    menu_item_t items[8];
    int active;
    int hovered_item;
  } context_menu_t;

  // PHASE 1: Window state
  int minimized;
  int maximized;
  int restore_x, restore_y, restore_w, restore_h;
  int type;           // Window type
  int hovered_button; // -1=none, 0=close, 1=minimize, 2=maximize
} window_t;

// PHASE 5: Clipboard
typedef struct {
  char data[2048];
  int has_data;
} clipboard_t;

extern clipboard_t global_clipboard;

#ifdef __cplusplus
extern "C" {
#endif

// Core GUI functions
void gui_init();
void draw_desktop();
void update_mouse_position(int8_t dx, int8_t dy, uint8_t buttons);
void handle_key_press(char c);
void gui_set_wallpaper(uint8_t *bmp_data);

// PHASE 1: Window management
void close_window(int id);
void minimize_window(int id);
void maximize_window(int id);
int create_window(const char *title, int x, int y, int w, int h, int type);

// PHASE 13: Retro Pixel
void init_terminal_apps();

#ifdef __cplusplus
}
#endif

// PHASE 2: File Manager
void open_file_manager();

// PHASE 5: Clipboard
void clipboard_set_text(const char *text);
const char *clipboard_get_text();
int clipboard_has_data();

// PHASE 7: Application launcher
void launch_app(const char *app_name);

// Retro Pixel Notifications
void add_notification(const char *from, const char *msg);
void draw_notifications();

#endif
