"""Build the central Korean report from the completed archived study artifacts."""
from pathlib import Path
import json,datetime,argparse
ROOT=Path(__file__).resolve().parents[2]
RESULTS=ROOT/'Docs/MDSSP-Vault/04_Development/Performance-Results'
DOC=ROOT/'Docs/MDSSP-Vault/04_Development/0003_Solver-Performance.md'
FOLDERS={'main':'20261022-optimization-suite-main','short':'20261022-single-transport-validation',
         'extra':'20261022-workload-supplement','history':'20261022-development-history'}
QUESTIONS={
 'disk_cache':('002','전처리 결과를 디스크 캐시에서 다시 읽기'),
 'buffer_pool':('003','같은 씬의 GPU 자원 재생성에서 버퍼 재사용'),
 'shared_profiles':('004','같은 Profile 표를 씬에서 공유'),
 'raw_flux_cache_fp32':('005','계산한 이동량을 저장해서 다시 쓰기 — FP32'),
 'inactive_zero_write_elision':('006','빈 source의 불필요한 0 쓰기 생략'),
 'sparse_solver':('007','상태가 있는 영역 주변만 Solver 실행'),
 'sparse_height':('008','필요한 영역만 높이 갱신'),
 'sparse_geometry':('009','필요한 영역만 형상·가중치 갱신'),
 'sparse_all':('010','부분 계산과 채널 생략을 묶어서 사용'),
 'global_channel_mask':('011','전체에서 사용하지 않는 채널 생략'),
 'workgroup_channel_mask':('012','각 workgroup에 필요한 채널만 계산'),
 'instance_batching':('013','여러 물체의 같은 계산 단계를 모아 실행'),
 'coalesced_fp32':('014','이웃 스레드가 연속된 이동량 데이터를 읽도록 배치'),
 'fp16_flux':('015','이동량을 FP16으로 저장'),
 'fp16_weights':('016','가중치를 FP16으로 저장'),
 'state_texture':('017','렌더링 상태를 SSBO 대신 Texture로 읽기'),
 'precompute_coverage':('018','화면에서 쓸 필터 결과를 compute로 미리 계산'),
 'separable_coverage':('019','필터를 가로·세로 계산으로 나누기'),
 'height_halo':('020','높이 필터의 주변값을 shared memory에서 재사용'),
 'coverage_halo':('021','Overlay coverage 필터의 주변값 재사용'),
 'state_texture_halo':('022','State texture 필터의 주변값 재사용'),
 'dirty_overlay':('023','Overlay 형상을 변경된 타일 중심으로 갱신'),
 'tile_culling':('024','Overlay 높이 필터·coverage의 타일 부분 갱신'),
 'mesh128':('025','렌더 메시 해상도 변경'),
 'mesh64':('025','렌더 메시 해상도 변경'),
 'mesh256':('025','렌더 메시 해상도 변경'),
 'mesh512':('025','렌더 메시 해상도 변경'),
 'frustum_culling':('026','화면 밖 물체의 draw 생략'),
 'gpu_validation_elision':('027','같은 C++ Release에서 GPU 반복 검증 제거'),
 'compile_time_solver':('028','Solver의 옵션 분기를 컴파일 시점에 고정'),
 'rotation_invariant_cache':('029','회전만 바뀔 때 정적 CPU 가중치 캐시 재사용'),
 'sparse_render_height':('030','렌더링용 높이 필터의 부분 계산'),
 'tile8':('031','Overlay 타일 크기 선택'),
 'tile32':('031','Overlay 타일 크기 선택'),
 'source_reuse':('032','빈 source 계산 생략·Profile/형상/행렬 공통값 재사용 묶음'),
 'render_math_reuse':('033','렌더 Profile 조회·조명 방향 계산 재사용'),
 'render_math_reuse_ssbo':('033','렌더 Profile 조회·조명 방향 계산 재사용 — SSBO 조건'),
 'top_triangle_compaction':('035','활성 top 삼각형만 압축해서 draw'),
}
GROUPS={'G1':'1. 로딩·리소스 재사용','G2':'2. Solver 중복 계산·캐시','G3':'3. 필요한 영역만 시뮬레이션',
        'G4':'4. 여러 인스턴스의 실행·동기화','G5':'5. 데이터 배치·정밀도','G6':'6. 렌더 상태 읽기·필터·조명',
        'G7':'7. Overlay 형상·draw 작업량','G8':'8. 셰이더 실행 경로 고정'}
VARIANTS={
 'reference':'현재 기본 경로', 'cache_off':'이동량 재계산(FP32)', 'cache_fp32':'이동량 캐시(FP32)',
 'zero_writes':'빈 source에도 0 기록', 'legacy_layout':'기존 이동량 배치(FP32)', 'weights_fp16':'가중치 FP16',
 'solver_dense':'Solver 전체 영역', 'height_dense':'높이 전체 영역', 'geometry_dense':'형상·가중치 전체 영역',
 'dense_all':'부분 계산 3종·채널 생략 모두 OFF', 'mask_none':'채널 생략 모두 OFF', 'mask_global':'전체 채널 생략만 ON',
 'fragment_ssbo':'화면에서 SSBO 필터', 'fragment_texture':'화면에서 Texture 필터', 'coverage_direct':'분리하지 않은 coverage 필터',
 'height_halo':'높이 shared memory ON', 'coverage_halo':'coverage shared memory ON', 'texture_halo':'Texture 필터 shared memory ON',
 'overlay_full':'Overlay 형상 매번 전체 갱신', 'tile_culling_off':'높이 필터·coverage의 타일 갱신 OFF',
 'no_frustum':'화면 밖 draw 생략 OFF', 'serial':'물체별 순차 Solver', 'validation':'GPU 반복 검증 ON', 'runtime':'실행 중 Solver 옵션 분기',
 'cold_load':'전처리 디스크 캐시 사용 안 함', 'reload_pool':'버퍼 재사용하며 자원 5회 재생성', 'reload_no_pool':'버퍼 재사용 없이 자원 5회 재생성',
 'unshared_profiles':'같은 Profile 표를 별도 할당', 'height_smoothing_full':'렌더 높이 필터 전체 영역',
 'solver_reuse_off':'빈 source·공통값을 다시 계산', 'render_reuse_off':'Profile·조명 공통값 다시 계산',
 'render_ssbo_reuse_off':'SSBO에서 공통값 다시 계산', 'render_ssbo_reuse_on':'SSBO에서 공통값 재사용',
 'full_top':'전체 top 인덱스로 직접 draw', 'compact_top':'활성 top 압축·간접 draw',
 **{f'mesh{r}':f'렌더 메시 {r}' for r in [64,128,256,512]},
 **{f'tile{r}':f'타일 한 변 {r} texel' for r in [8,32]}}
METRICS={'scene_load_ms':'Scene 로드 CPU ms','reload_ms':'자원 재생성 CPU ms','profile_bytes':'Profile 표 B',
         'simulation_gpu_elapsed_ms':'Simulation GPU ms','render_gpu_ms':'Render GPU ms','frame_gpu_elapsed_ms':'전체 command-buffer GPU ms'}

def load(path):return json.loads(path.read_text(encoding='utf8'))
def num(v,digits=3):return '—' if v is None else f'{v:.{digits}f}'
def table(headers,rows):
 def numeric(value):
  try:float(str(value).replace(',',''));return True
  except (ValueError,TypeError):return False
 alignment=[]
 for column in range(len(headers)):
  values=[row[column] for row in rows if str(row[column]) not in ['—','']]
  alignment.append('---:' if values and all(numeric(value) for value in values) else '---')
 return ['','| '+' | '.join(headers)+' |','|'+'|'.join(alignment)+'|',
         *['| '+' | '.join(str(x) for x in row)+' |' for row in rows],'']
def pathlink(key,name):return f'Performance-Results/{FOLDERS[key]}/{name}'

def overview(comparisons,complete):
 def selected(name,source=None,scene=None):
  return [r for r in comparisons if r['comparison']==name and (source is None or r['source']==source) and (scene is None or r['scene']==scene)]
 def span(name,source=None,scene=None):
  values=[r['reduction_percent'] for r in selected(name,source,scene) if r['reduction_percent'] is not None]
  return f'{min(values):.1f}~{max(values):.1f}%' if values else '미집계'
 rows=[
  ['1. 로딩·재사용',f"전처리 캐시로 Scene 로드 구간 {span('disk_cache','main')} 감소. 버퍼 pool은 최종 할당량을 늘리고 시간 효과는 씬별로 다름",'디스크 캐시 유지 근거 확보. pool은 보관 메모리와 재생성 빈도로 판단'],
  ['2. Solver 계산 재사용',f"FP32 이동량 캐시 {span('raw_flux_cache_fp32')} 감소, source 재사용 묶음 {span('source_reuse','extra')} 감소",'재사용 이득 확인. 긴 실행에서 생긴 상태 차이는 별도 검토'],
  ['3. 필요한 곳만 계산',f"기본 두 씬의 전체 부분 계산 묶음 {span('sparse_all','main')} 감소. 넓은 512에서는 오히려 증가",'활성 영역이 넓을 때 전체 계산으로 바꾸는 방식 검토'],
  ['4. 여러 물체 실행', '동일 물체 3/6개: 256에서는 묶음 실행이 느리고, 512에서는 빠름','해상도·작업량별 선택 검토. 항상 빠른 방식으로 설명하지 않음'],
  ['5. 데이터 배치·FP16',f"연속 배치 {span('coalesced_fp32','main')} 감소. FP16은 빨라졌지만 결과 차이가 있고 할당량은 그대로",'배치 개선은 유지 근거 확보. FP16은 허용 오차를 정해 선택'],
  ['6. 렌더링 필터',f"필터 사전 계산 {span('precompute_coverage','main')} 감소. Texture 단독 사용은 이득 확인 못 함",'사전 계산 경로는 유리. 화면 차이와 shared-memory 옵션은 각각 판단'],
  ['7. Overlay 작업량',f"기본 두 씬의 부분 갱신 {span('dirty_overlay','main')} 감소. 넓은 입력에서는 타일 필터가 불리",'부분/전체 갱신을 조건에 따라 선택. 메시 해상도는 화질과 함께 선택'],
  ['8. 셰이더 경로', '컴파일 시점 Solver 옵션 고정은 네 조건 모두 실행 중 분기보다 느림','고정 variant가 항상 빠르다는 주장은 철회. GPU 검증 제거는 별도 결과 확인']]
 lines=['<!-- PERFORMANCE_STUDY_OVERVIEW_START -->','## 이번 실험에서 먼저 볼 결론','',
        f"- 집계 시각: {datetime.datetime.now().astimezone().isoformat()}. 상태: **{'측정·검증 종료' if complete else '측정 진행 중'}**.",
        '- 아래는 이번 Radeon RX 6700 XT 실험에서 관찰한 결과다. 감소율은 각 행에 해당하는 CPU/GPU 구간의 시간이며 서로 합산하지 않는다. 세부 조건·p95·상태/화면 차이는 아래 8개 묶음 표에 있다.','']
 lines+=table(['분류','핵심 관찰','권장 판단'],rows)
 lines+=['제품 기본값은 바꾸지 않았다. V2와 회전 불변 CPU 캐시 보조 실험은 Windows 앱 제어로 실행하지 못했으며, 개별 효과를 분리하지 않은 과거 변경은 측정 범위 표에 명시했다.','',
         '<!-- PERFORMANCE_STUDY_OVERVIEW_END -->','']
 return '\n'.join(lines)

def main():
 ap=argparse.ArgumentParser();ap.add_argument('--allow-incomplete',action='store_true');args=ap.parse_args()
 manifests={key:load(RESULTS/folder/'manifest.json') for key,folder in FOLDERS.items() if (RESULTS/folder/'manifest.json').exists()}
 if args.allow_incomplete:
  for key,folder in FOLDERS.items():
   if key not in manifests:manifests[key]={'status':'not_started','plan':load(RESULTS/folder/'plan.json'),'runs':[]}
 if not args.allow_incomplete:
  assert len(manifests)==4 and all(m['status']!='running' for m in manifests.values()),'Study phases still running'
  audit_path=RESULTS/FOLDERS['main']/'study-audit.json'
  assert audit_path.exists() and load(audit_path)['status']=='pass','Final study audit has not passed'
 comparisons=[];workloads=[]
 for key in ['main','extra']:
  p=RESULTS/FOLDERS[key]/'comparisons.json'
  if p.exists():
   data=load(p)
   for row in data['comparisons']:comparisons.append({**row,'source':key})
   workloads.extend(data.get('workloads',[]))
 short_path=RESULTS/FOLDERS['short']/'validation-summary.json'
 short=load(short_path) if short_path.exists() else {}
 history_path=RESULTS/FOLDERS['history']/'history-summary.json'
 history=load(history_path) if history_path.exists() else {}
 audit_path=RESULTS/FOLDERS['main']/'study-audit.json'
 study_audit=load(audit_path) if audit_path.exists() else {}
 complete=all(key in manifests and manifests[key]['status'] in ['complete','completed_with_failures'] for key in FOLDERS) and study_audit.get('status')=='pass'
 lines=['## 8개 묶음 최적화 비교 — PERF-002~035','',
        '> 현재 고정 스냅샷에서 비교할 항목만 OFF·이전 방식으로 바꿔 대조했다. 아래 감소율은 표에 적힌 구간의 시간/할당량 감소율이며 FPS 개선률이 아니다.','',
        f"- 기록 유형: **사후 재측정**. 보고서 상태: **{'실행 종료·검증 결과 기록' if complete else '측정 진행 중'}**.",
        '- 코드: `fa83c68` + PERF-001 당시 미커밋 변경 + 실험용 계측/복원 patch. 제품 코드의 기본값을 바꾸지 않았다.',
        '- 각 질문에 ID를 부여했다. 같은 질문의 해상도·입력 범위·인스턴스 수가 다른 경우에는 같은 ID 아래 조건별 행으로 기록했다. 이 문서 하나가 중앙 보고서다.','']
 lines+=table(['실행 단계','계획','정상 종료','실패','상태'],[
  [key,len(m['plan']['runs']),sum(r['status']=='complete' for r in m['runs']),sum(r['status']!='complete' for r in m['runs']),m['status']] for key,m in manifests.items()])
 if study_audit.get('status')=='pass':
  lines+=[f"최종 독립 검증: **{sum(p['captured'] for p in study_audit['phases']):,}회 / {sum(p['measured_frames'] for p in study_audit['phases']):,}개 측정 프레임, 검사 오류 0개**. 프레임 연속성·옵션·동일 시작 규칙의 카메라/화면 영역·고정 timestep·상태 유효성·반복 State 기록·바이너리/보관본 해시·GPU 앱 실행 중복 여부를 확인했다. 구버전 exporter의 미지원 검사는 audit의 `unsupported_checks`와 아래 제한에 남겼다.",'']
 lines+=['### 이번 측정으로 확인한 범위','',
  '8개 묶음은 분류다. 아래 비교쌍의 현재 기여도를 측정했으며, 묶음 안의 모든 과거 변경을 하나씩 제거한 실험은 아니다. 개별 측정하지 않은 변경은 개선율을 붙이지 않는다. 여러 개별 감소율이나 묶음 감소율을 더해 전체 개선율로 사용하지 않는다.','']
 lines+=table(['묶음','이번에 직접 비교한 것','별도 효과를 분리하지 않은 것'],[
  ['1','디스크 캐시·동일 씬 GPU 자원 5회 재생성·여러 runtime의 Profile 공유','pool 재사용 횟수/누적 요청 바이트/보관량을 출력. 전체 vkAllocateMemory 호출 횟수는 별도 계측하지 않음'],
  ['2','RawFlux FP32 캐시·0 쓰기 생략·source 공통값 재사용 묶음','초기 TransferWeight/outgoing 합/ConcavityWeight 캐시 각각의 독립 효과. 회전 불변 CPU 캐시는 OS 실행 차단'],
  ['3','Solver/height/geometry 부분 계산·전역/workgroup 채널 마스크·넓은/여러 채널 입력','persistent schedule 유지 자체만 제거한 별도 비교는 없음'],
  ['4','현재 Solver의 물체별 순차 기록과 단계별 묶음 기록','개별 barrier 하나마다의 독립 비용은 미측정'],
  ['5','RawFlux 배치·FP16 Flux·FP16 Weights·실제 할당량','Profile 등 나머지 compact/SoA 변경 각각의 효과·하드웨어 실제 전송 바이트는 미측정'],
  ['6','Texture/SSBO·사전 계산·분리 필터·shared memory·공통 조명 계산 재사용','외관이 함께 달라진 과거 렌더 수식 전체를 동일 품질로 복원한 비교는 아님'],
  ['7','dirty 갱신·타일/메시 크기·화면 밖 draw 생략·top 압축 draw','side/top 갱신 통합 자체·occupancy scan 단독 시간은 미분리'],
  ['8','GPU 반복 검증·runtime/compile-time Solver 선택','pipeline 생성/드라이버 내부 메모리 효과는 미측정']])
 lines+=['### 공통 조건과 읽는 방법','',
  '- 기본: BrickCube·Mountain, simulation 256/512, 60프레임 준비 + 600프레임 측정 + query 회수 2프레임, 각 조건 3회, 새 프로세스, 고정 dt=1/60, auto substepping OFF, Geometry feedback ON.',
  '- Release·Vulkan validation OFF. GPU validation 비교만 production SPIR-V의 `MDSS_GPU_VALIDATION=1/0`을 바꾼다. C++ 빌드와 알고리즘 옵션은 동일하다.',
  '- 기본 base mesh·Overlay mesh는 모두 simulation과 같은 256/512다. 메시 실험은 두 render mesh를 함께 바꾸고 simulation 해상도는 유지한다. Overlay mesh 하나만 바꾼 효과로 해석하지 않는다.',
  '- 기본 화면은 1280×720 swapchain 안의 Lit viewport 469×571, 카메라는 Scene 값이다. 화면 밖 실험만 카메라를 이동한 별도 Scene을 사용한다.',
  '- 각 묶음에서 반복 2의 실행 순서를 반대로 한다. 시간은 실행별 중앙값/p95를 먼저 구하고, 세 반복의 해당 값의 중앙값으로 정리한다. 반복 범위 겹침은 효과 불확실의 근거이며 통계적 유의성 검정을 대신하지 않는다.',
  '- **로딩 예외:** 0 준비/2 측정 프레임으로 실행하되 지표는 `TSceneLoader::Load`의 CPU 경과 시간, 또는 `Renderer::ReloadSceneResources`를 5회 호출한 시간, 또는 Profile 표의 할당 B다. 자원 재생성은 GPU idle 대기·State system/descriptor/pipeline 등 재구성을 포함하며 Scene 파일을 다시 읽는 실험은 아니다. OS 디스크 캐시를 비운 실험도 아니다. CPU 시간은 CPU에서 잰 경과 시간으로, 대기/I/O를 제외한 순수 CPU 연산 시간이 아니다.',
  '- **짧은 정확성 검사:** 초기 입력을 넣은 step 1 이후 전달 계산을 한 번 수행한 step 2에서 시뮬레이션을 멈춘다. 이후 drain 프레임은 상태를 바꾸지 않는다. 이 검사의 한 프레임 GPU 시간은 성능 결론에 사용하지 않는다.',
  '- 프레임 성능 실행의 최종 상태·화면은 662 step 후이고, 로딩 실험은 4 step 후다. 화면 저장 시에만 UI를 숨기며, 측정 프레임에는 UI가 포함된다. GPU 구간은 공통 timestamp로 잰 simulation elapsed/전체 command-buffer elapsed를 추가했다. 과거의 인스턴스별 stage 합계와 직접 비교하지 않는다.',
  '- 표의 **Simulation GPU ms**는 공통 simulation elapsed, **Render GPU ms**는 기존 렌더 준비+scene draw+UI 구간의 합계, **전체 command-buffer GPU ms**는 공통 시작~끝 경과 시간이다. Render 합계에 계측 사이의 빈 구간이 포함된다고 가정하지 않는다. 세 지표를 혼용하거나 하위 구간을 중복 합산하지 않는다.',
  '- 상태 상대 L1 = 모든 texel의 절대 차이 합 / 기준 상태량 절댓값 합. 화면 MAE는 viewport의 RGB 평균 절대 차이(0~255)다. 배경도 포함하므로 작은 평균만 보고 화질 동등성을 단정하지 않는다. 최대 차이·p99·변경 픽셀 비율은 JSON 원자료에 있다.',
  '- 감소율이 음수이면 비교 경로가 더 느리거나 할당량이 더 크다는 뜻이다. `화면 차이`는 픽셀 하나라도 다르면 표시하며, 곧바로 눈에 보이는 화질 저하를 뜻하지 않는다. 같은 경로를 반복했을 때의 화면 MAE 범위도 JSON에 보관했다.','']
 lines+=['이 실험은 현재 설정에서 지정한 항목의 기여도를 비교한 것이며, 모든 옵션 조합에서 가장 빠른 구성을 찾은 실험은 아니다. 여러 권장 옵션을 함께 바꿀 때는 그 조합을 새로 검증해야 한다.','']
 lines+=table(['추가 Scene 이름','변경한 조건'],[
  ['BrickCube-Wide','lava 입력 반경 1000, falloff 0. 넓은 영역에 같은 초기 입력'],
  ['BrickCube-Mixed','heat/mud/waterfilm/lava를 서로 다른 x 위치에 입력, 반경 0.22, 강도 1.5'],
  ['*-Offscreen','카메라 위치와 target의 모든 좌표에 +100. 물체·입력은 동일'],
  ['Mountain-1/3/6-identical','첫 Mountain 물체·입력을 1/3/6개 복제, x 간격 1.1. 사용하지 않는 원본 animation 참조 제거'],
  ['Mountain-DistinctRuntime','서로 다른 mesh 경로 3개, 각 1 instance, 동일 Profile 세트'],
  ['*-Additional / *-Reuse / *-Top','현재 기본 에셋·카메라에서 지정한 렌더/계산 경로만 비교. Wide-Top은 넓은 입력 유지']])
 lines+=['### 실제 입력 영역의 참고 수치','',
         'State 양수 texel 비율은 마지막 662 step 후에 채널 하나라도 양수인 texel 수 / 할당 texel 수다. 유효하지 않은 atlas 공간도 분모에 포함된다. Overlay 비율은 측정 600프레임의 실행별 중앙값을 구한 뒤 세 반복의 중앙값이다. 이 수치들은 Solver의 실제 활성 workgroup 비율이 아니며, 전환 임계값을 바로 결정하는 근거로 사용하지 않는다.','']
 lines+=table(['씬 / simulation','할당 texel','State 양수 texel %','0 아닌 채널 / 등록 채널','Overlay 활성 tile %','활성 top 삼각형 %'],[
  [f"{w['scene']} / {w['resolution']}",f"{w['allocated_texels']:,}",num(w['positive_state_texel_fraction']*100,2),f"{w['nonzero_state_channels']} / {w['registered_channels']}",num(w.get('overlay_tile_fraction',0)*100,2),num(w.get('overlay_top_fraction',0)*100,2)] for w in workloads])
 for group,title in GROUPS.items():
  rows=[r for r in comparisons if r['group']==group and r.get('historical_base_valid') is not False]
  lines+=['### '+title,'']
  labels=[]
  for row in rows:
   name=row['comparison'];qid,label=QUESTIONS[name]
   if (qid,label) not in labels:labels.append((qid,label))
  definitions=[]
  for qid,label in labels:
   configurations=list(dict.fromkeys((r['base_variant'],r['candidate_variant'],r['metric']) for r in rows if QUESTIONS[r['comparison']]==(qid,label)))
   for a,b,metric in configurations:definitions.append([f'PERF-{qid}',label,VARIANTS.get(a,a),VARIANTS.get(b,b),METRICS.get(metric,metric)])
  lines+=table(['실험 ID','질문','기준 경로','비교 경로','측정 지표'],definitions) if labels else ['미측정.','']
  out=[]
  for r in rows:
   q=r['quality'];name=r['comparison'];qid,_=QUESTIONS[name]
   if q.get('status')!='complete':verdict='정확성 확인 실패'
   elif not r['repeat_state_reproducible']:verdict='반복 상태 불일치'
   elif r['speed_verdict']=='overlapping_repeat_ranges':verdict='시간/할당량 범위 겹침'
   elif r['speed_verdict']=='increase_observed':verdict='이 조건에서 증가'
   else:verdict='감소 관찰'
   if q.get('state_exact') is False:verdict+=' · 상태 차이'
   elif q.get('image_exact') is False and group!='G1':verdict+=' · 화면 차이'
   unit='B' if r['metric']=='profile_bytes' else 'ms'
   tag=f' ({name})' if name.startswith(('mesh','tile')) else ''
   out.append([f'PERF-{qid}{tag}',f"{r['scene']} / {r['resolution']}",unit,num(r['base_median'],0 if unit=='B' else 3),num(r['candidate_median'],0 if unit=='B' else 3),num(r['reduction_percent'],2),
               num(q.get('state_l1_relative')*100 if q.get('state_l1_relative') is not None else None,5),num(q.get('image_mae_u8') if group!='G1' else None),verdict])
  lines+=table(['ID / 조건','씬 / simulation','단위','기준 중앙값','비교 경로 중앙값','감소 %','상태 L1 %','화면 MAE','판단'],out) if out else []
  if group=='G1':lines+=['Profile 공유의 역사적 base는 runtime별 표다. 같은 runtime의 여러 instance는 예전에도 공유했다. 따라서 일반 Mountain의 per-instance 복제 arm은 역사적 효과 산정에서 제외하고, 서로 다른 runtime 3개가 같은 Profile을 쓰는 통제 씬을 사용했다. 표의 B는 **Profile 표만의 할당량**이며 전체 GPU 메모리 감소율이 아니다.','']
  if group=='G1':lines+=['로딩 실험은 준비 프레임 없이 frame 0에서 설정을 저장해, 초기 화면 영역 1280×672가 기록됐다. 일반 프레임 성능 실험의 안정된 영역 469×571과 구분한다. 카메라 위치·시선·FOV는 같고, 같은 로딩 비교쌍의 시작 규칙·영역도 일치한다. 저장 영역과 마지막 화면의 실제 viewport가 다를 수 있어 로딩 실험의 화면 MAE는 화질 판단에서 제외했다. Scene 로드/자원 재생성 CPU 시간과 메모리 비교에 이 화면 지표를 사용하지 않았다.','']
  if group=='G2':lines+=['PERF-032는 빈 source 계산 생략, Profile/포화도/source 형상 재사용, normal matrix의 CPU 준비를 묶은 대조다. 과거 파일 전체를 되돌린 것이 아니라 현재 ABI에서 지정 연산을 반복하도록 복원했다. 사용하지 않는 scratch 값까지 동일하다는 주장은 하지 않는다.','',
    '**PERF-029 회전 불변 CPU 캐시 보조 실험은 Windows 앱 제어의 실행 차단으로 미측정이다.** 속도 수치를 추정하거나 다른 실험으로 대체하지 않았다.','']
  if group=='G3':lines+=['Wide는 입력 반경을 넓힌 별도 씬이고 Mixed는 여러 상태 채널에 다른 위치의 입력을 넣은 조건이다. Overlay 활성 타일 비율을 Solver의 실제 active workgroup 비율로 해석하지 않는다. 전체 부분 계산 OFF는 여러 설정을 함께 바꾼 묶음 효과이며 개별 감소율과 합산할 수 없다.','']
  if group=='G4':lines+=['물체별 순차 경로는 현재 Solver를 한 instance씩 호출한다. 물리 수식·자원을 유지하고 실행 순서/동기화를 비교한다. 공통 simulation elapsed를 사용한다. 보조 timestamp 기록 수는 두 경로 모두 8개지만, 순차 arm은 시작/끝에 각각 4개를 배치하므로 세부 pass 합계를 지원하지 않는다. 기록 위치에 따른 영향이나 계측이 전혀 없는 실행의 효과까지 분리한 실험은 아니다. 인스턴스 수 통제 씬은 같은 물체를 복제한 1/3/6개 구성이다.','']
  if group=='G5':lines+=['FP16은 결과 차이가 생길 수 있는 정밀도 실험이다. 현재 버퍼는 큰 저장 용량을 유지하므로 FP16을 켠 것만으로 할당량이 절반이 되지는 않는다. 상태가 진화하는 실험이므로 시간 차이를 전부 전송 대역폭 효과로만 해석하지 않는다.','']
  if group=='G6':lines+=['Texture 비교는 precompute OFF 양쪽에서 수행한다. precompute ON은 Texture를 사용하는 경로다. PERF-033의 SSBO 조건은 두 arm 모두 precompute OFF·Texture OFF를 유지한다. 필터 경계 정규화나 수치 연산 순서의 차이가 화면에 나타나면 속도와 별개로 기록한다.','']
  if group=='G7':lines+=['메시 해상도 변경은 simulation 해상도를 바꾸지 않는 품질 trade-off다. 화면 밖 씬은 카메라만 달리한 통제 조건이다. 현재 타일 컬링 옵션은 높이 smoothing·coverage의 부분 갱신을 제어하며 top 삼각형 압축 자체를 끄지 않는다. PERF-035는 별도 실행 파일에서 같은 side 계산을 유지하고, 압축 목록 생성+간접 draw와 원본 전체 인덱스의 직접 draw를 비교한다. 두 arm의 자원 할당은 유지했다. Occupancy scan 단독 시간은 계측하지 않아 전체 Overlay 준비 시간의 원인으로 단정하지 않는다.','']
  if group=='G8':lines+=['GPU 검증 제거와 컴파일 시점 옵션 고정은 서로 다른 질문이다. 알고리즘 옵션 ON/OFF의 효과와 컴파일 시점 고정의 효과를 섞지 않는다. Pipeline 내부 메모리/생성 비용은 추적하지 않았다.','']
 lines+=['### 정밀도 옵션의 실제 할당량','',
         '아래는 마지막 GPU 작업 완료 후 capture buffer를 해제한 시점의 앱 추적 할당량이다. State는 FP32이며 FP16 옵션은 이동량/가중치 저장 경로를 바꾼다. swapchain·드라이버 내부·pipeline 메모리는 포함하지 않는다.','']
 memory=[]
 for r in comparisons:
  if r['comparison'] not in ['fp16_flux','fp16_weights']:continue
  values=r.get('secondary_metrics',{}).get('allocated_bytes')
  if values:
   a=values['base_median']/2**20;b=values['candidate_median']/2**20
   memory.append([QUESTIONS[r['comparison']][0],f"{r['scene']} / {r['resolution']}",num(a,2),num(b,2),num(a-b,2)])
 lines+=table(['PERF ID','조건','기준 MiB','FP16 MiB','감소 MiB'],memory)
 lines+=['### GPU 자원 재생성 시 버퍼 재사용의 할당량','',
         '최종 할당량과 실행 중 peak를 구분한다. peak는 capture buffer·로딩/재로드의 일시적인 할당도 포함하므로, 매 프레임 필요한 데이터량이나 실제 VRAM 상주량으로 해석하지 않는다.','']
 pool_memory=[]
 for r in comparisons:
  if r['comparison']!='buffer_pool':continue
  s=r.get('secondary_metrics',{});current=s.get('allocated_bytes');peak=s.get('peak_allocated_bytes')
  if current and peak:
   pool_memory.append([f"{r['scene']} / {r['resolution']}",num(current['base_median']/2**20,2),num(current['candidate_median']/2**20,2),num(peak['base_median']/2**20,2),num(peak['candidate_median']/2**20,2)])
 lines+=table(['조건','재사용 OFF 최종 MiB','재사용 ON 최종 MiB','OFF peak MiB','ON peak MiB'],pool_memory)
 pool_reuse=[]
 for r in comparisons:
  if r['comparison']!='buffer_pool':continue
  s=r.get('secondary_metrics',{});count=s.get('reload_reuse_count');reused=s.get('reload_reused_bytes');retained=s.get('reload_retained_bytes')
  if count and reused and retained:
   pool_reuse.append([f"{r['scene']} / {r['resolution']}",num(count['base_median'],0),num(count['candidate_median'],0),num(reused['candidate_median']/2**20,2),num(retained['candidate_median']/2**20,2)])
 lines+=table(['조건','OFF 누적 재사용 횟수','ON 누적 재사용 횟수','ON 누적 재사용 요청 MiB','ON 마지막 pool 보관 요청 MiB'],pool_reuse)
 lines+=['누적 재사용 용량은 같은 버퍼를 여러 번 재사용한 양의 합이며 메모리 절감량이 아니다. pool 보관량은 버퍼 요청 크기의 합으로, 위 Vulkan allocation 크기와 정의가 다르다. 첫 재생성에서 반납한 버퍼가 다음 호출부터 재사용되므로 5회 전체의 누적 수치로 기록했다.','']
 lines+=['이번 pool 실험은 메모리 절감으로 해석할 수 없다. 네 조건 모두 최종 할당량이 늘었고, Mountain에서는 자원 재생성 시간이 줄었지만 BrickCube에서는 이득이 불확실하거나 증가했다. pool의 대기 자원과 미선택 해상도 메시 캐시가 같은 버퍼를 가리킬 수 있으므로 두 숫자를 무조건 합치거나 전체에서 이중으로 빼지 않는다. 메모리 UI는 allocation마다 한 번만 분류하도록 소유 상태를 추가해 `Scene 참조 / pool 대기 / 기타 공유 자원`을 먼저 나누고, Scene 참조 안에서 선택 메시와 미선택 해상도 캐시를 구분하는 방식이 적절하다. 현재 Scene이 참조한다는 것과 매 프레임 GPU가 실제로 읽었다는 것은 다르다.','']
 lines+=['### 짧은 전달 계산의 정확성 검사','',
         '초기 입력을 넣은 뒤 전달 계산 한 번만 수행했을 때의 차이다. 긴 실행의 차이와 구분한다. 완전 일치는 이 입력·이 장치·이 비교 범위에서만 확인한 것이며 모든 Profile과 상태에 대한 증명은 아니다.','']
 short_rows=[]
 for r in short.get('comparisons',[]):
  name=r['comparison'];q=r['quality']
  if name=='shared_profiles':continue
  if name not in QUESTIONS:continue
  l1=q.get('state_l1_relative')
  tag=f' ({name})' if name.startswith(('mesh','tile')) else ' (SSBO)' if name=='render_math_reuse_ssbo' else ' (기본)' if name=='render_math_reuse' else ''
  short_rows.append([QUESTIONS[name][0]+tag,f"{r['scene']} / {r['resolution']}",
                     '일치' if q.get('state_exact') else '차이',f'{l1:.3e}' if l1 is not None else '—',num(q.get('state_max_absolute'),8)])
 lines+=table(['PERF ID','조건','State 완전 일치','상대 L1','최대 절대 차이'],short_rows)
 review_path=RESULTS/FOLDERS['main']/'raw/quality-review/review-inputs.json'
 if review_path.exists() and load(review_path).get('visual_review_status')=='reviewed':
  lines+=['### 캡처 화면 직접 확인','',
          '같은 662 step의 원본 viewport 픽셀을 나란히 확인했다. 아래 세 비교는 최종 시뮬레이션 State 기록이 같음을 별도로 확인했다. 차이 패널은 육안 확인을 위해 절대 RGB 차이를 4배 또는 16배로 표시했으며, 원본 화면은 확대 보정하지 않았다. 그림만으로 FP16의 수치 정확성이나 필터의 정답을 판정하지 않는다.','']
  lines+=table(['비교','관찰','같은 시점 화면과 차이'],[
   ['BrickCube / simulation 512 / render 512·128·64','메시를 줄이면 적층 경계뿐 아니라 벽돌 표면의 표시·조명도 달라짐',f"[원본·차이 ×4]({pathlink('main','raw/quality-review/brickcube-512-lod.png')})"],
   ['Mountain / simulation 512 / render 512·128·64','적층 경계와 흘러내리는 윤곽에 차이가 모임. 화면 전체 평균만으로 국소 차이를 무시하지 않음',f"[원본·차이 ×4]({pathlink('main','raw/quality-review/mountain-512-lod.png')})"],
   ['BrickCube / 256 / 필터 사전 계산 ON·OFF','상태는 같지만 적층 경계 주변 표시가 일부 다름. 어느 필터가 정답인지는 별도 문제',f"[원본·차이 ×16]({pathlink('main','raw/quality-review/brickcube-256-filter.png')})"]])
  lines+=[f"[그림에 사용한 실행 ID·상태 일치·검토 기록]({pathlink('main','raw/quality-review/review-inputs.json')}).",'']
 lines+=['### PERF-034 — 개발 단계의 보조 비교','',
         'V0~V4의 공통 현재 에셋·카메라·해상도·고정 dt·Geometry feedback ON·높이 smoothing ON을 사용했다. 그러나 전처리, 이동/적층 수식, 표시 방식이 달라 **같은 알고리즘의 순수 개선률로 환산하지 않는다**. 상태/화면 값도 현재 V4와의 차이이지 기준 정답에 대한 오차가 아니다. 과거 버전의 texel 배치까지 동일함을 입증한 것은 아니므로, State 값은 같은 인덱스 배열의 차이로만 해석한다.','',
         '구버전에는 공통 계측·입력 설정과 Windows 빌드 호환 수정을 적용했다. 원본 commit 그대로 실행한 결과가 아니며 실제 수정 patch·실행 파일·빌드 로그는 history/raw/inputs에 보관했다.','',
         'V2 `afe1c4f`는 빌드에 성공했지만 Windows 앱 제어가 실행을 차단했다(WinError 4551). 차단을 우회하지 않았고 표에 수치를 넣지 않았다.','']
 hist_rows=[]
 for r in history.get('rows',[]):
  t=r['metrics'];q=r.get('difference_from_V4',{})
  hist_rows.append([r['version'],f"{r['scene']} / {r['resolution']}",
                   num(t['frame_gpu_elapsed_ms']['median'])+' / '+num(t['frame_gpu_elapsed_ms']['p95']),
                   num(t['simulation_gpu_elapsed_ms']['median']),num(t['render_gpu_ms']['median']),
                   num(q.get('state_l1_relative')*100 if q.get('state_l1_relative') is not None else None,4),num(q.get('image_mae_u8'))])
 hist_rows.append(['V2','모든 계획 조건','실행 차단','—','—','—','—'])
 lines+=table(['버전','조건','전체 GPU 중앙값 / p95 ms','Simulation ms','Render ms','V4와 State L1 차이 %','V4와 화면 MAE'],hist_rows)
 lines+=['### 실패·미지원·해석 제한','']
 failures=[]
 for key,m in manifests.items():
  previous=m.get('previous_attempts',[])
  if previous:
   failures.append([key,f'이전 실패 시도 {len(previous)}회','실험용 Scene 경로 오류로 시작 전 종료. 상대 경로 수정 후 재측정했으며 실패 로그·manifest는 raw/failed-attempts와 inputs에 보관. 아래 최종 완료 수에 포함하지 않음'])
  for run in m['runs']:
   if run['status']!='complete':failures.append([key,run['id'],run.get('error',run['status'])])
 failures+=[['개발 단계','V2 afe1c4f','Windows Application Control 실행 차단 — 미측정'],
            ['PERF-029','회전 불변 CPU 캐시','실험용 addon 실행 파일의 Windows Application Control 차단 — 미측정'],
            ['사전 pilot','4회 — 본 집계 제외','마지막 화면에 Profiling UI가 포함돼 화질 비교용으로 부적절했다. capture를 수정한 뒤 본 비교를 새로 실행. 초기 수치가 불리해서 제외한 것이 아님']]
 failures.append(['최초 검증','로딩 60회의 화면 영역 검사','서로 다른 시작 규칙의 viewport까지 같아야 한다는 검사 가정이 잘못됐다. 카메라는 모든 조건에서 확인하고 viewport는 같은 준비/측정 규칙끼리 비교하도록 수정. 로딩 화면 수치는 품질 판단에서 제외; 원래 검사 결과도 보관'])
 lines+=table(['단계','항목','결과'],failures)
 lines+=['- GPU/CPU 시간은 해당 계측 구간만 나타낸다. 전체 command-buffer GPU 경과 시간도 acquire·present·CPU 대기까지 포함한 최종 사용자 FPS와 같지 않다.',
         '- 연속된 600프레임은 상태가 진화하는 구간이다. 특히 정밀도·반올림·dirty tolerance가 분포를 바꾸면 시간 차이에 작업량 차이도 포함된다. 단일 연산의 대역폭 효과로만 주장하지 않는다.',
         '- Frame fence 대기는 이미 제출한 GPU 작업을 기다리는 시간이다. GPU 시간에 다시 더하지 않는다. CPU frame 보조 자료와 command recording은 원자료에 남긴다.',
         '- 과거 V0/V1/V3 exporter는 최종 step serial·전체 CPU frame time·메모리 분류를 출력하지 않는다. 제출 프레임 수와 측정 구간의 step 수를 확인하되, 미지원 항목을 현재 버전처럼 검증했다고 주장하지 않는다.',
         '- 상태와 화면을 따로 확인했다. 화면이 같아 보여도 상태가 다를 수 있고, viewport 평균 화면 차이는 작은 경계 오류를 희석할 수 있다.',
         '- NaN/음수·반복 재현·경로 간 상태 차이를 확인했지만, 모든 물리 항의 분석적 정답이나 입력/감쇠를 포함한 보존식 잔차를 검증한 것은 아니다. 여러 채널의 상대 L1은 배열 전체 차이이며 서로 다른 단위를 가진 채널의 물리적 총량 보존을 뜻하지 않는다.',
         '- 이 결과는 Radeon RX 6700 XT와 고정된 대표 입력에 한정한다. GPU 온도·클록·다른 앱 부하를 별도로 통제/기록하지 않았다. 작은 차이는 반복 편차를 함께 보고 판단한다.',
         '- 제품 기본 옵션을 변경하거나 새 최적화를 배포한 작업이 아니다. 감소가 관찰돼도 결과 차이·조건 의존성이 있으면 옵션을 유지하고 조건부 결과로 기록한다.','']
 lines+=['### 원자료·코드·반복 범위','']
 artifacts=[]
 for key,m in manifests.items():
  artifacts.append([key,m.get('created_at','—'),m.get('finished_at','진행 중'),
                    f"[manifest]({pathlink(key,'manifest.json')})",f"[원자료·입력 보관본]({pathlink(key,'raw/')})"])
 lines+=table(['단계','시작 KST','종료 KST','실행 기록','프레임·상태·화면·설정'],artifacts)
 lines += [f"- [전체 p95·반복 중앙값 범위 표]({pathlink('main','timing-details.md')}) — 각 비교의 3회 값과 최소~최대 범위를 포함한다.",
           f"- [전체 실행·조건·상태·파일·실행 파일 독립 검증]({pathlink('main','study-audit.json')}) — 최종 보고서는 이 검사가 통과한 뒤 생성한다.",
           f"- [최초 startup viewport 검사의 진단 기록]({pathlink('main','study-audit-initial-startup-viewport.json')}) — 위 수정 이유를 보존한다.",
           f"- [현재 옵션 상세 집계]({pathlink('main','comparisons.json')}), [추가 workload 상세 집계]({pathlink('extra','comparisons.json')}), [한 번 전달 검사]({pathlink('short','validation-summary.json')}), [개발 단계 요약]({pathlink('history','history-summary.json')}).",
           '- [제외한 사전 pilot 실행 기록](Performance-Results/20261022-optimization-suite/manifest.json), [실험용 Scene 경로 수정 기록](Performance-Results/20261022-workload-supplement/raw/inputs/scenario-path-correction.json).',
           '- 각 manifest에 실제 시각, 명령, 종료 코드, 표본 수, 실행 파일 SHA-256이 있다. main/raw/inputs에 고정 소스·에셋 보관본, 실행 파일, patch, 입력 해시와 빌드 로그를 보관했다. 추가 workload의 Scene/복원 셰이더와 역사 버전 patch도 각 inputs에 있다.',
           '- 미커밋 변경이 포함돼 있으므로 commit만으로 재현되는 결과가 아니다. 같은 소스/에셋/설정 보관본과 도구 버전으로 복원해야 한다.',
           '- **raw는 Git ignore 대상이며 현재 PC에 저장돼 있다. Git push만으로 백업되지 않는다.** 장기 보관 시 결과 폴더 전체를 함께 보관해야 한다.','']
 details=['# 전체 비교의 p95와 반복 편차','','대표값은 3회 실행별 중앙값/p95의 중앙값이다. 배열은 실행 3회의 중앙값이다.','']
 detail_rows=[]
 for r in comparisons:
  if r.get('historical_base_valid') is False:continue
  qid=QUESTIONS[r['comparison']][0]
  detail_rows.append([qid,r['comparison'],f"{r['scene']} / {r['resolution']}",r['metric'],
    num(r['base_p95']),num(r['candidate_p95']),', '.join(num(x) for x in r['base_repeat_medians']),', '.join(num(x) for x in r['candidate_repeat_medians']),
    num(r['base_range'][0])+'~'+num(r['base_range'][1]),num(r['candidate_range'][0])+'~'+num(r['candidate_range'][1])])
 details+=table(['ID','비교','조건','지표','기준 p95','개선 p95','기준 3회 중앙값','개선 3회 중앙값','기준 범위','개선 범위'],detail_rows)
 (RESULTS/FOLDERS['main']/'timing-details.md').write_text('\n'.join(details)+'\n',encoding='utf8')
 text=DOC.read_text(encoding='utf8');end=text.index('## 과거 최적화 기록 복원')
 if '## PERF-002 — 8개 묶음' in text:start=text.index('## PERF-002 — 8개 묶음')
 elif '## 8개 묶음 최적화 비교 — PERF-002~034' in text:start=text.index('## 8개 묶음 최적화 비교 — PERF-002~034')
 elif '## 8개 묶음 최적화 비교 — PERF-002~035' in text:start=text.index('## 8개 묶음 최적화 비교 — PERF-002~035')
 else:start=end
 text=text[:start]+'\n'.join(lines)+'\n\n'+text[end:]
 top=overview(comparisons,complete)
 begin='<!-- PERFORMANCE_STUDY_OVERVIEW_START -->';finish='<!-- PERFORMANCE_STUDY_OVERVIEW_END -->'
 if begin in text:
  start=text.index(begin);end=text.index(finish,start)+len(finish)
  text=text[:start]+top+text[end:]
 else:
  start=text.index('## PERF-001')
  text=text[:start]+top+'\n'+text[start:]
 if complete:
  replacements={
   '- 상태: **Git 이력·기존 자료의 1차 목록 확인 완료 / 8개 묶음의 대조 재측정 미실행**. 현재 코드의 기준 측정은 위 `PERF-001`에 기록했다. 아래 목록은 전체 최적화 감사나 개별 성능 검증을 완료했다는 뜻이 아니다.':
   '- 상태 갱신 (2026-10-23): **주요 비교쌍의 사후 재측정 종료**. 현재 코드 기준은 `PERF-001`, 실제 비교 결과·실행 제한·미분리 항목은 위 `PERF-002~035`에 기록했다. 아래는 2026-10-22에 작성한 복원 목록이며, 모든 작은 변경의 개별 효과까지 측정했다는 뜻은 아니다.',
   '구현일은 Git commit 기준이며 측정일이 아니다. 모든 항목의 새 기준 전후 수치는 현재 **미측정**이다. 실제 비교를 시작할 때 실험 양식의 ID·코드 버전·조건·실행 시각을 채운다.':
   '구현일은 Git commit 기준이며 측정일이 아니다. 아래는 2026-10-22에 작성한 대상 목록이다. 이후 실제 수행한 비교는 위 `PERF-002~035`에 연결했으며, 개별 효과를 분리하지 않은 항목은 측정 범위 표에 명시했다.',
   '- 상태: **Git 주요 이력·일부 핵심 diff를 바탕으로 한 실험 구성안 / 빌드·실측·출력 동등성 미검증**.':
   '- 문서 성격: **2026-10-22 당시 실험 구성안**. 이후 빌드·실행·출력 대조 결과는 위 `PERF-002~035`에 기록했다. 아래 base 선정 원칙과 실제 완료/미측정 범위를 구분한다.',
   '- 현재 benchmark는 시작 전에 이 값을 ON으로 명시하고 `*.settings.json`에 기록한다 (`PERF-001`에서 추가). 구버전에는 같은 설정 적용·출력을 위한 실험용 patch가 필요하다. 구버전 실행 코드는 아직 수정하거나 측정하지 않았다.':
   '- 현재 benchmark는 시작 전에 이 값을 ON으로 명시하고 `*.settings.json`에 기록한다 (`PERF-001`에서 추가). 이후 V0·V1·V3에도 실험용 patch로 같은 ON 설정과 출력을 적용해 측정했다 (`PERF-034`). V2는 빌드했지만 Windows 앱 제어가 실행을 차단해 미측정이다.',
  }
  for old,new in replacements.items():text=text.replace(old,new)
  marker='| 항목 | 현재 자동 출력 | 기록 방법 |'
  clarification='아래 표의 기본 제품 출력과 구분: `PERF-002~035`의 **실험용 빌드**에서는 공통 command-buffer/simulation GPU 경과 시간, 전체 CPU frame 경과 시간, f32 상태 배열과 BGRA 화면 출력을 추가했다. 보관한 계측 patch·실행 파일을 사용한 결과이며, 현재 루트의 기본 제품 빌드가 이 추가 항목을 모두 제공한다는 뜻은 아니다.'
  if clarification not in text:text=text.replace(marker,clarification+'\n\n'+marker)
 DOC.write_text(text,encoding='utf8')
 print('Updated central study report:',len(comparisons),'case comparisons; execution phases finished:',complete)

if __name__=='__main__':main()
