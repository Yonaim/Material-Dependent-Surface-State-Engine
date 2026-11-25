# Surface Simulation texture sources

These maps are distributed under CC0. See the [Poly Haven license](https://polyhaven.com/license) and [ambientCG Clay 005 license information](https://ambientcg.com/view?id=Clay005).

| Local texture | Source | Preparation |
|---|---|---|
| `Fabric080ColorRoughness.png` | [ambientCG Fabric 080](https://ambientcg.com/view?id=Fabric080), CC0 | Original 1K-PNG tile (1024 × 512). Color in sRGB RGB, linear roughness in alpha. |
| `Fabric080NormalHeight.png` | [ambientCG Fabric 080](https://ambientcg.com/view?id=Fabric080), CC0 | Same tile. Linear OpenGL tangent normal in RGB, displacement quantized from 16 to 8 bits in alpha for view-dependent parallax. |
| `FiredClay.png` | [Clay 005](https://ambientcg.com/view?id=Clay005), ambientCG | Resize the 1K color and roughness to 512 × 512 and warm the color toward fired terracotta. |
| `FiredClayNormal.png` | [Clay 005](https://ambientcg.com/view?id=Clay005), ambientCG | Resize the OpenGL normal from the 1K JPG package to 512 × 512 RGB. |
| `WovenCottonNormal.png` | [Fabric Pattern 05](https://polyhaven.com/a/fabric_pattern_05), Rob Tuytel | Resize the 1K OpenGL normal PNG to 512 × 512 RGB. Shares UVs with all bundled fabric color variants. |
| `WovenCotton.png` | [Fabric Pattern 05](https://polyhaven.com/a/fabric_pattern_05), Rob Tuytel | Resize 1K `col_02` and roughness to 256 × 256. |
| `WovenCottonRusset.png` | `WovenCotton.png` | Recolor the albedo to russet using the source luminance. Keep the weave, check pattern, and original roughness channel. |
| `WovenCanvasCheck.png` | `WovenCotton.png` | Apply an ivory and mustard check pattern over the source weave detail and roughness. |
| `BrushedSteel.png` | Procedural texture | Generate 256 × 256 neutral gray reflectance and roughness with lengthwise brushing. |
| `WaxThermalRamp.png` | Procedural chart albedo | Bake HEAT and COOL labels at the chart ends and mark the travel direction along the rail. |
| `FineWood.png` | [Fine Grained Wood](https://polyhaven.com/a/fine_grained_wood), Rob Tuytel | Resize 1K diffuse and roughness to 512 × 512. |
| `WeatheredStone.png` | [Rock 01](https://polyhaven.com/a/rock_01), Rob Tuytel | Resize 1K diffuse and roughness to 512 × 512. |
| `GreyStonePathDiffuse.png` | [Grey Stone Path](https://polyhaven.com/a/grey_stone_path), Amal Kumar | Original CC0 1K diffuse PNG. |
| `GreyStonePathNormal.png` | [Grey Stone Path](https://polyhaven.com/a/grey_stone_path), Amal Kumar | Original CC0 1K tangent-space OpenGL normal PNG. |

| `MeadowGrass.png` | [Leafy Grass](https://polyhaven.com/a/leafy_grass), Charlotte Baglioni | Resize 1K diffuse and roughness to 512 × 512. Shift the albedo toward green to distinguish turf from bare soil. |

RGB stores sRGB albedo and the fourth channel stores linear roughness. The material alpha is read as roughness by the shader. Surface Simulation loads these bundled files from disk at scene creation. The separate normal maps are sampled as linear RGB in the substrate PBR pass.

## ambientCG demo assets

These assets are distributed under [ambientCG's CC0 license](https://docs.ambientcg.com/license/).

| Files in `AmbientCG/` | Source | Bundled use |
|---|---|---|
| `Wood049.png`, `Wood049Normal.png` | [Wood 049](https://ambientcg.com/view?id=Wood049) | Charring Wood |
| `Metal010.png`, `Metal010Normal.png` | [Metal 010](https://ambientcg.com/view?id=Metal010) | Heated Water Channel, Glacier Cut, and the steel comparison specimen |
| `Snow003.png`, `Snow003Normal.png` | [Snow 003](https://ambientcg.com/view?id=Snow003) | Snow surfaces in Snow on Fabric and Glacier Cut. Color, roughness, and NormalGL are resized to 512 × 512 from the 1K PNG package. |
| `Sponge001.png`, `Sponge001Normal.png` | [Sponge 001](https://ambientcg.com/view?id=Sponge001) | Yellow sponge comparison specimen |
| `Concrete030.png`, `Concrete030Normal.png` | [Concrete 030](https://ambientcg.com/view?id=Concrete030) | Terraced Catchment |
| `DayEnvironmentHDRI049.hdr` | [Day Environment 049](https://ambientcg.com/view?id=DayEnvironmentHDRI049) | Brick Runoff and Terraced Catchment |
| `DayEnvironmentHDRI067.hdr` | [Day Environment 067](https://ambientcg.com/view?id=DayEnvironmentHDRI067) | Glacier Cut |
| `IndoorEnvironmentHDRI014.hdr` | [Indoor Environment 014](https://ambientcg.com/view?id=IndoorEnvironmentHDRI014) | Heated Water Channel |

Material maps are resized to 512 × 512 from the 1K JPG packages. Snow 003 and Sponge 001 use the 1K PNG packages. Color is packed into RGB, roughness into alpha, and NormalGL is stored in a separate PNG. Steel uses the shared metal shader. The fabric specimen reuses the bundled Fabric 080 maps. The HDR panoramas are 1024 × 512 Radiance RGBE conversions of the linear EXRs from the 1K packages. Exposure and rotation are authored per scene.
