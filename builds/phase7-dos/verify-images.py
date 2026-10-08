"""Verify stopped Phase 7 images against the complete published Phase 6 disk."""
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
    target = "::/ARCHIVE/PHASE6.EXE" if name.upper() == "::/FREECIV/FREECIV.EXE" else name
    assert read(baseline, name) == read(disk, target), target
    assert (run("mattrib", "-i", baseline, name).split(b"::")[0]
            == run("mattrib", "-i", disk, target).split(b"::")[0]), target
    preserved += 1

installed = {}
for guest, host in [
    ("::/FREECIV/FREECIV.EXE", evidence / "civclient.exe"),
    ("::/FREECIV/INPUTCHK.EXE", evidence / "INPUTCHK.EXE"),
    ("::/FREECIV/MAPCHECK.EXE", project / "builds/phase6-dos/MAPCHECK.EXE"),
    ("::/FREECIV/RNDCHECK.EXE", project / "builds/phase5-dos/RNDCHECK.EXE"),
    ("::/FREECIV/VBECHECK.EXE", project / "builds/phase4-dos/VBECHECK.EXE"),
    ("::/FREECIV/RTCHECK.EXE", project / "builds/phase3-runtime/RTCHECK.EXE"),
    ("::/DOS/CTMOUSE.EXE", project / "runtime/cutemouse/bin/CTMOUSE.EXE"),
    ("::/DOS/CTMOUSE.TXT", project / "runtime/cutemouse/CTMOUSE.TXT"),
    ("::/DOS/CTMLIC.TXT", project / "runtime/cutemouse/COPYING"),
    ("::/DOS/CTMSRC.ZIP", project / "runtime/cutemouse/ctmouse.zip"),
]:
    assert read(disk, guest) == host.read_bytes(), guest
    installed[guest] = {"bytes": host.stat().st_size, "sha256": digest(host)}
for name in ("KBD640.LOG", "MOUSE800.LOG", "MUI800.LOG", "BRIDGE7.LOG",
             "REPEAT7.LOG", "POSTINP.LOG", "PROD7.OK", "VERSION7.TXT",
             "AUTO7.OK", "VERS7.OK", "REPEAT7.OK"):
    assert read(disk, "::/FREECIV/" + name) == (evidence / name).read_bytes(), name

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
    with tempfile.NamedTemporaryFile(prefix="freeciv-phase7-fat-") as partition:
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
report = {
    "preserved_pre_phase7_files_and_attributes": preserved,
    "floppy_sha256": digest(floppy_path),
    "disk_sha256": digest(disk_path),
    "baseline_disk_sha256": digest(baseline_path),
    "installed_files": installed,
    "phase8_not_started": True,
}
(evidence / "verification.json").write_text(json.dumps(report, indent=2) + "\n")
print(json.dumps(report, indent=2))
