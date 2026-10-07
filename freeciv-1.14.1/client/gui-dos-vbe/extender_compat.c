#include <stdio.h>

#include "extender_compat.h"
#include "vbe_hw.h"

const char *dos_extender_name(void)
{
  return "DJGPP / DPMI protected-mode DOS runtime (preferred)";
}

int dos_extender_init(void)
{
  return dos_vbe_hw_runtime();
}

void dos_extender_shutdown(void)
{
  /* Per-display resources are owned and released by vbe_shutdown_display(). */
}
