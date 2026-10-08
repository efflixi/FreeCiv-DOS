#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
if ! grep -q 'gui-dos-vbe' "$SCRIPT_DIR/gui_main.c"; then
  echo "DOS VBE scaffold missing gui-dos-vbe registration" >&2
  exit 1
fi
if ! grep -q 'dos_vbe_handle_input_event' "$SCRIPT_DIR/gui_main.c"; then
  echo "DOS VBE input flow missing keyboard queue handling" >&2
  exit 1
fi
if ! grep -q 'dos_vbe_apply_command' "$SCRIPT_DIR/mapview.c"; then
  echo "DOS VBE mapview missing command routing" >&2
  exit 1
fi
if ! grep -q 'dos_event_loop_step' "$SCRIPT_DIR/gui_main.c"; then
  echo "DOS VBE persistent event loop missing input processing" >&2
  exit 1
fi
if ! grep -q 'dos_vbe_mode_for_resolution' "$SCRIPT_DIR/vbe_init.c"; then
  echo "DOS VBE mode selection missing VESA resolution logic" >&2
  exit 1
fi
if ! grep -q 'dos_vbe_get_mode_info' "$SCRIPT_DIR/vbe_init.c"; then
  echo "DOS VBE mode info missing VESA metadata" >&2
  exit 1
fi
if ! grep -q 'update_info_label' "$SCRIPT_DIR/mapview.c"; then
  echo "DOS VBE mapview missing a HUD drawing pass" >&2
  exit 1
fi
if ! grep -q 'fill_tile_sprite_array' "$SCRIPT_DIR/mapview.c"; then
  echo "DOS VBE mapview missing tile rendering" >&2
  exit 1
fi

echo "DOS VBE source-presence smoke check passed; this does not validate runtime or gameplay."
