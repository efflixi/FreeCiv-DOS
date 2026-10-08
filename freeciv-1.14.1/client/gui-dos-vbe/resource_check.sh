#!/bin/sh
set -eu
ulimit -c 0
SOURCE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$SOURCE/../.." && pwd)
# Scratch belongs to the invoking build directory, not the source tree.
BUILD="$PWD/.dos-resource-check.$$"
umask 077
mkdir "$BUILD"
trap 'rm -rf "$BUILD"' 0
trap 'exit 1' 1 2 3 15
PYTHONDONTWRITEBYTECODE=1 python3 "$SOURCE/tests/resource_manifest_test.py" \
  "$ROOT" "$BUILD"
${HOST_CC:-cc} -std=gnu89 -g -O1 -Wall -Wextra -Werror -Wconversion \
  -Wshadow -Wstrict-prototypes -Wmissing-prototypes \
  -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie \
  -I"$SOURCE" -I"$ROOT/include" -I"$ROOT/common" -I"$ROOT/client/include" \
  "$SOURCE/tests/resource_test.c" "$SOURCE/graphics.c" "$SOURCE/resource_xpm.c" \
  "$SOURCE/framebuffer.c" "$SOURCE/bitmap_font.c" \
  -Wl,--wrap=malloc -Wl,--wrap=calloc -o "$BUILD/resource-test"
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  "$BUILD/resource-test" "$BUILD/manifest.txt" "$BUILD/atlases.txt" \
  "$BUILD/fixture.xpm" "$BUILD/staged/RESMAP.TXT"
if [ -n "${DOS_CC:-}" ]; then
  "$DOS_CC" -dumpmachine | grep -q msdosdjgpp
  for unit in graphics resource_xpm; do
    "$DOS_CC" -std=gnu89 -O2 -Wall -Wextra -Werror \
      -I"$SOURCE" -I"$ROOT/include" -I"$ROOT/common" -I"$ROOT/client/include" \
      -c "$SOURCE/$unit.c" -o "$BUILD/$unit.o"
  done
  echo "DJGPP graphics/resource objects compiled."
fi
