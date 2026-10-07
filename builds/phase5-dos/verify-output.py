import hashlib
import json
from pathlib import Path

directory = Path(__file__).resolve().parent
results = []
for width, height in [(640, 480), (800, 600)]:
    capture = (directory / f"render{width}.ppm").read_bytes()
    reference = (directory / f"reference{width}.ppm").read_bytes()
    actual = capture.split(b"\n", 3)
    expected = reference.split(b"\n", 3)
    assert actual[:3] == expected[:3] == [
        b"P6", f"{width} {height}".encode(), b"255"]
    assert len(actual[3]) == len(expected[3]) == width * height * 3
    mismatches = sum(actual[3][i:i + 3] != expected[3][i:i + 3]
                     for i in range(0, len(actual[3]), 3))
    assert mismatches == 0, (width, mismatches)
    results.append({"width": width, "height": height,
                    "matched_pixels": width * height,
                    "mismatched_pixels": mismatches,
                    "capture_sha256": hashlib.sha256(capture).hexdigest(),
                    "reference_sha256": hashlib.sha256(reference).hexdigest()})
(directory / "pixel-verification.json").write_text(json.dumps(results, indent=2) + "\n")
print(json.dumps(results, indent=2))
