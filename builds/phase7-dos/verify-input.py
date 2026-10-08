"""Check definitive DOS input traces, not historical/intermediate attempts."""
import json
from pathlib import Path
import re

evidence = Path(__file__).resolve().parent
report = {}
logs = {}
for name in ("KBD640", "MOUSE800", "MUI800", "BRIDGE7", "REPEAT7"):
    text = (evidence / (name + ".LOG")).read_text()
    assert "EXIT SUCCESS" in text and "FAIL" not in text, name
    assert text.count("PASS confirmed quit at safe boundary") == 1, name
    values = re.search(
        r"LOOP ticks=(\d+) events=(\d+) dispatch=(\d+) idle=(\d+) "
        r"timers=(\d+) packets=(\d+) yields=(\d+)", text)
    assert values, name
    metrics = dict(zip(("ticks", "events", "dispatch", "idle", "timers",
                        "packets", "yields"), map(int, values.groups())))
    assert metrics["events"] == metrics["dispatch"] > 0, name
    assert metrics["timers"] >= 3 and metrics["idle"] >= 10, name
    assert metrics["ticks"] <= 20 * metrics["timers"] + 128, name
    if name != "BRIDGE7":
        assert metrics["packets"] == metrics["yields"] == 0, name
    logs[name] = text
    report[name] = metrics

keyboard = logs["KBD640"]
assert "INPUT mouse=0 width=640" in keyboard
assert "ascii=52 scan=75 mods=1" in keyboard
assert "ascii=0 scan=116 mods=2" in keyboard
assert "ascii=0 scan=141 mods=2" in keyboard
for command in ("HELP", "OPTIONS", "MENU", "MOVE_EAST", "MOVE_NORTH",
                "MOVE_SOUTH_WEST", "MOVE_NORTH_EAST", "NONE"):
    assert "COMMAND " + command + "\n" in keyboard, command
assert "PASS options name=DOS7 scroll=5" in keyboard
navigation = re.findall(r"NAV tile=(\d+),(\d+) focus=(\d+),(\d+)", keyboard)
assert len(navigation) >= 6 and {n[2:] for n in navigation} == {("7", "9")}
assert navigation[-2][:2] == navigation[-1][:2] == ("7", "11")
assert "DIALOG kind=5 action=1 focus=0" in keyboard
assert "PASS rejected unavailable order" in keyboard

mouse = logs["MOUSE800"]
assert "INPUT mouse=1 width=800" in mouse
assert mouse.count("DIALOG kind=2 action=-2 focus=0") == 9
assert "x=0 y=0 pressed=1 buttons=0 released=1" in mouse
assert mouse.count("x=400 y=300 pressed=1") == 2
assert "x=400 y=300 pressed=2" in mouse
assert "DIALOG kind=4 action=-2 focus=0" in mouse
assert "COMMAND SELECT_TILE" in mouse
assert "PASS options name=Mouse scroll=3" in logs["MUI800"]
assert "PASS options name=Mouse7 scroll=3" in logs["MUI800"]
assert "x=400 y=240 pressed=1" in logs["MUI800"]
assert "ascii=0 scan=15 mods=1" in logs["MUI800"]

bridge = logs["BRIDGE7"]
assert "BRIDGE used=1 established=1 conn=1 client=1 engine=1 map=0 request=1" in bridge
assert "PASS bridge disconnected at safe boundary" in bridge
assert report["BRIDGE7"]["packets"] >= 20
assert report["BRIDGE7"]["yields"] >= report["BRIDGE7"]["packets"]
assert "PASS options name=AI scroll=5" in bridge
assert "ascii=17 scan=16 mods=2" in logs["REPEAT7"]
assert "ascii=0 scan=107 mods=4" in logs["REPEAT7"]

for name, marker in (("PROD7.OK", "production_quit_success"),
                     ("AUTO7.OK", "autoconnect_rejected"),
                     ("VERS7.OK", "version_success"),
                     ("REPEAT7.OK", "repeat_production_success")):
    assert (evidence / name).read_text().strip() == marker, name
assert "Freeciv version 1.14.1 gui-dos-vbe" in (evidence / "version-exit.txt").read_text()
assert not (evidence / "VERSION7.TXT").read_bytes()  # Upstream prints to stderr.
assert "DOS autoconnect is unavailable" in (evidence / "production-repeat-exit.txt").read_text()
runtime = (evidence / "POSTINP.LOG").read_text()
assert "EXIT SUCCESS" in runtime and "FAIL" not in runtime
assert "PASS QUIT: --batch" in runtime
report["idle_acceptance"] = "timers>=3, idle>=10, ticks<=20*timers+128"
report["quick_click_seconds"] = 0.025
report["quick_toolbar_clicks_received"] = 9
report["scope"] = "persistent pregame UI, real-state fixture, actual pregame bridge; no game start"
(evidence / "input-verification.json").write_text(json.dumps(report, indent=2) + "\n")
print(json.dumps(report, indent=2))
