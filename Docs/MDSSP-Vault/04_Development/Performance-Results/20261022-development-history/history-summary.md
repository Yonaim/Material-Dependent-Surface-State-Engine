# Development snapshot measurements

Whole GPU command-buffer elapsed time, ms. Values are median of three run medians. No isolated improvement percentages.

| Snapshot | Scene | Resolution | Whole GPU median / p95 | Simulation median | Render median | State relative L1 difference vs V4 |
|---|---|---:|---:|---:|---:|---:|
| V0 | BrickCube | 256 | 101.814 / 112.372 | 86.151 | 15.670 | 2.016786763796655 |
| V1 | BrickCube | 256 | 55.658 / 58.392 | 47.830 | 8.198 | 2.0167867478652917 |
| V3 | BrickCube | 256 | 11.262 / 11.983 | 4.679 | 6.595 | 0.33903081079308905 |
| V4 | BrickCube | 256 | 12.119 / 12.957 | 4.783 | 7.387 | 0.0 |
| V0 | BrickCube | 512 | 338.727 / 362.891 | 271.782 | 61.029 | 2.0356673513554124 |
| V1 | BrickCube | 512 | 172.353 / 195.446 | 140.656 | 31.405 | 2.035667328355505 |
| V3 | BrickCube | 512 | 51.368 / 54.547 | 22.937 | 28.409 | 0.2696883078223639 |
| V4 | BrickCube | 512 | 53.629 / 56.746 | 23.505 | 29.974 | 0.0 |
| V0 | Mountain | 256 | 54.064 / 60.980 | 45.113 | 8.868 | 2.004197476284589 |
| V1 | Mountain | 256 | 30.372 / 32.564 | 26.034 | 4.302 | 2.0041974108104252 |
| V3 | Mountain | 256 | 12.523 / 13.148 | 8.039 | 4.477 | 0.2527714544425147 |
| V4 | Mountain | 256 | 13.305 / 13.963 | 8.126 | 5.232 | 0.0 |
| V0 | Mountain | 512 | 193.721 / 209.459 | 152.371 | 41.224 | 2.0081275731546375 |
| V1 | Mountain | 512 | 91.169 / 94.386 | 72.749 | 18.417 | 2.008127361261348 |
| V3 | Mountain | 512 | 61.714 / 63.595 | 40.260 | 21.379 | 0.40796078770971217 |
| V4 | Mountain | 512 | 64.548 / 65.676 | 40.077 | 24.494 | 0.0 |

V2: OS Application Control blocked the executable; not measured.
