/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef FC__DOS_VBE_WIDGETS_H
#define FC__DOS_VBE_WIDGETS_H

#include <stddef.h>
#include "framebuffer.h"
#include "input.h"

#define DOS_WIDGETS_MAX 12U
#define DOS_WIDGETS_TEXT_MAX 256U
#define DOS_WIDGETS_ITEMS_MAX 1024U
#define DOS_WIDGETS_LABEL_MAX 128U
#define DOS_WIDGETS_BODY_MAX 512U
#define DOS_WIDGETS_PENDING 0
#define DOS_WIDGETS_ERROR (-1)
#define DOS_WIDGETS_CANCEL (-2)

enum dos_widget_type {
  DOS_WIDGET_BUTTON,
  DOS_WIDGET_LIST,
  DOS_WIDGET_TEXT
};

struct dos_widget_spec {
  enum dos_widget_type type;
  const char *label;
  int action;                   /* Positive ID, or 0 for non-activating fields. */
  const char *const *items;      /* List only; borrowed through close. */
  unsigned int item_count;
  unsigned int selected;        /* Initial selection; read changes via getter. */
  unsigned int rows;            /* Visible list rows, 1..6. */
  char *text;                   /* Text only; caller-owned, edited in place. */
  size_t capacity;              /* Includes NUL, 1..DOS_WIDGETS_TEXT_MAX. */
};

struct dos_widget_rect {
  int x, y, width, height;
};

/* One global, asynchronous dialog. Begin copies specifications, not labels,
 * items, title/body or text storage: keep these alive and unchanged until close,
 * except the text buffer intentionally edited by this module. Zero unused
 * fields. No allocation, platform calls, game logic, or nested event loops.
 * Begin is failure-atomic and rejects a second dialog. Dimensions must match
 * at draw time; minimum 320x240. Oversize content is rejected, not hidden.
 * Failures print a diagnostic and return -1 (text getter returns NULL).
 * Events return 0 pending,
 * -2 Esc cancel, or a positive activation ID. Neither cancels nor activations
 * close the dialog; the caller closes and restores its underlying map.
 */
int dos_widgets_begin(const struct dos_vbe_framebuffer *fb,
                      const char *title, const char *body,
                      const struct dos_widget_spec *spec, unsigned int count);
void dos_widgets_close(void);
int dos_widgets_active(void);
int dos_widgets_dirty(void);
/* Request repaint after underlying map/UI changes; inactive calls are no-ops. */
void dos_widgets_invalidate(void);
int dos_widgets_event(const struct dos_input_event *event);
/* Draw returns 1 if painted, 0 if unchanged/inactive, -1 on failure. */
int dos_widgets_draw(struct dos_vbe_framebuffer *fb);
/* Feedback is copied; NULL clears that line. Maximum 127 printable bytes. */
int dos_widgets_feedback(const char *status, const char *error);
int dos_widgets_selection(unsigned int widget);
const char *dos_widgets_text(unsigned int widget);
int dos_widgets_focus(void);
int dos_widgets_bounds(unsigned int widget, struct dos_widget_rect *rect);

/* Keyboard: Tab/Shift-Tab focus, Enter activate, Esc cancel; buttons also Space.
 * Lists: arrows, Home/End, PageUp/PageDown correspond to row/Prev/Next clicks.
 * Text: printable ASCII, Backspace/Delete, arrows/Home/End; click positions
 * the caret. Ctrl/Alt combinations never insert text. Status/error and focused
 * outlines are drawn with the existing RGB565 primitives and bitmap font.
 */
#endif
