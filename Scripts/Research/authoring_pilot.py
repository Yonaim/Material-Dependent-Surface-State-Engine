"""Archive and execute opt-in, synchronized authoring checkpoints. No human data."""
import argparse
import copy
import os
import pathlib
import shutil
import subprocess

from authoring_study import ROOT, STUDY, digest, read, write, stamp
from layout_lifetime_validation import archive

OUT = STUDY / 'goal-v1/pilot'
EXE = ROOT / 'Build/bin/Release/MDSS.exe'
OLD = STUDY / 'layout-lifetime-fix/render/raw/absorbedwetness'


def prepare(kind):
    folder = OUT / 'raw' / kind
    if folder.exists():
        raise RuntimeError('Preserving existing candidate and run evidence')
    source = OLD / 'input' if kind == 'compatibility' else (
        OUT/'raw/candidate-01/input' if kind=='candidate-01-retry' else STUDY/'goal-v1/raw/work')
    scene = read(source / 'Task.Scene')
    profile = read(source / 'Editable.SRProfile')
    distribution = read(source / 'Editable.SurfaceProfileMap')
    if kind == 'candidate-01':
        profile['states']['waterfilm'].update(saturationSpreadFactor=.30, gravityFlowFactor=.05, decayRate=.10)
        profile['states']['absorbedwetness'] = dict(
            stateCapacity=.5, inputFactor=1, saturationSpreadFactor=0, gravityFlowFactor=0,
            decayRate=0, cavityDecayProtectionFactor=0, cavityExitResistanceFactor=0,
            accumulationFactor=0, cavityFillFactor=0, thicknessPerAmount=.025)
        profile['reactions'].append(dict(id='surface_absorption', mode='inflowFraction',
            inflowState='waterfilm', inflowSources=['positiveInput', 'incoming'], fraction=.20,
            consumes=dict(waterfilm=1), produces=dict(absorbedwetness=1)))
    animation_path = (source / scene['animation']).resolve()
    input_folder = folder / 'input'
    scene['animation'] = 'Task.DemoAnim'
    for obj in scene['objects']:
        obj['mesh'] = os.path.relpath((source / obj['mesh']).resolve(), input_folder).replace('\\', '/')
        if 'surfaceProfileMap' in obj:
            obj['surfaceProfileMap'] = 'Editable.SurfaceProfileMap'
    animation=read(animation_path)
    if kind=='candidate-01-retry':
        assert not animation['tracks']
        # Loader requires a non-empty track array. A constant position keeps the frozen view unchanged.
        animation['tracks']=[dict(target='camera',property='position',interpolation='linear',
            keys=[dict(time=0,value=scene['camera']['position']),dict(time=16,value=scene['camera']['position'])])]
    for name, obj in [('Task.Scene', scene), ('Task.DemoAnim', animation),
                      ('Editable.SRProfile', profile), ('Editable.SurfaceProfileMap', distribution)]:
        write(input_folder / name, obj)
    settings = read(OLD / 'input/settings.json')
    settings['authoring_instance_id'] = 'b_heated'
    settings['authoring_checkpoint_steps'] = [60, 360] if kind == 'compatibility' else [60,120,240,360,480,600,720,840,960]
    write(input_folder / 'settings.json', settings)
    plan = dict(id=kind, created_at=stamp(), status='prepared_before_execution',
        candidate_number=None if kind=='compatibility' else 1,
        execution_amendment='Empty camera tracks rejected by actual loader; replace with constant-position track. '
            'Same profile, target, camera and event input. Original failed run preserved.' if kind=='candidate-01-retry' else None,
        scope='Instrumentation consistency' if kind == 'compatibility' else 'First AI-assisted candidate for AUTH-GOAL-01 v1',
        prior_knowledge='Earlier six-second results and published internal studies known; no blind or independent quality claim',
        hypothesis='Readback does not alter final State' if kind == 'compatibility' else
            'Increase transport and absorb 20% of positive/incoming water, with explicit water decay .10; '
            'persistent half-capacity wetness may expose a trace after water reduces. Untested, not a predicted result.',
        input_folder=str(input_folder), output=str(folder/'capture.jsonl'),
        final_requested_serial=360 if kind == 'compatibility' else 960,
        actual_final_serial_includes_two_drain_frames=True,
        required_checkpoints=[0]+settings['authoring_checkpoint_steps'],
        freeze_plan=str(STUDY/'goal-v1/plan.json'), performance_comparison=False,
        human_sessions=0, profile=profile, source_input_sha256={str(p.relative_to(ROOT)):digest(p) for p in source.glob('*') if p.is_file()},
        input_sha256={str(p.relative_to(ROOT)):digest(p) for p in input_folder.iterdir() if p.is_file()})
    write(OUT/(kind+'-plan.json'), plan)
    return plan


def run(kind):
    plan = read(OUT/(kind+'-plan.json')) if (OUT/(kind+'-plan.json')).exists() else prepare(kind)
    record_path = OUT/(kind+'-manifest.json')
    if record_path.exists():
        raise RuntimeError('Refusing to overwrite a run')
    if kind != 'compatibility':
        assert read(OUT/'compatibility-analysis.json')['passed'], 'Instrumentation consistency must pass first'
    for name,h in plan['input_sha256'].items():
        assert digest(ROOT/name)==h,name
    registry=ROOT/'Build/OptimizationLab/blocked-executables.json'
    if registry.exists() and digest(EXE) in read(registry):
        raise RuntimeError('Executable is in the active blocked-hash registry')
    folder=pathlib.Path(plan['input_folder']).parent
    hashes=archive(folder/'inputs', [EXE, pathlib.Path(__file__).resolve(),
        ROOT/'Scripts/Research/analyze_authoring_pilot.py',
        ROOT/'Scripts/Research/prepare_authoring_goal.py', OUT/(kind+'-plan.json'), STUDY/'goal-v1/plan.json',
        ROOT/'Docs/MDSSP-Agent-Vault/03_Development/06_Authoring-Study-Protocol.md',
        ROOT/'Assets/Animations/IceReactionLab.DemoAnim', *pathlib.Path(plan['input_folder']).iterdir()])
    output=pathlib.Path(plan['output'])
    command=[str(EXE),'--benchmark-scene',str(pathlib.Path(plan['input_folder'])/'Task.Scene'),
        '--benchmark-resolution','128','--benchmark-warmup-frames',str(plan['final_requested_serial']-1),
        '--benchmark-measure-frames','1','--benchmark-output',str(output)]
    record=dict(started_at=stamp(),command=command,executable_sha256=digest(EXE),source_sha256=hashes,
        status='running',scope=plan['scope'],diagnostic_timing_not_performance=True)
    write(record_path, record)
    layout=ROOT/'Config/EditorLayout.ini'; original=layout.read_bytes() if layout.exists() else None
    try:
        with output.with_suffix('.log').open('w',encoding='utf-8') as log:
            result=subprocess.run(command,cwd=ROOT,env=dict(os.environ,MDSS_PERF_SETTINGS=str(pathlib.Path(plan['input_folder'])/'settings.json')),
                stdout=log,stderr=subprocess.STDOUT,timeout=720)
        record.update(exit_code=result.returncode,status='completed' if result.returncode==0 else 'failed')
    except (OSError,subprocess.TimeoutExpired) as exc:
        record.update(status='execution_failed',error=str(exc))
    finally:
        if original is not None:layout.write_bytes(original)
        elif layout.exists():layout.unlink()
        record.update(finished_at=stamp(),editor_layout_restored=(layout.read_bytes() if layout.exists() else None)==original)
        write(record_path,record)
    print(kind,record['status'],flush=True)
    if record['status']!='completed':raise SystemExit(1)


if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('kind',choices=['compatibility','candidate-01','candidate-01-retry'])
    ap.add_argument('--prepare-only',action='store_true');args=ap.parse_args()
    if args.prepare_only:prepare(args.kind)
    else:run(args.kind)
