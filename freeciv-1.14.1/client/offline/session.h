#ifndef FC__OFFLINE_SESSION_H
#define FC__OFFLINE_SESSION_H

#include "local_queue.h"

struct connection;

struct offline_session {
  struct connection *client;
  struct local_queue requests;
  struct local_queue responses;
  int failed;
};

int offline_session_open(struct offline_session *session,
                         struct connection *client, const char *name);
int offline_session_pump(struct offline_session *session,
                         unsigned int packet_budget);
void offline_session_close(struct offline_session *session);

#endif
