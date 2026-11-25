"""Run matched Surface Simulation validation, captures, and transport timing."""

import argparse
import json
import os
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCENES = ROOT / "Assets" / "Scenes"
DEMO_NAMES = ("WettedContact", "RewettingDelta", "ThinVsThickSteel")


def run(exe: Path, output: Path, name: str, *args: str) -> str:
    """Preserve Vulkan and solver output for each invocation."""
    process = subprocess.run([str(exe), *args], cwd=ROOT, capture_output=True, check=False)
    transcript = (process.stdout + process.stderr).decode("utf-8", errors="replace")
    (output / f"{name}.log").write_text(transcript, encoding="utf-8")
    if process.returncode:
        raise RuntimeError(f"{name} failed. See {output / (name + '.log')}")
    print(name, flush=True)
    return transcript


def capture(exe: Path, output: Path, name: str, scene: Path, frame: int, views: str) -> str:
    return run(exe, output, name, "--capture-surface", "--scene", str(scene),
               "--frames", str(frame), "--surface-view-modes", views,
               "--surface-capture", str(output / f"{name}.png"))


def check(exe: Path, output: Path, name: str, scene: Path, *, gpu: bool) -> str:
    mode = "--check-surface" if gpu else "--reference-surface"
    return run(exe, output, name, mode, "--scene", str(scene),
               "--surface-output", str(output / name))


def compare_demos(exe: Path, output: Path, before: Path | None) -> None:
    for name in DEMO_NAMES:
        check(exe, output, name, SCENES / "Comparisons" / f"{name}.Scene", gpu=True)

    flat = SCENES / "SnowOnFabric_WettedContact_Flat.Scene"
    ruffled = SCENES / "SnowOnFabric_WettedContact_Ruffled.Scene"
    for label, scene in (("Flat", flat), ("Ruffled", ruffled)):
        capture(exe, output, f"Contact-{label}-Early", scene, 4,
                "pbr,contact-area,solver-depth,geometry-grid")
        capture(exe, output, f"Contact-{label}-Late", scene, 180,
                "pbr,absorption-fill,layer-thickness:SurfaceWater,areal-mass:AbsorbedWater")

    delta = SCENES / "SoilCoveredStonePath_RewettingDelta.Scene"
    no_rewet = SCENES / "SoilCoveredStonePath_RewettingDelta_Disabled.Scene"
    modes = "pbr,solver-depth,effective-bed,overflow-transfers"
    capture(exe, output, "Delta-FirstPour", delta, 115, modes)
    capture(exe, output, "Delta-BeforeSecondPour", delta, 305,
            "pbr,state:mud.moisture,layer-thickness:mud,effective-bed")
    for label, scene in (("Enabled", delta), ("Disabled", no_rewet)):
        capture(exe, output, f"Delta-{label}-SecondPour", scene, 373,
                "pbr,state:mud.moisture,areal-mass:mud.solids,overflow-transfers")
        capture(exe, output, f"Delta-{label}-After", scene, 465,
                "pbr,solver-depth,layer-thickness:mud,effective-bed")

    for label in ("Thin", "Thick"):
        scene = SCENES / f"HeatedWaterChannel_ThermalSteel{label}.Scene"
        capture(exe, output, f"Steel-{label}", scene, 156, "pbr,temperature,solver-depth,activity")

    pocket = SCENES / "CavityFillAndSpill_TwinPockets.Scene"
    check(exe, output, "TwinPockets", pocket, gpu=True)
    for frame in (25, 47, 70, 120):
        capture(exe, output, f"TwinPockets-{frame}", pocket, frame, modes)

    fabric = SCENES / "FabricVsSponge.Scene"
    check(exe, output, "FabricVsSponge", fabric, gpu=True)
    capture(exe, output, "FabricVsSponge", fabric, 120,
            "pbr,absorption-fill,areal-mass:AbsorbedWater,simulation-grid")

    like_for_like = SCENES / "SoilCoveredStonePath.Scene"
    if before:
        check(before, output, "Baseline-Soil", like_for_like, gpu=True)
        capture(before, output, "Baseline-Soil", like_for_like, 120,
                "pbr,layer-thickness,simulation-grid,geometry-grid")
    check(exe, output, "Updated-Soil", like_for_like, gpu=True)
    capture(exe, output, "Updated-Soil", like_for_like, 120,
            "pbr,layer-thickness,simulation-grid,geometry-grid")


def write_runoff_fixture(output: Path, count: int, substeps: int) -> Path:
    reference = SCENES / "BrickRunoff.Scene"
    scene = json.loads(reference.read_text(encoding="utf-8"))
    for key in ("surfaceTexture", "surfaceNormal", "chartAlbedo", "chartNormal"):
        if key in scene:
            scene[key] = os.path.relpath(reference.parent / scene[key], output).replace("\\", "/")
    if "environment" in scene:
        environment = scene["environment"]
        environment["texture"] = os.path.relpath(reference.parent / environment["texture"], output).replace("\\", "/")
    scene.pop("geometrySamplesPerCell", None)
    scene.update(id=f"BrickRunoff-{count}-{substeps}", simulationCells=[count, count],
                 geometrySamples=[64, 64], transportSubsteps=substeps)
    path = output / f"{scene['id']}.Scene"
    path.write_text(json.dumps(scene, indent=2) + "\n", encoding="utf-8")
    return path


def find_report(directory: Path) -> dict:
    for path in directory.glob("*.json"):
        data = json.loads(path.read_text(encoding="utf-8"))
        if "summary" in data:
            return data["summary"]
    raise RuntimeError(f"No scenario summary in {directory}")


def compare_transport(exe: Path, output: Path, grids: list[int], repetitions: int, gpu_check: bool) -> None:
    results = []
    for count in grids:
        for substeps in (1, 2, 4, 8):
            scene = write_runoff_fixture(output, count, substeps)
            check(exe, output, scene.stem, scene, gpu=gpu_check)
            timing = []
            for trial in range(repetitions):
                log = run(exe, output, f"{scene.stem}-Timing-{trial+1}",
                          "--capture-surface", "--scene", str(scene), "--frames", "150")
                match = re.search(r"GPU solver timing: mean ([.0-9]+) ms, p90 ([.0-9]+) ms, samples (\d+)", log)
                if not match:
                    raise RuntimeError(f"No GPU timings for {scene.stem}")
                timing.append(dict(mean_ms_per_frame=float(match[1]), p90_ms_per_frame=float(match[2]),
                                   frames_measured=int(match[3])))
            results.append(dict(cells=count, substeps=substeps, timing_runs=timing,
                                **{key: value for key, value in find_report(output / scene.stem).items()
                                   if key != "transport_substeps"}))
            (output / "TransportMatrix.json").write_text(json.dumps(results, indent=2) + "\n", encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("exe", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--before", type=Path)
    parser.add_argument("--mode", choices=("demos", "transport", "all"), default="all")
    parser.add_argument("--grids", type=int, nargs="+", default=[16, 32, 64])
    parser.add_argument("--repetitions", type=int, default=1)
    parser.add_argument("--gpu-check-matrix", action="store_true")
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    exe = args.exe.resolve()
    if args.mode in ("demos", "all"):
        compare_demos(exe, output, args.before.resolve() if args.before else None)
    if args.mode in ("transport", "all"):
        compare_transport(exe, output, args.grids, args.repetitions, args.gpu_check_matrix)


if __name__ == "__main__":
    main()
