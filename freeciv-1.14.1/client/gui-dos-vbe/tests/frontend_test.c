/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "game.h"
#include "connection.h"
#include "unit.h"
#include "civclient.h"
#include "control.h"
#include "clinet.h"
#include "gui_main.h"

struct civ_game game;
struct connection aconnection;
bool turn_done_sent;
static enum client_states state;
static struct unit focused;
static struct unit *focus;

enum client_states get_client_state(void) { return state; }
struct unit *get_unit_in_focus(void) { return focus; }
static int send_bytes(void *context, const unsigned char *data, int length)
{
  (void)context; (void)data;
  return length;
}

int main(void)
{
  struct player player;
  memset(&player, 0, sizeof(player));
  memset(&game, 0, sizeof(game));
  memset(&aconnection, 0, sizeof(aconnection));
  state = CLIENT_PRE_GAME_STATE;
  assert(!dos_vbe_gui_orders_allowed(DOS_VBE_CMD_END_TURN));
  state = CLIENT_GAME_RUNNING_STATE;
  assert(!dos_vbe_gui_orders_allowed(DOS_VBE_CMD_END_TURN));
  game.player_ptr = &player;
  player.player_no = 1; player.is_alive = player.is_connected = TRUE;
  aconnection.used = aconnection.established = TRUE;
  aconnection.local_write = send_bytes;
  assert(dos_vbe_gui_orders_allowed(DOS_VBE_CMD_END_TURN));
  assert(dos_vbe_gui_orders_allowed(DOS_VBE_CMD_NEXT_UNIT));
  assert(dos_vbe_gui_orders_allowed(DOS_VBE_CMD_SELECT_TILE));
  assert(!dos_vbe_gui_orders_allowed(DOS_VBE_CMD_MOVE_EAST));
  focus = &focused; focused.owner = 0;
  assert(!dos_vbe_gui_orders_allowed(DOS_VBE_CMD_MOVE_EAST));
  focused.owner = 1;
  assert(dos_vbe_gui_orders_allowed(DOS_VBE_CMD_MOVE_EAST));
  player.turn_done = TRUE;
  assert(!dos_vbe_gui_orders_allowed(DOS_VBE_CMD_MOVE_EAST));
  player.turn_done = FALSE; turn_done_sent = TRUE;
  assert(!dos_vbe_gui_orders_allowed(DOS_VBE_CMD_END_TURN));
  turn_done_sent = FALSE; aconnection.observer = TRUE;
  assert(!dos_vbe_gui_orders_allowed(DOS_VBE_CMD_SELECT_TILE));
  aconnection.observer = FALSE; aconnection.local_write = NULL;
  assert(!dos_vbe_gui_orders_allowed(DOS_VBE_CMD_MOVE_EAST));
  aconnection.local_write = send_bytes; player.ai.control = TRUE;
  assert(!dos_vbe_gui_orders_allowed(DOS_VBE_CMD_MOVE_EAST));
  player.ai.control = FALSE; aconnection.established = FALSE;
  assert(!dos_vbe_gui_orders_allowed(DOS_VBE_CMD_END_TURN));
  aconnection.established = TRUE; player.is_alive = FALSE;
  assert(!dos_vbe_gui_orders_allowed(DOS_VBE_CMD_END_TURN));
  player.is_alive = TRUE; player.is_connected = FALSE;
  assert(!dos_vbe_gui_orders_allowed(DOS_VBE_CMD_MOVE_EAST));
  player.is_connected = TRUE; state = CLIENT_WAITING_FOR_GAME_START_STATE;
  assert(!dos_vbe_gui_orders_allowed(DOS_VBE_CMD_MOVE_EAST));
  puts("PASS genuine frontend state/connection/observer/AI/turn/ownership order guards");
  return 0;
}
