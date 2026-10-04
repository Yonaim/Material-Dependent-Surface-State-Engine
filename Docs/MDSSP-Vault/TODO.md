- 적층 정책 설정 & 구현 완료
- 적층에 의한 높이값이 GeometryDrive에 반영되는지 확인
- 인상적인 데모를 위한 inject 방식 설계
- 문서 정리
- 노션 문서 보완 + pdf 뽑기
- wetness, 물막, mud 데모
	- mountain (기본 / 뒤집어짐): 흡수물, 물막, 머드 총 6개
	- cube: 


### Memo


- binding 22 (AccumulationHeights) 분리하기. 

- Overlay → StateOverlay로 이름 변경하기.

- 파일명 변경하기. 
```
SurfaceAccumulation.comp → SurfaceDynamicGeometryUpdate.comp
SurfaceGeometryUpdate.comp → SurfaceDynamicWeightsUpdate.comp
SurfaceDirtyDispatch.comp → SurfaceDynamicUpdateDispatch.comp
```


`Neighbor Direction Index` 용어
 
| 용어                             | 예                       | 의미                               |
| ------------------------------ | ----------------------- | -------------------------------- |
| **Descriptor binding**         | `binding = 8`           | Shader에 GPU Resource를 연결하는 포트 번호 |
| **Neighbor direction index**    | `DirectionIndex = 0..7` | Texel의 8개 이웃 중 어느 방향인지           |
| **Array index / memory index** | `Values[1234]`          | 실제 buffer 배열의 어느 element인지       |











### ADR 
- capacity가 필요한 이유
	- (그냥 state를 정규화하면 되지 않냐?에 대한 반박)
	- 재질에 다른 면 사이에서 state가 전파되는 경우, 해당 재질의 포화 기준량에 따라 농도가 다르게 표현되어야함

1. 속도가 시뮬레이션 해상도에 의존하는 문제
2. 렌더 방법 특성상 capacity 초과량을 보존해버리면 높이가 뾰족뾰족하게 되어버림
	1. 시뮬레이션 형상 높이를 capacity까지 clamp하여 제한

