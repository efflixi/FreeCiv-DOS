/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "game.h"
#include "map.h"
#include "unit.h"
#include "support.h"
#include "civclient.h"
#include "clinet.h"
#include "control.h"
#include "options.h"
#include "offline/engine.h"
#include "gui_main.h"
#include "commands.h"
#include "event_loop.h"
#include "input.h"
#include "widgets.h"
#include "mapview.h"
#include "vbe_init.h"

enum dialog_kind { DIALOG_NONE, DIALOG_HOME, DIALOG_HELP, DIALOG_OPTIONS,
                   DIALOG_COMMANDS, DIALOG_QUIT };
enum dialog_action { ACTION_CLOSE = 1, ACTION_HELP, ACTION_OPTIONS,
                     ACTION_COMMANDS, ACTION_QUIT, ACTION_DISCARD,
                     ACTION_APPLY, ACTION_RUN };

static struct dos_event_loop event_loop;
static struct dos_vbe_framebuffer pointer_save;
static struct dos_vbe_framebuffer chrome_canvas;
static enum dialog_kind dialog;
static int initialized, failed, quit_requested, waiting, trace_events;
static int chrome_dirty = 1, pointer_visible, pointer_x, pointer_y;
static unsigned int scroll_step = 1;
static unsigned long timer_calls, packet_calls, yield_calls;
static unsigned long last_timer_ms;
static char status_text[128] = "No session. New/load-game startup is not implemented.";
static char name_edit[MAX_LEN_NAME];
static const char *const steps[] = {"1 tile", "3 tiles", "5 tiles"};
static const char *const menu_items[] = {
  "Select tile", "Next unit", "Wait unit", "Unit done", "End turn",
  "Toggle overview", "Cancel action", "Help", "Quit"
};
static const enum dos_vbe_ui_command menu_commands[] = {
  DOS_VBE_CMD_SELECT_TILE, DOS_VBE_CMD_NEXT_UNIT, DOS_VBE_CMD_WAIT_UNIT,
  DOS_VBE_CMD_DONE_UNIT, DOS_VBE_CMD_END_TURN, DOS_VBE_CMD_TOGGLE_OVERVIEW,
  DOS_VBE_CMD_CANCEL, DOS_VBE_CMD_HELP, DOS_VBE_CMD_QUIT
};
const char *client_string = "gui-dos-vbe";

static unsigned long milliseconds(void)
{
  struct timeval now;
  if (gettimeofday(&now, NULL) != 0) {
    perror("DOS GUI timer");
    failed = 1;
    return last_timer_ms;
  }
  return (unsigned long)now.tv_sec * 1000UL + (unsigned long)now.tv_usec / 1000UL;
}

static int erase_pointer(void)
{
  if (!pointer_visible) return 0;
  pointer_visible = 0;
  return dos_vbe_framebuffer_blit(&dos_vbe_front_buffer, pointer_x, pointer_y,
                                 &pointer_save, 0, 0, 10, 14, 0, 0);
}

static int draw_pointer(void)
{
  int x, y, row;
  if (!dos_input_mouse_available() || pointer_visible) return 0;
  if (dos_input_pointer(&x, &y) != 0) return -1;
  if (dos_vbe_framebuffer_blit(&pointer_save, 0, 0, &dos_vbe_front_buffer,
                             x, y, 10, 14, 0, 0) != 0) return -1;
  pointer_x = x; pointer_y = y;
  for (row = 0; row < 12; ++row) {
    int width = row < 8 ? row / 2 + 1 : 2;
    if (dos_vbe_framebuffer_fill(&dos_vbe_front_buffer, x, y + row,
                                width + 1, 1, 0) != 0
        || dos_vbe_framebuffer_fill(&dos_vbe_front_buffer, x, y + row,
                                    width, 1, 0xffffU) != 0) return -1;
  }
  pointer_visible = 1;
  return 0;
}

static int redraw(void)
{
  if (erase_pointer() != 0) return -1;
  if (map.tiles) {
    update_map_canvas_visible();
  } else if (dos_vbe_framebuffer_fill(&dos_vbe_front_buffer, 0, 0,
                                     dos_vbe_front_buffer.width,
                                     dos_vbe_front_buffer.height, 0x0841U) != 0
             || dos_vbe_framebuffer_text(&dos_vbe_front_buffer, 24, 80,
               "Freeciv 1.14.1 DOS\nPersistent interface - no game session\n"
               "New/load-game startup is reserved for Phase 8.\n"
               "F1 Help  F2 Options  F3 Commands  Q Quit",
               0xffffU, 2) != 0) {
    return -1;
  }
  chrome_dirty = 1;
  dos_widgets_invalidate();
  return 0;
}

void dos_vbe_gui_status(const char *text)
{
  size_t i;
  if (!text) {
    fprintf(stderr, "DOS GUI: missing status text.\n");
    failed = 1;
    return;
  }
  for (i = 0; text[i] && i + 1 < sizeof(status_text); ++i) {
    unsigned char c = (unsigned char)text[i];
    status_text[i] = c >= 32 && c <= 126 ? (char)c : ' ';
  }
  status_text[i] = '\0';
  chrome_dirty = 1;
  if (dos_widgets_active() && dos_widgets_feedback(status_text, NULL) != 0) failed = 1;
}

static int open_dialog(enum dialog_kind kind)
{
  struct dos_widget_spec spec[5];
  const char *title, *body;
  unsigned int count = 0;
  if (erase_pointer() != 0) return -1;
  if (dos_widgets_active()) dos_widgets_close();
  if (redraw() != 0) return -1;
  memset(spec, 0, sizeof(spec));
  if (kind == DIALOG_HOME) {
    title = "Freeciv DOS - pregame interface";
    body = "No playable session yet. New/load-game setup is Phase 8.\n"
           "Input, dialogs and clean quit are available now.";
    spec[0].label = "Help (F1)"; spec[0].action = ACTION_HELP;
    spec[1].label = "Options (F2)"; spec[1].action = ACTION_OPTIONS;
    spec[2].label = "Commands (F3)"; spec[2].action = ACTION_COMMANDS;
    spec[3].label = "Quit (Q)"; spec[3].action = ACTION_QUIT;
    count = 4;
  } else if (kind == DIALOG_HELP) {
    title = "Keyboard and mouse controls";
    body = "Arrows: cursor. Shift-arrows: scroll.\n"
           "Ctrl-arrows / WASD / 1-9: unit orders.\n"
           "Tab/5: next. .: wait. F: done.\n"
           "Enter: select. E/Space: end turn. O: overview.\n"
           "Esc: cancel. F1/F2/F3: help/options/menu.\n"
           "Q: quit (confirmation required).\n"
           "Dialog: Tab/Shift-Tab; Enter; Esc.\n"
           "Lists: arrows, Home/End, PageUp/PageDown.\n"
           "Mouse: left select/activate, right menu.\n"
           "Keyboard-only works. Audio is disabled.";
    spec[0].label = "Close"; spec[0].action = ACTION_CLOSE;
    count = 1;
  } else if (kind == DIALOG_OPTIONS) {
    title = "Interface options";
    body = "Settings apply in memory. Session startup is not implemented.\n"
           "Player name is for a future session; no current player is renamed.";
    mystrlcpy(name_edit, player_name, sizeof(name_edit));
    spec[0].type = DOS_WIDGET_TEXT; spec[0].label = "Next-session name";
    spec[0].text = name_edit; spec[0].capacity = sizeof(name_edit);
    spec[1].type = DOS_WIDGET_LIST; spec[1].label = "Viewport scroll step";
    spec[1].items = steps; spec[1].item_count = 3; spec[1].rows = 3;
    spec[1].selected = scroll_step == 1 ? 0 : scroll_step == 3 ? 1 : 2;
    spec[2].label = "Apply"; spec[2].action = ACTION_APPLY;
    spec[3].label = "Cancel"; spec[3].action = ACTION_CLOSE;
    count = 4;
  } else if (kind == DIALOG_COMMANDS) {
    title = "Commands";
    body = "Game orders require an established, owned, active player session.\n"
           "Selection/navigation never move an authoritative unit locally.";
    spec[0].type = DOS_WIDGET_LIST; spec[0].label = "Choose a command";
    spec[0].items = menu_items; spec[0].item_count = 9; spec[0].rows = 5;
    spec[0].action = ACTION_RUN;
    spec[1].label = "Run"; spec[1].action = ACTION_RUN;
    spec[2].label = "Close"; spec[2].action = ACTION_CLOSE;
    count = 3;
  } else if (kind == DIALOG_QUIT) {
    title = "Confirm quit";
    body = map.tiles || aconnection.used
      ? "Quit without saving? This session may have unsaved changes.\n"
        "Saving is not implemented yet. Cancel keeps the session open."
      : "Exit the DOS interface and restore the original text display?";
    spec[0].label = "Cancel"; spec[0].action = ACTION_CLOSE;
    spec[1].label = "Quit without saving"; spec[1].action = ACTION_DISCARD;
    count = 2;
  } else {
    fprintf(stderr, "DOS GUI: invalid dialog.\n");
    return -1;
  }
  if (dos_widgets_begin(&dos_vbe_front_buffer, title, body, spec, count) != 0) return -1;
  dialog = kind;
  return dos_widgets_feedback(status_text, NULL);
}

static int close_dialog(void)
{
  dos_widgets_close();
  dialog = DIALOG_NONE;
  if (redraw() != 0) return -1;
  return map.tiles ? 0 : open_dialog(DIALOG_HOME);
}

int dos_vbe_gui_orders_allowed(enum dos_vbe_ui_command command)
{
  struct unit *unit;
  if (get_client_state() != CLIENT_GAME_RUNNING_STATE || !game.player_ptr
      || !game.player_ptr->is_alive || !game.player_ptr->is_connected
      || game.player_ptr->ai.control
      || game.player_ptr->turn_done || turn_done_sent
      || !aconnection.used || !aconnection.established || !aconnection.local_write
      || aconnection.observer) return 0;
  if (command == DOS_VBE_CMD_END_TURN || command == DOS_VBE_CMD_SELECT_TILE
      || command == DOS_VBE_CMD_NEXT_UNIT) return 1;
  unit = get_unit_in_focus();
  return unit && unit->owner == game.player_ptr->player_no;
}

static int apply_command(enum dos_vbe_ui_command command)
{
  if (trace_events) printf("COMMAND %s\n", dos_vbe_command_name(command));
  switch (command) {
  case DOS_VBE_CMD_HELP: return open_dialog(DIALOG_HELP);
  case DOS_VBE_CMD_OPTIONS: return open_dialog(DIALOG_OPTIONS);
  case DOS_VBE_CMD_MENU: return open_dialog(DIALOG_COMMANDS);
  case DOS_VBE_CMD_QUIT: return open_dialog(DIALOG_QUIT);
  case DOS_VBE_CMD_NONE: return 0;
  case DOS_VBE_CMD_TOGGLE_OVERVIEW:
  case DOS_VBE_CMD_CANCEL:
    if (map.tiles) dos_vbe_apply_command(command);
    else dos_vbe_gui_status("No active map/action. Use F1 Help, F2 Options, F3 Commands or Q Quit.");
    return 0;
  default:
    if (!dos_vbe_gui_orders_allowed(command)) {
      dos_vbe_gui_status("Order unavailable: no established owned active-player session.");
      if (trace_events) printf("PASS rejected unavailable order\n");
      return 0;
    }
    if (event_loop.servicing || waiting) {
      return dos_vbe_defer_command(&event_loop, command);
    }
    dos_vbe_apply_command(command);
    return 0;
  }
}

static int dispatch_event(const struct dos_input_event *event, void *context)
{
  enum dos_vbe_ui_command command;
  int result, dx, dy, x, y;
  (void)context;
  if (erase_pointer() != 0) return -1;
  if (trace_events) printf("EVENT type=%d ascii=%u scan=%u mods=%u x=%d y=%d pressed=%u buttons=%u released=%u\n",
                          event->type, event->ascii, event->scan, event->modifiers,
                          event->x, event->y, event->pressed, event->buttons, event->released);
  if (quit_requested) return 0;
  command = dos_vbe_event_command(event);
  if (event->type == DOS_INPUT_COMMAND) {
    return command == DOS_VBE_CMD_NONE ? -1 : apply_command(command);
  }
  if (dialog == DIALOG_HOME
      && (command == DOS_VBE_CMD_HELP || command == DOS_VBE_CMD_OPTIONS
          || command == DOS_VBE_CMD_MENU || command == DOS_VBE_CMD_QUIT)) {
    return apply_command(command);
  }
  if (dos_widgets_active()) {
    result = dos_widgets_event(event);
    if (result == DOS_WIDGETS_ERROR) return -1;
    if (result == DOS_WIDGETS_PENDING) return 0;
    if (trace_events) printf("DIALOG kind=%d action=%d focus=%d\n",
                             dialog, result, dos_widgets_focus());
    if (result == DOS_WIDGETS_CANCEL || result == ACTION_CLOSE) {
      return dialog == DIALOG_HOME ? open_dialog(DIALOG_QUIT) : close_dialog();
    }
    if (result == ACTION_HELP) return open_dialog(DIALOG_HELP);
    if (result == ACTION_OPTIONS) return open_dialog(DIALOG_OPTIONS);
    if (result == ACTION_COMMANDS) return open_dialog(DIALOG_COMMANDS);
    if (result == ACTION_QUIT) return open_dialog(DIALOG_QUIT);
    if (result == ACTION_DISCARD) {
      quit_requested = 1;
      if (trace_events) printf("PASS confirmed quit at safe boundary\n");
      return 0;
    }
    if (result == ACTION_APPLY) {
      int selection = dos_widgets_selection(1);
      if (selection < 0 || selection > 2) return -1;
      if (!name_edit[0]) return dos_widgets_feedback(NULL, "A future player name cannot be empty.");
      mystrlcpy(player_name, name_edit, sizeof(player_name));
      scroll_step = selection == 0 ? 1 : selection == 1 ? 3 : 5;
      if (trace_events) printf("PASS options name=%s scroll=%u\n", name_edit, scroll_step);
      dos_vbe_gui_status("Interface options applied in memory; no session created.");
      return close_dialog();
    }
    if (result == ACTION_RUN) {
      int selection = dos_widgets_selection(0);
      if (selection < 0 || selection >= 9) return -1;
      command = menu_commands[selection];
      if (close_dialog() != 0) return -1;
      return apply_command(command);
    }
    fprintf(stderr, "DOS GUI: unknown dialog action %d.\n", result);
    return -1;
  }
  if (event->type == DOS_INPUT_POINTER) {
    if (event->pressed & DOS_INPUT_RIGHT) return open_dialog(DIALOG_COMMANDS);
    if (event->pressed & DOS_INPUT_LEFT) {
      if (event->y < 16) {
        if (event->x < 160) return open_dialog(DIALOG_HELP);
        if (event->x < 320) return open_dialog(DIALOG_OPTIONS);
        if (event->x < 480) return open_dialog(DIALOG_COMMANDS);
        return open_dialog(DIALOG_QUIT);
      }
      if (event->y >= 32 && dos_vbe_canvas_to_map(event->x, event->y, &x, &y)) {
        dos_vbe_select_tile(x, y);
        return apply_command(DOS_VBE_CMD_SELECT_TILE);
      }
    }
    return 0;
  }
  if (dos_vbe_event_direction(event, &dx, &dy)
      && !(event->modifiers & (DOS_INPUT_CTRL | DOS_INPUT_ALT))
      && (event->ascii == 0xe0U || event->scan == 72U || event->scan == 75U
          || event->scan == 77U || event->scan == 80U)) {
    if (event->modifiers & DOS_INPUT_SHIFT) {
      dos_vbe_scroll_map(dx * (int)scroll_step, dy * (int)scroll_step);
    } else {
      dos_vbe_move_selection(dx, dy);
    }
    if (trace_events && dos_vbe_get_selected_tile(&x, &y)) {
      struct unit *unit = get_unit_in_focus();
      printf("NAV tile=%d,%d focus=%d,%d\n", x, y,
              unit ? unit->x : -1, unit ? unit->y : -1);
    }
    return 0;
  }
  return apply_command(command);
}

static int poll_event(struct dos_input_event *event, void *context)
{
  (void)context;
  return dos_input_poll(event);
}

static int present(void *context)
{
  int result;
  unsigned int button;
  static const char *const labels[] = {"F1 Help", "F2 Options", "F3 Commands", "Q Quit"};
  (void)context;
  if (chrome_dirty) {
    if (erase_pointer() != 0
        || dos_vbe_framebuffer_fill(&chrome_canvas, 0, 0,
                                    dos_vbe_front_buffer.width, 32, 0x18c3U) != 0
        || dos_vbe_framebuffer_text(&chrome_canvas, 2, 16,
                                    status_text, 0xffe0U, 1) != 0) return -1;
    for (button = 0; button < 4U; ++button) {
      int x = (int)button * 160;
      int width = button == 3U ? (int)dos_vbe_front_buffer.width - x : 160;
      if (dos_vbe_framebuffer_fill(&chrome_canvas, x, 0, width - 1, 15,
                                   0x2945U) != 0
          || dos_vbe_framebuffer_text(&chrome_canvas, x + 4, 3,
                                      labels[button], 0xffffU, 1) != 0) return -1;
    }
    if (dos_vbe_framebuffer_blit(&dos_vbe_front_buffer, 0, 0, &chrome_canvas,
                                0, 0, chrome_canvas.width, 32, 0, 0) != 0) return -1;
    chrome_dirty = 0;
  }
  result = dos_widgets_draw(&dos_vbe_front_buffer);
  if (result < 0 || draw_pointer() != 0 || dos_vbe_present() != 0) return -1;
  return failed ? -1 : 0;
}

static int service_timer(void)
{
  unsigned long now;
  now = milliseconds();
  if ((unsigned long)(now - last_timer_ms) >= 500UL) {
    last_timer_ms = now;
    if (game.player_ptr && get_client_state() == CLIENT_GAME_RUNNING_STATE) {
      if (erase_pointer() != 0) return -1;
      real_timer_callback();
      chrome_dirty = 1;
      dos_widgets_invalidate();
    }
    timer_calls++;
  }
  /* The selected audio-none plugin needs no polling or DMA/IRQ service. */
  return failed ? -1 : 0;
}

static int service(void *context)
{
  (void)context;
  if (quit_requested) return 0;
  if (aconnection.used) {
    int processed;
    if (erase_pointer() != 0) return -1;
    processed = poll_local_game(16);
    if (processed < 0) return -1;
    packet_calls++;
    if (processed) {
      chrome_dirty = 1;
      dos_widgets_invalidate();
    }
  }
  return service_timer();
}

static void idle(void *context)
{
  (void)context;
  dos_input_idle();
}

static void engine_service(void *context)
{
  (void)context;
  yield_calls++;
  if (dos_vbe_gui_service_wait() < 0) failed = 1;
}

int dos_vbe_gui_service_wait(void)
{
  int result;
  if (!initialized || !event_loop.running) return 0;
  if (quit_requested) return 1;
  if (waiting) return 0;
  waiting = 1;
  result = service_timer();
  if (!result) result = dos_event_loop_ui_service(&event_loop);
  waiting = 0;
  return result ? result : quit_requested ? 1 : 0;
}

void dos_vbe_gui_capture_input(void)
{
  if (initialized && event_loop.running && dos_event_loop_capture(&event_loop) != 0) {
    failed = 1;
  }
}

void dos_vbe_handle_input_event(int key)
{
  struct dos_input_event event;
  memset(&event, 0, sizeof(event));
  event.type = DOS_INPUT_KEY;
  event.ascii = (unsigned int)key;
  if (key < 0 || key > 255
      || dos_event_loop_enqueue(&event_loop, &event) != 0) {
    fprintf(stderr, "DOS GUI: injected ASCII event rejected.\n");
    failed = 1;
  }
}

void ui_init(void)
{
  if (initialized) {
    fprintf(stderr, "DOS GUI: repeated initialization.\n");
    exit(EXIT_FAILURE);
  }
  failed = quit_requested = 0;
  timer_calls = packet_calls = yield_calls = 0;
  chrome_dirty = 1;
  if (vbe_init_display() != 0
      || dos_input_init(dos_vbe_front_buffer.width, dos_vbe_front_buffer.height) != 0
      || dos_vbe_framebuffer_init(&pointer_save, 10, 14, 16) != 0
      || dos_vbe_framebuffer_init(&chrome_canvas, dos_vbe_front_buffer.width, 32, 16) != 0) {
    dos_vbe_gui_shutdown();
    exit(EXIT_FAILURE);
  }
  if (dos_vbe_mapview_set_top(32) != 0) {
    dos_vbe_gui_shutdown();
    exit(EXIT_FAILURE);
  }
  initialized = 1;
}

void ui_main(int argc, char *argv[])
{
  struct dos_event_hooks hooks;
  (void)argc; (void)argv;
  if (!initialized) ui_init();
  memset(&hooks, 0, sizeof(hooks));
  hooks.poll = poll_event; hooks.dispatch = dispatch_event;
  hooks.service = service; hooks.present = present; hooks.idle = idle;
  if (dos_event_loop_init(&event_loop, &hooks) != 0) { failed = 1; return; }
  last_timer_ms = milliseconds();
  if (get_client_state() == CLIENT_BOOT_STATE && !map.tiles) {
    set_client_state(CLIENT_PRE_GAME_STATE);
  }
  fc_offline_engine_set_service(engine_service, NULL);
  if (redraw() != 0 || (!map.tiles && open_dialog(DIALOG_HOME) != 0)) failed = 1;
  while (!failed && !quit_requested && event_loop.running) {
    if (dos_event_loop_step(&event_loop) != 0) failed = 1;
  }
  dos_event_loop_stop(&event_loop);
  fc_offline_engine_set_service(NULL, NULL);
  if (trace_events) printf("LOOP ticks=%lu events=%lu dispatch=%lu idle=%lu timers=%lu packets=%lu yields=%lu\n",
                          event_loop.ticks, event_loop.received, event_loop.dispatched,
                          event_loop.idle_calls, timer_calls, packet_calls, yield_calls);
  if (trace_events && aconnection.used) {
    struct fc_offline_snapshot snapshot;
    if (fc_offline_engine_snapshot(&snapshot) != 0) failed = 1;
    else printf("BRIDGE used=%d established=%d conn=%d client=%d engine=%d map=%d request=%d\n",
                aconnection.used, aconnection.established, game.conn_id,
                get_client_state(), snapshot.established, snapshot.map_allocated,
                snapshot.last_request);
  }
  if (aconnection.used) disconnect_from_server();
}

void dos_vbe_gui_shutdown(void)
{
  fc_offline_engine_set_service(NULL, NULL);
  dos_widgets_close();
  dos_input_shutdown();
  dos_vbe_mapview_free();
  dos_vbe_framebuffer_destroy(&pointer_save);
  dos_vbe_framebuffer_destroy(&chrome_canvas);
  if (vbe_shutdown_display() != 0) failed = 1;
  initialized = pointer_visible = 0;
  dialog = DIALOG_NONE;
}

int dos_vbe_gui_result(void) { return failed ? EXIT_FAILURE : EXIT_SUCCESS; }
void dos_vbe_gui_set_trace(int enabled) { trace_events = enabled != 0; }
void sound_bell(void) { dos_vbe_gui_status("Notification (audio is disabled)."); }
void add_net_input(int sock)
{
  if (sock != -1) {
    dos_vbe_gui_status("External network input is not supported by the DOS client.");
    fprintf(stderr, "DOS GUI: external network input rejected.\n");
  }
}
void remove_net_input(void) { }
void set_unit_icon(int idx, struct unit *unit)
{
  if (idx == -1) update_unit_info_label(unit);
}
void set_unit_icons_more_arrow(bool onoff)
{
  if (onoff) dos_vbe_gui_status("More units on this tile: use focus cycling or the command menu.");
}
