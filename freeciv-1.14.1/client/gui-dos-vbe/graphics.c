/**********************************************************************
 Freeciv - Copyright (C) 1996 - A Kjeldberg, L Gregersen, P Unold
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.
***********************************************************************/

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "graphics.h"
#include "resource_xpm.h"
#include "framebuffer.h"

bool isometric_view_supported(void)
{
  return FALSE;
}

bool overhead_view_supported(void)
{
  return TRUE;
}

/* Intro/cursors are not used by the diagnostic-only DOS client yet. */
void load_intro_gfx(void)
{
}

void load_cursors(void)
{
}

void free_intro_radar_sprites(void)
{
}

char **gfx_fileextensions(void)
{
  static char *extensions[] = { "xpm", "XPM", NULL };
  return extensions;
}

struct Sprite *load_gfxfile(const char *filename)
{
  return dos_xpm_load(filename);
}

void free_sprite(struct Sprite *sprite)
{
  if (sprite) {
    free(sprite->pixels);
    free(sprite->opacity);
    free(sprite);
  }
}

void get_sprite_dimensions(struct Sprite *sprite, int *width, int *height)
{
  if (width) {
    *width = sprite ? sprite->width : 0;
  }
  if (height) {
    *height = sprite ? sprite->height : 0;
  }
}

static int region_valid(const struct Sprite *sprite,
                        int x, int y, int width, int height)
{
  return dos_sprite_valid(sprite) && x >= 0 && y >= 0
      && width > 0 && height > 0 && x < sprite->width && y < sprite->height
      && width <= sprite->width - x && height <= sprite->height - y;
}

struct Sprite *crop_sprite(struct Sprite *source,
                          int x, int y, int width, int height)
{
  struct Sprite *sprite;
  int row;

  dos_resource_clear_error();
  if (!dos_sprite_valid(source) || x < 0 || y < 0 || width <= 0 || height <= 0
      || x >= source->width || y >= source->height) {
    dos_resource_error("invalid sprite crop bounds");
    return NULL;
  }
  sprite = dos_sprite_alloc(width, height);
  if (!sprite) {
    return NULL;
  }
  memset(sprite->pixels, 0, (size_t)width * (size_t)height
                            * sizeof(*sprite->pixels));
  memset(sprite->opacity, 0, (size_t)width * (size_t)height);
  if (width > source->width - x) {
    width = source->width - x;
  }
  if (height > source->height - y) {
    height = source->height - y;
  }
  for (row = 0; row < height; row++) {
    size_t src = (size_t)(y + row) * (size_t)source->width + (size_t)x;
    size_t dst = (size_t)row * (size_t)sprite->width;
    memcpy(sprite->pixels + dst, source->pixels + src,
           (size_t)width * sizeof(*sprite->pixels));
    memcpy(sprite->opacity + dst, source->opacity + src, (size_t)width);
  }
  return sprite;
}

int dos_vbe_sprite_draw_region(struct dos_vbe_framebuffer *fb,
                               const struct Sprite *sprite, int dx, int dy,
                               int sx, int sy, int width, int height)
{
  int col, row;
  int64_t left = dx, top = dy, right = left + width, bottom = top + height;

  dos_resource_clear_error();
  if (!region_valid(sprite, sx, sy, width, height)
      || dos_vbe_framebuffer_validate(fb) < 0) {
    dos_resource_error("invalid sprite draw source or framebuffer");
    return -1;
  }
  if (left < 0) {
    left = 0;
  }
  if (top < 0) {
    top = 0;
  }
  if (right > fb->width) {
    right = fb->width;
  }
  if (bottom > fb->height) {
    bottom = fb->height;
  }
  if (left >= right || top >= bottom) {
    return 0;
  }
  for (row = (int)top; row < (int)bottom; row++) {
    size_t src = (size_t)((int64_t)sy + row - dy) * (size_t)sprite->width
               + (size_t)((int64_t)sx + left - dx);
    for (col = (int)left; col < (int)right; col++, src++) {
      if (sprite->opacity[src]) {
        dos_vbe_framebuffer_put_pixel(fb, (unsigned int)col, (unsigned int)row,
                                     sprite->pixels[src]);
      }
    }
  }
  return 0;
}

int dos_vbe_sprite_draw(struct dos_vbe_framebuffer *fb,
                        const struct Sprite *sprite, int x, int y)
{
  return dos_vbe_sprite_draw_region(fb, sprite, x, y, 0, 0,
                                    sprite ? sprite->width : 0,
                                    sprite ? sprite->height : 0);
}
