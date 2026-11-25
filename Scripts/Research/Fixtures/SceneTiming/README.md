# Earlier scene timing inputs

These SurfaceScene v1 fixtures reproduce the timing inputs that were previously in the scene picker. The shipped families share one schedule for each of the camera, automatic tilt, inlet, and heat source.

| Fixture | Timing input |
|---|---|
| `CanyonCascade_FastPour.Scene` | Pour the same total water in 0.5 s. |
| `CharringWood_PreWetted.Scene` | Apply the 2.4 g prewet dose over the first 0.1 s. |
| `GlacierCut_ShortHeatPulse.Scene` | Fade the contact heat over 1.2 s. |
| `KilnSpillway.Scene` and `KilnSpillway_LowTilt.Scene` | Compare the original 18° and 4.6° tilt inputs. |

Open a fixture using `--scene` or File → Open Scene. It uses the current solver and renderer. The [validation record](../../../../Docs/Agent-Vault/05_Experiments/04_Surface-Simulation-Validation.md) preserves earlier results.
