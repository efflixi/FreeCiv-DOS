"""Send explicit keyboard/relative mouse events to the private DOS test VM."""
import json
import socket
import sys
import time


def command(stream, name, arguments=None):
    stream.write((json.dumps({"execute": name, "arguments": arguments or {}})
                  + "\n").encode())
    stream.flush()
    while True:
        reply = json.loads(stream.readline())
        if "error" in reply:
            raise RuntimeError(reply["error"])
        if "return" in reply:
            return reply["return"]


with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as sock:
    sock.settimeout(15)
    sock.connect("/tmp/freeciv-phase3-ad752174/qmp.sock")
    stream = sock.makefile("rwb")
    json.loads(stream.readline())
    command(stream, "qmp_capabilities")
    if sys.argv[1] == "keys":
        for key in sys.argv[2:]:
            response = command(stream, "human-monitor-command",
                               {"command-line": "sendkey " + key})
            if response:
                raise RuntimeError(response)
            time.sleep(0.16)
    elif sys.argv[1] == "mouse":
        dx, dy = map(int, sys.argv[2:4])
        command(stream, "input-send-event", {"events": [
            {"type": "rel", "data": {"axis": "x", "value": dx}},
            {"type": "rel", "data": {"axis": "y", "value": dy}}]})
    elif sys.argv[1] == "button":
        command(stream, "input-send-event", {"events": [{
            "type": "btn", "data": {"button": sys.argv[2],
                                    "down": sys.argv[3] == "down"}}]})
    elif sys.argv[1] == "click":
        duration = float(sys.argv[3])
        if not 0 < duration <= 1:
            raise ValueError("Click duration must be between 0 and 1 second")
        for down in (True, False):
            command(stream, "input-send-event", {"events": [{
                "type": "btn", "data": {"button": sys.argv[2], "down": down}}]})
            if down:
                time.sleep(duration)
    else:
        raise ValueError("Use keys KEY..., mouse DX DY, button left|right down|up, or click BUTTON SECONDS")
