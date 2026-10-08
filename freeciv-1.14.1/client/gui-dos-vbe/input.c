/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "input.h"

#ifdef __DJGPP__
#include <dpmi.h>
#include <go32.h>
#include <sys/farptr.h>
#include <unistd.h>

static int initialized, mouse_present, mouse_first;
static int pointer_x, pointer_y;
static unsigned int screen_width, screen_height, pointer_buttons;
static unsigned int mouse_button_count;
static unsigned int pending_presses[3], pending_releases[3];
static int press_x[3], press_y[3];

static int interrupt_call(int vector, __dpmi_regs *regs)
{
  if (__dpmi_int(vector, regs) != 0) {
    fprintf(stderr, "DOS input: INT %02xh failed (DPMI 0x%04x).\n",
            vector, __dpmi_error);
    return -1;
  }
  return 0;
}

static int mouse_call(unsigned int function, unsigned int cx,
                      unsigned int dx, __dpmi_regs *regs)
{
  memset(regs, 0, sizeof(*regs));
  regs->x.ax = function;
  regs->x.cx = cx;
  regs->x.dx = dx;
  return interrupt_call(0x33, regs);
}

static int clamp_coordinate(unsigned int coordinate, unsigned int size)
{
  int value = (short)coordinate;

  if (value < 0) {
    return 0;
  }
  if ((unsigned int)value >= size) {
    return size - 1;
  }
  return value;
}

static int read_modifiers(unsigned int *modifiers, unsigned int *flags)
{
  __dpmi_regs regs;

  memset(&regs, 0, sizeof(regs));
  regs.x.ax = 0x1200;
  if (interrupt_call(0x16, &regs) != 0) {
    return -1;
  }
  *modifiers = ((regs.x.ax & 3) ? DOS_INPUT_SHIFT : 0)
               | ((regs.x.ax & 4) ? DOS_INPUT_CTRL : 0)
               | ((regs.x.ax & 8) ? DOS_INPUT_ALT : 0);
  if (flags) *flags = regs.x.ax;
  return 0;
}

static int poll_keyboard(struct dos_input_event *event)
{
  __dpmi_regs regs;
  unsigned int flags;

  memset(&regs, 0, sizeof(regs));
  regs.x.ax = 0x1100;
  if (interrupt_call(0x16, &regs) != 0) {
    return -1;
  }
  if (regs.x.flags & 0x40) {
    return 0;
  }
  memset(&regs, 0, sizeof(regs));
  regs.x.ax = 0x1000;
  if (interrupt_call(0x16, &regs) != 0) {
    return -1;
  }
  event->type = DOS_INPUT_KEY;
  /* Keep AL intact: printable H/P/M/K are not directional scan codes.
   * Enhanced keys may carry either 00h or E0h in AL. */
  event->ascii = regs.x.ax & 0xff;
  event->scan = regs.x.ax >> 8;
  if (read_modifiers(&event->modifiers, &flags) != 0) return -1;
  /* Modifier keys can be released before a buffered key is read. Preserve
   * combinations encoded by the BIOS independently of its current flags. */
  if ((!event->ascii && event->scan == 15U)
      || (!(flags & 0x20U)
          && ((event->scan == 72U && event->ascii == '8')
              || (event->scan == 75U && event->ascii == '4')
              || (event->scan == 77U && event->ascii == '6')
              || (event->scan == 80U && event->ascii == '2')))) {
    event->modifiers |= DOS_INPUT_SHIFT;
  }
  if (event->ascii == 17U || ((!event->ascii || event->ascii == 0xe0U)
      && (event->scan == 115U || event->scan == 116U
          || event->scan == 141U || event->scan == 145U
          || event->scan == 119U || event->scan == 132U
          || event->scan == 117U || event->scan == 118U))) {
    event->modifiers |= DOS_INPUT_CTRL;
  }
  if ((!event->ascii || event->ascii == 0xe0U) && event->scan == 0x6bU) {
    event->modifiers |= DOS_INPUT_ALT;
  }
  return 1;
}

static int read_mouse_counts(int retain)
{
  __dpmi_regs regs;
  unsigned int button, function, *pending;

  for (button = 0; button < mouse_button_count; button++) {
    for (function = 5; function <= 6; function++) {
      memset(&regs, 0, sizeof(regs));
      regs.x.ax = function;
      regs.x.bx = button;
      if (interrupt_call(0x33, &regs) != 0) {
        return -1;
      }
      if (!retain || !regs.x.bx) {
        continue;
      }
      pending = function == 5 ? &pending_presses[button]
                              : &pending_releases[button];
      if (UINT_MAX - *pending < regs.x.bx) {
        fprintf(stderr, "DOS input: mouse transition counter overflow.\n");
        return -1;
      }
      *pending += regs.x.bx;
      if (function == 5) {
        /* INT 33h supplies only the last press position for each counter. */
        press_x[button] = clamp_coordinate(regs.x.cx, screen_width);
        press_y[button] = clamp_coordinate(regs.x.dx, screen_height);
      }
    }
  }
  return 0;
}

static int poll_mouse(struct dos_input_event *event)
{
  __dpmi_regs regs;
  unsigned int buttons, pressed = 0, released = 0, button, mask;
  int x, y, click_button = -1;

  if (!mouse_present) {
    return 0;
  }
  /* Drivers latch transitions even when both edges occur during idle.
   * Keep consumed counts across errors and emit at most one per button
   * per event, so repeated complete clicks survive a single polling gap. */
  if (read_mouse_counts(1) != 0 || mouse_call(3, 0, 0, &regs) != 0) {
    return -1;
  }
  x = clamp_coordinate(regs.x.cx, screen_width);
  y = clamp_coordinate(regs.x.dx, screen_height);
  buttons = regs.x.bx & ((1U << mouse_button_count) - 1U);
  for (button = 0; button < mouse_button_count; button++) {
    mask = 1U << button;
    if (pending_presses[button]) {
      pressed |= mask;
      if (click_button < 0) {
        click_button = button;
      }
    }
    if (pending_releases[button]) {
      released |= mask;
    }
  }
  if (x == pointer_x && y == pointer_y && buttons == pointer_buttons
      && !pressed && !released) {
    return 0;
  }
  event->type = DOS_INPUT_POINTER;
  event->x = x;
  event->y = y;
  if (click_button >= 0) {
    event->x = press_x[click_button];
    event->y = press_y[click_button];
  }
  event->buttons = buttons;
  event->pressed = pressed | (buttons & ~pointer_buttons);
  event->released = released | (pointer_buttons & ~buttons);
  if (read_modifiers(&event->modifiers, NULL) != 0) {
    return -1;
  }
  pointer_x = x;
  pointer_y = y;
  pointer_buttons = buttons;
  for (button = 0; button < mouse_button_count; button++) {
    if (pending_presses[button]) {
      pending_presses[button]--;
    }
    if (pending_releases[button]) {
      pending_releases[button]--;
    }
  }
  return 1;
}

void dos_input_shutdown(void)
{
  __dpmi_regs regs;

  if (mouse_present) {
    /* Reset removes our ranges/position and leaves the driver cursor hidden. */
    mouse_call(0, 0, 0, &regs);
  }
  initialized = mouse_present = mouse_first = 0;
  pointer_x = pointer_y = 0;
  screen_width = screen_height = pointer_buttons = 0;
  mouse_button_count = 0;
  memset(pending_presses, 0, sizeof(pending_presses));
  memset(pending_releases, 0, sizeof(pending_releases));
  memset(press_x, 0, sizeof(press_x));
  memset(press_y, 0, sizeof(press_y));
}

int dos_input_init(unsigned int width, unsigned int height)
{
  __dpmi_raddr vector;
  __dpmi_regs regs;
  unsigned long address;

  dos_input_shutdown();
  /* INT 33h ranges and coordinates are signed 16-bit values. */
  if (!width || !height || width - 1 > SHRT_MAX || height - 1 > SHRT_MAX) {
    fprintf(stderr, "DOS input: dimensions must fit positive INT 33h ranges.\n");
    return -1;
  }
  screen_width = width;
  screen_height = height;
  pointer_x = width / 2;
  pointer_y = height / 2;
  if (__dpmi_get_real_mode_interrupt_vector(0x33, &vector) != 0) {
    fprintf(stderr, "DOS input: INT 33h vector query failed (DPMI 0x%04x).\n",
            __dpmi_error);
    goto failure;
  }
  address = ((unsigned long)vector.segment << 4) + vector.offset16;
  /* Never invoke a null vector or an uninstalled driver's bare IRET. */
  if (!address || _farpeekb(_dos_ds, address) == 0xcf) {
    goto keyboard_only;
  }
  if (mouse_call(0, 0, 0, &regs) != 0) {
    goto failure;
  }
  if (regs.x.ax != 0xffff) {
    goto keyboard_only;
  }
  mouse_present = 1;
  mouse_button_count = regs.x.bx >= 3 ? 3 : (regs.x.bx == 1 ? 1 : 2);
  if (mouse_call(2, 0, 0, &regs) != 0
      || mouse_call(7, 0, width - 1, &regs) != 0
      || mouse_call(8, 0, height - 1, &regs) != 0
      || mouse_call(4, pointer_x, pointer_y, &regs) != 0
      || mouse_call(3, 0, 0, &regs) != 0
      || read_mouse_counts(0) != 0) {
    goto failure;
  }
  pointer_x = clamp_coordinate(regs.x.cx, width);
  pointer_y = clamp_coordinate(regs.x.dx, height);
  pointer_buttons = regs.x.bx & ((1U << mouse_button_count) - 1U);
  initialized = 1;
  return 0;

keyboard_only:
  fprintf(stderr, "DOS input: no INT 33h mouse driver; keyboard-only input.\n");
  initialized = 1;
  return 0;

failure:
  dos_input_shutdown();
  return -1;
}

int dos_input_poll(struct dos_input_event *event)
{
  struct dos_input_event result;
  int status;

  if (!initialized || !event) {
    fprintf(stderr, "DOS input: poll requires initialization and an event.\n");
    return -1;
  }
  memset(&result, 0, sizeof(result));
  /* At most one probe per device; alternate priority after every event. */
  status = mouse_first ? poll_mouse(&result) : poll_keyboard(&result);
  if (!status) {
    status = mouse_first ? poll_keyboard(&result) : poll_mouse(&result);
  }
  if (status == 1) {
    *event = result;
    mouse_first = result.type == DOS_INPUT_KEY;
  }
  return status;
}

int dos_input_mouse_available(void)
{
  return initialized && mouse_present;
}

int dos_input_pointer(int *x, int *y)
{
  if (!initialized || !x || !y) {
    fprintf(stderr, "DOS input: pointer query requires initialization and outputs.\n");
    return -1;
  }
  *x = pointer_x;
  *y = pointer_y;
  return 0;
}

void dos_input_idle(void)
{
  /* DJGPP usleep converts microseconds to 91-Hz clock units, but clock()
   * advances by five units per ~55-ms BIOS tick. 10000us truncates to zero
   * and never yields. 100000us becomes eight units, requiring two BIOS
   * ticks (~55-110ms regardless of phase), yielding via INT 2Fh/1680h
   * while waiting even when that host yield returns immediately. */
  if (usleep(100000U) != 0) {
    fprintf(stderr, "DOS input: idle sleep interrupted.\n");
  }
}
#else
int dos_input_init(unsigned int width, unsigned int height)
{
  (void)width;
  (void)height;
  fprintf(stderr, "DOS input: DJGPP BIOS/DPMI services are required.\n");
  return -1;
}

void dos_input_shutdown(void)
{
}

int dos_input_poll(struct dos_input_event *event)
{
  (void)event;
  fprintf(stderr, "DOS input: DJGPP BIOS/DPMI services are required.\n");
  return -1;
}

int dos_input_mouse_available(void)
{
  return 0;
}

int dos_input_pointer(int *x, int *y)
{
  (void)x;
  (void)y;
  fprintf(stderr, "DOS input: DJGPP BIOS/DPMI services are required.\n");
  return -1;
}

void dos_input_idle(void)
{
  fprintf(stderr, "DOS input: DJGPP BIOS/DPMI services are required.\n");
}
#endif
