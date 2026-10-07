import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess

project = Path.cwd()
session = Path("/home/efflixi/.copilot/session-state/ad752174-0eda-40ca-ba61-6614cfe47e56/files")
staging = session / "phase3-vm"
evidence = project / "builds/phase3-runtime"
evidence.mkdir(exist_ok=True)
disk = str(staging / "partitioned-c.img") + "@@32256"
baseline = str(staging / "drive-c.img")


def command(*args):
    return subprocess.run(args, check=True, stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE).stdout


def digest(data):
    return hashlib.sha256(data).hexdigest()


def file_digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def read(image, name):
    return command("mtype", "-i", image, name)


manifest = json.loads((session / "c-drive-layout-manifest.json").read_text())
for record in manifest.values():
    original_path = record["destination"]
    staged_path = original_path
    if staged_path == "::/FREECIV/FREECIV.EXE":
        staged_path = "::/ARCHIVE/OLDCLI.EXE"
    data = read(disk, staged_path)
    assert len(data) == record["bytes"], staged_path
    assert digest(data) == record["sha256"], staged_path
    old_attrs = command("mattrib", "-i", baseline, original_path).split(b"::")[0]
    new_attrs = command("mattrib", "-i", disk, staged_path).split(b"::")[0]
    assert old_attrs == new_attrs, staged_path

expected_files = {
    "::/FREECIV/FREECIV.EXE": project / "builds/phase2-dos/civclient.exe",
    "::/FREECIV/RTCHECK.EXE": session / "phase3-runtime-build/RTCHECK.EXE",
    "::/FREECIV/CWSDPMI.EXE": project / "runtime/cwsdpmi-r7/bin/CWSDPMI.EXE",
    "::/FREECIV/CWSDPMI.DOC": project / "runtime/cwsdpmi-r7/bin/cwsdpmi.doc",
    "::/FREECIV/DATA/RUNTIME.DAT": project / "dos-vm/RUNTIME.DAT",
    "::/AUTOEXEC.BAT": project / "dos-vm/AUTOEXEC.BAT",
    "::/CONFIG.SYS": project / "dos-vm/CONFIG.SYS",
}
installed = {}
for name, host in expected_files.items():
    data = read(disk, name)
    assert data == host.read_bytes(), name
    installed[name] = {"bytes": len(data), "sha256": digest(data)}
assert installed["::/FREECIV/FREECIV.EXE"]["sha256"] == (
    "bfc18c218e5953b3a30bcb3c3b2b5ead16b6401f0b0bd660c2657ff7d13930df")
assert read(disk, "::/FREECIV/WRITE.TXT").strip() == b"cwrite"

old_floppy = str(project / "DOS622.img")
new_floppy = str(staging / "drive-a.img")
old_names = command("mdir", "-b", "-s", "-i", old_floppy, "::/").splitlines()
new_names = command("mdir", "-b", "-s", "-i", new_floppy, "::/").splitlines()
assert sorted(old_names) == sorted(new_names)
floppy_count = 0
for raw_name in old_names:
    if raw_name.endswith(b"/"):
        continue
    name = raw_name.decode()
    if name.upper() == "::/AUTOEXEC.BAT":
        continue
    assert read(old_floppy, name) == read(new_floppy, name), name
    assert command("mattrib", "-i", old_floppy, name) == command(
        "mattrib", "-i", new_floppy, name), name
    floppy_count += 1
assert floppy_count == 60, floppy_count
assert read(new_floppy, "::/AUTOEXEC.BAT") == (project / "dos-vm/BOOT_A.BAT").read_bytes()
with Path(old_floppy).open("rb") as old, Path(new_floppy).open("rb") as new:
    assert old.read(512) == new.read(512)

wrapped = staging / "wrapper-check.img"
result = command("python3", str(project / "dos-vm/partition_drive.py"),
                 baseline, str(wrapped))
(evidence / "partition-wrapper.txt").write_bytes(result)
with Path(baseline).open("rb") as original, wrapped.open("rb") as partitioned:
    boot = bytearray(original.read(512))
    struct.pack_into("<HHI", boot, 24, 63, 16, 63)
    partitioned.seek(32256)
    assert partitioned.read(512) == boot
    while chunk := original.read(1024 * 1024):
        assert partitioned.read(len(chunk)) == chunk
for source, target in [(baseline, str(wrapped)), (old_floppy, str(staging / "bad-wrapper.img"))]:
    result = subprocess.run(["python3", str(project / "dos-vm/partition_drive.py"),
                             source, target], capture_output=True)
    assert result.returncode != 0
assert not (staging / "bad-wrapper.img").exists()
wrapped.unlink()

with (staging / "partitioned-c.img").open("rb") as disk_file:
    mbr = disk_file.read(512)
    assert mbr[:446] == bytes(446), "No standalone MBR loader is supplied"
    assert mbr[510:512] == b"\x55\xaa"
    assert mbr[446] == 0x80 and mbr[450] == 0x06
    assert struct.unpack_from("<II", mbr, 454) == (63, 524288)
    assert mbr[462:510] == bytes(48)
    assert (staging / "partitioned-c.img").stat().st_size == 521 * 16 * 63 * 512
    disk_file.seek(32256)
    boot = disk_file.read(512)
    assert struct.unpack_from("<HHI", boot, 24) == (63, 16, 63)
    disk_file.seek(32256)
    with (staging / "fat-check.img").open("xb") as filesystem:
        remaining = 524288 * 512
        while remaining:
            chunk = disk_file.read(min(remaining, 1024 * 1024))
            assert chunk
            filesystem.write(chunk)
            remaining -= len(chunk)
(evidence / "fat-check.txt").write_bytes(command(
    "fsck.fat", "-n", str(staging / "fat-check.img")))
(evidence / "partition-check.txt").write_bytes(command(
    "sfdisk", "--verify", str(staging / "partitioned-c.img")))
(staging / "fat-check.img").unlink()

root_names = command("mdir", "-b", "-i", disk, "::/").decode().splitlines()
assert {name.upper().rstrip("/") for name in root_names} == {
    "::/IO.SYS", "::/MSDOS.SYS", "::/COMMAND.COM", "::/DRVSPACE.BIN",
    "::/AUTOEXEC.BAT", "::/CONFIG.SYS", "::/DOS", "::/FREECIV", "::/ARCHIVE"}
(evidence / "root-directory.txt").write_bytes(command("mdir", "-i", disk, "::/"))
for guest, host in [
    ("RUNTIME.LOG", "dos-runtime.log"),
    ("NOPAGE16.LOG", "dos-no-paging16.log"),
    ("ROOT.LOG", "dos-root.log"),
    ("FROMA.LOG", "dos-from-a.log"),
    ("FROMDIR.LOG", "dos-from-install.log"),
    ("NODATA.LOG", "dos-missing-data.log"),
    ("REBUILT.LOG", "dos-rebuilt.log"),
]:
    data = read(disk, "::/FREECIV/" + guest)
    if guest == "NODATA.LOG":
        assert b"FAIL DATA:" in data and b"EXIT FAILURE" in data
        assert b"READY RTCHECK" not in data
    else:
        assert b"PASS ENGINE: reopen" in data and b"EXIT SUCCESS" in data
        if guest in ("RUNTIME.LOG", "NOPAGE16.LOG"):
            assert b"PASS QUIT: Q received" in data
    (evidence / host).write_bytes(data)
for screen in staging.glob("*.txt"):
    shutil.copyfile(screen, evidence / screen.name)
shutil.copyfile(session / "phase3-runtime-build/RTCHECK.EXE", evidence / "RTCHECK.EXE")
shutil.copyfile(session / "phase3-runtime-build/compile.log", evidence / "compile.log")
shutil.copyfile(session / "phase3-runtime-build/engine.symbols", evidence / "engine.symbols")
shutil.copyfile(session / "phase3-runtime-build/engine.isolated", evidence / "engine.isolated")
shutil.copyfile(session / "phase2-final-build/build/config.h", evidence / "config.h")
shutil.copyfile(Path(__file__), evidence / "verify-images.py")
report = {
    "preserved_original_c_files_and_attributes": len(manifest),
    "preserved_non_autoexec_floppy_files": floppy_count,
    "installed_files": installed,
    "disk_sha256": file_digest(staging / "partitioned-c.img"),
    "floppy_sha256": file_digest(staging / "drive-a.img"),
    "phase2_config_sha256": file_digest(evidence / "config.h"),
    "tested_rebuild_sha256": file_digest(evidence / "RTREBLD.EXE"),
    "source_sha256": {
        str(path.relative_to(project)): file_digest(path)
        for path in [
            project / "freeciv-1.14.1/client/offline/runtime_check.c",
            project / "freeciv-1.14.1/client/offline/build-runtime-check.sh",
            project / "freeciv-1.14.1/client/offline/build-engine.sh",
            project / "dos-vm/partition_drive.py",
        ]
    },
}
(evidence / "verification.json").write_text(json.dumps(report, indent=2) + "\n")
print(json.dumps(report, indent=2))
