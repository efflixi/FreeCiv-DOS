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


names = run("mdir", "-b", "-s", "-i", baseline, "::/").decode().splitlines()
preserved = 0
for name in names:
    if name.endswith("/"):
        continue
    target = "::/ARCHIVE/PHASE2.EXE" if name.upper() == "::/FREECIV/FREECIV.EXE" else name
    assert read(baseline, name) == read(disk, target), target
    old_attrs = run("mattrib", "-i", baseline, name).split(b"::")[0]
    new_attrs = run("mattrib", "-i", disk, target).split(b"::")[0]
    assert old_attrs == new_attrs, target
    preserved += 1

installed = {}
for guest, host in [
    ("::/FREECIV/FREECIV.EXE", evidence / "civclient.exe"),
    ("::/FREECIV/VBECHECK.EXE", evidence / "VBECHECK.EXE"),
    ("::/FREECIV/RTCHECK.EXE", project / "builds/phase3-runtime/RTCHECK.EXE"),
    ("::/FREECIV/CWSDPMI.EXE", project / "runtime/cwsdpmi-r7/bin/CWSDPMI.EXE"),
]:
    assert read(disk, guest) == host.read_bytes(), guest
    installed[guest] = {"bytes": host.stat().st_size, "sha256": digest(host)}
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
    with tempfile.NamedTemporaryFile(prefix="freeciv-phase4-fat-") as partition:
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
    ("VBE640.LOG", "dos-vbe640.log"),
    ("VBE800.LOG", "dos-vbe800.log"),
    ("VBEREPT.LOG", "dos-repeat.log"),
    ("POSTVBE.LOG", "dos-runtime-regression.log"),
    ("CIRRUS.LOG", "dos-cirrus.log"),
]:
    data = read(disk, "::/FREECIV/" + guest)
    if guest == "CIRRUS.LOG":
        assert b"BIOS display-state size query failed" in data and b"EXIT FAILURE" in data
        assert b"READY" not in data
    else:
        assert b"EXIT SUCCESS" in data and b"FAIL" not in data
        if guest in ("VBE640.LOG", "VBE800.LOG"):
            assert b"PASS Q received" in data and b"readback passed" in data
    (evidence / host).write_bytes(data)

assert read(disk, "::/FREECIV/GATE.OK").strip() == b"production_gate_preserved"
assert "text_restore_marker" in (evidence / "vbe800-exit.txt").read_text()
gate_screen = (evidence / "production-gate.txt").read_text()
assert "production_gate_preserved" in gate_screen
assert "Freeciv version 1.14.1 gui-dos-vbe" in gate_screen
assert "production graphics resources, input handling" in gate_screen
report = {
    "preserved_pre_phase4_files_and_attributes": preserved,
    "floppy_sha256": digest(floppy_path),
    "disk_sha256": digest(disk_path),
    "baseline_disk_sha256": digest(baseline_path),
    "installed_files": installed,
    "phase5_not_started": True,
}
(evidence / "verification.json").write_text(json.dumps(report, indent=2) + "\n")
print(json.dumps(report, indent=2))
