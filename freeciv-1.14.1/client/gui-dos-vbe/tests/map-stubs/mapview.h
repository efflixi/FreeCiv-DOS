#ifndef FC__MAPVIEW_H
#define FC__MAPVIEW_H
#define FC__GUI_MAIN_H

#include "map.h"

enum dos_vbe_ui_command {
  DOS_VBE_CMD_NONE = 0,
  DOS_VBE_CMD_MOVE_NORTH = 1,
  DOS_VBE_CMD_MOVE_SOUTH = 2,
  DOS_VBE_CMD_MOVE_EAST = 3,
  DOS_VBE_CMD_MOVE_WEST = 4,
  DOS_VBE_CMD_SELECT_TILE = 5,
  DOS_VBE_CMD_END_TURN = 6,
  DOS_VBE_CMD_TOGGLE_OVERVIEW = 7,
  DOS_VBE_CMD_CANCEL = 8
};

void dos_vbe_apply_command(enum dos_vbe_ui_command cmd);
void center_tile_mapcanvas(int x, int y);
void get_center_tile_mapcanvas(int *x, int *y);
bool tile_visible_mapcanvas(int x, int y);
bool tile_visible_and_not_on_border_mapcanvas(int x, int y);
void update_map_canvas(int x, int y, int width, int height, bool present);
void update_map_canvas_visible(void);

#endif
