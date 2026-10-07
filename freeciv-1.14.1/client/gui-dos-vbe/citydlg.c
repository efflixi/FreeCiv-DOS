/* citydlg.c -- PLACEHOLDER */

#include "citydlg.h"

#include <stdio.h>

bool city_dialog_is_open(struct city *pcity)
{
  (void)pcity;
  return FALSE;
}


void
popup_city_dialog(struct city *pcity, bool make_modal)
{
  (void)pcity;
  (void)make_modal;
  fprintf(stderr, "DOS VBE client: city dialogs are unavailable.\n");
}

void
popdown_city_dialog(struct city *pcity)
{
	/* PORTME */
}

void
popdown_all_city_dialogs(void)
{
	/* PORTME */
}

void
refresh_city_dialog(struct city *pcity)
{
	/* PORTME */
}

void
refresh_unit_city_dialogs(struct unit *punit)
{
	/* PORTME */
}
