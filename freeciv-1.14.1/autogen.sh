#!/bin/sh
set -eu

SOURCE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
for option in "$@"; do
  if [ "$option" = --disable-autoconf2.52 ]; then
    echo "Legacy configure.in regeneration is unsupported; use configure.ac." >&2
    exit 1
  fi
done
sh "$SOURCE/bootstrap.sh"
if [ "${NOCONFIGURE:-0}" != 1 ]; then
  "$SOURCE/configure" "$@"
fi
