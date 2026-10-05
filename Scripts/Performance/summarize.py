#!/usr/bin/env python3
"""Regenerate a Markdown performance report from a benchmark manifest and raw JSONL files."""

from __future__ import annotations

import argparse
import json
import math
import statistics
from pathlib import Path
from typing import Any


def percentile(values: list[float], fraction: float) -> float:
    ordered = sorted(values)
    if not ordered:
        raise ValueError("Cannot calculate a percentile without samples.")
    position = (len(ordered) - 1) * fraction
    lower = math.floor(position)
    upper = math.ceil(position)
    if lower == upper:
        return ordered[lower]
    return ordered[lower] * (upper - position) + ordered[upper] * (position - lower)


def read_samples(manifest_path: Path, run: dict[str, Any]) -> list[dict[str, Any]]:
    raw_path = manifest_path.parent / run["raw_jsonl"]
    samples = []
    for line in raw_path.read_text(encoding="utf-8").splitlines():
        if line.strip():
            samples.append(json.loads(line))
    return samples


def fmt(value: float | None) -> str:
    return "—" if value is None else f"{value:.3f}"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, required=True)
    args = parser.parse_args()
    manifest_path = args.manifest.resolve()
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    if manifest.get("status") != "complete":
        parser.error("The manifest must have status=complete.")

    grouped: dict[tuple[str, int], list[dict[str, Any]]] = {}
    for run in manifest["runs"]:
        grouped.setdefault((run["scene_id"], run["resolution"]), []).append(run)

    lines = [
        f"# Performance run — {manifest['run_set_id']}",
        "",
        f"- Status: **{manifest['status']}**",
        f"- Captured: {manifest['created_at']}",
        f"- Revision: `{manifest['revision'].get('commit') or 'unknown'}`",
        f"- Dirty working tree: `{manifest['revision'].get('working_tree_dirty')}`",
        f"- Dirty-state SHA-256: `{manifest['revision'].get('dirty_state_sha256') or 'clean'}`",
        f"- Machine: {manifest['environment'].get('platform')} ({manifest['environment'].get('machine')})",
        f"- GPU/runtime: {manifest['environment'].get('gpu_vulkan_summary') or 'not reported by vulkaninfo'}",
        f"- Matrix: `{manifest['matrix_path']}`",
        "",
        "> GPU timings are per-frame timestamp-query totals. Solver pass times sum the dispatched instance/step ranges for that frame. Compare revisions within the same scene, resolution, and run protocol.",
        "",
        "| Scene | Resolution | Repeats | Solver median ms (run median range) | Solver p95 ms (run p95 range) | Pass 1 median ms | Pass 2 median ms |",
        "|---|---:|---:|---:|---:|---:|---:|",
    ]

    for (scene_id, resolution), runs in sorted(grouped.items()):
        solver_medians: list[float] = []
        solver_p95s: list[float] = []
        pass1_medians: list[float] = []
        pass2_medians: list[float] = []
        run_sample_counts: list[int] = []
        for run in runs:
            samples = read_samples(manifest_path, run)
            run_sample_counts.append(len(samples))
            for field, destination in (("solver_gpu_ms", solver_medians), ("pass1_gpu_ms", pass1_medians),
                                       ("pass2_gpu_ms", pass2_medians)):
                values = [sample[field] for sample in samples if isinstance(sample.get(field), (int, float))]
                if field == "solver_gpu_ms" and values:
                    solver_medians.append(statistics.median(values))
                    solver_p95s.append(percentile([float(value) for value in values], 0.95))
                elif values:
                    destination.append(statistics.median(values))

        median_text = "—" if not solver_medians else f"{statistics.median(solver_medians):.3f} ({min(solver_medians):.3f}–{max(solver_medians):.3f})"
        p95_text = "—" if not solver_p95s else f"{statistics.median(solver_p95s):.3f} ({min(solver_p95s):.3f}–{max(solver_p95s):.3f})"
        lines.append("| {} | {} | {} | {} | {} | {} | {} |".format(
            scene_id, resolution, len(runs), median_text, p95_text,
            fmt(statistics.median(pass1_medians) if pass1_medians else None),
            fmt(statistics.median(pass2_medians) if pass2_medians else None)))
        if any(count != run.get("measurement_frames") for count, run in zip(run_sample_counts, runs)):
            raise ValueError(f"Raw sample count mismatch for {scene_id} at {resolution}.")

    lines.extend(["", "## Run files", "", "| Run | Samples | Raw measurements | App log |", "|---|---:|---|---|"])
    for run in manifest["runs"]:
        lines.append(f"| {run['name']} | {run['sample_count']} | [`{run['raw_jsonl']}`]({run['raw_jsonl']}) | [`{run['log']}`]({run['log']}) |")
    lines.extend([
        "",
        "## Protocol",
        "",
        f"- Warm-up frames: {manifest['matrix'].get('warmup_frames', 0)}",
        f"- Measurement frames per repeat: {manifest['matrix']['measurement_frames']}",
        f"- Fixed simulation step: 1/60 s per rendered frame; fixed timestep ON, auto substepping OFF.",
        "- Each repeat starts a fresh process and reloads the Scene initial contacts.",
        "- This report is generated from the listed JSONL files. Recreate it with `python3 Scripts/Performance/summarize.py --manifest <manifest.json>`. ",
        "",
    ])
    output = manifest_path.parent / "summary.md"
    output.write_text("\n".join(lines), encoding="utf-8")
    print(output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
