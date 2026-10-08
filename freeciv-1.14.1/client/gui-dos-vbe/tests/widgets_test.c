/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "widgets.h"

static const char *const items[] = {
  "First", "Second", "Third", "Fourth", "Fifth", "Sixth", "Seventh"
};

static int key(unsigned int ascii, unsigned int scan, unsigned int modifiers)
{
  struct dos_input_event e;
  memset(&e, 0, sizeof(e));
  e.type = DOS_INPUT_KEY;
  e.ascii = ascii;
  e.scan = scan;
  e.modifiers = modifiers;
  return dos_widgets_event(&e);
}

static int pointer(int x, int y, unsigned int pressed, unsigned int released)
{
  struct dos_input_event e;
  memset(&e, 0, sizeof(e));
  e.type = DOS_INPUT_POINTER;
  e.x = x;
  e.y = y;
  e.pressed = pressed;
  e.released = released;
  return dos_widgets_event(&e);
}

static void specs(struct dos_widget_spec *s, char *buffer, size_t capacity)
{
  memset(s, 0, sizeof(*s) * 4U);
  s[0].type = DOS_WIDGET_LIST;
  s[0].label = "Choose";
  s[0].items = items;
  s[0].item_count = 7U;
  s[0].rows = 3U;
  s[0].action = 10;
  s[1].type = DOS_WIDGET_TEXT;
  s[1].label = "Name";
  s[1].text = buffer;
  s[1].capacity = capacity;
  s[1].action = 11;
  s[2].type = DOS_WIDGET_BUTTON;
  s[2].label = "Accept";
  s[2].action = 12;
  s[3].type = DOS_WIDGET_BUTTON;
  s[3].label = "Cancel";
  s[3].action = INT_MAX;
}

static void no_op(struct dos_vbe_framebuffer *fb)
{
  unsigned int i;
  unsigned char *copy;
  assert(dos_widgets_draw(fb) == 1);
  assert(dos_widgets_dirty() == 0);
  assert(dos_vbe_framebuffer_dirty_clear(fb) == 0);
  copy = malloc(fb->size_bytes);
  assert(copy);
  memcpy(copy, fb->pixels, fb->size_bytes);
  for (i = 0; i < 100U; i++) {
    assert(pointer(INT_MIN, INT_MAX, 0U, 0U) == 0);
    assert(key(0U, 0U, 0U) == 0);
    assert(dos_widgets_draw(fb) == 0);
    assert(fb->dirty == 0);
    assert(memcmp(copy, fb->pixels, fb->size_bytes) == 0);
  }
  dos_widgets_invalidate();
  assert(dos_widgets_dirty());
  assert(dos_widgets_draw(fb) == 1);
  assert(!dos_widgets_dirty());
  assert(fb->dirty);
  assert(memcmp(copy, fb->pixels, fb->size_bytes) == 0);
  assert(dos_vbe_framebuffer_dirty_clear(fb) == 0);
  assert(dos_widgets_draw(fb) == 0);
  assert(!fb->dirty);
  free(copy);
}

static void behavior(unsigned int width, unsigned int height)
{
  struct dos_vbe_framebuffer fb;
  struct dos_widget_spec s[4], original[4];
  struct dos_widget_rect r;
  unsigned int i, y;
  char buffer[8] = "abc";
  memset(&fb, 0, sizeof(fb));
  assert(dos_vbe_framebuffer_init_pitch(&fb, width, height, 16U,
                                      width * 2U + 8U) == 0);
  memset(fb.pixels, 0xa5, fb.size_bytes);
  specs(s, buffer, sizeof(buffer));
  memcpy(original, s, sizeof(s));
  assert(dos_widgets_begin(&fb, "Options", "Select an item.\nEdit your name.",
                           s, 4U) == 0);
  assert(dos_widgets_active() && dos_widgets_dirty());
  assert(dos_widgets_focus() == 0);
  assert(dos_widgets_begin(&fb, "Other", "", s, 4U) == -1);
  no_op(&fb);
  assert(fb.pixels[0] == 0xa5);
  for (y = 0; y < height; y++) {
    for (i = width * 2U; i < fb.stride; i++) {
      assert(fb.pixels[(size_t)y * fb.stride + i] == 0xa5);
    }
  }
  assert(key(0U, 72U, 0U) == 0);
  assert(!dos_widgets_dirty());
  assert(key(0U, 80U, 0U) == 0);
  assert(dos_widgets_selection(0U) == 1);
  assert(key(0U, 81U, 0U) == 0);
  assert(dos_widgets_selection(0U) == 4);
  assert(key(0U, 79U, 0U) == 0);
  assert(dos_widgets_selection(0U) == 6);
  assert(key(0U, 80U, 0U) == 0);
  assert(dos_widgets_selection(0U) == 6);
  assert(key(0U, 73U, 0U) == 0);
  assert(dos_widgets_selection(0U) == 3);
  assert(key(0U, 71U, 0U) == 0);
  assert(dos_widgets_selection(0U) == 0);
  assert(dos_widgets_bounds(0U, &r) == 0);
  assert(pointer(r.x + 8, r.y + 16 + 24 + 4, DOS_INPUT_LEFT, 0U) == 0);
  assert(dos_widgets_selection(0U) == 1);
  assert(pointer(r.x + r.width - 8, r.y + r.height - 8,
                 DOS_INPUT_LEFT, 0U) == 0);
  assert(dos_widgets_selection(0U) == 4);
  assert(pointer(r.x + 8, r.y + r.height - 8, DOS_INPUT_LEFT, 0U) == 0);
  assert(dos_widgets_selection(0U) == 1);
  assert(key(13U, 28U, 0U) == 10);
  assert(key(9U, 15U, 0U) == 0);
  assert(dos_widgets_focus() == 1);
  assert(key('d', 32U, 0U) == 0 && !strcmp(buffer, "abcd"));
  assert(key(0U, 75U, 0U) == 0);
  assert(key('X', 45U, 0U) == 0 && !strcmp(buffer, "abcXd"));
  assert(key(8U, 14U, 0U) == 0 && !strcmp(buffer, "abcd"));
  assert(key(0U, 83U, 0U) == 0 && !strcmp(buffer, "abc"));
  assert(key(0U, 71U, 0U) == 0);
  assert(key('!', 2U, 0U) == 0 && !strcmp(buffer, "!abc"));
  assert(key(0U, 79U, 0U) == 0);
  assert(key('a', 30U, DOS_INPUT_CTRL) == 0);
  assert(key('b', 48U, DOS_INPUT_ALT) == 0);
  assert(key(200U, 0U, 0U) == 0 && !strcmp(buffer, "!abc"));
  assert(key('1', 2U, 0U) == 0);
  assert(key('2', 3U, 0U) == 0);
  assert(key('3', 4U, 0U) == 0);
  assert(key('4', 5U, 0U) == 0 && !strcmp(buffer, "!abc123"));
  assert(strlen(buffer) == sizeof(buffer) - 1U);
  assert(dos_widgets_text(1U) == buffer);
  assert(key(13U, 28U, 0U) == 11);
  assert(dos_widgets_bounds(1U, &r) == 0);
  assert(pointer(r.x, r.y + 20, DOS_INPUT_LEFT, 0U) == 0);
  assert(key(0U, 83U, 0U) == 0 && !strcmp(buffer, "abc123"));
  assert(key(9U, 15U, DOS_INPUT_SHIFT) == 0);
  assert(dos_widgets_focus() == 0);
  assert(key(9U, 15U, DOS_INPUT_SHIFT) == 0);
  assert(dos_widgets_focus() == 3);
  assert(key(13U, 28U, 0U) == INT_MAX);
  assert(key(32U, 57U, 0U) == INT_MAX);
  assert(key(9U, 15U, 0U) == 0 && dos_widgets_focus() == 0);
  assert(dos_widgets_bounds(2U, &r) == 0);
  assert(pointer(r.x + 8, r.y + 8, 0U, DOS_INPUT_LEFT) == 0);
  assert(pointer(r.x + 8, r.y + 8, DOS_INPUT_LEFT, 0U) == 0);
  assert(dos_widgets_focus() == 2);
  assert(pointer(INT_MIN, INT_MAX, 0U, DOS_INPUT_LEFT) == 0);
  assert(pointer(r.x + 8, r.y + 8, DOS_INPUT_LEFT, 0U) == 0);
  assert(pointer(r.x + 8, r.y + 8, 0U, DOS_INPUT_LEFT) == 12);
  assert(pointer(r.x, r.y, DOS_INPUT_RIGHT, DOS_INPUT_RIGHT) == 0);
  assert(key(13U, 28U, 0U) == 12);
  assert(dos_widgets_feedback("Saved", "Example error") == 0);
  no_op(&fb);
  assert(dos_widgets_feedback("Saved", "Example error") == 0);
  assert(!dos_widgets_dirty());
  assert(dos_widgets_feedback(NULL, NULL) == 0);
  assert(dos_widgets_dirty());
  assert(key(27U, 1U, 0U) == DOS_WIDGETS_CANCEL);
  assert(dos_widgets_active());
  assert(memcmp(original, s, sizeof(s)) == 0);
  dos_widgets_close();
  assert(!dos_widgets_active() && !dos_widgets_dirty());
  dos_widgets_invalidate();
  assert(!dos_widgets_active() && !dos_widgets_dirty());
  assert(dos_vbe_framebuffer_dirty_clear(&fb) == 0);
  assert(dos_widgets_draw(&fb) == 0);
  assert(!fb.dirty);
  assert(key(13U, 28U, 0U) == 0);
  dos_widgets_close();
  dos_vbe_framebuffer_destroy(&fb);
}

static void failures(void)
{
  struct dos_vbe_framebuffer fb, bad;
  struct dos_widget_spec s[4];
  struct dos_input_event event;
  char buffer[8] = "ok";
  char too_long[DOS_WIDGETS_BODY_MAX + 1U];
  memset(&fb, 0, sizeof(fb));
  specs(s, buffer, sizeof(buffer));
  assert(dos_widgets_begin(NULL, "Title", "", s, 4U) == -1);
  assert(dos_vbe_framebuffer_init(&fb, 640U, 480U, 16U) == 0);
  bad = fb;
  bad.width = 0U;
  assert(dos_widgets_begin(&bad, "Title", "", s, 4U) == -1);
  bad = fb;
  bad.size_bytes--;
  assert(dos_widgets_begin(&bad, "Title", "", s, 4U) == -1);
  assert(dos_widgets_begin(&fb, "Title", "", NULL, 4U) == -1);
  assert(dos_widgets_begin(&fb, "Title", "", s, 0U) == -1);
  assert(dos_widgets_begin(&fb, "Title", "", s, DOS_WIDGETS_MAX + 1U) == -1);
  assert(dos_widgets_begin(&fb, NULL, "", s, 4U) == -1);
  assert(dos_widgets_begin(&fb, "Title", NULL, s, 4U) == -1);
  assert(dos_widgets_begin(&fb, "Title", "\001", s, 4U) == -1);
  memset(too_long, 'a', sizeof(too_long));
  too_long[sizeof(too_long) - 1U] = '\0';
  assert(dos_widgets_begin(&fb, "Title", too_long, s, 4U) == -1);
  s[0].rows = 7U;
  assert(dos_widgets_begin(&fb, "Title", "", s, 4U) == -1);
  s[0].rows = 3U;
  s[0].selected = 7U;
  assert(dos_widgets_begin(&fb, "Title", "", s, 4U) == -1);
  s[0].selected = 0U;
  s[0].item_count = DOS_WIDGETS_ITEMS_MAX + 1U;
  assert(dos_widgets_begin(&fb, "Title", "", s, 4U) == -1);
  s[0].item_count = 7U;
  s[0].items = NULL;
  assert(dos_widgets_begin(&fb, "Title", "", s, 4U) == -1);
  s[0].items = items;
  s[1].capacity = 0U;
  assert(dos_widgets_begin(&fb, "Title", "", s, 4U) == -1);
  s[1].capacity = DOS_WIDGETS_TEXT_MAX + 1U;
  assert(dos_widgets_begin(&fb, "Title", "", s, 4U) == -1);
  s[1].capacity = sizeof(buffer);
  memset(buffer, 'x', sizeof(buffer));
  assert(dos_widgets_begin(&fb, "Title", "", s, 4U) == -1);
  strcpy(buffer, "ok");
  s[1].text = NULL;
  assert(dos_widgets_begin(&fb, "Title", "", s, 4U) == -1);
  s[1].text = buffer;
  s[2].action = 0;
  assert(dos_widgets_begin(&fb, "Title", "", s, 4U) == -1);
  s[2].action = -2;
  assert(dos_widgets_begin(&fb, "Title", "", s, 4U) == -1);
  s[2].action = 12;
  s[2].type = (enum dos_widget_type)99;
  assert(dos_widgets_begin(&fb, "Title", "", s, 4U) == -1);
  s[2].type = DOS_WIDGET_BUTTON;
  s[2].label = "bad\nlabel";
  assert(dos_widgets_begin(&fb, "Title", "", s, 4U) == -1);
  s[2].label = "Accept";
  assert(!dos_widgets_active());
  assert(dos_widgets_begin(&fb, "Title", "", s, 4U) == 0);
  assert(dos_widgets_event(NULL) == -1);
  memset(&event, 0, sizeof(event));
  event.type = (enum dos_input_type)99;
  assert(dos_widgets_event(&event) == -1);
  assert(dos_widgets_bounds(4U, NULL) == -1);
  assert(dos_widgets_selection(1U) == -1);
  assert(dos_widgets_text(0U) == NULL);
  assert(dos_widgets_feedback("bad\nstatus", "") == -1);
  assert(dos_widgets_feedback("", too_long) == -1);
  bad = fb;
  bad.width = 320U;
  assert(dos_widgets_draw(&bad) == -1);
  assert(dos_widgets_dirty());
  assert(dos_widgets_draw(NULL) == -1);
  assert(dos_widgets_draw(&fb) == 1);
  dos_widgets_close();
  dos_vbe_framebuffer_destroy(&fb);
  assert(dos_vbe_framebuffer_init(&fb, 319U, 240U, 16U) == 0);
  assert(dos_widgets_begin(&fb, "Title", "", s, 1U) == -1);
  dos_vbe_framebuffer_destroy(&fb);
  assert(dos_vbe_framebuffer_init(&fb, 320U, 240U, 16U) == 0);
  assert(dos_widgets_begin(&fb, "Title", "", s, 4U) == -1);
  s[0] = s[2];
  assert(dos_widgets_begin(&fb, "Small", "", s, 1U) == 0);
  no_op(&fb);
  dos_widgets_close();
  dos_vbe_framebuffer_destroy(&fb);
}

static void limits(void)
{
  struct dos_vbe_framebuffer fb;
  struct dos_widget_spec s[DOS_WIDGETS_MAX];
  const char *many_items[DOS_WIDGETS_ITEMS_MAX];
  char buffer[DOS_WIDGETS_TEXT_MAX];
  unsigned int i;
  memset(&fb, 0, sizeof(fb));
  assert(dos_vbe_framebuffer_init(&fb, 640U, 480U, 16U) == 0);
  specs(s, buffer, sizeof(buffer));
  memset(buffer, 'a', sizeof(buffer) - 1U);
  buffer[sizeof(buffer) - 1U] = '\0';
  s[0] = s[1];
  assert(dos_widgets_begin(&fb, "Long text", "", s, 1U) == 0);
  assert(dos_widgets_draw(&fb) == 1);
  assert(key('b', 48U, 0U) == 0);
  assert(strlen(buffer) == sizeof(buffer) - 1U);
  for (i = 0; i < sizeof(buffer) - 1U; i++) {
    assert(key(0U, 75U, 0U) == 0);
  }
  assert(key(0U, 83U, 0U) == 0);
  assert(key('Z', 44U, 0U) == 0);
  assert(buffer[0] == 'Z');
  no_op(&fb);
  dos_widgets_close();
  buffer[0] = '\0';
  s[0].capacity = 1U;
  assert(dos_widgets_begin(&fb, "Empty text", "", s, 1U) == 0);
  assert(key('a', 30U, 0U) == 0 && buffer[0] == '\0');
  assert(key(8U, 14U, 0U) == 0 && buffer[0] == '\0');
  assert(key(0U, 83U, 0U) == 0 && buffer[0] == '\0');
  no_op(&fb);
  dos_widgets_close();
  dos_vbe_framebuffer_destroy(&fb);
  assert(dos_vbe_framebuffer_init(&fb, 800U, 600U, 16U) == 0);
  memset(s, 0, sizeof(s));
  for (i = 0; i < DOS_WIDGETS_MAX; i++) {
    s[i].type = DOS_WIDGET_BUTTON;
    s[i].label = "Command";
    s[i].action = (int)i + 1;
  }
  assert(dos_widgets_begin(&fb, "Commands", "", s, DOS_WIDGETS_MAX) == 0);
  assert(key(9U, 15U, DOS_INPUT_SHIFT) == 0);
  assert(dos_widgets_focus() == (int)DOS_WIDGETS_MAX - 1);
  assert(key(13U, 28U, 0U) == (int)DOS_WIDGETS_MAX);
  no_op(&fb);
  dos_widgets_close();
  for (i = 0; i < DOS_WIDGETS_ITEMS_MAX; i++) { many_items[i] = "Item"; }
  memset(s, 0, sizeof(s));
  s[0].type = DOS_WIDGET_LIST;
  s[0].label = "Maximum list";
  s[0].items = many_items;
  s[0].item_count = DOS_WIDGETS_ITEMS_MAX;
  s[0].selected = DOS_WIDGETS_ITEMS_MAX - 1U;
  s[0].rows = 6U;
  assert(dos_widgets_begin(&fb, "Limits", "", s, 1U) == 0);
  assert(dos_widgets_selection(0U) == (int)DOS_WIDGETS_ITEMS_MAX - 1);
  assert(key(0U, 81U, 0U) == 0);
  assert(dos_widgets_selection(0U) == (int)DOS_WIDGETS_ITEMS_MAX - 1);
  no_op(&fb);
  assert(key(0U, 71U, 0U) == 0);
  assert(dos_widgets_selection(0U) == 0);
  assert(key(0U, 73U, 0U) == 0);
  assert(dos_widgets_selection(0U) == 0);
  no_op(&fb);
  dos_widgets_close();
  s[0].item_count = 1U;
  s[0].selected = 0U;
  s[0].rows = 1U;
  assert(dos_widgets_begin(&fb, "Single item", "", s, 1U) == 0);
  assert(key(0U, 81U, 0U) == 0);
  assert(key(0U, 73U, 0U) == 0);
  assert(key(13U, 28U, 0U) == 0);
  no_op(&fb);
  dos_widgets_close();
  dos_vbe_framebuffer_destroy(&fb);
}

int main(void)
{
  behavior(640U, 480U);
  behavior(800U, 600U);
  failures();
  limits();
  puts("widgets: ASan/UBSan behavior tests passed");
  return 0;
}
