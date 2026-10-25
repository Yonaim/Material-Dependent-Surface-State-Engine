# Optimization A/B summary

Completed runs: 432/432; status=complete

Time values are median of 3 per-run medians. Percent reductions use the indicated scope, not FPS. State/image differences are measured at the final drain frame.

## G1

| Comparison | Scene / resolution | Base | Candidate | Reduction % | State relative L1 | Image MAE (0–255) | Repeat ranges |
|---|---|---:|---:|---:|---:|---:|---|
| disk_cache | BrickCube / 256 | 1825.1510 | 169.8341 | 90.69 | 0.000e+00 | 0.000 | decrease_observed |
| disk_cache | BrickCube / 512 | 7421.2100 | 620.6212 | 91.64 | 0.000e+00 | 0.000 | decrease_observed |
| disk_cache | Mountain / 256 | 75.8110 | 47.2175 | 37.72 | 0.000e+00 | 0.000 | decrease_observed |
| disk_cache | Mountain / 512 | 244.9150 | 126.2695 | 48.44 | 0.000e+00 | 0.000 | decrease_observed |
| buffer_pool | BrickCube / 256 | 3266.4901 | 3242.9583 | 0.72 | 0.000e+00 | 0.000 | overlapping_repeat_ranges |
| buffer_pool | BrickCube / 512 | 5394.5082 | 5493.7158 | -1.84 | 0.000e+00 | 0.000 | increase_observed |
| buffer_pool | Mountain / 256 | 585.3021 | 526.0210 | 10.13 | 0.000e+00 | 0.000 | decrease_observed |
| buffer_pool | Mountain / 512 | 890.8669 | 816.9591 | 8.30 | 0.000e+00 | 0.000 | decrease_observed |
| shared_profiles | BrickCube / 256 | 208.0000 | 208.0000 | 0.00 | 0.000e+00 | 0.000 | overlapping_repeat_ranges |
| shared_profiles | BrickCube / 512 | 208.0000 | 208.0000 | 0.00 | 0.000e+00 | 0.000 | overlapping_repeat_ranges |
| shared_profiles | Mountain / 256 | 624.0000 | 208.0000 | 66.67 | 0.000e+00 | 0.000 | decrease_observed |
| shared_profiles | Mountain / 512 | 624.0000 | 208.0000 | 66.67 | 0.000e+00 | 0.000 | decrease_observed |

## G2

| Comparison | Scene / resolution | Base | Candidate | Reduction % | State relative L1 | Image MAE (0–255) | Repeat ranges |
|---|---|---:|---:|---:|---:|---:|---|
| raw_flux_cache_fp32 | BrickCube / 256 | 11.4691 | 10.0204 | 12.63 | 8.019e-04 | 0.002 | decrease_observed |
| raw_flux_cache_fp32 | BrickCube / 512 | 42.0434 | 38.7559 | 7.82 | 3.845e-03 | 0.004 | decrease_observed |
| raw_flux_cache_fp32 | Mountain / 256 | 15.4941 | 14.1492 | 8.68 | 9.830e-04 | 0.002 | decrease_observed |
| raw_flux_cache_fp32 | Mountain / 512 | 53.2932 | 51.2165 | 3.90 | 1.054e-02 | 0.024 | decrease_observed |
| inactive_zero_write_elision | BrickCube / 256 | 4.8251 | 4.8612 | -0.75 | 0.000e+00 | 0.000 | overlapping_repeat_ranges |
| inactive_zero_write_elision | BrickCube / 512 | 24.3375 | 24.4766 | -0.57 | 0.000e+00 | 0.000 | increase_observed |
| inactive_zero_write_elision | Mountain / 256 | 8.5788 | 8.3187 | 3.03 | 0.000e+00 | 0.000 | overlapping_repeat_ranges |
| inactive_zero_write_elision | Mountain / 512 | 41.1533 | 41.2336 | -0.20 | 0.000e+00 | 0.000 | overlapping_repeat_ranges |

## G3

| Comparison | Scene / resolution | Base | Candidate | Reduction % | State relative L1 | Image MAE (0–255) | Repeat ranges |
|---|---|---:|---:|---:|---:|---:|---|
| sparse_solver | BrickCube / 256 | 4.3291 | 4.7811 | -10.44 | 0.000e+00 | 0.000 | increase_observed |
| sparse_solver | BrickCube / 512 | 23.8622 | 23.8772 | -0.06 | 0.000e+00 | 0.000 | overlapping_repeat_ranges |
| sparse_solver | Mountain / 256 | 8.0375 | 8.1906 | -1.91 | 0.000e+00 | 0.000 | overlapping_repeat_ranges |
| sparse_solver | Mountain / 512 | 38.1027 | 40.2712 | -5.69 | 0.000e+00 | 0.000 | increase_observed |
| sparse_height | BrickCube / 256 | 6.0942 | 4.7811 | 21.55 | 0.000e+00 | 0.000 | decrease_observed |
| sparse_height | BrickCube / 512 | 28.9494 | 23.8772 | 17.52 | 0.000e+00 | 0.000 | decrease_observed |
| sparse_height | Mountain / 256 | 8.5595 | 8.1906 | 4.31 | 0.000e+00 | 0.000 | overlapping_repeat_ranges |
| sparse_height | Mountain / 512 | 42.4502 | 40.2712 | 5.13 | 0.000e+00 | 0.001 | decrease_observed |
| sparse_geometry | BrickCube / 256 | 42.9140 | 4.7811 | 88.86 | 2.587e-04 | 0.000 | decrease_observed |
| sparse_geometry | BrickCube / 512 | 95.1789 | 23.8772 | 74.91 | 2.271e-03 | 0.000 | decrease_observed |
| sparse_geometry | Mountain / 256 | 23.8130 | 8.1906 | 65.60 | 1.993e-04 | 0.001 | decrease_observed |
| sparse_geometry | Mountain / 512 | 67.4046 | 40.2712 | 40.25 | 7.793e-03 | 0.022 | decrease_observed |
| sparse_all | BrickCube / 256 | 83.4542 | 4.7811 | 94.27 | 2.587e-04 | 0.000 | decrease_observed |
| sparse_all | BrickCube / 512 | 216.9685 | 23.8772 | 89.00 | 2.271e-03 | 0.000 | decrease_observed |
| sparse_all | Mountain / 256 | 38.3060 | 8.1906 | 78.62 | 1.993e-04 | 0.001 | decrease_observed |
| sparse_all | Mountain / 512 | 127.3684 | 40.2712 | 68.38 | 7.793e-03 | 0.022 | decrease_observed |
| global_channel_mask | BrickCube / 256 | 8.2162 | 4.7067 | 42.71 | 0.000e+00 | 0.000 | decrease_observed |
| global_channel_mask | BrickCube / 512 | 36.4212 | 23.8387 | 34.55 | 0.000e+00 | 0.000 | decrease_observed |
| global_channel_mask | Mountain / 256 | 12.1800 | 8.2628 | 32.16 | 0.000e+00 | 0.000 | decrease_observed |
| global_channel_mask | Mountain / 512 | 56.0005 | 40.3694 | 27.91 | 0.000e+00 | 0.000 | decrease_observed |
| workgroup_channel_mask | BrickCube / 256 | 4.7067 | 4.7811 | -1.58 | 0.000e+00 | 0.000 | overlapping_repeat_ranges |
| workgroup_channel_mask | BrickCube / 512 | 23.8387 | 23.8772 | -0.16 | 0.000e+00 | 0.000 | overlapping_repeat_ranges |
| workgroup_channel_mask | Mountain / 256 | 8.2628 | 8.1906 | 0.87 | 0.000e+00 | 0.000 | overlapping_repeat_ranges |
| workgroup_channel_mask | Mountain / 512 | 40.3694 | 40.2712 | 0.24 | 0.000e+00 | 0.001 | overlapping_repeat_ranges |

## G4

| Comparison | Scene / resolution | Base | Candidate | Reduction % | State relative L1 | Image MAE (0–255) | Repeat ranges |
|---|---|---:|---:|---:|---:|---:|---|
| instance_batching | Mountain / 256 | 4.9873 | 8.1953 | -64.32 | 0.000e+00 | 0.000 | increase_observed |
| instance_batching | Mountain / 512 | 42.8189 | 40.1324 | 6.27 | 0.000e+00 | 0.000 | decrease_observed |

## G5

| Comparison | Scene / resolution | Base | Candidate | Reduction % | State relative L1 | Image MAE (0–255) | Repeat ranges |
|---|---|---:|---:|---:|---:|---:|---|
| coalesced_fp32 | BrickCube / 256 | 16.1290 | 9.6835 | 39.96 | 0.000e+00 | 0.000 | decrease_observed |
| coalesced_fp32 | BrickCube / 512 | 60.7297 | 39.1009 | 35.61 | 0.000e+00 | 0.000 | decrease_observed |
| coalesced_fp32 | Mountain / 256 | 21.9150 | 13.9311 | 36.43 | 0.000e+00 | 0.000 | decrease_observed |
| coalesced_fp32 | Mountain / 512 | 71.2143 | 49.8030 | 30.07 | 0.000e+00 | 0.000 | decrease_observed |
| fp16_flux | BrickCube / 256 | 9.6835 | 4.7213 | 51.24 | 1.654e-02 | 0.016 | decrease_observed |
| fp16_flux | BrickCube / 512 | 39.1009 | 24.0634 | 38.46 | 3.525e-02 | 0.034 | decrease_observed |
| fp16_flux | Mountain / 256 | 13.9311 | 8.2656 | 40.67 | 1.091e-02 | 0.009 | decrease_observed |
| fp16_flux | Mountain / 512 | 49.8030 | 40.5240 | 18.63 | 6.161e-02 | 0.040 | decrease_observed |
| fp16_weights | BrickCube / 256 | 4.7213 | 4.4928 | 4.84 | 3.394e-04 | 0.000 | decrease_observed |
| fp16_weights | BrickCube / 512 | 24.0634 | 22.3327 | 7.19 | 2.584e-03 | 0.001 | decrease_observed |
| fp16_weights | Mountain / 256 | 8.2656 | 7.1462 | 13.54 | 5.095e-03 | 0.003 | decrease_observed |
| fp16_weights | Mountain / 512 | 40.5240 | 36.8634 | 9.03 | 3.118e-02 | 0.024 | decrease_observed |

## G6

| Comparison | Scene / resolution | Base | Candidate | Reduction % | State relative L1 | Image MAE (0–255) | Repeat ranges |
|---|---|---:|---:|---:|---:|---:|---|
| state_texture | BrickCube / 256 | 10.5902 | 10.7381 | -1.40 | 0.000e+00 | 0.000 | increase_observed |
| state_texture | BrickCube / 512 | 37.3709 | 39.4704 | -5.62 | 0.000e+00 | 0.000 | increase_observed |
| state_texture | Mountain / 256 | 6.6649 | 6.7661 | -1.52 | 0.000e+00 | 0.000 | increase_observed |
| state_texture | Mountain / 512 | 27.2333 | 27.8535 | -2.28 | 0.000e+00 | 0.001 | overlapping_repeat_ranges |
| precompute_coverage | BrickCube / 256 | 10.7381 | 7.4023 | 31.06 | 0.000e+00 | 0.060 | decrease_observed |
| precompute_coverage | BrickCube / 512 | 39.4704 | 30.1693 | 23.56 | 0.000e+00 | 0.042 | decrease_observed |
| precompute_coverage | Mountain / 256 | 6.7661 | 5.3534 | 20.88 | 0.000e+00 | 0.046 | decrease_observed |
| precompute_coverage | Mountain / 512 | 27.8535 | 24.8465 | 10.80 | 0.000e+00 | 0.039 | decrease_observed |
| separable_coverage | BrickCube / 256 | 7.4492 | 7.4023 | 0.63 | 0.000e+00 | 0.000 | decrease_observed |
| separable_coverage | BrickCube / 512 | 29.4565 | 30.1693 | -2.42 | 0.000e+00 | 0.000 | increase_observed |
| separable_coverage | Mountain / 256 | 5.5146 | 5.3534 | 2.92 | 0.000e+00 | 0.000 | decrease_observed |
| separable_coverage | Mountain / 512 | 24.7721 | 24.8465 | -0.30 | 0.000e+00 | 0.001 | overlapping_repeat_ranges |
| height_halo | BrickCube / 256 | 7.4023 | 7.4109 | -0.12 | 0.000e+00 | 0.000 | overlapping_repeat_ranges |
| height_halo | BrickCube / 512 | 30.1693 | 29.6485 | 1.73 | 0.000e+00 | 0.000 | overlapping_repeat_ranges |
| height_halo | Mountain / 256 | 5.3534 | 5.4688 | -2.16 | 0.000e+00 | 0.000 | increase_observed |
| height_halo | Mountain / 512 | 24.8465 | 24.3333 | 2.07 | 0.000e+00 | 0.000 | decrease_observed |
| coverage_halo | BrickCube / 256 | 7.4023 | 7.3091 | 1.26 | 0.000e+00 | 0.000 | decrease_observed |
| coverage_halo | BrickCube / 512 | 30.1693 | 28.9930 | 3.90 | 0.000e+00 | 0.000 | decrease_observed |
| coverage_halo | Mountain / 256 | 5.3534 | 5.1236 | 4.29 | 0.000e+00 | 0.000 | decrease_observed |
| coverage_halo | Mountain / 512 | 24.8465 | 24.1839 | 2.67 | 0.000e+00 | 0.000 | overlapping_repeat_ranges |
| state_texture_halo | BrickCube / 256 | 7.4023 | 7.4397 | -0.50 | 0.000e+00 | 0.000 | increase_observed |
| state_texture_halo | BrickCube / 512 | 30.1693 | 30.0386 | 0.43 | 0.000e+00 | 0.000 | overlapping_repeat_ranges |
| state_texture_halo | Mountain / 256 | 5.3534 | 5.3410 | 0.23 | 0.000e+00 | 0.000 | overlapping_repeat_ranges |
| state_texture_halo | Mountain / 512 | 24.8465 | 24.9844 | -0.56 | 0.000e+00 | 0.000 | overlapping_repeat_ranges |

## G7

| Comparison | Scene / resolution | Base | Candidate | Reduction % | State relative L1 | Image MAE (0–255) | Repeat ranges |
|---|---|---:|---:|---:|---:|---:|---|
| dirty_overlay | BrickCube / 256 | 27.2689 | 7.3830 | 72.93 | 0.000e+00 | 0.000 | decrease_observed |
| dirty_overlay | BrickCube / 512 | 88.9895 | 29.9645 | 66.33 | 0.000e+00 | 0.000 | decrease_observed |
| dirty_overlay | Mountain / 256 | 10.9654 | 5.3020 | 51.65 | 0.000e+00 | 0.000 | decrease_observed |
| dirty_overlay | Mountain / 512 | 55.4601 | 24.6196 | 55.61 | 0.000e+00 | 0.000 | decrease_observed |
| tile_culling | BrickCube / 256 | 10.6052 | 7.3830 | 30.38 | 0.000e+00 | 0.000 | decrease_observed |
| tile_culling | BrickCube / 512 | 41.4237 | 29.9645 | 27.66 | 0.000e+00 | 0.000 | decrease_observed |
| tile_culling | Mountain / 256 | 6.6030 | 5.3020 | 19.70 | 0.000e+00 | 0.000 | decrease_observed |
| tile_culling | Mountain / 512 | 30.9162 | 24.6196 | 20.37 | 0.000e+00 | 0.000 | decrease_observed |
| mesh128 | BrickCube / 256 | 7.3830 | 5.3189 | 27.96 | 0.000e+00 | 0.868 | decrease_observed |
| mesh128 | BrickCube / 512 | 29.9645 | 20.2462 | 32.43 | 0.000e+00 | 1.468 | decrease_observed |
| mesh128 | Mountain / 256 | 5.3020 | 4.2022 | 20.74 | 0.000e+00 | 0.080 | decrease_observed |
| mesh128 | Mountain / 512 | 24.6196 | 18.0313 | 26.76 | 0.000e+00 | 0.178 | decrease_observed |
| frustum_culling | BrickCube / 256 | 8.2741 | 7.3830 | 10.77 | 0.000e+00 | 0.000 | overlapping_repeat_ranges |
| frustum_culling | BrickCube / 512 | 30.1857 | 29.9645 | 0.73 | 0.000e+00 | 0.000 | decrease_observed |
| frustum_culling | Mountain / 256 | 5.3524 | 5.3020 | 0.94 | 0.000e+00 | 0.000 | overlapping_repeat_ranges |
| frustum_culling | Mountain / 512 | 24.4945 | 24.6196 | -0.51 | 0.000e+00 | 0.000 | overlapping_repeat_ranges |

## G8

| Comparison | Scene / resolution | Base | Candidate | Reduction % | State relative L1 | Image MAE (0–255) | Repeat ranges |
|---|---|---:|---:|---:|---:|---:|---|
| gpu_validation_elision | BrickCube / 256 | 12.2064 | 12.1360 | 0.58 | 0.000e+00 | 0.000 | overlapping_repeat_ranges |
| gpu_validation_elision | BrickCube / 512 | 56.4205 | 53.9425 | 4.39 | 0.000e+00 | 0.000 | decrease_observed |
| gpu_validation_elision | Mountain / 256 | 14.4080 | 13.5405 | 6.02 | 0.000e+00 | 0.000 | decrease_observed |
| gpu_validation_elision | Mountain / 512 | 67.9949 | 64.9436 | 4.49 | 0.000e+00 | 0.000 | decrease_observed |
| compile_time_solver | BrickCube / 256 | 4.0110 | 4.7980 | -19.62 | 0.000e+00 | 0.000 | increase_observed |
| compile_time_solver | BrickCube / 512 | 21.3100 | 23.8765 | -12.04 | 0.000e+00 | 0.000 | increase_observed |
| compile_time_solver | Mountain / 256 | 7.7021 | 8.2063 | -6.55 | 0.000e+00 | 0.000 | increase_observed |
| compile_time_solver | Mountain / 512 | 37.0827 | 40.3115 | -8.71 | 0.000e+00 | 0.000 | increase_observed |

