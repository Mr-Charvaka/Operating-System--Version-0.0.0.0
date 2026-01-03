/*
 * regex.h - Simple Regular Expression Engine for Retro-OS
 * Implements basic regex matching from scratch
 */
#ifndef _REGEX_H
#define _REGEX_H

#include "userlib.h"

/* Regex match result */
typedef struct {
  int matched;
  int start;
  int end;
} regex_match_t;

/* Simple regex: supports . * ^ $ [] */
static inline int regex_match_here(const char *regex, const char *text);

static inline int regex_match_star(int c, const char *regex, const char *text) {
  do {
    if (regex_match_here(regex, text))
      return 1;
  } while (*text != '\0' && (*text++ == c || c == '.'));
  return 0;
}

static inline int regex_match_plus(int c, const char *regex, const char *text) {
  while (*text != '\0' && (*text == c || c == '.')) {
    text++;
    if (regex_match_here(regex, text))
      return 1;
  }
  return 0;
}

static inline int regex_match_here(const char *regex, const char *text) {
  if (regex[0] == '\0')
    return 1;
  if (regex[0] == '$' && regex[1] == '\0')
    return *text == '\0';
  if (regex[1] == '*')
    return regex_match_star(regex[0], regex + 2, text);
  if (regex[1] == '+')
    return regex_match_plus(regex[0], regex + 2, text);
  if (regex[1] == '?') {
    if (regex_match_here(regex + 2, text))
      return 1;
    if (*text != '\0' && (regex[0] == '.' || regex[0] == *text))
      return regex_match_here(regex + 2, text + 1);
    return 0;
  }
  if (*text != '\0' && (regex[0] == '.' || regex[0] == *text))
    return regex_match_here(regex + 1, text + 1);
  return 0;
}

static inline int regex_match(const char *regex, const char *text) {
  if (regex[0] == '^')
    return regex_match_here(regex + 1, text);
  do {
    if (regex_match_here(regex, text))
      return 1;
  } while (*text++ != '\0');
  return 0;
}

/* Extended: find match position */
static inline regex_match_t regex_find(const char *regex, const char *text) {
  regex_match_t result = {0, -1, -1};
  const char *start = text;
  int anchored = (regex[0] == '^');
  if (anchored)
    regex++;

  do {
    const char *t = text;
    const char *r = regex;
    int pos = text - start;

    if (regex_match_here(r, t)) {
      result.matched = 1;
      result.start = pos;
      /* Find end by matching */
      while (*t && regex_match_here(r, t + 1))
        t++;
      result.end = t - start;
      return result;
    }
    if (anchored)
      break;
  } while (*text++ != '\0');

  return result;
}

/* Replace first match */
static inline int regex_replace(const char *regex, const char *replacement,
                                const char *text, char *output,
                                uint32_t outsize) {
  regex_match_t m = regex_find(regex, text);
  if (!m.matched) {
    strncpy(output, text, outsize - 1);
    output[outsize - 1] = 0;
    return 0;
  }

  char *p = output;
  uint32_t rem = outsize - 1;

  /* Copy before match */
  uint32_t before = m.start;
  if (before > rem)
    before = rem;
  memcpy(p, text, before);
  p += before;
  rem -= before;

  /* Copy replacement */
  uint32_t rlen = strlen(replacement);
  if (rlen > rem)
    rlen = rem;
  memcpy(p, replacement, rlen);
  p += rlen;
  rem -= rlen;

  /* Copy after match */
  const char *after = text + m.end;
  uint32_t alen = strlen(after);
  if (alen > rem)
    alen = rem;
  memcpy(p, after, alen);
  p += alen;

  *p = 0;
  return 1;
}

/* Check if string matches glob pattern */
static inline int glob_match(const char *pattern, const char *text) {
  while (*pattern) {
    if (*pattern == '*') {
      pattern++;
      if (*pattern == '\0')
        return 1;
      while (*text) {
        if (glob_match(pattern, text))
          return 1;
        text++;
      }
      return 0;
    } else if (*pattern == '?') {
      if (*text == '\0')
        return 0;
      pattern++;
      text++;
    } else if (*pattern == '[') {
      pattern++;
      int invert = 0;
      if (*pattern == '!' || *pattern == '^') {
        invert = 1;
        pattern++;
      }
      int match = 0;
      while (*pattern && *pattern != ']') {
        if (pattern[1] == '-' && pattern[2] && pattern[2] != ']') {
          if (*text >= pattern[0] && *text <= pattern[2])
            match = 1;
          pattern += 3;
        } else {
          if (*text == *pattern)
            match = 1;
          pattern++;
        }
      }
      if (*pattern == ']')
        pattern++;
      if (match == invert)
        return 0;
      text++;
    } else {
      if (*pattern != *text)
        return 0;
      pattern++;
      text++;
    }
  }
  return *text == '\0';
}

#endif /* _REGEX_H */
