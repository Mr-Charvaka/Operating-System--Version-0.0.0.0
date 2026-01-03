/*
 * crypto.h - Cryptographic Functions for Retro-OS
 * MD5, SHA1, SHA256, Base64 implementations from scratch
 */
#ifndef _CRYPTO_H
#define _CRYPTO_H

#include "userlib.h"

/* ============== MD5 ============== */

typedef struct {
  uint32_t state[4];
  uint32_t count[2];
  uint8_t buffer[64];
} md5_ctx_t;

static const uint32_t md5_k[64] = {
    0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a,
    0xa8304613, 0xfd469501, 0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
    0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821, 0xf61e2562, 0xc040b340,
    0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
    0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8,
    0x676f02d9, 0x8d2a4c8a, 0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
    0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70, 0x289b7ec6, 0xeaa127fa,
    0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
    0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92,
    0xffeff47d, 0x85845dd1, 0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
    0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391};

static const uint8_t md5_s[64] = {
    7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
    5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20,
    4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
    6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};

#define ROTL32(x, n) (((x) << (n)) | ((x) >> (32 - (n))))

static inline void md5_transform(md5_ctx_t *ctx, const uint8_t *block) {
  uint32_t a = ctx->state[0], b = ctx->state[1];
  uint32_t c = ctx->state[2], d = ctx->state[3];
  uint32_t m[16];

  for (int i = 0; i < 16; i++)
    m[i] = block[i * 4] | (block[i * 4 + 1] << 8) | (block[i * 4 + 2] << 16) |
           (block[i * 4 + 3] << 24);

  for (int i = 0; i < 64; i++) {
    uint32_t f, g;
    if (i < 16) {
      f = (b & c) | (~b & d);
      g = i;
    } else if (i < 32) {
      f = (d & b) | (~d & c);
      g = (5 * i + 1) % 16;
    } else if (i < 48) {
      f = b ^ c ^ d;
      g = (3 * i + 5) % 16;
    } else {
      f = c ^ (b | ~d);
      g = (7 * i) % 16;
    }

    uint32_t temp = d;
    d = c;
    c = b;
    b = b + ROTL32(a + f + md5_k[i] + m[g], md5_s[i]);
    a = temp;
  }

  ctx->state[0] += a;
  ctx->state[1] += b;
  ctx->state[2] += c;
  ctx->state[3] += d;
}

static inline void md5_init(md5_ctx_t *ctx) {
  ctx->state[0] = 0x67452301;
  ctx->state[1] = 0xefcdab89;
  ctx->state[2] = 0x98badcfe;
  ctx->state[3] = 0x10325476;
  ctx->count[0] = ctx->count[1] = 0;
}

static inline void md5_update(md5_ctx_t *ctx, const uint8_t *data,
                              uint32_t len) {
  uint32_t i = (ctx->count[0] >> 3) & 63;
  ctx->count[0] += len << 3;
  if (ctx->count[0] < (len << 3))
    ctx->count[1]++;
  ctx->count[1] += len >> 29;

  uint32_t part = 64 - i;
  uint32_t j = 0;
  if (len >= part) {
    memcpy(ctx->buffer + i, data, part);
    md5_transform(ctx, ctx->buffer);
    for (j = part; j + 63 < len; j += 64)
      md5_transform(ctx, data + j);
    i = 0;
  }
  memcpy(ctx->buffer + i, data + j, len - j);
}

static inline void md5_final(md5_ctx_t *ctx, uint8_t *digest) {
  uint8_t bits[8];
  for (int i = 0; i < 4; i++) {
    bits[i] = ctx->count[0] >> (i * 8);
    bits[i + 4] = ctx->count[1] >> (i * 8);
  }

  uint32_t index = (ctx->count[0] >> 3) & 63;
  uint32_t padlen = (index < 56) ? (56 - index) : (120 - index);

  uint8_t padding[64] = {0x80};
  md5_update(ctx, padding, padlen);
  md5_update(ctx, bits, 8);

  for (int i = 0; i < 4; i++)
    for (int j = 0; j < 4; j++)
      digest[i * 4 + j] = ctx->state[i] >> (j * 8);
}

static inline void md5_hash(const uint8_t *data, uint32_t len,
                            uint8_t *digest) {
  md5_ctx_t ctx;
  md5_init(&ctx);
  md5_update(&ctx, data, len);
  md5_final(&ctx, digest);
}

/* ============== SHA256 ============== */

typedef struct {
  uint32_t state[8];
  uint64_t count;
  uint8_t buffer[64];
} sha256_ctx_t;

static const uint32_t sha256_k[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
    0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
    0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
    0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
    0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
    0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

#define ROTR32(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
#define CH(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x) (ROTR32(x, 2) ^ ROTR32(x, 13) ^ ROTR32(x, 22))
#define EP1(x) (ROTR32(x, 6) ^ ROTR32(x, 11) ^ ROTR32(x, 25))
#define SIG0(x) (ROTR32(x, 7) ^ ROTR32(x, 18) ^ ((x) >> 3))
#define SIG1(x) (ROTR32(x, 17) ^ ROTR32(x, 19) ^ ((x) >> 10))

static inline void sha256_transform(sha256_ctx_t *ctx, const uint8_t *block) {
  uint32_t w[64], a, b, c, d, e, f, g, h, t1, t2;

  for (int i = 0; i < 16; i++)
    w[i] = (block[i * 4] << 24) | (block[i * 4 + 1] << 16) |
           (block[i * 4 + 2] << 8) | block[i * 4 + 3];
  for (int i = 16; i < 64; i++)
    w[i] = SIG1(w[i - 2]) + w[i - 7] + SIG0(w[i - 15]) + w[i - 16];

  a = ctx->state[0];
  b = ctx->state[1];
  c = ctx->state[2];
  d = ctx->state[3];
  e = ctx->state[4];
  f = ctx->state[5];
  g = ctx->state[6];
  h = ctx->state[7];

  for (int i = 0; i < 64; i++) {
    t1 = h + EP1(e) + CH(e, f, g) + sha256_k[i] + w[i];
    t2 = EP0(a) + MAJ(a, b, c);
    h = g;
    g = f;
    f = e;
    e = d + t1;
    d = c;
    c = b;
    b = a;
    a = t1 + t2;
  }

  ctx->state[0] += a;
  ctx->state[1] += b;
  ctx->state[2] += c;
  ctx->state[3] += d;
  ctx->state[4] += e;
  ctx->state[5] += f;
  ctx->state[6] += g;
  ctx->state[7] += h;
}

static inline void sha256_init(sha256_ctx_t *ctx) {
  ctx->state[0] = 0x6a09e667;
  ctx->state[1] = 0xbb67ae85;
  ctx->state[2] = 0x3c6ef372;
  ctx->state[3] = 0xa54ff53a;
  ctx->state[4] = 0x510e527f;
  ctx->state[5] = 0x9b05688c;
  ctx->state[6] = 0x1f83d9ab;
  ctx->state[7] = 0x5be0cd19;
  ctx->count = 0;
}

static inline void sha256_update(sha256_ctx_t *ctx, const uint8_t *data,
                                 uint32_t len) {
  uint32_t i = ctx->count & 63;
  ctx->count += len;

  if (i + len < 64) {
    memcpy(ctx->buffer + i, data, len);
    return;
  }

  memcpy(ctx->buffer + i, data, 64 - i);
  sha256_transform(ctx, ctx->buffer);
  data += 64 - i;
  len -= 64 - i;

  while (len >= 64) {
    sha256_transform(ctx, data);
    data += 64;
    len -= 64;
  }
  memcpy(ctx->buffer, data, len);
}

static inline void sha256_final(sha256_ctx_t *ctx, uint8_t *digest) {
  uint32_t i = ctx->count & 63;
  ctx->buffer[i++] = 0x80;

  if (i > 56) {
    memset(ctx->buffer + i, 0, 64 - i);
    sha256_transform(ctx, ctx->buffer);
    i = 0;
  }
  memset(ctx->buffer + i, 0, 56 - i);

  uint64_t bits = ctx->count * 8;
  for (int j = 0; j < 8; j++)
    ctx->buffer[63 - j] = bits >> (j * 8);
  sha256_transform(ctx, ctx->buffer);

  for (int j = 0; j < 8; j++)
    for (int k = 0; k < 4; k++)
      digest[j * 4 + k] = ctx->state[j] >> (24 - k * 8);
}

static inline void sha256_hash(const uint8_t *data, uint32_t len,
                               uint8_t *digest) {
  sha256_ctx_t ctx;
  sha256_init(&ctx);
  sha256_update(&ctx, data, len);
  sha256_final(&ctx, digest);
}

/* ============== BASE64 ============== */

static const char base64_chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static inline uint32_t base64_encode(const uint8_t *in, uint32_t inlen,
                                     char *out) {
  char *p = out;
  uint32_t i;
  for (i = 0; i + 2 < inlen; i += 3) {
    *p++ = base64_chars[in[i] >> 2];
    *p++ = base64_chars[((in[i] & 3) << 4) | (in[i + 1] >> 4)];
    *p++ = base64_chars[((in[i + 1] & 15) << 2) | (in[i + 2] >> 6)];
    *p++ = base64_chars[in[i + 2] & 63];
  }
  if (i < inlen) {
    *p++ = base64_chars[in[i] >> 2];
    if (i + 1 < inlen) {
      *p++ = base64_chars[((in[i] & 3) << 4) | (in[i + 1] >> 4)];
      *p++ = base64_chars[(in[i + 1] & 15) << 2];
    } else {
      *p++ = base64_chars[(in[i] & 3) << 4];
      *p++ = '=';
    }
    *p++ = '=';
  }
  *p = 0;
  return p - out;
}

static inline int base64_decode_char(char c) {
  if (c >= 'A' && c <= 'Z')
    return c - 'A';
  if (c >= 'a' && c <= 'z')
    return c - 'a' + 26;
  if (c >= '0' && c <= '9')
    return c - '0' + 52;
  if (c == '+')
    return 62;
  if (c == '/')
    return 63;
  return -1;
}

static inline uint32_t base64_decode(const char *in, uint8_t *out) {
  uint8_t *p = out;
  int v[4];
  while (*in) {
    for (int i = 0; i < 4 && *in; i++) {
      if (*in == '=') {
        v[i] = 0;
        in++;
      } else
        v[i] = base64_decode_char(*in++);
    }
    *p++ = (v[0] << 2) | (v[1] >> 4);
    if (in[-2] != '=')
      *p++ = (v[1] << 4) | (v[2] >> 2);
    if (in[-1] != '=')
      *p++ = (v[2] << 6) | v[3];
  }
  return p - out;
}

/* Hex encoding */
static inline void hex_encode(const uint8_t *in, uint32_t len, char *out) {
  const char hex[] = "0123456789abcdef";
  for (uint32_t i = 0; i < len; i++) {
    *out++ = hex[in[i] >> 4];
    *out++ = hex[in[i] & 15];
  }
  *out = 0;
}

#endif /* _CRYPTO_H */
