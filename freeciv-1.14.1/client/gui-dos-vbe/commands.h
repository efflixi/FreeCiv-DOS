/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef FC__DOS_VBE_COMMANDS_H
#define FC__DOS_VBE_COMMANDS_H
#include "gui_main.h"
#include "input.h"
struct dos_event_loop;
int dos_vbe_event_direction(const struct dos_input_event *event, int *dx, int *dy);
enum dos_vbe_ui_command dos_vbe_event_command(const struct dos_input_event *event);
int dos_vbe_defer_command(struct dos_event_loop *loop, enum dos_vbe_ui_command command);
#endif
