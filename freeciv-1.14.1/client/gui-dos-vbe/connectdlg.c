/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <stdio.h>

#include "connectdlg.h"
#include "gui_main.h"

void gui_server_connect(void)
{
  dos_vbe_gui_status("No session. New/load-game startup is not implemented yet.");
}

void server_autoconnect(void)
{
  dos_vbe_gui_status("Autoconnect is unavailable. New/load-game startup is not implemented.");
  fprintf(stderr, "DOS GUI: unsupported autoconnect request.\n");
}
