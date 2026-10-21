"""Build simple UV-mapped stonework for the fountain and basalt showcase scenes."""

from __future__ import annotations

import math
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MESHES = ROOT / "Assets" / "Meshes"


class ObjWriter:
    def __init__(self, path: Path, material: str):
        path.parent.mkdir(parents=True, exist_ok=True)
        self.path = path
        self.lines = [f"mtllib {path.stem}.mtl", f"o {path.stem}", f"usemtl {material}"]
        self.position_indices = {}
        self.attribute_index = 1

    def triangle(self, points, uvs):
        ax, ay, az = points[0]
        bx, by, bz = points[1]
        cx, cy, cz = points[2]
        ux, uy, uz = bx - ax, by - ay, bz - az
        vx, vy, vz = cx - ax, cy - ay, cz - az
        nx, ny, nz = uy * vz - uz * vy, uz * vx - ux * vz, ux * vy - uy * vx
        length = max(math.sqrt(nx * nx + ny * ny + nz * nz), 1e-10)
        corners = []
        for (x, y, z), (u, v) in zip(points, uvs):
            key = (f"{x:.7f}", f"{y:.7f}", f"{z:.7f}")
            if key not in self.position_indices:
                self.position_indices[key] = len(self.position_indices) + 1
                self.lines.append(f"v {' '.join(key)}")
            position_index = self.position_indices[key]
            attribute_index = self.attribute_index
            self.lines.extend((f"vt {u:.7f} {v:.7f}",
                               f"vn {nx/length:.7f} {ny/length:.7f} {nz/length:.7f}"))
            corners.append(f"{position_index}/{attribute_index}/{attribute_index}")
            self.attribute_index += 1
        self.lines.append(f"f {' '.join(corners)}")

    def quad(self, points, uvs):
        self.triangle((points[0], points[1], points[2]), (uvs[0], uvs[1], uvs[2]))
        self.triangle((points[0], points[2], points[3]), (uvs[0], uvs[2], uvs[3]))

    def box(self, xmin, xmax, ymin, ymax, zmin, zmax,
            atlas=(0.02, 0.02, 0.98, 0.98), columns=3, rows=2, start_cell=0):
        a, b = xmin, xmax
        c, d = ymin, ymax
        e, f = zmin, zmax
        faces = (
            ((a,c,f),(b,c,f),(b,d,f),(a,d,f)),
            ((a,d,e),(b,d,e),(b,c,e),(a,c,e)),
            ((a,c,e),(b,c,e),(b,c,f),(a,c,f)),
            ((b,d,e),(a,d,e),(a,d,f),(b,d,f)),
            ((b,c,e),(b,d,e),(b,d,f),(b,c,f)),
            ((a,d,e),(a,c,e),(a,c,f),(a,d,f)),
        )
        left, bottom, right, top = atlas
        cell_width = (right - left) / columns
        cell_height = (top - bottom) / rows
        for face_index, face in enumerate(faces):
            cell = start_cell + face_index
            col, row = cell % columns, cell // columns
            if row >= rows:
                raise ValueError("UV atlas has too few cells")
            u0 = left + col * cell_width + 0.004
            u1 = left + (col + 1) * cell_width - 0.004
            v0 = bottom + row * cell_height + 0.004
            v1 = bottom + (row + 1) * cell_height - 0.004
            self.quad(face, ((u0,v0),(u1,v0),(u1,v1),(u0,v1)))

    def save(self):
        self.path.write_text("\n".join(self.lines) + "\n", encoding="utf-8")


def build_fountain():
    folder = MESHES / "LionFountain"
    writer = ObjWriter(folder / "FountainStonework.obj", "FountainStonework")
    # Keep the recessed bowl as simulation geometry; the lion now sits at its rim.
    profile = ((0.02, 0.11), (0.72, 0.11), (0.87, 0.19), (0.91, 0.31),
               (1.02, 0.32), (1.08, 0.26), (1.00, 0.02), (0.02, 0.02))
    segments = 64
    for ring in range(len(profile)):
        r0, z0 = profile[ring]
        r1, z1 = profile[(ring + 1) % len(profile)]
        for segment in range(segments):
            t0 = 2.0 * math.pi * segment / segments
            t1 = 2.0 * math.pi * (segment + 1) / segments
            def point(radius, angle, z):
                return (radius * math.cos(angle), -0.55 + radius * math.sin(angle), z)
            writer.quad((point(r0,t0,z0), point(r1,t0,z1),
                         point(r1,t1,z1), point(r0,t1,z0)),
                        ((0.02+0.47*segment/segments,0.02+0.96*ring/len(profile)),
                         (0.02+0.47*segment/segments,0.02+0.96*(ring+1)/len(profile)),
                         (0.02+0.47*(segment+1)/segments,0.02+0.96*(ring+1)/len(profile)),
                         (0.02+0.47*(segment+1)/segments,0.02+0.96*ring/len(profile))))
    writer.save()
    (folder / "FountainStonework.mtl").write_text(
        "newmtl FountainStonework\nKd 1 1 1\nd 1\n"
        "map_Kd FountainStoneAlbedo.jpg\nnorm FountainStoneNormal.jpg\n", encoding="utf-8"
    )


def build_altar():
    folder = MESHES / "BasaltAltar"
    writer = ObjWriter(folder / "AltarStonework.obj", "AltarStonework")
    writer.box(-1.48, 1.48, -1.78, 1.78, -0.08, 0.10,
               columns=6, rows=3, start_cell=0)
    writer.box(-1.25, 1.25, -1.55, 1.55, 0.10, 0.24,
               columns=6, rows=3, start_cell=6)
    writer.box(-0.95, 0.95, -1.35, 1.35, 0.24, 0.32,
               columns=6, rows=3, start_cell=12)
    writer.save()
    (folder / "AltarStonework.mtl").write_text(
        "newmtl AltarStonework\nKd 1 1 1\nd 1\n"
        "map_Kd AltarStoneAlbedo.jpg\nnorm AltarStoneNormal.jpg\n", encoding="utf-8"
    )


def build_bust_debris():
    folder = MESHES / "MarbleBust"
    writer = ObjWriter(folder / "BustDebris.obj", "BustDebris")
    # Keep only the scattered stone shards; the bust sits directly on the ground.
    shards = (
        ((-0.55,-0.55,-0.02),(-0.29,-0.51,-0.02),(-0.49,-0.28,-0.02),(-0.40,-0.39,0.20)),
        ((0.36,-0.61,-0.02),(0.63,-0.48,-0.02),(0.43,-0.26,-0.02),(0.51,-0.43,0.16)),
        ((-0.64,0.12,-0.02),(-0.47,0.34,-0.02),(-0.37,0.07,-0.02),(-0.49,0.17,0.14)),
    )
    faces = ((0,1,3),(1,2,3),(2,0,3),(2,1,0))
    for shard_index, shard in enumerate(shards):
        for face_index, face in enumerate(faces):
            cell = 6 + shard_index * 4 + face_index
            col, row = cell % 6, cell // 6
            u0, u1 = 0.02 + col*0.96/6 + 0.005, 0.02 + (col+1)*0.96/6 - 0.005
            v0, v1 = 0.02 + row*0.96/3 + 0.005, 0.02 + (row+1)*0.96/3 - 0.005
            writer.triangle(tuple(shard[i] for i in face),
                            ((u0,v0),(u1,v0),((u0+u1)/2,v1)))
    writer.save()
    (folder / "BustDebris.mtl").write_text(
        "newmtl BustDebris\nKd 1 1 1\nd 1\n"
        "map_Kd DebrisAlbedo.jpg\nnorm DebrisNormal.jpg\n", encoding="utf-8"
    )


if __name__ == "__main__":
    build_fountain()
    build_altar()
    build_bust_debris()
