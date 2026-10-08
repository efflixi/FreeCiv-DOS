/* DOS overhead viewport; selection never changes the authoritative focus unit. */
#ifndef FC__MAPVIEW_H
#define FC__MAPVIEW_H

#include "mapview_g.h"
#include "gui_main.h"

void dos_vbe_apply_command(enum dos_vbe_ui_command cmd);
bool dos_vbe_map_to_canvas(int x, int y, int *canvas_x, int *canvas_y);
bool dos_vbe_canvas_to_map(int canvas_x, int canvas_y, int *x, int *y);
void dos_vbe_select_tile(int x, int y);
void dos_vbe_scroll_map(int dx, int dy);
bool dos_vbe_get_selected_tile(int *x, int *y);
void dos_vbe_move_selection(int dx, int dy);
int dos_vbe_mapview_set_top(unsigned int pixels);
const char *dos_vbe_map_hud_text(void);
void dos_vbe_mapview_free(void);

#endif  /* FC__MAPVIEW_H */
