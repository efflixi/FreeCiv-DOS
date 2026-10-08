#!/usr/bin/env python3
"""Stage byte-identical default ruleset sections for the DOS map fixture."""
import argparse
import hashlib
import shutil
from pathlib import Path


FILES = (
    ("default/terrain.ruleset", "TERRAIN.RUL"),
    ("default/units.ruleset", "UNITS.RUL"),
    ("default/governments.ruleset", "GOVERN.RUL"),
    ("default/techs.ruleset", "TECHS.RUL"),
)


def main():
    parser = argparse.ArgumentParser(
        description="Stage actual default sections as flat 8.3 MAPCHECK fixture data; not gameplay."
    )
    parser.add_argument("source_data", type=Path)
    parser.add_argument("new_output", type=Path)
    args = parser.parse_args()
    if args.new_output.exists():
        parser.error("output must not already exist")
    for original, _ in FILES:
        if not (args.source_data / original).is_file():
            parser.error(f"missing source ruleset: {original}")
    args.new_output.mkdir(parents=True)
    try:
        records = [
            "MAPCHECK fixture-only ruleset sections; NOT gameplay.",
            "Original bytes are unchanged; graphics are staged separately.",
        ]
        for original, short_name in FILES:
            data = (args.source_data / original).read_bytes()
            (args.new_output / short_name).write_bytes(data)
            records.append(f"{short_name}\t{original}\tSHA256 {hashlib.sha256(data).hexdigest()}")
        (args.new_output / "FIXTURE.TXT").write_text("\n".join(records) + "\n", encoding="ascii")
    except Exception:
        shutil.rmtree(args.new_output)
        raise
    print(f"Staged 4 byte-identical default fixture rulesets in {args.new_output}")


if __name__ == "__main__":
    main()
