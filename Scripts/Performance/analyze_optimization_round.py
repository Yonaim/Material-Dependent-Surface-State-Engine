"""최적화 후보별 반복 측정에서 정확성·시간·메모리 차이를 분석한다.

Validate and tabulate the 2026-10-25 paired optimization measurements.
"""

from __future__ import annotations
import argparse, datetime as dt, json, statistics as st, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'Build/PerformanceLab/PythonPackages'))
import numpy as np
from optimization_round import RESULT, LAB, save

METRICS = [
    'simulation_gpu_elapsed_ms',
    'frame_gpu_elapsed_ms',
    'render_gpu_ms',
    'geometry_gpu_ms',
    'transfer_weight_gpu_ms',
    'pass1_gpu_ms',
    'pass2_gpu_ms',
    'command_record_cpu_ms',
]


def percentile(xs, q=0.95):
    return float(np.quantile(xs, q))


def analyze(folder):
    manifest = json.loads((folder / 'manifest.json').read_text(encoding='utf8'))
    groups = {}
    issues = []
    for run in manifest['runs']:
        if run['status'] != 'complete':
            issues.append(f"{run['id']}: {run.get('error')}")
            continue
        path = folder / run['raw']
        rows = [json.loads(s) for s in path.read_text().splitlines() if s.strip()]
        assert len(rows) == run['measure']
        assert [r['frame_index'] for r in rows] == list(
            range(run['warmup'], run['warmup'] + run['measure'])
        )
        assert all(r['simulation_steps'] == 1 for r in rows)
        assert all(
            np.isfinite(r[m]) and r[m] >= 0 for r in rows for m in METRICS
        ), 'Invalid timing sample'
        validation = json.loads(Path(str(path) + '.validation.json').read_text())
        assert validation['simulation_step_serial'] == run['warmup'] + run['measure'] + 2
        assert all(i['nonfinite'] == 0 and i['negative'] == 0 for i in validation['instances'])
        metric = {
            m: {'median': st.median([r[m] for r in rows]), 'p95': percentile([r[m] for r in rows])}
            for m in METRICS
        }
        metric['allocated_mib'] = {'median': validation['allocated_total_bytes'] / 1048576}
        metric['peak_mib'] = {'median': validation['peak_allocated_total_bytes'] / 1048576}
        cpu = json.loads(Path(str(path) + '.cpu.json').read_text())[
            run['warmup'] : run['warmup'] + run['measure']
        ]
        assert len(cpu) == run['measure'] and all(
            np.isfinite(v) and v >= 0 for v in cpu
        ), 'Invalid CPU timing sample'
        metric['frame_cpu_elapsed_ms'] = {'median': st.median(cpu), 'p95': percentile(cpu)}
        key = (run.get('experiment', folder.name), run['scene'], run['resolution'])
        groups.setdefault(key, {}).setdefault(run['arm'], []).append((run, metric, validation))
    comparisons = []
    for (experiment, scene, resolution), arms in groups.items():
        summary = {}
        for arm, records in arms.items():
            if len(records) != 3:
                issues.append(f'{scene}/{resolution}/{arm}: repeats={len(records)}')
            hashes = [tuple(i['state_fnv1a64'] for i in v['instances']) for _, _, v in records]
            if len(set(hashes)) != 1:
                issues.append(f'{scene}/{resolution}/{arm}: State differs across repeats')
            summary[arm] = {
                m: {
                    'median': st.median([metrics[m]['median'] for _, metrics, _ in records]),
                    'range': [
                        min(metrics[m]['median'] for _, metrics, _ in records),
                        max(metrics[m]['median'] for _, metrics, _ in records),
                    ],
                    'p95': st.median(
                        [metrics[m].get('p95', metrics[m]['median']) for _, metrics, _ in records]
                    ),
                }
                for m in records[0][1]
            }
        error = None
        if 'before' in arms and 'after' in arms:
            b, a = arms['before'][0], arms['after'][0]
            assert [i['instance'] for i in b[2]['instances']] == [
                i['instance'] for i in a[2]['instances']
            ], 'Instance mismatch'
            diff = den = 0.0
            maximum = 0.0
            equal = True
            for instance in b[2]['instances']:
                index = instance['instance']
                x = np.fromfile(
                    str(folder / b[0]['raw']) + f'.state-{index}.f32', dtype=np.float32
                ).astype(np.float64)
                y = np.fromfile(
                    str(folder / a[0]['raw']) + f'.state-{index}.f32', dtype=np.float32
                ).astype(np.float64)
                assert x.shape == y.shape
                d = np.abs(x - y)
                equal = equal and bool(np.array_equal(x.view(np.uint64), y.view(np.uint64)))
                diff += float(d.sum())
                den += float(np.abs(x).sum())
                maximum = max(maximum, float(d.max(initial=0)))
            error = {
                'exact': equal,
                'relative_l1': diff / den if den else (0.0 if not diff else None),
                'max_absolute': maximum,
            }
        comparisons.append(
            {
                'experiment': experiment,
                'scene': scene,
                'resolution': resolution,
                'arms': summary,
                'state': error,
            }
        )
    output = {
        'generated_at': dt.datetime.now().astimezone().isoformat(),
        'runs': len(manifest['runs']),
        'measured_frames': sum(r.get('samples', 0) for r in manifest['runs']),
        'issues': issues,
        'comparisons': comparisons,
    }
    save(folder / 'comparisons.json', output)
    lines = [
        '| 씬 / 해상도 | Simulation ms 전 → 후 | 감소 % | Render ms 전 → 후 | 할당 MiB 전 → 후 | State 상대 L1 % |',
        '|---|---:|---:|---:|---:|---:|',
    ]
    for c in comparisons:
        if 'after' not in c['arms']:
            continue
        b, a = c['arms']['before'], c['arms']['after']
        m = 'simulation_gpu_elapsed_ms'
        reduction = (b[m]['median'] - a[m]['median']) / b[m]['median'] * 100
        label = Path(c['scene']).stem + f" / {c['resolution']}"
        if c['experiment'] != folder.name:
            label = c['experiment'] + ' / ' + label
        lines.append(
            f"| {label} | {b[m]['median']:.3f} → {a[m]['median']:.3f} | {reduction:.2f} | {b['render_gpu_ms']['median']:.3f} → {a['render_gpu_ms']['median']:.3f} | {b['allocated_mib']['median']:.2f} → {a['allocated_mib']['median']:.2f} | {c['state']['relative_l1']*100:.6f} |"
        )
    (folder / 'comparisons.md').write_text('\n'.join(lines) + '\n', encoding='utf8')
    print(
        json.dumps(
            {
                'folder': str(folder),
                'runs': len(manifest['runs']),
                'issues': issues,
                'table': '\n'.join(lines),
            },
            ensure_ascii=False,
        )
    )
    return output


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('name')
    args = parser.parse_args()
    analyze(RESULT / args.name)
