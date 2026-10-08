/* Overhead map rendering. All game information comes from client state. */
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "city.h"
#include "game.h"
#include "government.h"
#include "map.h"
#include "player.h"
#include "tech.h"
#include "unit.h"
#include "civclient.h"
#include "climisc.h"
#include "control.h"
#include "goto.h"
#include "options.h"
#include "tilespec.h"
#include "graphics.h"
#include "mapview.h"
#include "colors.h"
#include "vbe_init.h"
#include "bitmap_font.h"

#define HUD_HEIGHT 112U
#define CITY_LABEL_BYTES 128U
#define CITY_LABEL_DAMAGE (CITY_LABEL_BYTES * 2U * (DOS_VBE_FONT_WIDTH + 1U) + 2U)

static struct dos_vbe_framebuffer map_canvas;
static struct dos_vbe_framebuffer ui_canvas;
static unsigned int map_top;
static int center_x, center_y, view_x, view_y;
static int selected_x, selected_y;
static bool selected;
static bool overview_enabled = TRUE;
static int overview_width, overview_height;
static int target_x, target_y;
static bool target;
static int workers_city_id;
static int workers_color = COLOR_STD_RED;
static int info_unit_id;
static int indicator[4] = {-1, -1, -1, -1};
static char hud_text[2048];

/* The shipped 1.14 city lists are Latin-1. Keep their accented names readable
 * with the original ASCII font, without changing the stored names or adding
 * an encoding subsystem. UTF-8 Latin-1 and combining accents also fold here. */
static void display_ascii(char *dst, size_t size, const char *src)
{
  static const char *latin[64] = {
    "A", "A", "A", "A", "A", "A", "AE", "C",
    "E", "E", "E", "E", "I", "I", "I", "I",
    "D", "N", "O", "O", "O", "O", "O", "x",
    "O", "U", "U", "U", "U", "Y", "Th", "ss",
    "a", "a", "a", "a", "a", "a", "ae", "c",
    "e", "e", "e", "e", "i", "i", "i", "i",
    "d", "n", "o", "o", "o", "o", "o", "/",
    "o", "u", "u", "u", "u", "y", "th", "y"
  };
  size_t used = 0;
  if (!size) return;
  while (*src && used + 1 < size) {
    unsigned int c = (unsigned char)*src++;
    const char *replacement = NULL;
    if ((c == 0xc2 || c == 0xc3)
        && (unsigned char)*src >= 0x80 && (unsigned char)*src <= 0xbf) {
      c = ((c & 0x1f) << 6) | ((unsigned char)*src++ & 0x3f);
    } else if ((c == 0xcc && (unsigned char)*src >= 0x80 && (unsigned char)*src <= 0xbf)
               || (c == 0xcd && (unsigned char)*src >= 0x80 && (unsigned char)*src <= 0xaf)) {
      ++src;
      continue;
    }
    if (c >= 0xc0 && c <= 0xff) replacement = latin[c - 0xc0];
    else if (c == 0xa0) replacement = " ";
    else if (c == 0xa1) replacement = "!";
    else if (c == 0xab || c == 0xbb) replacement = "\"";
    else if (c == 0xb0) replacement = "o";
    else if (c == 0xb4) replacement = "'";
    if (replacement) {
      while (*replacement && used + 1 < size) dst[used++] = *replacement++;
    } else {
      dst[used++] = c < 0x80 ? (char)c : '?';
    }
  }
  dst[used] = '\0';
}

static int tile_width(void) { return NORMAL_TILE_WIDTH > 0 ? NORMAL_TILE_WIDTH : 1; }
static int tile_height(void) { return NORMAL_TILE_HEIGHT > 0 ? NORMAL_TILE_HEIGHT : 1; }
static int panel_width(void)
{
  unsigned int width = dos_vbe_front_buffer.width;
  return overview_enabled && width >= 320U ? (int)(width / 4U) : 0;
}
static int canvas_width(void)
{
  return (int)dos_vbe_front_buffer.width - panel_width();
}
static int canvas_height(void)
{
  return dos_vbe_front_buffer.height > HUD_HEIGHT + map_top
         ? (int)(dos_vbe_front_buffer.height - HUD_HEIGHT - map_top) : 0;
}
static int columns(void)
{
  return (canvas_width() + tile_width() - 1) / tile_width();
}
static int rows(void)
{
  return (canvas_height() + tile_height() - 1) / tile_height();
}
static bool have_map(void)
{
  return map.tiles && map.xsize > 0 && map.ysize > 0;
}
static bool normalize_position(int *x, int *y)
{
  if (!have_map()) return FALSE;
  *x = map_adjust_x(*x);
  return normalize_map_pos(x, y);
}
static struct tile *safe_tile(int x, int y)
{
  return normalize_position(&x, &y) ? map_get_tile(x, y) : NULL;
}
static void update_origin(void)
{
  int full_rows;
  if (!have_map()) {
    view_x = view_y = 0;
    return;
  }
  center_x = map_adjust_x(center_x);
  center_y = map_adjust_y(center_y);
  /* The engine wraps longitude, not latitude. Keep the poles visible. */
  view_x = map_adjust_x(center_x - columns() / 2);
  view_y = center_y - rows() / 2;
  full_rows = canvas_height() / tile_height();
  if (full_rows < 1) full_rows = 1;
  if (view_y > map.ysize - full_rows) view_y = map.ysize - full_rows;
  if (view_y < 0) view_y = 0;
}

static bool map_to_local_canvas(int x, int y, int *cx, int *cy)
{
  int dx;
  if (!cx || !cy || !normalize_position(&x, &y)) {
    return FALSE;
  }
  update_origin();
  dx = map_adjust_x(x - view_x);
  *cx = dx * tile_width();
  *cy = (y - view_y) * tile_height();
  return *cx >= 0 && *cx < canvas_width()
         && *cy >= 0 && *cy < canvas_height();
}

bool dos_vbe_map_to_canvas(int x, int y, int *cx, int *cy)
{
  if (!map_to_local_canvas(x, y, cx, cy)) return FALSE;
  *cy += (int)map_top;
  return TRUE;
}

int dos_vbe_mapview_set_top(unsigned int pixels)
{
  if (pixels > INT_MAX || dos_vbe_front_buffer.height <= HUD_HEIGHT
      || pixels >= dos_vbe_front_buffer.height - HUD_HEIGHT) {
    fprintf(stderr, "DOS map: invalid reserved toolbar height.\n");
    return -1;
  }
  map_top = pixels;
  return 0;
}

bool dos_vbe_canvas_to_map(int cx, int cy, int *x, int *y)
{
  if (cy < (int)map_top) return FALSE;
  cy -= (int)map_top;
  if (!x || !y || !have_map() || cx < 0 || cy < 0
      || cx >= canvas_width() || cy >= canvas_height()) return FALSE;
  update_origin();
  *x = view_x + cx / tile_width();
  *y = view_y + cy / tile_height();
  return normalize_map_pos(x, y);
}

void dos_vbe_select_tile(int x, int y)
{
  if (!normalize_position(&x, &y)) return;
  selected_x = x;
  selected_y = y;
  selected = TRUE;
  update_map_canvas_visible();
}

void dos_vbe_scroll_map(int dx, int dy)
{
  int64_t x = (int64_t)center_x + dx;
  int64_t y = (int64_t)center_y + dy;
  if (!have_map()) return;
  center_x = (int)(x % map.xsize);
  center_y = y < 0 ? 0 : y >= map.ysize ? map.ysize - 1 : (int)y;
  update_map_canvas_visible();
}

static void box(struct dos_vbe_framebuffer *fb, int x, int y,
                int w, int h, unsigned int color)
{
  if (w <= 0 || h <= 0) return;
  dos_vbe_framebuffer_line(fb, x, y, x + w - 1, y, color);
  dos_vbe_framebuffer_line(fb, x, y, x, y + h - 1, color);
  dos_vbe_framebuffer_line(fb, x + w - 1, y, x + w - 1, y + h - 1, color);
  dos_vbe_framebuffer_line(fb, x, y + h - 1, x + w - 1, y + h - 1, color);
}

static void draw_route_half(int x, int y, int dir, int cx, int cy)
{
  int dx, dy;
  if (dir < 0 || dir >= 8 || !MAPSTEP(dx, dy, x, y, dir)) return;
  dos_vbe_framebuffer_line(&map_canvas,
                          cx + tile_width() / 2,
                          cy + tile_height() / 2,
                          cx + tile_width() / 2 + DIR_DX[dir] * tile_width() / 2,
                          cy + tile_height() / 2 + DIR_DY[dir] * tile_height() / 2,
                          dos_vbe_standard_color(COLOR_STD_CYAN));
}

static void draw_tile(int x, int y, int cx, int cy)
{
  struct Sprite *layers[80];
  struct player *owner;
  int count, solid, i, dir, nx, ny;
  unsigned int color;

  if (!normalize_map_pos(&x, &y) || tile_get_known(x, y) == TILE_UNKNOWN) return;
  count = fill_tile_sprite_array(layers, x, y, FALSE, &solid, &owner);
  if (solid) {
    color = dos_vbe_standard_color(owner ? player_color(owner) : COLOR_STD_BACKGROUND);
    dos_vbe_framebuffer_fill(&map_canvas, cx, cy, tile_width(), tile_height(), color);
  }
  for (i = 0; i < count; ++i) {
    if (layers[i]) dos_vbe_sprite_draw(&map_canvas, layers[i], cx, cy);
  }
  if (draw_map_grid) {
    dos_vbe_framebuffer_line(&map_canvas, cx, cy, cx + tile_width() - 1, cy,
                            dos_vbe_standard_color(get_grid_color(x, y, x, y - 1)));
    dos_vbe_framebuffer_line(&map_canvas, cx, cy, cx, cy + tile_height() - 1,
                            dos_vbe_standard_color(get_grid_color(x, y, x - 1, y)));
  }
  if (draw_coastline && !draw_terrain) {
    nx = x - 1; ny = y;
    if (normalize_map_pos(&nx, &ny)
        && ((map_get_terrain(x, y) == T_OCEAN) != (map_get_terrain(nx, ny) == T_OCEAN))) {
      dos_vbe_framebuffer_line(&map_canvas, cx, cy, cx, cy + tile_height() - 1,
                              dos_vbe_standard_color(COLOR_STD_OCEAN));
    }
    nx = x; ny = y - 1;
    if (normalize_map_pos(&nx, &ny)
        && ((map_get_terrain(x, y) == T_OCEAN) != (map_get_terrain(nx, ny) == T_OCEAN))) {
      dos_vbe_framebuffer_line(&map_canvas, cx, cy, cx + tile_width() - 1, cy,
                              dos_vbe_standard_color(COLOR_STD_OCEAN));
    }
  }
  if (goto_map.drawn) {
    for (dir = 0; dir < 8; ++dir) {
      if (get_drawn(x, y, dir)) draw_route_half(x, y, dir, cx, cy);
    }
  }
}

static void draw_city_label(int x, int y, int cx, int cy)
{
  struct tile *ptile;
  char label[CITY_LABEL_BYTES];
  char readable[256];
  if (!normalize_position(&x, &y) || tile_get_known(x, y) == TILE_UNKNOWN) return;
  ptile = map_get_tile(x, y);
  if (ptile->city && draw_city_names && draw_cities) {
    snprintf(label, sizeof(label), "%s %d", ptile->city->name, ptile->city->size);
    display_ascii(readable, sizeof(readable), label);
    dos_vbe_framebuffer_text(&map_canvas, cx + 2, cy + tile_height() + 2,
                            readable, dos_vbe_standard_color(COLOR_STD_BLACK), 1);
    dos_vbe_framebuffer_text(&map_canvas, cx + 1, cy + tile_height() + 1,
                            readable, dos_vbe_standard_color(COLOR_STD_WHITE), 1);
  }
  if (ptile->city && draw_city_productions && draw_cities
      && game.player_ptr == city_owner(ptile->city)) {
    get_city_mapview_production(ptile->city, label, sizeof(label));
    display_ascii(readable, sizeof(readable), label);
    dos_vbe_framebuffer_text(&map_canvas, cx + 1, cy + tile_height() + 9,
                            readable, dos_vbe_standard_color(COLOR_STD_WHITE), 1);
  }
}

static void draw_overlays(void)
{
  int cx, cy, wx, wy, px, py, step;
  struct city *pcity = workers_city_id ? find_city_by_id(workers_city_id) : NULL;
  if (selected && map_to_local_canvas(selected_x, selected_y, &cx, &cy)) {
    box(&map_canvas, cx, cy, tile_width(), tile_height(),
        dos_vbe_standard_color(COLOR_STD_YELLOW));
  }
  if (target && map_to_local_canvas(target_x, target_y, &cx, &cy)) {
    if (sprites.user.attention) {
      dos_vbe_sprite_draw(&map_canvas, sprites.user.attention, cx, cy);
    }
  }
  if (pcity) {
    city_map_iterate(x, y) {
      enum city_tile_type worked = get_worker_city(pcity, x, y);
      if (!is_city_center(x, y) && worked != C_TILE_UNAVAILABLE
          && city_map_to_map(&wx, &wy, pcity, x, y)
          && tile_get_known(wx, wy) != TILE_UNKNOWN
          && map_to_local_canvas(wx, wy, &cx, &cy)) {
        step = worked == C_TILE_WORKER ? 2 : 4;
        for (py = 2; py < tile_height() - 2; py += step) {
          for (px = 2; px < tile_width() - 2; px += step) {
            dos_vbe_framebuffer_put_pixel(&map_canvas, cx + px, cy + py,
                                         dos_vbe_standard_color((enum color_std)workers_color));
          }
        }
        box(&map_canvas, cx + 2, cy + 2, tile_width() - 4, tile_height() - 4,
            dos_vbe_standard_color((enum color_std)workers_color));
      }
    } city_map_iterate_end;
  }
}

static void format_moves(char *text, size_t size, int points)
{
  int whole = points / SINGLE_MOVE, fraction = points % SINGLE_MOVE;
  if (!fraction) snprintf(text, size, "%d", whole);
  else if (!whole) snprintf(text, size, "%d/%d", fraction, SINGLE_MOVE);
  else snprintf(text, size, "%d %d/%d", whole, fraction, SINGLE_MOVE);
}

static void draw_hud(void)
{
  struct player *pplayer = game.player_ptr;
  struct unit *punit = info_unit_id ? find_unit_by_id(info_unit_id) : get_unit_in_focus();
  struct tile *ptile;
  struct city *pcity;
  const char *gov = "unknown government", *research = "none";
  char unit_text[384] = "No unit selected";
  char city_text[256] = "No city";
  char tile_text[384] = "No tile selected";
  char moves[32], rate[32];
  char readable[sizeof(hud_text)];
  int x = selected_x, y = selected_y, i, ix, iy;
  int top = canvas_height() + (int)map_top;
  if (punit && !selected) { x = punit->x; y = punit->y; }
  ptile = (selected || punit) ? safe_tile(x, y) : NULL;
  if (ptile && tile_get_known(x, y) != TILE_UNKNOWN) {
    snprintf(tile_text, sizeof(tile_text), "Tile %d,%d: %s [%s]", x, y,
             map_get_tile_info_text(x, y),
             tile_get_known(x, y) == TILE_KNOWN_FOGGED ? "fog" : "visible");
    pcity = ptile->city;
    if (pcity) {
      snprintf(city_text, sizeof(city_text), "City %.12s size %d owner %.12s%s",
               pcity->name, pcity->size, city_owner(pcity)->name,
               city_unhappy(pcity) ? " DISORDER" : city_happy(pcity) ? " HAPPY" : "");
    }
  } else if (ptile) {
    snprintf(tile_text, sizeof(tile_text), "Tile %d,%d: unknown", x, y);
  }
  if (punit) {
    format_moves(moves, sizeof(moves), punit->moves_left);
    format_moves(rate, sizeof(rate), unit_move_rate(punit));
    snprintf(unit_text, sizeof(unit_text), "%.14s (%.12s) HP %d/%d MP %s/%s %.12s%s",
             unit_type(punit)->name, unit_owner(punit)->name,
             punit->hp, unit_type(punit)->hp, moves, rate, unit_activity_text(punit),
             punit->veteran ? " veteran" : "");
  }
  if (pplayer) {
    if (pplayer->government >= 0 && pplayer->government < game.government_count) {
      gov = get_government_name(pplayer->government);
    }
    if (pplayer->research.researching >= 0
        && pplayer->research.researching < game.num_tech_types) {
      research = get_tech_name(pplayer, pplayer->research.researching);
    }
    snprintf(hud_text, sizeof(hud_text),
             "%.16s  Year %s  %.18s\nGold %d  Tax %d%% Lux %d%% Sci %d%%\n"
             "Research %.24s (%d bulbs)\n%s\n%s\n%s\nTurn %s  Timeout %d",
             pplayer->name, textyear(game.year), gov,
             pplayer->economic.gold, pplayer->economic.tax,
             pplayer->economic.luxury, pplayer->economic.science,
             research,
             pplayer->research.bulbs_researched,
             tile_text, unit_text, city_text,
             pplayer->turn_done ? "DONE" : "ACTIVE", seconds_to_turndone);
  } else {
    snprintf(hud_text, sizeof(hud_text), "No client player  Year %s\n%s\n%s\n%s",
             textyear(game.year), tile_text, unit_text, city_text);
  }
  display_ascii(readable, sizeof(readable), hud_text);
  strcpy(hud_text, readable);
  dos_vbe_framebuffer_fill(&ui_canvas, 0, top,
                          (int)dos_vbe_front_buffer.width,
                          (int)dos_vbe_front_buffer.height - top, 0x18c3U);
  dos_vbe_framebuffer_text(&ui_canvas, 2, top, hud_text, 0xffffU, 2);
  ix = canvas_width() + 2;
  iy = top - (SMALL_TILE_HEIGHT > 0 ? SMALL_TILE_HEIGHT : 20) - 2;
  if (iy < 0) iy = 0;
  if (panel_width()) {
    dos_vbe_framebuffer_fill(&ui_canvas, canvas_width(), iy, panel_width(),
                            top - iy, 0x18c3U);
  }
  for (i = 0; i < 3 && panel_width(); ++i) {
    struct Sprite *icon = NULL;
    if (indicator[i] >= 0 && indicator[i] < NUM_TILES_PROGRESS) {
      icon = i == 0 ? sprites.bulb[indicator[i]]
             : i == 1 ? sprites.warming[indicator[i]] : sprites.cooling[indicator[i]];
    }
    if (icon) {
      dos_vbe_sprite_draw(&ui_canvas, icon, ix, iy);
      ix += icon->width + 2;
    }
  }
  if (panel_width() && indicator[3] >= 0 && indicator[3] < game.government_count) {
    struct Sprite *icon = get_government(indicator[3])->sprite;
    if (icon) dos_vbe_sprite_draw(&ui_canvas, icon, ix, iy);
  }
}

const char *dos_vbe_map_hud_text(void) { return hud_text; }

static void draw_overview(void)
{
  int pw = panel_width(), left = canvas_width(), height = canvas_height();
  int ow, oh, x, y, px, py, ex, ey, dx, dy, visible_cols, visible_rows;
  unsigned int color;
  if (!pw || !height) return;
  dos_vbe_framebuffer_fill(&ui_canvas, left, map_top, pw, height, 0x18c3U);
  if (!have_map()) return;
  ow = overview_width > 0 ? overview_width : map.xsize * 2;
  oh = overview_height > 0 ? overview_height : map.ysize * 2;
  if (ow > pw - 4) {
    oh = (int)((int64_t)oh * (pw - 4) / ow);
    ow = pw - 4;
  }
  if (oh > height - 44) {
    ow = (int)((int64_t)ow * (height - 44) / oh);
    oh = height - 44;
  }
  if (ow <= 0 || oh <= 0) return;
  dos_vbe_framebuffer_text(&ui_canvas, left + 2, (int)map_top + 2, "OVERVIEW", 0xffffU, 1);
  /* Sample every destination pixel: small overviews never omit entire rows. */
  for (py = 0; py < oh; ++py) {
    for (px = 0; px < ow; ++px) {
      x = px * map.xsize / ow;
      y = py * map.ysize / oh;
      color = dos_vbe_standard_color(overview_tile_color(x, y));
      dos_vbe_framebuffer_put_pixel(&ui_canvas, left + 2 + px, (int)map_top + 14 + py, color);
    }
  }
  /* Draw both sides of a viewport crossing the longitude seam. */
  visible_cols = columns() < map.xsize ? columns() : map.xsize;
  visible_rows = rows() < map.ysize - view_y ? rows() : map.ysize - view_y;
  for (y = 0; y < map.ysize; ++y) {
    for (x = 0; x < map.xsize; ++x) {
      dx = map_adjust_x(x - view_x);
      dy = y - view_y;
      if (dx < visible_cols && dy >= 0 && dy < visible_rows) {
        px = x * ow / map.xsize; py = y * oh / map.ysize;
        ex = (x + 1) * ow / map.xsize - 1;
        ey = (y + 1) * oh / map.ysize - 1;
        if (ex < px) ex = px;
        if (ey < py) ey = py;
        if (dx == 0 || dx == visible_cols - 1) {
          int edge = dx == 0 ? px : ex;
          dos_vbe_framebuffer_line(&ui_canvas, left + 2 + edge, (int)map_top + 14 + py,
                                  left + 2 + edge, (int)map_top + 14 + ey, 0xffffU);
        }
        if (dy == 0 || dy == visible_rows - 1) {
          int edge = dy == 0 ? py : ey;
          dos_vbe_framebuffer_line(&ui_canvas, left + 2 + px, (int)map_top + 14 + edge,
                                  left + 2 + ex, (int)map_top + 14 + edge, 0xffffU);
        }
      }
    }
  }
  if (selected && normalize_position(&selected_x, &selected_y)) {
    ex = selected_x * ow / map.xsize;
    ey = selected_y * oh / map.ysize;
    box(&ui_canvas, left + ex + 1, (int)map_top + ey + 13, 3, 3,
        dos_vbe_standard_color(COLOR_STD_YELLOW));
  }
}

static bool prepare_canvas(void)
{
  if (!dos_vbe_display_active() && vbe_init_display() != 0) return FALSE;
  if (dos_vbe_framebuffer_validate(&dos_vbe_front_buffer) != 0) return FALSE;
  if (canvas_width() <= 0 || canvas_height() <= 0) {
    fprintf(stderr, "DOS VBE: display has no usable map viewport.\n");
    return FALSE;
  }
  if (map_canvas.width != (unsigned int)canvas_width()
      || map_canvas.height != (unsigned int)canvas_height()) {
    if (dos_vbe_framebuffer_init(&map_canvas, canvas_width(), canvas_height(), 16) != 0) {
      fprintf(stderr, "DOS VBE: map composition buffer allocation failed.\n");
      return FALSE;
    }
  }
  if (ui_canvas.width != dos_vbe_front_buffer.width
      || ui_canvas.height != dos_vbe_front_buffer.height) {
    if (dos_vbe_framebuffer_init(&ui_canvas, dos_vbe_front_buffer.width,
                                 dos_vbe_front_buffer.height, 16) != 0) {
      fprintf(stderr, "DOS VBE: UI composition buffer allocation failed.\n");
      return FALSE;
    }
  }
  update_origin();
  return TRUE;
}

void dos_vbe_mapview_free(void)
{
  dos_vbe_framebuffer_destroy(&map_canvas);
  dos_vbe_framebuffer_destroy(&ui_canvas);
  selected = target = FALSE;
  workers_city_id = info_unit_id = 0;
  hud_text[0] = '\0';
  map_top = 0;
}

static void copy_ui(void)
{
  int top = canvas_height() + (int)map_top, left = canvas_width();
  if (panel_width()) {
    dos_vbe_framebuffer_blit(&dos_vbe_front_buffer, left, map_top, &ui_canvas,
                            left, map_top, panel_width(), canvas_height(), 0, 0);
  }
  dos_vbe_framebuffer_blit(&dos_vbe_front_buffer, 0, top, &ui_canvas,
                          0, top, dos_vbe_front_buffer.width,
                          dos_vbe_front_buffer.height - top, 0, 0);
}

void update_map_canvas(int x, int y, int width, int height, bool write_to_screen)
{
  int cx, cy, mx, my, start_x;
  bool full;
  if (width < 0 || height < 0) {
    fprintf(stderr, "DOS VBE: negative map update dimensions.\n");
    return;
  }
  if (!width || !height || !prepare_canvas()) return;
  start_x = have_map() ? map_adjust_x(x) : 0;
  full = have_map() && y <= view_y
         && (int64_t)y + height >= (int64_t)view_y + rows()
         && (width >= map.xsize || (start_x == view_x && width >= columns()));
  /* Compose dependency layers offscreen, then copy only requested world
   * damage and its label footprint to the visible map. */
  dos_vbe_framebuffer_clear(&map_canvas, 0);
  if (have_map() && NORMAL_TILE_WIDTH > 0 && NORMAL_TILE_HEIGHT > 0 && !is_isometric) {
    for (cy = 0; cy < canvas_height(); cy += tile_height()) {
      dos_vbe_gui_capture_input();
      for (cx = 0; cx < canvas_width(); cx += tile_width()) {
        mx = view_x + cx / tile_width();
        my = view_y + cy / tile_height();
        draw_tile(mx, my, cx, cy);
      }
    }
    for (cy = 0; cy < canvas_height(); cy += tile_height()) {
      dos_vbe_gui_capture_input();
      for (cx = 0; cx < canvas_width(); cx += tile_width()) {
        draw_city_label(view_x + cx / tile_width(), view_y + cy / tile_height(), cx, cy);
      }
    }
    draw_overlays();
  }
  if (full || !have_map()) {
    dos_vbe_framebuffer_blit(&dos_vbe_front_buffer, 0, map_top, &map_canvas, 0, 0,
                            canvas_width(), canvas_height(), 0, 0);
  } else {
    for (cy = 0; cy < canvas_height(); cy += tile_height()) {
      my = view_y + cy / tile_height();
      if ((int64_t)my < y || (int64_t)my >= (int64_t)y + height) continue;
      for (cx = 0; cx < canvas_width(); cx += tile_width()) {
        mx = map_adjust_x(view_x + cx / tile_width());
        if (width < map.xsize && map_adjust_x(mx - start_x) >= width) continue;
        /* City labels/production extend right and below the changed tile.
         * Recompose there too, including when a city was removed. */
        dos_vbe_framebuffer_blit(&dos_vbe_front_buffer, cx, cy + (int)map_top, &map_canvas,
                                cx, cy,
                                (unsigned int)(canvas_width() - cx) < CITY_LABEL_DAMAGE
                                  ? canvas_width() - cx : (int)CITY_LABEL_DAMAGE,
                                canvas_height() - cy < tile_height() + 16
                                  ? canvas_height() - cy : tile_height() + 16,
                                0, 0);
      }
    }
  }
  draw_overview();
  draw_hud();
  copy_ui();
  if (write_to_screen && dos_vbe_present() != 0) {
    fprintf(stderr, "DOS VBE: map presentation failed.\n");
  }
}

void update_map_canvas_visible(void)
{
  update_map_canvas(0, 0, INT_MAX, INT_MAX, TRUE);
}
bool tile_visible_mapcanvas(int x, int y)
{
  int cx, cy;
  return dos_vbe_map_to_canvas(x, y, &cx, &cy);
}
bool tile_visible_and_not_on_border_mapcanvas(int x, int y)
{
  int cx, cy;
  return map_to_local_canvas(x, y, &cx, &cy) && cx > 0 && cy > 0
         && cx + tile_width() < canvas_width() && cy + tile_height() < canvas_height();
}
void center_tile_mapcanvas(int x, int y)
{
  if (!have_map()) return;
  center_x = map_adjust_x(x);
  center_y = map_adjust_y(y);
  update_map_canvas_visible();
}
void get_center_tile_mapcanvas(int *x, int *y)
{
  update_origin();
  if (x) *x = have_map() ? map_adjust_x(view_x + canvas_width() / 2 / tile_width()) : 0;
  if (y) *y = have_map() ? map_adjust_y(view_y + canvas_height() / 2 / tile_height()) : 0;
}
void update_info_label(void)
{
  if (dos_vbe_display_active() && prepare_canvas()) {
    draw_overview();
    draw_hud();
    copy_ui();
    if (dos_vbe_present() != 0) fprintf(stderr, "DOS VBE: HUD presentation failed.\n");
  }
}
void update_unit_info_label(struct unit *punit)
{
  info_unit_id = punit ? punit->id : 0;
  update_info_label();
}
void update_timeout_label(void) { update_info_label(); }
void update_turn_done_button(bool do_restore)
{
  (void)do_restore;
  update_info_label();
}
void set_indicator_icons(int bulb, int sol, int flake, int gov)
{
  indicator[0] = bulb; indicator[1] = sol; indicator[2] = flake; indicator[3] = gov;
  update_info_label();
}
void set_overview_dimensions(int x, int y)
{
  overview_width = x > INT_MAX / 2 ? INT_MAX : x > 0 ? x * 2 : 0;
  overview_height = y > INT_MAX / 2 ? INT_MAX : y > 0 ? y * 2 : 0;
  refresh_overview_canvas();
}
void overview_update_tile(int x, int y)
{
  if (safe_tile(x, y)) refresh_overview_canvas();
}
void refresh_overview_canvas(void)
{
  if (dos_vbe_display_active() && prepare_canvas()) {
    update_origin();
    draw_overview();
    draw_hud();
    copy_ui();
    if (dos_vbe_present() != 0) fprintf(stderr, "DOS VBE: overview presentation failed.\n");
  }
}
void refresh_overview_viewrect(void) { refresh_overview_canvas(); }
void update_map_canvas_scrollbars(void) { update_map_canvas_visible(); }
void update_city_descriptions(void) { update_map_canvas_visible(); }
void put_cross_overlay_tile(int x, int y)
{
  target = normalize_position(&x, &y);
  target_x = x; target_y = y;
  update_map_canvas_visible();
}
void put_city_workers(struct city *pcity, int color)
{
  if (color == -1) {
    if (pcity && pcity->id != workers_city_id) workers_color = workers_color % 3 + 1;
  } else {
    workers_color = color >= 0 && color < COLOR_STD_LAST ? color : COLOR_STD_WHITE;
  }
  workers_city_id = pcity ? pcity->id : 0;
  update_map_canvas_visible();
}

static void draw_unit_endpoint(struct unit *punit, int x, int y, int hp)
{
  struct unit image;
  struct Sprite *layers[80];
  int cx, cy, count, solid, i;
  if (!punit || !normalize_position(&x, &y)
      || tile_get_known(x, y) == TILE_UNKNOWN
      || !map_to_local_canvas(x, y, &cx, &cy)) return;
  /* Animation coordinates/HP are packet-derived display inputs, not writes
   * to the client's unit object (the packet handler owns that object). */
  image = *punit;
  image.x = x; image.y = y;
  image.hp = hp < 0 ? 0 : hp > unit_type(punit)->hp ? unit_type(punit)->hp : hp;
  count = fill_unit_sprite_array(layers, &image, &solid);
  if (solid) {
    dos_vbe_framebuffer_fill(&map_canvas, cx, cy, tile_width(), tile_height(),
                            dos_vbe_standard_color(player_color(unit_owner(punit))));
  }
  for (i = 0; i < count; ++i) {
    if (layers[i]) dos_vbe_sprite_draw(&map_canvas, layers[i], cx, cy);
  }
}

static void present_map_layer(void)
{
  dos_vbe_framebuffer_blit(&dos_vbe_front_buffer, 0, map_top, &map_canvas, 0, 0,
                          canvas_width(), canvas_height(), 0, 0);
  dos_vbe_present();
}

void move_unit_map_canvas(struct unit *punit, int x0, int y0, int dx, int dy)
{
  int64_t x = (int64_t)x0 + dx, y = (int64_t)y0 + dy;
  update_map_canvas_visible();
  if (!punit || !have_map() || !game.player_ptr
      || dos_vbe_framebuffer_validate(&map_canvas) != 0
      || dx < -1 || dx > 1 || dy < -1 || dy > 1
      || y < 0 || y >= map.ysize || !player_can_see_unit(game.player_ptr, punit)) return;
  draw_unit_endpoint(punit, (int)(x % map.xsize), (int)y, punit->hp);
  present_map_layer();
}
void decrease_unit_hp_smooth(struct unit *punit0, int hp0, struct unit *punit1, int hp1)
{
  update_map_canvas_visible();
  if (!have_map() || dos_vbe_framebuffer_validate(&map_canvas) != 0) return;
  if (punit0) draw_unit_endpoint(punit0, punit0->x, punit0->y, hp0);
  if (punit1) draw_unit_endpoint(punit1, punit1->x, punit1->y, hp1);
  present_map_layer();
}
void put_nuke_mushroom_pixmaps(int x, int y)
{
  int i, j, cx, cy;
  if (!prepare_canvas() || !have_map()) return;
  if (!normalize_position(&x, &y)) return;
  for (j = 0; j < 3; ++j) {
    for (i = 0; i < 3; ++i) {
      if (map_to_local_canvas(x + i - 1, y + j - 1, &cx, &cy)
          && sprites.explode.nuke[j][i]) {
        dos_vbe_sprite_draw(&map_canvas, sprites.explode.nuke[j][i], cx, cy);
      }
    }
  }
  present_map_layer();
}
void draw_segment(int x, int y, int dir)
{
  int dx, dy;
  if (goto_map.drawn && have_map() && dir >= 0 && dir < 8
      && normalize_position(&x, &y) && MAPSTEP(dx, dy, x, y, dir)) {
    increment_drawn(x, y, dir);
    update_map_canvas_visible();
  }
}
void undraw_segment(int x, int y, int dir)
{
  if (goto_map.drawn && have_map() && dir >= 0 && dir < 8
      && normalize_position(&x, &y) && get_drawn(x, y, dir) > 0) {
    decrement_drawn(x, y, dir);
    update_map_canvas_visible();
  }
}
void dos_vbe_apply_command(enum dos_vbe_ui_command cmd)
{
  switch (cmd) {
  case DOS_VBE_CMD_MOVE_NORTH: key_move_north(); break;
  case DOS_VBE_CMD_MOVE_SOUTH: key_move_south(); break;
  case DOS_VBE_CMD_MOVE_EAST: key_move_east(); break;
  case DOS_VBE_CMD_MOVE_WEST: key_move_west(); break;
  case DOS_VBE_CMD_MOVE_NORTH_EAST: key_move_north_east(); break;
  case DOS_VBE_CMD_MOVE_SOUTH_EAST: key_move_south_east(); break;
  case DOS_VBE_CMD_MOVE_SOUTH_WEST: key_move_south_west(); break;
  case DOS_VBE_CMD_MOVE_NORTH_WEST: key_move_north_west(); break;
  case DOS_VBE_CMD_NEXT_UNIT: advance_unit_focus(); break;
  case DOS_VBE_CMD_WAIT_UNIT: key_unit_wait(); break;
  case DOS_VBE_CMD_DONE_UNIT: key_unit_done(); break;
  case DOS_VBE_CMD_SELECT_TILE:
    if (selected && safe_tile(selected_x, selected_y)) do_map_click(selected_x, selected_y);
    break;
  case DOS_VBE_CMD_END_TURN: key_end_turn(); break;
  case DOS_VBE_CMD_TOGGLE_OVERVIEW:
    overview_enabled = !overview_enabled; update_map_canvas_visible(); break;
  case DOS_VBE_CMD_CANCEL:
    target = FALSE; workers_city_id = 0;
    key_cancel_action(); update_map_canvas_visible(); break;
  default: break;
  }
}

bool dos_vbe_get_selected_tile(int *x, int *y)
{
  if (!x || !y || !selected || !safe_tile(selected_x, selected_y)) return FALSE;
  *x = selected_x;
  *y = selected_y;
  return TRUE;
}

void dos_vbe_move_selection(int dx, int dy)
{
  int x, y;
  if (!have_map()) return;
  if (!dos_vbe_get_selected_tile(&x, &y)) get_center_tile_mapcanvas(&x, &y);
  x = map_adjust_x((int)(((int64_t)x + dx) % map.xsize));
  y = (int)((int64_t)y + dy < 0 ? 0 :
            (int64_t)y + dy >= map.ysize ? map.ysize - 1 : (int64_t)y + dy);
  dos_vbe_select_tile(x, y);
  if (!tile_visible_mapcanvas(x, y)) center_tile_mapcanvas(x, y);
}
