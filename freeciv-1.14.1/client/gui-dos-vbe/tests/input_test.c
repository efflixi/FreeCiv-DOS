/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include <dpmi.h>
#include <go32.h>
#include <sys/farptr.h>
#include <unistd.h>

#include "input.h"

unsigned short __dpmi_error = 0x8015;
static int vector_error, null_vector, iret_vector, driver_present;
static int fail_vector, fail_function, calls, resets, hides, peeks;
static int fail_button;
static unsigned int driver_buttons;
static unsigned int mouse_x, mouse_y, mouse_buttons, modifiers;
static unsigned int range_x, range_y, sleep_calls, sleep_result;
static unsigned int key_ascii, key_scan, keys;
static unsigned int latched[2][3], latch_x[2][3], latch_y[2][3];
static unsigned int counter_calls[2][3];

int __dpmi_get_real_mode_interrupt_vector(int vector, __dpmi_raddr *address)
{
  assert(vector == 0x33);
  if (vector_error) {
    return -1;
  }
  address->segment = null_vector ? 0 : 0x1234;
  address->offset16 = null_vector ? 0 : 0x5678;
  return 0;
}

unsigned char _farpeekb(unsigned short selector, unsigned long address)
{
  assert(selector == _dos_ds);
  assert(address == 0x179b8UL);
  peeks++;
  return iret_vector ? 0xcf : 0xeb;
}

int __dpmi_int(int vector, __dpmi_regs *regs)
{
  unsigned int function = regs->x.ax;
  unsigned int button;

  calls++;
  if (vector == fail_vector && (int)function == fail_function
      && (fail_button < 0 || (int)regs->x.bx == fail_button)) {
    return -1;
  }
  if (vector == 0x16) {
    switch (function) {
    case 0x1100:
      regs->x.flags = keys ? 0 : 0x40;
      /* Poison AX on an empty queue: it must never become an event. */
      regs->x.ax = 0x4848;
      break;
    case 0x1000:
      assert(keys > 0);
      keys--;
      regs->x.ax = (key_scan << 8) | key_ascii;
      break;
    case 0x1200:
      regs->x.ax = modifiers;
      break;
    default:
      assert(0);
    }
  } else {
    assert(vector == 0x33);
    assert(!null_vector && !iret_vector);
    switch (function) {
    case 0:
      resets++;
      regs->x.ax = driver_present ? 0xffff : 0;
      regs->x.bx = driver_buttons;
      break;
    case 2:
      hides++;
      break;
    case 3:
      regs->x.cx = mouse_x;
      regs->x.dx = mouse_y;
      regs->x.bx = mouse_buttons;
      break;
    case 4:
      mouse_x = regs->x.cx;
      mouse_y = regs->x.dx;
      break;
    case 5:
    case 6:
      button = regs->x.bx;
      assert(button < driver_buttons && button < 3);
      counter_calls[function - 5][button]++;
      regs->x.ax = mouse_buttons;
      regs->x.bx = latched[function - 5][button];
      regs->x.cx = latch_x[function - 5][button];
      regs->x.dx = latch_y[function - 5][button];
      latched[function - 5][button] = 0;
      break;
    case 7:
      assert(regs->x.cx == 0);
      range_x = regs->x.dx;
      break;
    case 8:
      assert(regs->x.cx == 0);
      range_y = regs->x.dx;
      break;
    default:
      /* Showing the hardware cursor is never allowed. */
      assert(0);
    }
  }
  return 0;
}

unsigned int usleep(unsigned int useconds)
{
  unsigned int clock_units, bios_ticks;

  assert(useconds == 100000U);
  /* Match the installed DJGPP libc conversion and BIOS clock granularity.
   * A one-tick wait may end immediately at a tick boundary; require two. */
  clock_units = (useconds >> 10) * 91U / 1000U;
  bios_ticks = (clock_units + 4U) / 5U;
  assert(clock_units == 8U);
  assert(bios_ticks == 2U);
  assert((bios_ticks - 1U) * 54925U >= 10000U);
  assert(bios_ticks * 54925U <= 110000U);
  sleep_calls++;
  return sleep_result;
}

static void clean_services(void)
{
  dos_input_shutdown();
  vector_error = null_vector = iret_vector = 0;
  driver_present = 1;
  driver_buttons = 3;
  fail_vector = fail_function = fail_button = -1;
  calls = resets = hides = peeks = 0;
  mouse_x = mouse_y = mouse_buttons = modifiers = 0;
  range_x = range_y = sleep_calls = sleep_result = 0;
  keys = key_ascii = key_scan = 0;
  memset(latched, 0, sizeof(latched));
  memset(latch_x, 0, sizeof(latch_x));
  memset(latch_y, 0, sizeof(latch_y));
  memset(counter_calls, 0, sizeof(counter_calls));
}

static int poll(struct dos_input_event *event)
{
  int before = calls;
  int status = dos_input_poll(event);

  assert(calls - before <= 10);
  return status;
}

static void test_lifecycle(void)
{
  struct dos_input_event event, unchanged;
  int x = -1, y = -1, before;

  clean_services();
  memset(&event, 0xa5, sizeof(event));
  unchanged = event;
  assert(poll(&event) == -1);
  assert(memcmp(&event, &unchanged, sizeof(event)) == 0);
  assert(dos_input_pointer(&x, &y) == -1);
  assert(x == -1 && y == -1);
  assert(dos_input_init(0, 480) == -1);
  assert(dos_input_init(640, 0) == -1);
  assert(dos_input_init(UINT_MAX, 480) == -1);
  assert(dos_input_init(640, (unsigned int)SHRT_MAX + 2) == -1);
  assert(calls == 0);
  assert(dos_input_init(640, 480) == 0);
  assert(range_x == 639 && range_y == 479);
  assert(hides == 1 && resets == 1);
  assert(dos_input_mouse_available() == 1);
  assert(dos_input_pointer(&x, &y) == 0 && x == 320 && y == 240);
  assert(dos_input_pointer(NULL, &y) == -1);
  assert(dos_input_pointer(&x, NULL) == -1);
  assert(dos_input_poll(NULL) == -1);
  assert(poll(&event) == 0);
  assert(memcmp(&event, &unchanged, sizeof(event)) == 0);
  dos_input_idle();
  assert(sleep_calls == 1);
  sleep_result = 1;
  dos_input_idle();
  assert(sleep_calls == 2);
  assert(dos_input_init(800, 600) == 0);
  assert(range_x == 799 && range_y == 599);
  assert(resets == 3 && hides == 2);
  assert(dos_input_pointer(&x, &y) == 0 && x == 400 && y == 300);
  dos_input_shutdown();
  assert(!dos_input_mouse_available());
  before = calls;
  dos_input_shutdown();
  assert(calls == before);
  assert(dos_input_init(1, 1) == 0);
  assert(range_x == 0 && range_y == 0);
  assert(dos_input_pointer(&x, &y) == 0 && x == 0 && y == 0);
  assert(dos_input_init((unsigned int)SHRT_MAX + 1,
                        (unsigned int)SHRT_MAX + 1) == 0);
  assert(range_x == SHRT_MAX && range_y == SHRT_MAX);
}

static void test_keyboard_only(void)
{
  struct dos_input_event event;
  int variant, x, y;

  for (variant = 0; variant < 3; variant++) {
    clean_services();
    null_vector = variant == 0;
    iret_vector = variant == 1;
    driver_present = variant != 2;
    assert(dos_input_init(800, 600) == 0);
    assert(!dos_input_mouse_available());
    assert(peeks == (variant == 0 ? 0 : 1));
    assert(resets == (variant == 2 ? 1 : 0));
    assert(hides == 0);
    assert(dos_input_pointer(&x, &y) == 0 && x == 400 && y == 300);
    key_ascii = 'H';
    key_scan = 0x23;
    keys = 1;
    assert(poll(&event) == 1 && event.type == DOS_INPUT_KEY);
    assert(event.ascii == 'H' && event.scan == 0x23);
    assert(poll(&event) == 0);
  }
}

static void test_keys(void)
{
  static const unsigned int ascii[] = { 'H', 'P', 'M', 'K', 0, 0xe0 };
  static const unsigned int scans[] = {
    0x48, 0x50, 0x4b, 0x4d, 0x47, 0x49, 0x4f, 0x51
  };
  struct dos_input_event event;
  unsigned int a, s, m;

  clean_services();
  assert(dos_input_init(640, 480) == 0);
  for (a = 0; a < sizeof(ascii) / sizeof(ascii[0]); a++) {
    for (s = 0; s < sizeof(scans) / sizeof(scans[0]); s++) {
      for (m = 0; m < 16; m++) {
        key_ascii = ascii[a];
        key_scan = scans[s];
        modifiers = m | 0xff00;
        keys = 1;
        assert(poll(&event) == 1 && event.type == DOS_INPUT_KEY);
        assert(event.ascii == ascii[a] && event.scan == scans[s]);
        assert(event.modifiers == ((m & 3 ? DOS_INPUT_SHIFT : 0)
                                  | (m & 4 ? DOS_INPUT_CTRL : 0)
                                  | (m & 8 ? DOS_INPUT_ALT : 0)));
        assert(!event.buttons && !event.pressed && !event.released);
      }
    }
  }
  /* NumLock keypad digits stay printable, not directional keys. */
  key_ascii = '8';
  key_scan = 0x48;
  modifiers = 0x20;
  keys = 1;
  assert(poll(&event) == 1 && event.ascii == '8' && event.scan == 0x48);
  assert(!event.modifiers);
  modifiers = 0;
  keys = 1;
  assert(poll(&event) == 1 && event.modifiers == DOS_INPUT_SHIFT);
  key_ascii = 0; key_scan = 15; keys = 1;
  assert(poll(&event) == 1 && event.modifiers == DOS_INPUT_SHIFT);
  key_ascii = 17; key_scan = 16; keys = 1;
  assert(poll(&event) == 1 && event.modifiers == DOS_INPUT_CTRL);
  key_ascii = 0; key_scan = 116; keys = 1;
  assert(poll(&event) == 1 && event.modifiers == DOS_INPUT_CTRL);
  key_ascii = 0xe0; keys = 1;
  assert(poll(&event) == 1 && event.modifiers == DOS_INPUT_CTRL);
  key_scan = 0x6b; keys = 1;
  assert(poll(&event) == 1 && event.modifiers == DOS_INPUT_ALT);
}

static void test_mouse(void)
{
  struct dos_input_event event;
  int x, y, i;

  clean_services();
  mouse_buttons = DOS_INPUT_LEFT;
  assert(dos_input_init(800, 600) == 0);
  assert(poll(&event) == 0); /* A button held during init has no new edge. */
  mouse_buttons = 0;
  assert(poll(&event) == 1 && event.released == DOS_INPUT_LEFT);
  mouse_x = 100;
  mouse_y = 200;
  modifiers = 0x0d;
  assert(poll(&event) == 1 && event.type == DOS_INPUT_POINTER);
  assert(event.x == 100 && event.y == 200 && !event.pressed && !event.released);
  assert(event.modifiers == (DOS_INPUT_SHIFT | DOS_INPUT_CTRL | DOS_INPUT_ALT));
  assert(!event.ascii && !event.scan);
  mouse_buttons = DOS_INPUT_LEFT | DOS_INPUT_RIGHT;
  assert(poll(&event) == 1 && event.pressed == 3 && !event.released);
  for (i = 0; i < 32; i++) {
    assert(poll(&event) == 0);
  }
  mouse_x++;
  assert(poll(&event) == 1 && event.buttons == 3 && !event.pressed);
  mouse_buttons = DOS_INPUT_MIDDLE | 0xfff8;
  assert(poll(&event) == 1 && event.buttons == 4);
  assert(event.pressed == 4 && event.released == 3);
  mouse_buttons = 0;
  assert(poll(&event) == 1 && event.released == 4);
  mouse_x = 32000;
  mouse_y = 32000;
  assert(poll(&event) == 1 && event.x == 799 && event.y == 599);
  mouse_x = 0xffff;
  mouse_y = 0x8000;
  assert(poll(&event) == 1 && event.x == 0 && event.y == 0);
  assert(dos_input_pointer(&x, &y) == 0 && x == 0 && y == 0);
  dos_input_shutdown();
  assert(dos_input_init(800, 600) == 0);
  assert(poll(&event) == 0); /* Previous pointer/buttons do not leak. */
}

static void test_fairness(void)
{
  struct dos_input_event event;
  int i;

  clean_services();
  assert(dos_input_init(640, 480) == 0);
  keys = 1000;
  key_ascii = 'a';
  key_scan = 0x1e;
  for (i = 0; i < 100; i++) {
    mouse_x = 100 + i;
    mouse_buttons = i & 1;
    assert(poll(&event) == 1 && event.type == DOS_INPUT_KEY);
    assert(poll(&event) == 1 && event.type == DOS_INPUT_POINTER);
  }
  assert(keys == 900);
  /* An unchanged preferred mouse must not delay a pending key. */
  assert(poll(&event) == 1 && event.type == DOS_INPUT_KEY);
  assert(poll(&event) == 1 && event.type == DOS_INPUT_KEY);
}

static void latch_click(unsigned int button, unsigned int presses,
                        unsigned int releases, unsigned int x, unsigned int y)
{
  assert(button < 3);
  latched[0][button] += presses;
  latched[1][button] += releases;
  latch_x[0][button] = x;
  latch_y[0][button] = y;
  latch_x[1][button] = x + 1;
  latch_y[1][button] = y + 1;
}

static void test_latched_clicks(void)
{
  struct dos_input_event event;
  unsigned int button, mask;
  int x, y, i;

  clean_services();
  assert(dos_input_init(800, 600) == 0);
  for (button = 0; button < 3; button++) {
    mask = 1U << button;
    latch_click(button, 1, 1, 100 + button, 200 + button);
    /* The pointer can move after the short click; use its press location. */
    mouse_x = 700;
    mouse_y = 500;
    mouse_buttons = 0;
    assert(poll(&event) == 1 && event.type == DOS_INPUT_POINTER);
    assert(event.pressed == mask && event.released == mask && !event.buttons);
    assert(event.x == (int)(100 + button) && event.y == (int)(200 + button));
    assert(dos_input_pointer(&x, &y) == 0 && x == 700 && y == 500);
    assert(poll(&event) == 0);
  }
  /* Held buttons report only the first edge, then a single release. */
  latch_click(0, 1, 0, 123, 234);
  mouse_buttons = DOS_INPUT_LEFT;
  assert(poll(&event) == 1 && event.pressed == DOS_INPUT_LEFT);
  assert(event.buttons == DOS_INPUT_LEFT && !event.released);
  for (i = 0; i < 32; i++) {
    assert(poll(&event) == 0);
  }
  latch_click(0, 0, 1, 123, 234);
  mouse_buttons = 0;
  assert(poll(&event) == 1 && event.released == DOS_INPUT_LEFT);
  assert(!event.pressed && !event.buttons);
  assert(poll(&event) == 0);
  /* All counter occurrences survive a polling gap, not just a boolean. */
  latch_click(1, 3, 3, 456, 345);
  for (i = 0; i < 3; i++) {
    assert(poll(&event) == 1 && event.pressed == DOS_INPUT_RIGHT);
    assert(event.released == DOS_INPUT_RIGHT && !event.buttons);
    assert(event.x == 456 && event.y == 345);
  }
  assert(poll(&event) == 0);
  latch_click(0, 1, 1, 32000, 0xffff);
  assert(poll(&event) == 1 && event.x == 799 && event.y == 0);
  assert(poll(&event) == 0);
  /* One pending keyboard event between each queued click ensures fairness. */
  latch_click(0, 4, 4, 17, 18);
  keys = 20;
  for (i = 0; i < 4; i++) {
    assert(poll(&event) == 1 && event.type == DOS_INPUT_KEY);
    assert(poll(&event) == 1 && event.type == DOS_INPUT_POINTER);
    assert(event.pressed == DOS_INPUT_LEFT && event.released == DOS_INPUT_LEFT);
  }
  assert(keys == 16);
  clean_services();
  driver_buttons = 2;
  /* Stale counters from before init are discarded, not delivered. */
  latch_click(0, 2, 2, 10, 20);
  assert(dos_input_init(640, 480) == 0);
  assert(poll(&event) == 0);
  assert(counter_calls[0][2] == 0 && counter_calls[1][2] == 0);
  latch_click(1, 1, 1, 44, 55);
  assert(poll(&event) == 1 && event.pressed == DOS_INPUT_RIGHT);
  assert(event.released == DOS_INPUT_RIGHT && event.x == 44 && event.y == 55);
  clean_services();
  driver_buttons = 1;
  assert(dos_input_init(640, 480) == 0);
  latch_click(0, 1, 1, 22, 33);
  assert(poll(&event) == 1 && event.pressed == DOS_INPUT_LEFT);
  assert(counter_calls[0][1] == 0 && counter_calls[1][1] == 0);
}

static void test_counter_failures(void)
{
  struct dos_input_event event, unchanged;
  int function, button, x, y;

  for (function = 5; function <= 6; function++) {
    for (button = 0; button < 3; button++) {
      clean_services();
      assert(dos_input_init(640, 480) == 0);
      latch_click(0, 2, 2, 11, 12);
      latch_click(2, 1, 1, 33, 34);
      fail_vector = 0x33;
      fail_function = function;
      fail_button = button;
      memset(&event, 0xa5, sizeof(event));
      unchanged = event;
      assert(poll(&event) == -1);
      assert(memcmp(&event, &unchanged, sizeof(event)) == 0);
      assert(dos_input_pointer(&x, &y) == 0 && x == 320 && y == 240);
      fail_vector = -1;
      assert(poll(&event) == 1 && event.pressed == 5 && event.released == 5);
      assert(event.x == 11 && event.y == 12);
      assert(poll(&event) == 1 && event.pressed == 1 && event.released == 1);
      assert(poll(&event) == 0);
    }
  }
  for (function = 0; function < 2; function++) {
    clean_services();
    assert(dos_input_init(640, 480) == 0);
    latch_click(0, 1, 1, 71, 72);
    fail_vector = function == 0 ? 0x33 : 0x16;
    fail_function = function == 0 ? 3 : 0x1200;
    assert(poll(&event) == -1);
    fail_vector = -1;
    assert(poll(&event) == 1 && event.pressed == 1 && event.released == 1);
    assert(event.x == 71 && event.y == 72);
    assert(poll(&event) == 0);
  }
  clean_services();
  assert(dos_input_init(640, 480) == 0);
  latch_click(0, 3, 3, 71, 72);
  assert(poll(&event) == 1);
  dos_input_shutdown();
  assert(dos_input_init(640, 480) == 0);
  assert(poll(&event) == 0);
}

static void test_failures(void)
{
  static const int init_functions[] = { 0, 2, 7, 8, 4, 3, 5, 6 };
  static const int key_functions[] = { 0x1100, 0x1000, 0x1200 };
  struct dos_input_event event, unchanged;
  unsigned int i;
  int x, y;

  clean_services();
  vector_error = 1;
  assert(dos_input_init(640, 480) == -1 && calls == 0);
  assert(!dos_input_mouse_available());
  for (i = 0; i < sizeof(init_functions) / sizeof(init_functions[0]); i++) {
    clean_services();
    fail_vector = 0x33;
    fail_function = init_functions[i];
    assert(dos_input_init(640, 480) == -1);
    assert(!dos_input_mouse_available());
    assert(dos_input_pointer(&x, &y) == -1);
  }
  memset(&event, 0xa5, sizeof(event));
  unchanged = event;
  for (i = 0; i < sizeof(key_functions) / sizeof(key_functions[0]); i++) {
    clean_services();
    assert(dos_input_init(640, 480) == 0);
    keys = 1;
    fail_vector = 0x16;
    fail_function = key_functions[i];
    assert(poll(&event) == -1);
    assert(memcmp(&event, &unchanged, sizeof(event)) == 0);
  }
  clean_services();
  assert(dos_input_init(640, 480) == 0);
  fail_vector = 0x33;
  fail_function = 3;
  assert(poll(&event) == -1);
  assert(memcmp(&event, &unchanged, sizeof(event)) == 0);
  fail_vector = 0x16;
  fail_function = 0x1200;
  mouse_x = 17;
  mouse_buttons = 1;
  assert(poll(&event) == -1);
  assert(dos_input_pointer(&x, &y) == 0 && x == 320 && y == 240);
  fail_vector = -1;
  assert(poll(&event) == 1 && event.pressed == 1 && event.x == 17);
  fail_vector = 0x33;
  fail_function = 0;
  dos_input_shutdown(); /* Even a cleanup failure clears local state. */
  assert(!dos_input_mouse_available());
  assert(poll(&event) == -1);
}

int main(void)
{
  test_lifecycle();
  test_keyboard_only();
  test_keys();
  test_mouse();
  test_fairness();
  test_latched_clicks();
  test_counter_failures();
  test_failures();
  clean_services();
  puts("DOS input native service-injection tests passed.");
  return 0;
}
