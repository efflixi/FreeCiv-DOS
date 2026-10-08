#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "capstr.h"
#include "connection.h"
#include "game.h"
#include "log.h"
#include "map.h"
#include "packets.h"
#include "support.h"
#include "version.h"

#include "engine.h"
#include "local_queue.h"
#include "session.h"
#include "event_loop.h"
#include "widgets.h"

bool is_server = FALSE;
static unsigned long ui_service_calls;
static int test_service_reentry;
static struct dos_event_loop test_ui_loop;
static struct dos_vbe_framebuffer test_ui_buffer;
static char test_text[16];
static unsigned int ui_events, ui_activations;
static int ui_allow, ui_take;

static int ui_poll(struct dos_input_event *event, void *context)
{
  static const unsigned int keys[] = {'A', 'I', '\t', '\r'};
  (void)context;
  if (!ui_allow || !ui_take || ui_events >= sizeof(keys) / sizeof(keys[0])) return 0;
  ui_take = 0;
  memset(event, 0, sizeof(*event));
  event->ascii = keys[ui_events++];
  return 1;
}

static int ui_dispatch(const struct dos_input_event *event, void *context)
{
  int result;
  (void)context;
  result = dos_widgets_event(event);
  assert(result >= 0);
  if (result == 1) ui_activations++;
  return 0;
}

static int ui_present(void *context)
{
  (void)context;
  assert(dos_widgets_draw(&test_ui_buffer) >= 0);
  return dos_vbe_framebuffer_dirty_clear(&test_ui_buffer);
}
static int ui_no_service(void *context) { (void)context; return 0; }
static void ui_idle(void *context) { (void)context; }

static void service_ui(void *context)
{
  assert(context == &ui_service_calls);
  ui_service_calls++;
  assert(!is_server);
  ui_take = 1;
  assert(dos_event_loop_ui_service(&test_ui_loop) == 0);
  if (test_service_reentry) {
    test_service_reentry = 0;
    assert(fc_offline_engine_poll(1) == -1);
  }
}

void dealloc_id(int id)
{
  (void)id;
  assert(!is_server);
}

void send_unit_info(struct player *owner, struct unit *unit)
{
  (void)owner;
  (void)unit;
  fputs("Unexpected server-only notification in client test context.\n",
        stderr);
  abort();
}

static void init_client(struct connection *pc, struct local_queue *out)
{
  memset(pc, 0, sizeof(*pc));
  pc->sock = -1;
  pc->used = TRUE;
  pc->first_packet = TRUE;
  pc->buffer = new_socket_packet_buffer();
  pc->send_buffer = new_socket_packet_buffer();
  pc->local_write = local_queue_write;
  pc->local_context = out;
}

static void free_client(struct connection *pc)
{
  free_socket_packet_buffer(pc->buffer);
  free_socket_packet_buffer(pc->send_buffer);
  memset(pc, 0, sizeof(*pc));
}

static void join_with_capability(struct connection *pc, const char *capability)
{
  struct packet_req_join_game request;

  memset(&request, 0, sizeof(request));
  sz_strlcpy(request.name, "DOSPlayer");
  sz_strlcpy(request.short_name, "DOSPlayer");
  sz_strlcpy(request.capability, capability);
  request.major_version = MAJOR_VERSION;
  request.minor_version = MINOR_VERSION;
  request.patch_version = PATCH_VERSION;
  sz_strlcpy(request.version_label, VERSION_LABEL);
  assert(send_packet_req_join_game(pc, &request) == 1);
}

static void join(struct connection *pc)
{
  join_with_capability(pc, our_capability);
}

static void test_queue(void)
{
  struct local_queue queue = {0};
  unsigned char data[8];
  const unsigned char input[] = {1, 2, 3, 4, 5, 6};

  assert(local_queue_init(&queue, 8) == 0);
  assert(local_queue_write(&queue, input, 6) == 6);
  assert(local_queue_write(&queue, input, 6) == 0);
  assert(queue.count == 6);
  assert(local_queue_read(&queue, data, 4) == 4);
  assert(memcmp(data, input, 4) == 0);
  assert(local_queue_write(&queue, input, 6) == 6);
  assert(local_queue_read(&queue, data, sizeof(data)) == 8);
  assert(data[0] == 5 && data[1] == 6);
  assert(memcmp(data + 2, input, 6) == 0);
  assert(queue.count == 0);
  local_queue_free(&queue);
  local_queue_free(&queue);
}

static void test_session(void)
{
  struct local_queue outgoing = {0}, incoming = {0};
  struct connection client;
  struct fc_offline_snapshot snapshot;
  unsigned char bytes[MAX_LEN_PACKET];
  size_t count;
  bool available;
  enum packet_type type;
  void *packet;
  int starts = 0, finishes = 0, replies = 0;
  struct tile *client_tiles = map.tiles;

  assert(local_queue_init(&outgoing, MAX_LEN_BUFFER) == 0);
  assert(local_queue_init(&incoming, MAX_LEN_BUFFER) == 0);
  init_client(&client, &outgoing);
  assert(fc_offline_engine_open(local_queue_write, &incoming) == 0);
  assert(fc_offline_engine_open(local_queue_write, &incoming) == -1);
  assert(fc_offline_engine_snapshot(&snapshot) == 0);
  assert(snapshot.players == 0 && !snapshot.established);
  assert(!snapshot.map_allocated && map.tiles == client_tiles);
  assert(fc_offline_engine_poll(0) == -1);
  assert(fc_offline_engine_poll(UINT_MAX) == -1);

  connection_do_buffer(&client);
  join(&client);
  assert(send_packet_generic_empty(&client, PACKET_CONN_PONG) == 2);
  assert(outgoing.count == 0);
  connection_do_unbuffer(&client);
  count = local_queue_read(&outgoing, bytes, sizeof(bytes));
  assert(count > 3);
  assert(fc_offline_engine_feed(bytes, 2) == 0);
  assert(fc_offline_engine_poll(1) == 0);
  assert(fc_offline_engine_feed(bytes + 2, (int)count - 2) == 0);
  assert(fc_offline_engine_poll(1) == 1);
  assert(fc_offline_engine_snapshot(&snapshot) == 0);
  assert(snapshot.players == 1 && snapshot.established);
  assert(snapshot.last_request == 1);
  assert(fc_offline_engine_poll(1) == 1);
  assert(fc_offline_engine_snapshot(&snapshot) == 0);
  assert(snapshot.last_request == 2);
  assert(game.nplayers == 0);
  assert(is_server == FALSE);

  while ((count = local_queue_read(&incoming, bytes, sizeof(bytes))) != 0) {
    assert(connection_receive_data(&client, bytes, (int)count));
  }
  for (;;) {
    packet = get_packet_from_connection(&client, &type, &available);
    if (!available) {
      break;
    }
    if (type == PACKET_PROCESSING_STARTED) {
      assert(starts == finishes);
      starts++;
    } else if (type == PACKET_JOIN_GAME_REPLY) {
      struct packet_join_game_reply *reply = packet;
      assert(reply && reply->you_can_join && starts == 1);
      sz_strlcpy(client.capability, reply->capability);
      replies++;
    } else if (type == PACKET_PROCESSING_FINISHED) {
      assert(starts == finishes + 1 && replies == 1);
      finishes++;
    }
    free(packet);
  }
  assert(starts == 2 && finishes == 2 && replies == 1);
  assert(client.buffer->ndata == 0);
  game.year = 1234;
  assert(fc_offline_engine_snapshot(&snapshot) == 0);
  assert(snapshot.year != game.year);
  fc_offline_engine_close();
  fc_offline_engine_close();
  assert(game.year == 1234 && game.nplayers == 0);
  assert(map.tiles == client_tiles);
  assert(fc_offline_engine_open(local_queue_write, &incoming) == 0);
  assert(fc_offline_engine_snapshot(&snapshot) == 0);
  assert(snapshot.players == 0 && snapshot.last_request == 0);
  fc_offline_engine_close();

  free_client(&client);
  local_queue_free(&outgoing);
  local_queue_free(&incoming);
}

static void test_backpressure(void)
{
  struct local_queue queue = {0};
  struct connection client;
  unsigned char bytes[MAX_LEN_PACKET];
  size_t count;

  assert(local_queue_init(&queue, MAX_LEN_PACKET) == 0);
  init_client(&client, &queue);
  memset(bytes, 0xAA, sizeof(bytes));
  assert(local_queue_write(&queue, bytes, sizeof(bytes)) == sizeof(bytes));
  join(&client);
  assert(client.send_buffer->ndata > 0);
  assert(!client.delayed_disconnect);
  assert(local_queue_read(&queue, bytes, sizeof(bytes)) == sizeof(bytes));
  flush_connection_send_buffer_all(&client);
  assert(client.send_buffer->ndata == 0);
  count = local_queue_read(&queue, bytes, sizeof(bytes));
  assert(count > 3 && bytes[2] == PACKET_REQUEST_JOIN_GAME);
  free_client(&client);
  local_queue_free(&queue);
}

static void test_invalid_frame(void)
{
  struct local_queue queue = {0};
  const unsigned char invalid[][3] = {
    {0, 0, PACKET_REQUEST_JOIN_GAME},
    {(MAX_LEN_PACKET + 1) >> 8, (MAX_LEN_PACKET + 1) & 255,
     PACKET_REQUEST_JOIN_GAME},
    {0, 3, PACKET_LAST}
  };
  unsigned int frame;

  assert(local_queue_init(&queue, MAX_LEN_PACKET) == 0);
  for (frame = 0; frame < sizeof(invalid) / sizeof(invalid[0]); frame++) {
    assert(fc_offline_engine_open(local_queue_write, &queue) == 0);
    assert(fc_offline_engine_feed(invalid[frame], sizeof(invalid[frame])) == 0);
    assert(fc_offline_engine_poll(1) == -1);
    assert(fc_offline_engine_poll(1) == -1);
    fc_offline_engine_close();
  }
  local_queue_free(&queue);
}

static int invalid_write(void *context, const unsigned char *data, int len)
{
  (void)data;
  return context ? len + 1 : -1;
}

static void test_transport_failure(void)
{
  struct connection client;
  unsigned char bytes[3] = {0};
  int mode;

  for (mode = 0; mode < 3; mode++) {
    init_client(&client, NULL);
    client.local_write = mode == 0 ? NULL : invalid_write;
    client.local_context = mode == 2 ? &mode : NULL;
    assert(!connection_receive_data(&client, bytes, MAX_LEN_BUFFER + 1));
    join(&client);
    assert(client.delayed_disconnect);
    free_client(&client);
  }
}

static int recursive_write(void *context, const unsigned char *data, int len)
{
  assert(fc_offline_engine_poll(1) == -1);
  fc_offline_engine_close();
  return local_queue_write(context, data, len);
}

static void test_recursive_processing(void)
{
  struct local_queue outgoing = {0}, incoming = {0};
  struct connection client;
  struct fc_offline_snapshot snapshot;
  unsigned char bytes[MAX_LEN_PACKET];
  size_t count;

  assert(local_queue_init(&outgoing, MAX_LEN_BUFFER) == 0);
  assert(local_queue_init(&incoming, MAX_LEN_BUFFER) == 0);
  init_client(&client, &outgoing);
  assert(fc_offline_engine_open(recursive_write, &incoming) == 0);
  join(&client);
  count = local_queue_read(&outgoing, bytes, sizeof(bytes));
  assert(count > 3);
  assert(fc_offline_engine_feed(bytes, (int)count) == 0);
  assert(fc_offline_engine_poll(1) == 1);
  assert(incoming.count > 0);
  assert(fc_offline_engine_snapshot(&snapshot) == 0);
  assert(snapshot.established && snapshot.last_request == 1);
  fc_offline_engine_close();
  free_client(&client);
  local_queue_free(&outgoing);
  local_queue_free(&incoming);
}

static void test_rejection_delivery(void)
{
  struct offline_session session = {0};
  struct connection client;
  int received, replies = 0;
  unsigned int iterations = 0;

  assert(local_queue_init(&session.requests, MAX_LEN_BUFFER) == 0);
  assert(local_queue_init(&session.responses, MAX_LEN_BUFFER) == 0);
  init_client(&client, &session.requests);
  session.client = &client;
  assert(fc_offline_engine_open(local_queue_write, &session.responses) == 0);
  join_with_capability(&client, "unsupported-capability");
  do {
    enum packet_type type;
    bool available;
    void *packet;

    received = offline_session_pump(&session, 1);
    assert(received > 0 || session.failed);
    for (;;) {
      packet = get_packet_from_connection(&client, &type, &available);
      if (!available) {
        break;
      }
      if (type == PACKET_JOIN_GAME_REPLY) {
        assert(!((struct packet_join_game_reply *)packet)->you_can_join);
        replies++;
      }
      free(packet);
    }
    assert(++iterations < 100);
  } while (received >= 0);
  assert(replies == 1 && client.buffer->ndata == 0);
  offline_session_close(&session);
}

static void test_session_bridge(void)
{
  struct offline_session session = {0};
  struct connection client;
  struct fc_offline_snapshot snapshot;
  int received = 0, joins = 0;
  unsigned int iterations = 0;

  memset(&client, 0, sizeof(client));
  assert(offline_session_open(&session, &client, "DOSPlayer") == 0);
  assert(offline_session_pump(&session, 0) == -1);
  assert(offline_session_pump(&session, UINT_MAX) == -1);
  do {
    enum packet_type type;
    bool available;
    void *packet;

    received = offline_session_pump(&session, 1);
    assert(received >= 0);
    for (;;) {
      packet = get_packet_from_connection(&client, &type, &available);
      if (!available) {
        break;
      }
      if (type == PACKET_JOIN_GAME_REPLY) {
        assert(((struct packet_join_game_reply *)packet)->you_can_join);
        sz_strlcpy(client.capability,
                   ((struct packet_join_game_reply *)packet)->capability);
        joins++;
      }
      free(packet);
    }
    assert(++iterations < 100);
  } while (received != 0);
  assert(joins == 1);
  assert(fc_offline_engine_snapshot(&snapshot) == 0);
  assert(snapshot.players == 1 && game.nplayers == 0);
  offline_session_close(&session);
  assert(!client.used && !client.buffer && !client.send_buffer);
  assert(offline_session_open(&session, &client, "DOSPlayer") == 0);
  offline_session_close(&session);
  offline_session_close(&session);
}

struct observations {
  int nation_prompts;
  int start_turns;
  int tiles;
  int units;
  int tax;
  int science;
  int width;
  int height;
};

/* The observer frees payloads that a real client transfers to its cache. */
static void release_observed_packet(enum packet_type type, void *packet)
{
  switch (type) {
  case PACKET_RULESET_UNIT:
    free(((struct packet_ruleset_unit *)packet)->helptext);
    break;
  case PACKET_RULESET_TECH:
    free(((struct packet_ruleset_tech *)packet)->helptext);
    break;
  case PACKET_RULESET_BUILDING:
    {
      struct packet_ruleset_building *building = packet;
      free(building->terr_gate);
      free(building->spec_gate);
      free(building->equiv_dupl);
      free(building->equiv_repl);
      free(building->effect);
      free(building->helptext);
    }
    break;
  case PACKET_RULESET_TERRAIN:
    free(((struct packet_ruleset_terrain *)packet)->helptext);
    break;
  case PACKET_RULESET_TERRAIN_CONTROL:
    free(((struct terrain_misc *)packet)->river_help_text);
    break;
  case PACKET_RULESET_GOVERNMENT:
    free(((struct packet_ruleset_government *)packet)->helptext);
    break;
  default:
    break;
  }
  free(packet);
}

static void drain_game(struct offline_session *session,
                       struct observations *seen)
{
  unsigned int iterations = 0;
  int received;

  do {
    struct connection *client = session->client;
    enum packet_type type;
    bool available;
    void *packet;

    received = offline_session_pump(session, 32);
    assert(received >= 0);
    for (;;) {
      packet = get_packet_from_connection(client, &type, &available);
      if (!available) {
        break;
      }
      if (type == PACKET_JOIN_GAME_REPLY) {
        struct packet_join_game_reply *reply = packet;
        assert(reply->you_can_join);
        sz_strlcpy(client->capability, reply->capability);
        client->established = TRUE;
      } else if (type == PACKET_SELECT_NATION) {
        seen->nation_prompts++;
      } else if (type == PACKET_START_TURN) {
        seen->start_turns++;
      } else if (type == PACKET_TILE_INFO) {
        seen->tiles++;
      } else if (type == PACKET_UNIT_INFO) {
        seen->units++;
      } else if (type == PACKET_MAP_INFO) {
        struct packet_map_info *info = packet;
        seen->width = info->xsize;
        seen->height = info->ysize;
      } else if (type == PACKET_PLAYER_INFO) {
        struct packet_player_info *player = packet;
        if (player->playerno == 0) {
          seen->tax = player->tax;
          seen->science = player->science;
        }
      }
      release_observed_packet(type, packet);
    }
    assert(++iterations < 1000);
  } while (received != 0);
}

static void command(struct connection *client, const char *text)
{
  struct packet_generic_message message;

  memset(&message, 0, sizeof(message));
  sz_strlcpy(message.message, text);
  send_packet_generic_message(client, PACKET_CHAT_MSG, &message);
}

static void test_gameplay_bridge(void)
{
  int cycle;
  struct dos_widget_spec widgets[2];
  struct dos_event_hooks hooks = {ui_poll, ui_dispatch, ui_no_service,
                                 ui_present, ui_idle, NULL};
  memset(widgets, 0, sizeof(widgets));
  widgets[0].type = DOS_WIDGET_TEXT;
  widgets[0].label = "Responsive UI during real engine turns";
  widgets[0].text = test_text; widgets[0].capacity = sizeof(test_text);
  widgets[1].type = DOS_WIDGET_BUTTON;
  widgets[1].label = "Continue"; widgets[1].action = 1;
  assert(dos_vbe_framebuffer_init(&test_ui_buffer, 640, 480, 16) == 0);
  assert(dos_widgets_begin(&test_ui_buffer, "AI-turn UI service", "", widgets, 2) == 0);
  assert(dos_event_loop_init(&test_ui_loop, &hooks) == 0);
  fc_offline_engine_set_service(service_ui, &ui_service_calls);
  test_service_reentry = 1;

  for (cycle = 0; cycle < 2; cycle++) {
    struct offline_session session = {0};
    struct connection client;
    struct observations seen = {0};
    struct fc_offline_snapshot before, after;
    struct packet_alloc_nation nation;
    struct packet_player_request rates;
    struct packet_generic_message turn_done;
    struct tile *client_tiles = map.tiles;
    unsigned long services_before = ui_service_calls;

    memset(&client, 0, sizeof(client));
    assert(offline_session_open(&session, &client, "DOSPlayer") == 0);
    assert(fc_offline_engine_start() == -1);
    drain_game(&session, &seen);
    command(&client, "/set xsize 40");
    command(&client, "/set ysize 40");
    command(&client, "/set aifill 2");
    command(&client, "/set timeout 0");
    drain_game(&session, &seen);
    assert(fc_offline_engine_start() == 0);
    assert(fc_offline_engine_start() == -1);
    drain_game(&session, &seen);
    assert(seen.nation_prompts == 1);
    assert(fc_offline_engine_snapshot(&before) == 0);
    assert(before.state == SELECT_RACES_STATE && !before.map_allocated);
    assert(fc_offline_engine_poll(1) == 0);

    memset(&nation, 0, sizeof(nation));
    nation.nation_no = 0;
    nation.is_male = TRUE;
    sz_strlcpy(nation.name, "DOSRuler");
    ui_allow = 1;
    send_packet_alloc_nation(&client, &nation);
    drain_game(&session, &seen);
    assert(fc_offline_engine_snapshot(&before) == 0);
    assert(before.state == RUN_GAME_STATE && before.map_allocated);
    assert(before.ai_players >= 1);
    assert(seen.start_turns >= 1 && seen.tiles > 0 && seen.units > 0);
    assert(seen.width == 40 && seen.height == 40);
    assert(seen.tiles < seen.width * seen.height);
    assert(game.nplayers == 0 && map.tiles == client_tiles);

    memset(&rates, 0, sizeof(rates));
    rates.tax = 50;
    rates.science = 50;
    send_packet_player_request(&client, &rates, PACKET_PLAYER_RATES);
    drain_game(&session, &seen);
    assert(seen.tax == 50 && seen.science == 50);

    memset(&turn_done, 0, sizeof(turn_done));
    send_packet_generic_message(&client, PACKET_TURN_DONE, &turn_done);
    drain_game(&session, &seen);
    assert(fc_offline_engine_snapshot(&after) == 0);
    assert(after.turn == before.turn + 1 && after.year > before.year);
    assert(after.last_request == client.client.last_request_id_used);
    assert(seen.start_turns >= 2);
    assert(ui_service_calls > services_before + 8);
    assert(fc_offline_engine_poll(1) == 0);

    offline_session_close(&session);
    assert(game.nplayers == 0 && map.tiles == client_tiles);
  }
  assert(!test_service_reentry);
  assert(!strcmp(test_text, "AI") && ui_activations == 1);
  assert(test_ui_loop.received == 4 && test_ui_loop.dispatched == 4);
  fc_offline_engine_set_service(NULL, NULL);
  dos_widgets_close();
  dos_event_loop_stop(&test_ui_loop);
  dos_vbe_framebuffer_destroy(&test_ui_buffer);
  puts("PASS real engine/AI turns service the actual event queue and editable modal widgets without recursive engine polling");
}

int main(void)
{
  log_init(NULL, LOG_ERROR, NULL);
  game_init();
  map.xsize = 8;
  map.ysize = 8;
  map_allocate();
  test_queue();
  test_session();
  test_backpressure();
  test_invalid_frame();
  test_transport_failure();
  test_recursive_processing();
  test_rejection_delivery();
  test_session_bridge();
  test_gameplay_bridge();
  test_session_bridge();
  game_free();
  puts("Offline transport tests passed: isolated real engine join, request "
       "boundaries, fragmented packets, buffering, backpressure, wraparound, "
       "invalid frames, explicit failures, reentrancy guards, rejection "
       "delivery, reset, real map/AI initialization, rates action, "
       "and authoritative turn advancement.");
  return 0;
}
