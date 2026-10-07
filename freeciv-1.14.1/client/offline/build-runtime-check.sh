#!/bin/sh
set -eu

SOURCE=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
if [ "$#" -ne 2 ]; then
  echo "Usage: sh build-runtime-check.sh /absolute/new-output-dir /absolute/config-dir" >&2
  exit 1
fi
case "$1:$2" in
  /*:/*) BUILD=$1; CONFIG=$2 ;;
  *) echo "FAIL: output and config directories must be absolute." >&2; exit 1 ;;
esac
if [ -e "$BUILD" ]; then
  echo "FAIL: output directory must not already exist: $BUILD" >&2
  exit 1
fi
if [ ! -f "$CONFIG/config.h" ]; then
  echo "FAIL: missing generated DOS config: $CONFIG/config.h" >&2
  exit 1
fi
CC=${CC:-i586-pc-msdosdjgpp-gcc}
LD=${LD:-i586-pc-msdosdjgpp-ld}
NM=${NM:-i586-pc-msdosdjgpp-nm}
OBJCOPY=${OBJCOPY:-i586-pc-msdosdjgpp-objcopy}
for tool in "$CC" "$LD" "$NM" "$OBJCOPY"; do
  if ! command -v "$tool" >/dev/null 2>&1; then
    echo "FAIL: required tool not found: $tool" >&2
    exit 1
  fi
done
CFLAGS="${CFLAGS:--O2 -g} -std=gnu89 -Wall -Wextra -Wstrict-prototypes -Werror=implicit-function-declaration -Werror=incompatible-pointer-types"
export CC LD NM OBJCOPY CFLAGS
mkdir -p "$BUILD"

if ! sh "$SOURCE/client/offline/build-engine.sh" "$BUILD" "$CONFIG"; then
  echo "FAIL: real isolated authoritative engine build failed." >&2
  exit 1
fi

compile()
{
  if ! "$CC" ${CPPFLAGS:-} $CFLAGS -UNDEBUG -fcommon \
      -DHAVE_CONFIG_H -DFC_LOCAL_ENGINE \
      -I"$CONFIG" -I"$SOURCE" -I"$SOURCE/common" -I"$SOURCE/intl" \
      -I"$SOURCE/client/offline" -c "$1" -o "$2"; then
    echo "FAIL: diagnostic/client compilation failed: $1" >&2
    exit 1
  fi
}

compile "$SOURCE/client/offline/local_queue.c" "$BUILD/local_queue.o"
compile "$SOURCE/client/offline/session.c" "$BUILD/session.o"
compile "$SOURCE/client/offline/runtime_check.c" "$BUILD/runtime_check.o"
# The unrenamed common objects are the independent client common instance.
if ! "$CC" ${LDFLAGS:-} "$BUILD/runtime_check.o" "$BUILD/local_queue.o" \
    "$BUILD/session.o" "$BUILD/engine.o" "$BUILD"/common/*.o \
    -lm -o "$BUILD/RTCHECK.EXE"; then
  echo "FAIL: RTCHECK.EXE link failed." >&2
  exit 1
fi
echo "Built $BUILD/RTCHECK.EXE (not executed; DOS 6.22/DPMI verification pending)."
echo 'Stage DATA\RUNTIME.DAT beside RTCHECK.EXE with one ASCII line:'
echo 'FREECIV DOS RUNTIME FIXTURE 1'
echo 'Run: RTCHECK --log C:\FREECIV\RUNTIME.LOG [--fixture A:\RUNTIME.DAT] [--batch]'
