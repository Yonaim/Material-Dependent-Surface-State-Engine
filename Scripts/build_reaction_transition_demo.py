"""Build a side-by-side consuming Transition and multi-input Reaction demo."""

# ============================================================
# 00. Imports and Module Configuration
# ============================================================
# region 00_Imports_and_Module_Configuration

from __future__ import annotations

import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCENES = ROOT / "Assets/Scenes"
ANIMATIONS = ROOT / "Assets/Animations"
PROFILES = ROOT / "Assets/SurfaceProfiles"
MAPS = ROOT / "Assets/SurfaceProfileMaps"
MESHES = ROOT / "Assets/Meshes/ReactionLab"
PLATE_MESHES = ROOT / "Assets/Meshes/MarbleCube"


# endregion

# ============================================================
# 01. Processing Helpers
# ============================================================
# region 01_Processing_Helpers


def save(path: Path, value: dict) -> None:
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


def state(capacity: float, spread: float = 0.0, flow: float = 0.0) -> dict:
    return {
        "stateCapacity": capacity,
        "inputFactor": 1.0,
        "saturationSpreadFactor": spread,
        "gravityFlowFactor": flow,
        "decayRate": 0.0,
        "cavityDecayProtectionFactor": 0.0,
        "cavityExitResistanceFactor": 0.0,
        "accumulationFactor": 0.0,
        "cavityFillFactor": 0.0,
        "thicknessPerAmount": 0.035,
    }


STATES = {
    "fuel": state(4.0),
    "heat": state(2.0),
    "burn": state(2.0),
    "scorched": state(2.0),
}
STATES["heat"]["decayRate"] = 0.12

PROFILE_NAMES = {
    "control": "ReactionTransitionControl",
    "transition": "ReactionTransitionConversion",
    "reaction": "ReactionTransitionConsume",
}


def build_profiles() -> None:
    for kind, name in PROFILE_NAMES.items():
        profile = {
            "type": "SurfaceResponseProfile",
            "version": 5,
            "name": name,
            "states": STATES,
        }
        if kind == "transition":
            profile["reactions"] = [
                {
                    "id": "heat_to_burn",
                    "consumes": {"heat": 1.0},
                    "produces": {"burn": 1.0},
                    "rate": {"model": "stateProportional", "state": "heat", "coefficient": 0.8},
                },
                {
                    "id": "burn_to_scorched",
                    "consumes": {"burn": 1.0},
                    "produces": {"scorched": 1.0},
                    "rate": {"model": "stateProportional", "state": "burn", "coefficient": 0.65},
                },
            ]
        elif kind == "control":
            profile["reactions"] = []
        else:
            profile["reactions"] = [
                {
                    "id": "fuel_heat_to_burn",
                    "consumes": {"fuel": 1.0, "heat": 0.35},
                    "produces": {"burn": 1.0},
                    "rate": {"model": "stateProportional", "state": "fuel", "coefficient": 0.7},
                    "modifiers": [
                        {"state": "heat", "function": "rampAbove", "threshold": 0.1, "scale": 1.0}
                    ],
                },
                {
                    "id": "burn_to_scorched",
                    "consumes": {"burn": 1.0},
                    "produces": {"scorched": 1.0},
                    "rate": {"model": "stateProportional", "state": "burn", "coefficient": 0.65},
                },
            ]
        save(PROFILES / f"{name}.SRProfile", profile)
        save(
            MAPS / f"{name}.SurfaceProfileMap",
            {
                "type": "SurfaceProfileMap",
                "version": 1,
                "profiles": [f"../SurfaceProfiles/{name}.SRProfile"],
                "surfaces": [{"surfaceId": 0, "profileIndex": 0}],
            },
        )


GLYPHS = {
    " ": ["000", "000", "000", "000", "000"],
    "A": ["010", "101", "111", "101", "101"],
    "B": ["110", "101", "110", "101", "110"],
    "C": ["011", "100", "100", "100", "011"],
    "E": ["111", "100", "110", "100", "111"],
    "F": ["111", "100", "110", "100", "100"],
    "H": ["101", "101", "111", "101", "101"],
    "I": ["111", "010", "010", "010", "111"],
    "L": ["100", "100", "100", "100", "111"],
    "M": ["101", "111", "111", "101", "101"],
    "N": ["101", "111", "111", "111", "101"],
    "O": ["111", "101", "101", "101", "111"],
    "R": ["110", "101", "110", "101", "101"],
    "S": ["011", "100", "010", "001", "110"],
    "T": ["111", "010", "010", "010", "010"],
    "U": ["101", "101", "101", "101", "111"],
    "Y": ["101", "101", "010", "010", "010"],
}


def build_labels() -> None:
    lines = ["mtllib Lab.mtl", "o comparison_labels", "usemtl lettering"]
    vertex = 1
    pixel = 0.036
    pitch = 0.038
    for center, title, subtitle in [
        (-2.05, "NO RULE", "HEAT ONLY"),
        (0.0, "TRANSITION", "HEAT BURNS"),
        (2.05, "REACTION", "FUEL BURNS"),
    ]:
        for label, baseline in [(title, 0.07), (subtitle, -0.18)]:
            width = (len(label) * 4 - 1) * pitch
            left = center - width / 2
            for column, char in enumerate(label):
                for row, bits in enumerate(GLYPHS[char]):
                    for bit, on in enumerate(bits):
                        if on != "1":
                            continue
                        x = left + (column * 4 + bit) * pitch
                        y = baseline + (4 - row) * pitch
                        lines.extend(
                            [
                                f"v {x:.5f} {y:.5f} 0",
                                f"v {x + pixel:.5f} {y:.5f} 0",
                                f"v {x + pixel:.5f} {y + pixel:.5f} 0",
                                f"v {x:.5f} {y + pixel:.5f} 0",
                                f"f {vertex} {vertex + 1} {vertex + 2}",
                                f"f {vertex} {vertex + 2} {vertex + 3}",
                            ]
                        )
                        vertex += 4
    (MESHES / "ReactionTransitionLabels.obj").write_text("\n".join(lines) + "\n", encoding="utf-8")


def build_plate_meshes() -> None:
    source = (PLATE_MESHES / "MarbleCube.obj").read_text(encoding="utf-8")
    top = source.index("usemtl marble\n")
    sides = source.index("usemtl marble_back\n")
    preamble = source[:top].replace("mtllib MarbleCube.mtl", "mtllib ReactionTransitionPlate.mtl")
    (PLATE_MESHES / "ReactionTransitionPlate.mtl").write_text(
        "\n".join(
            f"newmtl {name}\nKd 0.52 0.31 0.16\nNs 35"
            for name in (
                "marble",
                "marble_back",
                "marble_right",
                "marble_left",
                "marble_top",
                "marble_bottom",
            )
        )
        + "\n",
        encoding="utf-8",
    )
    (PLATE_MESHES / "ReactionTransitionPlateTop.obj").write_text(
        preamble + source[top:sides], encoding="utf-8"
    )
    (PLATE_MESHES / "ReactionTransitionPlateSides.obj").write_text(
        preamble + source[sides:], encoding="utf-8"
    )


def build_scene() -> None:
    objects = []
    contacts = []
    for x, kind in [(-2.05, "control"), (0.0, "transition"), (2.05, "reaction")]:
        transform = {
            "position": [x, 0, 0],
            "rotationDegrees": [8, 0, 0],
            "scale": [1.65, 2.2, 0.22],
        }
        objects.append(
            {
                "id": kind,
                "mesh": "../Meshes/MarbleCube/ReactionTransitionPlateTop.obj",
                "surfaceProfileMap": f"../SurfaceProfileMaps/{PROFILE_NAMES[kind]}.SurfaceProfileMap",
                "transform": transform,
            }
        )
        objects.append(
            {
                "id": kind + "_sides",
                "mesh": "../Meshes/MarbleCube/ReactionTransitionPlateSides.obj",
                "transform": transform,
            }
        )
        contacts.append(
            {
                "target": kind,
                "state": "fuel",
                "worldPosition": [x, 0.23225797607978538, 0.1437227628015891],
                "radius": 0.7,
                "strength": 2.0,
                "falloff": 0.25,
            }
        )
    objects.extend(
        [
            {
                "id": "labels",
                "mesh": "../Meshes/ReactionLab/ReactionTransitionLabels.obj",
                "transform": {
                    "position": [0, -1.42, -0.02],
                    "rotationDegrees": [0, 0, 0],
                    "scale": [1, 1, 1],
                },
            },
            {
                "id": "stage",
                "mesh": "../Meshes/ReactionLab/Stage.obj",
                "transform": {
                    "position": [0, 0, -0.29],
                    "rotationDegrees": [0, 0, 0],
                    "scale": [7, 3.65, 0.18],
                },
            },
        ]
    )
    save(
        SCENES / "ReactionTransitionComparison.Scene",
        {
            "type": "TScene",
            "version": 1,
            "animation": "../Animations/ReactionTransitionComparison.DemoAnim",
            "camera": {
                "position": [0.8, -6.9, 5.9],
                "target": [0, 0, 0.12],
                "verticalFieldOfViewDegrees": 40,
            },
            "simulationResolution": 128,
            "litHeightDisplayScale": 1,
            "objects": objects,
            "initialContacts": contacts,
        },
    )
    save(
        ANIMATIONS / "ReactionTransitionComparison.DemoAnim",
        {
            "version": 1,
            "durationSeconds": 12,
            "loop": False,
            "autoplay": True,
            "tracks": [
                {
                    "target": "camera",
                    "property": "position",
                    "interpolation": "linear",
                    "keys": [
                        {"time": 0, "value": [0.8, -6.9, 5.9]},
                        {"time": 12, "value": [0.8, -6.9, 5.9]},
                    ],
                }
            ],
            "contactEvents": [
                {
                    "time": 1.0,
                    "target": kind,
                    "state": "heat",
                    "operation": "add",
                    "shape": "ellipse",
                    "localPosition": [0, 0.12, 0.5],
                    "radius": 0.65,
                    "secondaryRadius": 0.75,
                    "depth": 0.05,
                    "strength": 1.4,
                    "falloff": 0.3,
                }
                for kind in ("control", "transition", "reaction")
            ],
        },
    )


# endregion

# ============================================================
# 02. Script Execution
# ============================================================
# region 02_Script_Execution

if __name__ == "__main__":
    build_profiles()
    build_labels()
    build_plate_meshes()
    build_scene()

# endregion
