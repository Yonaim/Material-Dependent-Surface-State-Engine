"""Audit completed trajectories and preserve failure/incomplete statuses explicitly."""

import collections
import json
import sys

sys.path.insert(
    0, str(__import__('pathlib').Path(__file__).resolve().parents[2] / "Build/ResearchPython")
)
import numpy as np
from run_coupling_study import ROOT, EXPERIMENT, digest, write, stamp

manifest = json.loads((EXPERIMENT / "manifest.json").read_text(encoding="utf-8"))
selected = json.loads((EXPERIMENT / "execution-amendment.json").read_text())["selected"]
records = []
mapping = {}
for run in manifest["runs"]:
    if run["status"] != "completed":
        continue
    base = ROOT / run["output"]
    s = json.loads((base / "summary.json").read_text())
    config = json.loads((base / "request.json").read_text())
    mapping_key = f"{config['case']}-{config['resolution']}"
    if mapping_key not in mapping:
        warnings = s.get("mapping_warnings", [])
        mapping[mapping_key] = dict(
            valid_texels=s["valid_texels"],
            allocated_texels=s["texels"],
            warning_count=len(warnings),
            no_texel_centers=sum("covers no texel centers" in w for w in warnings),
            seam_no_candidates=sum("has no texel candidates" in w for w in warnings),
            dropped_link_messages=[w for w in warnings if "links were dropped" in w],
            note="Mapping warnings are scientific limitations, distinct from runtime/budget failures",
        )
    rows = [json.loads(line) for line in (base / "series.jsonl").read_text().splitlines()]
    failures = []
    if len(rows) != s["steps"]:
        failures.append("row_count")
    if [r["step"] for r in rows] != list(range(1, s["steps"] + 1)):
        failures.append("step_sequence")
    if abs(rows[-1]["time"] - config["seconds"]) > 1e-9:
        failures.append("end_time")
    final = np.fromfile(base / "final.f32", dtype="<f4")
    if final.size != s["texels"] * len(s["channels"]):
        failures.append("state_size")
    if not np.isfinite(final).all() or np.any(final < 0):
        failures.append("state_validity")
    if s["variant"] == "E":
        g0 = (base / "geometry-0.f32").read_bytes()
        if any(f.read_bytes() != g0 for f in base.glob("geometry-*.f32")):
            failures.append("frozen_geometry")
    if any(
        s[key]
        for key in [
            "nonfinite",
            "negative",
            "reaction_budget",
            "reaction_arithmetic",
            "frozen_failures",
        ]
    ):
        failures.append("gpu_diagnostics")
    if run["relative_mass_residual"] > 1e-5:
        failures.append("mass_gate")
    asset_current = {p: digest(ROOT / p) == sha for p, sha in run["assets"].items()}
    if not all(asset_current.values()):
        failures.append("asset_changed_since_run")
    records.append(
        dict(
            id=run["id"],
            attempt=run["attempt"],
            steps=len(rows),
            failures=failures,
            relative_mass_residual=run["relative_mass_residual"],
            assets_unchanged=asset_current,
            final_sha256=digest(base / "final.f32"),
            series_sha256=digest(base / "series.jsonl"),
        )
    )
latest = {r["id"]: r for r in manifest["runs"]}
remaining = [key for key in selected if key not in latest or latest[key]["status"] != "completed"]
write(
    EXPERIMENT / "study-audit.json",
    dict(
        created_at=stamp(),
        selected_conditions=len(selected),
        remaining_selected=remaining,
        attempt_status_counts=dict(collections.Counter(r["status"] for r in manifest["runs"])),
        completed_attempts=len(records),
        audited_steps=sum(r["steps"] for r in records),
        audit_failures=sum(len(r["failures"]) for r in records),
        max_relative_mass_residual=max(r["relative_mass_residual"] for r in records),
        records=records,
        mapping=mapping,
        limitations=[
            "No hardware-independent ground truth; no pixel/appearance assessment",
            "Ledger-disabled control excludes per-rule extent validation",
            "Original harness executables before the archive were hashed but not preserved",
            "Raw captures are local git-ignored artifacts",
        ],
    ),
)
print(
    json.dumps(
        dict(
            audited=len(records),
            remaining=remaining,
            failures=sum(len(r["failures"]) for r in records),
        ),
        indent=2,
    )
)
