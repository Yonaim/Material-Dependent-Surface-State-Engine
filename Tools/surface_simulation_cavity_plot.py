"""Compare four Cavity fill & spill runs against the sampled original OBJ."""

import argparse
import json
import math
from pathlib import Path
from statistics import fmean

from surface_simulation_plot import Canvas, GRID, PANEL, draw_series

SOURCES = (
    ("mesh", "Original OBJ", (231, 240, 255)),
    ("normal", "Normal map", (255, 181, 101)),
    ("height", "Height map", (81, 226, 180)),
    ("displacement", "Displacement", (134, 163, 255)),
)


def compare(directory: Path) -> None:
    reports = {key: json.loads((directory / f"cavity-{key}.json").read_text(encoding="utf-8"))
               for key, _, _ in SOURCES}
    baseline = reports["mesh"]
    grid = baseline["geometry"]
    cols, rows = grid["columns"], grid["rows"]
    reference = baseline["summary"]["cavity"]
    reference_bed = grid["height_mm"]
    series = [{"seconds": row["seconds"]} for row in baseline["series"]]
    results = {}
    beds = {}
    for key, _, _ in SOURCES:
        report = reports[key]
        geometry = report["geometry"]
        if ((geometry["columns"], geometry["rows"]) != (cols, rows)
                or report["simulation_grid"] != baseline["simulation_grid"]
                or report["summary"]["source_mass_kg"] != baseline["summary"]["source_mass_kg"]):
            raise ValueError(f"Cavity settings differ for {key}.")
        raw_bed = geometry["height_mm"]
        offset = fmean(value - original for value, original in zip(raw_bed, reference_bed, strict=True))
        beds[key] = [value - offset for value in raw_bed]
        errors = [abs(value - original) for value, original in zip(beds[key], reference_bed, strict=True)]
        data = report["summary"]["cavity"]
        results[key] = {
            "bed_rms_mm": math.sqrt(fmean(error ** 2 for error in errors)),
            "bed_peak_error_mm": max(errors),
            "sill_depth_mm": data["sill_depth_mm"],
            "capacity_at_sill_mL": data["capacity_at_sill_mL"],
            "capacity_delta_percent": (data["capacity_at_sill_mL"] / reference["capacity_at_sill_mL"] - 1) * 100,
            "spill_time_s": data["spill_time_s"],
            "spill_delta_ms": (data["spill_time_s"] - reference["spill_time_s"]) * 1000,
        }
        for row, value in zip(series, report["series"], strict=True):
            if row["seconds"] != value["seconds"]:
                raise ValueError(f"Cavity sampling times differ for {key}.")
            row[key] = max(0.0, value["received_mL"])

    draw_comparison(directory, cols, rows, beds, series, results)
    (directory / "Cavity_Accuracy.json").write_text(
        json.dumps({"baseline": "mesh", "shape_grid": [cols, rows], "solver_grid": baseline["simulation_grid"],
                    "alignment": "mean height offset", "sources": results}, indent=2) + "\n", encoding="utf-8")


def draw_comparison(directory: Path, cols: int, rows: int, beds: dict, series: list[dict], results: dict) -> None:
    canvas = Canvas()
    canvas.label(28, 36, "Cavity reconstruction and overflow", 28)
    canvas.label(29, 64, f"Same {cols} x {rows} geometry samples and 2 x 1 solver. Original OBJ is the reference.")
    canvas.rect(16, 86, 1048, 166, PANEL)
    column_x = (29, 252, 424, 572, 721, 892)
    for x, title in zip(column_x, ("Geometry input", "Bed RMS (mm)", "Sill depth (mm)", "Storage (mL)",
                                     "Capacity delta (%)", "Spill at 0.01 mL (s)"), strict=True):
        canvas.label(x, 113, title)
    canvas.rect(28, 123, 1024, 1, GRID)
    for row, (key, title, fill) in enumerate(SOURCES):
        item = results[key]
        values = (title, f"{item['bed_rms_mm']:.4f}", f"{item['sill_depth_mm']:.3f}",
                  f"{item['capacity_at_sill_mL']:.3f}", f"{item['capacity_delta_percent']:+.2f}",
                  f"{item['spill_time_s']:.3f}")
        for x, value in zip(column_x, values, strict=True):
            canvas.label(x, 149 + 27 * row, value, fill=fill)

    definitions = [(key, title, fill) for key, title, fill in SOURCES]
    notch_row = min(range(rows), key=lambda row: beds["mesh"][row * cols + cols // 2])
    slice_at = notch_row * cols
    profile = [{"distance_mm": (col + 0.5) * 100.0 / cols,
                **{key: beds[key][slice_at + col] for key in beds}} for col in range(cols)]
    draw_series(canvas, profile, 28, 305, 490, 255, "Bed section through the notch (height in mm)",
                definitions, horizontal_unit="mm", horizontal_key="distance_mm")
    draw_series(canvas, series, 561, 305, 490, 255, "Water reaching the receiver or outlet (mL)", definitions)
    canvas.label(29, 654, "Height comparison removes one constant offset per source. "
                          "Water input is 9.00 mL over 4 s.", 13)
    canvas.save(directory / "Cavity_Accuracy")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("report_directory", type=Path)
    compare(parser.parse_args().report_directory)


if __name__ == "__main__":
    main()
