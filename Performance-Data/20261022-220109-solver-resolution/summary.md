# Performance run — 20261022-220109-solver-resolution

- Status: **complete**
- Captured: 2026-10-22T22:01:09+09:00
- Revision: `fa83c68b26b83f4765ebb47fded53c946d444fff`
- Dirty working tree: `True`
- Dirty-state SHA-256: `6cbaf4874221f5afdbb81e018756381f1b2864dd6b3607379d18ce5e4ed6b0e6`
- Machine: Windows-11-10.0.26300-SP0 (AMD64)
- GPU/runtime: GPU0:; deviceName         = AMD Radeon RX 6700 XT
- Matrix: `C:\Users\yonai\Documents\MDSS_Engine\Config\Performance\solver-resolution-matrix.json`

> GPU timings are per-frame timestamp-query totals. Solver pass times sum the dispatched stage batches for that frame. Compare revisions within the same scene, resolution, and run protocol.

| Scene | Resolution | Repeats | Solver median ms (run median range) | Solver p95 ms (run p95 range) | Pass 1 median ms | Pass 2 median ms |
|---|---:|---:|---:|---:|---:|---:|
| brickcube | 256 | 3 | 4.824 (4.791–4.834) | 5.629 (5.616–5.681) | 2.272 | 1.843 |
| brickcube | 512 | 3 | 24.255 (24.249–24.813) | 26.418 (26.370–26.967) | 9.177 | 6.409 |
| mountain | 256 | 3 | 8.238 (8.173–8.327) | 8.953 (8.904–9.213) | 2.287 | 2.087 |
| mountain | 512 | 3 | 41.477 (41.105–41.758) | 42.954 (42.860–43.887) | 11.053 | 7.691 |

## Detailed timings

Subranges overlap their parent ranges; do not add all rows. Values are median of repeat medians / median of repeat p95, in ms.

| Scene | Resolution | Metric | Median ms | p95 ms | Repeat median min-max ms |
|---|---:|---|---:|---:|---:|
| brickcube | 256 | Render GPU (includes UI) | 7.737 | 8.372 | 7.692–8.049 |
| brickcube | 256 | Render preparation GPU | 5.198 | 5.774 | 5.169–5.545 |
| brickcube | 256 | Scene draw GPU | 2.246 | 2.739 | 2.236–2.251 |
| brickcube | 256 | Overlay preparation GPU (within render preparation) | 4.329 | 4.853 | 4.290–4.416 |
| brickcube | 256 | Base draw GPU | 2.041 | 2.082 | 2.033–2.044 |
| brickcube | 256 | Overlay draw GPU | 0.152 | 0.164 | 0.148–0.154 |
| brickcube | 256 | Simulation geometry GPU | 0.553 | 1.035 | 0.545–0.589 |
| brickcube | 256 | Transfer weights GPU | 0.037 | 0.098 | 0.031–0.039 |
| brickcube | 256 | Command recording CPU | 0.128 | 0.184 | 0.127–0.129 |
| brickcube | 256 | Frame fence wait CPU | 11.889 | 12.942 | 11.882–12.229 |
| brickcube | 512 | Render GPU (includes UI) | 34.062 | 36.995 | 32.867–35.956 |
| brickcube | 512 | Render preparation GPU | 25.152 | 27.819 | 23.906–26.842 |
| brickcube | 512 | Scene draw GPU | 8.582 | 9.268 | 8.557–8.683 |
| brickcube | 512 | Overlay preparation GPU (within render preparation) | 20.281 | 22.978 | 19.287–21.966 |
| brickcube | 512 | Base draw GPU | 7.687 | 8.066 | 7.669–7.724 |
| brickcube | 512 | Overlay draw GPU | 0.535 | 0.561 | 0.535–0.536 |
| brickcube | 512 | Simulation geometry GPU | 5.090 | 5.468 | 5.064–5.112 |
| brickcube | 512 | Transfer weights GPU | 3.782 | 3.935 | 3.607–4.047 |
| brickcube | 512 | Command recording CPU | 0.192 | 0.253 | 0.189–0.192 |
| brickcube | 512 | Frame fence wait CPU | 57.675 | 61.964 | 56.789–59.521 |
| mountain | 256 | Render GPU (includes UI) | 5.960 | 6.540 | 5.957–5.983 |
| mountain | 256 | Render preparation GPU | 4.550 | 5.124 | 4.529–4.600 |
| mountain | 256 | Scene draw GPU | 1.213 | 1.506 | 1.212–1.215 |
| mountain | 256 | Overlay preparation GPU (within render preparation) | 4.006 | 4.608 | 3.986–4.010 |
| mountain | 256 | Base draw GPU | 1.032 | 1.060 | 1.031–1.034 |
| mountain | 256 | Overlay draw GPU | 0.132 | 0.144 | 0.131–0.132 |
| mountain | 256 | Simulation geometry GPU | 1.249 | 1.728 | 1.175–1.306 |
| mountain | 256 | Transfer weights GPU | 2.573 | 2.748 | 2.537–2.625 |
| mountain | 256 | Command recording CPU | 0.154 | 0.217 | 0.153–0.158 |
| mountain | 256 | Frame fence wait CPU | 13.389 | 14.322 | 13.332–13.526 |
| mountain | 512 | Render GPU (includes UI) | 27.663 | 29.076 | 27.035–29.045 |
| mountain | 512 | Render preparation GPU | 22.579 | 23.844 | 22.048–23.898 |
| mountain | 512 | Scene draw GPU | 4.706 | 5.257 | 4.700–4.725 |
| mountain | 512 | Overlay preparation GPU (within render preparation) | 20.090 | 21.154 | 19.376–20.953 |
| mountain | 512 | Base draw GPU | 3.828 | 4.213 | 3.824–3.867 |
| mountain | 512 | Overlay draw GPU | 0.730 | 0.762 | 0.726–0.731 |
| mountain | 512 | Simulation geometry GPU | 11.840 | 12.456 | 11.610–11.972 |
| mountain | 512 | Transfer weights GPU | 10.936 | 11.425 | 10.913–11.172 |
| mountain | 512 | Command recording CPU | 0.216 | 0.291 | 0.212–0.216 |
| mountain | 512 | Frame fence wait CPU | 68.446 | 71.258 | 67.240–69.150 |

## Final-state and allocation checks

After drain frames and device idle, before resource destruction. Allocation totals include cached meshes; exclude swapchain/driver allocations.

| Run | Submitted frames | Simulation steps | Nonfinite / negative State | Allocation MiB | Peak MiB |
|---|---:|---:|---|---:|---:|
| brickcube-r512-repeat-1 | 662 | 662 | 0 / 0 | 1567.40 | 1567.40 |
| brickcube-r512-repeat-2 | 662 | 662 | 0 / 0 | 1567.40 | 1567.40 |
| mountain-r512-repeat-2 | 662 | 662 | 0 / 0 | 635.76 | 635.76 |
| mountain-r512-repeat-3 | 662 | 662 | 0 / 0 | 635.76 | 635.76 |
| brickcube-r512-repeat-3 | 662 | 662 | 0 / 0 | 1567.40 | 1567.40 |
| mountain-r256-repeat-3 | 662 | 662 | 0 / 0 | 213.24 | 213.24 |
| brickcube-r256-repeat-3 | 662 | 662 | 0 / 0 | 629.63 | 629.63 |
| brickcube-r256-repeat-2 | 662 | 662 | 0 / 0 | 629.63 | 629.63 |
| brickcube-r256-repeat-1 | 662 | 662 | 0 / 0 | 629.63 | 629.63 |
| mountain-r256-repeat-1 | 662 | 662 | 0 / 0 | 213.24 | 213.24 |
| mountain-r256-repeat-2 | 662 | 662 | 0 / 0 | 213.24 | 213.24 |
| mountain-r512-repeat-1 | 662 | 662 | 0 / 0 | 635.76 | 635.76 |

## Run files

| Run | Samples | Raw measurements | App log |
|---|---:|---|---|
| brickcube-r512-repeat-1 | 600 | [`raw/brickcube-r512-repeat-1.jsonl`](raw/brickcube-r512-repeat-1.jsonl) | [`raw/brickcube-r512-repeat-1.log`](raw/brickcube-r512-repeat-1.log) |
| brickcube-r512-repeat-2 | 600 | [`raw/brickcube-r512-repeat-2.jsonl`](raw/brickcube-r512-repeat-2.jsonl) | [`raw/brickcube-r512-repeat-2.log`](raw/brickcube-r512-repeat-2.log) |
| mountain-r512-repeat-2 | 600 | [`raw/mountain-r512-repeat-2.jsonl`](raw/mountain-r512-repeat-2.jsonl) | [`raw/mountain-r512-repeat-2.log`](raw/mountain-r512-repeat-2.log) |
| mountain-r512-repeat-3 | 600 | [`raw/mountain-r512-repeat-3.jsonl`](raw/mountain-r512-repeat-3.jsonl) | [`raw/mountain-r512-repeat-3.log`](raw/mountain-r512-repeat-3.log) |
| brickcube-r512-repeat-3 | 600 | [`raw/brickcube-r512-repeat-3.jsonl`](raw/brickcube-r512-repeat-3.jsonl) | [`raw/brickcube-r512-repeat-3.log`](raw/brickcube-r512-repeat-3.log) |
| mountain-r256-repeat-3 | 600 | [`raw/mountain-r256-repeat-3.jsonl`](raw/mountain-r256-repeat-3.jsonl) | [`raw/mountain-r256-repeat-3.log`](raw/mountain-r256-repeat-3.log) |
| brickcube-r256-repeat-3 | 600 | [`raw/brickcube-r256-repeat-3.jsonl`](raw/brickcube-r256-repeat-3.jsonl) | [`raw/brickcube-r256-repeat-3.log`](raw/brickcube-r256-repeat-3.log) |
| brickcube-r256-repeat-2 | 600 | [`raw/brickcube-r256-repeat-2.jsonl`](raw/brickcube-r256-repeat-2.jsonl) | [`raw/brickcube-r256-repeat-2.log`](raw/brickcube-r256-repeat-2.log) |
| brickcube-r256-repeat-1 | 600 | [`raw/brickcube-r256-repeat-1.jsonl`](raw/brickcube-r256-repeat-1.jsonl) | [`raw/brickcube-r256-repeat-1.log`](raw/brickcube-r256-repeat-1.log) |
| mountain-r256-repeat-1 | 600 | [`raw/mountain-r256-repeat-1.jsonl`](raw/mountain-r256-repeat-1.jsonl) | [`raw/mountain-r256-repeat-1.log`](raw/mountain-r256-repeat-1.log) |
| mountain-r256-repeat-2 | 600 | [`raw/mountain-r256-repeat-2.jsonl`](raw/mountain-r256-repeat-2.jsonl) | [`raw/mountain-r256-repeat-2.log`](raw/mountain-r256-repeat-2.log) |
| mountain-r512-repeat-1 | 600 | [`raw/mountain-r512-repeat-1.jsonl`](raw/mountain-r512-repeat-1.jsonl) | [`raw/mountain-r512-repeat-1.log`](raw/mountain-r512-repeat-1.log) |

## Protocol

- Warm-up frames: 60
- Measurement frames per repeat: 600
- Fixed simulation step: 1/60 s per rendered frame; fixed timestep ON, auto substepping OFF.
- Each repeat starts a fresh process and reloads the Scene initial contacts.
- This report is generated from the listed JSONL files. Recreate it with `python3 Scripts/Performance/summarize.py --manifest <manifest.json>`. 
