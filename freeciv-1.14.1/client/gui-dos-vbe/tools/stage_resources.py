#!/usr/bin/env python3
# Freeciv - Copyright (C) 1996 - A Kjeldberg, L Gregersen, P Unold
# This program is free software under GNU GPL version 2 or (at your option)
# any later version. See COPYING in the Freeciv source distribution.
"""Stage a tileset's graphics in a new, flat, strictly 8.3 DOS directory.

Usage: python3 stage_resources.py DATA_ROOT NEW_OUTPUT [--tileset trident]

No original assets are modified. XPM bytes are copied without conversion;
specifications retain their notices and tags, with resource references rewritten.
TRIDENT.TSP is the default top-level file. RESMAP.TXT gives every original and
staged filename, including extensionless gfx/intro references. PROVEN.TXT records
original and staged SHA-256 hashes, distinguishing rewritten specifications from
byte-identical XPMs. To use the output on DOS, copy the directory to an 8.3 path,
such as C:\\FC\\GFX.

This is graphics-only staging, not a replacement for the ruleset/data installer.
DOS tilespec startup resolves the original ".tilespec" filename using the
production dos_vbe_resource_filename(map_path, original_name) API, then passes
the allocated alias to datafilename(). Discovery recognizes ".TSP"; original
source .tilespec paths remain supported without a mapping. All dependent spec
and gfx references are already rewritten. Staging does not remove the normal
DOS GUI readiness gate or imply gameplay readiness.
"""

import argparse
import hashlib
from pathlib import Path, PurePosixPath
import re
import shutil


def quoted(text):
    return re.findall(r'"([^"\n]+)"', text)


def source_file(root, relative):
    path = PurePosixPath(relative)
    if path.is_absolute() or ".." in path.parts or "\\" in relative:
        raise ValueError(f"unsafe resource reference: {relative}")
    result = (root / relative).resolve()
    if not result.is_relative_to(root) or not result.is_file():
        raise ValueError(f"missing or escaping resource: {relative}")
    return result


def stage(root, output, tileset="trident"):
    root = Path(root).resolve()
    output = Path(output).resolve()
    if output.exists() or output.is_relative_to(root):
        raise ValueError("output must be new and outside the original data tree")
    if not re.fullmatch(r"[A-Za-z0-9_]{1,8}", tileset):
        raise ValueError("tileset name must fit an 8.3 basename")
    top_name = tileset + ".tilespec"
    top = source_file(root, top_name).read_text()
    specs = sorted(set(s for s in quoted(top) if s.endswith(".spec")))
    if not specs:
        raise ValueError("tilespec does not reference any specs")
    texts = {top_name: top}
    images = set()
    for spec in specs:
        text = source_file(root, spec).read_text()
        texts[spec] = text
        match = re.search(r'(?m)^\s*gfx\s*=\s*"([^"]+)"', text)
        if not match:
            raise ValueError(f"spec has no gfx reference: {spec}")
        images.add(match[1] + ".xpm")
    for key in ("main_intro_file", "minimap_intro_file"):
        match = re.search(rf'(?m)^\s*{key}\s*=\s*"([^"]+)"', top)
        if match:
            images.add(match[1] + ".xpm")
    # Validate every input before creating output.
    for image in images:
        source_file(root, image)
    mapping = {top_name: tileset.upper() + ".TSP"}
    mapping.update({name: f"S{i:07d}.SPC" for i, name in enumerate(specs)})
    mapping.update({name: f"G{i:07d}.XPM"
                    for i, name in enumerate(sorted(images))})
    references = dict(mapping)
    references.update({name[:-4]: staged[:-4] for name, staged in mapping.items()
                       if name.endswith(".xpm")})
    output.mkdir()
    try:
        for name, text in texts.items():
            text = re.sub(r'"([^"\n]+)"',
                          lambda m: '"' + references.get(m[1], m[1]) + '"', text)
            (output / mapping[name]).write_text(text)
        for image in images:
            shutil.copyfile(source_file(root, image), output / mapping[image])
        manifest = (
            "; Freeciv graphics resource lookup: original<TAB>8.3 staged\n"
            "; DOS tilespec lookup resolves .tilespec to .TSP using this map.\n"
            "; Graphics only; original data files are not changed.\n"
        )
        manifest += "".join(f"{name}\t{target}\n"
                            for name, target in sorted(references.items()))
        (output / "RESMAP.TXT").write_text(manifest)
        provenance = (
            "; Freeciv graphics staging provenance, SHA-256\n"
            "; original<TAB>staged<TAB>source-sha256<TAB>staged-sha256<TAB>mode\n"
            "; Names are data-root-relative; XPM assets must be byte-identical.\n"
        )
        for name, target in sorted(mapping.items()):
            original_hash = hashlib.sha256(source_file(root, name).read_bytes()).hexdigest()
            staged_hash = hashlib.sha256((output / target).read_bytes()).hexdigest()
            mode = "byte-copy" if name.endswith(".xpm") else "reference-rewrite"
            if mode == "byte-copy" and original_hash != staged_hash:
                raise ValueError(f"XPM staging changed source bytes: {name}")
            provenance += f"{name}\t{target}\t{original_hash}\t{staged_hash}\t{mode}\n"
        (output / "PROVEN.TXT").write_text(provenance)
    except BaseException:
        shutil.rmtree(output)
        raise
    return references


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("data_root", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--tileset", default="trident")
    args = parser.parse_args()
    try:
        mapping = stage(args.data_root, args.output, args.tileset)
    except (OSError, ValueError) as error:
        parser.exit(1, f"Resource staging failed: {error}\n")
    print(f"Staged {len(mapping)} filename/reference mappings in {args.output}")


if __name__ == "__main__":
    main()
