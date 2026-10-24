# Midterm presentation: three scenes

Load a scene from the **Midterm demos** buttons in the Scene window. Each scene plays automatically. Press **F9** to reset its surface state and replay; press **F10** for a clean presentation view. The animations are deterministic and do not loop.

1. **Rain / Materials** (`MidtermRainMaterials.Scene`, 12 s): identical timed rain on a marked dark asphalt road and compacted soil. Rain ends at 4.75 s. The road keeps a reflective, dark surface film; soil rain enters the `absorbedwetness` state and leaves a matte dark stain. Cloud cover fades after the rain. Rain streaks and a smooth cloud band are an ImGui weather overlay; the original localized `contactEvents` cause the actual state input.
2. **Dustpan / Gravity** (`MidtermDustpanGravity.Scene`, 16.5 s): water starts at the raised open end of a single-piece metal dustpan with subdued diffuse color and its scanned normal map. It runs along the tray into the shallow rear trough. Input stops, then the pan tilts from 7.0 to 9.3 s, pauses, and tilts back in the opposite direction from 11.8 to 14.1 s. The tray and rear wall share a connected simulation surface; there are no scripted transfer droplets or deep water volume.
3. **Water / Mud** (`MidtermWaterMud.Scene`, 12 s): one fractured stone surface receives water in its left crack and mud in its right crack. The slab tilts from 4.8 to 6.8 s. Water runs down the inclined left channel while mud remains in the narrow right channel. This covers residue without a fourth scene dedicated to dynamic geometry.

The soil appearance uses a distinct `absorbedwetness` surface state. The current engine does not track underground water volume. The fractured stone is an open upper surface, shown from above, so its UV simulation domain remains continuous.

Assets: [Poly Haven Clean Asphalt](https://polyhaven.com/a/clean_asphalt), [Dirt](https://polyhaven.com/a/dirt), and [Dustpan](https://polyhaven.com/a/dustpan), all CC0; the fractured slab is generated here and reuses the project's existing fountain stone albedo. Run `Scripts/build_midterm_demos.py` with Python 3 to rebuild the scene files and generated assets.
