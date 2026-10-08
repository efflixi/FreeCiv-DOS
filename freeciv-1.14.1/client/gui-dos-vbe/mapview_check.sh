#!/bin/sh
set -eu
ulimit -c 0
SOURCE=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
GUI="$SOURCE/client/gui-dos-vbe"
BUILD="$PWD/.dos-mapview-check-$$"
mkdir "$BUILD"
trap 'rm -rf "$BUILD"' 0
export TMPDIR="$BUILD"
CC=${HOST_CC:-cc}
DATA=${MAPVIEW_DATA_PATH:-"$SOURCE/data"}
if [ -n "${MAPVIEW_STAGED_DATA:-}" ]; then
  if [ ! -f "$MAPVIEW_STAGED_DATA/TRIDENT.TSP" ] \
      || [ ! -f "$MAPVIEW_STAGED_DATA/RESMAP.TXT" ]; then
    echo "MAPVIEW_STAGED_DATA must contain TRIDENT.TSP and RESMAP.TXT." >&2
    exit 1
  fi
  PYTHONDONTWRITEBYTECODE=1 python3 "$GUI/tests/stage_phase6_fixture.py" \
    "$SOURCE/data" "$BUILD/fixture-rulesets"
  DATA="$MAPVIEW_STAGED_DATA:$BUILD/fixture-rulesets"
  echo "STAGED fixture DATA: unchanged original graphics package; private 8.3 rulesets"
fi
cp "$GUI/tests/map-config.h" "$BUILD/config.h"
FLAGS="-std=gnu89 -fcommon -g -O1 -Wall -Wextra -Wno-unused-parameter -Wno-ignored-qualifiers -DHAVE_CONFIG_H -DFC_LOCAL_ENGINE -ffunction-sections -fdata-sections -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie"
compile() {
  input=$1
  shift
  "$CC" $FLAGS -I"$BUILD" -I"$SOURCE" -I"$SOURCE/common" -I"$SOURCE/intl" \
    -I"$SOURCE/client" -I"$SOURCE/client/include" -I"$SOURCE/client/agents" \
    -I"$GUI" -I"$GUI/tests" \
    "$@" -c "$input" -o "$BUILD/$(basename "$input" .c).o"
}
for source in astring capability capstr city combat connection dataio diptreaty \
  fcintl game genlist government hash idex improvement inputfile ioz log map \
  nation netintf packets player mem rand registry sbuffer shared spaceship \
  support tech timing unit unittype worklist version; do
  compile "$SOURCE/common/$source.c" > "$BUILD/$source.log" 2>&1 \
    || { cat "$BUILD/$source.log"; exit 1; }
done
for source in audio climisc control goto mapview_common options packhand tilespec; do
  compile "$SOURCE/client/$source.c" > "$BUILD/$source.log" 2>&1 \
    || { cat "$BUILD/$source.log"; exit 1; }
done
for source in "$GUI/graphics.c" "$GUI/resource_xpm.c" "$GUI/framebuffer.c" "$GUI/bitmap_font.c" \
  "$GUI/colors.c" "$GUI/mapview.c" "$GUI/tests/phase6_scene.c" "$GUI/tests/mapview_test.c"; do
  compile "$source" -Werror=implicit-function-declaration -Werror=incompatible-pointer-types
done
"$CC" -fsanitize=address,undefined -fno-pie -no-pie -Wl,--gc-sections \
  "$BUILD/"*.o -lm -o "$BUILD/mapview-test"
FREECIV_PATH="$DATA" ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  timeout "${MAPVIEW_TEST_TIMEOUT:-60}" "$BUILD/mapview-test" "$@"
