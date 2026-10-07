/* gui_main.c -- DOS VESA client scaffold */

#include <stdio.h>
#include <stdlib.h>

#include "gui_main.h"
#include "extender_compat.h"
#include "mapview.h"
#include "vbe_init.h"

#define DOS_VBE_COMMAND_QUEUE_SIZE 32U

static unsigned int dos_vbe_command_head = 0U;
static unsigned int dos_vbe_command_tail = 0U;
static enum dos_vbe_ui_command dos_vbe_command_queue[DOS_VBE_COMMAND_QUEUE_SIZE];

const char *client_string = "gui-dos-vbe";

static void dos_vbe_require_runtime(void)
{
  /* Later phases must replace this gate with real runtime readiness checks. */
  fprintf(stderr,
          "DOS VBE client unavailable: production graphics resources, "
          "input handling and GUI integration are incomplete. "
          "The separately tested VBE display is not a playable client.\n");
  exit(EXIT_FAILURE);
}

static void dos_vbe_enqueue_command(enum dos_vbe_ui_command cmd)
{
  if (cmd == DOS_VBE_CMD_NONE) {
    return;
  }

  if (((dos_vbe_command_tail + 1U) % DOS_VBE_COMMAND_QUEUE_SIZE)
      == dos_vbe_command_head) {
    dos_vbe_command_head = (dos_vbe_command_head + 1U) % DOS_VBE_COMMAND_QUEUE_SIZE;
  }

  dos_vbe_command_queue[dos_vbe_command_tail] = cmd;
  dos_vbe_command_tail = (dos_vbe_command_tail + 1U) % DOS_VBE_COMMAND_QUEUE_SIZE;
}

static bool dos_vbe_dequeue_command(enum dos_vbe_ui_command *cmd)
{
  if (dos_vbe_command_head == dos_vbe_command_tail) {
    return FALSE;
  }

  if (cmd != NULL) {
    *cmd = dos_vbe_command_queue[dos_vbe_command_head];
  }
  dos_vbe_command_head = (dos_vbe_command_head + 1U) % DOS_VBE_COMMAND_QUEUE_SIZE;
  return TRUE;
}

static void dos_vbe_queue_reset(void)
{
  dos_vbe_command_head = 0U;
  dos_vbe_command_tail = 0U;
}

const char *dos_vbe_command_name(enum dos_vbe_ui_command cmd)
{
  switch (cmd) {
    case DOS_VBE_CMD_MOVE_NORTH: return "MOVE_NORTH";
    case DOS_VBE_CMD_MOVE_SOUTH: return "MOVE_SOUTH";
    case DOS_VBE_CMD_MOVE_EAST: return "MOVE_EAST";
    case DOS_VBE_CMD_MOVE_WEST: return "MOVE_WEST";
    case DOS_VBE_CMD_SELECT_TILE: return "SELECT_TILE";
    case DOS_VBE_CMD_END_TURN: return "END_TURN";
    case DOS_VBE_CMD_TOGGLE_OVERVIEW: return "TOGGLE_OVERVIEW";
    case DOS_VBE_CMD_CANCEL: return "CANCEL";
    default: return "NONE";
  }
}

enum dos_vbe_ui_command dos_vbe_key_to_command(int key)
{
  switch (key) {
    case 'w':
    case 'W':
    case 72: return DOS_VBE_CMD_MOVE_NORTH;
    case 's':
    case 'S':
    case 80: return DOS_VBE_CMD_MOVE_SOUTH;
    case 'd':
    case 'D':
    case 77: return DOS_VBE_CMD_MOVE_EAST;
    case 'a':
    case 'A':
    case 75: return DOS_VBE_CMD_MOVE_WEST;
    case 13:
    case ' ':
      return DOS_VBE_CMD_SELECT_TILE;
    case 'e':
    case 'E':
      return DOS_VBE_CMD_END_TURN;
    case 'o':
    case 'O':
    case 9: return DOS_VBE_CMD_TOGGLE_OVERVIEW;
    case 'q':
    case 'Q':
    case 27: return DOS_VBE_CMD_CANCEL;
    default: return DOS_VBE_CMD_NONE;
  }
}

void dos_vbe_handle_input_event(int key)
{
  enum dos_vbe_ui_command cmd = dos_vbe_key_to_command(key);

  if (cmd != DOS_VBE_CMD_NONE) {
    dos_vbe_enqueue_command(cmd);
  }
}

static void dos_vbe_queue_turn_sequence(void)
{
  static const enum dos_vbe_ui_command sequence[] = {
    DOS_VBE_CMD_MOVE_EAST,
    DOS_VBE_CMD_MOVE_SOUTH,
    DOS_VBE_CMD_SELECT_TILE,
    DOS_VBE_CMD_TOGGLE_OVERVIEW,
    DOS_VBE_CMD_MOVE_WEST,
    DOS_VBE_CMD_MOVE_NORTH,
    DOS_VBE_CMD_END_TURN,
    DOS_VBE_CMD_CANCEL
  };
  size_t i;

  for (i = 0U; i < sizeof(sequence) / sizeof(sequence[0]); ++i) {
    dos_vbe_enqueue_command(sequence[i]);
  }
}

static void dos_vbe_process_session_queue(void)
{
  enum dos_vbe_ui_command cmd = DOS_VBE_CMD_NONE;

  while (dos_vbe_dequeue_command(&cmd)) {
    dos_vbe_apply_command(cmd);
    update_map_canvas_visible();
    fprintf(stderr, "Freeciv DOS VBE session: processed input %s.\n",
            dos_vbe_command_name(cmd));
  }
}

void ui_init(void)
{
	dos_vbe_require_runtime();
	dos_vbe_queue_reset();
	if (vbe_init_display() != 0) {
	  exit(EXIT_FAILURE);
	}
}

void ui_main(int argc, char *argv[])
{
	/* Keep the scripted scaffold dormant, even if called without ui_init(). */
	(void)argc;
	(void)argv;

	dos_vbe_require_runtime();
	dos_vbe_queue_turn_sequence();
	dos_vbe_process_session_queue();
	update_map_canvas_visible();
	fprintf(stderr, "DOS VBE scripted scaffold completed; not a playable client.\n");
}

void sound_bell(void)
{
	/* PORTME */
}

void add_net_input(int sock)
{
	/* PORTME */
}

void remove_net_input(void)
{
	/* PORTME */
}

void set_unit_icon(int idx, struct unit *punit)
{
	/* PORTME */
}

void set_unit_icons_more_arrow(bool onoff)
{
	/* PORTME */
}
