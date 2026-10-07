#ifndef FC__MAP_H
#define FC__MAP_H

#include "terrain.h"

typedef int bool;
#define TRUE 1
#define FALSE 0

struct unit;
struct city { int unused; };
struct unit_list { int count; };
struct tile {
  enum tile_terrain_type terrain;
  struct city *city;
  struct unit_list units;
};
struct civ_map {
  int xsize;
  int ysize;
  struct tile *tiles;
};

extern struct civ_map map;
struct tile *map_get_tile(int x, int y);
int unit_list_size(const struct unit_list *units);

#endif
