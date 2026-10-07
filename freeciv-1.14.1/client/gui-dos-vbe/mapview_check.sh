#!/bin/sh
set -eu
ulimit -c 0
SOURCE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
BUILD="$SOURCE/tests/.mapview-check-$$"
mkdir "$BUILD"
trap 'rm -f "$BUILD/mapview-test"; rmdir "$BUILD"' 0
TMPDIR="$BUILD" ${HOST_CC:-cc} -std=gnu89 -g -O1 -Wall -Wextra \
  -Wno-unused-parameter -Werror \
  -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie \
  -I"$SOURCE/tests/map-stubs" -I"$SOURCE" -I"$SOURCE/../../common" \
  -I"$SOURCE/../include" \
  -include "$SOURCE/tests/map-stubs/mapview.h" \
  "$SOURCE/tests/mapview_test.c" "$SOURCE/mapview.c" \
  "$SOURCE/framebuffer.c" "$SOURCE/bitmap_font.c" "$SOURCE/colors.c" \
  -o "$BUILD/mapview-test"
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  timeout "${MAPVIEW_TEST_TIMEOUT:-30}" "$BUILD/mapview-test" "$@"
