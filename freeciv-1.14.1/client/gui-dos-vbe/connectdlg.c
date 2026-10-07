/* connectdlg.c -- DOS port stub */

#include <stdio.h>

#include "connectdlg.h"

void gui_server_connect(void)
{
  /* Ported client uses a DOS session stub; actual network connect is deferred. */
  fprintf(stderr, "DOS VBE client: gui_server_connect() stubbed.\n");
}

void server_autoconnect(void)
{
  /* The DOS port is intentionally not implementing automatic server retries yet. */
  fprintf(stderr, "DOS VBE client: server_autoconnect() stubbed.\n");
}
