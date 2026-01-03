#pragma once

#include "string.hpp"
#include "syscalls.hpp"

// We need to know the message structures.
// Ideally these should be in a shared header, but for now we replicate what was
// in demo_ipc.c / src/include/msg.h

namespace OS {

// Message definitions
constexpr int MSG_GFX_CREATE_WINDOW = 1;
constexpr int MSG_GFX_WINDOW_CREATED = 2;
constexpr int MSG_GFX_INVALIDATE_RECT = 3;

struct msg_gfx_create_window_t {
  int width;
  int height;
  char title[64];
};

struct msg_gfx_window_created_t {
  int window_id;
  int shm_id;
};

struct msg_gfx_invalidate_t {
  int window_id;
  int x, y, width, height;
};

struct gfx_msg_t {
  uint32_t type;
  uint32_t size;
  union {
    msg_gfx_create_window_t create;
    msg_gfx_window_created_t created;
    msg_gfx_invalidate_t invalidate;
  } data;
};

class IPCClient {
private:
  int sock_fd;
  int window_id;
  uint32_t *framebuffer;
  int width;
  int height;

public:
  IPCClient()
      : sock_fd(-1), window_id(-1), framebuffer(nullptr), width(0), height(0) {}

  bool connect() {
    sock_fd = Syscall::socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock_fd < 0)
      return false;

    if (Syscall::connect(sock_fd, "/tmp/ws.sock") < 0) {
      Syscall::close(sock_fd);
      sock_fd = -1;
      return false;
    }
    return true;
  }

  bool create_window(const char *title, int w, int h) {
    if (sock_fd < 0)
      return false;

    width = w;
    height = h;

    gfx_msg_t msg;
    msg.type = MSG_GFX_CREATE_WINDOW;
    msg.size = sizeof(msg_gfx_create_window_t);
    msg.data.create.width = width;
    msg.data.create.height = height;
    String::strcpy(msg.data.create.title, title);

    Syscall::write(sock_fd, &msg, sizeof(msg));

    // Wait for response
    gfx_msg_t resp;
    int n = Syscall::read(sock_fd, &resp, sizeof(resp));
    if (n <= 0)
      return false;

    if (resp.type == MSG_GFX_WINDOW_CREATED) {
      window_id = resp.data.created.window_id;
      int shm_id = resp.data.created.shm_id;

      framebuffer = (uint32_t *)Syscall::shmat(shm_id);
      if (!framebuffer)
        return false;

      return true;
    }

    return false;
  }

  void invalidate(int x, int y, int w, int h) {
    if (sock_fd < 0)
      return;
    gfx_msg_t msg;
    msg.type = MSG_GFX_INVALIDATE_RECT;
    msg.size = sizeof(msg_gfx_invalidate_t);
    msg.data.invalidate.window_id = window_id;
    msg.data.invalidate.x = x;
    msg.data.invalidate.y = y;
    msg.data.invalidate.width = w;
    msg.data.invalidate.height = h;
    Syscall::write(sock_fd, &msg, sizeof(msg));
  }

  void flush() { invalidate(0, 0, width, height); }

  uint32_t *get_buffer() const { return framebuffer; }

  int get_width() const { return width; }
  int get_height() const { return height; }

  void put_pixel(int x, int y, uint32_t color) {
    if (framebuffer && x >= 0 && x < width && y >= 0 && y < height) {
      framebuffer[y * width + x] = color;
    }
  }

  void fill_rect(int x, int y, int w, int h, uint32_t color) {
    for (int j = 0; j < h; ++j) {
      for (int i = 0; i < w; ++i) {
        put_pixel(x + i, y + j, color);
      }
    }
  }
};
} // namespace OS
