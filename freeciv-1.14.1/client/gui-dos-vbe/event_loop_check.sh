#!/bin/sh
set -eu
GUI=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
SOURCE=$(CDPATH= cd -- "$GUI/../.." && pwd)
BUILD="$PWD/.dos-event-check-$$"
mkdir "$BUILD"
trap 'rm -rf "$BUILD"' 0
${HOST_CC:-cc} -std=gnu89 -g -O1 -Wall -Wextra -Werror \
  -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie \
  -I"$GUI" -I"$SOURCE/client/include" -I"$SOURCE/common" \
  "$GUI/event_loop.c" "$GUI/commands.c" "$GUI/tests/event_loop_test.c" \
  -o "$BUILD/event-test"
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  "$BUILD/event-test"
