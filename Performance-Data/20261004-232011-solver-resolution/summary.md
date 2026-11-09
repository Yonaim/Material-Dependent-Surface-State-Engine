# Performance run — 20261004-232011-solver-resolution

- Status: **complete**
- Captured: 2026-10-04T23:20:11+09:00
- Revision: `63dd78b56c7c5cea5cf8294b9dc9ba2cc3ba6b93`
- Dirty working tree: `True`
- Dirty-state SHA-256: `b9d9b9283d294ba84aa5d2966d27ec97ec497839112cae90032edbd0435655b7`
- Machine: macOS-26.6.2-arm64-arm-64bit-Mach-O (arm64)
- GPU/runtime: GPU0:; deviceName         = Apple M1
- Matrix: `/Users/yona/Documents/KU/Dream_MDSS_Engine/MDSS_Engine/Config/Performance/solver-resolution-matrix.json`

> GPU timings are per-frame timestamp-query totals. Solver pass times sum the dispatched instance/step ranges for that frame. Compare revisions within the same scene, resolution, and run protocol.

| Scene | Resolution | Repeats | Solver median ms (run median range) | Solver p95 ms (run p95 range) | Pass 1 median ms | Pass 2 median ms |
|---|---:|---:|---:|---:|---:|---:|
| brickcube | 128 | 3 | 2.468 (2.024–2.506) | 4.491 (4.398–5.424) | 1.330 | 1.130 |
| brickcube | 256 | 3 | 7.124 (6.617–7.319) | 10.036 (9.406–10.045) | 2.301 | 4.871 |
| brickcube | 512 | 3 | 24.164 (23.409–31.720) | 41.086 (25.862–41.642) | 5.949 | 17.365 |
| mountain | 128 | 3 | 2.024 (1.124–2.174) | 2.548 (1.958–2.645) | 0.447 | 0.744 |
| mountain | 256 | 3 | 4.632 (4.337–4.714) | 7.930 (6.333–13.687) | 1.875 | 2.653 |
| mountain | 512 | 3 | 14.882 (13.676–15.598) | 22.967 (16.158–26.419) | 4.097 | 9.617 |

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
