"""Verify a stopped Phase 6 candidate against its preceding DOS baseline."""
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

evidence = Path(__file__).resolve().parent
project = evidence.parent.parent
disk_path, floppy_path, baseline_path = map(Path, sys.argv[1:4])
disk = str(disk_path) + "@@32256"
baseline = str(baseline_path) + "@@32256"


def run(*args):
    return subprocess.run(args, check=True, stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE).stdout


def read(image, name):
    return run("mtype", "-i", image, name)


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


preserved = 0
for name in run("mdir", "-b", "-s", "-i", baseline, "::/").decode().splitlines():
    if name.endswith("/"):
        continue
    target = "::/ARCHIVE/PHASE5.EXE" if name.upper() == "::/FREECIV/FREECIV.EXE" else name
    assert read(baseline, name) == read(disk, target), target
    assert (run("mattrib", "-i", baseline, name).split(b"::")[0]
            == run("mattrib", "-i", disk, target).split(b"::")[0]), target
    preserved += 1

installed = {}
for guest, host in [
    ("::/FREECIV/FREECIV.EXE", evidence / "civclient.exe"),
    ("::/FREECIV/MAPCHECK.EXE", evidence / "MAPCHECK.EXE"),
    ("::/FREECIV/RNDCHECK.EXE", project / "builds/phase5-dos/RNDCHECK.EXE"),
    ("::/FREECIV/VBECHECK.EXE", project / "builds/phase4-dos/VBECHECK.EXE"),
    ("::/FREECIV/RTCHECK.EXE", project / "builds/phase3-runtime/RTCHECK.EXE"),
    ("::/FREECIV/CWSDPMI.EXE", project / "runtime/cwsdpmi-r7/bin/CWSDPMI.EXE"),
]:
    assert read(disk, guest) == host.read_bytes(), guest
    installed[guest] = {"bytes": host.stat().st_size, "sha256": digest(host)}
resources = {}
for host in sorted((evidence / "GFX8").iterdir()):
    if not host.is_file():
        continue
    assert read(disk, "::/FREECIV/GFX8/" + host.name) == host.read_bytes(), host.name
    resources[host.name] = digest(host)
for original, alias in [
    ("terrain.ruleset", "TERRAIN.RUL"), ("units.ruleset", "UNITS.RUL"),
    ("governments.ruleset", "GOVERN.RUL"), ("techs.ruleset", "TECHS.RUL"),
]:
    host = project / "freeciv-1.14.1/data/default" / original
    assert read(disk, "::/FREECIV/GFX8/" + alias) == host.read_bytes(), alias
    resources[alias] = digest(host)
assert floppy_path.read_bytes() == (project / "DOS622.img").read_bytes()
assert read(str(floppy_path), "::/AUTOEXEC.BAT") == (project / "dos-vm/BOOT_A.BAT").read_bytes()
with disk_path.open("rb") as current, baseline_path.open("rb") as old:
    assert current.read(32256 + 512) == old.read(32256 + 512)
    current.seek(0)
    mbr = current.read(512)
    assert mbr[446] == 0x80 and mbr[450] == 6
    assert struct.unpack_from("<II", mbr, 454) == (63, 524288)
    assert disk_path.stat().st_size == 521 * 16 * 63 * 512
    current.seek(32256)
    with tempfile.NamedTemporaryFile(prefix="freeciv-phase6-fat-") as partition:
        remaining = 524288 * 512
        while remaining:
            chunk = current.read(min(remaining, 1024 * 1024))
            assert chunk
            partition.write(chunk)
            remaining -= len(chunk)
        partition.flush()
        (evidence / "fat-check.txt").write_bytes(run("fsck.fat", "-n", partition.name))
(evidence / "partition-check.txt").write_bytes(run("sfdisk", "--verify", str(disk_path)))
root = run("mdir", "-b", "-i", disk, "::/").decode().splitlines()
assert {name.upper().rstrip("/") for name in root} == {
    "::/IO.SYS", "::/MSDOS.SYS", "::/COMMAND.COM", "::/DRVSPACE.BIN",
    "::/AUTOEXEC.BAT", "::/CONFIG.SYS", "::/DOS", "::/FREECIV", "::/ARCHIVE"}

for guest, host in [
    ("MAP640.LOG", "dos-map640.log"),
    ("MAP800.LOG", "dos-map800.log"),
    ("MAPREPT.LOG", "dos-repeat.log"),
    ("POSTMAP.LOG", "dos-runtime-regression.log"),
]:
    data = read(disk, "::/FREECIV/" + guest)
    assert b"EXIT SUCCESS" in data and b"FAIL" not in data
    if guest.startswith("MAP"):
        assert b"PASS Q received" in data
        assert b"PASS resource/map fixture and full readback" in data
        assert b"PASS repeated map bytes=0" in data
    (evidence / host).write_bytes(data)
assert read(disk, "::/FREECIV/GATE6.OK").strip() == b"production_gate_preserved"
assert "text_restore_marker" in (evidence / "map800-exit.txt").read_text()
gate_screen = (evidence / "production-gate.txt").read_text()
assert "production_gate_preserved" in gate_screen
assert "Freeciv version 1.14.1 gui-dos-vbe" in gate_screen
report = {
    "preserved_pre_phase6_files_and_attributes": preserved,
    "floppy_sha256": digest(floppy_path),
    "disk_sha256": digest(disk_path),
    "baseline_disk_sha256": digest(baseline_path),
    "installed_files": installed,
    "installed_graphics_and_fixture_sha256": resources,
    "phase7_not_started": True,
}
(evidence / "verification.json").write_text(json.dumps(report, indent=2) + "\n")
print(json.dumps(report, indent=2))
