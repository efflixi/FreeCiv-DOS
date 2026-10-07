#!/bin/sh
set -eu
ulimit -c 0
SOURCE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
BUILD="$SOURCE/.framebuffer-check.$$"
umask 077
mkdir "$BUILD"
trap 'rm -f "$BUILD/framebuffer-test"; rmdir "$BUILD"' 0
trap 'exit 1' 1 2 3 15
${HOST_CC:-cc} -std=gnu89 -g -O1 -Wall -Wextra -Werror -Wconversion \
  -Wshadow -Wstrict-prototypes -Wmissing-prototypes \
  -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie \
  -I"$SOURCE" "$SOURCE/tests/framebuffer_test.c" \
  "$SOURCE/framebuffer.c" "$SOURCE/bitmap_font.c" \
  -Wl,--wrap=calloc -o "$BUILD/framebuffer-test"
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1 "$BUILD/framebuffer-test"
