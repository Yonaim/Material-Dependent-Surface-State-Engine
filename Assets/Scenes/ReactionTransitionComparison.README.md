# Transition / Reaction comparison

Load `ReactionTransitionComparison.Scene` from the **Transition / Reaction** button in the Scene panel, or run:

```powershell
& ./Build/bin/Debug/MDSS.exe --scene Assets/Scenes/ReactionTransitionComparison.Scene
```

The 12-second clip plays automatically. Press **F9** to reset all three plates and replay; **F10** shows the presentation view. The plates use the same brown surface, initial Fuel patch, Heat pulse at 1.0 s, State parameters, and camera. Burn and Scorched are generated only by rules.

Each plate's top face is the only simulated surface. Its five side faces use a separate render-only mesh, so the visual cube shape is retained without scheduling solver work for unseen side State. The three top faces keep the original MarbleCube UVs and material. This scene uses 49,152 simulation texels at resolution 128; the original six-surface setup used 294,912.

When this scene is launched directly, the app enables **Sparse Solver Workgroup Scheduling** for its local contacts. The Performance tab shows the option. PERF-045 measured the preceding Ice/Lava revision of this scene; its timing does not measure this Burn/Scorched revision. Benchmark command-line settings can explicitly override this startup choice.

| Plate | Profile | Expected change |
| --- | --- | --- |
| NO RULE | v5, empty `reactions` | Fuel stays unchanged. The Heat pulse fades by decay; no Burn or Scorched appears. |
| TRANSITION | v5 `heat_to_burn`, then `burn_to_scorched` | Each rule consumes one State and produces one State. Heat becomes temporary Burn, then persistent Scorched. Fuel stays unchanged. |
| REACTION | v5 `fuel_heat_to_burn`, then `burn_to_scorched` | The first rule consumes Fuel and Heat together to create Burn. The second consumes Burn to create persistent Scorched. Limited Fuel or Heat limits burning. |

Transition is a one-to-one subtype of Reaction here; the right plate demonstrates a multi-input Reaction. Both use the v5 `reactions` format and explicit `consumes`/`produces`. The older v3/v4 `transitions` format has different, non-consuming compatibility semantics and is not used in this scene. The coefficients are visual demonstration values, not calibrated combustion chemistry.

Run `python Scripts/build_reaction_transition_demo.py` from the repository root to regenerate the Scene, animation, three profiles and maps, split plate meshes, and label mesh. The original `IceReactionLab.Scene` remains the detailed finite/sustained-heating and cooling example used by reaction validation.
