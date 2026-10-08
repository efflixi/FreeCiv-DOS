/**********************************************************************
 Freeciv - Copyright (C) 1996 - A Kjeldberg, L Gregersen, P Unold
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.
***********************************************************************/
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "graphics.h"
#include "framebuffer.h"

void *__real_malloc(size_t size);
void *__real_calloc(size_t count, size_t size);
void *__wrap_malloc(size_t size);
void *__wrap_calloc(size_t count, size_t size);

static unsigned int allocation, fail_at;

void *__wrap_malloc(size_t size)
{
  if (fail_at && ++allocation == fail_at) {
    return NULL;
  }
  return __real_malloc(size);
}

void *__wrap_calloc(size_t count, size_t size)
{
  if (fail_at && ++allocation == fail_at) {
    return NULL;
  }
  return __real_calloc(count, size);
}

static void fixture(const char *path, const char *text)
{
  FILE *file = fopen(path, "wb");
  assert(file);
  assert(fwrite(text, 1, strlen(text), file) == strlen(text));
  assert(fclose(file) == 0);
}

static struct Sprite *load(const char *path)
{
  struct Sprite *sprite = load_gfxfile(path);
  if (!sprite) {
    fprintf(stderr, "%s: %s\n", path, dos_vbe_graphics_error());
  }
  assert(sprite);
  assert(!*dos_vbe_graphics_error());
  return sprite;
}

static unsigned int pixel(const struct dos_vbe_framebuffer *fb,
                           unsigned int x, unsigned int y)
{
  size_t offset = (size_t)y * fb->stride + (size_t)x * 2U;
  return (unsigned int)fb->pixels[offset]
       | ((unsigned int)fb->pixels[offset + 1] << 8);
}

static void mapping(const char *path, const char *staged_map)
{
  char *alias, *again;
  unsigned int i;
  FILE *file;
  char line[1024];
  const char *invalid[] = {
    "",
    "trident.tilespec\n",
    "trident.tilespec\tTRIDENT.TSP\textra\n",
    "trident.tilespec\t../TRIDENT.TSP\n",
    "trident.tilespec\tTRIDENT.TILESPEC\n",
    "trident.tilespec\t.TSP\n",
    "trident.tilespec\tTRIDENT.\n",
    "trident.tilespec\tC:FOO.TSP\n",
    "../trident.tilespec\tTRIDENT.TSP\n",
    "misc//intro\tG0000000\n",
    "trident.tilespec\tCON.TSP\n",
    "trident.tilespec\tcom1.TSP\n",
    "trident.tilespec\tTRIDENT.TSP\nTRIDENT.TILESPEC\tOTHER.TSP\n",
    "trident.tilespec\tTRIDENT.TSP\nmalformed\n",
    "trident.tilespec\tTRIDENT.TSP\rgarbage\n",
    "\rtrident.tilespec\tTRIDENT.TSP\n"
  };
  assert(!strcmp(DOS_VBE_TILESPEC_SUFFIX, ".TSP"));
  fixture(path, "; comment\r\n\ntrident.tilespec\tTRIDENT.TSP\r\n"
                "misc/intro\tG0000000\n");
  alias = dos_vbe_resource_filename(path, "TRIDENT.tilespec");
  assert(alias && !strcmp(alias, "TRIDENT.TSP"));
  again = dos_vbe_resource_filename(path, "trident.tilespec");
  assert(again && again != alias && !strcmp(again, alias));
  free(again);
  assert(!strcmp(alias, "TRIDENT.TSP"));
  again = dos_vbe_resource_filename(path, "misc/intro");
  assert(again && !strcmp(again, "G0000000"));
  free(again);
  free(alias);
  assert(!dos_vbe_resource_filename(path, "absent.tilespec"));
  assert(*dos_vbe_graphics_error());
  for (i = 1; i <= 2; i++) {
    allocation = 0;
    fail_at = i;
    assert(!dos_vbe_resource_filename(path, "trident.tilespec"));
    fail_at = 0;
    assert(*dos_vbe_graphics_error());
  }
  for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
    fixture(path, invalid[i]);
    assert(!dos_vbe_resource_filename(path, "trident.tilespec"));
    assert(*dos_vbe_graphics_error());
  }
  {
    static const char binary[] = "trident.tilespec\tTRIDENT.TSP\0hidden\n";
    file = fopen(path, "wb");
    assert(file && fwrite(binary, 1, sizeof(binary) - 1, file) == sizeof(binary) - 1
           && fclose(file) == 0);
    assert(!dos_vbe_resource_filename(path, "trident.tilespec"));
  }
  file = fopen(path, "wb");
  assert(file && fputc(';', file) != EOF);
  for (i = 0; i < 1024; i++) {
    assert(fputc('a', file) != EOF);
  }
  assert(fclose(file) == 0);
  assert(!dos_vbe_resource_filename(path, "trident.tilespec"));
  file = fopen(path, "wb");
  assert(file);
  for (i = 0; i < 513; i++) {
    assert(fprintf(file, "set%u.tilespec\tTRIDENT.TSP\n", i) > 0);
  }
  assert(fclose(file) == 0);
  assert(!dos_vbe_resource_filename(path, "set0.tilespec"));
  file = fopen(path, "wb");
  assert(file);
  for (i = 0; i < 65537; i++) {
    assert(fputs(";\n", file) >= 0);
  }
  assert(fclose(file) == 0);
  assert(!dos_vbe_resource_filename(path, "trident.tilespec"));
  assert(remove(path) == 0);
  assert(!dos_vbe_resource_filename(path, "trident.tilespec"));
  assert(!dos_vbe_resource_filename(NULL, "trident.tilespec"));
  assert(!dos_vbe_resource_filename(path, NULL));
  assert(!dos_vbe_resource_filename(path, "/trident.tilespec"));
  alias = dos_vbe_resource_filename(staged_map, "TRIDENT.TILESPEC");
  assert(alias && !strcmp(alias, "TRIDENT.TSP"));
  free(alias);
  file = fopen(staged_map, "rb");
  assert(file);
  i = 0;
  while (fgets(line, sizeof(line), file)) {
    char original[256], target[13];
    if (*line == ';') {
      continue;
    }
    assert(sscanf(line, "%255[^\t]\t%12s", original, target) == 2);
    alias = dos_vbe_resource_filename(staged_map, original);
    assert(alias && !strcmp(alias, target));
    free(alias);
    i++;
  }
  assert(i == 32 && !ferror(file) && fclose(file) == 0);
  puts("Production RESMAP lookup: all 32 aliases, repetition and invalid-map cleanup passed.");
}

static void drawing(const struct Sprite *sprite)
{
  struct dos_vbe_framebuffer fb;
  struct dos_vbe_dirty_rect dirty;
  unsigned int x, y;

  memset(&fb, 0, sizeof(fb));
  assert(dos_vbe_framebuffer_init_pitch(&fb, 4, 3, 16, 12) == 0);
  memset(fb.pixels, 0xa5, fb.size_bytes);
  assert(dos_vbe_framebuffer_dirty_clear(&fb) == 0);
  assert(dos_vbe_sprite_draw(&fb, sprite, -1, -1) == 0);
  for (y = 0; y < fb.height; y++) {
    for (x = 0; x < fb.width; x++) {
      size_t src = (size_t)(y + 1) * (size_t)sprite->width + x + 1;
      unsigned int expected = 0xa5a5;
      if (x + 1 < (unsigned int)sprite->width
          && y + 1 < (unsigned int)sprite->height && sprite->opacity[src]) {
        expected = sprite->pixels[src];
      }
      assert(pixel(&fb, x, y) == expected);
    }
    for (x = 8; x < fb.stride; x++) {
      assert(fb.pixels[(size_t)y * fb.stride + x] == 0xa5);
    }
  }
  assert(dos_vbe_framebuffer_validate(&fb) == 0);
  assert(dos_vbe_framebuffer_dirty_clear(&fb) == 0);
  assert(dos_vbe_sprite_draw(&fb, sprite, INT_MIN, INT_MIN) == 0);
  assert(dos_vbe_sprite_draw(&fb, sprite, INT_MAX, INT_MAX) == 0);
  assert(dos_vbe_framebuffer_dirty_peek(&fb, &dirty) == 0);
  assert(dos_vbe_sprite_draw_region(&fb, sprite, 0, 0, INT_MAX, 0, 1, 1) < 0);
  assert(dos_vbe_sprite_draw_region(&fb, sprite, 0, 0, 0, 0, INT_MAX, 1) < 0);
  assert(dos_vbe_sprite_draw_region(&fb, sprite, 0, 0, 0, 0, 0, 1) < 0);
  assert(dos_vbe_sprite_draw(&fb, NULL, 0, 0) < 0);
  assert(dos_vbe_framebuffer_dirty_peek(&fb, &dirty) == 0);
  dos_vbe_framebuffer_destroy(&fb);
}

static void synthetic(const char *path)
{
  struct Sprite *sprite, *crop;
  struct dos_vbe_framebuffer fb;
  struct dos_vbe_dirty_rect dirty;
  unsigned int i;
  int w, h;
  const char *valid =
    "/* XPM: comments with misleading \\\"quotes\\\" */\n"
    "static char *x[]={\n"
    "\"4 2 8 2 0 0\",\n"
    "\"   c None\",\n"
    "\"AA s name m white g4 gray50 g gray90 c black\",\n"
    "\"BB c #ff00ff\",\n"
    "\"CC c LiGhT Goldenrod Yellow\",\n"
    "\"DD c #0f0\",\n"
    "\"EE c #000fff000\",\n"
    "\"FF c #0000ffff0000\",\n"
    "\"GG c goldenrod3\",\n"
    "\"  AABBCC\",\n"
    "\"DDEEFFGG\",\n"
    "}; /* final comment */\n";
  const char *invalid[] = {
    "", "garbage",
    "/* unclosed",
    "{ \"0 1 1 1\", \"a c red\", \"a\" };",
    "{ \"1 0 1 1\", \"a c red\", \"a\" };",
    "{ \"999999999999999999999 1 1 1\" };",
    "{ \"8193 1 1 1\" };",
    "{ \"8192 8192 1 1\" };",
    "{ \"1 1 0 1\" };",
    "{ \"1 1 1 3\" };",
    "{ \"1 1 257 1\" };",
    "{ \"1 1 65537 2\" };",
    "{ \"1 1 1 1 XPMEXT\" };",
    "{ \"1 1 1 1\", \"a c red\", \"b\" };",
    "{ \"1 1 1 1\", \"a c red\", \"aa\" };",
    "{ \"2 1 1 1\", \"a c red\", \"a\" };",
    "{ \"1 1 2 1\", \"a c red\", \"a c blue\", \"a\" };",
    "{ \"1 1 1 1\", \"a c madeup\", \"a\" };",
    "{ \"1 1 1 1\", \"a c gray101\", \"a\" };",
    "{ \"1 1 1 1\", \"a c gray50x\", \"a\" };",
    "{ \"1 1 1 1\", \"a c #00000g\", \"a\" };",
    "{ \"1 1 1 1\", \"a c #00\", \"a\" };",
    "{ \"1 1 1 1\", \"a s symbolic\", \"a\" };",
    "{ \"1 1 1 1\", \"a z red\", \"a\" };",
    "{ \"1 1 1 1\", \"a c\", \"a\" };",
    "{ \"1 1 1 1\", \"a c red\",",
    "{ \"1 1 1 1\", \"a c red\", \"a\"",
    "{ \"1 1 1 1\", \"a c red\", \"a\", \"a\" };",
    "{ \"1 1 1 1\", \"a c red\", \"a\" }; extra",
    "{ \"1 1 1 1\", \"a c red\", \"\\000\" };",
    "{ \"1 1 1 1\", \"a c red\", \"\\x0000\" };",
    "{ \"1 1 1 1\", \"a c red\", \"\\x\" };",
    "{ \"1 1 1 1\", \"a c red\", \"a\" }; /* unclosed"
  };
  fixture(path, valid);
  sprite = load(path);
  get_sprite_dimensions(sprite, &w, &h);
  assert(w == 4 && h == 2);
  assert(sprite->opacity[0] == 0 && sprite->pixels[0] == 0);
  assert(sprite->opacity[1] == 255 && sprite->pixels[1] == 0);
  assert(sprite->opacity[2] == 255 && sprite->pixels[2] == 0xf81f);
  assert(sprite->pixels[3] == 0xffda);
  assert(sprite->pixels[4] == 0x07e0 && sprite->pixels[5] == 0x07e0
         && sprite->pixels[6] == 0x07e0);
  assert(sprite->pixels[7] == 0xccc3);
  memset(&fb, 0, sizeof(fb));
  assert(dos_vbe_framebuffer_init(&fb, 4, 2, 16) == 0);
  dos_vbe_framebuffer_clear(&fb, 0x1234);
  assert(dos_vbe_sprite_draw(&fb, sprite, 0, 0) == 0);
  assert(pixel(&fb, 0, 0) == 0x1234 && pixel(&fb, 1, 0) == 0);
  assert(pixel(&fb, 2, 0) == 0xf81f);
  assert(dos_vbe_framebuffer_dirty_clear(&fb) == 0);
  assert(dos_vbe_sprite_draw_region(&fb, sprite, 0, 0, 0, 0, 1, 1) == 0);
  assert(dos_vbe_framebuffer_dirty_peek(&fb, &dirty) == 0);
  assert(dos_vbe_sprite_draw_region(&fb, sprite, 1, 1, 2, 0, 2, 1) == 0);
  assert(pixel(&fb, 1, 1) == 0xf81f && pixel(&fb, 2, 1) == 0xffda);
  assert(dos_vbe_framebuffer_dirty_peek(&fb, &dirty) == 1);
  assert(dirty.x == 1 && dirty.y == 1 && dirty.width == 2 && dirty.height == 1);
  assert(dos_vbe_sprite_draw(NULL, sprite, 0, 0) < 0);
  dos_vbe_framebuffer_destroy(&fb);
  crop = crop_sprite(sprite, 1, 0, 2, 2);
  assert(crop && crop->pixels != sprite->pixels && crop->opacity != sprite->opacity);
  assert(!crop_sprite(sprite, -1, 0, 1, 1));
  assert(!crop_sprite(sprite, INT_MAX, INT_MAX, 1, 1));
  assert(!crop_sprite(sprite, 1, 0, INT_MAX, 1));
  assert(!crop_sprite(sprite, 0, 0, 0, 1));
  assert(!crop_sprite(NULL, 0, 0, 1, 1));
  free_sprite(sprite);
  assert(crop->pixels[0] == 0 && crop->opacity[0] == 255);
  free_sprite(crop);
  free_sprite(NULL);
  get_sprite_dimensions(NULL, &w, &h);
  assert(w == 0 && h == 0);
  for (i = 1; i <= 5; i++) {
    allocation = 0;
    fail_at = i;
    assert(!load_gfxfile(path));
    fail_at = 0;
    assert(*dos_vbe_graphics_error());
  }
  sprite = load(path);
  for (i = 1; i <= 3; i++) {
    allocation = 0;
    fail_at = i;
    assert(!crop_sprite(sprite, 0, 0, 1, 1));
    fail_at = 0;
    assert(*dos_vbe_graphics_error());
  }
  free_sprite(sprite);
  for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
    fixture(path, invalid[i]);
    assert(!load_gfxfile(path));
    assert(*dos_vbe_graphics_error());
  }
  fixture(path, "{ \"2 1 2 1\", \"\\042 c white\", \"\\134 c red\", "
                "\"\\x22\\134\" };");
  sprite = load(path);
  assert(sprite->pixels[0] == 0xffff && sprite->pixels[1] == 0xf800);
  free_sprite(sprite);
  fixture(path, "{ \"1 1 1 1\", \"a g gray50\", \"a\" };");
  sprite = load(path);
  assert(sprite->pixels[0] == 0x7bef);
  free_sprite(sprite);
  {
    FILE *file = fopen(path, "wb");
    assert(file && fputs("{ \"", file) >= 0);
    for (i = 0; i < 65537U; i++) {
      assert(fputc('a', file) != EOF);
    }
    assert(fputs("\" };", file) >= 0 && fclose(file) == 0);
    assert(!load_gfxfile(path) && *dos_vbe_graphics_error());
  }
  /* Deterministic malformed/truncated input variations exercise lexical exits. */
  for (i = 0; i < 128; i++) {
    char malformed[128];
    unsigned int j;
    strcpy(malformed, "{ \"1 1 1 1\", \"a c red\", \"a\" };");
    for (j = 0; j < i % 31U; j++) {
      malformed[j] = (char)((i * 17U + j * 23U) & 127U);
    }
    fixture(path, malformed);
    sprite = load_gfxfile(path);
    free_sprite(sprite);
  }
  assert(!load_gfxfile(NULL) && *dos_vbe_graphics_error());
  assert(!load_gfxfile("") && *dos_vbe_graphics_error());
  assert(remove(path) == 0);
  assert(!load_gfxfile(path) && *dos_vbe_graphics_error());
  puts("Synthetic XPM, color-key collision, clipping, failures and cleanup passed.");
}

static void actual_atlases(const char *manifest)
{
  FILE *file = fopen(manifest, "r");
  char path[4096];
  unsigned int count = 0;
  assert(file);
  while (fgets(path, sizeof(path), file)) {
    struct Sprite *sprite;
    path[strcspn(path, "\r\n")] = '\0';
    sprite = load(path);
    assert(sprite->width > 0 && sprite->height > 0);
    free_sprite(sprite);
    count++;
  }
  assert(!ferror(file) && fclose(file) == 0);
  assert(count >= 21);
  printf("Loaded all %u source XPM atlases.\n", count);
}

static void actual_tags(const char *manifest)
{
  FILE *file = fopen(manifest, "r");
  struct Sprite *atlas = NULL;
  char line[12288], last_path[4096] = "";
  unsigned int count = 0;
  assert(file);
  while (fgets(line, sizeof(line), file)) {
    char path[4096], spec[4096], tag[256];
    char spec_text[65536];
    int x, y, w, h, row;
    FILE *source;
    struct Sprite *sprite;
    assert(sscanf(line, "%4095[^\t]\t%4095[^\t]\t%255[^\t]\t%d\t%d\t%d\t%d",
                  path, spec, tag, &x, &y, &w, &h) == 7);
    source = fopen(spec, "r");
    assert(source);
    spec_text[fread(spec_text, 1, sizeof(spec_text) - 1, source)] = '\0';
    assert(!ferror(source) && fclose(source) == 0);
    assert(strstr(spec_text, tag));
    if (strcmp(path, last_path)) {
      free_sprite(atlas);
      atlas = load(path);
      strcpy(last_path, path);
    }
    sprite = crop_sprite(atlas, x, y, w, h);
    if (!sprite) {
      fprintf(stderr, "%s tag %s: crop %d,%d %dx%d in %dx%d: %s\n",
              path, tag, x, y, w, h, atlas->width, atlas->height,
              dos_vbe_graphics_error());
    }
    assert(sprite);
    for (row = 0; row < h; row++) {
      int col;
      for (col = 0; col < w; col++) {
        size_t dst = (size_t)row * (size_t)w + (size_t)col;
        if (y + row < atlas->height && x + col < atlas->width) {
          size_t src = (size_t)(y + row) * (size_t)atlas->width
                     + (size_t)(x + col);
          assert(sprite->pixels[dst] == atlas->pixels[src]);
          assert(sprite->opacity[dst] == atlas->opacity[src]);
        } else {
          assert(sprite->pixels[dst] == 0 && sprite->opacity[dst] == 0);
        }
      }
    }
    drawing(sprite);
    if (count % 31U == 0) {
      /* Atlas release must not invalidate tag-cache crops. */
      free_sprite(atlas);
      atlas = NULL;
      last_path[0] = '\0';
      drawing(sprite);
    }
    free_sprite(sprite);
    count++;
  }
  free_sprite(atlas);
  assert(!ferror(file) && fclose(file) == 0);
  assert(count > 800);
  printf("Validated %u actual source/staged Trident tag crops and draws.\n", count);
}

int main(int argc, char **argv)
{
  assert(argc == 5);
  assert(overhead_view_supported() && !isometric_view_supported());
  assert(!strcmp(gfx_fileextensions()[0], "xpm"));
  assert(!strcmp(gfx_fileextensions()[1], "XPM"));
  assert(gfx_fileextensions()[2] == NULL);
  mapping(argv[3], argv[4]);
  synthetic(argv[3]);
  actual_atlases(argv[2]);
  actual_tags(argv[1]);
  puts("DOS graphics resource tests passed (ASan/UBSan).");
  return 0;
}
