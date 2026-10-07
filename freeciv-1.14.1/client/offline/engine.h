#ifndef FC__OFFLINE_ENGINE_H
#define FC__OFFLINE_ENGINE_H

typedef int (*fc_offline_write_fn)(void *context,
                                   const unsigned char *data, int len);

struct fc_offline_snapshot {
  int players;
  int turn;
  int year;
  int established;
  int last_request;
  int map_allocated;
  int state;
  int ai_players;
};

int fc_offline_engine_open(fc_offline_write_fn write, void *context);
int fc_offline_engine_feed(const unsigned char *data, int len);
int fc_offline_engine_start(void);
int fc_offline_engine_poll(unsigned int packet_budget);
int fc_offline_engine_snapshot(struct fc_offline_snapshot *snapshot);
void fc_offline_engine_close(void);

#endif
