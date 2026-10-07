#!/bin/sh
set -eu

SOURCE=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
if [ "$#" -ne 1 ]; then
  echo "Usage: sh client/offline/check.sh /absolute/empty-build-directory" >&2
  exit 1
fi
case "$1" in
  /*) BUILD=$1 ;;
  *) echo "Build directory must be absolute." >&2; exit 1 ;;
esac
if [ -e "$BUILD" ]; then
  echo "Build directory must not already exist: $BUILD" >&2
  exit 1
fi
mkdir -p "$BUILD/common" "$BUILD/engine"
CC=${CC:-gcc}
LD=${LD:-ld}
NM=${NM:-nm}
OBJCOPY=${OBJCOPY:-objcopy}

compile()
{
  "$CC" ${CFLAGS:--O0 -g} -UNDEBUG -fcommon -DHAVE_CONFIG_H -DFC_LOCAL_ENGINE \
    -I"$SOURCE" -I"$SOURCE/common" -I"$SOURCE/server" -I"$SOURCE/ai" \
    -I"$SOURCE/intl" -I"$SOURCE/client/offline" \
    -c "$1" -o "$2"
}

CC="$CC" LD="$LD" NM="$NM" OBJCOPY="$OBJCOPY" \
  sh "$SOURCE/client/offline/build-engine.sh" "$BUILD" "$SOURCE"
compile "$SOURCE/client/offline/local_queue.c" "$BUILD/local_queue.o"
compile "$SOURCE/client/offline/session.c" "$BUILD/session.o"
compile "$SOURCE/client/offline/transport_test.c" "$BUILD/transport_test.o"
"$CC" ${LDFLAGS:-} "$BUILD/transport_test.o" "$BUILD/local_queue.o" "$BUILD/session.o" \
  "$BUILD/engine.o" "$BUILD"/common/*.o -lm -o "$BUILD/transport_test"
if [ "${CROSS_COMPILE:-0}" = 1 ]; then
  echo "Cross-compiled offline test; runtime execution remains required: $BUILD/transport_test"
else
  cd "$BUILD"
  FREECIV_PATH="$SOURCE/data" "$BUILD/transport_test"
fi
