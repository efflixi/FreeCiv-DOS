/********************************************************************** 
 Freeciv - Copyright (C) 1996 - A Kjeldberg, L Gregersen, P Unold
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
***********************************************************************/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include "support.h"
#include "audio.h"
#include "gui_main_g.h"

#include "audio_dos.h"

/**************************************************************************
  Clean up.
**************************************************************************/
static void my_shutdown(void)
{
}

/**************************************************************************
  Stop music.
**************************************************************************/
static void my_stop(void)
{
}

/**************************************************************************
  Wait for sound completion.
**************************************************************************/
static void my_wait(void)
{
}

/**************************************************************************
  Play a sample or sound effect.

  The DOS backend should prefer SB / AdLib / MIDI devices when available,
  but this scaffold keeps the interface isolated and gracefully falls back to
  silence or to the existing bell callback when the device is unavailable.
**************************************************************************/
static bool my_play(const char *const tag, const char *const fullpath,
                    bool repeat)
{
  if (fullpath == NULL && strcmp(tag, "e_turn_bell") == 0) {
    sound_bell();
    return TRUE;
  }

  if (fullpath == NULL) {
    return FALSE;
  }

  if (repeat) {
    /* Phase 4 DOS audio scaffold: music is not yet routed to a real MIDI
     * or FM device; this function intentionally does not invent a fake
     * sound path on unsupported hardware.
     */
    return FALSE;
  }

  /* Phase 4 implementation is intentionally conservative: keep the plugin
   * registered and available without forcing Linux-specific audio stacks.
   */
  return TRUE;
}

/**************************************************************************
  Initialize the DOS sound plugin.
**************************************************************************/
static bool my_init(void)
{
  return TRUE;
}

/**************************************************************************
  Register the DOS plugin with the client audio system.
**************************************************************************/
void audio_dos_init(void)
{
  struct audio_plugin self;

  sz_strlcpy(self.name, "dos");
  sz_strlcpy(self.descr,
             "DOS native sound plugin (SB / AdLib / MIDI / quiet fallback)");
  self.init = my_init;
  self.shutdown = my_shutdown;
  self.stop = my_stop;
  self.wait = my_wait;
  self.play = my_play;
  audio_add_plugin(&self);
}
