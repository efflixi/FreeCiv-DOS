/* dos_client_stubs.c -- minimal DOS client ABI stub for the Freeciv link step */

#include <stdbool.h>
#include <stdio.h>

struct unit;
struct packet_diplomacy_info;

const char *client_string = "gui-dos-vbe";

void ui_init(void)
{
  fprintf(stderr, "DOS VBE UI stub: ui_init()\n");
}

void ui_main(int argc, char *argv[])
{
  (void)argc;
  (void)argv;
  fprintf(stderr, "DOS VBE UI stub: ui_main()\n");
}

void sound_bell(void)
{
}

void add_net_input(int sock)
{
  (void)sock;
}

void remove_net_input(void)
{
}

void set_unit_icon(int idx, struct unit *punit)
{
  (void)idx;
  (void)punit;
}

void set_unit_icons_more_arrow(bool onoff)
{
  (void)onoff;
}

void update_menus(void)
{
}

void gui_server_connect(void)
{
}

void server_autoconnect(void)
{
}

void popdown_all_city_dialogs(void)
{
}

void close_all_diplomacy_dialogs(void)
{
}

void handle_diplomacy_accept_treaty(struct packet_diplomacy_info *pa)
{
  (void)pa;
}

void handle_diplomacy_init_meeting(struct packet_diplomacy_info *pa)
{
  (void)pa;
}

void handle_diplomacy_create_clause(struct packet_diplomacy_info *pa)
{
  (void)pa;
}

void handle_diplomacy_cancel_meeting(struct packet_diplomacy_info *pa)
{
  (void)pa;
}

void handle_diplomacy_remove_clause(struct packet_diplomacy_info *pa)
{
  (void)pa;
}

void update_turn_done_button(void)
{
}

void update_timeout_label(void)
{
}

void center_tile_mapcanvas(int x, int y)
{
  (void)x;
  (void)y;
}

bool tile_visible_and_not_on_border_mapcanvas(int x, int y)
{
  (void)x;
  (void)y;
  return true;
}

void create_line_at_mouse_pos(int x, int y)
{
  (void)x;
  (void)y;
}

void popdown_help_dialog(void)
{
}

void popup_city_dialog(void *pcity, bool make_modal)
{
  (void)pcity;
  (void)make_modal;
}

void popdown_city_dialog(void *pcity)
{
  (void)pcity;
}

void refresh_city_dialog(void *pcity)
{
  (void)pcity;
}

void update_map_canvas_visible(void)
{
}

void refresh_overview_canvas(void)
{
}

void refresh_overview_viewrect(void)
{
}

void update_map_canvas(void)
{
}

void update_unit_info_label(void)
{
}

void popup_unit_select_dialog(void)
{
}

void popup_unit_connect_dialog(void)
{
}

void popup_caravan_dialog(void *punit)
{
  (void)punit;
}

void popup_diplomat_dialog(void *punit)
{
  (void)punit;
}

void popup_pillage_dialog(void *punit)
{
  (void)punit;
}

void popup_notify_dialog(void *msg)
{
  (void)msg;
}

void popup_notify_goto_dialog(void *msg)
{
  (void)msg;
}

void popup_government_dialog(void)
{
}

void popup_newcity_dialog(void)
{
}

void popup_incite_dialog(void)
{
}

void popup_races_dialog(void)
{
}

void popup_sabotage_dialog(void)
{
}

void popdown_races_dialog(void)
{
}

void set_overview_dimensions(int width, int height)
{
  (void)width;
  (void)height;
}

void set_turn_done_button_state(int state)
{
  (void)state;
}

void update_city_descriptions(void)
{
}

void update_info_label(void)
{
}

void update_players_dialog(void)
{
}

void update_report_dialogs(void)
{
}

void races_toggles_set_sensitive(int bits1, int bits2)
{
  (void)bits1;
  (void)bits2;
}

void real_append_output_window(const char *msg)
{
  if (msg != NULL) {
    fputs(msg, stderr);
    fputc('\n', stderr);
  }
}

void real_update_meswin_dialog(void)
{
}

bool is_meswin_open(void)
{
  return false;
}

void move_unit_map_canvas(void)
{
}

void draw_segment(int x1, int y1, int x2, int y2)
{
  (void)x1;
  (void)y1;
  (void)x2;
  (void)y2;
}

void undraw_segment(void)
{
}

bool caravan_dialog_is_open(void)
{
  return false;
}

bool diplomat_dialog_is_open(void)
{
  return false;
}

void *get_center_tile_mapcanvas(void)
{
  return NULL;
}

void *tile_visible_mapcanvas(int x, int y)
{
  (void)x;
  (void)y;
  return NULL;
}

void *city_workers_display(void)
{
  return NULL;
}

void *gfx_fileextensions(void)
{
  return NULL;
}

int isometric_view_supported(void)
{
  return 0;
}

int overhead_view_supported(void)
{
  return 1;
}

void *new_timer(void)
{
  return NULL;
}

void free_timer(void *timer)
{
  (void)timer;
}

void start_timer(void)
{
}

void put_cross_overlay_tile(int x, int y)
{
  (void)x;
  (void)y;
}

void put_nuke_mushroom_pixmaps(void)
{
}

void free_sprite(void *sprite)
{
  (void)sprite;
}

void crop_sprite(void *sprite)
{
  (void)sprite;
}

void refresh_spaceship_dialog(void)
{
}

void science_dialog_update(void)
{
}

void *city_workers_display_dialog(void)
{
  return NULL;
}

void city_report_dialog_update(void)
{
}

void overview_update_tile(int x, int y)
{
  (void)x;
  (void)y;
}

void popup_meswin_dialog(void)
{
}

bool city_dialog_is_open(void)
{
  return false;
}

void put_city_workers(void)
{
}

void decrease_unit_hp_smooth(void)
{
}

void free_intro_radar_sprites(void)
{
}

void refresh_unit_city_dialogs(void *punit)
{
  (void)punit;
}

void update_worklist_report_dialog(void)
{
}

void popup_science_dialog(void)
{
}

void popup_bribe_dialog(void)
{
}

void load_gfxfile(void)
{
}

void stop_timer(void)
{
}
