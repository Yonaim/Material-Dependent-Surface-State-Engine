"""Audit archived experiment outputs independently of the timing comparison tables."""
from pathlib import Path
import argparse, collections, datetime, hashlib, json, math

ROOT = Path(__file__).resolve().parents[2]
RESULTS = ROOT / 'Docs/MDSSP-Vault/04_Development/Performance-Results'
FOLDERS = ['20261022-optimization-suite-main', '20261022-single-transport-validation',
           '20261022-workload-supplement', '20261022-development-history']


def load(path):
    return json.loads(path.read_text(encoding='utf8'))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=RESULTS / FOLDERS[0] / 'study-audit.json')
    args = parser.parse_args()
    errors, phases, all_periods = [], [], []
    binary_digests = {}
    for folder in FOLDERS:
        base = RESULTS / folder
        if not (base / 'manifest.json').exists():
            errors.append({'phase': folder, 'error': 'missing manifest'})
            continue
        manifest = load(base / 'manifest.json')
        planned = {run['id'] for run in manifest['plan']['runs']}
        seen, binaries, repeat_states = set(), collections.defaultdict(set), collections.defaultdict(list)
        frames = 0
        case_views = {}; case_cameras = {}
        unsupported = set()
        for run in manifest['runs']:
            context = {'phase': folder, 'run': run['id']}
            try:
                assert run['id'] not in seen, 'duplicate run id'
                seen.add(run['id'])
                assert run['status'] == 'complete', run.get('error', run['status'])
                assert run['exit_code'] == 0, 'nonzero exit code'
                assert run['binary_sha256'], 'missing executable hash'
                if run['binary'] not in binary_digests:
                    binary_digests[run['binary']] = hashlib.sha256(Path(run['binary']).read_bytes()).hexdigest()
                assert binary_digests[run['binary']] == run['binary_sha256'], 'measured executable differs from saved hash'
                binaries[run['binary']].add(run['binary_sha256'])
                all_periods.append((run['started_at'], run['finished_at'], folder, run['id']))
                raw = base / run['raw']
                samples = [json.loads(line) for line in raw.read_text(encoding='utf8').splitlines() if line.strip()]
                warmup, measure = run.get('warmup', 60), run.get('measure', 600)
                assert len(samples) == measure, 'wrong sample count'
                assert [s['frame_index'] for s in samples] == list(range(warmup, warmup + measure)), 'non-contiguous measured frames'
                frames += len(samples)
                for sample in samples:
                    for field, value in sample.items():
                        if field.endswith('_ms') and value is not None:
                            assert isinstance(value, (int, float)) and math.isfinite(value) and value >= 0, f'invalid {field}'
                    assert sample['simulation_steps'] == 1, 'unexpected simulation substep count'
                    if 'simulated_seconds' in sample:
                        assert abs(sample['simulated_seconds'] - 1/60) < 1e-7, 'unexpected simulated timestep'
                validation = load(Path(str(raw) + '.validation.json'))
                expected_steps = 2 if 'transport-validation' in folder else warmup + measure + 2
                if 'simulation_step_serial' in validation:
                    assert validation['simulation_step_serial'] == expected_steps, 'unexpected final simulation step'
                else:
                    unsupported.add('final simulation step serial (historical exporter; frame count and measured steps checked)')
                assert validation['submitted_frames'] == warmup + measure + 2, 'unexpected submitted frames'
                if 'allocated_total_bytes' in validation:
                    assert validation['allocated_total_bytes'] == validation['allocated_buffer_bytes'] + validation['allocated_image_bytes'], 'allocation sum mismatch'
                    assert sum(validation['buffer_category_bytes']) == validation['allocated_buffer_bytes'], 'buffer category sum mismatch'
                else:
                    unsupported.add('allocation categories (historical exporter)')
                for instance in validation['instances']:
                    assert instance['nonfinite'] == 0 and instance['negative'] == 0, 'invalid state values'
                    state = Path(str(raw) + f".state-{instance['instance']}.f32")
                    assert state.stat().st_size == instance['elements'] * 4, 'wrong state file size'
                settings = load(Path(str(raw) + '.settings.json'))
                assert settings['geometry_feedback'] is True, 'feedback not enabled'
                if 'fixed_dt' in settings:
                    assert abs(settings['fixed_dt'] - 1/60) < 1e-12, 'historical fixed timestep differs'
                for field, expected in run.get('settings', {}).items():
                    if field in settings and field != 'frame_index':
                        assert settings[field] == expected, f'exported setting differs: {field}'
                if 'experiment' in settings:
                    assert settings['experiment'] == run.get('settings', {}), 'experiment settings differ'
                if 'tile_size' in run.get('settings', {}):
                    assert settings['overlay_tile_size'] == run['settings']['tile_size'], 'tile size mismatch'
                view = {'extent': settings['swapchain_extent'], 'viewports': [
                    {k: v for k,v in viewport.items() if k != 'view_mode'} for viewport in settings['viewports']]}
                camera_case = (run['scene_name'], run['resolution'])
                cameras = [{k:v for k,v in viewport.items() if k in ['camera_position','camera_target','fov_degrees']} for viewport in settings['viewports']]
                assert cameras == case_cameras.setdefault(camera_case, cameras), 'camera pose changed within a scene-resolution case'
                case = (*camera_case, warmup, measure)
                assert view == case_views.setdefault(case, view), 'viewport changed within the same scene-resolution/start protocol'
                if 'surface_mesh_resolution' in settings:
                    render_resolution = run.get('settings', {}).get('render_mesh_resolution', run['resolution'])
                    assert settings['surface_mesh_resolution'] == render_resolution, 'base mesh resolution mismatch'
                    assert settings['overlay_mesh_resolution'] == render_resolution, 'overlay mesh resolution mismatch'
                else:
                    assert settings['simulation_resolution'] == run['resolution'], 'historical simulation resolution mismatch'
                    unsupported.add('separate base/overlay resolution fields (historical mesh follows scene resolution)')
                assert Path(str(raw) + '.bgra').stat().st_size == math.prod(settings['swapchain_extent']) * 4, 'wrong capture byte count'
                if Path(str(raw) + '.cpu.json').exists():
                    cpu = load(Path(str(raw) + '.cpu.json'))
                    assert len(cpu) == warmup + measure + 2, 'wrong CPU frame sample count'
                    assert all(isinstance(v, (int, float)) and math.isfinite(v) and v >= 0 for v in cpu), 'invalid CPU elapsed value'
                else:
                    assert run.get('historical_env'), 'missing CPU frame export'
                    unsupported.add('CPU frame elapsed (historical exporter)')
                reload_path = Path(str(raw) + '.reload.json')
                if reload_path.exists():
                    reloads = load(reload_path)
                    assert len(reloads) == run.get('settings', {}).get('reload_count', 0), 'wrong resource-rebuild count'
                    assert [v['reload'] for v in reloads] == list(range(len(reloads))), 'non-contiguous resource-rebuild records'
                    assert all(math.isfinite(v['milliseconds']) and v['milliseconds'] >= 0 for v in reloads), 'invalid resource-rebuild time'
                    assert all(0 <= v['retained_bytes'] <= 256 * 2**20 for v in reloads), 'invalid buffer-pool retained size'
                    assert all(a['reuse_count'] <= b['reuse_count'] and a['reused_bytes'] <= b['reused_bytes'] for a,b in zip(reloads,reloads[1:])), 'non-monotonic cumulative reuse counter'
                key = (run['group'], run['scene_name'], run['resolution'], run['variant'])
                repeat_states[key].append(validation['instances'])
            except Exception as exc:
                errors.append({**context, 'error': str(exc)})
        if planned != seen:
            errors.append({'phase': folder, 'missing': sorted(planned - seen), 'unexpected': sorted(seen - planned)})
        for binary, hashes in binaries.items():
            if len(hashes) != 1:
                errors.append({'phase': folder, 'binary': binary, 'error': 'executable changed within phase', 'hashes': sorted(hashes)})
        for key, values in repeat_states.items():
            if any(value != values[0] for value in values[1:]):
                errors.append({'phase': folder, 'case': key, 'error': 'state validation differs between repeats'})
        phases.append({'phase': folder, 'status': manifest['status'], 'planned': len(planned), 'captured': len(seen),
                       'measured_frames': frames, 'unique_cases': len(repeat_states), 'unsupported_checks': sorted(unsupported),
                       'binary_hashes': {path: sorted(values) for path, values in binaries.items()}})
    periods = sorted((datetime.datetime.fromisoformat(a), datetime.datetime.fromisoformat(b), phase, run) for a, b, phase, run in all_periods)
    for previous, current in zip(periods, periods[1:]):
        if current[0] < previous[1]:
            errors.append({'error': 'overlapping measurement processes', 'runs': [previous[3], current[3]]})
    archive_checks = [
        (ROOT/'Build/PerformanceLab/current/Build/bin/Release/MDSS.exe', RESULTS/FOLDERS[0]/'raw/inputs/MDSS-current.exe'),
        (ROOT/'Build/PerformanceLab/MDSS-validation.exe', RESULTS/FOLDERS[1]/'raw/inputs/MDSS-validation.exe'),
        (ROOT/'Build/PerformanceLab/compact/Build/bin/Release/MDSS.exe', RESULTS/FOLDERS[2]/'raw/inputs/top-draw/MDSS-top-draw.exe'),
        *[(ROOT/f'Build/PerformanceLab/{v}/Build/bin/Release/MDSS.exe', RESULTS/FOLDERS[3]/f'raw/inputs/{v}-MDSS.exe') for v in ['V0','V1','V3']]]
    for original, archived in archive_checks:
        try:
            assert hashlib.sha256(original.read_bytes()).digest() == hashlib.sha256(archived.read_bytes()).digest(), 'archive differs'
        except Exception as exc:
            errors.append({'archive': str(archived), 'error': str(exc)})
    result = {'generated_at': datetime.datetime.now().astimezone().isoformat(), 'status': 'pass' if not errors else 'issues_found',
              'scope': 'File completeness, measured frame continuity, exported conditions, final step, finite state/timings, repeat state records, binary identity, non-overlapping app execution. Does not prove conservation, image correctness or absence of external GPU load.',
              'phases': phases, 'errors': errors}
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf8')
    print(f"Study audit: {result['status']}; {sum(p['captured'] for p in phases)} runs; {sum(p['measured_frames'] for p in phases)} frames; {len(errors)} issues")
    return bool(errors)


if __name__ == '__main__':
    raise SystemExit(main())
