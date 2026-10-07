#!/usr/bin/env python3
"""Wrap the project's FAT16 volume in a DOS-visible MBR partition."""

import pathlib
import struct
import sys


def chs(lba):
    cylinder, remainder = divmod(lba, 16 * 63)
    head, sector = divmod(remainder, 63)
    if cylinder > 1023:
        raise ValueError("Volume exceeds the supported DOS CHS geometry")
    return bytes((head, sector + 1 | (cylinder >> 8) << 6, cylinder & 255))


if len(sys.argv) != 3:
    sys.exit("Usage: partition_drive.py source-fat16.img new-disk.img")

source = pathlib.Path(sys.argv[1])
destination = pathlib.Path(sys.argv[2])
with source.open("rb") as original:
    boot = bytearray(original.read(512))
    if boot[510:512] != b"\x55\xaa" or boot[54:62] != b"FAT16   ":
        sys.exit("Source must be the project's unpartitioned FAT16 volume")
    if struct.unpack_from("<H", boot, 11)[0] != 512:
        sys.exit("Only 512-byte FAT sectors are supported")
    if struct.unpack_from("<I", boot, 28)[0] != 0:
        sys.exit("Source already has a partition offset")
    sectors = struct.unpack_from("<H", boot, 19)[0]
    if not sectors:
        sectors = struct.unpack_from("<I", boot, 32)[0]
    if sectors * 512 != source.stat().st_size:
        sys.exit("Source volume size does not match its boot record")

    start = 63
    cylinders = (start + sectors + 16 * 63 - 1) // (16 * 63)
    mbr = bytearray(512)
    mbr[446:462] = (b"\x80" + chs(start) + b"\x06"
                    + chs(start + sectors - 1)
                    + struct.pack("<II", start, sectors))
    mbr[510:512] = b"\x55\xaa"
    struct.pack_into("<HHI", boot, 24, 63, 16, start)

    with destination.open("xb") as output:
        output.truncate(cylinders * 16 * 63 * 512)
        output.seek(0)
        output.write(mbr)
        output.seek(start * 512)
        output.write(boot)
        while block := original.read(1024 * 1024):
            if any(block):
                output.write(block)
            else:
                output.seek(len(block), 1)

print(f"Created FAT16 type-06 partition at LBA {start}, {sectors} sectors")
print(f"Geometry: {cylinders}/16/63; mtools offset: {start * 512} bytes")
print("Partition table only: continue booting DOS from A:, not this disk.")
