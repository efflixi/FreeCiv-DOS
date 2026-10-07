#!/bin/sh
set -eu

SOURCE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
if [ "$#" -ne 1 ]; then
  echo "Usage: sh build-dos.sh /absolute/new-build-directory" >&2
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
case "$BUILD" in
  *[[:space:]]*|*:*)
    echo "Autotools build directories must not contain whitespace or colons." >&2
    exit 1 ;;
esac
BUILD=$(CDPATH= cd -- "$(dirname -- "$BUILD")" && pwd)/$(basename -- "$BUILD")
case "$BUILD/" in
  "$SOURCE/"*) echo "Build directory must be outside the source tree." >&2; exit 1 ;;
esac
trap 'result=$?; if [ "$result" -ne 0 ]; then
  echo "DOS build failed; inspect available logs in $BUILD." >&2
fi' 0

PREFIX=${DJGPP_PREFIX:-i586-pc-msdosdjgpp-}
CC=${CC:-${PREFIX}gcc}
AR=${AR:-${PREFIX}ar}
RANLIB=${RANLIB:-${PREFIX}ranlib}
LD=${LD:-${PREFIX}ld}
NM=${NM:-${PREFIX}nm}
OBJCOPY=${OBJCOPY:-${PREFIX}objcopy}
export CC AR RANLIB LD NM OBJCOPY
for tool in "$CC" "$AR" "$RANLIB" "$LD" "$NM" "$OBJCOPY" \
            autoconf autoheader automake aclocal make tar sha256sum \
            file awk grep find sort wc; do
  command -v "$tool" >/dev/null
  "$tool" --version >/dev/null
done

mkdir -p "$BUILD/source" "$BUILD/build"
# A fresh snapshot avoids configured-source VPATH checks and stale objects.
tar -C "$SOURCE" --exclude='./autom4te.cache' --exclude='*/.deps' \
  --exclude='*.o' --exclude='*.a' --exclude='*.exe' --exclude='*~' \
  --exclude='Makefile' --exclude='./config.h' --exclude='./config.status' \
  --exclude='./config.log' --exclude='./config.cache' \
  --exclude='./client/civclient' --exclude='./server/civserver' \
  -cf "$BUILD/source.tar" .
tar -C "$BUILD/source" -xf "$BUILD/source.tar"
rm "$BUILD/source.tar"
(
  cd "$BUILD/source"
  find . -type f -print | LC_ALL=C sort | while IFS= read -r path; do
      sha256sum "$path"
    done
) > "$BUILD/source-inputs.sha256"
sh "$BUILD/source/bootstrap.sh" > "$BUILD/bootstrap.log" 2>&1
(
  cd "$BUILD/build"
  CFLAGS="${CFLAGS:--O2 -g}" "$BUILD/source/configure" \
    --host=i586-pc-msdosdjgpp --enable-client=dos-vbe --disable-server \
    --disable-nls --without-readline --without-zlib \
    --disable-esd --disable-sdl-mixer --disable-winmm \
    --disable-make-data --disable-cvs-deps \
    > "$BUILD/configure.log" 2>&1
  make -j"${JOBS:-2}" > "$BUILD/make.log" 2>&1
)
"$AR" t "$BUILD/build/client/gui-dos-vbe/libguiclient.a" \
  > "$BUILD/gui-members.txt"
if grep -q 'dos_client_stubs' "$BUILD/gui-members.txt"; then
  echo "Diagnostic ABI stubs reached the DOS link." >&2
  exit 1
fi
members=0
for source in "$BUILD/source/client/gui-dos-vbe/"*.c; do
  member=$(basename "$source" .c).o
  case "$member" in dos_client_stubs.o) continue ;; esac
  grep -Fx "$member" "$BUILD/gui-members.txt" >/dev/null
  members=$((members + 1))
done
if [ "$(wc -l < "$BUILD/gui-members.txt")" -ne "$members" ]; then
  echo "Unexpected DOS backend member count; review the source/archive contract." >&2
  exit 1
fi
"$NM" "$BUILD/build/client/civclient.exe" > "$BUILD/client-symbols.txt"
for symbol in ui_main load_gfxfile update_map_canvas \
              new_timer fc_offline_engine_open fc_offline_engine_poll \
              fc_engine_game fc_engine_is_server fc_engine_ai_do_first_activities; do
  grep -Eq " [ABCDGIRSTVW] _?$symbol$" "$BUILD/client-symbols.txt"
done
file "$BUILD/build/client/civclient.exe" > "$BUILD/executable.txt"
grep -q 'DJGPP go32 DOS extender' "$BUILD/executable.txt"
{
  echo "Target: i586-pc-msdosdjgpp; client: dos-vbe; standalone server: disabled"
  echo "Configured CFLAGS: ${CFLAGS:--O2 -g}; configure adds -Wall -std=gnu89 -fcommon"
  echo "Source snapshot: $BUILD/source"
  echo "Output: $BUILD/build/client/civclient.exe"
  for tool in "$CC" "$AR" "$RANLIB" "$LD" "$NM" "$OBJCOPY" autoconf automake; do
    "$tool" --version
  done
  sha256sum "$BUILD/build/client/civclient.exe" \
    "$BUILD/build/config.h" "$BUILD/source-inputs.sha256"
} > "$BUILD/build-info.txt"
echo "Fresh DOS link and symbol checks passed: $BUILD/build/client/civclient.exe"
echo "See $BUILD for build/configuration evidence; DOS runtime remains untested."
