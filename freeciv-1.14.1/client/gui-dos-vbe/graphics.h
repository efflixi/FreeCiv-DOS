/**********************************************************************
 Freeciv - Copyright (C) 1996 - A Kjeldberg, L Gregersen, P Unold
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.
***********************************************************************/
#ifndef FC__GRAPHICS_H
#define FC__GRAPHICS_H

#include "graphics_g.h"

struct dos_vbe_framebuffer;

/* Each sprite owns both arrays. Crops remain valid after freeing the atlas.
 * Crop origins must be inside the atlas. Right/bottom overhang is padded
 * transparent (Trident's final explosion frame extends 2 rows past its atlas).
 * Opacity is 0 or 255, independent of RGB565 (no reserved color key).
 * Tilespec owns its tag cache and frees aliased sprites once; this backend
 * deliberately does not cache pointers returned by load_gfxfile().
 */
struct Sprite {
  int width, height;
  unsigned short *pixels;
  unsigned char *opacity;
};

void get_sprite_dimensions(struct Sprite *sprite, int *width, int *height);
int dos_vbe_sprite_draw(struct dos_vbe_framebuffer *fb,
                        const struct Sprite *sprite, int x, int y);
/* Invalid source regions fail; valid destination regions clip, with no-op
 * success when offscreen. The framebuffer's pitch and dirty state are honored.
 */
int dos_vbe_sprite_draw_region(struct dos_vbe_framebuffer *fb,
                               const struct Sprite *sprite, int dx, int dy,
                               int sx, int sy, int width, int height);
/* Last resource error, valid until the next resource operation. */
const char *dos_vbe_graphics_error(void);

/* Production DOS tilespec discovery also recognizes this short extension.
 * Source trees keep their original .tilespec filenames.
 */
#define DOS_VBE_TILESPEC_SUFFIX ".TSP"
/* Resolve one data-relative filename using a staged RESMAP.TXT. The complete
 * bounded map is validated, including duplicate keys and safe 8.3 aliases.
 * Matching is ASCII case-insensitive for DOS discovery. Return value is newly
 * allocated: caller frees it after datafilename(alias). NULL means missing or
 * invalid map/entry; inspect dos_vbe_graphics_error(). No source files change.
 */
char *dos_vbe_resource_filename(const char *map_filename,
                                const char *original_filename);

#endif  /* FC__GRAPHICS_H */
