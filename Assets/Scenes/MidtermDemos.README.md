# Midterm presentation: three scenes

Load a scene from the **Midterm demos** buttons in the Scene window. Each scene plays automatically. Press **F9** to reset its surface state and replay; press **F10** for a clean presentation view. The animations are deterministic and do not loop.

1. **Rain / Materials** (`MidtermRainMaterials.Scene`, 12 s): identical timed rain on a marked asphalt road and dark soil. Rain ends at 4.75 s. The road keeps a reflective, dark surface film; soil rain enters the `absorbedwetness` state and leaves a matte dark stain. Cloud cover fades after the rain. The rain streaks and cloud are an ImGui weather overlay; the `contactEvents` cause the actual state input.
2. **Dustpan / Gravity** (`MidtermDustpanGravity.Scene`, 10 s): water starts at the raised open end of a single-piece metal dustpan. It runs along the tray into the shallow rear trough. Input stops, then the pan tilts once and stays tilted so the remaining thin film changes direction. The tray and rear wall share a connected simulation surface; there are no scripted transfer droplets or deep water volume.
3. **Water / Mud** (`MidtermWaterMud.Scene`, 12 s): two instances of the same fractured stone surface receive equal-duration input. The narrow, jagged, deep cracks retain mud while water moves down and decays. This covers residue without a fourth scene dedicated to dynamic geometry.

The soil appearance uses a distinct `absorbedwetness` surface state. The current engine does not track underground water volume. The fractured stone is an open upper surface, shown from above, so its UV simulation domain remains continuous.

Assets: [Poly Haven Road Damaged Clean](https://polyhaven.com/a/road_damaged_clean), [Farm Soil](https://polyhaven.com/a/farm_soil), and [Dustpan](https://polyhaven.com/a/dustpan), all CC0; the fractured slab is generated here and reuses the project's existing fountain stone albedo. Run `Scripts/build_midterm_demos.py` with Python 3 to rebuild the scene files and generated assets.
