/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef FC__DOS_VBE_INPUT_H
#define FC__DOS_VBE_INPUT_H

#define DOS_INPUT_SHIFT 1U
#define DOS_INPUT_CTRL 2U
#define DOS_INPUT_ALT 4U
#define DOS_INPUT_LEFT 1U
#define DOS_INPUT_RIGHT 2U
#define DOS_INPUT_MIDDLE 4U

enum dos_input_type {
  DOS_INPUT_KEY,
  DOS_INPUT_POINTER,
  DOS_INPUT_COMMAND
};

struct dos_input_event {
  enum dos_input_type type;
  unsigned int ascii;
  unsigned int scan;
  unsigned int modifiers;
  int x, y;
  unsigned int buttons;
  unsigned int pressed;
  unsigned int released;
  unsigned int command;
};

int dos_input_init(unsigned int width, unsigned int height);
void dos_input_shutdown(void);
int dos_input_poll(struct dos_input_event *event);
int dos_input_mouse_available(void);
int dos_input_pointer(int *x, int *y);
void dos_input_idle(void);

#endif
