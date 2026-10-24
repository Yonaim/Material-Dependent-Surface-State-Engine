"""Download the CC0 Poly Haven meshes used by the three showcase scenes.

Run with: uv run --python 3.13 --with trimesh --with scipy --with pillow python Scripts/import_showcase_assets.py
The generated OBJ files keep the source UVs and normals, share position indices
across UV seams, and use the engine's `norm` MTL key.
"""

from __future__ import annotations

import hashlib
import json
import tempfile
import urllib.request
from pathlib import Path

import numpy as np
import trimesh
from PIL import Image
from scipy.ndimage import distance_transform_edt


ROOT = Path(__file__).resolve().parents[1]
MESHES = ROOT / "Assets" / "Meshes"
ASSETS = {
    "lion_head": ("LionFountain", 1.18),
    "marble_bust_01": ("MarbleBust", 1.42),
    "rock_07": ("BasaltAltar", 1.12),
}


def fetch(url: str, destination: Path, md5: str | None = None) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    if not destination.exists() or (md5 and hashlib.md5(destination.read_bytes()).hexdigest() != md5):
        print(f"Downloading {destination.name}", flush=True)
        request = urllib.request.Request(url, headers={"User-Agent": "MDSS-Engine/1.0"})
        with urllib.request.urlopen(request) as response, destination.open("wb") as output:
            while chunk := response.read(1024 * 1024):
                output.write(chunk)
    if md5 and hashlib.md5(destination.read_bytes()).hexdigest() != md5:
        raise RuntimeError(f"Checksum mismatch: {destination}")


def metadata(slug: str) -> dict:
    request = urllib.request.Request(
        f"https://api.polyhaven.com/files/{slug}", headers={"User-Agent": "MDSS-Engine/1.0"}
    )
    with urllib.request.urlopen(request) as response:
        return json.load(response)


def source_gltf(slug: str, temporary: Path, files: dict) -> Path:
    gltf = files["gltf"]["1k"]["gltf"]
    path = temporary / f"{slug}_1k.gltf"
    fetch(gltf["url"], path, gltf["md5"])
    for relative, included in gltf["include"].items():
        fetch(included["url"], temporary / relative, included["md5"])
    return path


def texture(slug: str, channel: str, folder: Path, name: str, size: str = "2k") -> Path:
    files = metadata(slug)
    entry = files[channel][size]["jpg"]
    path = folder / name
    fetch(entry["url"], path, entry["md5"])
    return path


def export_obj(gltf: Path, destination: Path, material: str, target_height: float) -> None:
    scene = trimesh.load(gltf, force="scene", process=False)
    parts = []
    for node in scene.graph.nodes_geometry:
        transform, geometry_name = scene.graph[node]
        part = scene.geometry[geometry_name]
        linear = transform[:3, :3]
        positions = np.asarray(part.vertices) @ linear.T + transform[:3, 3]
        # Capture glTF's authored vertex normals before copying the mesh. Trimesh's
        # scene.dump() discards its cached NORMAL accessor and recomputes them.
        normals = np.asarray(part.vertex_normals) @ np.linalg.inv(linear)
        normals /= np.maximum(np.linalg.norm(normals, axis=1, keepdims=True), 1e-12)
        parts.append((part, positions, normals))
    if not parts:
        raise RuntimeError(f"No geometry in {gltf}")
    # Poly Haven glTF is Y-up. Restore Blender's Z-up, with the model facing -Y.
    rotation = np.array([[1.0, 0.0, 0.0], [0.0, 0.0, -1.0], [0.0, 1.0, 0.0]])
    bounds = np.vstack([positions @ rotation.T for _, positions, _ in parts])
    low, high = bounds.min(axis=0), bounds.max(axis=0)
    scale = target_height / (high[2] - low[2])
    center = (low + high) * 0.5
    print(f"{destination.name}: {len(parts)} parts, bounds {low.round(3)}..{high.round(3)}", flush=True)

    with destination.open("w", encoding="utf-8", newline="\n") as output:
        output.write(f"mtllib {destination.stem}.mtl\no {destination.stem}\nusemtl {material}\n")
        position_offset = 0
        attribute_offset = 0
        for part, source_positions, source_normals in parts:
            vertices = (source_positions @ rotation.T - center) * scale
            vertices[:, 2] += (high[2] - low[2]) * scale * 0.5
            normals = source_normals @ rotation.T
            uv = getattr(part.visual, "uv", None)
            if uv is None or len(uv) != len(vertices):
                raise RuntimeError(f"Missing source UVs in {gltf}")
            # glTF duplicates a vertex at each UV seam. OBJ can give those copies
            # separate texture/normal indices while retaining one position index.
            # SurfaceMappingBuilder uses that position identity for seam topology.
            unique_positions = {}
            position_indices = []
            for x, y, z in vertices:
                key = (f"{x:.7f}", f"{y:.7f}", f"{z:.7f}")
                if key not in unique_positions:
                    unique_positions[key] = position_offset + len(unique_positions) + 1
                    output.write(f"v {' '.join(key)}\n")
                position_indices.append(unique_positions[key])
            for u, v in uv:
                output.write(f"vt {u:.7f} {v:.7f}\n")
            for x, y, z in normals:
                output.write(f"vn {x:.7f} {y:.7f} {z:.7f}\n")
            for face in np.asarray(part.faces, dtype=np.int64):
                corners = [f"{position_indices[i]}/{attribute_offset+i+1}/{attribute_offset+i+1}" for i in face]
                output.write(f"f {' '.join(corners)}\n")
            position_offset += len(unique_positions)
            attribute_offset += len(vertices)


def material(folder: Path, name: str, diffuse: str, normal: str) -> None:
    (folder / f"{name}.mtl").write_text(
        f"newmtl {name}\nKd 1 1 1\nd 1\nmap_Kd {diffuse}\nnorm {normal}\n", encoding="utf-8"
    )


def gltf_vertex_colors(gltf: Path, vertex_count: int) -> np.ndarray | None:
    document = json.loads(gltf.read_text(encoding="utf-8"))
    primitives = [primitive for mesh in document["meshes"] for primitive in mesh["primitives"]]
    if len(primitives) != 1:
        raise RuntimeError(f"Expected one source primitive in {gltf}")
    accessor_index = primitives[0]["attributes"].get("COLOR_0")
    if accessor_index is None:
        return None
    accessor = document["accessors"][accessor_index]
    if accessor["count"] != vertex_count or accessor["type"] not in ("VEC3", "VEC4"):
        raise RuntimeError(f"Unexpected vertex color layout in {gltf}")
    dtype = {5121: np.uint8, 5123: np.uint16, 5126: np.float32}[accessor["componentType"]]
    components = 3 if accessor["type"] == "VEC3" else 4
    view = document["bufferViews"][accessor["bufferView"]]
    buffer = document["buffers"][view["buffer"]]
    data = (gltf.parent / buffer["uri"]).read_bytes()
    offset = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
    stride = view.get("byteStride", components * np.dtype(dtype).itemsize)
    colors = np.ndarray((vertex_count, components), dtype=dtype, buffer=data, offset=offset,
                        strides=(stride, np.dtype(dtype).itemsize)).copy().astype(np.float32)
    if accessor["componentType"] != 5126 and accessor.get("normalized", False):
        colors /= np.iinfo(dtype).max
    return colors[:, :3]


def bake_display_albedo(gltf: Path, source_diffuse: Path, source_ao: Path, destination: Path) -> str:
    scene = trimesh.load(gltf, force="scene", process=False)
    if len(scene.graph.nodes_geometry) != 1:
        raise RuntimeError(f"Expected one geometry node in {gltf}")
    part = scene.geometry[scene.graph[next(iter(scene.graph.nodes_geometry))][1]]
    colors = gltf_vertex_colors(gltf, len(part.vertices))
    # Rock 07 has strongly red/blue vertex paint. Its chroma made the demo much
    # redder than Poly Haven's neutral studio preview. Retain its luminance as
    # source shading information without shifting the source diffuse hue.
    vertex_shading = (colors @ np.array([0.2126, 0.7152, 0.0722], dtype=np.float32)
                      if colors is not None else None)
    base = np.asarray(Image.open(source_diffuse).convert("RGB"), dtype=np.float32) / 255.0
    height, width = base.shape[:2]
    ao = np.asarray(Image.open(source_ao).convert("L"), dtype=np.float32) / 255.0
    if ao.shape != (height, width):
        raise RuntimeError(f"AO size does not match diffuse in {gltf}")
    uv = np.asarray(part.visual.uv)
    pixels = np.column_stack((uv[:, 0] * width, (1.0 - uv[:, 1]) * height))
    factors = np.ones((height, width), dtype=np.float32)
    covered = np.zeros((height, width), dtype=bool)
    for face in np.asarray(part.faces):
        a, b, c = pixels[face]
        x0 = max(0, int(np.floor(min(a[0], b[0], c[0]))))
        x1 = min(width - 1, int(np.ceil(max(a[0], b[0], c[0]))))
        y0 = max(0, int(np.floor(min(a[1], b[1], c[1]))))
        y1 = min(height - 1, int(np.ceil(max(a[1], b[1], c[1]))))
        if x1 < x0 or y1 < y0:
            continue
        area = (b[1] - c[1]) * (a[0] - c[0]) + (c[0] - b[0]) * (a[1] - c[1])
        if abs(area) < 1e-10:
            continue
        xx, yy = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
        wa = ((b[1] - c[1]) * (xx - c[0]) + (c[0] - b[0]) * (yy - c[1])) / area
        wb = ((c[1] - a[1]) * (xx - c[0]) + (a[0] - c[0]) * (yy - c[1])) / area
        wc = 1.0 - wa - wb
        inside = (wa >= -1e-5) & (wb >= -1e-5) & (wc >= -1e-5)
        if not inside.any():
            continue
        if vertex_shading is not None:
            tile = factors[y0:y1 + 1, x0:x1 + 1]
            tile[inside] = (wa * vertex_shading[face[0]] + wb * vertex_shading[face[1]] +
                            wc * vertex_shading[face[2]])[inside]
        covered[y0:y1 + 1, x0:x1 + 1][inside] = True
    if not covered.any():
        raise RuntimeError(f"Display map bake covered no texels in {gltf}")
    nearest = distance_transform_edt(~covered, return_distances=False, return_indices=True)
    factors = factors[tuple(nearest)]
    ao[~covered] = ao[tuple(nearest)][~covered]
    linear = np.where(base <= 0.04045, base / 12.92, ((base + 0.055) / 1.055) ** 2.4)
    linear *= np.clip(factors * ao, 0.0, 1.0)[..., None]
    encoded = np.where(linear <= 0.0031308, linear * 12.92, 1.055 * linear ** (1.0 / 2.4) - 0.055)
    Image.fromarray(np.uint8(np.clip(encoded * 255.0 + 0.5, 0, 255)), "RGB").save(destination)
    print(f"Baked source AO and neutral vertex shading into {destination.name}", flush=True)
    return destination.name


def main() -> None:
    with tempfile.TemporaryDirectory(prefix="mdss-showcase-") as tmp:
        temporary = Path(tmp)
        for slug, (folder_name, target_height) in ASSETS.items():
            folder = MESHES / folder_name
            folder.mkdir(parents=True, exist_ok=True)
            files = metadata(slug)
            gltf = source_gltf(slug, temporary / slug, files)
            export_obj(gltf, folder / f"{folder_name}.obj", folder_name, target_height)
            if slug == "lion_head":
                diffuse = texture(slug, "Diffuse", folder, "LionHeadAlbedo.jpg")
                normal = texture(slug, "nor_dx", folder, "LionHeadNormal.jpg")
                ao = texture(slug, "AO", folder, "LionHeadAO.jpg")
                display = bake_display_albedo(gltf, diffuse, ao, folder / "LionHeadDisplayAlbedo.png")
                material(folder, folder_name, display, normal.name)
                texture("rock_01", "Diffuse", folder, "FountainStoneAlbedo.jpg")
                texture("rock_01", "nor_dx", folder, "FountainStoneNormal.jpg")
            elif slug == "rock_07":
                diffuse = texture(slug, "Diffuse", folder, "Rock07Albedo.jpg")
                normal = texture(slug, "nor_dx", folder, "BasaltNormal.jpg")
                ao = texture(slug, "AO", folder, "Rock07AO.jpg")
                display = bake_display_albedo(gltf, diffuse, ao, folder / "Rock07DisplayAlbedo.png")
                material(folder, folder_name, display, normal.name)
                texture("volcanic_rock_tiles", "Diffuse", folder, "AltarStoneAlbedo.jpg")
                texture("volcanic_rock_tiles", "nor_dx", folder, "AltarStoneNormal.jpg")
            else:
                diffuse = texture(slug, "Diffuse", folder, "MarbleAlbedo.jpg")
                normal = texture(slug, "nor_dx", folder, "MarbleNormal.jpg")
                ao = texture(slug, "AO", folder, "MarbleAO.jpg")
                display = bake_display_albedo(gltf, diffuse, ao, folder / "MarbleDisplayAlbedo.png")
                material(folder, folder_name, display, normal.name)
                texture("rock_01", "Diffuse", folder, "DebrisAlbedo.jpg")
                texture("rock_01", "nor_dx", folder, "DebrisNormal.jpg")


if __name__ == "__main__":
    main()
