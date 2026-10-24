# Showcase mesh sources

The three showcase meshes and their source textures come from Poly Haven under CC0.
The engine uses OBJ and an extended MTL `norm` entry, so the downloaded glTF
meshes were converted to OBJ while keeping their UVs. The normal images are the
DirectX variants because the renderer's default `Flip Y` setting converts them
to its tangent-space convention.

| Scene | Original asset | Local changes |
| --- | --- | --- |
| Lion Head | [Lion Head](https://polyhaven.com/a/lion_head) by Tina | Original color, AO, and normal maps are preserved. The display color map combines diffuse and AO. Scaled and converted to OBJ. The authored basin uses [Rock 01](https://polyhaven.com/a/rock_01) by Rob Tuytel. |
| Marble Bust 01 | [Marble Bust 01](https://polyhaven.com/a/marble_bust_01) by Rico Cilliers | Original color, AO, and normal maps are preserved. The display color map combines diffuse and AO. Scaled and converted to OBJ. Authored fragments use [Rock 01](https://polyhaven.com/a/rock_01). |
| Rock 07 | [Rock 07](https://polyhaven.com/a/rock_07) by Jenelle van Heerden | Original color, AO, and normal maps are preserved. The display color map combines diffuse, AO, and the luminance of glTF `COLOR_0` without its red/blue tint. Scaled and converted to OBJ. An optional stepped plinth mesh uses [Volcanic Rock Tiles](https://polyhaven.com/a/volcanic_rock_tiles) by Charlotte Baglioni. |

The downloaded assets are stored as converted meshes and 2K color/normal maps.
OBJ position indices are shared across source UV seams, while UV and authored
normal indices remain separate. This preserves the original closed mesh topology
for the engine's surface simulation and displaced geometry.
The authored basin, altar, and debris meshes also share positions across
adjacent triangles; the basin's inner profile is closed.
`Scripts/import_showcase_assets.py` and `Scripts/build_showcase_stonework.py`
reproduce the conversion and authored geometry.

The engine currently shades these meshes with diffuse and normal maps. Source AO
is baked into separate display maps because the engine does not have a material
AO channel. Rock 07's strongly colored vertex paint is used for neutral shading
only, avoiding a red tint in the dry rock. Roughness and metalness channels and
the online viewer's lighting can still change highlights.
