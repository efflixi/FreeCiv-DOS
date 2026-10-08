/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef FC__GUI_MAIN_H
#define FC__GUI_MAIN_H

#include "gui_main_g.h"

enum dos_vbe_ui_command {
  DOS_VBE_CMD_NONE = 0,
  DOS_VBE_CMD_MOVE_NORTH = 1,
  DOS_VBE_CMD_MOVE_SOUTH = 2,
  DOS_VBE_CMD_MOVE_EAST = 3,
  DOS_VBE_CMD_MOVE_WEST = 4,
  DOS_VBE_CMD_SELECT_TILE = 5,
  DOS_VBE_CMD_END_TURN = 6,
  DOS_VBE_CMD_TOGGLE_OVERVIEW = 7,
  DOS_VBE_CMD_CANCEL = 8,
  DOS_VBE_CMD_MOVE_NORTH_EAST,
  DOS_VBE_CMD_MOVE_SOUTH_EAST,
  DOS_VBE_CMD_MOVE_SOUTH_WEST,
  DOS_VBE_CMD_MOVE_NORTH_WEST,
  DOS_VBE_CMD_NEXT_UNIT,
  DOS_VBE_CMD_WAIT_UNIT,
  DOS_VBE_CMD_DONE_UNIT,
  DOS_VBE_CMD_MENU,
  DOS_VBE_CMD_HELP,
  DOS_VBE_CMD_OPTIONS,
  DOS_VBE_CMD_QUIT
};

const char *dos_vbe_command_name(enum dos_vbe_ui_command cmd);
enum dos_vbe_ui_command dos_vbe_key_to_command(int key);
void dos_vbe_handle_input_event(int key);
/* UI-only cooperative service: 0 continue, 1 confirmed quit, -1 failure.
 * Never pump/close the engine recursively from an engine callback. */
int dos_vbe_gui_service_wait(void);
void dos_vbe_gui_status(const char *text);
int dos_vbe_gui_result(void);
void dos_vbe_gui_shutdown(void);
void dos_vbe_gui_set_trace(int enabled);
void dos_vbe_gui_capture_input(void);
int dos_vbe_gui_orders_allowed(enum dos_vbe_ui_command command);
#endif  /* FC__GUI_MAIN_H */
