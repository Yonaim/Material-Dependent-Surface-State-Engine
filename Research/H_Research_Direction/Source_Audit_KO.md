# 원문 조회와 한계의 근거 기록

조사 기준일은 2026-11-23이다. 저자 공개 원문, 출판사, 공식 제품 문서를 근거로 사용했다. 인용 인덱스와 제목·저자 검색은 후속 원문의 탐색에 사용했다. 서로 다른 데이터베이스의 인용 수를 합치거나 완전한 조사로 해석하지 않았다.

| 항목 | 실제 확인한 위치와 범위 | 관련 없는 결론 |
|---|---|---|
| Herson–Paris–Michel | 원문 Eq. 5–7, §4.2·4.3, §5.1–5.2 Fig. 8, §7. 저자 프로젝트의 논문·영상·fluid demo 링크. | 더 높은 안정 한계에 도달하는 기존 implicit/IMEX 효과의 검증. [원문](https://eliemichel.github.io/dripping-thin-films/documents/herson26dripping_thin_films.pdf) |
| Herson 인용 후속 | DOI 10.1111/cgf.70416 → OpenAlex W7143464913. 2026-11-23까지의 인용 조회에서 Price 한 건. 기관 초록과 접근 제한 표시를 확인. | Price의 전체 방법·부록 확인, 모든 미색인 후속의 부재. [인용 인덱스](https://openalex.org/W7143464913) · [Price 저자 기관 기록](https://eprints.worc.ac.uk/16682/) |
| 박막 수치 선례 | *Real-time Viscous Thin Films* 저자 기관, *Functional Thin Films on Surfaces* 원문과 양수성 한계, Beltrame–Thiele의 §3, Orizaga–Witelski의 §1·3. | Herson과 동일 경계·렌더·CPU/GPU에서 정확한 재현. [박막 IMEX 원문](https://arxiv.org/html/2405.19483) |
| NDE | 공간 정상성과 비정상 실패 §6.8, 보충자료 §3 희소 프레임·§6 관련 외관으로의 전이. JAX 소스, 샘플, 가중치 다운로드 경로 확인. DOI OpenAlex W4404526525에서 확인한 인용 저자 후속은 비조건부 영상의 *Generative Video Bi-flow*. | wet–wipe–rewet 조건부 입력 일반화, 코드 실행 또는 데이터 라이선스 승인. [NDE 원문](https://arxiv.org/html/2410.07128v2) · [저자 코드](https://github.com/ryushinn/ode-appearance) |
| MeshNCA와 후속 | MeshNCA 원문 §7·8, 공개 브라우저 데모·코드 링크. OpenAlex W4400820364와 제목 검색에서 신호 반응형 NCA, SPH-NCA 포스터, *From Cells to Pixels*, DynaMesh의 원문을 추적했다. DynaMesh의 per-video fit, 고정 형상, 전역 배열 실패를 §4–5에서 확인했다. | Meshing과 PBR UI가 수분 상태의 이력 및 밸런스를 검증했다는 결론. [MeshNCA 원문](https://infoscience.epfl.ch/bitstreams/07fa1b30-9248-4b6b-88ce-2b2b099e625d/download) · [DynaMesh 원문](https://arxiv.org/html/2610.05529) |
| 전통적 appearance | Dorsey·Jensen·Lu의 모델 및 CAVE의 실제 시간별 BRDF. *Appearance Manifolds*도 한 시점의 공간적 weathering 차이로 시간 순서를 재구성한다. | 단순 젖음 변화 학습, 적은 입력으로 시간적 appearance를 복원하는 발상의 신규성. [Appearance Manifolds](https://www.microsoft.com/en-us/research/publication/appearance-manifolds-for-modeling-time-variant-appearance-of-materials/) · [CAVE](https://www1.cs.columbia.edu/CAVE/databases/tvbrdf/about.php) |
| 제품 및 LOD | Unreal UE 5.9 Fluids Reference와 Effect Types, Unity VFX Graph 10.8의 Update 문서, 현재 Fluid Frenzy 공식 문서의 time step, wetness layer, culling, 텍스처 해상도. Goldade 등의 dual-resolution 논문도 확인했다. | 실제 효과 장면 벤치마크, 최신 모든 Unity 버전 및 플러그인 조합 검증, 숨은 플러그인 기능의 부재. [Epic](https://dev.epicgames.com/documentation/en-us/unreal-engine/niagara-fluids-reference-in-unreal-engine) · [Unity](https://docs.unity3d.com/Packages/com.unity.visualeffectgraph@10.8/manual/Context-Update.html) · [Fluid Frenzy](https://frenzybyte.github.io/fluidfrenzy/docs/fluid_simulation_components/) |
| 국소 역제어 | Chen 등의 *Fluid Control with Localized Spacetime Windows* §6, 저해상도→세부 계층적 비교군. OpenAlex W7106334723의 지정 기간 인용 0건을 확인했다. | 겹치는 창이나 움직이는 물체가 다른 논문에서 이미 지원되지 않는다는 보편 결론. [국소 제어 원문](https://arxiv.org/html/2511.15189) · [계층적 제어](https://doi.org/10.1111/cgf.70226) |

검색 쟁점에는 “Dripping Thin Films Herson Paris Michel follow-up fingering IMEX”, “Neural Differential Appearance Equations action conditioned relightable”, “Mesh Neural Cellular Automata wetting resolution follow-up”, “interactive fluid localized spacetime windows editing transfer”, “Niagara settings Fluid Frenzy wetness resolution”를 썼다. OpenAlex에서는 DOI로 문헌 ID를 얻고 인용 검색 필터에 cites:<ID>와 to_publication_date:2026-11-23을 사용했다. 새로운 주제는 직접적인 일치 결과와 선행 기술의 유사성을 함께 검토했다.

## 검증과 재현

이번 작업은 F/G 보고서와 결과를 읽었다. 동일한 작동 조건으로 F/G 장기 벤치마크를 다시 실행하지 않았다. 새 실험 데이터는 stdlib Python으로 계산한 B/C 반례만 포함한다. 한국어 Report_KO.md에 인용한 실제 현상·페이퍼 결과와 이번의 해석을 구분했다.

~~~powershell
python Evidence/check_counterexamples.py > Evidence/Counterexamples.json
~~~

실행은 Python 3.12.10이며 매개변수와 수학 검사가 코드·출력에 있다. 새 폴더에 종료·문헌·실험 설계만 기록하고 엔진 소스와 F/G의 파일은 보존했다. 엔진 빌드와 원격 Git은 실행하지 않았다. [종료 근거 해시](Evidence/Temporal_Evidence_SHA256.json)
