# Material-Dependent Surface State Engine

A Vulkan-based engine for material-dependent surface state dynamics, developed as a 9-credit individual project at Konkuk University under the supervision of Prof. Hyungseok Kim.

The project focuses on representing surface state changes in UV-space and updating them through a common simulation framework rather than implementing each visual effect independently.

## Overview

- Vulkan 1.2 based custom rendering engine
- UV-space texel-based surface state simulation
- Material-dependent surface response
- Geometry-aware state transport
- Heat, Ice, WaterFilm, Mud, and Lava appearance
- Data-driven local State conversion, generation, consumption, and interactions
- Compute shader based state update
- Raycast-based state injection
- ImGui-based debug and simulation controls

## Editor and demos

Run `./run.ps1` on Windows. The editor opens Heated Water Channel and lists the demos in **Scene & Camera**. Use **Simulation → Assets** to select a scene asset and edit its settings. **Apply & Restart** runs the simulation with the new settings, and **Restore Defaults** resets them for the current scene.

For Visual Studio 2026 builds through CMake Tools, select the `windows-msvc-x64` configure preset and its matching Debug or Release build preset. The preset requires CMake 4.2 or newer. `./Scripts/run.sh` launches an existing POSIX build.

Under **Layout**, **Layer & Substrate** pairs PBR with substrate states on the top row, and layer states with layer thickness below. **Elevation & Grids** pairs PBR with elevation contours on the top row, and simulation cells with geometry samples below. The heatmap legends use the names from the state assets. The world grid and axes are off by default.

Choose **Layer Thickness [mm]** or **Areal Mass [kg/m²]**, then pick a compatible **Source** in **Render Options**. Each viewport has its own source. The legend shows its name and color range.

**Snow on Fabric** has a flat bed. **Snow on Fabric_Ruffled** appears next to it in **Demos** and uses the same inputs and surface profile with a more pronounced relief. To capture or check either scene from the command line, pass its `.Scene` file with `--scene PATH`.

Bundled `Base_Variant.Scene` files appear next to their base scene and share its category. An unfiltered `--check-surface --surface-output DIRECTORY` also runs the bundled variants. The [variant comparisons](Docs/Agent-Vault/05_Experiments/04_Surface-Simulation-Validation.md#기본-씬과-변형-8쌍) record the inputs and observed differences.

Under **Simulation → Input**, enable **Inject water with Space** and aim at the surface. The cursor outlines the receiving region. Each press adds the configured volume once. Disable **Run scheduled inlet** to observe a manual dose on its own.

```powershell
& ./run.ps1 -AppArgs @('--scene', 'Assets/Scenes/SnowOnFabric.Scene')
& ./Scripts/Validation/verify_reactions.ps1
```

The reaction check compares the CPU and Vulkan implementations for the current textile demo and exports JSON results.

## Project Status

Currently under development...

## Tech Stack

- C++
- Vulkan 1.2
- GLSL / SPIR-V
- ImGui
- OBJ / MTL
- JSON-based custom asset formats

## Academic Information

This repository is an individual project developed as part of the **Self-Designed Semester Program at Konkuk University**.

- **Credits:** 9
- **Institution:** Konkuk University
- **Project Type:** Individual Project
- **Supervisor:** Prof. Hyungseok Kim

## License

TBD
