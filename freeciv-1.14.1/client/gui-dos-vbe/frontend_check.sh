#!/bin/sh
set -eu
GUI=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
SOURCE=$(CDPATH= cd -- "$GUI/../.." && pwd)
BUILD="$PWD/.dos-frontend-check-$$"
mkdir "$BUILD"
trap 'rm -rf "$BUILD"' 0
cp "$GUI/tests/map-config.h" "$BUILD/config.h"
${HOST_CC:-cc} -std=gnu89 -g -O1 -Wall -Wextra -Wno-ignored-qualifiers \
  -Werror=implicit-function-declaration -Werror=incompatible-pointer-types \
  -DHAVE_CONFIG_H -DFC_LOCAL_ENGINE -ffunction-sections -fdata-sections \
  -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie \
  -I"$BUILD" -I"$GUI" -I"$SOURCE/common" -I"$SOURCE/client" \
  -I"$SOURCE/client/include" \
  "$GUI/gui_main.c" "$GUI/tests/frontend_test.c" -Wl,--gc-sections \
  -o "$BUILD/frontend-test"
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  "$BUILD/frontend-test"
