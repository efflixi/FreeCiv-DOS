#!/bin/sh
set -eu

SOURCE=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
if [ "$#" -ne 2 ]; then
  echo "Usage: sh build-engine.sh /absolute/object-directory /absolute/config-directory" >&2
  exit 1
fi
case "$1:$2" in
  /*:/*) BUILD=$1; CONFIG=$2 ;;
  *) echo "Engine build/config directories must be absolute." >&2; exit 1 ;;
esac
test -f "$CONFIG/config.h"
mkdir -p "$BUILD/common" "$BUILD/engine"
CC=${CC:-gcc}
LD=${LD:-ld}
NM=${NM:-nm}
OBJCOPY=${OBJCOPY:-objcopy}

compile()
{
  "$CC" ${CPPFLAGS:-} ${CFLAGS:--O0 -g} -UNDEBUG -fcommon \
    -DHAVE_CONFIG_H -DFC_LOCAL_ENGINE \
    -I"$CONFIG" -I"$SOURCE" -I"$SOURCE/common" -I"$SOURCE/server" \
    -I"$SOURCE/ai" -I"$SOURCE/intl" -I"$SOURCE/client/offline" \
    -c "$1" -o "$2"
}

for source in "$SOURCE"/common/*.c; do
  name=$(basename "$source" .c)
  compile "$source" "$BUILD/common/$name.o"
done
for directory in server ai; do
  for source in "$SOURCE/$directory"/*.c; do
    name=$(basename "$source" .c)
    case "$directory/$name" in server/civserver) continue ;; esac
    compile "$source" "$BUILD/engine/${directory}_$name.o"
  done
done
compile "$SOURCE/client/offline/engine.c" "$BUILD/engine/adapter.o"
"$LD" -r "$BUILD"/common/*.o "$BUILD"/engine/*.o -o "$BUILD/engine.raw.o"
"$NM" --defined-only --extern-only --format=posix "$BUILD/engine.raw.o" \
  > "$BUILD/engine.defined"
awk '$2 ~ /^[ABCDGIRSTVW]$/ &&
       $1 !~ /^_?fc_offline_engine_(open|feed|start|poll|snapshot|close|set_service)$/ {
         if (substr($1, 1, 1) == "_") {
           print $1, "_fc_engine_" substr($1, 2)
         } else {
           print $1, "fc_engine_" $1
         }
       }' "$BUILD/engine.defined" > "$BUILD/engine.symbols"
if ! grep -Eq '^_?game ' "$BUILD/engine.symbols" ||
   ! grep -Eq '^_?map ' "$BUILD/engine.symbols" ||
   ! grep -Eq '^_?is_server ' "$BUILD/engine.symbols"; then
  echo "Engine state symbols were not isolated; refusing to link." >&2
  exit 1
fi
"$OBJCOPY" --redefine-syms="$BUILD/engine.symbols" \
  "$BUILD/engine.raw.o" "$BUILD/engine.o.tmp"
"$NM" --defined-only --extern-only --format=posix "$BUILD/engine.o.tmp" \
  > "$BUILD/engine.isolated"
if awk '$2 ~ /^[ABCDGIRSTVW]$/ &&
        $1 !~ /^_?fc_engine_/ &&
        $1 !~ /^_?fc_offline_engine_(open|feed|start|poll|snapshot|close|set_service)$/ {
          print; bad=1
        } END {exit !bad}' "$BUILD/engine.isolated"; then
  echo "Unisolated engine symbols remain; refusing to link." >&2
  exit 1
fi
mv "$BUILD/engine.o.tmp" "$BUILD/engine.o"
