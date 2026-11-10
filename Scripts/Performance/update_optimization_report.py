"""최적화 실험 집계를 현재 성능 보고서에 반영한다.

Update one experiment section in the existing report from preserved run files.
"""

from __future__ import annotations
import argparse, datetime as dt, json, os
from pathlib import Path
from optimization_round import ROOT, RESULT, LAB
from analyze_optimization_round import analyze

REPORT = ROOT / 'Docs/Agent-Vault/05_Experiments/02_Performance-Measurements.md'
DESCRIPTIONS = {
    'PERF028': (
        'PERF-028 후속',
        'Solver shader 실행 중 분기 복구',
        '컴파일 시점 고정 → 실행 중 flags 분기',
    ),
    'PERF007': (
        'PERF-007 후속',
        'Solver 부분 계산 기본 정책',
        'Sparse Solver ON → OFF; Geometry/height 부분 계산·채널 마스크 유지',
    ),
    'PERF036': (
        'PERF-036',
        '저장 형식에 맞춘 scratch 할당',
        '최대 FP32 용량 → 같은 FP16 값을 저장하는 데 필요한 용량',
    ),
    'PERF036-formats': (
        'PERF-036 보충',
        'FP32·FP16 weights·cache OFF의 할당',
        '동일한 계산 옵션에서 최대 용량 → 실제 필요 용량',
    ),
    'PERF037': (
        'PERF-037',
        'GPU 메시 해상도 캐시 지연 생성',
        '전 해상도 생성 → 시뮬레이션 해상도 메시 + 요청한 해상도만 생성',
    ),
    'PERF037-lowmesh': (
        'PERF-037 보충',
        '64 메시 선택 시 캐시 할당',
        '같은 64 메시에서 모든 해상도 사전 생성 → 필요한 해상도만 생성',
    ),
    'PERF038': (
        'PERF-038',
        '넓은 영역의 Geometry 갱신',
        '항상 부분 목록 → 높이 후보 workgroup 비율에 따른 전체/부분 선택',
    ),
    'PERF039': ('PERF-039', '인스턴스 실행 순서 선택', '항상 단계별 batch → 순차/자동 선택'),
    'PERF040': (
        'PERF-040',
        'Overlay 필터 전체/부분 선택',
        '항상 tile 필터 → 이전 activity에 따른 전체/부분 선택',
    ),
}


def link(path):
    if path.is_relative_to(LAB):
        relative = path.relative_to(LAB)
        preserved = (
            RESULT
            / 'raw/inputs'
            / ('stages' if relative.parts[0] in {'S00', 'S01', 'S02', 'S03', 'S04'} else '')
            / relative
        )
        if preserved.exists():
            path = preserved
    return os.path.relpath(path, REPORT.parent).replace('\\', '/')


def update(name, decision, notes):
    folder = RESULT / name
    data = analyze(folder)
    manifest = json.loads((folder / 'manifest.json').read_text())
    if manifest['status'] != 'complete' or data['issues']:
        raise ValueError('Cannot finalize incomplete/invalid experiment')
    id, title, change = DESCRIPTIONS.get(name, (name, name, name))
    stages = sorted({Path(r['binary']).parent.name for r in manifest['runs']})
    lines = [
        f'### {id} — {title} (2026-10-25)',
        '',
        f'- 기록 유형: **구현 당시 측정**. 상태: **완료 — {data["runs"]}회 / {data["measured_frames"]:,}개 측정 프레임**.',
        f'- 질문·변경 한 가지: {change}.',
        f'- 실행 일시: {min(r["started_at"] for r in manifest["runs"])} ~ {max(r["finished_at"] for r in manifest["runs"])} (KST).',
        '- 코드: `2b115ff` 이후 미커밋 변경. commit 단독 결과가 아니며 아래 단계별 patch·소스·SPIR-V·실행 파일 해시를 보관했다.',
        '- 비교 조건: 씬·입력·카메라·실제 viewport·해상도·나머지 옵션 고정. 새 프로세스, fixed dt=1/60, feedback ON, 60 준비 + 600 측정 + 2 drain, 조건별 3회. 반복 2에서는 arm 순서를 반대로 실행했다.',
        '- 측정 범위: simulation은 공통 GPU timestamp 21→22, 전체 command buffer는 21→23. CPU loop는 별도 `.cpu.json`. 상위/하위 구간을 합산하지 않는다. 전체 표시 FPS·VRAM 상주량은 측정하지 않았다.',
        '',
        '| 조건 | Simulation 중앙값 전 → 후 (ms) | Simulation p95 전 → 후 (ms) | 시간 감소 % | 할당 전 → 후 (MiB) | State 상대 L1 % |',
        '|---|---:|---:|---:|---:|---:|',
    ]
    for c in data['comparisons']:
        b, a = c['arms']['before'], c['arms']['after']
        m = 'simulation_gpu_elapsed_ms'
        reduction = (b[m]['median'] - a[m]['median']) / b[m]['median'] * 100
        label = Path(c['scene']).stem + f' / {c["resolution"]}'
        if c['experiment'] != name:
            label = c['experiment'] + ' / ' + label
        lines.append(
            f"| {label} | {b[m]['median']:.3f} → {a[m]['median']:.3f} | {b[m]['p95']:.3f} → {a[m]['p95']:.3f} | {reduction:.2f} | {b['allocated_mib']['median']:.2f} → {a['allocated_mib']['median']:.2f} | {c['state']['relative_l1']*100:.6f} |"
        )
    lines += [
        '',
        '| 조건 | Simulation 반복 중앙값 범위 전 → 후 (ms) | Render 중앙값 전 → 후 (ms) | 최대 할당 전 → 후 (MiB) |',
        '|---|---:|---:|---:|',
    ]
    for c in data['comparisons']:
        b, a = c['arms']['before'], c['arms']['after']
        br = b['simulation_gpu_elapsed_ms']['range']
        ar = a['simulation_gpu_elapsed_ms']['range']
        label = Path(c['scene']).stem + f' / {c["resolution"]}'
        if c['experiment'] != name:
            label = c['experiment'] + ' / ' + label
        lines.append(
            f"| {label} | {br[0]:.3f}–{br[1]:.3f} → {ar[0]:.3f}–{ar[1]:.3f} | {b['render_gpu_ms']['median']:.3f} → {a['render_gpu_ms']['median']:.3f} | {b['peak_mib']['median']:.2f} → {a['peak_mib']['median']:.2f} |"
        )
    if name == 'PERF038':
        lines += [
            '',
            '| 조건 | Geometry 전 → 후 (ms) | Transfer Weight 전 → 후 (ms) | Pass 1 전 → 후 (ms) | Pass 2 전 → 후 (ms) |',
            '|---|---:|---:|---:|---:|',
        ]
        for c in data['comparisons']:
            b, a = c['arms']['before'], c['arms']['after']
            values = [
                f"{b[m]['median']:.3f} → {a[m]['median']:.3f}"
                for m in [
                    'geometry_gpu_ms',
                    'transfer_weight_gpu_ms',
                    'pass1_gpu_ms',
                    'pass2_gpu_ms',
                ]
            ]
            lines.append(
                '| '
                + Path(c['scene']).stem
                + f' / {c["resolution"]} | '
                + ' | '.join(values)
                + ' |'
            )
        lines += [
            '',
            '하위 구간의 중앙값은 전체 simulation elapsed와 더하지 않는다. Geometry 목록 구성·height 처리·구간 사이 동기화는 전체 elapsed에는 포함되지만 위 네 구간에 모두 분리되어 있지는 않다.',
        ]
    exact = all(c['state']['exact'] for c in data['comparisons'])
    lines += [
        '',
        f'- 정확성: 최종 {manifest["runs"][0]["warmup"]+manifest["runs"][0]["measure"]+2} step의 State 배열 비교. 전후 완전 일치: **{"모든 조건 일치" if exact else "차이 있는 조건 존재"}**. NaN/Inf·음수 0, 같은 arm 반복 State 해시 일치. 물리적 총량 보존식·화면 픽셀 검증과는 구분한다.',
        f'- 판단: **{decision}**. {notes}',
        '- 한계: RX 6700 XT 한 장치의 결과. GPU 클록·온도·외부 부하는 별도 계측하지 않았다. 반복 범위가 겹치는 작은 차이는 확정적 개선으로 해석하지 않는다. 이미지 비교는 별도 지원 여부를 표시한다.',
        f'- 원자료: [manifest]({link(folder/"manifest.json")}), [전후 집계·p95·범위]({link(folder/"comparisons.json")}), [프레임·설정·State 배열]({link(folder/"raw")}).',
    ]
    lines.append(
        '- 코드 보관본: ' + ', '.join(f'[{s}]({link(LAB/s/"snapshot.json")})' for s in stages) + '.'
    )
    text = '\n'.join(lines) + '\n'
    start = f'<!-- OPT_ROUND_{name}_START -->'
    end = f'<!-- OPT_ROUND_{name}_END -->'
    source = REPORT.read_text(encoding='utf8')
    block = start + '\n' + text + end
    if start in source:
        i = source.index(start)
        j = source.index(end, i) + len(end)
        source = source[:i] + block + source[j:]
    else:
        source += '\n\n' + block + '\n'
    REPORT.write_text(source, encoding='utf8')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('name')
    parser.add_argument('--decision', required=True)
    parser.add_argument('--notes', default='')
    args = parser.parse_args()
    update(args.name, args.decision, args.notes)
