#!/bin/sh
set -eu

SOURCE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$SOURCE"
if [ "$#" -ne 0 ]; then
  echo "Usage: sh bootstrap.sh (then configure in a separate build directory)" >&2
  exit 1
fi

# configure.in is retained for historical reference, not DOS regeneration.
if [ ! -f config.rpath ]; then
  RPATH=${GETTEXT_DATADIR:-/usr/share/gettext}/config.rpath
  if [ ! -f "$RPATH" ]; then
    echo "Missing gettext config.rpath; install gettext or set GETTEXT_DATADIR." >&2
    exit 1
  fi
  cp "$RPATH" config.rpath
fi
cp m4/x.252 m4/x.m4
cat m4/*.m4 > acinclude.m4
aclocal
autoheader configure.ac
autoconf -o configure configure.ac
automake --add-missing --copy
