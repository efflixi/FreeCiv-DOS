#!/bin/sh
set -eu
SOURCE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
BUILD="$SOURCE/tests/.dpmi-check.$$"
mkdir "$BUILD"
trap 'rm -f "$BUILD/dpmi-test"; rmdir "$BUILD"' 0
${HOST_CC:-cc} -std=gnu89 -g -O1 -Wall -Wextra -Werror \
  -Wstrict-prototypes -Wmissing-prototypes -Wno-pointer-to-int-cast \
  -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie \
  -D__DJGPP__ -I"$SOURCE/tests/dpmi-stubs" -I"$SOURCE" \
  "$SOURCE/tests/dpmi_test.c" "$SOURCE/vbe_hw.c" -o "$BUILD/dpmi-test"
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1 "$BUILD/dpmi-test"
