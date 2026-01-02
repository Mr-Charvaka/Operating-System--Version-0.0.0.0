#include <stdint.h>

void print(const char *str) { asm volatile("int $0x80" : : "a"(0), "b"(str)); }

int read(int fd, void *buf, uint32_t size) {
  int res;
  asm volatile("int $0x80" : "=a"(res) : "a"(3), "b"(fd), "c"(buf), "d"(size));
  return res;
}

int write(int fd, const void *buf, uint32_t size) {
  int res;
  asm volatile("int $0x80" : "=a"(res) : "a"(4), "b"(fd), "c"(buf), "d"(size));
  return res;
}

void exit(int status) { asm volatile("int $0x80" : : "a"(12), "b"(status)); }

int atoi(const char *s) {
  int res = 0;
  while (*s >= '0' && *s <= '9') {
    res = res * 10 + (*s - '0');
    s++;
  }
  return res;
}

void itoa(int n, char *s) {
  int i = 0, sign;
  if ((sign = n) < 0)
    n = -n;
  do {
    s[i++] = n % 10 + '0';
  } while ((n /= 10) > 0);
  if (sign < 0)
    s[i++] = '-';
  s[i] = '\0';

  // Reverse
  for (int j = 0, k = i - 1; j < k; j++, k--) {
    char temp = s[j];
    s[j] = s[k];
    s[k] = temp;
  }
}

void _start() {
  print("\n--- Terminal Calculator ---\n");
  print("Options: +, -, *, /\n");
  print("Enter 'q' to quit (not yet implemented fully, just loop)\n");

  while (1) {
    char buf[32];
    print("\nEnter first number: ");
    int len = read(0, buf, 31);
    if (len <= 0)
      continue;
    buf[len - 1] = 0; // Remove newline if TTY handled it
    int a = atoi(buf);

    print("Enter operator (+, -, *, /): ");
    len = read(0, buf, 31);
    if (len <= 0)
      continue;
    char op = buf[0];

    print("Enter second number: ");
    len = read(0, buf, 31);
    if (len <= 0)
      continue;
    buf[len - 1] = 0;
    int b = atoi(buf);

    int res = 0;
    if (op == '+')
      res = a + b;
    else if (op == '-')
      res = a - b;
    else if (op == '*')
      res = a * b;
    else if (op == '/') {
      if (b != 0)
        res = a / b;
      else
        print("Error: Division by zero\n");
    }

    char res_buf[32];
    itoa(res, res_buf);
    print("Result: ");
    print(res_buf);
    print("\n");
  }
  exit(0);
}
