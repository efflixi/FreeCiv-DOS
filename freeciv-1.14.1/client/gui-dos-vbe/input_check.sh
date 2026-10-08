#!/bin/sh
set -eu
SOURCE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
BUILD="$SOURCE/tests/.input-check.$$"
mkdir "$BUILD"
trap 'rm -f "$BUILD/input-test" "$BUILD/input.o"; rmdir "$BUILD"' 0
${HOST_CC:-cc} -std=gnu89 -g -O1 -Wall -Wextra -Werror \
  -Wstrict-prototypes -Wmissing-prototypes \
  -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie \
  -D__DJGPP__ -I"$SOURCE/tests/input-stubs" -I"$SOURCE" \
  "$SOURCE/tests/input_test.c" "$SOURCE/input.c" -o "$BUILD/input-test"
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1 "$BUILD/input-test"
if test -n "${DJGPP_CC:-}"; then
  "$DJGPP_CC" -std=gnu89 -O2 -Wall -Wextra -Werror \
    -Wstrict-prototypes -Wmissing-prototypes \
    -I"$SOURCE" -c "$SOURCE/input.c" -o "$BUILD/input.o"
fi
