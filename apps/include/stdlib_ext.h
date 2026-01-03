/*
 * stdlib_ext.h - Extended Standard Library for Retro-OS
 * Memory allocation, process control, data structures
 */
#ifndef _STDLIB_EXT_H
#define _STDLIB_EXT_H

#include "libc.h"
#include "syscall.h"


/* ============== HEAP MANAGEMENT ============== */

#define HEAP_MAGIC 0xDEADBEEF

typedef struct heap_block {
  uint32_t magic;
  uint32_t size;
  int free;
  struct heap_block *next;
  struct heap_block *prev;
} heap_block_t;

static heap_block_t *heap_start = 0;
static uint32_t heap_end_addr = 0;

static inline void heap_init(void) {
  if (!heap_start) {
    heap_end_addr = sbrk(4096);
    heap_start = (heap_block_t *)heap_end_addr;
    heap_start->magic = HEAP_MAGIC;
    heap_start->size = 4096 - sizeof(heap_block_t);
    heap_start->free = 1;
    heap_start->next = 0;
    heap_start->prev = 0;
    heap_end_addr += 4096;
  }
}

static inline void *malloc(uint32_t size) {
  if (size == 0)
    return 0;
  if (!heap_start)
    heap_init();

  size = (size + 7) & ~7; /* Align to 8 bytes */

  heap_block_t *block = heap_start;
  while (block) {
    if (block->free && block->size >= size) {
      if (block->size > size + sizeof(heap_block_t) + 16) {
        heap_block_t *new_block =
            (heap_block_t *)((uint8_t *)block + sizeof(heap_block_t) + size);
        new_block->magic = HEAP_MAGIC;
        new_block->size = block->size - size - sizeof(heap_block_t);
        new_block->free = 1;
        new_block->next = block->next;
        new_block->prev = block;
        if (block->next)
          block->next->prev = new_block;
        block->next = new_block;
        block->size = size;
      }
      block->free = 0;
      return (void *)((uint8_t *)block + sizeof(heap_block_t));
    }
    if (!block->next) {
      uint32_t need = size + sizeof(heap_block_t);
      need = (need + 4095) & ~4095;
      uint32_t new_mem = sbrk(need);
      if ((int)new_mem == -1)
        return 0;

      heap_block_t *new_block = (heap_block_t *)new_mem;
      new_block->magic = HEAP_MAGIC;
      new_block->size = need - sizeof(heap_block_t);
      new_block->free = 1;
      new_block->next = 0;
      new_block->prev = block;
      block->next = new_block;
      heap_end_addr = new_mem + need;
    }
    block = block->next;
  }
  return 0;
}

static inline void free(void *ptr) {
  if (!ptr)
    return;
  heap_block_t *block = (heap_block_t *)((uint8_t *)ptr - sizeof(heap_block_t));
  if (block->magic != HEAP_MAGIC)
    return;
  block->free = 1;

  /* Coalesce with next */
  if (block->next && block->next->free) {
    block->size += sizeof(heap_block_t) + block->next->size;
    block->next = block->next->next;
    if (block->next)
      block->next->prev = block;
  }
  /* Coalesce with prev */
  if (block->prev && block->prev->free) {
    block->prev->size += sizeof(heap_block_t) + block->size;
    block->prev->next = block->next;
    if (block->next)
      block->next->prev = block->prev;
  }
}

static inline void *calloc(uint32_t nmemb, uint32_t size) {
  uint32_t total = nmemb * size;
  void *ptr = malloc(total);
  if (ptr)
    memset(ptr, 0, total);
  return ptr;
}

static inline void *realloc(void *ptr, uint32_t size) {
  if (!ptr)
    return malloc(size);
  if (size == 0) {
    free(ptr);
    return 0;
  }

  heap_block_t *block = (heap_block_t *)((uint8_t *)ptr - sizeof(heap_block_t));
  if (block->size >= size)
    return ptr;

  void *new_ptr = malloc(size);
  if (new_ptr) {
    memcpy(new_ptr, ptr, block->size);
    free(ptr);
  }
  return new_ptr;
}

/* ============== ATEXIT ============== */

typedef void (*atexit_func)(void);
static atexit_func atexit_funcs[32];
static int atexit_count = 0;

static inline int atexit(atexit_func func) {
  if (atexit_count >= 32)
    return -1;
  atexit_funcs[atexit_count++] = func;
  return 0;
}

static inline void _call_atexit(void) {
  while (atexit_count > 0)
    atexit_funcs[--atexit_count]();
}

/* ============== ABORT ============== */

static inline void abort(void) {
  print("Aborted\n");
  exit(134);
}

/* ============== SYSTEM ============== */

static inline int system(const char *cmd) {
  if (!cmd)
    return 1;
  int pid = fork();
  if (pid == 0) {
    /* In child - would exec shell with command */
    exit(0);
  }
  if (pid > 0)
    wait(0);
  return 0;
}

/* ============== DATA STRUCTURES ============== */

/* Dynamic array */
typedef struct {
  void **items;
  uint32_t count;
  uint32_t capacity;
} vector_t;

static inline void vector_init(vector_t *v) {
  v->items = 0;
  v->count = 0;
  v->capacity = 0;
}

static inline void vector_push(vector_t *v, void *item) {
  if (v->count >= v->capacity) {
    v->capacity = v->capacity ? v->capacity * 2 : 8;
    v->items = (void **)realloc(v->items, v->capacity * sizeof(void *));
  }
  v->items[v->count++] = item;
}

static inline void *vector_pop(vector_t *v) {
  return v->count > 0 ? v->items[--v->count] : 0;
}

static inline void vector_free(vector_t *v) {
  free(v->items);
  v->items = 0;
  v->count = 0;
  v->capacity = 0;
}

/* Hash table (simple) */
#define HASH_SIZE 256

typedef struct hash_entry {
  char *key;
  void *value;
  struct hash_entry *next;
} hash_entry_t;

typedef struct {
  hash_entry_t *buckets[HASH_SIZE];
} hash_table_t;

static inline uint32_t hash_string(const char *s) {
  uint32_t h = 5381;
  while (*s)
    h = ((h << 5) + h) + *s++;
  return h % HASH_SIZE;
}

static inline void hash_init(hash_table_t *ht) {
  memset(ht->buckets, 0, sizeof(ht->buckets));
}

static inline void hash_set(hash_table_t *ht, const char *key, void *value) {
  uint32_t idx = hash_string(key);
  hash_entry_t *e = ht->buckets[idx];
  while (e) {
    if (strcmp(e->key, key) == 0) {
      e->value = value;
      return;
    }
    e = e->next;
  }
  e = (hash_entry_t *)malloc(sizeof(hash_entry_t));
  e->key = strdup(key);
  e->value = value;
  e->next = ht->buckets[idx];
  ht->buckets[idx] = e;
}

static inline void *hash_get(hash_table_t *ht, const char *key) {
  uint32_t idx = hash_string(key);
  hash_entry_t *e = ht->buckets[idx];
  while (e) {
    if (strcmp(e->key, key) == 0)
      return e->value;
    e = e->next;
  }
  return 0;
}

/* ============== STRING BUILDER ============== */

typedef struct {
  char *data;
  uint32_t len;
  uint32_t cap;
} string_builder_t;

static inline void sb_init(string_builder_t *sb) {
  sb->data = 0;
  sb->len = 0;
  sb->cap = 0;
}

static inline void sb_append(string_builder_t *sb, const char *s) {
  uint32_t slen = strlen(s);
  if (sb->len + slen + 1 > sb->cap) {
    sb->cap = (sb->len + slen + 1) * 2;
    sb->data = (char *)realloc(sb->data, sb->cap);
  }
  memcpy(sb->data + sb->len, s, slen + 1);
  sb->len += slen;
}

static inline void sb_appendc(string_builder_t *sb, char c) {
  char s[2] = {c, 0};
  sb_append(sb, s);
}

static inline char *sb_finish(string_builder_t *sb) { return sb->data; }

static inline void sb_free(string_builder_t *sb) {
  free(sb->data);
  sb->data = 0;
  sb->len = 0;
  sb->cap = 0;
}

#endif /* _STDLIB_EXT_H */
