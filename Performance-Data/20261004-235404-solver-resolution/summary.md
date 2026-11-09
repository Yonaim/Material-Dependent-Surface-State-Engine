# Performance run — 20261004-235404-solver-resolution

- Status: **complete**
- Captured: 2026-10-04T23:54:04+09:00
- Revision: `dd3b5a04f9c9f07adedf33adeca0a13119685978`
- Dirty working tree: `True`
- Dirty-state SHA-256: `a111302168101714bda543e713d87beaa1776d2de3dac24fdda5f75bffe95740`
- Machine: macOS-26.6.2-arm64-arm-64bit-Mach-O (arm64)
- GPU/runtime: GPU0:; deviceName         = Apple M1
- Matrix: `/Users/yona/Documents/KU/Dream_MDSS_Engine/MDSS_Engine/Config/Performance/solver-resolution-matrix.json`

> GPU timings are per-frame timestamp-query totals. Solver pass times sum the dispatched instance/step ranges for that frame. Compare revisions within the same scene, resolution, and run protocol.

| Scene | Resolution | Repeats | Solver median ms (run median range) | Solver p95 ms (run p95 range) | Pass 1 median ms | Pass 2 median ms |
|---|---:|---:|---:|---:|---:|---:|
| brickcube | 128 | 3 | 1.412 (1.086–1.534) | 1.935 (1.465–1.978) | 0.380 | 0.812 |
| brickcube | 256 | 3 | 4.163 (4.145–4.266) | 4.925 (4.891–4.953) | 1.295 | 2.573 |
| brickcube | 512 | 3 | 13.344 (12.819–13.360) | 14.302 (14.289–14.317) | 3.799 | 9.365 |
| mountain | 128 | 3 | 1.029 (1.017–1.029) | 2.236 (2.186–2.263) | 0.354 | 0.536 |
| mountain | 256 | 3 | 2.816 (2.679–2.837) | 4.098 (3.343–4.242) | 0.813 | 1.650 |
| mountain | 512 | 3 | 7.916 (7.882–7.924) | 8.709 (8.705–8.731) | 2.434 | 5.329 |

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
