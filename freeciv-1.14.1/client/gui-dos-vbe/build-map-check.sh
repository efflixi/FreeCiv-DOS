#!/bin/sh
set -eu
SOURCE=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
if [ "$#" -ne 2 ]; then
  echo "Usage: sh build-map-check.sh /absolute/new-output-dir /absolute/production-build-root" >&2
  exit 1
fi
case "$1:$2" in
  /*:/*) OUTPUT=$1; BUILD=$2 ;;
  *) echo "Both paths must be absolute." >&2; exit 1 ;;
esac
if [ -e "$OUTPUT" ] || [ ! -f "$BUILD/config.h" ] \
    || [ ! -f "$BUILD/client/civclient.exe" ]; then
  echo "Require a nonexistent output directory and complete production build." >&2
  exit 1
fi
CC=${CC:-i586-pc-msdosdjgpp-gcc}
if ! "$CC" -dumpmachine | grep -q msdosdjgpp; then
  echo "A real DJGPP target compiler is required." >&2
  exit 1
fi
mkdir "$OUTPUT"
CFLAGS="${CFLAGS:--O2 -g} -std=gnu89 -fcommon -Wall -Wextra -Werror=implicit-function-declaration -Werror=incompatible-pointer-types"
set -- -DHAVE_CONFIG_H -DFC_LOCAL_ENGINE -I"$BUILD" -I"$SOURCE" \
  -I"$SOURCE/common" -I"$SOURCE/intl" -I"$SOURCE/client" \
  -I"$SOURCE/client/include" -I"$SOURCE/client/agents" \
  -I"$SOURCE/client/gui-dos-vbe"
"$CC" $CFLAGS "$@" -Dmain=fc_dos_unused_client_main \
  -c "$SOURCE/client/civclient.c" -o "$OUTPUT/client_state.o"
"$CC" $CFLAGS "$@" -c "$SOURCE/client/gui-dos-vbe/tests/map_check.c" \
  -o "$OUTPUT/map_check.o"
"$CC" $CFLAGS "$@" -c "$SOURCE/client/gui-dos-vbe/tests/phase6_scene.c" \
  -o "$OUTPUT/phase6_scene.o"
set -- "$OUTPUT/map_check.o" "$OUTPUT/phase6_scene.o" "$OUTPUT/client_state.o"
for object in "$BUILD/client/"*.o; do
  case "$object" in */civclient.o) continue ;; esac
  set -- "$@" "$object"
done
"$CC" $CFLAGS "$@" \
  "$BUILD/client/gui-dos-vbe/libguiclient.a" "$BUILD/common/libcivcommon.a" \
  "$BUILD/client/agents/libagents.a" "$BUILD/client/gui-dos-vbe/libguiclient.a" \
  "$BUILD/common/libcivcommon.a" "$BUILD/client/offline/liboffline.a" \
  "$BUILD/client/offline/engine-build/engine.o" "$BUILD/common/libcivcommon.a" \
  -lm -o "$OUTPUT/MAPCHECK.EXE"
echo "Built $OUTPUT/MAPCHECK.EXE; fixture/resource rendering, not a playable game."
