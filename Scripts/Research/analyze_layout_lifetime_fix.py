"""descriptor 수명 수정 전후의 State와 검증 로그를 비교한다.

Run common capture gates and compare repaired runs with preserved earlier observations.
"""

import pathlib, sys
from authoring_study import ROOT, STUDY, read, write, stamp
import analyze_authoring_retry as common

OUT = STUDY / 'layout-lifetime-fix'
EXP = OUT / 'render'


def main():
    common.OUT = OUT
    common.EXP = EXP
    common.TEST_RECORD = OUT / 'after.json'
    common.TEST_ARCHIVE = OUT / 'raw/after/inputs'
    common.ALLOWED_PRODUCTION_CHANGES = {
        'Source\\SurfaceState\\GPU\\Resources\\DescriptorResources.h',
        'Source\\SurfaceState\\GPU\\Resources\\DescriptorResources.cpp',
        'Source\\SurfaceState\\GPU\\Resources\\SurfaceGPUResourceManager.cpp',
    }
    common.main()
    import numpy as np

    original = read(STUDY / 'policy-change-retry/capture-selection.json')
    plan = read(EXP / 'plan.json')
    checks = []
    for run in plan['runs']:
        old = next(r for r in original['selected_runs'] if r['id'] == run['id'])
        validation = read(pathlib.Path(run['output'] + '.validation.json'))
        for instance in validation['instances']:
            suffix = f".state-{instance['instance']}.f32"
            a = np.fromfile(old['output'] + suffix, dtype='<f4')
            b = np.fromfile(run['output'] + suffix, dtype='<f4')
            checks.append(
                dict(
                    id=run['id'],
                    instance=instance['instance'],
                    bit_equal=bool(np.array_equal(a.view(np.uint32), b.view(np.uint32))),
                    l1=float(np.abs(a.astype(np.float64) - b.astype(np.float64)).sum()),
                )
            )
    result = dict(
        created_at=stamp(),
        scope='Consistency against earlier outputs that failed Vulkan validation; not a physical ground truth',
        arrays=checks,
        all_bit_equal=all(c['bit_equal'] for c in checks),
    )
    write(OUT / 'before-after-state.json', result)
    before = read(OUT / 'before.json')
    after = read(OUT / 'after.json')
    changed = [
        p
        for p, h in before['source_hashes'].items()
        if p.startswith('Source') and after['source_hashes'].get(p) != h
    ]
    write(
        OUT / 'source-change-audit.json',
        dict(
            created_at=stamp(),
            changed_source_files=changed,
            expected=sorted(common.ALLOWED_PRODUCTION_CHANGES),
            matches_expected=set(changed) == common.ALLOWED_PRODUCTION_CHANGES,
        ),
    )
    print('Before/after State arrays:', len(checks), 'bit equal:', result['all_bit_equal'])
    if not result['all_bit_equal'] or set(changed) != common.ALLOWED_PRODUCTION_CHANGES:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
