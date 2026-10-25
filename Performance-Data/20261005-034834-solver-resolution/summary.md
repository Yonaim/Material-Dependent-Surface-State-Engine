# Performance run — 20261005-034834-solver-resolution

- Status: **complete**
- Captured: 2026-10-05T03:48:34+09:00
- Revision: `9c3c6b0471362481b6de3631478b91d43d8c18b7`
- Dirty working tree: `True`
- Dirty-state SHA-256: `476f41ccb8dc3f5d9002b2b47b5522eaf23c87a746a327b6388b993da8249368`
- Machine: macOS-26.6.2-arm64-arm-64bit-Mach-O (arm64)
- GPU/runtime: GPU0:; deviceName         = Apple M1
- Matrix: `/Users/yona/Documents/KU/Dream_MDSS_Engine/MDSS_Engine/Config/Performance/solver-resolution-matrix.json`

> GPU timings are per-frame timestamp-query totals. Solver pass times sum the dispatched instance/step ranges for that frame. Compare revisions within the same scene, resolution, and run protocol.

| Scene | Resolution | Repeats | Solver median ms (run median range) | Solver p95 ms (run p95 range) | Pass 1 median ms | Pass 2 median ms |
|---|---:|---:|---:|---:|---:|---:|
| brickcube | 128 | 3 | 2.439 (2.387–2.490) | 2.958 (2.910–3.121) | 1.355 | 1.125 |
| brickcube | 256 | 3 | 6.544 (6.531–6.982) | 8.645 (7.337–9.028) | 2.182 | 4.340 |
| brickcube | 512 | 3 | 20.114 (20.095–20.162) | 21.987 (21.552–30.376) | 5.296 | 14.792 |
| mountain | 128 | 3 | 1.809 (1.805–1.814) | 1.935 (1.931–2.000) | 0.395 | 0.573 |
| mountain | 256 | 3 | 3.571 (3.545–3.587) | 4.979 (4.487–5.257) | 1.704 | 1.796 |
| mountain | 512 | 3 | 11.859 (11.820–12.754) | 12.925 (12.891–17.559) | 3.791 | 7.978 |

## Run files

| Run | Samples | Raw measurements | App log |
|---|---:|---|---|
| brickcube-r256-repeat-3 | 180 | [`raw/brickcube-r256-repeat-3.jsonl`](raw/brickcube-r256-repeat-3.jsonl) | [`raw/brickcube-r256-repeat-3.log`](raw/brickcube-r256-repeat-3.log) |
| brickcube-r128-repeat-3 | 180 | [`raw/brickcube-r128-repeat-3.jsonl`](raw/brickcube-r128-repeat-3.jsonl) | [`raw/brickcube-r128-repeat-3.log`](raw/brickcube-r128-repeat-3.log) |
| brickcube-r512-repeat-2 | 180 | [`raw/brickcube-r512-repeat-2.jsonl`](raw/brickcube-r512-repeat-2.jsonl) | [`raw/brickcube-r512-repeat-2.log`](raw/brickcube-r512-repeat-2.log) |
| mountain-r128-repeat-3 | 180 | [`raw/mountain-r128-repeat-3.jsonl`](raw/mountain-r128-repeat-3.jsonl) | [`raw/mountain-r128-repeat-3.log`](raw/mountain-r128-repeat-3.log) |
| mountain-r512-repeat-2 | 180 | [`raw/mountain-r512-repeat-2.jsonl`](raw/mountain-r512-repeat-2.jsonl) | [`raw/mountain-r512-repeat-2.log`](raw/mountain-r512-repeat-2.log) |
| brickcube-r256-repeat-1 | 180 | [`raw/brickcube-r256-repeat-1.jsonl`](raw/brickcube-r256-repeat-1.jsonl) | [`raw/brickcube-r256-repeat-1.log`](raw/brickcube-r256-repeat-1.log) |
| brickcube-r512-repeat-1 | 180 | [`raw/brickcube-r512-repeat-1.jsonl`](raw/brickcube-r512-repeat-1.jsonl) | [`raw/brickcube-r512-repeat-1.log`](raw/brickcube-r512-repeat-1.log) |
| mountain-r128-repeat-2 | 180 | [`raw/mountain-r128-repeat-2.jsonl`](raw/mountain-r128-repeat-2.jsonl) | [`raw/mountain-r128-repeat-2.log`](raw/mountain-r128-repeat-2.log) |
| mountain-r128-repeat-1 | 180 | [`raw/mountain-r128-repeat-1.jsonl`](raw/mountain-r128-repeat-1.jsonl) | [`raw/mountain-r128-repeat-1.log`](raw/mountain-r128-repeat-1.log) |
| mountain-r512-repeat-1 | 180 | [`raw/mountain-r512-repeat-1.jsonl`](raw/mountain-r512-repeat-1.jsonl) | [`raw/mountain-r512-repeat-1.log`](raw/mountain-r512-repeat-1.log) |
| brickcube-r256-repeat-2 | 180 | [`raw/brickcube-r256-repeat-2.jsonl`](raw/brickcube-r256-repeat-2.jsonl) | [`raw/brickcube-r256-repeat-2.log`](raw/brickcube-r256-repeat-2.log) |
| brickcube-r128-repeat-2 | 180 | [`raw/brickcube-r128-repeat-2.jsonl`](raw/brickcube-r128-repeat-2.jsonl) | [`raw/brickcube-r128-repeat-2.log`](raw/brickcube-r128-repeat-2.log) |
| mountain-r512-repeat-3 | 180 | [`raw/mountain-r512-repeat-3.jsonl`](raw/mountain-r512-repeat-3.jsonl) | [`raw/mountain-r512-repeat-3.log`](raw/mountain-r512-repeat-3.log) |
| mountain-r256-repeat-2 | 180 | [`raw/mountain-r256-repeat-2.jsonl`](raw/mountain-r256-repeat-2.jsonl) | [`raw/mountain-r256-repeat-2.log`](raw/mountain-r256-repeat-2.log) |
| brickcube-r512-repeat-3 | 180 | [`raw/brickcube-r512-repeat-3.jsonl`](raw/brickcube-r512-repeat-3.jsonl) | [`raw/brickcube-r512-repeat-3.log`](raw/brickcube-r512-repeat-3.log) |
| brickcube-r128-repeat-1 | 180 | [`raw/brickcube-r128-repeat-1.jsonl`](raw/brickcube-r128-repeat-1.jsonl) | [`raw/brickcube-r128-repeat-1.log`](raw/brickcube-r128-repeat-1.log) |
| mountain-r256-repeat-1 | 180 | [`raw/mountain-r256-repeat-1.jsonl`](raw/mountain-r256-repeat-1.jsonl) | [`raw/mountain-r256-repeat-1.log`](raw/mountain-r256-repeat-1.log) |
| mountain-r256-repeat-3 | 180 | [`raw/mountain-r256-repeat-3.jsonl`](raw/mountain-r256-repeat-3.jsonl) | [`raw/mountain-r256-repeat-3.log`](raw/mountain-r256-repeat-3.log) |

## Protocol

- Warm-up frames: 30
- Measurement frames per repeat: 180
- Fixed simulation step: 1/60 s per rendered frame; fixed timestep ON, auto substepping OFF.
- Each repeat starts a fresh process and reloads the Scene initial contacts.
- This report is generated from the listed JSONL files. Recreate it with `python3 Scripts/Performance/summarize.py --manifest <manifest.json>`. 
