/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <stdio.h>
#include <string.h>
#include "gui_main.h"
#include "input.h"
#include "commands.h"
#include "event_loop.h"

const char *dos_vbe_command_name(enum dos_vbe_ui_command command)
{
  static const char *names[] = {
    "NONE", "MOVE_NORTH", "MOVE_SOUTH", "MOVE_EAST", "MOVE_WEST",
    "SELECT_TILE", "END_TURN", "TOGGLE_OVERVIEW", "CANCEL",
    "MOVE_NORTH_EAST", "MOVE_SOUTH_EAST", "MOVE_SOUTH_WEST",
    "MOVE_NORTH_WEST", "NEXT_UNIT", "WAIT_UNIT", "DONE_UNIT",
    "MENU", "HELP", "OPTIONS", "QUIT"
  };
  if (command < DOS_VBE_CMD_NONE || command > DOS_VBE_CMD_QUIT) return "INVALID";
  return names[command];
}

enum dos_vbe_ui_command dos_vbe_key_to_command(int key)
{
  switch (key) {
  case '8': case 'w': case 'W': return DOS_VBE_CMD_MOVE_NORTH;
  case '2': case 's': case 'S': return DOS_VBE_CMD_MOVE_SOUTH;
  case '6': case 'd': case 'D': return DOS_VBE_CMD_MOVE_EAST;
  case '4': case 'a': case 'A': return DOS_VBE_CMD_MOVE_WEST;
  case '9': return DOS_VBE_CMD_MOVE_NORTH_EAST;
  case '3': return DOS_VBE_CMD_MOVE_SOUTH_EAST;
  case '1': return DOS_VBE_CMD_MOVE_SOUTH_WEST;
  case '7': return DOS_VBE_CMD_MOVE_NORTH_WEST;
  case '\r': return DOS_VBE_CMD_SELECT_TILE;
  case ' ': case 'e': case 'E': return DOS_VBE_CMD_END_TURN;
  case '\t': case '5': return DOS_VBE_CMD_NEXT_UNIT;
  case '.': return DOS_VBE_CMD_WAIT_UNIT;
  case 'f': case 'F': return DOS_VBE_CMD_DONE_UNIT;
  case 'o': case 'O': return DOS_VBE_CMD_TOGGLE_OVERVIEW;
  case 27: return DOS_VBE_CMD_CANCEL;
  case 'm': case 'M': return DOS_VBE_CMD_MENU;
  case 'h': case 'H': case '?': return DOS_VBE_CMD_HELP;
  case 'p': case 'P': return DOS_VBE_CMD_OPTIONS;
  case 'q': case 'Q': return DOS_VBE_CMD_QUIT;
  default: return DOS_VBE_CMD_NONE;
  }
}

int dos_vbe_event_direction(const struct dos_input_event *event, int *dx, int *dy)
{
  if (!event || !dx || !dy || event->type != DOS_INPUT_KEY) return 0;
  *dx = *dy = 0;
  /* Some BIOSes return keypad digits for Shift-arrow combinations. */
  if (event->ascii && event->ascii != 0xe0U
      && !((event->modifiers & DOS_INPUT_SHIFT)
           && ((event->scan == 72U && event->ascii == '8')
               || (event->scan == 75U && event->ascii == '4')
               || (event->scan == 77U && event->ascii == '6')
               || (event->scan == 80U && event->ascii == '2')))) return 0;
  switch (event->scan) {
  case 71: case 119: *dx = -1; *dy = -1; break;
  case 72: case 141: *dy = -1; break;
  case 73: case 132: *dx = 1; *dy = -1; break;
  case 75: case 115: *dx = -1; break;
  case 77: case 116: *dx = 1; break;
  case 79: case 117: *dx = -1; *dy = 1; break;
  case 80: case 145: *dy = 1; break;
  case 81: case 118: *dx = 1; *dy = 1; break;
  default: return 0;
  }
  return 1;
}

enum dos_vbe_ui_command dos_vbe_event_command(const struct dos_input_event *event)
{
  int dx, dy;
  if (event && event->type == DOS_INPUT_COMMAND) {
    if (event->command >= DOS_VBE_CMD_MOVE_NORTH
        && event->command <= DOS_VBE_CMD_DONE_UNIT
        && event->command != DOS_VBE_CMD_CANCEL
        && event->command != DOS_VBE_CMD_TOGGLE_OVERVIEW) return event->command;
    fprintf(stderr, "DOS commands: invalid deferred gameplay command.\n");
    return DOS_VBE_CMD_NONE;
  }
  if (!event || event->type != DOS_INPUT_KEY) return DOS_VBE_CMD_NONE;
  if (event->modifiers & DOS_INPUT_ALT) {
    return event->scan == 0x3eU || event->scan == 0x6bU
           ? DOS_VBE_CMD_QUIT : DOS_VBE_CMD_NONE;
  }
  if (dos_vbe_event_direction(event, &dx, &dy)) {
    if (dy < 0) return dx < 0 ? DOS_VBE_CMD_MOVE_NORTH_WEST
                        : dx > 0 ? DOS_VBE_CMD_MOVE_NORTH_EAST : DOS_VBE_CMD_MOVE_NORTH;
    if (dy > 0) return dx < 0 ? DOS_VBE_CMD_MOVE_SOUTH_WEST
                        : dx > 0 ? DOS_VBE_CMD_MOVE_SOUTH_EAST : DOS_VBE_CMD_MOVE_SOUTH;
    return dx < 0 ? DOS_VBE_CMD_MOVE_WEST : DOS_VBE_CMD_MOVE_EAST;
  }
  if (!event->ascii || event->ascii == 0xe0U) {
    if (event->scan == 0x3bU) return DOS_VBE_CMD_HELP;
    if (event->scan == 0x3cU) return DOS_VBE_CMD_OPTIONS;
    if (event->scan == 0x3dU) return DOS_VBE_CMD_MENU;
    if (event->scan == 0x4cU) return DOS_VBE_CMD_NEXT_UNIT;
    return DOS_VBE_CMD_NONE;
  }
  if (event->modifiers & DOS_INPUT_CTRL) {
    return event->ascii == 17U ? DOS_VBE_CMD_QUIT : DOS_VBE_CMD_NONE;
  }
  return dos_vbe_key_to_command((int)event->ascii);
}

int dos_vbe_defer_command(struct dos_event_loop *loop, enum dos_vbe_ui_command command)
{
  struct dos_input_event event;
  memset(&event, 0, sizeof(event));
  event.type = DOS_INPUT_COMMAND;
  event.command = command;
  if (dos_vbe_event_command(&event) == DOS_VBE_CMD_NONE) return -1;
  return dos_event_loop_enqueue(loop, &event);
}
