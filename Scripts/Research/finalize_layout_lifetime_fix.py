"""Publish correctness evidence without turning a resource-lifetime repair into research novelty."""
import pathlib, re, shutil
from authoring_study import ROOT, STUDY, read, write, stamp, digest

OUT=STUDY/'layout-lifetime-fix';EXP=OUT/'render'


def main():
    before=read(OUT/'before.json');after=read(OUT/'after.json');gpu=read(OUT/'gpu.json')
    analysis=read(EXP/'analysis.json');mf=read(EXP/'manifest.json');comparison=read(OUT/'before-after-state.json')
    change=read(OUT/'source-change-audit.json')
    if before['exit_code']!=1 or after['status']!='passed' or gpu['status']!='passed' or analysis['audit_errors'] or not change['matches_expected']:
        raise RuntimeError('Unresolved correctness gate')
    source='Source/SurfaceState/GPU/Resources/DescriptorResources.cpp'
    old=(OUT/'raw/before/inputs'/source).read_text(encoding='utf-8');new=(ROOT/source).read_text(encoding='utf-8')
    marker='        const std::array<const GPU::TGPUBuffer*, DescriptorBindingCount> SharedAndProfileBuffers'
    end='    TSurfaceStateDescriptorResources::~TSurfaceStateDescriptorResources()'
    unchanged=old[old.index(marker):old.index(end)]==new[new.index(marker):new.index(end)]
    errors=[]
    for name,entry,folder in [('before',before,OUT/'raw/before/inputs'),('after',after,OUT/'raw/after/inputs'),('gpu',gpu,OUT/'raw/gpu/inputs')]:
        for p,h in entry['source_hashes'].items():
            if digest(folder/p)!=h:errors.append(name+': '+p)
    if errors or not unchanged:raise RuntimeError('Source/archive audit requires review')
    summary=dict(created_at=stamp(),status='correctness_fix_validated',before_targeted_test='failed_first_storage_switch',
        after_targeted_test='passed_12_switches',after_targeted_vulkan_errors=after['vulkan_error_lines'],
        surface_gpu_resource='passed',rendered_conditions=len(analysis['runs']),
        accepted_state_steps=sum(r['simulation_step_serial'] for r in analysis['runs']),
        rendered_vulkan_error_lines=sum(r['runtime_error_count'] for r in analysis['runs']),
        capture_audit_errors=analysis['audit_errors'],archive_errors=errors,
        before_after_state_arrays=len(comparison['arrays']),all_state_arrays_bit_equal=comparison['all_bit_equal'],
        descriptor_write_body_unchanged=unchanged,source_changes=change['changed_source_files'],
        shader_changes=[],whole_repository_suite='not_run',human_sessions=0,performance_comparison='not_performed',
        research_interpretation='Ordinary Vulkan resource lifetime correctness repair; no algorithmic contribution or productivity evidence')
    write(OUT/'summary.json',summary)
    current=dict(updated_at=stamp(),status='layout_lifetime_fixed_reference_capture_gates_passed_human_study_not_ready',
        geometry_test='passed_on_previous_policy_retry',scratch_lifetime_regression='passed_12_switches',surface_gpu_resource='passed',
        render_conditions_observed=len(analysis['runs']),reference_render_validations_passed=len(analysis['runs']),
        render_acceptance='passed_scoped_functional_gates',human_sessions=0,human_productivity='not_measured',
        independent_quality_gate='not_validated',full_material_ledger='not_collected_in_app_capture',
        earlier_failures_preserved=True,sources=dict(summary='layout-lifetime-fix/summary.json',analysis='layout-lifetime-fix/render/analysis.json',
            before_after='layout-lifetime-fix/before-after-state.json'))
    write(STUDY/'readiness-current.json',current)
    study=read(STUDY/'study.json');study.update(updated_at=stamp(),status=current['status'])
    study['experiments']=[x for x in study['experiments'] if x['id']!='descriptor-lifetime-correctness-fix']+[dict(
        id='descriptor-lifetime-correctness-fix',status='validated_targeted_gpu_and_6_renders',path='layout-lifetime-fix/')]
    write(STUDY/'study.json',study)
    prefix='../../../Performance-Data/20261027-authoring-readiness/layout-lifetime-fix/'
    text=f'''\n## 8. Descriptor layout 수명 수정·실제 재검증 — 2026-10-27\n\n사용자의 후속 진행 승인으로 GPU 자원 수명 오류를 수정했다. `TSurfaceStateDescriptorResources::RebindInstanceBuffers`는 기존 25-binding의 AB/BA descriptor set에 buffer handle/range만 다시 쓴다. `ConfigureScratchStorage`는 종전과 같이 device idle을 기다리고, layout/pool/set을 교체하지 않는다. Solver·Renderer의 기존 layout 참조를 유지하며 recorded command는 다음 frame에서 reset·record한다. 기존 buffer-write 본문은 소스상 동일하고 State·반응·수송·Geometry·셰이더 식을 바꾸지 않았다.\n\n- **변경 전:** 새 `--scratch-layout-only` 회귀가 첫 저장 형식 전환에서 stable layout/set 조건에 실패했다(exit 1). 최초 빌드가 끝나기 전 캡처 호출 1회는 실행 전 실패로 명세에 기록했다. [변경 전 결과]({prefix}before.json).\n- **변경 후:** 같은 검사에서 12회 정밀도/cache/compact 전환, 즉시 State 보존, layout/AB/BA handle 유지, 실제 compute/render 및 종료 시 Vulkan 오류 0을 확인했다. 기존 `MDSS_SurfaceGPUResourceTests` 전체 실행도 통과했다. 전체 저장소 CTest suite는 미실시다. [변경 후]({prefix}after.json), [GPU 회귀]({prefix}gpu.json).\n- **실제 앱 재검증:** `{mf['started_at']}` ~ `{mf['finished_at']}` (KST), Release exe SHA-256 `{mf['executable_sha256']}`. 원래 6조건의 scene/profile/입력/카메라·128 해상도·FP32 Dense·fixed 1/60을 유지했다. **6조건·{summary['accepted_state_steps']:,} Step, Vulkan 오류 0, 최종 State/Reaction flag 및 frame/image/settings/자산 경로/보관 감사 오류 0**. Reaction flag는 최종 scratch 값이며 전 Step 장부를 수집한 것은 아니다. [실행 manifest]({prefix}render/manifest.json), [조건별 분석]({prefix}render/analysis.json).\n- **수정 전후 관측 비교:** 저장 State 배열 **{summary['before_after_state_arrays']}개, 전부 비트 일치={comparison['all_bit_equal']}**. 이전 실행은 Vulkan 오류가 있었으므로 ground truth가 아니다. 이번 비교는 수명 수정으로 수치 경로가 달라졌는지 보는 일관성 검사다. GLSL/SPIR-V 변경 0, 제품 소스 변경은 descriptor h/cpp 및 resource manager cpp의 3개로 한정했다. [수치 대조]({prefix}before-after-state.json), [소스 변경 감사]({prefix}source-change-audit.json).\n- **판정:** 해당 수명 오류는 해결됐고 제한된 6조건의 기능 검증을 통과했다. 상태 이름 대응과 새 프로세스 반복의 수치 일치, 임계값/회전의 분포 변화는 새 정상 실행으로 관측했다. 화면의 bit 일치 여부·차이는 실제 분석 표를 따르며 임의로 동일하다고 가정하지 않는다.\n- **한계 유지:** 약 6초·같은 mesh이며 이미지 frame 359/최종 State Step 362의 시차가 있다. 독립 예술 품질 gate, 인간 제작 시간/성공률, 새 Mesh 일반화, 전 시간 물질 수지, GPU 성능 우위는 검증하지 않았다. 일반적인 GPU 자원 수명 수정은 새로운 연구 Contribution이 아니다. U/M 제작 비교 No-Go 및 현 시스템 기여 미확보 판정도 유지한다. 이전 오류·누락·차단 결과를 새 성공으로 덮어쓰지 않는다.\n\n![수명 수정 후 6조건의 기능 검증]({prefix}render/figures/reference-sheet.png)\n'''
    report=ROOT/'Docs/MDSSP-Agent-Vault/03_Development/06_Authoring-Study-Protocol.md'
    old=report.read_text(encoding='utf-8');marker='\n## 8. Descriptor layout 수명 수정'
    if marker in old:old=old[:old.index(marker)]
    old=re.sub(r'^> .*$', '> **현재 판정:** Descriptor layout 수명 오류를 수정했고, 12회 형식 전환 회귀·기존 GPU 테스트·실제 화면 6조건이 해당 검증을 통과했다(§8). 이전 실패는 보존했다. 인간 제작 비교와 학술적 기여는 여전히 미입증이다.',old,count=1,flags=re.MULTILINE)
    report.write_text(old+text,encoding='utf-8')
    archive_folder=OUT/'raw/final-analysis'
    files=[pathlib.Path(__file__).resolve(),ROOT/'Scripts/Research/layout_lifetime_validation.py',
        ROOT/'Scripts/Research/analyze_layout_lifetime_fix.py',ROOT/'Scripts/Research/analyze_authoring_retry.py',report,
        ROOT/'CMakeLists.txt',ROOT/'Tests/SceneResourceTests.cpp',
        ROOT/'Docs/MDSSP-Agent-Vault/04_Experiments/01_Validation-and-Tests.md',
        ROOT/'Docs/MDSSP-Agent-Vault/02_Architecture/02_Contracts/04_GPU-Layout.md',
        ROOT/'Docs/MDSSP-Agent-Vault/03_Development/01_Current-Work.md',
        OUT/'summary.json',OUT/'before-after-state.json',OUT/'source-change-audit.json',STUDY/'readiness-current.json']
    hashes={}
    for p in files:
        rel=p.relative_to(ROOT);dest=archive_folder/rel;dest.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2('\\\\?\\'+str(p.resolve()),'\\\\?\\'+str(dest.resolve()));hashes[str(rel)]=digest(p)
        if digest(dest)!=hashes[str(rel)]:raise RuntimeError('Final archive mismatch')
    write(OUT/'final-analysis-archive.json',dict(created_at=stamp(),scope='Post-execution interpretation, separate from pre-run inputs',
        archive=str(archive_folder.relative_to(ROOT)),sha256=hashes))
    print('Layout lifetime fix validated:',summary['rendered_conditions'],'render cases;',summary['rendered_vulkan_error_lines'],'Vulkan errors')


if __name__=='__main__':main()
