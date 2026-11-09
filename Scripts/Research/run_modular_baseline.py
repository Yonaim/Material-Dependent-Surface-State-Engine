"""Immutable v1 requests, independent Vulkan runs, explicit failures, and reproducibility archives."""

# ============================================================
# 00. Imports and Module Configuration
# ============================================================
# region 00_Imports_and_Module_Configuration

import argparse
import json
import pathlib
import shutil
import subprocess
import time
import sys
import hashlib
from run_coupling_study import ROOT, EXE, digest, stamp, write, compact_mapping_warnings

SPEC_PATH = ROOT / "Scripts/Research/Plans/modular-baseline-v1.json"
EXP = ROOT / "Performance-Data/20261026-coupling-study/baseline"
RAW = EXP / "raw"

# endregion

# ============================================================
# 01. Processing Helpers
# ============================================================
# region 01_Processing_Helpers


def prepare():
    if (EXP / "plan.json").exists():
        return json.loads((EXP / "plan.json").read_text(encoding="utf-8"))
    spec = json.loads(SPEC_PATH.read_text(encoding="utf-8"))
    tasks = {t["id"]: t for t in spec["tasks"]}

    def request(task, variant, implementation, resolution=128, seconds=14, **extra):
        t = tasks[task]
        c = dict(spec["reference_condition"])
        c.update(
            id=f"{task}-{variant}-{implementation}-{resolution}",
            task=task,
            implementation=implementation,
            case=t["input_case"],
            rotation_x_degrees=t["rotation_x_degrees"],
            variant=variant,
            resolution=resolution,
            seconds=seconds,
            sparse_solver=False,
            ledger=True,
            full_geometry=True,
        )
        additions = tasks["T2"]["profile_changes"] if task in ["T2", "T3"] else {}
        c.update(additions)
        c.update(extra)
        return c

    conditions = []

    def add(stage, task, variant, implementation, **extra):
        c = request(task, variant, implementation, **extra)
        c["stage"] = stage
        c["id"] = f"{stage}-" + c["id"]
        conditions.append(c)

    for imp in ["U", "M", "M-separate"]:
        add("smoke", "T0", "D", imp, seconds=1)
    for task, t in tasks.items():
        for v in t["variants"]:
            for imp in ["U", "M"]:
                add("diagnostic", task, v, imp)
    add("separate", "T0", "D", "M-separate")
    for imp in ["U", "M"]:
        add("ledger-off", "T0", "D", imp, seconds=1, ledger=False)
        add("reverse", "T3", "D", imp, seconds=1, reverse_modules=True)
    for task in ["T0", "T3"]:
        for resolution in [128, 256]:
            for pair, order in enumerate(spec["cost_evaluation"]["pair_order"], 1):
                for imp in order:
                    add(
                        "cost",
                        task,
                        "D",
                        imp,
                        resolution=resolution,
                        timing_only=True,
                        ledger=False,
                        pair=pair,
                    )
                    conditions[-1]["id"] += f"-pair{pair}"
    plan = dict(
        spec_id=spec["spec_id"],
        spec_sha256=digest(SPEC_PATH),
        created_at=stamp(),
        status="requests_frozen_before_baseline_execution",
        conditions=conditions,
        shared_code="Transport provider bodies, numeric helpers, reaction/geometry services and resource ABI; independent modular command coordinator",
        scope="H3 on identical discrete model; not prior-framework reproduction or human productivity",
        reporting="Failures retained; no tolerance relaxation; no task-specific solver branches added for T1-T3",
    )
    write(EXP / "plan.json", plan)
    return plan


def archive():
    files = [EXE, SPEC_PATH, ROOT / "CMakeLists.txt"]
    for rel, pattern in [
        ("Scripts/Research", "*.py"),
        ("Scripts/Research", "*.h"),
        ("Scripts/Research", "*.cpp"),
        ("Scripts/Research", "*.cmake"),
        ("Source/SurfaceState", "*.cpp"),
        ("Source/SurfaceState", "*.h"),
        ("Shaders/Simulation", "*.glsl"),
        ("Shaders/Simulation", "*.comp"),
        ("Build/bin/Shaders/Release", "*.spv"),
        ("Build/research/Release", "*.spv"),
        ("Build/research", "*.comp"),
    ]:
        files.extend((ROOT / rel).rglob(pattern))
    for rel in [
        "Assets/SurfaceProfiles/IceReactionLab.SRProfile",
        "Assets/SurfaceProfiles/DemoMud.SRProfile",
        "Assets/Meshes/MarbleCube/MarbleCube.obj",
        "Assets/Meshes/MarbleCube/MarbleCube.mtl",
    ]:
        files.append(ROOT / rel)
    records = {str(f.relative_to(ROOT)): digest(f) for f in set(files)}
    revision = hashlib.sha256(json.dumps(records, sort_keys=True).encode()).hexdigest()[:12]
    destination = RAW / "inputs" / revision
    if (destination / "archive.json").exists():
        return str(destination.relative_to(ROOT))
    destination.mkdir(parents=True, exist_ok=True)
    for f in set(files):
        target = destination / f.relative_to(ROOT)
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2("\\\\?\\" + str(f.resolve()), "\\\\?\\" + str(target.resolve()))
        if digest(target) != records[str(f.relative_to(ROOT))]:
            raise RuntimeError("Archive digest mismatch")
    write(destination / "archive.json", dict(created_at=stamp(), files=records))
    (destination / "working-tree.patch").write_bytes(
        subprocess.check_output(["git", "diff"], cwd=ROOT)
    )
    return str(destination.relative_to(ROOT))


def run(stage, ids):
    plan = prepare()
    path = EXP / "manifest.json"
    for name in ["cost-reference-plan.json", "order-control-plan.json", "timing-control-plan.json"]:
        if (EXP / name).exists():
            plan["conditions"] += json.loads((EXP / name).read_text(encoding="utf-8"))["conditions"]
    manifest = json.loads(path.read_text(encoding="utf-8")) if path.exists() else dict(runs=[])
    inputs = archive()
    for c in plan["conditions"]:
        if c["id"] not in ids if ids else c["stage"] != stage:
            continue
        previous = [r for r in manifest["runs"] if r["id"] == c["id"]]
        if previous and previous[-1]["status"] == "completed":
            continue
        attempt = len(previous) + 1
        out = RAW / f"{c['id']}-attempt{attempt}"
        out.mkdir(parents=True, exist_ok=False)
        write(out / "request.json", dict(c, output=str(out)))
        r = dict(
            id=c["id"],
            attempt=attempt,
            stage=c["stage"],
            started_at=stamp(),
            status="running",
            output=str(out.relative_to(ROOT)),
            request_sha256=digest(out / "request.json"),
            executable_sha256=digest(EXE),
            plan_sha256=digest(EXP / "plan.json"),
            archive=inputs,
        )
        manifest["runs"].append(r)
        write(path, manifest)
        print("RUN", c["id"], flush=True)
        start = time.monotonic()
        try:
            with (out / "run.log").open("w", encoding="utf-8") as log:
                result = subprocess.run(
                    [str(EXE), str(out / "request.json")],
                    cwd=ROOT,
                    stdout=log,
                    stderr=subprocess.STDOUT,
                    timeout=1800,
                )
            r.update(
                status="completed" if result.returncode == 0 else "failed",
                exit_code=result.returncode,
            )
        except (OSError, subprocess.TimeoutExpired) as exc:
            r.update(status="execution_failed", error=str(exc))
        if (out / "summary.json").exists():
            r["summary"] = compact_mapping_warnings(
                json.loads((out / "summary.json").read_text(encoding="utf-8"))
            )
        r.update(finished_at=stamp(), elapsed_seconds=time.monotonic() - start)
        write(path, manifest)
        print(r["status"], c["id"], round(r["elapsed_seconds"], 2), flush=True)
        if r["status"] != "completed":
            print(
                (out / "run.log").read_text(encoding="utf-8", errors="replace")[-2000:], flush=True
            )
            return 1
    return 0


def structural_stop():
    """Operationalize v1 section 8: stop large cost collection after the declared null explanation holds."""
    if not (EXP / "structural-stop-rule.json").exists():
        return False
    subprocess.run(
        [sys.executable, str(ROOT / "Scripts/Research/analyze_modular_baseline.py")],
        cwd=ROOT,
        check=True,
    )
    comparison = json.loads((EXP / "comparisons.json").read_text(encoding="utf-8"))
    manifest = json.loads((EXP / "manifest.json").read_text(encoding="utf-8"))
    latest = {r["id"]: r for r in manifest["runs"]}
    main = [c for c in prepare()["conditions"] if c["stage"] == "diagnostic"]
    if comparison["failed_comparisons"] or any(
        latest.get(c["id"], {}).get("status") != "completed" for c in main
    ):
        return False
    if any(not key.startswith("cost-reference-") for key in comparison["remaining_diagnostics"]):
        return False
    selected = [latest[c["id"]] for c in main]
    hashes = {r["executable_sha256"] for r in selected}
    if len(hashes) != 1:
        return False
    source = ROOT / "Scripts/Research/ModularBaseline.h"
    module_hashes = set()
    for r in selected:
        archive = json.loads((ROOT / r["archive"] / "archive.json").read_text(encoding="utf-8"))
        module_hashes.add(
            archive["files"]["Scripts\\Research\\ModularBaseline.h"]
            if "Scripts\\Research\\ModularBaseline.h" in archive["files"]
            else archive["files"]["Scripts/Research/ModularBaseline.h"]
        )
    if module_hashes != {digest(source)}:
        return False
    # Profiles are the module registration recipe. All non-architectural request fields must match.
    plan = prepare()
    numeric_requests = True
    for c in main:
        if c["implementation"] != "U":
            continue
        b = next(
            x
            for x in main
            if x["task"] == c["task"]
            and x["variant"] == c["variant"]
            and x["implementation"] == "M"
        )
        numeric_requests &= {k: v for k, v in c.items() if k not in ["id", "implementation"]} == {
            k: v for k, v in b.items() if k not in ["id", "implementation"]
        }
    if not numeric_requests:
        return False
    deferred = [c["id"] for c in plan["conditions"] if c["stage"] == "cost"]
    if (EXP / "cost-reference-plan.json").exists():
        deferred += [
            c["id"]
            for c in json.loads((EXP / "cost-reference-plan.json").read_text(encoding="utf-8"))[
                "conditions"
            ]
        ]
    write(
        EXP / "decision.json",
        dict(
            decided_at=stamp(),
            status="no_go_for_H3_in_v1_tasks",
            basis="v1 section 8 structural stopping rule",
            evidence=dict(
                quality_gate_failures=0,
                same_diagnostic_executable=True,
                same_modular_core=True,
                symmetric_numeric_recipes=True,
                extension="T1/T2/T3 execute with profile declarations, generic module factory and unchanged coordinator",
            ),
            rule_sha256=digest(EXP / "structural-stop-rule.json"),
            deferred_run_ids=deferred,
            cost_status="not_run_predefined_structural_stop",
            scope="No demonstrated architectural extension-burden advantage in these tasks; not universal equivalence, independent numerical validation, human-productivity or novelty proof",
        ),
    )
    subprocess.run(
        [sys.executable, str(ROOT / "Scripts/Research/analyze_modular_baseline.py")],
        cwd=ROOT,
        check=True,
    )
    print(
        "STRUCTURAL NO-GO: v1 tasks require no task-specific modular core changes; large cost collection stopped by declared rule",
        flush=True,
    )
    return True


# endregion

# ============================================================
# 02. Script Execution
# ============================================================
# region 02_Script_Execution

if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument(
        "stage",
        choices=[
            "prepare",
            "smoke",
            "diagnostic",
            "separate",
            "ledger-off",
            "reverse",
            "order-control",
            "timing-smoke",
            "cost-reference",
            "cost",
            "finish",
        ],
    )
    ap.add_argument("--ids", nargs="*", default=[])
    a = ap.parse_args()
    if a.stage == "prepare":
        print(EXP / "plan.json")
        prepare()
    elif a.stage == "finish":
        for stage in [
            "diagnostic",
            "separate",
            "ledger-off",
            "reverse",
            "order-control",
            "timing-smoke",
            "cost-reference",
            "cost",
        ]:
            if stage == "cost-reference" and structural_stop():
                raise SystemExit(0)
            if stage == "cost":
                subprocess.run(
                    [sys.executable, str(ROOT / "Scripts/Research/analyze_modular_baseline.py")],
                    cwd=ROOT,
                    check=True,
                )
                comparisons = json.loads((EXP / "comparisons.json").read_text(encoding="utf-8"))
                if comparisons["failed_comparisons"] or comparisons["remaining_diagnostics"]:
                    raise SystemExit("Quality gate not satisfied: refusing cost collection")
            if run(stage, []):
                raise SystemExit(1)
        subprocess.run(
            [sys.executable, str(ROOT / "Scripts/Research/analyze_modular_baseline.py")],
            cwd=ROOT,
            check=True,
        )
    else:
        raise SystemExit(run(a.stage, a.ids))

# endregion
