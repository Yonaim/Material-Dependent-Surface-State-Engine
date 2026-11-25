"""Render Surface Simulation JSON as a self-contained validation dashboard."""

import argparse
import html
import json
import math
import struct
import zlib
from pathlib import Path

WIDTH, HEIGHT = 1080, 690
BACKGROUND = (15, 23, 42)
PANEL = (23, 36, 59)
GRID = (48, 64, 89)
TEXT = (229, 238, 250)


def color(value: float, lo: float, hi: float) -> tuple[int, int, int]:
    """Piecewise-linear palette shared by the exported images."""
    t = min(1.0, max(0.0, (value - lo) / max(1.0e-12, hi - lo)))
    stops = ((31, 28, 74), (34, 138, 141), (247, 221, 102))
    index = min(1, int(t * 2))
    red, green, blue = (
        round(a + (b - a) * (t * 2 - index))
        for a, b in zip(stops[index], stops[index + 1])
    )
    return red, green, blue


def rgb(value: tuple[int, int, int]) -> str:
    return "#%02x%02x%02x" % value

# =============================================================================
# 01. Dependency-Free Label Rasterization
# =============================================================================
#region 01_Dependency_Free_Label_Rasterization

# Each base-32 digit encodes one 5-bit row, with the left pixel in the high bit.
GLYPH_ROWS = {
    "A": "ehh vhhh", "B": "uhh uhhu", "C": "fgg gggf", "D": "uhh hhhu",
    "E": "vgg uggv", "F": "vgg uggg", "G": "fgj hhhf", "H": "hhh vhhh",
    "I": "e44 444e", "J": "722 22ic", "K": "hik okih", "L": "ggg gggv",
    "M": "hrl hhhh", "N": "hpl jhhh", "O": "ehh hhhe", "P": "uhh uggg",
    "Q": "ehh hlid", "R": "uhh ukih", "S": "fgg e11u", "T": "v44 4444",
    "U": "hhh hhhe", "V": "hhh haa4", "W": "hhh hlrh", "X": "hha 4ahh",
    "Y": "hha 4444", "Z": "v12 48gv",
    "0": "ehj lphe", "1": "4c4 444e", "2": "eh1 248v", "3": "u11 e11u",
    "4": "26a iv22", "5": "vgg u11u", "6": "egg uhhe", "7": "v12 4888",
    "8": "ehh ehhe", "9": "ehh f11e",
    " ": "000 0000", ".": "000 00cc", ",": "000 0cc8", ":": "0cc 0cc0",
    "-": "000 v000", "+": "044 v440", "/": "112 48gg", "=": "00v 0v00",
    "(": "248 8842", ")": "842 2248", "[": "e88 888e", "]": "e22 222e",
    "%": "p92 489j", "&": "ca8 lakd", "_": "000 000v", "?": "eh1 2404",
}
GLYPHS = {char: tuple(int(row, 32) for row in rows.replace(" ", ""))
          for char, rows in GLYPH_ROWS.items()}
LABEL_TRANSLATION = str.maketrans({
    "\u00b2": "^2", "\u00b3": "^3", "\u00b7": ".", "\u2022": "|",
    "\u2013": "-", "\u2014": "-", "\u2192": "->",
})
GLYPHS["|"] = (4,) * 7
GLYPHS["^"] = (4, 10, 17, 0, 0, 0, 0)
GLYPHS[">"] = (16, 8, 4, 2, 4, 8, 16)


def raster_label(pixels: bytearray, height: int, x: int, baseline: int, label: str,
                 size: int, fill: tuple[int, int, int]) -> None:
    """Keep report names, scales and units legible when Pillow is unavailable."""
    scale = max(1.0, size / 9)
    top = baseline - round(scale * 7)
    rgb_bytes = bytes(fill)
    for index, char in enumerate(label.translate(LABEL_TRANSLATION).upper()):
        left = x + index * 6 * scale
        if left >= WIDTH:
            break
        for row, mask in enumerate(GLYPHS.get(char, GLYPHS["?"])):
            first_y = max(0, round(top + row * scale))
            last_y = min(height, round(top + (row + 1) * scale))
            for column in range(5):
                if not mask & (1 << (4 - column)):
                    continue
                first_x = max(0, round(left + column * scale))
                last_x = min(WIDTH, round(left + (column + 1) * scale))
                for py in range(first_y, last_y):
                    offset = (py * WIDTH + first_x) * 3
                    pixels[offset:offset + (last_x - first_x) * 3] = rgb_bytes * (last_x - first_x)

#endregion


# =============================================================================
# 02. Drawing and Image Output
# =============================================================================
#region 02_Drawing_and_Image_Output

class Canvas:
    """Draw once to an SVG and a dependency-free RGB PNG."""

    def __init__(self, height: int = HEIGHT) -> None:
        self.height = height
        self.pixels = bytearray(bytes(BACKGROUND) * (WIDTH * height))
        self.svg = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{WIDTH}" height="{height}" '
                    f'viewBox="0 0 {WIDTH} {height}">']
        self.rect(0, 0, WIDTH, height, BACKGROUND)
        try:
            from PIL import Image, ImageDraw, ImageFont
        except ImportError:
            self.fonts = None
        else:
            self.fonts = (Image, ImageDraw, ImageFont)
        self.labels: list[tuple[int, int, str, int, tuple[int, int, int]]] = []

    def rect(self, x: int, y: int, w: int, h: int, fill: tuple[int, int, int]) -> None:
        self.svg.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" fill="{rgb(fill)}"/>')
        stripe = bytes(fill) * w
        for j in range(y, y + h):
            start = (j * WIDTH + x) * 3
            self.pixels[start:start + w * 3] = stripe

    def line(self, a: tuple[int, int], b: tuple[int, int], fill: tuple[int, int, int]) -> None:
        self.svg.append(f'<line x1="{a[0]}" y1="{a[1]}" x2="{b[0]}" y2="{b[1]}" '
                        f'stroke="{rgb(fill)}" stroke-width="2"/>')
        count = max(abs(b[0] - a[0]), abs(b[1] - a[1]), 1)
        for i in range(count + 1):
            x = round(a[0] + (b[0] - a[0]) * i / count)
            y = round(a[1] + (b[1] - a[1]) * i / count)
            index = (y * WIDTH + x) * 3
            self.pixels[index:index + 6] = bytes(fill) * 2

    def label(self, x: int, y: int, label: str, size: int = 14,
              fill: tuple[int, int, int] = TEXT) -> None:
        self.svg.append(f'<text x="{x}" y="{y}" font-family="Arial,sans-serif" font-size="{size}" '
                        f'fill="{rgb(fill)}">{html.escape(label)}</text>')
        self.labels.append((x, y, label, size, fill))

    def save(self, stem: Path, png_only: bool = False) -> None:
        if not png_only:
            stem.with_suffix(".svg").write_text("\n".join([*self.svg, "</svg>"]), encoding="utf-8")
        if self.fonts:
            image_api, draw_api, font_api = self.fonts
            image = image_api.frombytes("RGB", (WIDTH, self.height), bytes(self.pixels))
            draw = draw_api.Draw(image)
            for x, y, label, size, fill in self.labels:
                try:
                    font = font_api.truetype("DejaVuSans.ttf", size)
                except OSError:
                    try:
                        font = font_api.truetype("arial.ttf", size)
                    except OSError:
                        font = font_api.load_default()
                draw.text((x, y - size), label, fill=fill, font=font)
            image.save(stem.with_suffix(".png"))
            return

        # Draw labels on a copy so repeated exports preserve the original canvas.
        pixels = self.pixels.copy()
        for x, y, label, size, fill in self.labels:
            raster_label(pixels, self.height, x, y, label, size, fill)

        def chunk(kind: bytes, value: bytes) -> bytes:
            return struct.pack(">I", len(value)) + kind + value + struct.pack(">I", zlib.crc32(kind + value))

        stride = WIDTH * 3
        scan = b"".join(b"\0" + pixels[i:i + stride] for i in range(0, len(pixels), stride))
        png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", WIDTH, self.height, 8, 2, 0, 0, 0))
        png += chunk(b"IDAT", zlib.compress(scan, level=7)) + chunk(b"IEND", b"")
        stem.with_suffix(".png").write_bytes(png)

#endregion


def draw_field(canvas: Canvas, values: list[float], cols: int, rows: int, x: int, y: int,
               w: int, h: int, title: str, zero_based: bool) -> None:
    if len(values) != cols * rows or min(cols, rows) <= 0:
        raise ValueError(f"Invalid raster dimensions for {title}.")
    lo, hi = (0.0 if zero_based else min(values)), max(values)
    canvas.rect(x - 12, y - 28, w + 24, h + 59, PANEL)
    canvas.label(x, y - 8, title)
    for row in range(rows):
        top = y + (rows - row - 1) * h // rows
        bottom = y + (rows - row) * h // rows
        for col in range(cols):
            left = x + col * w // cols
            right = x + (col + 1) * w // cols
            value = values[row * cols + col]
            canvas.rect(left, top, right - left, bottom - top, color(value, lo, hi))
    for step in range(w):
        canvas.rect(x + step, y + h + 5, 1, 5, color(lo + (hi - lo) * step / max(1, w - 1), lo, hi))
    canvas.label(x, y + h + 25, f"{lo:.3g}", 11)
    canvas.label(x + w - 40, y + h + 25, f"{hi:.3g}", 11)


def draw_series(canvas: Canvas, series: list[dict], x: int, y: int, w: int, h: int,
                title: str, definitions: list[tuple[str, str, tuple[int, int, int]]],
                horizontal_unit: str = "s", horizontal_key: str = "seconds",
                vertical_range: tuple[float, float] | None = None) -> None:
    canvas.rect(x - 12, y - 28, w + 24, h + 65, PANEL)
    canvas.label(x, y - 8, title)
    max_horizontal = max((sample[horizontal_key] for sample in series), default=1.0)
    max_value = max((sample[key] for sample in series for key, _, _ in definitions), default=1.0)
    max_value = max(0.001, max_value)
    min_value = min(0.0, min((sample[key] for sample in series for key, _, _ in definitions), default=0.0))
    if vertical_range is not None:
        min_value, max_value = vertical_range
    span = max_value - min_value
    for i in range(5):
        row = y + h * i // 4
        canvas.rect(x + 31, row, w - 31, 1, GRID)
        precision = 3 if vertical_range else 2
        canvas.label(x, row + 4, f"{max_value - span * i / 4:.{precision}g}", 10)
    for i, (key, name, fill) in enumerate(definitions):
        points = [(x + 31 + round((w - 31) * sample[horizontal_key] / max_horizontal),
                   y + h - round(h * (sample[key] - min_value) / span)) for sample in series]
        for a, b in zip(points, points[1:]):
            canvas.line(a, b, fill)
        lx = x + 95 * i
        canvas.rect(lx, y + h + 21, 10, 3, fill)
        canvas.label(lx + 13, y + h + 27, name, 10)
    canvas.label(x + w - 45, y + h + 14, f"{max_horizontal:.1f} {horizontal_unit}", 10)


def render_phase(report: Path, data: dict, png_only: bool) -> None:
    summary = data["summary"]
    cycle = summary["phase_cycle"]
    samples = data["series"]
    heights = [sample["phase_level_mm"] for sample in samples]
    height_range = (math.floor((min(heights) - 0.1) * 5) / 5, math.ceil((max(heights) + 0.1) * 5) / 5)
    canvas = Canvas()
    canvas.label(28, 44, "Freeze and thaw", 30)
    canvas.label(29, 72, "Closed tray  /  1000 kg/m³ liquid water  /  917 kg/m³ ice")
    draw_series(canvas, samples, 28, 134, 495, 193, "Phase mass (g)", [
        ("surface_g", "Liquid", (69, 181, 251)), ("ice_g", "Ice", (190, 230, 253)),
        ("water_total_g", "Total", (251, 194, 111)),
    ])
    draw_series(canvas, samples, 555, 134, 493, 193, "Occupied volume (mL)", [
        ("phase_volume_mL", "Volume", (84, 223, 187)),
    ], vertical_range=(11.5, 13.5))
    draw_series(canvas, samples, 28, 435, 495, 168, "Prescribed temperature (K)", [
        ("phase_temperature_K", "Temperature", (255, 153, 112)),
    ], vertical_range=(260.0, 300.0))
    draw_series(canvas, samples, 555, 435, 493, 168, "Surface above tray bottom (mm)", [
        ("phase_level_mm", "Surface", (133, 188, 255)),
    ], vertical_range=height_range)
    canvas.label(29, 669,
                 f"{cycle['initial_volume_mL']:.2f} → {cycle['peak_volume_mL']:.2f} → "
                 f"{cycle['final_volume_mL']:.2f} mL   /   Mass residual "
                 f"{summary['mass_ledger_error_kg'] * 1e6:.3g} mg", 14)
    canvas.save(report, png_only)


def render_layers(report: Path, data: dict, png_only: bool) -> None:
    geometry = data["geometry"]
    canvas = Canvas(height=HEIGHT)
    canvas.label(28, 35, data["scene"], 28)
    canvas.label(29, 58, "Layer thickness and cumulative outlet mass", 14)
    fields = [("height_mm", "Surface elevation (mm)"), ("depth_mm", "Water depth (mm)"),
              ("lower_layer_mm", "Ice or dry soil (mm)")]
    if "middle_layer_mm" in geometry:
        fields.append(("middle_layer_mm", "Mud (mm)"))
    for index, (key, label) in enumerate(fields):
        draw_field(canvas, geometry[key], geometry["columns"], geometry["rows"],
                   28 + index * 264, 100, 228, 213, label, key != "height_mm")

    samples = []
    for entry in data["series"]:
        a, b = entry["outlets_water_kg"]
        sample = {"seconds": entry["seconds"], "outlet_a": a * 1000, "outlet_b": b * 1000}
        sample.update({f"layer_{i}": mass * 1000 for i, mass in enumerate(entry["layer_kg"])})
        samples.append(sample)
    draw_series(canvas, samples, 28, 394, 495, 195, "Water through outlets (g)", [
        ("outlet_a", "Shortcut A", (97, 205, 247)), ("outlet_b", "Bypass B", (255, 211, 107)),
    ])
    names = (("Ice", (185, 213, 255)), ("Water", (97, 205, 247)))
    if "middle_layer_mm" in geometry:
        names = (("Dry soil", (236, 183, 122)), ("Mud", (252, 134, 84)), ("Water", (97, 205, 247)))
    draw_series(canvas, samples, 555, 394, 495, 195, "Mass on the surface (g)", [
        (f"layer_{i}", name, color) for i, (name, color) in enumerate(names)
    ])
    residual = data["summary"]["ledger_error_kg"] * 1e6
    canvas.label(29, 659, f"Maximum mass residual {residual:.4g} mg", 13)
    canvas.save(report, png_only)


def render(report: Path, png_only: bool = False) -> None:
    data = json.loads(report.read_text(encoding="utf-8"))
    if "bed_coupling" in data.get("summary", {}):
        render_layers(report, data, png_only)
        return
    if "phase_cycle" in data.get("summary", {}):
        render_phase(report, data, png_only)
        return
    geometry = data["geometry"]
    cols, rows = geometry["columns"], geometry["rows"]
    series = data.get("series")
    canvas = Canvas(height=HEIGHT if series else 362)
    canvas.label(28, 35, data["scene"], 28)
    canvas.label(29, 58, data.get("purpose", "Current simulation snapshot"), 14)
    fields = (("height_mm", "Bed elevation (mm)", False), ("depth_mm", "Water depth (mm)", True),
              ("absorbed_kg_m2", "Substrate water (kg/m²)", True), ("char_fraction", "Char fraction", True))
    for index, (key, label, zero_based) in enumerate(fields):
        custom_labels = data.get("field_labels")
        draw_field(canvas, geometry[key], cols, rows, 28 + index * 264, 100, 228, 213,
                   custom_labels[index] if custom_labels else label, zero_based and not custom_labels)

    if series:
        draw_series(canvas, series, 28, 394, 490, 195, "Water ledger (g)", [
            ("surface_g", "Surface", (97, 205, 247)), ("ice_g", "Snow", (185, 213, 255)),
            ("absorbed_g", "Absorbed", (92, 232, 189)), ("evaporated_g", "Vapor/drying", (255, 211, 107)),
            ("outlet_g", "Outlet", (219, 136, 255)),
        ])
        draw_series(canvas, series, 563, 394, 267, 195, "Reaction (0–1)", [
            ("char", "Char frac.", (255, 148, 115)), ("flame", "Flame", (255, 214, 105)),

        ])
        draw_series(canvas, series, 866, 394, 190, 195, "Steam (g/s)", [
            ("steam_g_s", "Evaporation", (102, 218, 249)),
        ])
        summary = data["summary"]
        mode = "CPU / GPU" if summary["gpu_checked"] else "CPU"
        canvas.label(29, 659, f"{mode} • max mass residual {summary['mass_ledger_error_kg'] * 1e6:.3g} mg "
                              f"• max reconstructed water residual {summary['reconstructed_mass_error_kg'] * 1e6:.3g} mg",
                     13)
    canvas.save(report, png_only)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("report", type=Path, nargs="+")
    parser.add_argument("--png-only", action="store_true", help="Write PNG without the SVG export.")
    args = parser.parse_args()
    for report in args.report:
        render(report, args.png_only)


if __name__ == "__main__":
    main()
