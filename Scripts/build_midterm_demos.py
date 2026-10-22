"""Build the three midterm demo meshes and CC0 texture assets.

Run with Python 3. The dustpan glTF and ground textures come from Poly Haven.
"""

import json
import math
import shutil
import struct
import urllib.request
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MESHES = ROOT / "Assets" / "Meshes"
USER_AGENT = {"User-Agent": "MDSS-Engine midterm demo asset builder"}


def download(url, path):
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists():
        return
    with urllib.request.urlopen(urllib.request.Request(url, headers=USER_AGENT)) as response:
        path.write_bytes(response.read())


def texture(asset, kind, output):
    if output.exists():
        return
    request = urllib.request.Request(f"https://api.polyhaven.com/files/{asset}", headers=USER_AGENT)
    with urllib.request.urlopen(request) as response:
        files = json.load(response)
    download(files[kind]["1k"]["jpg"]["url"], output)


def write_grid(name, material, height, nx=64, ny=64):
    folder = MESHES / "MidtermGround"
    folder.mkdir(parents=True, exist_ok=True)
    lines = [f"mtllib {name}.mtl", f"o {name}"]
    vertices = []
    for j in range(ny + 1):
        y = (j / ny - 0.5) * 1.8
        for i in range(nx + 1):
            x = (i / nx - 0.5) * 1.7
            z = height(x, y)
            vertices.append((x, y, z))
            lines.append(f"v {x:.6f} {y:.6f} {z:.6f}")
    for j in range(ny + 1):
        for i in range(nx + 1):
            lines.append(f"vt {0.02 + 0.96 * i / nx:.6f} {0.02 + 0.96 * j / ny:.6f}")
    for j in range(ny + 1):
        for i in range(nx + 1):
            x, y, _ = vertices[j * (nx + 1) + i]
            eps = 0.0001
            dx = (height(x + eps, y) - height(x - eps, y)) / (2 * eps)
            dy = (height(x, y + eps) - height(x, y - eps)) / (2 * eps)
            length = math.sqrt(dx * dx + dy * dy + 1)
            lines.append(f"vn {-dx / length:.6f} {-dy / length:.6f} {1 / length:.6f}")
    lines.append(f"usemtl {material}")
    for j in range(ny):
        for i in range(nx):
            a = j * (nx + 1) + i + 1
            b, c, d = a + 1, a + nx + 1, a + nx + 2
            lines.append(f"f {a}/{a}/{a} {b}/{b}/{b} {d}/{d}/{d}")
            lines.append(f"f {a}/{a}/{a} {d}/{d}/{d} {c}/{c}/{c}")
    (folder / f"{name}.obj").write_text("\n".join(lines) + "\n")
    return folder


def build_ground():
    folder = MESHES / "MidtermGround"
    texture("road_damaged_clean", "Diffuse", folder / "Road.jpg")
    texture("road_damaged_clean", "nor_dx", folder / "RoadNormal.jpg")
    texture("farm_soil", "Diffuse", folder / "Soil.jpg")
    for name, image in (("Road", "Road.jpg"), ("Soil", "Soil.jpg")):
        extra = ("norm RoadNormal.jpg\nmdss_water_style film\n"
                 "mdss_water_opacity 2.2\nmdss_water_darkening 0.60\n") if name == "Road" else ""
        (folder / f"{name}.mtl").write_text(f"newmtl {name}\nKd 1 1 1\nd 1\nmap_Kd {image}\n{extra}")
        write_grid(name, name, lambda x, y: 0.02 + 0.025 * y)
    # A narrow painted center line gives the asphalt unmistakable road context.
    lines = ["mtllib RoadMarks.mtl", "o RoadMarks"]
    vertices = []
    for y0, y1 in ((-0.72, -0.40), (-0.22, 0.10), (0.28, 0.60)):
        for x, y in ((-0.02, y0), (0.02, y0), (0.02, y1), (-0.02, y1)):
            vertices.append((x, y, 0.027 + 0.025 * y))
    lines += [f"v {x:.6f} {y:.6f} {z:.6f}" for x, y, z in vertices]
    lines += ["vt 0 0", "vt 1 0", "vt 1 1", "vt 0 1", "vn 0 0 1", "usemtl RoadMarks"]
    for i in range(0, len(vertices), 4):
        a, b, c, d = (i + 1, i + 2, i + 3, i + 4)
        lines += [f"f {a}/1/1 {b}/2/1 {c}/3/1", f"f {a}/1/1 {c}/3/1 {d}/4/1"]
    (folder / "RoadMarks.obj").write_text("\n".join(lines) + "\n")
    (folder / "RoadMarks.mtl").write_text("newmtl RoadMarks\nKd 0.85 0.83 0.70\nd 1\n")


def build_fractured_stone():
    folder = MESHES / "MidtermGround"
    # Jagged, narrow, deep joints between broad stone faces. The valley bottoms
    # stay connected so the simulator can represent material lodged in the cracks.
    def height(x, y):
        left = -0.30 + 0.045 * math.sin(7.0 * y) + 0.017 * math.sin(21.0 * y)
        right = 0.29 + 0.048 * math.sin(6.2 * y + 1.2) + 0.014 * math.sin(24.0 * y)
        crevice = sum(0.19 * math.exp(-((x - center) / 0.068) ** 4)
                      for center in (left, right))
        stone_variation = 0.016 * math.sin(4.7 * x + 2.0 * y) * math.sin(5.1 * y)
        return 0.24 + 0.10 * y + stone_variation - crevice

    write_grid("FracturedStone", "FracturedStone", height, 112, 112)
    source = MESHES / "LionFountain" / "FountainStoneAlbedo.jpg"
    shutil.copyfile(source, folder / "FracturedStone.jpg")
    (folder / "FracturedStone.mtl").write_text(
        "newmtl FracturedStone\nKd 1 1 1\nd 1\nmap_Kd FracturedStone.jpg\n"
        "mdss_water_opacity 1.5\nmdss_water_darkening 0.42\n")


def build_dustpan():
    """Import the one-piece metal dustpan, preserving shared positions at UV seams."""
    folder = MESHES / "MidtermDustpan"
    source = folder / "Source"
    gltf_path = source / "dustpan_1k.gltf"
    if not gltf_path.exists() or not (source / "dustpan.bin").exists():
        request = urllib.request.Request("https://api.polyhaven.com/files/dustpan", headers=USER_AGENT)
        with urllib.request.urlopen(request) as response:
            entry = json.load(response)["gltf"]["1k"]["gltf"]
        download(entry["url"], gltf_path)
        for relative_path, included in entry["include"].items():
            download(included["url"], source / relative_path)

    texture("dustpan", "Diffuse", folder / "Dustpan.jpg")
    texture("dustpan", "nor_dx", folder / "DustpanNormal.jpg")
    (folder / "Dustpan.mtl").write_text(
        "newmtl Dustpan\nKd 1 1 1\nd 1\nmap_Kd Dustpan.jpg\n"
        "norm DustpanNormal.jpg\nmdss_water_style film\n"
        "mdss_water_opacity 2.2\nmdss_water_darkening 0.55\n")

    gltf = json.loads(gltf_path.read_text())
    binary = (source / gltf["buffers"][0]["uri"]).read_bytes()

    def accessor(index):
        a = gltf["accessors"][index]
        view = gltf["bufferViews"][a["bufferView"]]
        fmt = {5126: "f", 5123: "H", 5125: "I"}[a["componentType"]]
        count = {"SCALAR": 1, "VEC2": 2, "VEC3": 3}[a["type"]]
        size = struct.calcsize("<" + fmt * count)
        start = view.get("byteOffset", 0) + a.get("byteOffset", 0)
        stride = view.get("byteStride", size)
        return [struct.unpack_from("<" + fmt * count, binary, start + i * stride)
                for i in range(a["count"])]

    primitive = gltf["meshes"][0]["primitives"][0]
    positions = accessor(primitive["attributes"]["POSITION"])
    normals = accessor(primitive["attributes"]["NORMAL"])
    uvs = accessor(primitive["attributes"]["TEXCOORD_0"])
    indices = [entry[0] + 1 for entry in accessor(primitive["indices"])]
    lines = ["mtllib Dustpan.mtl", "o Dustpan"]
    unique_positions = {}
    position_indices = []
    for x, y, z in positions:
        # Poly Haven is Y-up; the engine is Z-up. Reuse positions across UV seams.
        key = (f"{x:.7f}", f"{-z:.7f}", f"{y:.7f}")
        if key not in unique_positions:
            unique_positions[key] = len(unique_positions) + 1
            lines.append(f"v {' '.join(key)}")
        position_indices.append(unique_positions[key])
    for u, v in uvs:
        lines.append(f"vt {u:.7f} {1-v:.7f}")
    for x, y, z in normals:
        lines.append(f"vn {x:.7f} {-z:.7f} {y:.7f}")
    lines.append("usemtl Dustpan")
    for i in range(0, len(indices), 3):
        a, b, c = indices[i:i + 3]
        lines.append(f"f {position_indices[a-1]}/{a}/{a} {position_indices[b-1]}/{b}/{b} {position_indices[c-1]}/{c}/{c}")
    (folder / "Dustpan.obj").write_text("\n".join(lines) + "\n")


def save_json(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n")


def water_profile(decay, gravity=0.34, spread=0.22):
    return {"stateCapacity": 1.0, "inputFactor": 1.0,
            "saturationSpreadFactor": spread, "gravityFlowFactor": gravity,
            "decayRate": decay, "cavityDecayProtectionFactor": 0.65,
            "cavityExitResistanceFactor": 0.85, "accumulationFactor": 0.08,
            "cavityFillFactor": 0.8, "thicknessPerAmount": 0.055}


def profile(name, states):
    save_json(ROOT / "Assets/SurfaceProfiles" / f"Midterm{name}.SRProfile",
              {"type": "SurfaceResponseProfile", "version": 4,
               "name": f"Midterm{name}", "states": states, "transitions": []})
    save_json(ROOT / "Assets/SurfaceProfileMaps" / f"Midterm{name}.SurfaceProfileMap",
              {"type": "SurfaceProfileMap", "version": 1,
               "profiles": [f"../SurfaceProfiles/Midterm{name}.SRProfile"],
               "surfaces": [{"surfaceId": 0, "profileIndex": 0}]})


def scene(name, camera, objects, duration, tracks, contacts, weather=None):
    title = "Midterm" + name
    save_json(ROOT / "Assets/Scenes" / f"{title}.Scene",
              {"type": "TScene", "version": 1,
               "animation": f"../Animations/{title}.DemoAnim",
               "camera": {"position": camera[0], "target": camera[1],
                          "verticalFieldOfViewDegrees": 46.0},
               "simulationResolution": 256, "litHeightDisplayScale": 1.4,
               "objects": objects})
    animation = {"version": 1, "durationSeconds": duration, "loop": False,
                 "autoplay": True, "tracks": tracks,
                 "contactEvents": sorted(contacts, key=lambda event: event["time"])}
    if weather:
        animation["weather"] = weather
    save_json(ROOT / "Assets/Animations" / f"{title}.DemoAnim", animation)


def obj(identifier, mesh, profile_name, position):
    return {"id": identifier, "mesh": f"../Meshes/{mesh}",
            "surfaceProfileMap": f"../SurfaceProfileMaps/Midterm{profile_name}.SurfaceProfileMap",
            "transform": {"position": position, "rotationDegrees": [0, 0, 0], "scale": [1, 1, 1]}}


def camera_track(position, target, duration):
    return [{"target": "camera", "property": "position", "keys":
             [{"time": 0, "value": position}, {"time": duration, "value": position}]},
            {"target": "camera", "property": "target", "keys":
             [{"time": 0, "value": target}, {"time": duration, "value": target}]}]


def contact(time, target, state, position, radius, strength, depth=0.14):
    return {"time": round(time, 3), "target": target, "state": state,
            "operation": "add", "shape": "circle", "localPosition": position,
            "radius": radius, "depth": depth, "strength": strength, "falloff": 0.7}


def build_scenes():
    profile("Road", {"waterfilm": water_profile(0.015, 0.36, 0.23)})
    # Rain infiltrates the soil on impact, leaving a persistent, matte wet stain.
    profile("Soil", {"absorbedwetness": water_profile(0.035, 0.0, 0.10)})
    profile("Dustpan", {"waterfilm": water_profile(0.018, 0.82, 0.16)})
    profile("Fractured", {"waterfilm": water_profile(0.07, 0.48, 0.25),
                        "mud": {"stateCapacity": 2.0, "inputFactor": 1.0,
                                "saturationSpreadFactor": 0.035, "gravityFlowFactor": 0.018,
                                "decayRate": 0.002, "cavityDecayProtectionFactor": 0.92,
                                "cavityExitResistanceFactor": 0.95, "accumulationFactor": 0.4,
                                "cavityFillFactor": 0.8, "thicknessPerAmount": 0.18}})

    ground_camera = ([0, -3.6, 3.1], [0, 0, 0.02])
    rain = []
    drops = [(-0.43, 0.4), (0.38, 0.45), (-0.3, -0.3), (0.43, -0.38),
             (0.02, 0.1), (0.15, -0.55)]
    for step in range(19):
        t = 0.55 + step * 0.22
        x, y = drops[step % len(drops)]
        rain.append(contact(t, "road", "waterfilm", [x, y, 0.02 + 0.025 * y], 0.38, 0.3))
        rain.append(contact(t, "soil", "absorbedwetness", [x, y, 0.02 + 0.025 * y], 0.38, 0.3))
    scene("RainMaterials", ground_camera,
          [obj("road", "MidtermGround/Road.obj", "Road", [-0.99, 0, 0]),
           {"id": "road_marks", "mesh": "../Meshes/MidtermGround/RoadMarks.obj",
            "transform": {"position": [-0.99, 0, 0], "rotationDegrees": [0, 0, 0], "scale": [1, 1, 1]}},
           obj("soil", "MidtermGround/Soil.obj", "Soil", [0.99, 0, 0])],
          12.0, camera_track(*ground_camera, 12.0), rain,
          {"rainStartSeconds": 0.5, "rainEndSeconds": 4.75,
           "cloudEndSeconds": 7.5, "intensity": 0.75})

    pan_camera = ([1.25, -2.45, 1.50], [0, 0, 0.24])
    # Put the water on the open, initially high end of the pan. Its actual
    # connected metal surface carries the state toward the shallow rear trough.
    pan_contacts = [contact(0.60 + i * 0.32, "dustpan", "waterfilm",
                            [(i % 3 - 1) * 0.018, -0.126, 0.003], 0.17, 0.32, 0.085)
                    for i in range(11)]
    def x_quat(degrees):
        a = math.radians(degrees) * 0.5
        return [math.sin(a), 0, 0, math.cos(a)]
    pan_tracks = camera_track(*pan_camera, 10.0)
    pan_tracks.append({"target": "dustpan", "property": "rotation", "interpolation": "slerp",
                       "keys": [{"time": t, "value": x_quat(angle)} for t, angle in
                                ((0, -16), (5.0, -16), (6.0, 17), (10.0, 17))]})
    pan_object = obj("dustpan", "MidtermDustpan/Dustpan.obj", "Dustpan", [0, 0, 0.25])
    pan_object["transform"]["rotationDegrees"] = [-16, 0, 0]
    pan_object["transform"]["scale"] = [3.4, 3.4, 3.4]
    scene("DustpanGravity", pan_camera, [pan_object], 10.0, pan_tracks, pan_contacts)

    stone_camera = ([0, -3.45, 2.7], [0, 0, 0.15])
    stone_contacts = []
    for i in range(13):
        t = 0.55 + i * 0.3
        for target, state in (("water_stone", "waterfilm"), ("mud_stone", "mud")):
            stone_contacts.append(contact(t, target, state, [-0.30, 0.58, 0.10], 0.31, 0.26, 0.20))
    scene("WaterMud", stone_camera,
          [obj("water_stone", "MidtermGround/FracturedStone.obj", "Fractured", [-0.99, 0, 0]),
           obj("mud_stone", "MidtermGround/FracturedStone.obj", "Fractured", [0.99, 0, 0])],
          12.0, camera_track(*stone_camera, 12.0), stone_contacts)


if __name__ == "__main__":
    build_ground()
    build_fractured_stone()
    build_dustpan()
    build_scenes()
    print("Midterm demo mesh and texture assets built.")
