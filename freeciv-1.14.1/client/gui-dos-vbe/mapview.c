/* Rendering foundation; game resources and remaining GUI hooks are not ready. */
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "control.h"
#include "map.h"
#include "mapview.h"
#include "colors.h"
#include "vbe_init.h"

#define DOS_VBE_MAP_TILE_SIZE 16
#define DOS_VBE_HUD_HEIGHT 48
#define DOS_VBE_HUD_COLOR 0x4210U
#define DOS_VBE_PANEL_COLOR 0x18c3U
#define DOS_VBE_TILE_SELECTED 0xf800U
#define DOS_VBE_TILE_UNIT 0x07e0U

static int current_center_x;
static int current_center_y;
static int current_selected_x;
static int current_selected_y;
static bool dos_vbe_overview_enabled = FALSE;

static unsigned int map_height(void)
{
  return dos_vbe_front_buffer.height > DOS_VBE_HUD_HEIGHT
         ? dos_vbe_front_buffer.height - DOS_VBE_HUD_HEIGHT : 0;
}

static struct tile *safe_tile(int x, int y)
{
  if (!map.tiles || x < 0 || y < 0 || x >= map.xsize || y >= map.ysize) {
    return NULL;
  }
  return map_get_tile(x, y);
}

static void dos_vbe_clamp_to_map_bounds(void)
{
  if (!map.tiles || map.xsize <= 0 || map.ysize <= 0) {
    return;
  }
  if (current_center_x < 0) current_center_x = 0;
  if (current_center_y < 0) current_center_y = 0;
  if (current_center_x >= map.xsize) current_center_x = map.xsize - 1;
  if (current_center_y >= map.ysize) current_center_y = map.ysize - 1;
  if (current_selected_x < 0) current_selected_x = 0;
  if (current_selected_y < 0) current_selected_y = 0;
  if (current_selected_x >= map.xsize) current_selected_x = map.xsize - 1;
  if (current_selected_y >= map.ysize) current_selected_y = map.ysize - 1;
}

static unsigned int dos_vbe_terrain_color(enum tile_terrain_type terrain)
{
  switch (terrain) {
  case T_OCEAN: return 0x0a2dU;
  case T_GRASSLAND:
  case T_PLAINS: return 0x1c46U;
  case T_FOREST:
  case T_JUNGLE: return 0x2a6aU;
  case T_MOUNTAINS:
  case T_HILLS: return 0x4a52U;
  case T_DESERT:
  case T_TUNDRA: return 0xc8a4U;
  case T_ARCTIC: return 0xd6d6U;
  case T_SWAMP: return dos_vbe_rgb(47U, 93U, 31U);
  default: return 0x1c46U;
  }
}

static void draw_pixel(int x, int y, unsigned int color)
{
  if (x >= 0 && y >= 0 && (unsigned int)x < dos_vbe_front_buffer.width
      && (unsigned int)y < map_height()) {
    dos_vbe_framebuffer_put_pixel(&dos_vbe_front_buffer, x, y, color);
  }
}

static void dos_vbe_draw_map_tile(int world_x, int world_y,
                                 int canvas_x, int canvas_y)
{
  struct tile *tile = safe_tile(world_x, world_y);
  int x;
  int y;
  unsigned int color = tile ? dos_vbe_terrain_color(tile->terrain) : 0;

  for (y = 0; y < DOS_VBE_MAP_TILE_SIZE; ++y) {
    for (x = 0; x < DOS_VBE_MAP_TILE_SIZE; ++x) {
      unsigned int pixel = color;
      if (tile && tile->city && x >= 5 && x < 11 && y >= 5 && y < 11
          && (x == 5 || x == 10 || y == 5 || y == 10
              || x == y || x + y == 15)) {
        pixel = DOS_VBE_TILE_SELECTED;
      }
      if (tile && unit_list_size(&tile->units) > 0
          && x >= 6 && x < 10 && y >= 6 && y < 10) {
        pixel = DOS_VBE_TILE_UNIT;
      }
      if (world_x == current_selected_x && world_y == current_selected_y
          && (x == 0 || y == 0 || x == 15 || y == 15)) {
        pixel = DOS_VBE_TILE_SELECTED;
      }
      draw_pixel(canvas_x + x, canvas_y + y, pixel);
    }
  }
}

static void dos_vbe_draw_hud(void)
{
  static char previous[160];
  static unsigned long previous_generation;
  static unsigned int previous_width;
  static unsigned int previous_height;
  struct tile *tile = safe_tile(current_selected_x, current_selected_y);
  unsigned int top = map_height();
  unsigned long generation = dos_vbe_display_generation();
  char text[160];

  if (dos_vbe_framebuffer_validate(&dos_vbe_front_buffer) != 0) {
    return;
  }
  snprintf(text, sizeof(text), "MAP (%d,%d) SELECT (%d,%d)\n"
           "UNITS %d  CITY %s  GRID %s", current_center_x, current_center_y,
           current_selected_x, current_selected_y,
           tile ? unit_list_size(&tile->units) : 0,
           tile && tile->city ? "YES" : "NO",
           dos_vbe_overview_enabled ? "ON" : "OFF");
  if (previous_generation == generation
      && previous_width == dos_vbe_front_buffer.width
      && previous_height == dos_vbe_front_buffer.height
      && strcmp(previous, text) == 0) {
    return;
  }
  if (dos_vbe_framebuffer_fill(&dos_vbe_front_buffer, 0, (int)top,
                               (int)dos_vbe_front_buffer.width,
                               (int)(dos_vbe_front_buffer.height - top),
                               DOS_VBE_PANEL_COLOR) != 0
      || dos_vbe_framebuffer_text(&dos_vbe_front_buffer, 4, (int)top + 4,
                                  text, 0xffffU, 2) != 0) {
    fprintf(stderr, "DOS VBE: HUD rendering failed.\n");
    return;
  }
  strcpy(previous, text);
  previous_generation = generation;
  previous_width = dos_vbe_front_buffer.width;
  previous_height = dos_vbe_front_buffer.height;
}

void dos_vbe_apply_command(enum dos_vbe_ui_command cmd)
{
  switch (cmd) {
  case DOS_VBE_CMD_MOVE_NORTH:
    key_move_north();
    if (current_center_y > INT_MIN) --current_center_y;
    if (current_selected_y > INT_MIN) --current_selected_y;
    break;
  case DOS_VBE_CMD_MOVE_SOUTH:
    key_move_south();
    if (current_center_y < INT_MAX) ++current_center_y;
    if (current_selected_y < INT_MAX) ++current_selected_y;
    break;
  case DOS_VBE_CMD_MOVE_EAST:
    key_move_east();
    if (current_center_x < INT_MAX) ++current_center_x;
    if (current_selected_x < INT_MAX) ++current_selected_x;
    break;
  case DOS_VBE_CMD_MOVE_WEST:
    key_move_west();
    if (current_center_x > INT_MIN) --current_center_x;
    if (current_selected_x > INT_MIN) --current_selected_x;
    break;
  case DOS_VBE_CMD_SELECT_TILE:
    if (safe_tile(current_selected_x, current_selected_y)) {
      do_map_click(current_selected_x, current_selected_y);
    }
    break;
  case DOS_VBE_CMD_END_TURN: key_end_turn(); break;
  case DOS_VBE_CMD_TOGGLE_OVERVIEW:
    request_toggle_map_grid();
    dos_vbe_overview_enabled = !dos_vbe_overview_enabled;
    break;
  case DOS_VBE_CMD_CANCEL: key_cancel_action(); break;
  default: break;
  }
  dos_vbe_clamp_to_map_bounds();
}

bool tile_visible_mapcanvas(int x, int y)
{
  int64_t dx = (int64_t)x - current_center_x;
  int64_t dy = (int64_t)y - current_center_y;
  return safe_tile(x, y) != NULL && dx >= 0 && dy >= 0
         && dx * DOS_VBE_MAP_TILE_SIZE < dos_vbe_front_buffer.width
         && dy * DOS_VBE_MAP_TILE_SIZE < map_height();
}

bool tile_visible_and_not_on_border_mapcanvas(int x, int y)
{
  int64_t dx = (int64_t)x - current_center_x;
  int64_t dy = (int64_t)y - current_center_y;
  return tile_visible_mapcanvas(x, y) && dx > 0 && dy > 0
         && (dx + 1) * DOS_VBE_MAP_TILE_SIZE < dos_vbe_front_buffer.width
         && (dy + 1) * DOS_VBE_MAP_TILE_SIZE < map_height();
}

void update_info_label(void)
{
  if (dos_vbe_display_active()) {
    dos_vbe_draw_hud();
  }
}
void update_unit_info_label(struct unit *punit) { (void)punit; update_info_label(); }
void update_timeout_label(void) { /* Pending game HUD integration. */ }
void update_turn_done_button(bool do_restore) { (void)do_restore; }
void set_indicator_icons(int bulb, int sol, int flake, int gov)
{ (void)bulb; (void)sol; (void)flake; (void)gov; }
void set_overview_dimensions(int x, int y) { (void)x; (void)y; }
void overview_update_tile(int x, int y) { (void)x; (void)y; }

void center_tile_mapcanvas(int x, int y)
{
  current_center_x = current_selected_x = x;
  current_center_y = current_selected_y = y;
}

void get_center_tile_mapcanvas(int *x, int *y)
{
  if (x) *x = current_center_x;
  if (y) *y = current_center_y;
}

void update_map_canvas(int tile_x, int tile_y, int width, int height,
                      bool write_to_screen)
{
  int64_t left;
  int64_t top;
  int64_t right;
  int64_t bottom;
  int64_t columns;
  int64_t rows;
  int64_t x;
  int64_t y;

  if (width < 0 || height < 0) {
    fprintf(stderr, "DOS VBE: negative map update dimensions.\n");
    return;
  }
  if (!width || !height) {
    return;
  }
  if (!dos_vbe_display_active() && vbe_init_display() != 0) {
    return;
  }
  if (dos_vbe_framebuffer_validate(&dos_vbe_front_buffer) != 0) {
    return;
  }
  columns = ((int64_t)dos_vbe_front_buffer.width + 15) / 16;
  rows = ((int64_t)map_height() + 15) / 16;
  left = (int64_t)tile_x - current_center_x;
  top = (int64_t)tile_y - current_center_y;
  right = left + width;
  bottom = top + height;
  if (left < 0) left = 0;
  if (top < 0) top = 0;
  if (right > columns) right = columns;
  if (bottom > rows) bottom = rows;
  for (y = top; y < bottom; ++y) {
    for (x = left; x < right; ++x) {
      int64_t world_x = (int64_t)current_center_x + x;
      int64_t world_y = (int64_t)current_center_y + y;
      if (world_x >= INT_MIN && world_x <= INT_MAX
          && world_y >= INT_MIN && world_y <= INT_MAX) {
        dos_vbe_draw_map_tile((int)world_x, (int)world_y,
                              (int)(x * 16), (int)(y * 16));
      }
    }
  }
  if (current_selected_x >= (int64_t)tile_x
      && current_selected_x < (int64_t)tile_x + width
      && current_selected_y >= (int64_t)tile_y
      && current_selected_y < (int64_t)tile_y + height) {
    dos_vbe_draw_hud();
  }
  if (write_to_screen && dos_vbe_present() != 0) {
    fprintf(stderr, "DOS VBE: map presentation failed.\n");
  }
}

void update_map_canvas_visible(void)
{
  if (!dos_vbe_display_active() && vbe_init_display() != 0) {
    return;
  }
  if (dos_vbe_framebuffer_validate(&dos_vbe_front_buffer) != 0) {
    return;
  }
  update_map_canvas(current_center_x, current_center_y,
                    (int)((dos_vbe_front_buffer.width + 15U) / 16U),
                    (int)((map_height() + 15U) / 16U), FALSE);
  dos_vbe_draw_hud();
  if (dos_vbe_present() != 0) {
    fprintf(stderr, "DOS VBE: visible map presentation failed.\n");
  }
}

void update_map_canvas_scrollbars(void) {}
void update_city_descriptions(void) {}
void put_cross_overlay_tile(int x, int y) { (void)x; (void)y; }
void put_city_workers(struct city *pcity, int color) { (void)pcity; (void)color; }
void move_unit_map_canvas(struct unit *punit, int x0, int y0, int x1, int y1)
{ (void)punit; (void)x0; (void)y0; (void)x1; (void)y1; }
void decrease_unit_hp_smooth(struct unit *punit0, int hp0,
                             struct unit *punit1, int hp1)
{ (void)punit0; (void)hp0; (void)punit1; (void)hp1; }
void put_nuke_mushroom_pixmaps(int x, int y) { (void)x; (void)y; }
void refresh_overview_canvas(void) {}
void refresh_overview_viewrect(void) {}
void draw_segment(int src_x, int src_y, int dir)
{ (void)src_x; (void)src_y; (void)dir; }
void undraw_segment(int src_x, int src_y, int dir)
{ (void)src_x; (void)src_y; (void)dir; }
