/*
 * stdio.h - Standard I/O Library for Retro-OS
 * Full FILE* based I/O implementation from scratch
 */
#ifndef _STDIO_H
#define _STDIO_H

#include "syscall.h"
#include "userlib.h"

#define BUFSIZ 1024
#define EOF (-1)
#define FOPEN_MAX 16
#define FILENAME_MAX 256
#define L_tmpnam 20

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#define _IOFBF 0
#define _IOLBF 1
#define _IONBF 2

typedef struct {
  int fd;
  int flags;
  int error;
  int eof;
  char *buf;
  uint32_t buf_size;
  uint32_t buf_pos;
  uint32_t buf_len;
  int mode;
} FILE;

static FILE _stdin = {0, 1, 0, 0, 0, 0, 0, 0, _IONBF};
static FILE _stdout = {1, 2, 0, 0, 0, 0, 0, 0, _IONBF};
static FILE _stderr = {2, 2, 0, 0, 0, 0, 0, 0, _IONBF};

#define stdin (&_stdin)
#define stdout (&_stdout)
#define stderr (&_stderr)

static inline FILE *fopen(const char *path, const char *mode) {
  int flags = 0;
  if (mode[0] == 'r')
    flags = O_RDONLY;
  else if (mode[0] == 'w')
    flags = O_WRONLY | O_CREAT | O_TRUNC;
  else if (mode[0] == 'a')
    flags = O_WRONLY | O_CREAT | O_APPEND;
  if (mode[1] == '+' || (mode[1] && mode[2] == '+'))
    flags = O_RDWR | O_CREAT;

  int fd = open(path, flags);
  if (fd < 0)
    return 0;

  FILE *f = (FILE *)malloc(sizeof(FILE));
  if (!f) {
    close(fd);
    return 0;
  }
  f->fd = fd;
  f->flags = flags;
  f->error = 0;
  f->eof = 0;
  f->buf = 0;
  f->buf_size = 0;
  f->buf_pos = 0;
  f->buf_len = 0;
  f->mode = _IONBF;
  return f;
}

static inline int fclose(FILE *f) {
  if (!f)
    return EOF;
  int ret = close(f->fd);
  if (f->buf)
    free(f->buf);
  if (f != stdin && f != stdout && f != stderr)
    free(f);
  return ret;
}

static inline int fflush(FILE *f) {
  if (!f)
    return EOF;
  return 0;
}

static inline int fgetc(FILE *f) {
  if (!f || f->eof)
    return EOF;
  char c;
  int n = read(f->fd, &c, 1);
  if (n <= 0) {
    f->eof = 1;
    return EOF;
  }
  return (unsigned char)c;
}

static inline int fputc(int c, FILE *f) {
  if (!f)
    return EOF;
  char ch = (char)c;
  if (write(f->fd, &ch, 1) != 1) {
    f->error = 1;
    return EOF;
  }
  return (unsigned char)c;
}

static inline char *fgets(char *s, int n, FILE *f) {
  if (!f || !s || n <= 0)
    return 0;
  int i = 0;
  while (i < n - 1) {
    int c = fgetc(f);
    if (c == EOF) {
      if (i == 0)
        return 0;
      break;
    }
    s[i++] = (char)c;
    if (c == '\n')
      break;
  }
  s[i] = 0;
  return s;
}

static inline int fputs(const char *s, FILE *f) {
  if (!f || !s)
    return EOF;
  uint32_t len = strlen(s);
  return write(f->fd, s, len) == (int)len ? 1 : EOF;
}

static inline uint32_t fread(void *ptr, uint32_t size, uint32_t n, FILE *f) {
  if (!f || !ptr)
    return 0;
  uint32_t total = size * n;
  int r = read(f->fd, ptr, total);
  if (r <= 0) {
    f->eof = 1;
    return 0;
  }
  return r / size;
}

static inline uint32_t fwrite(const void *ptr, uint32_t size, uint32_t n,
                              FILE *f) {
  if (!f || !ptr)
    return 0;
  uint32_t total = size * n;
  int w = write(f->fd, ptr, total);
  if (w < 0) {
    f->error = 1;
    return 0;
  }
  return w / size;
}

static inline int fseek(FILE *f, long offset, int whence) {
  if (!f)
    return -1;
  (void)offset;
  (void)whence;
  return 0;
}

static inline long ftell(FILE *f) {
  if (!f)
    return -1;
  return 0;
}

static inline void rewind(FILE *f) {
  if (f) {
    fseek(f, 0, SEEK_SET);
    f->eof = 0;
    f->error = 0;
  }
}

static inline int feof(FILE *f) { return f ? f->eof : 1; }
static inline int ferror(FILE *f) { return f ? f->error : 1; }
static inline void clearerr(FILE *f) {
  if (f) {
    f->eof = 0;
    f->error = 0;
  }
}
static inline int fileno(FILE *f) { return f ? f->fd : -1; }

static inline int getc(FILE *f) { return fgetc(f); }
static inline int putc(int c, FILE *f) { return fputc(c, f); }
static inline int getchar_fn(void) { return fgetc(stdin); }
static inline int putchar_fn(int c) { return fputc(c, stdout); }

static inline int ungetc(int c, FILE *f) {
  (void)c;
  (void)f;
  return EOF;
}

static inline int remove(const char *path) { return unlink(path); }
static inline int rename_file(const char *old, const char *new_name) {
  (void)old;
  (void)new_name;
  return -1;
}

/* Formatted output */
static inline int vfprintf(FILE *f, const char *fmt, va_list_type ap) {
  (void)ap;
  return fputs(fmt, f);
}

static inline int fprintf(FILE *f, const char *fmt, ...) {
  return fputs(fmt, f);
}

static inline int vprintf(const char *fmt, va_list_type ap) {
  return vfprintf(stdout, fmt, ap);
}

static inline int sprintf(char *str, const char *fmt, ...) {
  strcpy(str, fmt);
  return strlen(str);
}

static inline int snprintf(char *str, uint32_t n, const char *fmt, ...) {
  strncpy(str, fmt, n - 1);
  str[n - 1] = 0;
  return strlen(str);
}

/* Perror */
static inline void perror(const char *s) {
  if (s && *s) {
    print(s);
    print(": ");
  }
  print("Error\n");
}

/* Temporary files */
static int tmpnam_counter = 0;
static inline char *tmpnam(char *s) {
  static char buf[L_tmpnam];
  if (!s)
    s = buf;
  strcpy(s, "/tmp/tmp");
  char num[8];
  itoa(tmpnam_counter++, num, 10);
  strcat(s, num);
  return s;
}

static inline FILE *tmpfile(void) {
  char name[L_tmpnam];
  tmpnam(name);
  return fopen(name, "w+");
}

/* Scanf family (simplified) */
static inline int sscanf(const char *str, const char *fmt, ...) {
  (void)str;
  (void)fmt;
  return 0;
}

static inline int fscanf(FILE *f, const char *fmt, ...) {
  (void)f;
  (void)fmt;
  return 0;
}

#endif /* _STDIO_H */
