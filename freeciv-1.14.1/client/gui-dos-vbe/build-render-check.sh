#!/bin/sh
set -eu
SOURCE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
if [ "$#" -ne 1 ]; then
  echo "Usage: sh build-render-check.sh /absolute/new-output-directory" >&2
  exit 1
fi
case "$1" in
  /*) BUILD=$1 ;;
  *) echo "Output directory must be absolute." >&2; exit 1 ;;
esac
if [ -e "$BUILD" ]; then
  echo "Output directory must not already exist: $BUILD" >&2
  exit 1
fi
CC=${CC:-i586-pc-msdosdjgpp-gcc}
if ! "$CC" -dumpmachine | grep -q msdosdjgpp; then
  echo "A real DJGPP target compiler is required." >&2
  exit 1
fi
mkdir "$BUILD"
"$CC" -std=gnu89 -O2 -g -Wall -Wextra -Werror -I"$SOURCE" \
  "$SOURCE/tests/render_check.c" "$SOURCE/tests/render_pattern.c" \
  "$SOURCE/vbe_init.c" "$SOURCE/vbe_hw.c" "$SOURCE/framebuffer.c" \
  "$SOURCE/bitmap_font.c" "$SOURCE/extender_compat.c" \
  -o "$BUILD/RNDCHECK.EXE"
echo "Built $BUILD/RNDCHECK.EXE; actual DOS rendering validation still required."
