/**********************************************************************
 Freeciv - Copyright (C) 1996 - A Kjeldberg, L Gregersen, P Unold
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.
***********************************************************************/
#ifndef FC__DOS_RESOURCE_XPM_H
#define FC__DOS_RESOURCE_XPM_H

#include "graphics.h"

struct Sprite *dos_xpm_load(const char *filename);
struct Sprite *dos_sprite_alloc(int width, int height);
int dos_sprite_valid(const struct Sprite *sprite);
void dos_resource_error(const char *message);
void dos_resource_clear_error(void);

#endif
