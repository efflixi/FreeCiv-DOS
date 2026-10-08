/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef FC__DOS_VBE_EVENT_LOOP_H
#define FC__DOS_VBE_EVENT_LOOP_H

#include <stddef.h>
#include "input.h"

#define DOS_EVENT_CAPACITY 64U
#define DOS_EVENT_BUDGET 8U

struct dos_event_hooks {
  int (*poll)(struct dos_input_event *event, void *context);
  int (*dispatch)(const struct dos_input_event *event, void *context);
  int (*service)(void *context);
  int (*present)(void *context);
  void (*idle)(void *context);
  void *context;
};

struct dos_event_loop {
  struct dos_event_hooks hooks;
  struct dos_input_event events[DOS_EVENT_CAPACITY];
  unsigned int head, count;
  unsigned long ticks, received, dispatched, idle_calls;
  int running, servicing;
};

int dos_event_loop_init(struct dos_event_loop *loop,
                        const struct dos_event_hooks *hooks);
int dos_event_loop_enqueue(struct dos_event_loop *loop,
                           const struct dos_input_event *event);
int dos_event_loop_step(struct dos_event_loop *loop);
int dos_event_loop_ui_service(struct dos_event_loop *loop);
int dos_event_loop_capture(struct dos_event_loop *loop);
void dos_event_loop_stop(struct dos_event_loop *loop);

#endif
