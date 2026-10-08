/**********************************************************************
 Freeciv - Copyright (C) 1996 - A Kjeldberg, L Gregersen, P Unold
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.
***********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#include "resource_xpm.h"
#include "resource_colors.h"

/* Limits bound DOS memory use and reject overflow before allocating.
 * The largest supplied atlas (intro) is comfortably inside these limits.
 */
#define XPM_MAX_DIMENSION 8192
#define XPM_MAX_PIXELS 16777216UL
#define XPM_MAX_FILE (64UL * 1024UL * 1024UL)
#define XPM_MAX_STRING 65536U

static char resource_error[192];

const char *dos_vbe_graphics_error(void)
{
  return resource_error;
}

void dos_resource_clear_error(void)
{
  resource_error[0] = '\0';
}

void dos_resource_error(const char *message)
{
  size_t length = strlen(message);
  if (length >= sizeof(resource_error)) {
    length = sizeof(resource_error) - 1;
  }
  memcpy(resource_error, message, length);
  resource_error[length] = '\0';
}

static int ascii_lower(int ch)
{
  return ch >= 'A' && ch <= 'Z' ? ch + 'a' - 'A' : ch;
}

static int same_name(const char *left, const char *right)
{
  while (*left && *right) {
    if (ascii_lower((unsigned char)*left)
        != ascii_lower((unsigned char)*right)) {
      return 0;
    }
    left++;
    right++;
  }
  return *left == *right;
}

static int source_name_valid(const char *name)
{
  const char *component = name, *p = name;
  size_t length = strlen(name);
  if (!length || length > 255 || *name == '/') {
    return 0;
  }
  for (;;) {
    if (*p == '/' || !*p) {
      size_t part = (size_t)(p - component);
      if (!part || (part == 1 && component[0] == '.')
          || (part == 2 && component[0] == '.' && component[1] == '.')) {
        return 0;
      }
      if (!*p) {
        return 1;
      }
      component = p + 1;
    } else if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z')
                 || (*p >= '0' && *p <= '9')
                 || *p == '_' || *p == '-' || *p == '.')) {
      return 0;
    }
    p++;
  }
}

static int alias_valid(const char *name)
{
  const char *dot = strchr(name, '.');
  size_t base = dot ? (size_t)(dot - name) : strlen(name);
  size_t suffix = dot ? strlen(dot + 1) : 0;
  size_t i;
  char stem[9];
  if (!base || base > 8 || (dot && (!suffix || suffix > 3))) {
    return 0;
  }
  for (i = 0; name[i]; i++) {
    if (dot && name + i == dot) {
      continue;
    }
    if (!((name[i] >= 'a' && name[i] <= 'z')
          || (name[i] >= 'A' && name[i] <= 'Z')
          || (name[i] >= '0' && name[i] <= '9') || name[i] == '_')) {
      return 0;
    }
  }
  memcpy(stem, name, base);
  stem[base] = '\0';
  if (same_name(stem, "CON") || same_name(stem, "PRN")
      || same_name(stem, "AUX") || same_name(stem, "NUL")
      || (base == 4 && ((ascii_lower(stem[0]) == 'c'
                        && ascii_lower(stem[1]) == 'o'
                        && ascii_lower(stem[2]) == 'm')
                       || (ascii_lower(stem[0]) == 'l'
                           && ascii_lower(stem[1]) == 'p'
                           && ascii_lower(stem[2]) == 't'))
          && stem[3] >= '1' && stem[3] <= '9')) {
    return 0;
  }
  return 1;
}

static int mapping_line(FILE *file, char *line, size_t capacity, size_t *bytes)
{
  size_t used = 0;
  int ch;
  while ((ch = fgetc(file)) != EOF) {
    if (++*bytes > 131072 || used + 1 >= capacity || ch == 0) {
      dos_resource_error("resource mapping exceeds limits or contains NUL");
      return -1;
    }
    line[used++] = (char)ch;
    if (ch == '\n') {
      break;
    }
  }
  line[used] = '\0';
  return used ? 1 : 0;
}

char *dos_vbe_resource_filename(const char *map_filename,
                                const char *original_filename)
{
  struct mapping_entry {
    char original[256], alias[13];
  };
  struct mapping_entry *entries = NULL;
  FILE *file;
  char line[1024];
  char *result = NULL;
  const char *failure = NULL;
  size_t count = 0, bytes = 0, selected = 512, i;
  int status;

  dos_resource_clear_error();
  if (!map_filename || !*map_filename || !original_filename
      || !source_name_valid(original_filename)) {
    dos_resource_error("invalid resource lookup filename");
    return NULL;
  }
  file = fopen(map_filename, "rb");
  if (!file) {
    dos_resource_error("unable to open resource mapping file");
    return NULL;
  }
  entries = calloc(512, sizeof(*entries));
  if (!entries) {
    failure = "unable to allocate resource mapping entries";
    goto done;
  }
  while ((status = mapping_line(file, line, sizeof(line), &bytes)) > 0) {
    char *tab, *start = line;
    size_t length = strlen(line);
    while (length && (line[length - 1] == '\n' || line[length - 1] == '\r')) {
      line[--length] = '\0';
    }
    while (*start == ' ' || *start == '\t') {
      start++;
    }
    if (*start == ';' || !*start) {
      continue;
    }
    tab = strchr(line, '\t');
    if (!tab || strchr(tab + 1, '\t') || count == 512) {
      failure = "invalid resource mapping columns or entry limit";
      goto done;
    }
    *tab++ = '\0';
    if (!source_name_valid(line) || !alias_valid(tab)) {
      failure = "unsafe source name or invalid 8.3 resource alias";
      goto done;
    }
    for (i = 0; i < count; i++) {
      if (same_name(line, entries[i].original)) {
        failure = "duplicate resource mapping filename";
        goto done;
      }
    }
    strcpy(entries[count].original, line);
    strcpy(entries[count].alias, tab);
    if (same_name(line, original_filename)) {
      selected = count;
    }
    count++;
  }
  if (status < 0) {
    goto done;
  }
  if (ferror(file)) {
    failure = "error reading resource mapping file";
    goto done;
  }
  if (selected == 512) {
    failure = "resource filename absent from mapping";
    goto done;
  }
  result = malloc(strlen(entries[selected].alias) + 1);
  if (!result) {
    failure = "unable to allocate resource filename alias";
    goto done;
  }
  strcpy(result, entries[selected].alias);
done:
  free(entries);
  if (fclose(file) != 0 && !failure) {
    failure = "error closing resource mapping file";
  }
  if (failure) {
    dos_resource_error(failure);
  }
  if (*resource_error) {
    free(result);
    result = NULL;
  }
  return result;
}

int dos_sprite_valid(const struct Sprite *sprite)
{
  return sprite && sprite->pixels && sprite->opacity
      && sprite->width > 0 && sprite->height > 0
      && sprite->width <= XPM_MAX_DIMENSION
      && sprite->height <= XPM_MAX_DIMENSION
      && (unsigned long)sprite->width * (unsigned long)sprite->height
         <= XPM_MAX_PIXELS;
}

struct Sprite *dos_sprite_alloc(int width, int height)
{
  struct Sprite *sprite;
  size_t count;

  if (width <= 0 || height <= 0 || width > XPM_MAX_DIMENSION
      || height > XPM_MAX_DIMENSION
      || (unsigned long)width * (unsigned long)height > XPM_MAX_PIXELS) {
    dos_resource_error("sprite dimensions exceed resource limits");
    return NULL;
  }
  count = (size_t)width * (size_t)height;
  sprite = calloc(1, sizeof(*sprite));
  if (!sprite) {
    dos_resource_error("unable to allocate sprite");
    return NULL;
  }
  sprite->width = width;
  sprite->height = height;
  sprite->pixels = malloc(count * sizeof(*sprite->pixels));
  sprite->opacity = malloc(count);
  if (!sprite->pixels || !sprite->opacity) {
    dos_resource_error("unable to allocate sprite pixels or opacity");
    free_sprite(sprite);
    return NULL;
  }
  return sprite;
}

struct xpm_reader {
  FILE *file;
  unsigned long bytes;
  int pending;
  int in_array;
};

static int next_char(struct xpm_reader *reader)
{
  int ch;
  if (reader->pending != EOF) {
    ch = reader->pending;
    reader->pending = EOF;
    return ch;
  }
  if (reader->bytes >= XPM_MAX_FILE) {
    dos_resource_error("XPM file exceeds resource limit");
    return EOF;
  }
  ch = fgetc(reader->file);
  if (ch != EOF) {
    reader->bytes++;
  }
  return ch;
}

/* Read one XPM3 C string, ignoring declarations and C comments.
 * After entering the array, only string entries and commas are accepted.
 */
static int next_token(struct xpm_reader *reader)
{
  int ch, previous;
  for (;;) {
    ch = next_char(reader);
    if (ch == EOF) {
      return EOF;
    }
    if (isspace((unsigned char)ch)) {
      continue;
    }
    if (ch == '/') {
      ch = next_char(reader);
      if (ch == '*') {
        previous = 0;
        do {
          ch = next_char(reader);
          if (ch == EOF) {
            dos_resource_error("unterminated XPM comment");
            return EOF;
          }
          if (previous == '*' && ch == '/') {
            break;
          }
          previous = ch;
        } while (1);
        continue;
      }
      if (ch == '/') {
        do {
          ch = next_char(reader);
        } while (ch != EOF && ch != '\n');
        continue;
      }
      reader->pending = ch;
      ch = '/';
    }
    return ch;
  }
}

static int next_string(struct xpm_reader *reader, char *out, size_t *length)
{
  int ch;
  size_t used = 0;
  for (;;) {
    ch = next_token(reader);
    if (ch == EOF) {
      dos_resource_error("truncated XPM or missing string");
      return -1;
    }
    if (ch == ',') {
      continue;
    }
    if (ch == '{' && !reader->in_array) {
      reader->in_array = 1;
      continue;
    }
    if (ch == '"' && reader->in_array) {
      break;
    }
    if (reader->in_array) {
      dos_resource_error("invalid XPM array entry");
      return -1;
    }
  }
  for (;;) {
    ch = next_char(reader);
    if (ch == '"' ) {
      out[used] = '\0';
      *length = used;
      return 0;
    }
    if (ch == '\\') {
      int digit, value, count;
      ch = next_char(reader);
      if (ch == '\n') {
        continue;
      }
      if (ch == '"' || ch == '\\' || ch == '\'' || ch == '?') {
        /* These are already their unescaped values. */
      } else if (ch == 't') {
        ch = '\t';
      } else if (ch >= '0' && ch <= '7') {
        value = ch - '0';
        for (count = 1; count < 3; count++) {
          digit = next_char(reader);
          if (digit < '0' || digit > '7') {
            reader->pending = digit;
            break;
          }
          value = value * 8 + digit - '0';
        }
        ch = value;
      } else if (ch == 'x') {
        value = 0;
        count = 0;
        while (1) {
          digit = next_char(reader);
          if (digit >= '0' && digit <= '9') {
            digit -= '0';
          } else if (digit >= 'a' && digit <= 'f') {
            digit = digit - 'a' + 10;
          } else if (digit >= 'A' && digit <= 'F') {
            digit = digit - 'A' + 10;
          } else {
            reader->pending = digit;
            break;
          }
          if (++count > 2) {
            dos_resource_error("oversized XPM hexadecimal C escape");
            return -1;
          }
          value = value * 16 + digit;
        }
        if (!count) {
          dos_resource_error("empty XPM hexadecimal C escape");
          return -1;
        }
        ch = value;
      } else {
        dos_resource_error("unsupported XPM C escape");
        return -1;
      }
    }
    if (ch == EOF || ch == '\n' || ch == '\r' || ch <= 0 || ch > 255
        || used >= XPM_MAX_STRING) {
      dos_resource_error("invalid, unterminated or oversized XPM string");
      return -1;
    }
    out[used++] = (char)ch;
  }
}

static int number(const char **text, unsigned long *value)
{
  unsigned long n = 0;
  const char *p = *text;
  while (isspace((unsigned char)*p)) {
    p++;
  }
  if (*p < '0' || *p > '9') {
    return -1;
  }
  do {
    unsigned long digit = (unsigned long)(*p - '0');
    if (n > (ULONG_MAX - digit) / 10UL) {
      return -1;
    }
    n = n * 10UL + digit;
    p++;
  } while (*p >= '0' && *p <= '9');
  if (*p && !isspace((unsigned char)*p)) {
    return -1;
  }
  *value = n;
  *text = p;
  return 0;
}

static int header(const char *text, int *width, int *height,
                  unsigned int *colors, unsigned int *cpp)
{
  unsigned long values[4], ignored;
  int i;
  for (i = 0; i < 4; i++) {
    if (number(&text, &values[i]) < 0) {
      return -1;
    }
  }
  while (isspace((unsigned char)*text)) {
    text++;
  }
  if (*text) {
    if (number(&text, &ignored) < 0 || number(&text, &ignored) < 0) {
      return -1;
    }
    while (isspace((unsigned char)*text)) {
      text++;
    }
    if (*text) {
      return -1;
    }
  }
  if (!values[0] || !values[1] || !values[2]
      || values[0] > XPM_MAX_DIMENSION || values[1] > XPM_MAX_DIMENSION
      || values[0] * values[1] > XPM_MAX_PIXELS
      || (values[3] != 1 && values[3] != 2)
      || values[2] > (values[3] == 1 ? 256UL : 65536UL)) {
    return -1;
  }
  *width = (int)values[0];
  *height = (int)values[1];
  *colors = (unsigned int)values[2];
  *cpp = (unsigned int)values[3];
  return 0;
}

static int hex_digit(int ch)
{
  if (ch >= '0' && ch <= '9') {
    return ch - '0';
  }
  ch = tolower((unsigned char)ch);
  return ch >= 'a' && ch <= 'f' ? ch - 'a' + 10 : -1;
}

static int color_value(const char *value, unsigned short *pixel,
                       unsigned char *opacity)
{
  unsigned int rgb[3];
  size_t i, n = strlen(value);
  char name[128];

  *opacity = 255;
  if (*value == '#') {
    size_t digits;
    if (n != 4 && n != 7 && n != 10 && n != 13) {
      return -1;
    }
    digits = (n - 1) / 3;
    for (i = 0; i < 3; i++) {
      size_t j;
      unsigned int v = 0, max = 0;
      for (j = 0; j < digits; j++) {
        int d = hex_digit(value[1 + i * digits + j]);
        if (d < 0) {
          return -1;
        }
        v = v * 16U + (unsigned int)d;
        max = max * 16U + 15U;
      }
      rgb[i] = (v * 255U + max / 2U) / max;
    }
  } else {
    size_t used = 0;
    unsigned long packed = 0;
    int found = 0;
    unsigned int variant = 0;
    for (i = 0; i < n; i++) {
      unsigned char ch = (unsigned char)value[i];
      if (!isspace(ch)) {
        if (used + 1 >= sizeof(name)) {
          return -1;
        }
        name[used++] = (char)tolower(ch);
      }
    }
    name[used] = '\0';
    if (!strcmp(name, "none")) {
      *pixel = 0;
      *opacity = 0;
      return 0;
    }
    if ((!strncmp(name, "gray", 4) || !strncmp(name, "grey", 4))
        && name[4] >= '0' && name[4] <= '9') {
      const char *p = name + 4;
      unsigned long shade;
      if (number(&p, &shade) < 0 || *p || shade > 100) {
        return -1;
      }
      packed = (unsigned long)dos_x11_gray[shade] * 0x010101UL;
      found = 1;
    }
    if (!found && used && name[used - 1] >= '1' && name[used - 1] <= '4') {
      variant = (unsigned int)(name[used - 1] - '0');
      name[--used] = '\0';
    }
    for (i = 0; !found
         && i < sizeof(dos_x11_colors) / sizeof(dos_x11_colors[0]); i++) {
      if (!strcmp(name, dos_x11_colors[i].name)
          && (!variant || dos_x11_colors[i].rgb[variant])) {
        packed = dos_x11_colors[i].rgb[variant];
        found = 1;
      }
    }
    if (!found) {
      return -1;
    }
    rgb[0] = (unsigned int)((packed >> 16) & 255UL);
    rgb[1] = (unsigned int)((packed >> 8) & 255UL);
    rgb[2] = (unsigned int)(packed & 255UL);
  }
  *pixel = (unsigned short)(((rgb[0] >> 3) << 11)
                           | ((rgb[1] >> 2) << 5) | (rgb[2] >> 3));
  return 0;
}

static int color_key(const char *start, size_t len)
{
  return (len == 1 && (*start == 'c' || *start == 'm' || *start == 'g'
                      || *start == 's'))
      || (len == 2 && start[0] == 'g' && start[1] == '4');
}

/* Palette values may contain spaces; another recognized key ends a value.
 * Prefer true color, then grayscale, then monochrome. Symbolic names alone
 * cannot be resolved without an application-provided symbol table.
 */
static int palette(const char *text, unsigned int cpp,
                   unsigned short *pixel, unsigned char *opacity)
{
  const char *p = text + cpp;
  char selected[256];
  int priority = 0;
  while (*p) {
    const char *key, *begin, *end, *scan;
    size_t keylen, length;
    int rank;
    while (isspace((unsigned char)*p)) {
      p++;
    }
    if (!*p) {
      break;
    }
    key = p;
    while (*p && !isspace((unsigned char)*p)) {
      p++;
    }
    keylen = (size_t)(p - key);
    if (!color_key(key, keylen)) {
      return -1;
    }
    rank = *key == 'c' ? 4 : *key == 'g' ? (keylen == 1 ? 3 : 2)
         : *key == 'm' ? 1 : 0;
    while (isspace((unsigned char)*p)) {
      p++;
    }
    begin = p;
    end = p + strlen(p);
    scan = p;
    while (*scan) {
      const char *token;
      while (*scan && !isspace((unsigned char)*scan)) {
        scan++;
      }
      while (isspace((unsigned char)*scan)) {
        scan++;
      }
      token = scan;
      while (*scan && !isspace((unsigned char)*scan)) {
        scan++;
      }
      if (color_key(token, (size_t)(scan - token))) {
        end = token;
        break;
      }
    }
    p = end;
    while (end > begin && isspace((unsigned char)end[-1])) {
      end--;
    }
    length = (size_t)(end - begin);
    if (!length || length >= sizeof(selected)) {
      return -1;
    }
    if (rank > priority) {
      memcpy(selected, begin, length);
      selected[length] = '\0';
      priority = rank;
    }
  }
  return priority ? color_value(selected, pixel, opacity) : -1;
}

struct palette_entry {
  unsigned short pixel;
  unsigned char opacity, present;
};

static unsigned int symbol(const char *text, unsigned int cpp)
{
  unsigned int code = (unsigned char)text[0];
  return cpp == 1 ? code : code * 256U + (unsigned char)text[1];
}

struct Sprite *dos_xpm_load(const char *filename)
{
  struct xpm_reader reader;
  struct Sprite *sprite = NULL;
  struct palette_entry *entries = NULL;
  char *line = NULL;
  const char *failure = NULL;
  size_t length;
  unsigned int colors = 0, cpp = 0, i;
  int width = 0, height = 0, x, y;

  dos_resource_clear_error();
  if (!filename || !*filename) {
    dos_resource_error("missing XPM filename");
    return NULL;
  }
  memset(&reader, 0, sizeof(reader));
  reader.pending = EOF;
  reader.file = fopen(filename, "rb");
  if (!reader.file) {
    dos_resource_error("unable to open XPM file");
    return NULL;
  }
  line = malloc(XPM_MAX_STRING + 1U);
  if (!line) {
    failure = "unable to allocate XPM line";
    goto done;
  }
  if (next_string(&reader, line, &length) < 0) {
    goto done;
  }
  if (header(line, &width, &height, &colors, &cpp) < 0) {
    failure = "invalid XPM header (limits, cpp or unsupported extension)";
    goto done;
  }
  entries = calloc(cpp == 1 ? 256U : 65536U, sizeof(*entries));
  if (!entries) {
    failure = "unable to allocate XPM palette";
    goto done;
  }
  for (i = 0; i < colors; i++) {
    unsigned int code;
    if (next_string(&reader, line, &length) < 0) {
      goto done;
    }
    if (length <= cpp) {
      failure = "truncated XPM palette";
      goto done;
    }
    code = symbol(line, cpp);
    if (entries[code].present) {
      failure = "duplicate XPM palette symbol";
      goto done;
    }
    if (palette(line, cpp, &entries[code].pixel, &entries[code].opacity) < 0) {
      failure = "invalid or unknown XPM palette color";
      goto done;
    }
    entries[code].present = 1;
  }
  sprite = dos_sprite_alloc(width, height);
  if (!sprite) {
    goto done;
  }
  for (y = 0; y < height; y++) {
    if (next_string(&reader, line, &length) < 0) {
      goto done;
    }
    if (length != (size_t)width * cpp) {
      failure = "incorrect XPM pixel row length";
      goto done;
    }
    for (x = 0; x < width; x++) {
      unsigned int code = symbol(line + (size_t)x * cpp, cpp);
      size_t offset = (size_t)y * (size_t)width + (size_t)x;
      if (!entries[code].present) {
        failure = "XPM pixel references undefined palette symbol";
        goto done;
      }
      sprite->pixels[offset] = entries[code].pixel;
      sprite->opacity[offset] = entries[code].opacity;
    }
  }
  {
    int token = next_token(&reader);
    if (token == ',') {
      token = next_token(&reader);
    }
    if (token != '}') {
      failure = "missing XPM array end or excess pixel rows";
      goto done;
    }
    token = next_token(&reader);
    if (token == ';') {
      token = next_token(&reader);
    }
    if (token != EOF) {
      failure = "unexpected trailing XPM data";
    }
  }
done:
  if (ferror(reader.file) && !failure) {
    failure = "error reading XPM file";
  }
  if (fclose(reader.file) != 0 && !failure) {
    failure = "error closing XPM file";
  }
  free(entries);
  free(line);
  if (failure) {
    dos_resource_error(failure);
  }
  if (*resource_error) {
    free_sprite(sprite);
    sprite = NULL;
  }
  return sprite;
}
