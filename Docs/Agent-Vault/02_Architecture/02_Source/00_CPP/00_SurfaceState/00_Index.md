# Surface State CPU Source — Index

**신규로 확인된 자료:** [[02_Architecture/02_Source/00_CPP/00_SurfaceState/04_Cache-and-Lifecycle|CPU `.Surface` Cache와 Version/수명]]


기존 Surface State 목록을 Runtime·Registry, Mapping·Geometry, Contact·State의 세 책임으로 분리했다. Surface GPU dispatch는 [[02_Architecture/02_Source/00_CPP/05_Solver|Solver]], 리소스와 ABI는 [[02_Architecture/02_Source/00_CPP/01_GPU/00_Index|GPU]]에서 찾는다.

## 어디서 읽을까?

| 질문 | 문서 |
|---|---|
| Scene profile을 어떻게 등록·캐시하는가? | [[02_Architecture/02_Source/00_CPP/00_SurfaceState/01_Runtime-and-Registry|Runtime & Registry]] |
| UV texel graph·normal/meso geometry는 누가 만드는가? | [[02_Architecture/02_Source/00_CPP/00_SurfaceState/02_Mapping-and-Geometry|Mapping & Geometry]] |
| Contact가 어느 texel의 InputDelta가 되고 디버그로 어떻게 확인하나? | [[02_Architecture/02_Source/00_CPP/00_SurfaceState/03_Contact-and-State|Contact & State]] |

## 데이터 흐름

Scene Mesh/Profile/Resolution → Runtime Data/Registry → UV Mapping/Shared Geometry → Instance State/InputDelta → GPU Resource → Solver.
