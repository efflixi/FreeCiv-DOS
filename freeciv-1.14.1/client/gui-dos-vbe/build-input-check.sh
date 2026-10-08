#!/bin/sh
set -eu
GUI=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
if [ "$#" -ne 2 ]; then
  echo "Usage: sh build-input-check.sh /absolute/new-output-dir /absolute/production-build-root" >&2
  exit 1
fi
exec sh "$GUI/build-map-check.sh" "$1" "$2" input
