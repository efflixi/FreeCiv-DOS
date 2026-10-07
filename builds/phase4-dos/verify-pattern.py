import hashlib
import json
from pathlib import Path

directory = Path(__file__).resolve().parent
bars = [(255, 0, 0), (0, 255, 0), (0, 0, 255), (255, 255, 255),
        (0, 0, 0), (255, 255, 0), (0, 255, 255), (255, 0, 255)]
results = []
for width, height in [(640, 480), (800, 600)]:
    path = directory / f"vbe{width}.ppm"
    data = path.read_bytes()
    magic, dimensions, maximum, pixels = data.split(b"\n", 3)
    assert magic == b"P6" and maximum == b"255"
    assert dimensions == f"{width} {height}".encode()
    assert len(pixels) == width * height * 3
    for y in range(height):
        for x in range(width):
            if y < height // 2:
                expected = bars[x * 8 // width]
            else:
                expected = (255, 255, 255) if (x // 16 + y // 16) & 1 else (0, 0, 0)
            offset = (y * width + x) * 3
            assert pixels[offset:offset + 3] == bytes(expected), (path.name, x, y)
    results.append({"image": path.name, "width": width, "height": height,
                    "matched_pixels": width * height, "mismatched_pixels": 0,
                    "sha256": hashlib.sha256(data).hexdigest()})
(directory / "pattern-verification.json").write_text(json.dumps(results, indent=2) + "\n")
print(json.dumps(results, indent=2))
