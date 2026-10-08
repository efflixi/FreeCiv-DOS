/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <stdio.h>
#include <string.h>
#include "widgets.h"

#define CELL 12
#define LINE 16
#define PAD 12
#define ROW 24
#define BACK 0x18e3U
#define FACE 0x3186U
#define WHITE 0xffffU
#define FOCUS 0xffe0U
#define ERROR_COLOR 0xf980U

struct widget_state {
  struct dos_widget_spec spec;
  struct dos_widget_rect rect;
  unsigned int selected, top;
  size_t caret, offset;
};

static struct {
  int active, dirty, focus, capture;
  unsigned int width, height, count;
  const char *title, *body;
  struct dos_widget_rect panel;
  int body_lines;
  char status[DOS_WIDGETS_LABEL_MAX], error[DOS_WIDGETS_LABEL_MAX];
  struct widget_state widgets[DOS_WIDGETS_MAX];
} dialog;

static int invalid(const char *message)
{
  fprintf(stderr, "DOS VBE widgets: %s.\n", message);
  return DOS_WIDGETS_ERROR;
}

static int valid_string(const char *s, size_t limit, int multiline)
{
  size_t i;
  if (!s) {
    return 0;
  }
  for (i = 0; i < limit; i++) {
    unsigned char c = (unsigned char)s[i];
    if (!c) {
      return 1;
    }
    if ((c < 32U || c > 126U) && !(multiline && c == '\n')) {
      return 0;
    }
  }
  return 0;
}

static int body_rows(const char *body, int columns)
{
  int rows = 1, column = 0;
  while (*body) {
    if (*body++ == '\n') {
      rows++;
      column = 0;
    } else {
      if (column == columns) {
        rows++;
        column = 0;
      }
      column++;
    }
  }
  return rows;
}

int dos_widgets_begin(const struct dos_vbe_framebuffer *fb,
                      const char *title, const char *body,
                      const struct dos_widget_spec *spec, unsigned int count)
{
  unsigned int i, j;
  int width, height, body_lines, y;
  if (dialog.active) {
    return invalid("a dialog is already active");
  }
  if (dos_vbe_framebuffer_validate(fb) < 0) {
    return invalid("invalid target framebuffer");
  }
  if (fb->width < 320U || fb->height < 240U
      || fb->width > 16384U || fb->height > 16384U
      || !spec || !count || count > DOS_WIDGETS_MAX
      || !valid_string(title, DOS_WIDGETS_LABEL_MAX, 0)
      || !valid_string(body, DOS_WIDGETS_BODY_MAX, 1)) {
    return invalid("invalid dimensions, count, title or body");
  }
  width = (int)fb->width - 32;
  if (width > 720) {
    width = 720;
  }
  body_lines = body_rows(body, (width - 2 * PAD) / CELL);
  height = 48 + body_lines * LINE + 56;
  for (i = 0; i < count; i++) {
    const struct dos_widget_spec *s = &spec[i];
    if (!valid_string(s->label, DOS_WIDGETS_LABEL_MAX, 0)
        || s->action < 0 || s->type < DOS_WIDGET_BUTTON
        || s->type > DOS_WIDGET_TEXT) {
      return invalid("invalid widget type, label or action");
    }
    if (s->type == DOS_WIDGET_BUTTON && !s->action) {
      return invalid("buttons require a positive action");
    }
    if (s->type == DOS_WIDGET_LIST) {
      if (!s->items || !s->item_count
          || s->item_count > DOS_WIDGETS_ITEMS_MAX
          || s->selected >= s->item_count || !s->rows || s->rows > 6U) {
        return invalid("invalid list limits or selection");
      }
      for (j = 0; j < s->item_count; j++) {
        if (!valid_string(s->items[j], DOS_WIDGETS_LABEL_MAX, 0)) {
          return invalid("invalid list item");
        }
      }
      height += LINE + ((int)s->rows + 1) * ROW + 8;
    } else if (s->type == DOS_WIDGET_TEXT) {
      if (!s->capacity || s->capacity > DOS_WIDGETS_TEXT_MAX
          || !valid_string(s->text, s->capacity, 0)) {
        return invalid("invalid bounded text buffer");
      }
      height += LINE + ROW + 8;
    } else {
      height += ROW + 8;
    }
  }
  if (height > (int)fb->height - 24) {
    return invalid("dialog contents do not fit the framebuffer");
  }
  memset(&dialog, 0, sizeof(dialog));
  dialog.width = fb->width;
  dialog.height = fb->height;
  dialog.title = title;
  dialog.body = body;
  dialog.body_lines = body_lines;
  dialog.count = count;
  dialog.capture = -1;
  dialog.panel.x = ((int)fb->width - width) / 2;
  dialog.panel.y = ((int)fb->height - height) / 2;
  dialog.panel.width = width;
  dialog.panel.height = height;
  y = dialog.panel.y + 48 + body_lines * LINE;
  for (i = 0; i < count; i++) {
    struct widget_state *w = &dialog.widgets[i];
    w->spec = spec[i];
    w->selected = spec[i].selected;
    w->rect.x = dialog.panel.x + PAD;
    w->rect.y = y;
    w->rect.width = width - 2 * PAD;
    w->rect.height = ROW;
    if (spec[i].type == DOS_WIDGET_LIST) {
      w->rect.height = LINE + ((int)spec[i].rows + 1) * ROW;
      w->top = w->selected / spec[i].rows * spec[i].rows;
    } else if (spec[i].type == DOS_WIDGET_TEXT) {
      w->rect.height += LINE;
      w->caret = strlen(spec[i].text);
    }
    y += w->rect.height + 8;
  }
  dialog.active = dialog.dirty = 1;
  return 0;
}

void dos_widgets_close(void)
{
  memset(&dialog, 0, sizeof(dialog));
}

int dos_widgets_active(void) { return dialog.active; }
int dos_widgets_dirty(void) { return dialog.active && dialog.dirty; }
int dos_widgets_focus(void) { return dialog.active ? dialog.focus : -1; }

void dos_widgets_invalidate(void)
{
  if (dialog.active) {
    dialog.dirty = 1;
  }
}

int dos_widgets_selection(unsigned int widget)
{
  if (!dialog.active || widget >= dialog.count
      || dialog.widgets[widget].spec.type != DOS_WIDGET_LIST) {
    return invalid("selection requested for a non-list");
  }
  return (int)dialog.widgets[widget].selected;
}

const char *dos_widgets_text(unsigned int widget)
{
  if (!dialog.active || widget >= dialog.count
      || dialog.widgets[widget].spec.type != DOS_WIDGET_TEXT) {
    invalid("text requested for a non-text widget");
    return NULL;
  }
  return dialog.widgets[widget].spec.text;
}

int dos_widgets_bounds(unsigned int widget, struct dos_widget_rect *rect)
{
  if (!dialog.active || widget >= dialog.count || !rect) {
    return invalid("invalid bounds request");
  }
  *rect = dialog.widgets[widget].rect;
  return 0;
}

int dos_widgets_feedback(const char *status, const char *error)
{
  if (!status) { status = ""; }
  if (!error) { error = ""; }
  if (!dialog.active || !valid_string(status, DOS_WIDGETS_LABEL_MAX, 0)
      || !valid_string(error, DOS_WIDGETS_LABEL_MAX, 0)) {
    return invalid("invalid feedback");
  }
  if (strcmp(status, dialog.status)) {
    strcpy(dialog.status, status);
    dialog.dirty = 1;
  }
  if (strcmp(error, dialog.error)) {
    strcpy(dialog.error, error);
    dialog.dirty = 1;
  }
  return 0;
}

static int contains(const struct dos_widget_rect *r, int x, int y)
{
  return x >= r->x && y >= r->y
    && x - r->x < r->width && y - r->y < r->height;
}

static void select_item(struct widget_state *w, unsigned int selected)
{
  if (selected != w->selected) {
    w->selected = selected;
    if (selected < w->top) {
      w->top = selected;
    } else if (selected - w->top >= w->spec.rows) {
      w->top = selected - w->spec.rows + 1U;
    }
    dialog.dirty = 1;
  }
}

static void page_list(struct widget_state *w, int forward)
{
  unsigned int n = w->selected;
  if (forward) {
    n += w->spec.rows;
    if (n >= w->spec.item_count) { n = w->spec.item_count - 1U; }
  } else {
    n = n < w->spec.rows ? 0U : n - w->spec.rows;
  }
  select_item(w, n);
}

static void text_offset(struct widget_state *w)
{
  size_t cells = (size_t)(w->rect.width - 12) / CELL;
  if (w->caret < w->offset) { w->offset = w->caret; }
  if (w->caret - w->offset >= cells) {
    w->offset = w->caret - cells + 1U;
  }
}

int dos_widgets_event(const struct dos_input_event *event)
{
  struct widget_state *w;
  unsigned int i, ascii, scan;
  size_t length, old_caret;
  if (!event || (event->type != DOS_INPUT_KEY
                 && event->type != DOS_INPUT_POINTER)) {
    return invalid("invalid input event");
  }
  if (!dialog.active) { return DOS_WIDGETS_PENDING; }
  if (event->type == DOS_INPUT_POINTER) {
    if (event->pressed & DOS_INPUT_LEFT) {
      dialog.capture = -1;
      for (i = 0; i < dialog.count; i++) {
        w = &dialog.widgets[i];
        if (!contains(&w->rect, event->x, event->y)) { continue; }
        if (dialog.focus != (int)i) {
          dialog.focus = (int)i;
          dialog.dirty = 1;
        }
        if (w->spec.type == DOS_WIDGET_BUTTON) {
          dialog.capture = (int)i;
        } else if (w->spec.type == DOS_WIDGET_LIST
                   && event->y >= w->rect.y + LINE) {
          unsigned int row = (unsigned int)(event->y - w->rect.y - LINE) / ROW;
          if (row == w->spec.rows) {
            page_list(w, event->x >= w->rect.x + w->rect.width / 2);
          } else if (w->top + row < w->spec.item_count) {
            select_item(w, w->top + row);
          }
        } else if (w->spec.type == DOS_WIDGET_TEXT) {
          text_offset(w);
          length = strlen(w->spec.text);
          old_caret = w->caret;
          w->caret = w->offset + (size_t)(event->x - w->rect.x) / CELL;
          if (w->caret > length) { w->caret = length; }
          if (old_caret != w->caret) { dialog.dirty = 1; }
          text_offset(w);
        }
        break;
      }
    }
    if (event->released & DOS_INPUT_LEFT) {
      int capture = dialog.capture;
      dialog.capture = -1;
      if (capture >= 0) {
        w = &dialog.widgets[capture];
        if (contains(&w->rect, event->x, event->y)) { return w->spec.action; }
      }
    }
    return DOS_WIDGETS_PENDING;
  }
  dialog.capture = -1;
  ascii = event->ascii;
  scan = event->scan;
  if (ascii == 27U || scan == 1U) { return DOS_WIDGETS_CANCEL; }
  if (ascii == 9U || scan == 15U) {
    int next = dialog.focus + ((event->modifiers & DOS_INPUT_SHIFT) ? -1 : 1);
    if (next < 0) { next = (int)dialog.count - 1; }
    if (next >= (int)dialog.count) { next = 0; }
    if (next != dialog.focus) { dialog.dirty = 1; }
    dialog.focus = next;
    return DOS_WIDGETS_PENDING;
  }
  w = &dialog.widgets[dialog.focus];
  if (ascii == 13U || scan == 28U
      || (w->spec.type == DOS_WIDGET_BUTTON && ascii == 32U)) {
    return w->spec.action;
  }
  if (w->spec.type == DOS_WIDGET_LIST) {
    if (scan == 72U && w->selected) { select_item(w, w->selected - 1U); }
    if (scan == 80U && w->selected + 1U < w->spec.item_count) {
      select_item(w, w->selected + 1U);
    }
    if (scan == 71U) { select_item(w, 0U); }
    if (scan == 79U) { select_item(w, w->spec.item_count - 1U); }
    if (scan == 73U) { page_list(w, 0); }
    if (scan == 81U) { page_list(w, 1); }
  } else if (w->spec.type == DOS_WIDGET_TEXT) {
    length = strlen(w->spec.text);
    old_caret = w->caret;
    if (scan == 75U && w->caret) { w->caret--; }
    else if (scan == 77U && w->caret < length) { w->caret++; }
    else if (scan == 71U) { w->caret = 0; }
    else if (scan == 79U) { w->caret = length; }
    else if ((ascii == 8U || scan == 14U) && w->caret) {
      memmove(w->spec.text + w->caret - 1U, w->spec.text + w->caret,
              length - w->caret + 1U);
      w->caret--;
      dialog.dirty = 1;
    } else if (scan == 83U && w->caret < length) {
      memmove(w->spec.text + w->caret, w->spec.text + w->caret + 1U,
              length - w->caret);
      dialog.dirty = 1;
    } else if (ascii >= 32U && ascii <= 126U
               && !(event->modifiers & (DOS_INPUT_CTRL | DOS_INPUT_ALT))) {
      if (length + 1U >= w->spec.capacity) {
        return dos_widgets_feedback(dialog.status, "Text buffer is full");
      }
      memmove(w->spec.text + w->caret + 1U, w->spec.text + w->caret,
              length - w->caret + 1U);
      w->spec.text[w->caret++] = (char)ascii;
      dialog.dirty = 1;
    }
    if (old_caret != w->caret) { dialog.dirty = 1; }
    text_offset(w);
  }
  return DOS_WIDGETS_PENDING;
}

static void fill(struct dos_vbe_framebuffer *fb, int x, int y,
                 int width, int height, unsigned int color)
{
  dos_vbe_framebuffer_fill_rect(fb, x, y, width, height, color);
}

static void outline(struct dos_vbe_framebuffer *fb,
                    const struct dos_widget_rect *r, unsigned int color)
{
  fill(fb, r->x, r->y, r->width, 1, color);
  fill(fb, r->x, r->y + r->height - 1, r->width, 1, color);
  fill(fb, r->x, r->y, 1, r->height, color);
  fill(fb, r->x + r->width - 1, r->y, 1, r->height, color);
}

static void text(struct dos_vbe_framebuffer *fb, int x, int y,
                 int columns, const char *s, unsigned int color)
{
  char line[DOS_WIDGETS_LABEL_MAX];
  int n = 0;
  while (*s && *s != '\n' && n < columns
         && n < (int)sizeof(line) - 1) { line[n++] = *s++; }
  line[n] = '\0';
  dos_vbe_framebuffer_text(fb, x, y, line, color, 2U);
}

int dos_widgets_draw(struct dos_vbe_framebuffer *fb)
{
  unsigned int i, row;
  int columns, x, y, column;
  const char *s;
  char line[DOS_WIDGETS_LABEL_MAX];
  if (!dialog.active) { return 0; }
  if (dos_vbe_framebuffer_validate(fb) < 0
      || fb->width != dialog.width || fb->height != dialog.height) {
    return invalid("framebuffer dimensions changed during dialog");
  }
  if (!dialog.dirty) { return 0; }
  fill(fb, dialog.panel.x, dialog.panel.y,
       dialog.panel.width, dialog.panel.height, BACK);
  outline(fb, &dialog.panel, WHITE);
  x = dialog.panel.x + PAD;
  y = dialog.panel.y + PAD;
  columns = (dialog.panel.width - 2 * PAD) / CELL;
  text(fb, x, y, columns, dialog.title, FOCUS);
  y += 28;
  s = dialog.body;
  do {
    column = 0;
    while (*s && *s != '\n' && column < columns) { line[column++] = *s++; }
    line[column] = '\0';
    text(fb, x, y, columns, line, WHITE);
    y += LINE;
    if (*s == '\n') { s++; }
  } while (*s);
  for (i = 0; i < dialog.count; i++) {
    struct widget_state *w = &dialog.widgets[i];
    struct dos_widget_rect *r = &w->rect;
    unsigned int color = (int)i == dialog.focus ? FOCUS : WHITE;
    fill(fb, r->x, r->y, r->width, r->height, FACE);
    outline(fb, r, color);
    text(fb, r->x + 6, r->y + 1, columns - 1, w->spec.label, color);
    if (w->spec.type == DOS_WIDGET_LIST) {
      for (row = 0; row < w->spec.rows
                    && w->top + row < w->spec.item_count; row++) {
        y = r->y + LINE + (int)row * ROW;
        if (w->top + row == w->selected) {
          fill(fb, r->x + 2, y, r->width - 4, ROW, BACK);
        }
        text(fb, r->x + 6, y + 4, columns - 1,
             w->spec.items[w->top + row],
             w->top + row == w->selected ? FOCUS : WHITE);
      }
      y = r->y + LINE + (int)w->spec.rows * ROW;
      text(fb, r->x + 6, y + 4, columns / 2 - 1, "Prev [PgUp]", WHITE);
      text(fb, r->x + r->width / 2 + 6, y + 4,
           columns / 2 - 1, "Next [PgDn]", WHITE);
    } else if (w->spec.type == DOS_WIDGET_TEXT) {
      text_offset(w);
      text(fb, r->x + 6, r->y + LINE + 4, columns - 1,
           w->spec.text + w->offset, WHITE);
      if ((int)i == dialog.focus) {
        fill(fb, r->x + 6 + (int)(w->caret - w->offset) * CELL,
             r->y + LINE + 19, CELL - 2, 2, FOCUS);
      }
    }
  }
  y = dialog.panel.y + dialog.panel.height - 48;
  text(fb, x, y, columns, dialog.status, WHITE);
  text(fb, x, y + LINE, columns, dialog.error, ERROR_COLOR);
  text(fb, x, y + 2 * LINE, columns, "Tab: focus  Enter: activate  Esc: cancel", WHITE);
  dialog.dirty = 0;
  return 1;
}
