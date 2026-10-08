#!/usr/bin/env python3
# Freeciv - Copyright (C) 1996 - A Kjeldberg, L Gregersen, P Unold
# GNU GPL version 2 or (at your option) any later version.
"""Generate actual Trident atlas/tag crop cases and verify 8.3 staging."""

import hashlib
import importlib.util
from pathlib import Path
import re
import sys


def cases(root, spec_name, mapping=None):
    text = (root / spec_name).read_text()
    gfx = re.search(r'(?m)^\s*gfx\s*=\s*"([^"]+)"', text)[1] + ".xpm"
    for match in re.finditer(r'(?ms)^\[(grid_[^\]]+)\]\s*(.*?)(?=^\[|\Z)', text):
        block = match[2]
        def value(name, default=None):
            found = re.search(rf"(?m)^\s*{name}\s*=\s*(\d+)", block)
            if found:
                return int(found[1])
            if default is not None:
                return default
            raise AssertionError(f"missing {name} in {spec_name}")
        x, y, dx, dy = (value(n) for n in ("x_top_left", "y_top_left", "dx", "dy"))
        border = value("is_pixel_border", 0)
        count = 0
        for row in re.finditer(r'(?m)^\s*(\d+)\s*,\s*(\d+)\s*,\s*("[^\n]+)', block):
            r, c = int(row[1]), int(row[2])
            tags = re.findall(r'"([^"]+)"', row[3])
            assert tags
            for tag in tags:
                yield (str(root / gfx), str(root / spec_name), tag,
                       x + c * (dx + border), y + r * (dy + border), dx, dy)
                count += 1
        assert count, spec_name


def main():
    source, build = map(Path, sys.argv[1:])
    root = source / "data"
    utility = source / "client/gui-dos-vbe/tools/stage_resources.py"
    spec = importlib.util.spec_from_file_location("dos_stage", utility)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    before = {str(p): hashlib.sha256(p.read_bytes()).hexdigest()
              for p in root.rglob("*") if p.is_file()}
    staged = build / "staged"
    mapping = module.stage(root, staged)
    assert all(re.fullmatch(r"[A-Z0-9]{1,8}\.[A-Z0-9]{1,3}", p.name)
               for p in staged.iterdir())
    provenance = (staged / "PROVEN.TXT").read_text()
    records = [line.split("\t") for line in provenance.splitlines()
               if line and not line.startswith(";")]
    assert len(records) == 21
    for name, target, source_hash, target_hash, mode in records:
        assert hashlib.sha256((root / name).read_bytes()).hexdigest() == source_hash
        assert hashlib.sha256((staged / target).read_bytes()).hexdigest() == target_hash
        assert mode in ("byte-copy", "reference-rewrite")
        if mode == "byte-copy":
            assert source_hash == target_hash
    top = (root / "trident.tilespec").read_text()
    names = module.quoted(top)
    specs = [s for s in names if s.endswith(".spec")]
    for original, target in mapping.items():
        if original.endswith(".xpm"):
            assert (root / original).read_bytes() == (staged / target).read_bytes()
    for name in specs:
        refs = module.quoted((staged / mapping[name]).read_text())
        gfx = re.search(r'(?m)^\s*gfx\s*=\s*"([^"]+)"',
                        (staged / mapping[name]).read_text())[1]
        assert (staged / (gfx + ".XPM")).is_file()
        assert refs
    staged_top = (staged / "TRIDENT.TSP").read_text()
    assert all((staged / mapping[s]).is_file() and mapping[s] in staged_top
               for s in specs)
    original_cases = [case for name in specs for case in cases(root, name)]
    staged_cases = [case for name in specs
                    for case in cases(staged, mapping[name])]
    # Staged gfx suffix is upper-case; cases() uses lowercase source suffix.
    staged_cases = [(c[0][:-4] + ".XPM",) + c[1:] for c in staged_cases]
    assert [(c[2:]) for c in original_cases] == [(c[2:]) for c in staged_cases]
    assert len(original_cases) > 400
    with (build / "manifest.txt").open("w") as stream:
        for case in original_cases + staged_cases:
            stream.write("\t".join(map(str, case)) + "\n")
    # Load all source atlases, not only the ones referenced by Trident.
    with (build / "atlases.txt").open("w") as stream:
        for path in sorted(root.rglob("*.xpm")):
            stream.write(str(path) + "\n")
    assert before == {str(p): hashlib.sha256(p.read_bytes()).hexdigest()
                      for p in root.rglob("*") if p.is_file()}
    print(f"8.3 staging: {len(mapping)} mappings; "
          f"{len(original_cases)} source tag crops plus staged copies")


if __name__ == "__main__":
    main()
