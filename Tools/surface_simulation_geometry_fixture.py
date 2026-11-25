"""Generate a repeatable XY test patch in four supported geometry formats."""

import argparse
import math
import struct
import zlib
from pathlib import Path
from collections.abc import Callable
from collections.abc import Callable

WIDTH = 0.09
LENGTH = 0.06
AMPLITUDE = 0.009
COLS, ROWS = 161, 109


def height(u: float, v: float) -> float:
    return (0.002 * math.sin(math.tau * u)
            - 0.0012 * math.cos(math.tau * v)
            + 0.00052 * math.sin(math.tau * 5 * u) * math.cos(math.tau * 4 * v)
            + 0.00014 * math.cos(math.tau * 13 * u) * math.cos(math.tau * 9 * v))


def p6(path: Path, transform: Callable[[float, float], tuple[float, ...]]) -> None:
    pixels = bytearray()
    for y in range(ROWS):
        v = 1.0 - (y + 0.5) / ROWS
        for x in range(COLS):
            u = (x + 0.5) / COLS
            pixels.extend(round(255 * min(1.0, max(0.0, a))) for a in transform(u, v))
    path.write_bytes(f"P6\n{COLS} {ROWS}\n255\n".encode("ascii") + pixels)


def normal(u: float, v: float) -> tuple[float, float, float]:
    step = 1.0e-6
    dx = (height(u + step, v) - height(u - step, v)) / (2 * step * WIDTH)
    dy = (height(u, v + step) - height(u, v - step)) / (2 * step * LENGTH)
    norm = math.sqrt(1.0 + dx * dx + dy * dy)
    nx, ny, nz = (0.5 + 0.5 * a / norm for a in (-dx, -dy, 1.0))
    return nx, ny, nz


def write_fixture(path: Path) -> None:
    path.mkdir(parents=True, exist_ok=True)
    p6(path / "height.ppm", lambda u, v: (0.5 + height(u, v) / AMPLITUDE,) * 3)
    p6(path / "displacement.ppm", lambda u, v: (0.5 + height(u, v) / (2 * AMPLITUDE),) * 3)
    p6(path / "normal.ppm", normal)
    with (path / "patch.obj").open("w", encoding="ascii") as obj:
        obj.write("o SurfaceValidationPatch\n")
        for y in range(ROWS):
            for x in range(COLS):
                u, v = x / (COLS - 1), y / (ROWS - 1)
                obj.write(f"v {u * WIDTH:.8f} {v * LENGTH:.8f} {height(u, v):.9f}\n")
        for y in range(ROWS - 1):
            for x in range(COLS - 1):
                i = 1 + x + y * COLS
                obj.write(f"f {i} {i + 1} {i + 1 + COLS}\nf {i} {i + 1 + COLS} {i + COLS}\n")


def smoothstep(low: float, high: float, value: float) -> float:
    t = min(1.0, max(0.0, (value - low) / (high - low)))
    return t * t * (3.0 - 2.0 * t)


def cavity_height(u: float, v: float) -> float:
    """Return the pocket, rim, and outlet elevation in meters."""
    rim = 0.00845 + 0.00012 * math.sin(37 * u + 12 * v) * math.cos(27 * v - 15 * u)
    center = 0.54 + 0.055 * math.sin((u - 0.22) * 9)
    radius = math.hypot((u - 0.252) / 0.215, (v - center) / 0.365)
    floor = 0.0003 + 0.00035 * math.cos(26 * u + 17 * v) ** 2
    bowl = floor + (rim - floor) * smoothstep(0.28, 1.03, radius)
    notch_y = 0.525 + 0.12 * math.sin((u - 0.57) * 7)
    notch_level = 0.00495 - 0.00315 * smoothstep(0.50, 1.0, u)
    notch = notch_level + (rim - notch_level) * smoothstep(0.055, 0.20, abs(v - notch_y))
    notch += (rim - notch) * (1.0 - smoothstep(0.34, 0.45, u))
    return min(bowl, notch)


def twin_pockets_height(u: float, v: float) -> float:
    """Cut a second pocket beside the source bowl, inside the same coarse cell."""
    radius = math.hypot((u - 0.31) / 0.135, (v - 0.16) / 0.135)
    side_pocket = 0.0012 + 0.0076 * smoothstep(0.25, 1.0, radius)
    return min(cavity_height(u, v), side_pocket)


def terraced_channel_height(u: float, v: float) -> float:
    """Add a downstream catchment and a narrower curved channel."""
    center = 0.59 - 0.11 * math.sin((u - 0.65) * 8.0)
    radius = math.hypot((u - 0.69) / 0.20, (v - center) / 0.29)
    catchment = 0.0010 + 0.0078 * smoothstep(0.22, 1.08, radius)
    return min(cavity_height(u, v), catchment)


def write_geometry_set(path: Path, name: str, height: Callable[[float, float], float]) -> None:
    path.mkdir(parents=True, exist_ok=True)
    width, length = 0.100, 0.062
    columns, rows = 145, 97
    images = {key: bytearray() for key in ("height", "displacement", "normal")}
    for y in range(rows):
        for pixels in images.values():
            pixels.append(0)
        v = 1.0 - (y + 0.5) / rows
        for x in range(columns):
            u = (x + 0.5) / columns
            z = height(u, v)
            images["height"].extend((round(255 * z / 0.010),) * 3)
            images["displacement"].extend((round(255 * (0.5 + (z - 0.006) / 0.014)),) * 3)
            step = 0.0001
            dx = (height(u + step, v) - height(u - step, v)) / (2 * step * width)
            dy = (height(u, v + step) - height(u, v - step)) / (2 * step * length)
            norm = math.sqrt(1 + dx * dx + dy * dy)
            images["normal"].extend(round(255 * (0.5 + 0.5 * n / norm)) for n in (-dx, -dy, 1.0))

    def chunk(kind: bytes, data: bytes) -> bytes:
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))

    header = chunk(b"IHDR", struct.pack(">IIBBBBB", columns, rows, 8, 2, 0, 0, 0))
    for key, pixels in images.items():
        png = b"\x89PNG\r\n\x1a\n" + header + chunk(b"IDAT", zlib.compress(pixels, level=9)) + chunk(b"IEND", b"")
        (path / f"{name}.{key}.png").write_bytes(png)

    with (path / f"{name}.obj").open("w", encoding="ascii") as obj:
        obj.write(f"o {name}\n")
        for y in range(rows):
            for x in range(columns):
                u, v = x / (columns - 1), y / (rows - 1)
                obj.write(f"v {u * width:.8f} {v * length:.8f} {height(u, v):.9f}\n")
        for y in range(rows - 1):
            for x in range(columns - 1):
                i = 1 + x + y * columns
                obj.write(f"f {i} {i + 1} {i + 1 + columns}\nf {i} {i + 1 + columns} {i + columns}\n")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--cavity", action="store_true", help="Generate the three Surface Simulation geometry sets.")
    args = parser.parse_args()
    if args.cavity:
        for name, height in (
            ("pocket", cavity_height),
            ("twin-pockets", twin_pockets_height),
            ("terraced-channel", terraced_channel_height),
        ):
            write_geometry_set(args.output, name, height)
    else:
        write_fixture(args.output)


if __name__ == "__main__":
    main()
