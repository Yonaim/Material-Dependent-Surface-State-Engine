"""저작 재검증의 입력 대응과 실행 결과를 조건별로 분석한다.

Audit fresh rendered references and compare named states; no human productivity inference.
"""

# ============================================================
# 00. Imports and Module Configuration
# ============================================================
# region 00_Imports_and_Module_Configuration
# 실행에 필요한 모듈과 입력·결과 경로를 한곳에서 구성한다.

import copy, pathlib, sys
from authoring_study import ROOT, STUDY, read, write, digest, stamp

sys.path.insert(0, str(ROOT / 'Build/ResearchPython'))
import numpy as np
from PIL import Image, ImageDraw, ImageFont

OUT = STUDY / 'policy-change-retry'
EXP = OUT / 'render-v2'
TEST_RECORD = None
TEST_ARCHIVE = None
ALLOWED_PRODUCTION_CHANGES = set()


# endregion

# ============================================================
# 01. Processing Helpers
# ============================================================
# region 01_Processing_Helpers
# 저작 재검증의 입력 대응과 실행 결과를 조건별로 분석한다.


def compare_authoring_conditions(arrays, comparisons, errors, images):
    for a, b in [
        ('base', 'threshold'),
        ('wetness', 'absorbedwetness'),
        ('absorbedwetness', 'absorbedwetness-repeat'),
        ('absorbedwetness', 'reuse20'),
    ]:
        mapped = {}
        bit_equal = True
        for index, channels in arrays[a].items():
            for name, data in channels.items():
                dest = 'absorbedwetness' if a == 'wetness' and name == 'wetness' else name
                other = arrays[b][index][dest]
                equal = np.array_equal(data.view(np.uint32), other.view(np.uint32))
                delta = np.abs(data.astype(np.float64) - other.astype(np.float64))
                entry = mapped.setdefault(name, dict(l1=0.0, max_absolute=0.0, bit_equal=True))
                entry['l1'] += float(delta.sum())
                entry['max_absolute'] = max(entry['max_absolute'], float(delta.max(initial=0)))
                entry['bit_equal'] &= equal
                bit_equal &= equal
        diff = np.abs(images[a].astype(np.int16) - images[b].astype(np.int16))
        comparisons.append(
            dict(
                a=a,
                b=b,
                named_states=mapped,
                all_named_states_bit_equal=bool(bit_equal),
                pixels_equal=bool(np.array_equal(images[a], images[b])),
                changed_pixels=int(np.any(diff > 0, axis=2).sum()),
                mean_absolute_rgb_8bit=float(diff.mean()),
                max_absolute_rgb_8bit=int(diff.max()),
                caution='Whole-frame pixel difference is descriptive, not a quality metric or a matched-state-time causal proof',
            )
        )
        if a == 'absorbedwetness' and b == 'absorbedwetness-repeat' and not bit_equal:
            errors.append('Fresh-process State repeatability failed')


def check_authored_paths(errors, old, path_checks, plan):
    for run in plan['runs']:
        scene = pathlib.Path(run['scene'])
        s = read(scene)
        previous = read(pathlib.Path(next(x for x in old['runs'] if x['id'] == run['id'])['scene']))
        ok = (scene.parent / s['animation']).resolve() == pathlib.Path(
            previous['animation']
        ).resolve()
        ok &= not pathlib.Path(s['animation']).is_absolute()
        for obj, was in zip(s['objects'], previous['objects']):
            ok &= (
                not pathlib.Path(obj['mesh']).is_absolute()
                and (scene.parent / obj['mesh']).resolve() == pathlib.Path(was['mesh']).resolve()
            )
        canonical = copy.deepcopy(s)
        canonical['animation'] = str((scene.parent / s['animation']).resolve())
        for obj in canonical['objects']:
            obj['mesh'] = str((scene.parent / obj['mesh']).resolve())
        same_scene = canonical == previous
        previous_folder = pathlib.Path(
            next(x for x in old['runs'] if x['id'] == run['id'])['scene']
        ).parent
        same_profiles = all(
            digest(scene.parent / f) == digest(previous_folder / f)
            for f in ['Editable.SRProfile', 'Editable.SurfaceProfileMap', 'settings.json']
        )
        ok &= same_scene and same_profiles
        path_checks.append(
            dict(
                id=run['id'],
                same_resolved_assets=bool(ok),
                same_scene_except_paths=same_scene,
                same_profile_map_settings=same_profiles,
            )
        )
        if not ok:
            errors.append(run['id'] + ': rebased asset mismatch')


def main():
    plan = read(EXP / 'plan.json')
    manifest = read(EXP / 'manifest.json')
    if manifest['status'] != 'completed':
        raise RuntimeError('Render retry incomplete; do not analyze partial set as success')
    errors = []
    records = []
    arrays = {}
    images = {}
    selection = (
        read(OUT / 'capture-selection.json') if (OUT / 'capture-selection.json').exists() else None
    )
    replay_manifest = (
        read(pathlib.Path(selection['replay_manifest']))
        if selection and selection['replay_manifest']
        else None
    )
    if selection:
        plan['runs'] = selection['selected_runs']
    figure = EXP / 'figures'
    figure.mkdir(exist_ok=True)
    expected = dict(
        geometry_feedback=True,
        fp16_raw_flux=False,
        fp16_weights=False,
        sparse_solver=False,
        sparse_height=False,
        sparse_geometry=False,
        active_channel_mask=False,
        per_workgroup_channel_mask=False,
        surface_mesh_resolution=128,
        overlay_mesh_resolution=128,
    )
    for run in plan['runs']:
        name = run['id']
        path = pathlib.Path(run['output'])
        run_manifest = (
            replay_manifest if selection and name in selection['replayed_conditions'] else manifest
        )
        status = next(x for x in run_manifest['runs'] if x['id'] == name)
        log = path.with_suffix('.log').read_text(encoding='utf-8')
        validation = read(pathlib.Path(str(path) + '.validation.json'))
        settings = read(pathlib.Path(str(path) + '.settings.json'))
        frames = [
            __import__('json').loads(line)
            for line in path.read_text(encoding='utf-8').splitlines()
            if line.strip()
        ]
        if status['exit_code'] != 0 or '[ERROR]' in log:
            errors.append(name + ': runtime error')
        if len(frames) != 1 or frames[0]['frame_index'] != 359:
            errors.append(name + ': frame sample')
        if validation['simulation_step_serial'] != 362 or validation['submitted_frames'] != 362:
            errors.append(name + ': simulation/frame count mismatch')
        if any(settings.get(k) != v for k, v in expected.items()):
            errors.append(name + ': actual settings mismatch')
        channels = run['channels']
        arrays[name] = {}
        totals = {}
        instances = []
        for item in validation['instances']:
            index = item['instance']
            data = np.fromfile(str(path) + f'.state-{index}.f32', dtype='<f4').reshape(
                -1, len(channels)
            )
            if data.size != item['elements'] or not np.isfinite(data).all() or np.any(data < 0):
                errors.append(name + ': invalid State array')
            for flag in [
                'nonfinite',
                'negative',
                'reaction_budget_exhausted_texels',
                'reaction_arithmetic_failure_texels',
            ]:
                if item[flag] != 0:
                    errors.append(name + ': ' + flag)
            if not np.allclose(
                data.sum(axis=0, dtype=np.float64), item['state_sums'], rtol=1e-12, atol=1e-8
            ):
                errors.append(name + ': exported channel sum mismatch')
            arrays[name][index] = {channel: data[:, j] for j, channel in enumerate(channels)}
            sums = dict(zip(channels, item['state_sums']))
            instances.append(dict(instance=index, state_sums=sums))
            for key, value in sums.items():
                totals[key] = totals.get(key, 0) + value
        im = Image.open(str(path) + '.ppm').convert('RGB')
        images[name] = np.asarray(im).copy()
        if list(im.size) != settings['swapchain_extent']:
            errors.append(name + ': screenshot extent')
        im.save(figure / f'{name}.png')
        runtime_errors = [line for line in log.splitlines() if '[ERROR]' in line]
        records.append(
            dict(
                id=name,
                output=str(path),
                simulation_step_serial=validation['simulation_step_serial'],
                runtime_error_count=len(runtime_errors),
                first_runtime_error=runtime_errors[0] if runtime_errors else None,
                submitted_frames=validation['submitted_frames'],
                sampled_frame_index=frames[0]['frame_index'],
                sampled_frame_simulated_seconds=frames[0]['simulated_seconds'],
                channels=channels,
                totals=totals,
                instances=instances,
                warning_lines=log.count('[WARNING]'),
                camera=settings['viewports'],
                screenshot_sha256=digest(pathlib.Path(str(path) + '.ppm')),
            )
        )
    if any(run['camera'] != records[0]['camera'] for run in records):
        errors.append('Captured camera/viewport mismatch')
    comparisons = []
    compare_authoring_conditions(arrays, comparisons, errors, images)

    archives = []
    archive_sources = [
        (
            'geometry',
            read(TEST_RECORD or OUT / 'geometry.json'),
            TEST_ARCHIVE or OUT / 'raw/geometry/inputs',
        ),
        ('render', manifest, EXP / 'raw/inputs'),
    ]
    if replay_manifest:
        archive_sources.append(
            ('capture-replay', replay_manifest, OUT / 'render-v2-replays/raw/inputs')
        )
    for kind, entry, directory in archive_sources:
        mismatch = []
        for rel, h in entry['source_hashes'].items():
            try:
                actual = digest(directory / rel)
            except OSError:
                actual = None
            if actual != h:
                mismatch.append(rel)
        archives.append(dict(kind=kind, files=len(entry['source_hashes']), mismatch=mismatch))
        errors.extend(kind + ': archive ' + p for p in mismatch)
    # Verify that rebased paths resolve to exactly the earlier intended mesh/animation assets.
    old = read(STUDY / 'render-validation/plan.json')
    path_checks = []
    check_authored_paths(errors, old, path_checks, plan)
    old_manifest = read(STUDY / 'render-validation/manifest.json')
    changed_production = [
        p
        for p, h in old_manifest['source_hashes'].items()
        if (
            p.startswith('Source') or p.startswith('Shaders') or p.startswith('Build\\bin\\Shaders')
        )
        and manifest['source_hashes'].get(p) != h
    ]
    if set(changed_production) != ALLOWED_PRODUCTION_CHANGES:
        errors.append('Unexpected production source/shader changes since blocked attempt')
    font = ImageFont.truetype('C:/Windows/Fonts/arial.ttf', 20)
    sheet = Image.new('RGB', (1280, 3 * 392 + 36), (245, 245, 245))
    draw = ImageDraw.Draw(sheet)
    caption = (
        'Diagnostic captures: Vulkan validation errors; not accepted reference results'
        if any(r['runtime_error_count'] for r in records)
        else 'Post-fix captures: functional validation; no human quality study'
    )
    draw.text(
        (10, 6),
        caption,
        fill=(150, 25, 25) if any(r['runtime_error_count'] for r in records) else (25, 85, 45),
        font=font,
    )
    titles = [
        'Base / 8 deg',
        'Heat threshold 0.25 -> 0.45',
        'wetness / no builtin stain binding',
        'absorbedwetness / builtin stain',
        'Same profile / 20 deg',
        'Fresh-process repeat / 8 deg',
    ]
    for j, (run, title) in enumerate(zip(plan['runs'], titles)):
        x = (j % 2) * 640
        y = (j // 2) * 392 + 36
        draw.text((x + 10, y + 6), title, fill=(25, 25, 25), font=font)
        im = Image.fromarray(images[run['id']]).resize((640, 360), Image.Resampling.LANCZOS)
        sheet.paste(im, (x, y + 32))
    sheet.save(figure / 'reference-sheet.png')
    crop = (225, 250, 1055, 555)
    closeup = Image.new('RGB', (1660, 373), (245, 245, 245))
    draw = ImageDraw.Draw(closeup)
    draw.text(
        (10, 6),
        caption,
        fill=(150, 25, 25) if any(r['runtime_error_count'] for r in records) else (25, 85, 45),
        font=font,
    )
    for j, name in enumerate(['wetness', 'absorbedwetness']):
        draw.text(
            (j * 830 + 10, 42), name + ' / same crop, original pixels', fill=(25, 25, 25), font=font
        )
        closeup.paste(Image.fromarray(images[name]).crop(crop), (j * 830, 68))
    closeup.save(figure / 'wetness-binding-comparison.png')
    write(
        EXP / 'analysis.json',
        dict(
            created_at=stamp(),
            scope=plan['scope'],
            runs=records,
            comparisons=comparisons,
            archive_audit=archives,
            path_rebasing_checks=path_checks,
            audit_errors=errors,
            capture_selection=selection,
            changed_production_inputs_since_blocked_attempt=changed_production,
            acceptance=(
                'failed_vulkan_validation'
                if any(r['runtime_error_count'] for r in records)
                else ('failed_other_gate' if errors else 'passed')
            ),
            closeup_figure=dict(
                file='figures/wetness-binding-comparison.png',
                crop_xyxy=crop,
                scope='Identical fixed crop for display only, no color/difference amplification; full frames retained',
            ),
            human_sessions=0,
            human_productivity='not_measured',
            independent_quality_gate='not_validated',
            full_material_ledger='not_exported_by_this_app_capture; final sums do not prove conservation',
            image_state_time_limit=plan['state_capture_scope'],
            reaction_diagnostic_limit='Exported reaction flags describe the final scratch buffer, not a step-by-step audit of the full run',
            performance='Single captured frame per condition is not a performance measurement',
            temporal_scope='Approximately six seconds of demo playback; not the full fourteen-second sequence',
        ),
    )
    print('Render cases:', len(records), 'audit errors:', len(errors))
    for pair in comparisons:
        print(
            pair['a'],
            pair['b'],
            'State bit equal:',
            pair['all_named_states_bit_equal'],
            'changed pixels:',
            pair['changed_pixels'],
        )
    if errors:
        print(errors)
        raise SystemExit(1)


# endregion

# ============================================================
# 02. Script Execution
# ============================================================
# region 02_Script_Execution
# 저작 재검증의 입력 대응과 실행 결과를 조건별로 분석한다.

if __name__ == '__main__':
    main()

# endregion
