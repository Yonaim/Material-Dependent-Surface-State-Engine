"""Prepare immutable conditions, invoke the production GPU harness, and retain every attempt."""
# ============================================================
# 00. Imports and Module Configuration
# ============================================================
#region 00_Imports_and_Module_Configuration

import argparse
import collections
import datetime as dt
import hashlib
import json
import pathlib
import subprocess
import sys
import time

ROOT = pathlib.Path(__file__).resolve().parents[2]
STUDY = ROOT / "Performance-Data/20261026-coupling-study"
EXPERIMENT = STUDY / "PERF-041-coupling"
RAW = EXPERIMENT / "raw"
EXE = ROOT / "Build/Release/MDSS_CouplingStudy.exe"


#endregion

# ============================================================
# 01. Processing Helpers
# ============================================================
#region 01_Processing_Helpers

def stamp():
    return dt.datetime.now(dt.timezone(dt.timedelta(hours=9))).isoformat()


def write(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2, ensure_ascii=False), encoding="utf-8")


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def compact_mapping_warnings(summary):
    """Keep verbose mesh mapping diagnostics in raw summary, not repeated in the index."""
    result=dict(summary)
    warnings=result.pop("mapping_warnings",[])
    def category(message):
        if "covers no texel centers" in message:return "no_texel_centers"
        if "has no texel candidates" in message:return "seam_no_candidates"
        if "links were dropped" in message:return "dropped_links"
        return "other"
    result["mapping_warning_count"]=len(warnings)
    result["mapping_warning_categories"]=dict(collections.Counter(category(w) for w in warnings))
    result["mapping_warning_details"]="See this run's raw summary.json for every original warning"
    result["mapping_warning_examples"]=list(dict.fromkeys(next(w for w in warnings if category(w)==c) for c in result["mapping_warning_categories"]))
    return result


def prepare():
    plan_path = EXPERIMENT / "plan.json"
    if plan_path.exists():
        return json.loads(plan_path.read_text(encoding="utf-8"))
    conditions = []
    def add(name, case, variant, res=128, hz=60, seconds=14, **extra):
        bunny = case == "bunny"
        conditions.append(dict(id=name, case=case, variant=variant, resolution=res, hz=hz, seconds=seconds,
            profile="Assets/SurfaceProfiles/IceReactionLab.SRProfile",
            mesh="Assets/Meshes/StanfordBunny/StanfordBunny.obj" if bunny else "Assets/Meshes/MarbleCube/MarbleCube.obj",
            scale=[6.42264883]*3 if bunny else [1.65, 2.2, .22], rotation_x_degrees=90 if bunny else 8,
            ledger=True, full_geometry=True, **extra))
    for v in "ABCDEF":
        add(f"smoke-{v}", "plate", v, seconds=1)
    add("smoke-D-ledger-off", "plate", "D", seconds=1)
    conditions[-1]["ledger"] = False
    for hz in [60, 120, 240]:
        for v in "ABCDEF":
            add(f"plate-128-{hz}-{v}", "plate", v, hz=hz)
    for v in "ABCDEF":
        add(f"plate-256-60-{v}", "plate", v, res=256)
    add("plate-128-60-D-repeat", "plate", "D")
    add("plate-128-60-D-dirty", "plate", "D")
    conditions[-1]["full_geometry"] = False
    for case in ["bunny", "mud"]:
        for v in "ABCDEF":
            add(f"{case}-128-60-{v}", case, v)
    plan = dict(schema_version=1, created_at=stamp(), status="frozen_before_first_run", conditions=conditions,
        source_commit=subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
        development_budget="one fixed field design; no coefficient search; failed cases retained",
        scope="H1 mechanism and exploratory H2 transfer; no modular-baseline superiority claim",
        inputs="ice density 3.5 exp(-(u/.30)^4-(v/.10)^4); heat .2 gaussian(-.08,.02,.16,.14), water .15 gaussian(0,.30,.18,.08) at t=0,.25,...,11.75",
        downstream="v < -0.1 over whole mesh; deposition patch top-facing normals; normalized coordinates for bunny",
        accuracy_gate=dict(relative_ice_water_budget=1e-5, invalid_counts=0, frozen_changes=0),
        effect_rule="Report effect alongside dt and resolution sensitivity; no physical truth or statistical significance claim",
        timing="one diagnostic trajectory per condition; timestamps descriptive, not repeated application performance",
        deviations=["Headless production solver bypasses GUI benchmark and its forced coupling/drain behavior",
                    "Direct area-scaled analytic fields replace contact rasterization to define S0 and identical timed inputs",
                    "No normal-map meso geometry in this study", "No rendered image quality or author productivity measurement"])
    write(plan_path, plan)
    write(STUDY / "study.json", dict(created_at=stamp(), question="State-driven geometry causal ablation", status="planned",
        experiments=["PERF-041-coupling"], report="Docs/MDSSP-Agent-Vault/04_Experiments/02_Performance-Measurements.md"))
    return plan


def run(stage, selected, sparse=False, repeat=False):
    plan = prepare()
    manifest_path = EXPERIMENT / "manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8")) if manifest_path.exists() else dict(runs=[])
    chosen = [c for c in plan["conditions"] if (c["id"] in selected if selected else
        c["id"].startswith("smoke-") if stage == "smoke" else
        c["id"].startswith("plate-") if stage == "development" else c["case"] in ["bunny", "mud"])]
    if not EXE.exists():
        raise RuntimeError(f"Missing runner: {EXE}")
    shader_dir = ROOT / "Build/Shaders/Release"
    # Actual shader path is collected from the generated build definitions below, not assumed for attribution.
    shader_files = sorted((ROOT / "Build").glob("**/*SurfaceReaction*.spv"))
    sources = [ROOT/"Scripts/Research/CouplingStudy.cpp", ROOT/"Source/SurfaceState/Solver/SurfaceStateSolver.cpp",
               ROOT/"Shaders/Simulation/Solver/SurfaceReaction.comp", ROOT/"Shaders/Simulation/Solver/StateMath.glsl",
               ROOT/"Shaders/Simulation/Geometry/SurfaceAccumulationHeight.comp"]
    for condition in chosen:
        previous = [r for r in manifest["runs"] if r["id"] == condition["id"]]
        if previous and previous[-1]["status"] == "completed" and not repeat:
            print("SKIP completed", condition["id"], flush=True)
            continue
        attempt = len(previous) + 1
        output = RAW / f"{condition['id']}-attempt{attempt}"
        output.mkdir(parents=True, exist_ok=False)
        config = dict(condition, output=str(output), sparse_solver=sparse)
        config_path = output / "request.json"
        write(config_path, config)
        run_record = dict(id=condition["id"], attempt=attempt, started_at=stamp(), status="running",
            output=str(output.relative_to(ROOT)), executable_sha256=digest(EXE), plan_sha256=digest(EXPERIMENT/"plan.json"),
            assets={p: digest(ROOT/p) for p in [condition["mesh"], condition["profile"], "Assets/SurfaceProfiles/DemoMud.SRProfile"]},
            sources={str(p.relative_to(ROOT)): digest(p) for p in sources},
            reaction_shaders={str(p.relative_to(ROOT)): digest(p) for p in shader_files})
        manifest["runs"].append(run_record)
        write(manifest_path, manifest)
        print("RUN", condition["id"], stamp(), flush=True)
        start=time.monotonic()
        try:
            with (output/"run.log").open("w", encoding="utf-8") as log:
                result=subprocess.run([str(EXE), str(config_path)], cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, timeout=1800)
            run_record.update(exit_code=result.returncode, status="completed" if result.returncode==0 else "failed")
        except (OSError, subprocess.TimeoutExpired) as exc:
            run_record.update(status="execution_failed", error=str(exc))
        run_record.update(finished_at=stamp(), elapsed_seconds=time.monotonic()-start)
        if (output/"summary.json").exists():
            summary=json.loads((output/"summary.json").read_text(encoding="utf-8"))
            run_record["summary"] = compact_mapping_warnings(summary)
            names=summary["channels"]
            budget=summary["initial_ice_water"]+sum(summary["cumulative_input"][names.index(s)] for s in ["ice","waterfilm"])
            run_record["relative_mass_residual"] = summary["max_mass_residual"]/max(budget,1e-20)
            if run_record["relative_mass_residual"] > plan["accuracy_gate"]["relative_ice_water_budget"]:
                run_record["status"]="accuracy_failed"
        write(manifest_path, manifest)
        print(run_record["status"], condition["id"], round(run_record["elapsed_seconds"],2), flush=True)
        if run_record["status"] != "completed":
            print((output/"run.log").read_text(encoding="utf-8", errors="replace")[-2500:] if (output/"run.log").exists() else run_record.get("error"), flush=True)
            return 1
    return 0


#endregion

# ============================================================
# 02. Script Execution
# ============================================================
#region 02_Script_Execution

if __name__ == "__main__":
    ap=argparse.ArgumentParser()
    ap.add_argument("stage",choices=["prepare","smoke","development","heldout"])
    ap.add_argument("--ids",nargs="*",default=[])
    ap.add_argument("--sparse",action="store_true")
    ap.add_argument("--repeat",action="store_true")
    args=ap.parse_args()
    if args.stage=="prepare":
        prepare()
        print(EXPERIMENT/"plan.json")
    else:
        sys.exit(run(args.stage,args.ids,args.sparse,args.repeat))

#endregion
