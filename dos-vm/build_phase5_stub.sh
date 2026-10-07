#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$SCRIPT_DIR"

nasm -f bin -o fc_dos_stub.com fc_dos_stub.asm
mcopy -o -i DOS622_VHD.img@@32256 fc_dos_stub.com ::/ARCHIVE/FCSTUB.COM

echo "Historical demo copied to C:\\ARCHIVE\\FCSTUB.COM; startup files unchanged."
