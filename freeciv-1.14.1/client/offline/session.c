#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <limits.h>
#include <string.h>

#include "capstr.h"
#include "connection.h"
#include "log.h"
#include "packets.h"
#include "support.h"
#include "version.h"

#include "engine.h"
#include "session.h"

int offline_session_open(struct offline_session *session,
                         struct connection *client, const char *name)
{
  struct packet_req_join_game request;

  if (!session || session->client || !client || client->used
      || client->buffer || client->send_buffer || !name
      || strlen(name) == 0 || strlen(name) >= MAX_LEN_NAME) {
    freelog(LOG_ERROR, "Offline session: invalid initialization");
    return -1;
  }
  if (local_queue_init(&session->requests, MAX_LEN_BUFFER) != 0) {
    return -1;
  }
  if (local_queue_init(&session->responses, MAX_LEN_BUFFER) != 0) {
    local_queue_free(&session->requests);
    return -1;
  }
  if (fc_offline_engine_open(local_queue_write, &session->responses) != 0) {
    local_queue_free(&session->requests);
    local_queue_free(&session->responses);
    return -1;
  }
  session->client = client;
  session->failed = 0;
  client->sock = -1;
  client->used = TRUE;
  client->established = FALSE;
  client->player = NULL;
  client->observer = FALSE;
  client->first_packet = TRUE;
  client->byte_swap = FALSE;
  client->delayed_disconnect = FALSE;
  client->capability[0] = '\0';
  client->name[0] = '\0';
  client->addr[0] = '\0';
  client->last_write = 0;
  client->buffer = new_socket_packet_buffer();
  client->send_buffer = new_socket_packet_buffer();
  memset(&client->client, 0, sizeof(client->client));
  client->local_write = local_queue_write;
  client->local_context = &session->requests;

  memset(&request, 0, sizeof(request));
  sz_strlcpy(request.short_name, name);
  sz_strlcpy(request.name, name);
  sz_strlcpy(request.capability, our_capability);
  request.major_version = MAJOR_VERSION;
  request.minor_version = MINOR_VERSION;
  request.patch_version = PATCH_VERSION;
  sz_strlcpy(request.version_label, VERSION_LABEL);
  send_packet_req_join_game(client, &request);
  return 0;
}

int offline_session_pump(struct offline_session *session,
                         unsigned int packet_budget)
{
  unsigned char bytes[MAX_LEN_PACKET];
  size_t count;
  struct connection *client;

  if (!session || !session->client || packet_budget == 0
      || packet_budget > INT_MAX) {
    freelog(LOG_ERROR, "Offline session: invalid pump");
    return -1;
  }
  client = session->client;
  if (!session->failed) {
    flush_connection_send_buffer_all(client);
    if (client->delayed_disconnect) {
      freelog(LOG_ERROR, "Offline session: client transport failed");
      return -1;
    }
    while ((count = local_queue_read(&session->requests, bytes,
                                     sizeof(bytes))) != 0) {
      if (fc_offline_engine_feed(bytes, (int)count) != 0) {
        return -1;
      }
    }
    if (fc_offline_engine_poll(packet_budget) < 0) {
      session->failed = 1;
    }
  }
  count = local_queue_read(&session->responses, bytes, sizeof(bytes));
  if (count != 0 && !connection_receive_data(client, bytes, (int)count)) {
    return -1;
  }
  if (count == 0 && session->failed) {
    freelog(LOG_ERROR, "Offline session: engine connection failed");
    return -1;
  }
  return (int)count;
}

void offline_session_close(struct offline_session *session)
{
  if (session && session->client) {
    struct connection *client = session->client;

    fc_offline_engine_close();
    client->used = FALSE;
    client->established = FALSE;
    free_socket_packet_buffer(client->buffer);
    free_socket_packet_buffer(client->send_buffer);
    client->buffer = NULL;
    client->send_buffer = NULL;
    client->local_write = NULL;
    client->local_context = NULL;
    local_queue_free(&session->requests);
    local_queue_free(&session->responses);
    session->client = NULL;
    session->failed = 0;
  }
}
