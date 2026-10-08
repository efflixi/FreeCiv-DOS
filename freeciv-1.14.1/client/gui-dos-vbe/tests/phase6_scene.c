#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "city.h"
#include "game.h"
#include "government.h"
#include "idex.h"
#include "map.h"
#include "mem.h"
#include "nation.h"
#include "player.h"
#include "registry.h"
#include "shared.h"
#include "support.h"
#include "tech.h"
#include "unit.h"
#include "civclient.h"
#include "climisc.h"
#include "control.h"
#include "goto.h"
#include "options.h"
#include "tilespec.h"
#include "mapview.h"
#include "vbe_init.h"
#include "phase6_scene.h"

static int initialized;
static int tiles_loaded;

static bool load_fixture_file(struct section_file *file,
                              const char *original, const char *short_name)
{
  const char *path;
  path = datafilename(original);
  if (!path) path = datafilename(short_name);
  if (!path || !section_file_load(file, path)) {
    fprintf(stderr, "FIXTURE missing or unreadable ruleset: %s (%s); DATA path: %s\n",
            short_name, original, datafilename(NULL));
    return FALSE;
  }
  fprintf(stderr, "FIXTURE loaded ruleset %s from %s\n", short_name, path);
  return TRUE;
}

static int load_fixture_ruleset(void)
{
  static const char *terrain_sections[T_COUNT] = {
    "glacier", "desert", "forest", "grassland", "hills", "jungle",
    "mountains", "ocean", "plains", "unused_0", "swamp", "tundra"
  };
  static const char *unit_sections[2] = {"warriors", "settlers"};
  struct section_file file;
  int i, j;

  if (!load_fixture_file(&file, "default/terrain.ruleset", "TERRAIN.RUL")) return -1;
  for (i = 0; i < T_COUNT; ++i) {
    struct tile_type *tt = get_tile_type((enum tile_terrain_type)i);
    const char *section = terrain_sections[i];
    const char *name = secfile_lookup_str(&file, "terrain_%s.terrain_name", section);
    if (!strcmp(name, "unused")) continue;
    sz_strlcpy(tt->terrain_name, name);
    sz_strlcpy(tt->graphic_str, secfile_lookup_str(&file, "terrain_%s.graphic", section));
    sz_strlcpy(tt->graphic_alt, secfile_lookup_str(&file, "terrain_%s.graphic_alt", section));
    sz_strlcpy(tt->special_1_name, secfile_lookup_str(&file, "terrain_%s.special_1_name", section));
    sz_strlcpy(tt->special_2_name, secfile_lookup_str(&file, "terrain_%s.special_2_name", section));
    tt->movement_cost = secfile_lookup_int(&file, "terrain_%s.movement_cost", section);
    for (j = 0; j < 2; ++j) {
      sz_strlcpy(tt->special[j].graphic_str,
                 secfile_lookup_str(&file, "terrain_%s.graphic_special_%d", section, j + 1));
      sz_strlcpy(tt->special[j].graphic_alt,
                 secfile_lookup_str(&file, "terrain_%s.graphic_special_%da", section, j + 1));
    }
  }
  section_file_free(&file);
  if (!load_fixture_file(&file, "default/units.ruleset", "UNITS.RUL")) return -1;
  game.num_unit_types = 2;
  for (i = 0; i < 2; ++i) {
    struct unit_type *ut = get_unit_type(i);
    const char *section = unit_sections[i];
    sz_strlcpy(ut->name, secfile_lookup_str(&file, "unit_%s.name", section));
    sz_strlcpy(ut->graphic_str, secfile_lookup_str(&file, "unit_%s.graphic", section));
    sz_strlcpy(ut->graphic_alt, secfile_lookup_str(&file, "unit_%s.graphic_alt", section));
    ut->hp = secfile_lookup_int(&file, "unit_%s.hitpoints", section);
    ut->move_rate = SINGLE_MOVE * secfile_lookup_int(&file, "unit_%s.move_rate", section);
    ut->move_type = LAND_MOVING;
  }
  section_file_free(&file);
  if (!load_fixture_file(&file, "default/governments.ruleset", "GOVERN.RUL")) return -1;
  governments_alloc(1);
  sz_strlcpy(get_government(0)->name, secfile_lookup_str(&file, "government_despotism.name"));
  sz_strlcpy(get_government(0)->graphic_str, secfile_lookup_str(&file, "government_despotism.graphic"));
  sz_strlcpy(get_government(0)->graphic_alt, "-");
  section_file_free(&file);
  if (!load_fixture_file(&file, "default/techs.ruleset", "TECHS.RUL")) return -1;
  game.num_tech_types = 2;
  sz_strlcpy(advances[1].name, secfile_lookup_str(&file, "advance_alphabet.name"));
  section_file_free(&file);
  nations_alloc(2);
  sz_strlcpy(get_nation_by_idx(0)->name, "Romans");
  sz_strlcpy(get_nation_by_idx(0)->flag_graphic_str, "f.rome");
  sz_strlcpy(get_nation_by_idx(1)->name, "Greeks");
  sz_strlcpy(get_nation_by_idx(1)->flag_graphic_str, "f.greece");
  city_styles_alloc(1);
  city_styles[0].replaced_by = -1;
  sz_strlcpy(city_styles[0].graphic, "classical");
  sz_strlcpy(city_styles[0].graphic_alt, "cd");
  return 0;
}

static struct unit *fixture_unit(int id, int owner, int type, int x, int y)
{
  struct unit *punit = fc_calloc(1, sizeof(*punit));
  punit->id = id;
  punit->owner = owner;
  punit->type = type;
  punit->x = x; punit->y = y;
  punit->hp = unit_type(punit)->hp - 2;
  punit->moves_left = SINGLE_MOVE;
  punit->activity = ACTIVITY_IDLE;
  unit_list_insert(&unit_owner(punit)->units, punit);
  unit_list_insert(&map_get_tile(x, y)->units, punit);
  idex_register_unit(punit);
  return punit;
}

static struct city *fixture_city(int id, int owner, int x, int y, const char *name)
{
  struct city *pcity = fc_calloc(1, sizeof(*pcity));
  pcity->id = id; pcity->owner = owner;
  pcity->x = x; pcity->y = y;
  pcity->size = owner ? 5 : 7;
  sz_strlcpy(pcity->name, name);
  unit_list_init(&pcity->units_supported);
  unit_list_init(&pcity->info_units_supported);
  unit_list_init(&pcity->info_units_present);
  ceff_vector_init(&pcity->effects);
  city_list_insert(&city_owner(pcity)->cities, pcity);
  map_set_city(x, y, pcity);
  idex_register_city(pcity);
  set_worker_city(pcity, 2, 2, C_TILE_WORKER);
  set_worker_city(pcity, 1, 2, C_TILE_WORKER);
  pcity->ppl_content[4] = pcity->size;
  return pcity;
}

int dos_phase6_scene_init(void)
{
  int i;
  struct unit *focus;
  if (initialized) {
    fprintf(stderr, "FIXTURE initialization refused: already initialized\n");
    return -1;
  }
  game_init();
  initialized = 1;
  fprintf(stderr, "FIXTURE initializing from DATA path: %s\n", datafilename(NULL));
  if (load_fixture_ruleset() != 0) return -1;
  game.nplayers = 2;
  game.year = -3500;
  seconds_to_turndone = 42;
  for (i = 0; i < 2; ++i) {
    struct player *pplayer = &game.players[i];
    pplayer->nation = i;
    pplayer->city_style = 0;
    pplayer->government = 0;
    sz_strlcpy(pplayer->name, i ? "Greek rival" : "Roman player");
    pplayer->economic.gold = 123;
    pplayer->economic.tax = 40;
    pplayer->economic.luxury = 10;
    pplayer->economic.science = 50;
    pplayer->research.researching = 1;
    pplayer->research.bulbs_researched = 17;
  }
  fprintf(stderr, "FIXTURE reading default Trident tilespec; staged lookup uses RESMAP.TXT/TRIDENT.TSP\n");
  tilespec_read_toplevel("");
  fprintf(stderr, "FIXTURE loading Trident specs and atlases\n");
  tilespec_load_tiles();
  tiles_loaded = 1;
  for (i = 0; i < T_COUNT; ++i) tilespec_setup_tile_type(i);
  for (i = 0; i < 2; ++i) {
    tilespec_setup_unit_type(i);
    tilespec_setup_nation_flag(i);
  }
  tilespec_setup_government(0);
  tilespec_alloc_city_tiles(1);
  tilespec_setup_city_tiles(0);
  map.xsize = 40; map.ysize = 24;
  map_allocate();
  whole_map_iterate(x, y) {
    struct tile *ptile = map_get_tile(x, y);
    ptile->terrain = y < 2 ? T_OCEAN : T_GRASSLAND;
    ptile->known = TILE_KNOWN;
  } whole_map_iterate_end;
  for (i = 0; i < T_COUNT; ++i) {
    if (i != T_RIVER) map_set_terrain(i + 2, 4, i);
  }
  map_get_tile(2, 3)->known = TILE_UNKNOWN;
  map_get_tile(3, 3)->known = TILE_KNOWN_FOGGED;
  map_set_special(4, 5, S_ROAD);
  map_set_special(5, 5, S_ROAD);
  map_set_special(6, 5, S_ROAD | S_RAILROAD);
  map_set_special(7, 5, S_ROAD | S_RAILROAD);
  map_set_special(8, 5, S_IRRIGATION);
  map_set_special(9, 5, S_IRRIGATION | S_FARMLAND);
  map_set_terrain(10, 5, T_HILLS);
  map_set_special(10, 5, S_MINE);
  map_set_special(4, 6, S_RIVER);
  map_set_special(5, 6, S_RIVER);
  map_set_special(6, 6, S_SPECIAL_1);
  map_set_terrain(7, 6, T_FOREST);
  map_set_special(7, 6, S_SPECIAL_2);
  map_set_special(8, 6, S_POLLUTION);
  map_set_special(9, 6, S_FORTRESS);
  map_set_special(10, 6, S_AIRBASE);
  map_set_special(11, 6, S_HUT);
  map_set_special(12, 6, S_FALLOUT);
  fixture_city(200, 0, 5, 9, "Rome");
  fixture_city(201, 1, 10, 9, "Athens");
  find_city_by_id(201)->ppl_unhappy[4] = 1;
  fixture_city(202, 0, 14, 12, "M\374nchen");
  fixture_city(203, 0, 2, 11, "S\343o Paulo");
  focus = fixture_unit(100, 0, 0, 7, 9);
  focus->veteran = TRUE;
  fixture_unit(101, 0, 1, 7, 9)->activity = ACTIVITY_FORTIFIED;
  fixture_unit(102, 1, 1, 12, 9)->activity = ACTIVITY_IRRIGATE;
  fixture_unit(103, 1, 0, 10, 9);
  draw_city_productions = FALSE;
  draw_city_names = draw_fog_of_war = TRUE;
  set_focus_unit_hidden_state(FALSE);
  init_client_goto();
  set_unit_focus_no_center(focus);
  printf("SCENE actual game/map/list/idex; Trident %dx%d; 2 players, 4 units, 4 cities\n",
         NORMAL_TILE_WIDTH, NORMAL_TILE_HEIGHT);
  return 0;
}

int dos_phase6_scene_render(void)
{
  uint32_t hash = 2166136261U;
  unsigned int x, y;
  if (!initialized || !tiles_loaded || !dos_vbe_display_active()) {
    fprintf(stderr, "FIXTURE render refused: initialized=%d tiles=%d display=%d\n",
            initialized, tiles_loaded, dos_vbe_display_active());
    return -1;
  }
  center_tile_mapcanvas(8, 7);
  dos_vbe_select_tile(5, 9);
  update_unit_info_label(find_unit_by_id(100));
  set_overview_dimensions(map.xsize, map.ysize);
  set_indicator_icons(1, 2, 3, 0);
  draw_segment(7, 9, DIR8_NORTH);
  draw_segment(7, 8, DIR8_EAST);
  put_cross_overlay_tile(9, 8);
  put_city_workers(find_city_by_id(200), COLOR_STD_GROUND);
  update_map_canvas_visible();
  printf("SCENE HUD: %s\n", dos_vbe_map_hud_text());
  if (dos_vbe_framebuffer_validate(&dos_vbe_front_buffer) != 0) {
    fprintf(stderr, "FIXTURE render produced an invalid framebuffer\n");
    return -1;
  }
  for (y = 0; y < dos_vbe_front_buffer.height; ++y) {
    for (x = 0; x < dos_vbe_front_buffer.width * 2; ++x) {
      hash = (hash ^ dos_vbe_front_buffer.pixels[(size_t)y * dos_vbe_front_buffer.stride + x])
             * 16777619U;
    }
  }
  printf("SCENE framebuffer %ux%u RGB565 FNV1a %08lx (visible pixels, no pitch padding)\n",
         dos_vbe_front_buffer.width, dos_vbe_front_buffer.height, (unsigned long)hash);
  return 0;
}

void dos_phase6_scene_free(void)
{
  if (!initialized) return;
  set_unit_focus_no_center(NULL);
  free_client_goto();
  dos_vbe_mapview_free();
  if (tiles_loaded) {
    tilespec_free_city_tiles(game.styles_count);
    tilespec_free_tiles();
    tiles_loaded = 0;
  }
  game_free();
  initialized = 0;
}
