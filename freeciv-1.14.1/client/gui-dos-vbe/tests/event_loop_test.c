/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "event_loop.h"
#include "commands.h"

static unsigned int polled, dispatched, serviced, presented, idled;
static int fail_service, waiting;
static struct dos_event_loop loop;

static int poll_event(struct dos_input_event *event, void *context)
{
  assert(context == &loop);
  if (polled == 1000U) return 0;
  memset(event, 0, sizeof(*event));
  event->ascii = polled++;
  return 1;
}
static int dispatch(const struct dos_input_event *event, void *context)
{
  assert(context == &loop && event->ascii == dispatched++);
  return 0;
}
static int service(void *context)
{
  assert(context == &loop);
  serviced++;
  if (waiting) {
    assert(dos_event_loop_step(&loop) == -1);
    assert(dos_event_loop_ui_service(&loop) == 0);
  }
  return fail_service ? -1 : 0;
}
static int present(void *context)
{
  assert(context == &loop);
  presented++;
  return 0;
}
static void idle(void *context)
{
  assert(context == &loop);
  idled++;
}

int main(void)
{
  struct dos_event_hooks hooks = {poll_event, dispatch, service, present, idle, &loop};
  struct dos_input_event event;
  unsigned int i;
  int dx, dy;
  assert(dos_event_loop_init(&loop, &hooks) == 0);
  for (i = 0; i < 125; ++i) assert(dos_event_loop_step(&loop) == 0);
  assert(polled == 1000 && dispatched == 1000 && serviced == 125 && presented == 125);
  assert(loop.received == 1000 && loop.dispatched == 1000 && !loop.count);
  assert(dos_event_loop_step(&loop) == 0 && idled == 1);
  memset(&event, 0, sizeof(event));
  for (i = 0; i < DOS_EVENT_CAPACITY; ++i) {
    event.ascii = dispatched + i;
    assert(dos_event_loop_enqueue(&loop, &event) == 0);
  }
  assert(dos_event_loop_enqueue(&loop, &event) == -1);
  waiting = 1;
  for (i = 0; i < DOS_EVENT_CAPACITY / (2U * DOS_EVENT_BUDGET); ++i) {
    assert(dos_event_loop_step(&loop) == 0);
  }
  assert(!loop.count && loop.servicing == 0);
  waiting = 0;
  fail_service = 1;
  assert(dos_event_loop_step(&loop) == -1 && !loop.running);
  assert(dos_event_loop_init(NULL, &hooks) == -1);
  memset(&event, 0, sizeof(event));
  event.ascii = 'H'; event.scan = 35;
  assert(dos_vbe_event_command(&event) == DOS_VBE_CMD_HELP);
  assert(!dos_vbe_event_direction(&event, &dx, &dy));
  event.ascii = 'P';
  assert(dos_vbe_event_command(&event) == DOS_VBE_CMD_OPTIONS);
  event.ascii = 'M';
  assert(dos_vbe_event_command(&event) == DOS_VBE_CMD_MENU);
  event.ascii = 'K';
  assert(dos_vbe_event_command(&event) == DOS_VBE_CMD_NONE);
  event.ascii = 0xe0; event.scan = 72;
  assert(dos_vbe_event_direction(&event, &dx, &dy) && dx == 0 && dy == -1);
  event.modifiers = DOS_INPUT_CTRL;
  assert(dos_vbe_event_command(&event) == DOS_VBE_CMD_MOVE_NORTH);
  event.ascii = 0; event.scan = 116;
  assert(dos_vbe_event_command(&event) == DOS_VBE_CMD_MOVE_EAST);
  event.scan = 141;
  assert(dos_vbe_event_command(&event) == DOS_VBE_CMD_MOVE_NORTH);
  event.ascii = '4'; event.scan = 75; event.modifiers = DOS_INPUT_SHIFT;
  assert(dos_vbe_event_direction(&event, &dx, &dy) && dx == -1 && dy == 0);
  event.modifiers = 0;
  assert(!dos_vbe_event_direction(&event, &dx, &dy));
  assert(dos_vbe_event_command(&event) == DOS_VBE_CMD_MOVE_WEST);
  event.ascii = '4'; event.scan = 5; event.modifiers = DOS_INPUT_SHIFT;
  assert(!dos_vbe_event_direction(&event, &dx, &dy));
  event.ascii = 0; event.scan = 81; event.modifiers = 0;
  assert(dos_vbe_event_command(&event) == DOS_VBE_CMD_MOVE_SOUTH_EAST);
  event.ascii = '1';
  assert(dos_vbe_event_command(&event) == DOS_VBE_CMD_MOVE_SOUTH_WEST);
  event.modifiers = DOS_INPUT_ALT; event.scan = 0x3e;
  assert(dos_vbe_event_command(&event) == DOS_VBE_CMD_QUIT);
  event.scan = 0x6b;
  assert(dos_vbe_event_command(&event) == DOS_VBE_CMD_QUIT);
  assert(dos_event_loop_init(&loop, &hooks) == 0);
  for (i = DOS_VBE_CMD_MOVE_NORTH; i <= DOS_VBE_CMD_DONE_UNIT; ++i) {
    unsigned int count = loop.count;
    if (i == DOS_VBE_CMD_CANCEL || i == DOS_VBE_CMD_TOGGLE_OVERVIEW) continue;
    assert(dos_vbe_defer_command(&loop, i) == 0);
    assert(loop.count == count + 1);
    assert(loop.events[count].type == DOS_INPUT_COMMAND);
    assert(dos_vbe_event_command(&loop.events[count]) == i);
  }
  assert(dos_vbe_defer_command(&loop, DOS_VBE_CMD_QUIT) == -1);
  while (loop.count < DOS_EVENT_CAPACITY) {
    assert(dos_vbe_defer_command(&loop, DOS_VBE_CMD_WAIT_UNIT) == 0);
  }
  assert(dos_vbe_defer_command(&loop, DOS_VBE_CMD_WAIT_UNIT) == -1);
  dos_event_loop_stop(&loop);
  assert(dos_vbe_defer_command(&loop, DOS_VBE_CMD_WAIT_UNIT) == -1);
  puts("PASS bounded input/dispatch/engine/presentation fairness, FIFO/backpressure,"
       " 1000 repeats, idle yielding, modal service, failure/reentry and ASCII/scan/deferred commands");
  return 0;
}
