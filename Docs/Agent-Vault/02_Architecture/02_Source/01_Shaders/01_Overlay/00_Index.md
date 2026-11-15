# Overlay Shader Source — Index

Render용 Texel Geometry·Coverage 준비와 material shading을 분리한다. 이 단계의 smoothing/scale은 Solver의 geometry에 다시 기록하지 않는다.

| 구간 | 문서 |
|---|---|
| top/side geometry, coverage, indirect command | [[02_Architecture/02_Source/01_Shaders/01_Overlay/01_Geometry-and-Coverage|Geometry & Coverage]] |
| Mud/WaterFilm/Lava 혼합과 Fragment Material | [[02_Architecture/02_Source/01_Shaders/01_Overlay/02_Overlay-Shading|Shading]] |

C++ 기록 순서는 [[02_Architecture/02_Source/00_CPP/06_Rendering|Rendering Source]], 데이터 의미는 [[02_Architecture/01_Contracts/03_Geometry|Geometry 계약]]을 따른다.
