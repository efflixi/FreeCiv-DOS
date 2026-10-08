/* The map/game/compositor/control/goto implementations are the real client.
 * Only the unavailable VBE hardware boundary is replaced by a pitched buffer. */
#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "city.h"
#include "game.h"
#include "map.h"
#include "mem.h"
#include "nation.h"
#include "player.h"
#include "shared.h"
#include "support.h"
#include "unit.h"
#include "civclient.h"
#include "climisc.h"
#include "control.h"
#include "goto.h"
#include "options.h"
#include "packhand.h"
#include "packets.h"
#include "tilespec.h"
#include "graphics.h"
#include "mapview.h"
#include "vbe_init.h"
#include "phase6_scene.h"

struct dos_vbe_framebuffer dos_vbe_front_buffer;
bool is_server = FALSE;
int seconds_to_turndone = 42;
static unsigned int present_count;
static size_t present_bytes;
static unsigned long generation;
static int active;
static unsigned int display_width = 640;
static unsigned int display_height = 480;

void dealloc_id(int id) { (void)id; }
int dos_vbe_display_active(void) { return active; }
unsigned long dos_vbe_display_generation(void) { return generation; }
int dos_vbe_current_mode(struct dos_vbe_mode_info *info, unsigned int *mode)
{
  if (!active) return -1;
  memset(info, 0, sizeof(*info));
  info->bits_per_pixel = 16;
  info->red_mask_size = 5; info->red_field_position = 11;
  info->green_mask_size = 6; info->green_field_position = 5;
  info->blue_mask_size = 5;
  *mode = DOS_VBE_MODE_640X480X16;
  return 0;
}
int vbe_init_display(void)
{
  assert(dos_vbe_framebuffer_init_pitch(&dos_vbe_front_buffer,
                                        display_width, display_height, 16,
                                        display_width * 2 + 8) == 0);
  active = 1;
  ++generation;
  return 0;
}
int dos_vbe_present(void)
{
  struct dos_vbe_dirty_rect rect;
  int dirty = dos_vbe_framebuffer_dirty_peek(&dos_vbe_front_buffer, &rect);
  assert(dirty >= 0);
  present_bytes = dirty ? (size_t)rect.width * rect.height * 2 : 0;
  ++present_count;
  return dos_vbe_framebuffer_dirty_clear(&dos_vbe_front_buffer);
}

static uint32_t tile_hash(int x, int y);

static void test_partial_and_noop(void)
{
  enum tile_terrain_type old_local, old_remote;
  uint32_t remote_before;
  unsigned int before;
  center_tile_mapcanvas(8, 7);
  update_map_canvas_visible();
  update_map_canvas_visible();
  assert(present_bytes == 0);
  old_local = map_get_tile(7, 10)->terrain;
  old_remote = map_get_tile(7, 3)->terrain;
  remote_before = tile_hash(7, 3);
  map_get_tile(7, 10)->terrain = old_local == T_OCEAN ? T_GRASSLAND : T_OCEAN;
  map_get_tile(7, 3)->terrain = old_remote == T_OCEAN ? T_GRASSLAND : T_OCEAN;
  before = present_count;
  update_map_canvas(7, 10, 1, 1, FALSE);
  assert(present_count == before);
  assert(tile_hash(7, 3) == remote_before);
  update_map_canvas(7, 10, 1, 1, TRUE);
  update_map_canvas(7, 10, 1, 1, TRUE);
  assert(present_bytes == 0);
  map_get_tile(7, 10)->terrain = old_local;
  map_get_tile(7, 3)->terrain = old_remote;
  update_map_canvas_visible();
  printf("PASS partial world damage retains unrelated map rows, FALSE batches,\n"
         "     repeated composed map/HUD/overview presentation writes zero bytes\n");
}

static unsigned int pixel(int x, int y)
{
  size_t offset;
  assert(x >= 0 && y >= 0 && x < (int)dos_vbe_front_buffer.width
         && y < (int)dos_vbe_front_buffer.height);
  offset = (size_t)y * dos_vbe_front_buffer.stride + (size_t)x * 2;
  return dos_vbe_front_buffer.pixels[offset]
         | ((unsigned int)dos_vbe_front_buffer.pixels[offset + 1] << 8);
}

static uint32_t region_hash(int cx, int cy, int width, int height)
{
  int i, j;
  uint32_t hash = 2166136261U;
  for (j = 0; j < height; ++j) {
    for (i = 0; i < width; ++i) {
      hash = (hash ^ pixel(cx + i, cy + j)) * 16777619U;
    }
  }
  return hash;
}

static uint32_t tile_hash(int x, int y)
{
  int cx, cy;
  assert(dos_vbe_map_to_canvas(x, y, &cx, &cy));
  return region_hash(cx, cy, NORMAL_TILE_WIDTH, NORMAL_TILE_HEIGHT);
}

static void assert_layer(int x, int y, struct Sprite *sprite)
{
  struct Sprite *layers[80];
  struct player *owner;
  int i, count, solid;
  assert(sprite);
  count = fill_tile_sprite_array(layers, x, y, FALSE, &solid, &owner);
  for (i = 0; i < count; ++i) if (layers[i] == sprite) return;
  assert(!"Expected real tileset layer missing");
}

static void test_map_and_sprites(void)
{
  static const int sx[] = {4, 5, 6, 7, 8, 9, 10, 4, 6, 7, 8, 9, 10, 11, 12};
  static const int sy[] = {5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6};
  uint32_t before, after;
  enum tile_special_type special;
  int i, cx, cy, x, y;
  struct unit *focus = get_unit_in_focus();
  assert(focus && focus->id == 100);
  draw_city_names = FALSE;
  center_tile_mapcanvas(8, 7);
  update_map_canvas_visible();
  assert(dos_vbe_map_to_canvas(2, 3, &cx, &cy));
  for (y = 0; y < NORMAL_TILE_HEIGHT; ++y)
    for (x = 0; x < NORMAL_TILE_WIDTH; ++x) assert(pixel(cx + x, cy + y) == 0);
  assert(tile_hash(3, 3) != tile_hash(4, 3));
  assert_layer(3, 3, sprites.tx.fog);
  assert_layer(8, 5, sprites.tx.irrigation);
  assert_layer(9, 5, sprites.tx.farmland);
  assert_layer(10, 5, sprites.tx.mine);
  assert_layer(6, 6, get_tile_type(T_GRASSLAND)->special[0].sprite);
  assert_layer(7, 6, get_tile_type(T_FOREST)->special[1].sprite);
  assert_layer(8, 6, sprites.tx.pollution);
  assert_layer(9, 6, sprites.tx.fortress);
  assert_layer(10, 6, sprites.tx.airbase);
  assert_layer(11, 6, sprites.tx.village);
  assert_layer(12, 6, sprites.tx.fallout);
  assert_layer(7, 9, sprites.unit.stack);
  assert_layer(7, 9, unit_type(focus)->sprite);
  assert_layer(7, 9, get_nation_by_idx(0)->flag_sprite);
  assert_layer(7, 9, sprites.unit.hp_bar[(NUM_TILES_HP_BAR - 1) * 8 / 10]);
  assert_layer(12, 9, unit_type(find_unit_by_id(102))->sprite);
  assert_layer(12, 9, get_nation_by_idx(1)->flag_sprite);
  assert_layer(12, 9, sprites.unit.irrigate);
  assert_layer(5, 9, sprites.city.size[7]);
  assert_layer(10, 9, sprites.city.size[5]);
  assert_layer(10, 9, sprites.city.disorder);
  assert_layer(5, 9, get_nation_by_idx(0)->flag_sprite);
  assert_layer(10, 9, get_nation_by_idx(1)->flag_sprite);
  before = tile_hash(7, 9);
  set_focus_unit_hidden_state(TRUE);
  update_map_canvas_visible();
  assert(before != tile_hash(7, 9));
  set_focus_unit_hidden_state(FALSE);
  update_map_canvas_visible();
  for (i = 0; i < (int)(sizeof(sx) / sizeof(sx[0])); ++i) {
    struct tile *ptile = map_get_tile(sx[i], sy[i]);
    before = tile_hash(sx[i], sy[i]);
    special = ptile->special;
    ptile->special = S_NO_SPECIAL;
    update_map_canvas(sx[i], sy[i], 1, 1, FALSE);
    after = tile_hash(sx[i], sy[i]);
    assert(before != after);
    ptile->special = special;
    update_map_canvas(sx[i], sy[i], 1, 1, FALSE);
  }
  before = tile_hash(5, 10);
  draw_city_names = TRUE;
  update_city_descriptions();
  assert(before != tile_hash(5, 10));
  printf("PASS real known/unknown/fog terrain, road/rail, irrigation/farmland, mine,\n"
         "     river/resource/pollution/fortress/airbase/hut/fallout and unit stack/type sprites\n");
}

static void test_transforms(void)
{
  int cx, cy, x, y;
  struct unit *focus = get_unit_in_focus();
  center_tile_mapcanvas(39, 7);
  assert(dos_vbe_map_to_canvas(0, 7, &cx, &cy));
  assert(dos_vbe_canvas_to_map(cx + 15, cy + 15, &x, &y));
  assert(x == 0 && y == 7);
  assert(tile_visible_mapcanvas(-40, 7));
  assert(!tile_visible_mapcanvas(15, 7));
  assert(!tile_visible_mapcanvas(0, -1));
  assert(!tile_visible_mapcanvas(0, map.ysize));
  assert(!dos_vbe_canvas_to_map(-1, 0, &x, &y));
  assert(!dos_vbe_canvas_to_map(480, 0, &x, &y));
  assert(!dos_vbe_canvas_to_map(0, 368, &x, &y));
  draw_segment(39, 7, DIR8_EAST);
  assert(get_drawn(39, 7, DIR8_EAST) == 1);
  undraw_segment(39, 7, DIR8_EAST);
  assert(get_drawn(39, 7, DIR8_EAST) == 0);
  dos_vbe_select_tile(0, 7);
  dos_vbe_scroll_map(1, 0);
  assert(get_unit_in_focus() == focus && focus->x == 7 && focus->y == 9);
  assert(strstr(dos_vbe_map_hud_text(), "Tile 0,7"));
  center_tile_mapcanvas(INT_MIN, INT_MAX);
  assert(tile_visible_mapcanvas(map_adjust_x(INT_MIN), map.ysize - 1));
  assert(dos_vbe_map_to_canvas(map_adjust_x(INT_MIN), map.ysize - 1, &cx, &cy));
  assert(cy + NORMAL_TILE_HEIGHT <= 368); /* complete south-pole tile */
  center_tile_mapcanvas(8, 7);
  assert(dos_vbe_map_to_canvas(0, 1, &cx, &cy));
  assert(!tile_visible_and_not_on_border_mapcanvas(0, 1));
  assert(tile_visible_and_not_on_border_mapcanvas(1, 2));
  printf("PASS seam normalization, centering/poles, scroll, inverse transforms, borders;\n"
         "     selection and viewport do not move the authoritative focus unit\n");
}

static void test_hud_overview_and_hooks(void)
{
  struct tile *snapshot;
  struct unit before = *find_unit_by_id(100);
  struct city city_before = *find_city_by_id(200);
  struct player_economic economy = game.player_ptr->economic;
  const char *hud;
  int cx, cy;
  uint32_t hash;
  size_t bytes = (size_t)map.xsize * map.ysize * sizeof(*snapshot);
  snapshot = malloc(bytes);
  assert(snapshot);
  memcpy(snapshot, map.tiles, bytes);
  assert(dos_phase6_scene_render() == 0);
  hud = dos_vbe_map_hud_text();
  assert(strstr(hud, "Roman player") && strstr(hud, "3500"));
  assert(strstr(hud, "Despotism") && strstr(hud, "Gold 123"));
  assert(strstr(hud, "Tax 40% Lux 10% Sci 50%"));
  assert(strstr(hud, "Research Alphabet (17 bulbs)"));
  assert(strstr(hud, "Warriors (Roman player) HP 8/10 MP 1/1"));
  assert(strstr(hud, "City Rome size 7 owner Roman player"));
  assert(strstr(hud, "Timeout 42"));
  dos_vbe_select_tile(10, 9);
  assert(strstr(dos_vbe_map_hud_text(), "City Athens size 5 owner Greek rival DISORDER"));
  dos_vbe_select_tile(5, 9);
  assert(pixel(482 + 2 * 2, 14 + 3 * 2) == 0); /* unknown overview */
  assert(pixel(482, 14 + 3 * 2) == 0xffffU); /* viewport left edge */
  assert(dos_vbe_map_to_canvas(9, 8, &cx, &cy));
  assert(sprites.user.attention);
  {
    uint32_t marked = tile_hash(9, 8);
    put_cross_overlay_tile(13, 3);
    assert(marked != tile_hash(9, 8));
    put_cross_overlay_tile(9, 8);
  }
  assert(get_drawn(7, 9, DIR8_NORTH) == 1);
  hash = tile_hash(4, 9);
  put_city_workers(NULL, -1);
  assert(hash != tile_hash(4, 9));
  put_city_workers(find_city_by_id(200), COLOR_STD_GROUND);
  draw_segment(7, 9, DIR8_NORTH);
  assert(get_drawn(7, 9, DIR8_NORTH) == 2);
  undraw_segment(7, 9, DIR8_NORTH);
  assert(get_drawn(7, 9, DIR8_NORTH) == 1);
  undraw_segment(7, 9, DIR8_NORTH);
  assert(get_drawn(7, 9, DIR8_NORTH) == 0);
  hash = tile_hash(7, 9);
  draw_segment(7, 9, DIR8_NORTH);
  assert(hash != tile_hash(7, 9));
  update_info_label();
  update_timeout_label();
  update_turn_done_button(TRUE);
  update_unit_info_label(find_unit_by_id(100));
  update_city_descriptions();
  update_map_canvas_scrollbars();
  overview_update_tile(2, 3);
  refresh_overview_canvas();
  refresh_overview_viewrect();
  hash = tile_hash(8, 9);
  move_unit_map_canvas(find_unit_by_id(100), 7, 9, 1, 0);
  assert(hash != tile_hash(8, 9));
  update_map_canvas_visible();
  hash = tile_hash(7, 9);
  decrease_unit_hp_smooth(find_unit_by_id(100), 3, find_unit_by_id(102), 0);
  assert(hash != tile_hash(7, 9));
  put_nuke_mushroom_pixmaps(11, 5);
  assert(!memcmp(snapshot, map.tiles, bytes));
  assert(!memcmp(&before, find_unit_by_id(100), sizeof(before)));
  assert(!memcmp(&city_before, find_city_by_id(200), sizeof(city_before)));
  assert(!memcmp(&economy, &game.player_ptr->economic, sizeof(economy)));
  free(snapshot);
  printf("PASS HUD/economy/research/government/tile/unit/city, overview unknown/viewport,\n"
         "     persistent target/workers/routes, counted route removal, all repaint hooks;\n"
         "     render hooks leave actual map/unit/city/economy state unchanged\n");
}

static void assert_padding(void)
{
  unsigned int y, x;
  for (y = 0; y < dos_vbe_front_buffer.height; ++y) {
    for (x = dos_vbe_front_buffer.width * 2; x < dos_vbe_front_buffer.stride; ++x) {
      assert(dos_vbe_front_buffer.pixels[(size_t)y * dos_vbe_front_buffer.stride + x] == 0xa5);
    }
  }
}

static void test_display_sizes(void)
{
  static const unsigned int widths[] = {800, 1024, 640};
  static const unsigned int heights[] = {600, 768, 480};
  unsigned int i;
  int cx, cy, x, y;
  for (i = 0; i < sizeof(widths) / sizeof(widths[0]); ++i) {
    assert(dos_vbe_framebuffer_init_pitch(&dos_vbe_front_buffer,
                                          widths[i], heights[i], 16,
                                          widths[i] * 2 + 8) == 0);
    ++generation;
    memset(dos_vbe_front_buffer.pixels, 0xa5, dos_vbe_front_buffer.size_bytes);
    set_overview_dimensions(INT_MAX, INT_MAX);
    center_tile_mapcanvas(8, 7);
    update_map_canvas(INT_MIN, INT_MIN, INT_MAX, INT_MAX, TRUE);
    assert(dos_vbe_map_to_canvas(5, 9, &cx, &cy));
    assert(dos_vbe_canvas_to_map(cx + 15, cy + 15, &x, &y));
    assert(x == 5 && y == 9);
    assert(strstr(dos_vbe_map_hud_text(), "Roman player"));
    assert_padding();
  }
  printf("PASS 640x480, 800x600, 1024x768 resize, bounded extreme damage/overview dimensions\n");
}

static void test_combat_packet_ownership(void)
{
  struct unit *attacker = find_unit_by_id(100);
  struct unit *defender = find_unit_by_id(102);
  struct unit before_attacker = *attacker, before_defender = *defender;
  struct unit expected_attacker, expected_defender;
  struct packet_unit_combat packet;
  char fight[2][MAX_LEN_NAME], alternate[2][MAX_LEN_NAME];
  bool animation = do_combat_animation, autocenter = auto_center_on_combat;
  unsigned int presented;
  uint32_t before;
  int i;
  /* Combat packet ownership is tested with sound disabled in the fixture;
   * audio_play_sound remains the real implementation, not a test stub. */
  for (i = 0; i < 2; ++i) {
    memcpy(fight[i], get_unit_type(i)->sound_fight, sizeof(fight[i]));
    memcpy(alternate[i], get_unit_type(i)->sound_fight_alt, sizeof(alternate[i]));
    strcpy(get_unit_type(i)->sound_fight, "-");
    strcpy(get_unit_type(i)->sound_fight_alt, "-");
  }
  memset(&packet, 0, sizeof(packet));
  packet.attacker_unit_id = attacker->id;
  packet.defender_unit_id = defender->id;
  center_tile_mapcanvas(8, 7);
  before = tile_hash(attacker->x, attacker->y);
  presented = present_count;
  do_combat_animation = TRUE;
  packet.make_winner_veteran = attacker->veteran ? 1 : 0;
  packet.attacker_hp = 4; packet.defender_hp = 0;
  handle_unit_combat(&packet);
  assert(attacker->hp == 4 && defender->hp == 0);
  expected_attacker = before_attacker; expected_attacker.hp = 4;
  expected_defender = before_defender; expected_defender.hp = 0;
  assert(!memcmp(&expected_attacker, attacker, sizeof(*attacker)));
  assert(!memcmp(&expected_defender, defender, sizeof(*defender)));
  assert(before != tile_hash(attacker->x, attacker->y));
  assert(present_count > presented);
  assert(strstr(dos_vbe_map_hud_text(), "HP 4/10"));
  attacker->hp = before_attacker.hp;
  defender->hp = before_defender.hp;
  do_combat_animation = FALSE;
  packet.attacker_hp = 3; packet.defender_hp = 0;
  handle_unit_combat(&packet);
  assert(attacker->hp == 3 && defender->hp == 0);
  assert(strstr(dos_vbe_map_hud_text(), "HP 3/10"));
  attacker->hp = before_attacker.hp;
  defender->hp = before_defender.hp;
  auto_center_on_combat = FALSE;
  do_combat_animation = TRUE;
  center_tile_mapcanvas(39, 7);
  assert(!tile_visible_mapcanvas(attacker->x, attacker->y));
  assert(!tile_visible_mapcanvas(defender->x, defender->y));
  presented = present_count;
  packet.attacker_hp = 2; packet.defender_hp = 0;
  handle_unit_combat(&packet);
  /* The offscreen notification retains legacy behavior: unithand.c sends
   * winner unit-info and loser removal after this combat notification. */
  assert(attacker->hp == before_attacker.hp && defender->hp == before_defender.hp);
  assert(present_count == presented);
  attacker->hp = before_attacker.hp;
  defender->hp = before_defender.hp;
  for (i = 0; i < 2; ++i) {
    memcpy(get_unit_type(i)->sound_fight, fight[i], sizeof(fight[i]));
    memcpy(get_unit_type(i)->sound_fight_alt, alternate[i], sizeof(alternate[i]));
  }
  do_combat_animation = animation;
  auto_center_on_combat = autocenter;
  center_tile_mapcanvas(8, 7);
  printf("PASS real combat packet owns visible animated/nonanimated HP updates;\n"
         "     renderer preserves other unit fields and offscreen notification behavior\n");
}

static void test_accented_names(void)
{
  struct city *pcity = find_city_by_id(202);
  char original[MAX_LEN_NAME];
  uint32_t accented, ascii;
  int cx, cy;
  assert(pcity);
  memcpy(original, pcity->name, sizeof(original));
  center_tile_mapcanvas(8, 7);
  dos_vbe_select_tile(14, 12);
  assert(strstr(dos_vbe_map_hud_text(), "City Munchen size 7"));
  assert(!memcmp(original, pcity->name, sizeof(original)));
  assert(dos_vbe_map_to_canvas(14, 12, &cx, &cy));
  accented = region_hash(cx, cy + NORMAL_TILE_HEIGHT + 1, 60, 7);
  strcpy(pcity->name, "Munchen");
  update_city_descriptions();
  ascii = region_hash(cx, cy + NORMAL_TILE_HEIGHT + 1, 60, 7);
  assert(accented == ascii);
  strcpy(pcity->name, "M\303\274nchen"); /* equivalent UTF-8 display */
  update_info_label();
  assert(strstr(dos_vbe_map_hud_text(), "City Munchen size 7"));
  assert(!strcmp(pcity->name, "M\303\274nchen"));
  memcpy(pcity->name, original, sizeof(original));
  dos_vbe_select_tile(2, 11);
  assert(strstr(dos_vbe_map_hud_text(), "City Sao Paulo size 7"));
  dos_vbe_select_tile(5, 9);
  printf("PASS readable Latin-1/UTF-8 accented fixture names without stored-name mutation\n");
}

static void test_resource_metadata(void)
{
  const char **tilesets;
  const char *staged = getenv("MAPVIEW_STAGED_DATA");
  struct Sprite *intro, *radar;
  char *saved;
  int i, found = 0;
  assert(NORMAL_TILE_WIDTH == 30 && NORMAL_TILE_HEIGHT == 30 && !is_isometric);
  assert(main_intro_filename && minimap_intro_filename);
  assert(main_intro_filename != minimap_intro_filename);
  assert(strcmp(main_intro_filename, minimap_intro_filename));
  saved = mystrdup(main_intro_filename);
  intro = load_gfxfile(main_intro_filename);
  radar = load_gfxfile(minimap_intro_filename);
  assert(intro && radar && intro != radar);
  assert(intro->width > 0 && intro->height > 0);
  assert(radar->width > 0 && radar->height > 0);
  assert(!strcmp(saved, main_intro_filename));
  free(saved);
  free_sprite(intro);
  free_sprite(radar);
  if (staged && *staged) {
    char *mapfile = datafilename("RESMAP.TXT");
    char *alias;
    const char *intro_name = strrchr(main_intro_filename, '/');
    const char *radar_name = strrchr(minimap_intro_filename, '/');
    assert(mapfile);
    alias = dos_vbe_resource_filename(mapfile, "TRIDENT.tilespec");
    assert(alias && !strcmp(alias, "TRIDENT.TSP"));
    free(alias);
    assert(!datafilename("trident.tilespec"));
    assert(!datafilename("default/terrain.ruleset"));
    assert(datafilename("TERRAIN.RUL"));
    assert(datafilename("UNITS.RUL"));
    assert(datafilename("GOVERN.RUL"));
    assert(datafilename("TECHS.RUL"));
    assert(intro_name && !mystrcasecmp(intro_name + 1, "G0000000.XPM"));
    assert(radar_name && !mystrcasecmp(radar_name + 1, "G0000001.XPM"));
    printf("PASS staged 8.3 rulesets, RESMAP/default and independent intro/radar aliases\n");
  } else {
    printf("PASS source default and independent intro/radar filenames\n");
  }
  tilesets = get_tileset_list();
  for (i = 0; tilesets[i]; ++i) {
    if (!mystrcasecmp(tilesets[i], "trident")) found = 1;
  }
  assert(found);
  printf("PASS Trident tileset discovery\n");
}

static int write_reference(const char *path)
{
  FILE *file = fopen(path, "wb");
  unsigned int x, y;
  uint32_t hash = 2166136261U;
  int failed = 0;
  if (!file) { perror(path); return -1; }
  fprintf(file, "P6\n%u %u\n255\n", dos_vbe_front_buffer.width, dos_vbe_front_buffer.height);
  for (y = 0; y < dos_vbe_front_buffer.height; ++y) {
    for (x = 0; x < dos_vbe_front_buffer.width; ++x) {
      unsigned int color = pixel(x, y);
      unsigned int red = (color >> 11) & 31;
      unsigned int green = (color >> 5) & 63;
      unsigned int blue = color & 31;
      unsigned char rgb[3];
      rgb[0] = (unsigned char)((red << 3) | (red >> 2));
      rgb[1] = (unsigned char)((green << 2) | (green >> 4));
      rgb[2] = (unsigned char)((blue << 3) | (blue >> 2));
      hash = (hash ^ (color & 255)) * 16777619U;
      hash = (hash ^ (color >> 8)) * 16777619U;
      if (fwrite(rgb, 1, sizeof(rgb), file) != sizeof(rgb)) failed = 1;
    }
  }
  if (fclose(file) != 0) failed = 1;
  if (failed) { fprintf(stderr, "Reference image write failed: %s\n", path); return -1; }
  printf("REFERENCE fixture-only %ux%u RGB565 FNV1a %08lx: %s\n",
         dos_vbe_front_buffer.width, dos_vbe_front_buffer.height, (unsigned long)hash, path);
  return 0;
}

int main(int argc, char **argv)
{
  int reference = argc >= 3 && !strcmp(argv[1], "--reference");
  int expect_init_failure = argc == 2 && !strcmp(argv[1], "--expect-init-failure");
  int result = EXIT_SUCCESS;
  setvbuf(stdout, NULL, _IONBF, 0);
  if (argc != 1 && !expect_init_failure && !(reference && (argc == 3 || argc == 4))) {
    fprintf(stderr, "Usage: mapview-test [--reference output.ppm [640|800] | --expect-init-failure]\n");
    return EXIT_FAILURE;
  }
  if (reference && argc == 4) {
    if (!strcmp(argv[3], "800")) { display_width = 800; display_height = 600; }
    else if (strcmp(argv[3], "640")) return EXIT_FAILURE;
  }
  assert(vbe_init_display() == 0);
  memset(dos_vbe_front_buffer.pixels, 0xa5, dos_vbe_front_buffer.size_bytes);
  if (expect_init_failure) {
    assert(dos_phase6_scene_init() == -1);
    dos_phase6_scene_free();
    dos_vbe_framebuffer_destroy(&dos_vbe_front_buffer);
    active = 0;
    printf("PASS fixture initialization failure and cleanup\n");
    return EXIT_SUCCESS;
  }
  assert(dos_phase6_scene_init() == 0);
  test_resource_metadata();
  if (reference) {
    assert(dos_phase6_scene_render() == 0);
    if (write_reference(argv[2]) != 0) result = EXIT_FAILURE;
  } else {
    test_map_and_sprites();
    test_transforms();
    test_hud_overview_and_hooks();
    test_combat_packet_ownership();
    test_accented_names();
    test_partial_and_noop();
    assert_padding();
    test_display_sizes();
  }
  assert(present_count > 0);
  dos_phase6_scene_free();
  dos_vbe_framebuffer_destroy(&dos_vbe_front_buffer);
  active = 0;
  printf("PASS real-state DOS map rendering with ASan/UBSan and pitched guard bytes\n");
  return result;
}
