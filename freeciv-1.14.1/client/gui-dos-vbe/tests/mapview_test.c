/* Execute the production renderer with a bounded fake world and display. */
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mapview.h"
#include "vbe_init.h"

#define WORLD_SIZE 64
#define SENTINEL 0xA5A5U
#define GRASS 0x1C46U
#define CITY 0xF800U
#define UNIT 0x07E0U
#define SELECTION 0xF800U

struct civ_map map;
struct dos_vbe_framebuffer dos_vbe_front_buffer;
static struct tile tiles[WORLD_SIZE * WORLD_SIZE];
static struct city test_city;
static int active;
static unsigned long display_generation;
static int init_calls;
static int present_calls;
static int lookup_calls;
static int click_calls;
static int control_calls;
static int clicked_x;
static int clicked_y;
static unsigned int init_width;
static unsigned int init_height;
static size_t presented_bytes;
static struct dos_vbe_dirty_rect presented_rect;

struct tile *map_get_tile(int x, int y)
{
  /* Fail immediately if clipping happens after an engine lookup. */
  assert(x >= 0 && y >= 0 && x < map.xsize && y < map.ysize);
  ++lookup_calls;
  return map.tiles ? &map.tiles[y * map.xsize + x] : NULL;
}

int unit_list_size(const struct unit_list *units)
{
  return units->count;
}

void key_move_north(void) { ++control_calls; }
void key_move_south(void) { ++control_calls; }
void key_move_east(void) { ++control_calls; }
void key_move_west(void) { ++control_calls; }
void key_end_turn(void) { ++control_calls; }
void key_cancel_action(void) { ++control_calls; }
void request_toggle_map_grid(void) { ++control_calls; }
void do_map_click(int x, int y)
{
  ++click_calls;
  clicked_x = x;
  clicked_y = y;
}

int dos_vbe_display_active(void)
{
  return active;
}

unsigned long dos_vbe_display_generation(void)
{
  return display_generation;
}

int dos_vbe_current_mode(struct dos_vbe_mode_info *info, unsigned int *mode)
{
  (void)info;
  (void)mode;
  return -1;
}

int vbe_init_display(void)
{
  ++init_calls;
  ++display_generation;
  assert(dos_vbe_framebuffer_init_pitch(&dos_vbe_front_buffer,
                                      init_width, init_height, 16U,
                                      init_width * 2U + 6U) == 0);
  active = 1;
  return 0;
}

int dos_vbe_present(void)
{
  int dirty;

  ++present_calls;
  presented_bytes = 0U;
  dirty = dos_vbe_framebuffer_dirty_peek(&dos_vbe_front_buffer,
                                        &presented_rect);
  assert(dirty >= 0);
  if (dirty) {
    assert(presented_rect.x + presented_rect.width
           <= dos_vbe_front_buffer.width);
    assert(presented_rect.y + presented_rect.height
           <= dos_vbe_front_buffer.height);
    presented_bytes = (size_t)presented_rect.width
                      * presented_rect.height * 2U;
  }
  assert(dos_vbe_framebuffer_dirty_clear(&dos_vbe_front_buffer) == 0);
  return 0;
}

size_t dos_vbe_last_present_bytes(void)
{
  return presented_bytes;
}

static unsigned int pixel(unsigned int x, unsigned int y)
{
  size_t offset;
  assert(x < dos_vbe_front_buffer.width && y < dos_vbe_front_buffer.height);
  offset = (size_t)y * dos_vbe_front_buffer.stride + x * 2U;
  return dos_vbe_front_buffer.pixels[offset]
         | ((unsigned int)dos_vbe_front_buffer.pixels[offset + 1U] << 8);
}

static void reset(unsigned int width, unsigned int height, int initialize)
{
  unsigned int i;

  dos_vbe_framebuffer_destroy(&dos_vbe_front_buffer);
  map.xsize = WORLD_SIZE;
  map.ysize = WORLD_SIZE;
  map.tiles = tiles;
  memset(tiles, 0, sizeof(tiles));
  for (i = 0U; i < WORLD_SIZE * WORLD_SIZE; ++i) {
    tiles[i].terrain = T_GRASSLAND;
  }
  active = 0;
  init_calls = 0;
  present_calls = 0;
  lookup_calls = 0;
  click_calls = 0;
  control_calls = 0;
  init_width = width;
  init_height = height;
  if (initialize) {
    assert(vbe_init_display() == 0);
    memset(dos_vbe_front_buffer.pixels, 0xA5,
           (size_t)dos_vbe_front_buffer.stride * height);
    assert(dos_vbe_framebuffer_dirty_clear(&dos_vbe_front_buffer) == 0);
  }
  center_tile_mapcanvas(0, 0);
  dos_vbe_apply_command(DOS_VBE_CMD_SELECT_TILE);
  click_calls = 0;
}

static void assert_padding(void)
{
  unsigned int y;
  unsigned int x;
  for (y = 0U; y < dos_vbe_front_buffer.height; ++y) {
    for (x = dos_vbe_front_buffer.width * 2U;
         x < dos_vbe_front_buffer.stride; ++x) {
      assert(dos_vbe_front_buffer.pixels[
               (size_t)y * dos_vbe_front_buffer.stride + x] == 0xA5);
    }
  }
}

static void test_partial_and_overlays(void)
{
  unsigned int x;
  unsigned int y;
  int old_clicks;
  struct dos_vbe_dirty_rect dirty;

  reset(49U, 97U, 1);
  center_tile_mapcanvas(10, 11);
  dos_vbe_apply_command(DOS_VBE_CMD_SELECT_TILE);
  assert(tile_visible_mapcanvas(10, 11));
  assert(tile_visible_mapcanvas(13, 14));
  assert(!tile_visible_mapcanvas(14, 14));
  assert(!tile_visible_mapcanvas(9, 11));
  assert(!tile_visible_mapcanvas(INT_MIN, INT_MAX));
  assert(tile_visible_and_not_on_border_mapcanvas(11, 12));
  assert(!tile_visible_and_not_on_border_mapcanvas(10, 11));
  assert(!tile_visible_and_not_on_border_mapcanvas(13, 14));
  tiles[12 * WORLD_SIZE + 11].city = &test_city;
  tiles[11 * WORLD_SIZE + 12].units.count = 1;
  old_clicks = click_calls;
  update_map_canvas(11, 12, 1, 1, FALSE);
  assert(present_calls == 0);
  assert(click_calls == old_clicks);
  assert(control_calls == 0);
  assert(pixel(21U, 21U) == CITY);
  assert(pixel(18U, 18U) == GRASS);
  assert(dos_vbe_framebuffer_dirty_peek(&dos_vbe_front_buffer, &dirty) == 1);
  assert(dirty.x == 16U && dirty.y == 16U
         && dirty.width == 16U && dirty.height == 16U);
  for (y = 0U; y < 97U; ++y) {
    for (x = 0U; x < 49U; ++x) {
      if (x < 16U || x >= 32U || y < 16U || y >= 32U) {
        assert(pixel(x, y) == SENTINEL);
      }
    }
  }
  update_map_canvas(12, 11, 1, 1, FALSE);
  assert(pixel(38U, 6U) == UNIT);
  update_map_canvas(10, 11, 1, 1, FALSE);
  assert(pixel(0U, 0U) == SELECTION);
  assert(pixel(8U, 8U) == GRASS);
  assert(present_calls == 0);
  assert_padding();

  /* The rightmost and bottommost tiles are only one pixel wide/high. */
  update_map_canvas(13, 14, 1, 1, FALSE);
  assert(pixel(48U, 48U) == GRASS);
  assert(pixel(47U, 48U) == SENTINEL);
  assert(pixel(48U, 47U) == SENTINEL);
  /* Updating the selected tile also refreshes its HUD summary. */
  assert(pixel(48U, 49U) != SENTINEL);
  assert_padding();

  update_map_canvas(11, 12, 1, 1, TRUE);
  assert(present_calls == 1);
  assert(dos_vbe_last_present_bytes() > 0U);
  assert(presented_rect.x == 0U && presented_rect.y == 0U
         && presented_rect.width == 49U && presented_rect.height == 97U);
  assert(pixel(47U, 48U) == SENTINEL);
  assert(pixel(0U, 49U) != SENTINEL);
  update_map_canvas(11, 12, 1, 1, TRUE);
  assert(present_calls == 2);
  assert(dos_vbe_last_present_bytes() == 0U);
  assert(click_calls == old_clicks && control_calls == 0);
  /* Redrawing must not silently move selection three/two tiles forward. */
  dos_vbe_apply_command(DOS_VBE_CMD_SELECT_TILE);
  assert(clicked_x == 10 && clicked_y == 11);

  /* A clipped TRUE request still flushes previously queued FALSE updates. */
  reset(49U, 97U, 1);
  center_tile_mapcanvas(10, 11);
  tiles[12 * WORLD_SIZE + 11].city = &test_city;
  update_map_canvas(11, 12, 1, 1, FALSE);
  old_clicks = lookup_calls;
  update_map_canvas(INT_MAX, INT_MAX, 1, 1, TRUE);
  assert(lookup_calls == old_clicks && present_calls == 1);
  assert(presented_rect.x == 16U && presented_rect.y == 16U
         && presented_rect.width == 16U && presented_rect.height == 16U);
  assert(dos_vbe_last_present_bytes() == 16U * 16U * 2U);
  assert(pixel(0U, 49U) == SENTINEL);
  update_map_canvas(11, 12, 1, 1, TRUE);
  assert(present_calls == 2 && dos_vbe_last_present_bytes() == 0U);
  assert_padding();
}

static void test_rectangle_clipping(void)
{
  unsigned char *before;
  size_t size;
  int count;
  struct dos_vbe_dirty_rect dirty;

  reset(33U, 81U, 1);
  center_tile_mapcanvas(10, 10);
  dos_vbe_apply_command(DOS_VBE_CMD_SELECT_TILE);
  size = (size_t)dos_vbe_front_buffer.stride * dos_vbe_front_buffer.height;
  before = malloc(size);
  assert(before);
  memcpy(before, dos_vbe_front_buffer.pixels, size);
  count = lookup_calls;
  update_map_canvas(10, 10, -1, 1, FALSE);
  update_map_canvas(10, 10, 1, INT_MIN, FALSE);
  update_map_canvas(10, 10, 0, INT_MAX, FALSE);
  update_map_canvas(INT_MIN, INT_MIN, 1, 1, FALSE);
  update_map_canvas(INT_MAX, INT_MAX, INT_MAX, INT_MAX, FALSE);
  update_map_canvas(-1000, -1000, 2, 2, FALSE);
  update_map_canvas(63, 63, 2, 2, FALSE);
  assert(lookup_calls == count);
  assert(memcmp(before, dos_vbe_front_buffer.pixels, size) == 0);
  assert(present_calls == 0);
  assert(dos_vbe_framebuffer_dirty_peek(&dos_vbe_front_buffer, &dirty) == 0);

  update_map_canvas(9, 9, 2, 2, FALSE);
  assert(pixel(0U, 0U) == SELECTION);
  assert(pixel(15U, 15U) == SELECTION);
  assert(pixel(16U, 0U) == SENTINEL);
  assert(pixel(0U, 16U) == SENTINEL);
  count = lookup_calls;
  /* A huge requested rectangle must be clipped before iteration/lookup. */
  update_map_canvas(INT_MIN, INT_MIN, INT_MAX, INT_MAX, FALSE);
  assert(lookup_calls == count);
  update_map_canvas(9, 9, INT_MAX, INT_MAX, FALSE);
  assert(lookup_calls > count && lookup_calls - count <= 18);
  assert(pixel(32U, 32U) == GRASS);
  assert(pixel(0U, 33U) != SENTINEL);
  assert_padding();

  center_tile_mapcanvas(INT_MAX, INT_MAX);
  count = lookup_calls;
  update_map_canvas(INT_MAX, INT_MAX, 1, 1, FALSE);
  center_tile_mapcanvas(INT_MIN, INT_MIN);
  update_map_canvas(INT_MIN, INT_MIN, INT_MAX, INT_MAX, FALSE);
  assert(lookup_calls == count);
  free(before);
}

static void test_tiny_and_lazy_display(void)
{
  static const unsigned int widths[] = { 1U, 2U, 7U, 17U, 33U };
  static const unsigned int heights[] = { 1U, 4U, 23U, 47U, 48U, 49U };
  unsigned int i;
  unsigned int j;
  int count;

  for (i = 0U; i < sizeof(widths) / sizeof(widths[0]); ++i) {
    for (j = 0U; j < sizeof(heights) / sizeof(heights[0]); ++j) {
      reset(widths[i], heights[j], 1);
      count = lookup_calls;
      update_map_canvas(0, 0, INT_MAX, INT_MAX, FALSE);
      assert(present_calls == 0);
      if (heights[j] <= 48U) {
        /* No world tiles are visible; only the selected HUD tile is read. */
        assert(lookup_calls == count + 1);
        assert(pixel(0U, 0U) != SENTINEL);
      } else {
        assert(pixel(0U, 0U) == SELECTION);
      }
      update_map_canvas_visible();
      assert(present_calls == 1);
      assert(pixel(0U, heights[j] - 1U) != SENTINEL);
      assert_padding();
    }
  }

  reset(33U, 81U, 0);
  update_map_canvas_visible();
  assert(init_calls == 1);
  assert(active && present_calls == 1);
  assert(pixel(32U, 32U) == GRASS);
  assert(pixel(0U, 0U) == SELECTION);
}

int main(int argc, char **argv)
{
  if (argc > 2 || (argc == 2 && strcmp(argv[1], "partial") != 0
                  && strcmp(argv[1], "clipping") != 0
                  && strcmp(argv[1], "tiny") != 0)) {
    fprintf(stderr, "usage: mapview-test [partial|clipping|tiny]\n");
    return 2;
  }
  if (argc == 1 || strcmp(argv[1], "partial") == 0) {
    test_partial_and_overlays();
  }
  if (argc == 1 || strcmp(argv[1], "clipping") == 0) {
    test_rectangle_clipping();
  }
  if (argc == 1 || strcmp(argv[1], "tiny") == 0) {
    test_tiny_and_lazy_display();
  }
  dos_vbe_framebuffer_destroy(&dos_vbe_front_buffer);
  puts("mapview: requested sanitizer checks passed");
  return 0;
}
