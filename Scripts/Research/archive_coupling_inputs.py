"""Preserve the current study implementation and build artifacts without touching past attempts."""

import json
import hashlib
import pathlib
import shutil
import subprocess
from run_coupling_study import ROOT, RAW, EXE, digest, write, stamp

files = [EXE]
files.extend((ROOT / "Build/bin/Shaders/Release").rglob("*.spv"))
files.extend((ROOT / "Scripts/Research").glob("*.cpp"))
files.extend((ROOT / "Scripts/Research").glob("*.h"))
files.extend((ROOT / "Scripts/Research").glob("*.py"))
changed = subprocess.check_output(["git", "diff", "--name-only"], cwd=ROOT, text=True).splitlines()
files.extend(
    ROOT / p for p in changed if p.startswith(("Source/", "Shaders/")) or p == "CMakeLists.txt"
)
files.extend(
    ROOT / p
    for p in [
        "Assets/SurfaceProfiles/IceReactionLab.SRProfile",
        "Assets/SurfaceProfiles/DemoMud.SRProfile",
        "Assets/Meshes/MarbleCube/MarbleCube.obj",
        "Assets/Meshes/MarbleCube/MarbleCube.mtl",
        "Assets/Meshes/StanfordBunny/StanfordBunny.obj",
    ]
)
records = {str(file.relative_to(ROOT)): digest(file) for file in files}
revision = hashlib.sha256(json.dumps(records, sort_keys=True).encode()).hexdigest()[:12]
destination = RAW / "inputs" / revision
destination.mkdir(parents=True, exist_ok=True)
for file in files:
    relative = file.relative_to(ROOT)
    target = destination / relative
    target.parent.mkdir(parents=True, exist_ok=True)
    if not target.exists():
        # Shader variant names can exceed legacy Windows MAX_PATH under a study archive.
        shutil.copy2("\\\\?\\" + str(file.resolve()), "\\\\?\\" + str(target.resolve()))
    if digest(target) != records[str(relative)]:
        raise RuntimeError(f"Archive content mismatch: {relative}")
write(
    destination / "archive.json",
    dict(
        created_at=stamp(),
        files=records,
        base_commit=subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
        ).strip(),
        limitation="Earlier harness executables were hashed but not archived before this snapshot; current harness reproduces dense and sparse control outputs.",
    ),
)
(destination / "source.patch").write_bytes(
    subprocess.check_output(["git", "diff", "--", "Source", "Shaders", "CMakeLists.txt"], cwd=ROOT)
)
print(destination)
