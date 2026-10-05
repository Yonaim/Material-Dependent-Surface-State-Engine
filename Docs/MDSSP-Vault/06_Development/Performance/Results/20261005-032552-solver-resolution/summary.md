# Performance run — 20261005-032552-solver-resolution

- Status: **complete**
- Captured: 2026-10-05T03:25:52+09:00
- Revision: `9c3c6b0471362481b6de3631478b91d43d8c18b7`
- Dirty working tree: `True`
- Dirty-state SHA-256: `6176c2e48fa589ab9e137ef31a4e5dcf1bce7e73a9f85ed39b03649419355e0b`
- Machine: macOS-26.6.2-arm64-arm-64bit-Mach-O (arm64)
- GPU/runtime: GPU0:; deviceName         = Apple M1
- Matrix: `/Users/yona/Documents/KU/Dream_MDSS_Engine/MDSS_Engine/Config/Performance/solver-resolution-matrix.json`

> GPU timings are per-frame timestamp-query totals. Solver pass times sum the dispatched instance/step ranges for that frame. Compare revisions within the same scene, resolution, and run protocol.

| Scene | Resolution | Repeats | Solver median ms (run median range) | Solver p95 ms (run p95 range) | Pass 1 median ms | Pass 2 median ms |
|---|---:|---:|---:|---:|---:|---:|
| brickcube | 128 | 3 | 2.510 (2.485–2.526) | 3.099 (3.004–3.293) | 1.349 | 1.127 |
| brickcube | 256 | 3 | 6.914 (6.858–7.567) | 9.386 (8.035–10.062) | 2.169 | 4.692 |
| brickcube | 512 | 3 | 22.109 (22.086–22.119) | 25.088 (24.650–30.241) | 5.323 | 16.706 |
| mountain | 128 | 3 | 1.877 (1.127–1.899) | 2.049 (2.005–2.101) | 0.395 | 0.677 |
| mountain | 256 | 3 | 3.728 (3.719–3.817) | 4.837 (4.801–5.236) | 1.706 | 1.968 |
| mountain | 512 | 3 | 12.888 (12.764–13.035) | 21.470 (17.589–21.544) | 3.925 | 8.988 |

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
