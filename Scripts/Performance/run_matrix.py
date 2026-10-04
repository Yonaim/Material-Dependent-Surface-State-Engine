#!/usr/bin/env python3
"""Build MDSS, run the configured scene/resolution matrix, and preserve raw captures."""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import platform
import random
import re
import subprocess
import sys
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_MATRIX = ROOT / "Config/Performance/solver-resolution-matrix.json"
DEFAULT_RESULTS = ROOT / "Docs/MDSSP-Vault/06_Development/Performance/Results"


def run_text(command: list[str], *, cwd: Path, timeout: int = 30) -> str:
    try:
        result = subprocess.run(command, cwd=cwd, text=True, capture_output=True, timeout=timeout, check=False)
    except (OSError, subprocess.TimeoutExpired):
        return ""
    return result.stdout if result.returncode == 0 else ""


def git_snapshot() -> dict[str, Any]:
    commit = run_text(["git", "rev-parse", "HEAD"], cwd=ROOT).strip() or None
    status = run_text(["git", "status", "--porcelain"], cwd=ROOT)
    tracked_diff = run_text(["git", "diff", "--binary", "HEAD"], cwd=ROOT, timeout=60)
    digest = hashlib.sha256(tracked_diff.encode("utf-8", errors="replace"))
    untracked = run_text(["git", "ls-files", "--others", "--exclude-standard"], cwd=ROOT).splitlines()
    for relative in sorted(untracked):
        path = ROOT / relative
        if path.is_file():
            digest.update(relative.encode())
            digest.update(path.read_bytes())
    return {
        "commit": commit,
        "working_tree_dirty": bool(status.strip()),
        "dirty_state_sha256": digest.hexdigest() if status.strip() else None,
    }


def gpu_summary() -> str | None:
    output = run_text(["vulkaninfo", "--summary"], cwd=ROOT, timeout=15)
    candidates = [line.strip() for line in output.splitlines() if re.search(r"deviceName|GPU[0-9]+", line, re.I)]
    return "; ".join(candidates[:8]) if candidates else None


def load_config(matrix_path: Path) -> tuple[dict[str, Any], dict[str, Any]]:
    matrix = json.loads(matrix_path.read_text(encoding="utf-8"))
    if matrix.get("schema_version") != 1:
        raise ValueError("Unsupported matrix schema_version; expected 1.")
    scene_set_path = (matrix_path.parent / matrix["scene_set"]).resolve()
    scene_set = json.loads(scene_set_path.read_text(encoding="utf-8"))
    if scene_set.get("schema_version") != 1:
        raise ValueError("Unsupported scene-set schema_version; expected 1.")
    known = {scene["id"]: scene for scene in scene_set.get("scenes", [])}
    selected = matrix.get("scenes", [])
    if not selected or any(scene_id not in known for scene_id in selected):
        raise ValueError("Matrix scenes must reference scene IDs in its scene_set.")
    resolutions = matrix.get("resolutions", [])
    if not resolutions or any(value not in (128, 256, 512) for value in resolutions):
        raise ValueError("resolutions must contain one or more of 128, 256, 512.")
    for field in ("measurement_frames", "repeats"):
        if not isinstance(matrix.get(field), int) or matrix[field] <= 0:
            raise ValueError(f"{field} must be a positive integer.")
    if not isinstance(matrix.get("warmup_frames", 0), int) or matrix.get("warmup_frames", 0) < 0:
        raise ValueError("warmup_frames must be a nonnegative integer.")
    matrix["_matrix_path"] = str(matrix_path.resolve())
    matrix["_scene_set_path"] = str(scene_set_path)
    matrix["_selected_scenes"] = {
        scene_id: {
            **known[scene_id],
            "resolved_path": str((scene_set_path.parent / known[scene_id]["path"]).resolve()),
        }
        for scene_id in selected
    }
    return matrix, scene_set


def write_json(path: Path, payload: Any) -> None:
    path.write_text(json.dumps(payload, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--matrix", type=Path, default=DEFAULT_MATRIX, help="Matrix JSON path.")
    parser.add_argument("--binary", type=Path, default=ROOT / "Build/bin/MDSS", help="MDSS executable.")
    parser.add_argument("--results-dir", type=Path, default=DEFAULT_RESULTS, help="Result-set parent directory.")
    parser.add_argument("--skip-build", action="store_true", help="Use an already built executable.")
    parser.add_argument("--dry-run", action="store_true", help="Print planned commands without building or launching.")
    parser.add_argument("--timeout", type=int, default=600, help="Per-run timeout in seconds.")
    args = parser.parse_args()

    matrix_path = args.matrix.resolve()
    matrix, _ = load_config(matrix_path)
    binary = args.binary.resolve()
    warmup = matrix.get("warmup_frames", 0)
    measure = matrix["measurement_frames"]
    runs: list[dict[str, Any]] = []
    for scene_id, scene in matrix["_selected_scenes"].items():
        for resolution in matrix["resolutions"]:
            for repeat in range(1, matrix["repeats"] + 1):
                name = f"{scene_id}-r{resolution}-repeat-{repeat}"
                runs.append({"name": name, "scene_id": scene_id, "scene": scene, "resolution": resolution,
                             "repeat": repeat})
    random.Random(matrix.get("order_seed", 0)).shuffle(runs)

    if args.dry_run:
        for run in runs:
            output = Path("<result-set>") / "raw" / f"{run['name']}.jsonl"
            command = [str(binary), "--benchmark-scene", run["scene"]["resolved_path"],
                       "--benchmark-resolution", str(run["resolution"]), "--benchmark-warmup-frames", str(warmup),
                       "--benchmark-measure-frames", str(measure), "--benchmark-output", str(output)]
            print(" ".join(command))
        return 0

    if matrix.get("build_before_run", True) and not args.skip_build:
        build_type = matrix.get("build_type", "Release")
        validation = "ON" if matrix.get("validation_layers", False) else "OFF"
        build = subprocess.run([str(ROOT / "Scripts/build.sh"), f"-DCMAKE_BUILD_TYPE={build_type}",
                                f"-DMDSS_ENABLE_VALIDATION={validation}"], cwd=ROOT, check=False)
        if build.returncode != 0:
            return build.returncode
    if not binary.is_file():
        parser.error(f"MDSS binary not found: {binary}; build it or pass --skip-build with --binary.")

    timestamp = dt.datetime.now().astimezone().strftime("%Y%m%d-%H%M%S")
    result_root = args.results_dir.resolve()
    result_dir = result_root / f"{timestamp}-solver-resolution"
    suffix = 1
    while result_dir.exists():
        result_dir = result_root / f"{timestamp}-solver-resolution-{suffix}"
        suffix += 1
    raw_dir = result_dir / "raw"
    raw_dir.mkdir(parents=True)

    manifest: dict[str, Any] = {
        "schema_version": 1,
        "run_set_id": result_dir.name,
        "created_at": dt.datetime.now().astimezone().isoformat(timespec="seconds"),
        "status": "running",
        "matrix_path": str(matrix_path),
        "matrix": {key: value for key, value in matrix.items() if not key.startswith("_")},
        "revision": git_snapshot(),
        "environment": {
            "platform": platform.platform(),
            "python": platform.python_version(),
            "machine": platform.machine(),
            "gpu_vulkan_summary": gpu_summary(),
        },
        "binary": str(binary),
        "runs": [],
    }
    manifest_path = result_dir / "manifest.json"
    write_json(manifest_path, manifest)
    try:
        for run in runs:
            raw_path = raw_dir / f"{run['name']}.jsonl"
            log_path = raw_dir / f"{run['name']}.log"
            command = [str(binary), "--benchmark-scene", run["scene"]["resolved_path"],
                       "--benchmark-resolution", str(run["resolution"]), "--benchmark-warmup-frames", str(warmup),
                       "--benchmark-measure-frames", str(measure), "--benchmark-output", str(raw_path)]
            with log_path.open("w", encoding="utf-8") as log:
                completed = subprocess.run(command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT,
                                           timeout=args.timeout, check=False)
            sample_count = sum(1 for line in raw_path.read_text(encoding="utf-8").splitlines() if line.strip()) \
                if raw_path.is_file() else 0
            record = {
                "name": run["name"], "scene_id": run["scene_id"], "scene_path": run["scene"]["resolved_path"],
                "role": run["scene"].get("role"), "resolution": run["resolution"], "repeat": run["repeat"],
                "warmup_frames": warmup, "measurement_frames": measure, "exit_code": completed.returncode,
                "sample_count": sample_count, "raw_jsonl": str(raw_path.relative_to(result_dir)),
                "log": str(log_path.relative_to(result_dir)), "command": command,
            }
            manifest["runs"].append(record)
            write_json(manifest_path, manifest)
            if completed.returncode != 0 or sample_count != measure:
                raise RuntimeError(f"{run['name']} failed: exit={completed.returncode}, samples={sample_count}/{measure}; see {log_path}")
            print(f"Captured {run['name']} ({sample_count} frames)", flush=True)
        manifest["status"] = "complete"
    except Exception as exc:
        manifest["status"] = "failed"
        manifest["error"] = str(exc)
        write_json(manifest_path, manifest)
        print(f"Benchmark matrix stopped: {exc}", file=sys.stderr)
        return 1

    write_json(manifest_path, manifest)
    summary_script = Path(__file__).with_name("summarize.py")
    summary = subprocess.run([sys.executable, str(summary_script), "--manifest", str(manifest_path)], cwd=ROOT,
                             check=False)
    return summary.returncode


if __name__ == "__main__":
    raise SystemExit(main())
