"""Preserve incomplete captures and retry only conditions failing pre-existing output/count gates."""

import copy, os, pathlib, shutil
from authoring_study import ROOT, STUDY, read, write, stamp
import authoring_render_validation as render

OUT = STUDY / 'policy-change-retry'
EXP = OUT / 'render-v2'


def main():
    manifest = read(EXP / 'manifest.json')
    plan = read(EXP / 'plan.json')
    if manifest.get('status') != 'completed':
        raise RuntimeError('Wait for the initial sequence to finish')
    audit = []
    failed = []
    for case in plan['runs']:
        path = pathlib.Path(case['output'])
        reasons = []
        missing = [
            suffix
            for suffix in ['.ppm', '.settings.json', '.validation.json']
            if not pathlib.Path(str(path) + suffix).exists()
        ]
        lines = path.read_text(encoding='utf-8').splitlines() if path.exists() else []
        if missing:
            reasons.append('Missing outputs: ' + str(missing))
        if len(lines) != 1:
            reasons.append('Frame sample count ' + str(len(lines)))
        validation_path = pathlib.Path(str(path) + '.validation.json')
        v = read(validation_path) if validation_path.exists() else {}
        if v.get('simulation_step_serial') != 362 or v.get('submitted_frames') != 362:
            reasons.append(
                'Expected 362 simulation/submission frames, got '
                + str([v.get('simulation_step_serial'), v.get('submitted_frames')])
            )
        audit.append(
            dict(
                id=case['id'],
                accepted=not reasons,
                reasons=reasons,
                swapchain_recreations=path.with_suffix('.log')
                .read_text(encoding='utf-8')
                .count('TSwapchain recreation complete'),
            )
        )
        if reasons:
            failed.append(case)
    write(
        OUT / 'initial-capture-gates.json',
        dict(
            created_at=stamp(),
            criteria='Original required frame image/settings/count conditions; exit zero alone insufficient',
            cases=audit,
            failed_cases=[c['id'] for c in failed],
            initial_outputs_preserved=True,
        ),
    )
    selected = copy.deepcopy(plan['runs'])
    if failed:
        dest = OUT / 'render-v2-replays'
        if dest.exists():
            raise RuntimeError('Refusing to overwrite replay evidence')
        replay = copy.deepcopy(plan)
        replay.update(
            created_at=stamp(),
            runs=[],
            protocol_change='Retry only capture-gate failures in a fresh process; same numeric settings, inputs and executable. Earlier outputs retained.',
            triggering_audit=str(OUT / 'initial-capture-gates.json'),
        )
        for case in failed:
            new = copy.deepcopy(case)
            folder = dest / 'raw' / case['id'] / 'input'
            old = pathlib.Path(case['scene']).parent
            shutil.copytree(old, folder)
            scene = read(folder / 'Task.Scene')
            scene['animation'] = os.path.relpath(
                (old / scene['animation']).resolve(), folder
            ).replace('\\', '/')
            for obj in scene['objects']:
                obj['mesh'] = os.path.relpath((old / obj['mesh']).resolve(), folder).replace(
                    '\\', '/'
                )
            write(folder / 'Task.Scene', scene)
            new.update(
                scene=str(folder / 'Task.Scene'),
                settings=str(folder / 'settings.json'),
                output=str(folder.parent / 'capture.jsonl'),
            )
            replay['runs'].append(new)
            selected = [new if r['id'] == new['id'] else r for r in selected]
        write(dest / 'plan.json', replay)
        render.OUT = dest
        render.run()
    write(
        OUT / 'capture-selection.json',
        dict(
            created_at=stamp(),
            original_manifest=str(EXP / 'manifest.json'),
            replay_manifest=str(OUT / 'render-v2-replays/manifest.json') if failed else None,
            replayed_conditions=[c['id'] for c in failed],
            selected_runs=selected,
            rule='Replace only earlier runs failing required capture/count gates; never select by favorable numeric outcome',
        ),
    )
    print('Capture selection complete; replaced', len(failed), 'incomplete conditions')


if __name__ == '__main__':
    main()
