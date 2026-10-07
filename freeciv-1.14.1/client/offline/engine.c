#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#if !defined(FC_LOCAL_ENGINE) || !defined(FC_NO_SOCKET_API)
#error The offline engine requires FC_LOCAL_ENGINE and FC_NO_SOCKET_API
#endif

#include <limits.h>
#include <string.h>

#include "connection.h"
#include "game.h"
#include "log.h"
#include "map.h"
#include "packets.h"
#include "plrhand.h"
#include "sernet.h"
#include "srv_main.h"
#include "support.h"

#include "engine.h"

static struct connection local_connection;
static bool active;
static bool polling;

int fc_offline_engine_open(fc_offline_write_fn write, void *context)
{
  if (active || !write) {
    freelog(LOG_ERROR, "Offline engine: invalid or repeated initialization");
    return -1;
  }
  log_init(NULL, LOG_ERROR, NULL);
  srv_local_reset();
  srv_init();
  srvarg.metaserver_no_send = TRUE;
  game_init();
  conn_list_init(&game.all_connections);
  conn_list_init(&game.est_connections);
  conn_list_init(&game.game_connections);
  server_state = PRE_GAME_STATE;
  force_end_of_sniff = FALSE;

  memset(&local_connection, 0, sizeof(local_connection));
  local_connection.sock = -1;
  local_connection.id = 1;
  local_connection.used = TRUE;
  local_connection.first_packet = TRUE;
  local_connection.ponged = TRUE;
  local_connection.access_level = ALLOW_CTRL;
  local_connection.buffer = new_socket_packet_buffer();
  local_connection.send_buffer = new_socket_packet_buffer();
  local_connection.local_write = write;
  local_connection.local_context = context;
  sz_strlcpy(local_connection.name, "offline");
  sz_strlcpy(local_connection.addr, "local");
  conn_list_init(&local_connection.self);
  conn_list_insert(&local_connection.self, &local_connection);
  conn_list_insert_back(&game.all_connections, &local_connection);
  active = TRUE;
  return 0;
}

int fc_offline_engine_feed(const unsigned char *data, int len)
{
  if (!active) {
    freelog(LOG_ERROR, "Offline engine: receive without an active session");
    return -1;
  }
  return connection_receive_data(&local_connection, data, len) ? 0 : -1;
}

int fc_offline_engine_start(void)
{
  bool started;

  if (!active || polling) {
    freelog(LOG_ERROR, "Offline engine: invalid game start");
    return -1;
  }
  polling = TRUE;
  started = srv_local_start_game();
  polling = FALSE;
  return started ? 0 : -1;
}

static int poll_packets(unsigned int packet_budget)
{
  unsigned int processed = 0;

  if (!active || packet_budget == 0 || packet_budget > INT_MAX) {
    freelog(LOG_ERROR, "Offline engine: invalid poll");
    return -1;
  }
  if (local_connection.delayed_disconnect) {
    freelog(LOG_ERROR, "Offline engine: local transport failed");
    return -1;
  }
  while (processed < packet_budget && local_connection.buffer->ndata >= 3) {
    const unsigned char *bytes = local_connection.buffer->data;
    int size = (bytes[0] << 8) | bytes[1];
    enum packet_type type;
    bool available;
    void *packet;

    if (size < 3 || size > MAX_LEN_PACKET || bytes[2] >= PACKET_LAST) {
      freelog(LOG_ERROR, "Offline engine: invalid packet header");
      local_connection.delayed_disconnect = TRUE;
      return -1;
    }
    if (size > local_connection.buffer->ndata) {
      break;
    }
    packet = get_packet_from_connection(&local_connection, &type, &available);
    if (!available) {
      freelog(LOG_ERROR, "Offline engine: complete packet did not decode");
      local_connection.delayed_disconnect = TRUE;
      return -1;
    }
    if (!server_process_local_packet(&local_connection, packet, type)) {
      freelog(LOG_ERROR, "Offline engine: request rejected; session must close");
      local_connection.delayed_disconnect = TRUE;
      return -1;
    }
    processed++;
    if (local_connection.delayed_disconnect) {
      freelog(LOG_ERROR, "Offline engine: failed to deliver response");
      return -1;
    }
  }
  if (srv_local_step() < 0) {
    return -1;
  }
  flush_connection_send_buffer_all(&local_connection);
  return local_connection.delayed_disconnect ? -1 : (int)processed;
}

int fc_offline_engine_poll(unsigned int packet_budget)
{
  int processed;

  if (polling) {
    freelog(LOG_ERROR, "Offline engine: recursive poll is not allowed");
    return -1;
  }
  polling = TRUE;
  processed = poll_packets(packet_budget);
  polling = FALSE;
  return processed;
}

int fc_offline_engine_snapshot(struct fc_offline_snapshot *snapshot)
{
  if (!active || !snapshot) {
    freelog(LOG_ERROR, "Offline engine: invalid snapshot request");
    return -1;
  }
  snapshot->players = game.nplayers;
  snapshot->turn = game.turn;
  snapshot->year = game.year;
  snapshot->established = local_connection.established;
  snapshot->last_request = local_connection.server.last_request_id_seen;
  snapshot->map_allocated = map.tiles != NULL;
  snapshot->state = server_state;
  snapshot->ai_players = 0;
  players_iterate(player) {
    if (player->ai.control) {
      snapshot->ai_players++;
    }
  } players_iterate_end;
  return 0;
}

void fc_offline_engine_close(void)
{
  if (polling) {
    freelog(LOG_ERROR, "Offline engine: cannot close during packet processing");
    return;
  }
  if (active) {
    if (local_connection.player) {
      unassociate_player_connection(local_connection.player, &local_connection);
    }
    close_connection(&local_connection);
    conn_list_unlink_all(&local_connection.self);
    srv_local_reset();
    server_game_free();
    conn_list_unlink_all(&game.all_connections);
    conn_list_unlink_all(&game.est_connections);
    conn_list_unlink_all(&game.game_connections);
    memset(&local_connection, 0, sizeof(local_connection));
    active = FALSE;
  }
}
