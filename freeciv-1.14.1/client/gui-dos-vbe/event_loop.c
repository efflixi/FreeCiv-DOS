/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <stdio.h>
#include <string.h>
#include "event_loop.h"

int dos_event_loop_init(struct dos_event_loop *loop,
                        const struct dos_event_hooks *hooks)
{
  if (!loop || !hooks || !hooks->poll || !hooks->dispatch
      || !hooks->service || !hooks->present || !hooks->idle) {
    fprintf(stderr, "DOS event loop: incomplete service hooks.\n");
    return -1;
  }
  memset(loop, 0, sizeof(*loop));
  loop->hooks = *hooks;
  loop->running = 1;
  return 0;
}

int dos_event_loop_enqueue(struct dos_event_loop *loop,
                           const struct dos_input_event *event)
{
  if (!loop || !loop->running || !event || loop->count == DOS_EVENT_CAPACITY) {
    fprintf(stderr, "DOS event loop: event rejected; queue is full or inactive.\n");
    return -1;
  }
  loop->events[(loop->head + loop->count) % DOS_EVENT_CAPACITY] = *event;
  loop->count++;
  loop->received++;
  return 0;
}

int dos_event_loop_capture(struct dos_event_loop *loop)
{
  unsigned int i;
  struct dos_input_event event;
  if (!loop || !loop->running) return 0;
  for (i = 0; i < DOS_EVENT_BUDGET && loop->count < DOS_EVENT_CAPACITY; ++i) {
    int result = loop->hooks.poll(&event, loop->hooks.context);
    if (result < 0) return -1;
    if (!result) break;
    if (dos_event_loop_enqueue(loop, &event) != 0) return -1;
  }
  return 0;
}

static int input_slice(struct dos_event_loop *loop)
{
  unsigned int i;
  struct dos_input_event event;
  if (dos_event_loop_capture(loop) != 0) return -1;
  for (i = 0; i < DOS_EVENT_BUDGET && loop->count && loop->running; ++i) {
    event = loop->events[loop->head];
    loop->head = (loop->head + 1U) % DOS_EVENT_CAPACITY;
    loop->count--;
    loop->dispatched++;
    if (loop->hooks.dispatch(&event, loop->hooks.context) != 0) return -1;
  }
  return 0;
}

int dos_event_loop_ui_service(struct dos_event_loop *loop)
{
  if (!loop || !loop->running) return 0;
  if (input_slice(loop) != 0
      || loop->hooks.present(loop->hooks.context) != 0) {
    fprintf(stderr, "DOS event loop: UI service failed.\n");
    loop->running = 0;
    return -1;
  }
  return 0;
}

int dos_event_loop_step(struct dos_event_loop *loop)
{
  unsigned long dispatched;
  if (!loop || !loop->running || loop->servicing) {
    fprintf(stderr, "DOS event loop: inactive or recursive step.\n");
    return -1;
  }
  dispatched = loop->dispatched;
  loop->ticks++;
  if (input_slice(loop) != 0) goto failed;
  if (!loop->running) return 0;
  loop->servicing = 1;
  if (loop->hooks.service(loop->hooks.context) != 0) {
    loop->servicing = 0;
    goto failed;
  }
  loop->servicing = 0;
  if (loop->hooks.present(loop->hooks.context) != 0) goto failed;
  if (loop->dispatched == dispatched && !loop->count) {
    loop->hooks.idle(loop->hooks.context);
    loop->idle_calls++;
  }
  return 0;
failed:
  fprintf(stderr, "DOS event loop: input, engine or presentation failed.\n");
  loop->running = 0;
  return -1;
}

void dos_event_loop_stop(struct dos_event_loop *loop)
{
  if (loop) loop->running = 0;
}
