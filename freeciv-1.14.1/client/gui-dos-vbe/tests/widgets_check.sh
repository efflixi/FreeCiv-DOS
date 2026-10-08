#!/bin/sh
set -eu
ulimit -c 0
SOURCE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BUILD="$SOURCE/tests/.widgets-check.$$"
umask 077
mkdir "$BUILD"
trap 'rm -f "$BUILD/widgets-test" "$BUILD/widgets.o"; rmdir "$BUILD"' 0
trap 'exit 1' 1 2 3 15
${HOST_CC:-cc} -std=gnu89 -g -O1 -Wall -Wextra -Werror -Wconversion \
  -Wshadow -Wstrict-prototypes -Wmissing-prototypes \
  -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie \
  -I"$SOURCE" "$SOURCE/tests/widgets_test.c" \
  "$SOURCE/widgets.c" "$SOURCE/framebuffer.c" "$SOURCE/bitmap_font.c" \
  -o "$BUILD/widgets-test"
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1 "$BUILD/widgets-test"
if [ -n "${DJGPP_CC:-}" ]; then
  "$DJGPP_CC" -dumpmachine | grep -q msdosdjgpp
  "$DJGPP_CC" -std=gnu89 -O2 -Wall -Wextra -Werror -Wconversion \
    -Wshadow -Wstrict-prototypes -Wmissing-prototypes \
    -I"$SOURCE" -c "$SOURCE/widgets.c" -o "$BUILD/widgets.o"
  echo "widgets: DJGPP compile passed"
elif command -v i586-pc-msdosdjgpp-gcc >/dev/null 2>&1; then
  i586-pc-msdosdjgpp-gcc -std=gnu89 -O2 -Wall -Wextra -Werror \
    -I"$SOURCE" -c "$SOURCE/widgets.c" -o "$BUILD/widgets.o"
  echo "widgets: DJGPP compile passed"
else
  echo "widgets: DJGPP compile skipped (set DJGPP_CC to target compiler)" >&2
fi
