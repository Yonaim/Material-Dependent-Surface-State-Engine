# Optimization A/B summary

Completed runs: 357/357; status=complete

Time values are median of 3 per-run medians. Percent reductions use the indicated scope, not FPS. State/image differences are measured at the final drain frame.

## G1

| Comparison | Scene / resolution | Base | Candidate | Reduction % | State relative L1 | Image MAE (0–255) | Repeat ranges |
|---|---|---:|---:|---:|---:|---:|---|
| shared_profiles | Mountain-DistinctRuntime / 256 | 624.0000 | 208.0000 | 66.67 | 0.000e+00 | 0.000 | decrease_observed |
| shared_profiles | Mountain-DistinctRuntime / 512 | 624.0000 | 208.0000 | 66.67 | 0.000e+00 | 0.000 | decrease_observed |

## G2

| Comparison | Scene / resolution | Base | Candidate | Reduction % | State relative L1 | Image MAE (0–255) | Repeat ranges |
|---|---|---:|---:|---:|---:|---:|---|
| raw_flux_cache_fp32 | BrickCube-Wide / 256 | 62.1981 | 57.7906 | 7.09 | 1.061e-03 | 0.019 | decrease_observed |
| raw_flux_cache_fp32 | BrickCube-Wide / 512 | 296.3517 | 286.6259 | 3.28 | 6.494e-03 | 0.029 | decrease_observed |
| inactive_zero_write_elision | BrickCube-Wide / 256 | 57.2463 | 57.8336 | -1.03 | 0.000e+00 | 0.000 | increase_observed |
| inactive_zero_write_elision | BrickCube-Wide / 512 | 282.2516 | 285.4239 | -1.12 | 0.000e+00 | 0.000 | increase_observed |
| source_reuse | BrickCube-Source-Reuse / 256 | 6.1832 | 4.7670 | 22.90 | 0.000e+00 | 0.000 | decrease_observed |
| source_reuse | BrickCube-Source-Reuse / 512 | 27.5469 | 23.7143 | 13.91 | 0.000e+00 | 0.000 | decrease_observed |
| source_reuse | Mountain-Source-Reuse / 256 | 10.9864 | 8.1262 | 26.03 | 2.508e-04 | 0.001 | decrease_observed |
| source_reuse | Mountain-Source-Reuse / 512 | 46.2720 | 40.1425 | 13.25 | 1.615e-02 | 0.021 | decrease_observed |

## G3

| Comparison | Scene / resolution | Base | Candidate | Reduction % | State relative L1 | Image MAE (0–255) | Repeat ranges |
|---|---|---:|---:|---:|---:|---:|---|
| sparse_solver | BrickCube-Wide / 256 | 57.1557 | 57.6706 | -0.90 | 0.000e+00 | 0.000 | increase_observed |
| sparse_solver | BrickCube-Wide / 512 | 262.4917 | 284.7581 | -8.48 | 0.000e+00 | 0.000 | increase_observed |
| sparse_height | BrickCube-Wide / 256 | 58.1708 | 57.6706 | 0.86 | 0.000e+00 | 0.000 | decrease_observed |
| sparse_height | BrickCube-Wide / 512 | 283.0443 | 284.7581 | -0.61 | 0.000e+00 | 0.000 | overlapping_repeat_ranges |
| sparse_geometry | BrickCube-Wide / 256 | 75.7190 | 57.6706 | 23.84 | 3.273e-04 | 0.003 | decrease_observed |
| sparse_geometry | BrickCube-Wide / 512 | 216.8128 | 284.7581 | -31.34 | 5.404e-03 | 0.010 | increase_observed |
| sparse_all | BrickCube-Wide / 256 | 101.4497 | 57.6706 | 43.15 | 3.273e-04 | 0.003 | decrease_observed |
| sparse_all | BrickCube-Wide / 512 | 273.3942 | 284.7581 | -4.16 | 5.404e-03 | 0.010 | increase_observed |
| global_channel_mask | BrickCube-Mixed / 256 | 8.2511 | 8.4399 | -2.29 | 0.000e+00 | 0.000 | overlapping_repeat_ranges |
| global_channel_mask | BrickCube-Wide / 256 | 85.7133 | 57.7987 | 32.57 | 0.000e+00 | 0.000 | decrease_observed |
| global_channel_mask | BrickCube-Wide / 512 | 389.3238 | 284.9654 | 26.81 | 0.000e+00 | 0.000 | decrease_observed |
| workgroup_channel_mask | BrickCube-Mixed / 256 | 8.4399 | 5.5792 | 33.89 | 0.000e+00 | 0.000 | decrease_observed |
| workgroup_channel_mask | BrickCube-Wide / 256 | 57.7987 | 57.6706 | 0.22 | 0.000e+00 | 0.000 | overlapping_repeat_ranges |
| workgroup_channel_mask | BrickCube-Wide / 512 | 284.9654 | 284.7581 | 0.07 | 0.000e+00 | 0.000 | overlapping_repeat_ranges |

## G4

| Comparison | Scene / resolution | Base | Candidate | Reduction % | State relative L1 | Image MAE (0–255) | Repeat ranges |
|---|---|---:|---:|---:|---:|---:|---|
| instance_batching | Mountain-1-identical / 256 | 1.6647 | 1.6918 | -1.63 | 0.000e+00 | 0.000 | overlapping_repeat_ranges |
| instance_batching | Mountain-1-identical / 512 | 14.3823 | 14.4526 | -0.49 | 0.000e+00 | 0.000 | overlapping_repeat_ranges |
| instance_batching | Mountain-3-identical / 256 | 4.9968 | 7.3786 | -47.67 | 0.000e+00 | 0.000 | increase_observed |
| instance_batching | Mountain-3-identical / 512 | 43.3720 | 40.6341 | 6.31 | 0.000e+00 | 0.000 | decrease_observed |
| instance_batching | Mountain-6-identical / 256 | 10.1105 | 14.7327 | -45.72 | 0.000e+00 | 0.000 | increase_observed |
| instance_batching | Mountain-6-identical / 512 | 87.0820 | 80.2335 | 7.86 | 0.000e+00 | 0.000 | decrease_observed |

## G7

| Comparison | Scene / resolution | Base | Candidate | Reduction % | State relative L1 | Image MAE (0–255) | Repeat ranges |
|---|---|---:|---:|---:|---:|---:|---|
| dirty_overlay | BrickCube-Wide-Overlay / 256 | 56.4862 | 53.5336 | 5.23 | 0.000e+00 | 0.000 | decrease_observed |
| dirty_overlay | BrickCube-Wide-Overlay / 512 | 199.9891 | 200.5855 | -0.30 | 0.000e+00 | 0.000 | overlapping_repeat_ranges |
| tile_culling | BrickCube-Wide-Overlay / 256 | 36.0345 | 53.5336 | -48.56 | 0.000e+00 | 0.000 | increase_observed |
| tile_culling | BrickCube-Wide-Overlay / 512 | 130.5810 | 200.5855 | -53.61 | 0.000e+00 | 0.000 | increase_observed |
| frustum_culling | BrickCube-Offscreen / 256 | 7.1237 | 5.3260 | 25.23 | 0.000e+00 | 0.000 | decrease_observed |
| frustum_culling | BrickCube-Offscreen / 512 | 30.2561 | 22.2404 | 26.49 | 0.000e+00 | 0.000 | decrease_observed |
| frustum_culling | Mountain-Offscreen / 256 | 5.1768 | 4.2269 | 18.35 | 0.000e+00 | 0.000 | decrease_observed |
| frustum_culling | Mountain-Offscreen / 512 | 24.0038 | 20.6414 | 14.01 | 0.000e+00 | 0.000 | decrease_observed |
| tile8 | BrickCube / 256 | 7.3281 | 6.1909 | 15.52 | 0.000e+00 | 0.000 | decrease_observed |
| tile8 | BrickCube / 512 | 32.7393 | 29.2846 | 10.55 | 0.000e+00 | 0.001 | decrease_observed |
| tile8 | Mountain / 256 | 5.2631 | 4.0987 | 22.12 | 0.000e+00 | 0.000 | decrease_observed |
| tile8 | Mountain / 512 | 24.7593 | 25.6728 | -3.69 | 0.000e+00 | 0.000 | increase_observed |
| tile32 | BrickCube / 256 | 7.3281 | 7.7503 | -5.76 | 0.000e+00 | 0.000 | increase_observed |
| tile32 | BrickCube / 512 | 32.7393 | 33.4885 | -2.29 | 0.000e+00 | 0.000 | overlapping_repeat_ranges |
| tile32 | Mountain / 256 | 5.2631 | 9.3743 | -78.11 | 0.000e+00 | 0.000 | increase_observed |
| tile32 | Mountain / 512 | 24.7593 | 29.3944 | -18.72 | 0.000e+00 | 0.000 | increase_observed |
| mesh64 | BrickCube / 256 | 7.3281 | 4.7043 | 35.81 | 0.000e+00 | 2.095 | decrease_observed |
| mesh64 | BrickCube / 512 | 32.7393 | 21.1086 | 35.53 | 0.000e+00 | 2.537 | decrease_observed |
| mesh64 | Mountain / 256 | 5.2631 | 3.6780 | 30.12 | 0.000e+00 | 0.113 | decrease_observed |
| mesh64 | Mountain / 512 | 24.7593 | 17.8205 | 28.02 | 0.000e+00 | 0.231 | decrease_observed |
| mesh256 | BrickCube / 512 | 32.7393 | 23.0533 | 29.59 | 0.000e+00 | 0.597 | decrease_observed |
| mesh256 | Mountain / 512 | 24.7593 | 19.7527 | 20.22 | 0.000e+00 | 0.117 | decrease_observed |
| mesh512 | BrickCube / 256 | 7.3281 | 15.0785 | -105.76 | 0.000e+00 | 0.083 | increase_observed |
| mesh512 | Mountain / 256 | 5.2631 | 10.4700 | -98.93 | 0.000e+00 | 0.050 | increase_observed |
| top_triangle_compaction | BrickCube-TopDraw / 256 | 9.1836 | 7.3610 | 19.85 | 0.000e+00 | 0.000 | decrease_observed |
| top_triangle_compaction | BrickCube-TopDraw / 512 | 37.2509 | 30.2387 | 18.82 | 0.000e+00 | 0.000 | decrease_observed |
| top_triangle_compaction | BrickCube-Wide-TopDraw / 256 | 50.6637 | 55.7920 | -10.12 | 0.000e+00 | 0.000 | increase_observed |
| top_triangle_compaction | BrickCube-Wide-TopDraw / 512 | 215.4849 | 198.6791 | 7.80 | 0.000e+00 | 0.000 | overlapping_repeat_ranges |
| top_triangle_compaction | Mountain-TopDraw / 256 | 6.1968 | 5.1796 | 16.42 | 0.000e+00 | 0.000 | decrease_observed |
| top_triangle_compaction | Mountain-TopDraw / 512 | 28.3380 | 24.7028 | 12.83 | 0.000e+00 | 0.000 | decrease_observed |

## G6

| Comparison | Scene / resolution | Base | Candidate | Reduction % | State relative L1 | Image MAE (0–255) | Repeat ranges |
|---|---|---:|---:|---:|---:|---:|---|
| sparse_render_height | BrickCube / 256 | 8.4168 | 7.4135 | 11.92 | 0.000e+00 | 0.000 | decrease_observed |
| sparse_render_height | BrickCube / 512 | 34.8072 | 30.7556 | 11.64 | 0.000e+00 | 0.000 | overlapping_repeat_ranges |
| sparse_render_height | Mountain / 256 | 5.6378 | 5.2451 | 6.97 | 0.000e+00 | 0.000 | decrease_observed |
| sparse_render_height | Mountain / 512 | 25.7950 | 25.0063 | 3.06 | 0.000e+00 | 0.000 | decrease_observed |
| render_math_reuse | BrickCube-Render-Reuse / 256 | 7.4696 | 7.5094 | -0.53 | 0.000e+00 | 0.000 | overlapping_repeat_ranges |
| render_math_reuse | BrickCube-Render-Reuse / 512 | 32.1459 | 30.2768 | 5.81 | 0.000e+00 | 0.000 | overlapping_repeat_ranges |
| render_math_reuse | Mountain-Render-Reuse / 256 | 5.2064 | 5.2615 | -1.06 | 0.000e+00 | 0.000 | increase_observed |
| render_math_reuse | Mountain-Render-Reuse / 512 | 24.5981 | 24.4501 | 0.60 | 0.000e+00 | 0.000 | overlapping_repeat_ranges |
| render_math_reuse_ssbo | BrickCube-SSBO-Reuse / 256 | 11.1942 | 10.5287 | 5.95 | 0.000e+00 | 0.000 | decrease_observed |
| render_math_reuse_ssbo | BrickCube-SSBO-Reuse / 512 | 38.9998 | 37.2205 | 4.56 | 0.000e+00 | 0.000 | decrease_observed |
| render_math_reuse_ssbo | Mountain-SSBO-Reuse / 256 | 6.7864 | 6.4736 | 4.61 | 0.000e+00 | 0.000 | decrease_observed |
| render_math_reuse_ssbo | Mountain-SSBO-Reuse / 512 | 27.8556 | 27.3057 | 1.97 | 0.000e+00 | 0.000 | decrease_observed |

