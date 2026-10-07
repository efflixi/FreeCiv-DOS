#!/bin/sh
set -eu
ulimit -c 0
SOURCE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
BUILD=$(mktemp -d "${TMPDIR:-/tmp}/freeciv-vbe-check.XXXXXX")
trap 'rm -f "$BUILD/vbe-test"; rmdir "$BUILD"' 0
${HOST_CC:-cc} -std=gnu89 -g -O1 -Wall -Wextra -Werror \
  -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie \
  -I"$SOURCE" "$SOURCE/tests/vbe_test.c" "$SOURCE/vbe_init.c" \
  "$SOURCE/framebuffer.c" "$SOURCE/extender_compat.c" \
  "$SOURCE/bitmap_font.c" \
  -Wl,--wrap=calloc -o "$BUILD/vbe-test"
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 "$BUILD/vbe-test"
