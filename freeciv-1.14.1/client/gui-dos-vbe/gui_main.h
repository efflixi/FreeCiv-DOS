/* gui_main.h -- PLACEHOLDER */
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
  DOS_VBE_CMD_CANCEL = 8
};

const char *dos_vbe_command_name(enum dos_vbe_ui_command cmd);
enum dos_vbe_ui_command dos_vbe_key_to_command(int key);
void dos_vbe_handle_input_event(int key);
#endif  /* FC__GUI_MAIN_H */
