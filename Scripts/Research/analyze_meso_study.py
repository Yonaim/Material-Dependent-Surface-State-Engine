"""Analyse the frozen meso study, preserving nulls and numerical/representation limitations."""

# ============================================================
# 00. Imports and Module Configuration
# ============================================================
# region 00_Imports_and_Module_Configuration

import json, pathlib, sys, hashlib, re, datetime as dt
from meso_study import ROOT, EXP, RAW, read, write, digest, arr, stamp
import numpy as np
import matplotlib

matplotlib.use('Agg')
import matplotlib.pyplot as plt

plan = read(EXP / 'plan.json')
manifest = read(EXP / 'manifest.json')
expected = plan['conditions'] + read(EXP / 'postprocess-control-plan.json')['conditions']
if (EXP / 'shader-capture-replay-plan.json').exists():
    expected += read(EXP / 'shader-capture-replay-plan.json')['conditions']
runs = {r['id']: r for r in manifest['runs'] if r['status'] == 'completed'}
errors = []
channel_budget_checks = []
missing = [c['id'] for c in expected if c['id'] not in runs]
# endregion

# ============================================================
# 01. Processing Helpers
# ============================================================
# region 01_Processing_Helpers


def folder(k):
    return ROOT / runs[k]['output']


def summary(k):
    return runs[k]['summary']


def states(k, t='final'):
    s = summary(k)
    return arr(folder(k), t, s['channels'].__len__())


def scale(k):
    s = summary(k)
    a = states(k, 'state-0').sum(axis=0) + np.asarray(s['cumulative_input'])
    names = s['channels']
    mass = sum(a[names.index(n)] for n in ['ice', 'waterfilm'])
    for n in ['ice', 'waterfilm']:
        a[names.index(n)] = mass
    return a


def compare(a, b, label, restriction=False):
    x = states(a)
    y = states(b)
    initial_b = states(b, 'state-0')
    if restriction:
        coarse = int(round(np.sqrt(len(x))))
        fine = int(round(np.sqrt(len(y))))
        f = fine // coarse
        k = x.shape[1]
        if fine % coarse:
            raise RuntimeError('Nonintegral restriction')
        y = y.reshape(coarse, f, coarse, f, k).sum(axis=(1, 3)).reshape(-1, k)
        initial_b = initial_b.reshape(coarse, f, coarse, f, k).sum(axis=(1, 3)).reshape(-1, k)
    z = scale(a)
    diff = x - y
    record = dict(
        a=a,
        b=b,
        label=label,
        channels=summary(a)['channels'],
        state_l1_pct_supply=(100 * np.abs(diff).sum(axis=0) / z).tolist(),
        state_l1_pct_final_reference=(
            100 * np.abs(diff).sum(axis=0) / np.maximum(np.abs(y).sum(axis=0), 1e-30)
        ).tolist(),
        signed_total_delta=diff.sum(axis=0).tolist(),
        max_abs=abs(diff).max(axis=0).tolist(),
        initial_l1_pct_supply=(
            100 * np.abs(states(a, 'state-0') - initial_b).sum(axis=0) / z
        ).tolist(),
        area_sum_a=float(arr(folder(a), 'areas').sum()),
        area_sum_b=float(arr(folder(b), 'areas').sum()),
        final_extents_a=summary(a)['cumulative_extents'],
        final_extents_b=summary(b)['cumulative_extents'],
    )
    return record


# endregion

# ============================================================
# 02. Script Execution
# ============================================================
# region 02_Script_Execution

for k, r in runs.items():
    f = folder(k)
    if digest(f / 'request.json') != r['request_sha256']:
        errors.append('request hash ' + k)
    if r['plan_sha256'] != digest(EXP / 'plan.json'):
        errors.append('plan changed ' + k)
    archive = read(ROOT / r['archive'] / 'archive.json')
    if (
        archive['files'].get(str(pathlib.Path('Build/Release/MDSS_CouplingStudy.exe')))
        != r['executable_sha256']
    ):
        # Archives use host-native paths, tolerate explicit slash form.
        if archive['files'].get('Build/Release/MDSS_CouplingStudy.exe') != r['executable_sha256']:
            errors.append('exe archive ' + k)
    for name in ['meso-height', 'meso-transfer-normals', 'meso-derived-normals']:
        if not np.isfinite(arr(f, name)).all():
            errors.append('nonfinite meso ' + k)
    if k.startswith('cpu-'):
        m = r['summary']['meso']
        if m['production_active'] != m['texels'] or m['production_components'] != 1:
            errors.append('reference domain assumption ' + k)
        continue
    s = r['summary']
    expectedsteps = int(read(f / 'request.json')['seconds'] * s['hz'])
    rows = [json.loads(l) for l in (f / 'series.jsonl').read_text().splitlines()]
    if len(rows) != expectedsteps or [row['step'] for row in rows] != list(
        range(1, expectedsteps + 1)
    ):
        errors.append('step continuity ' + k)
    for field in [
        'nonfinite',
        'negative',
        'reaction_budget',
        'reaction_arithmetic',
        'frozen_failures',
    ]:
        if s[field]:
            errors.append(field + ' ' + k)
    for t in [0, 1] + ([2, 4] if expectedsteps >= 4 * s['hz'] else []):
        for prefix, count in [
            ('state', s['texels'] * len(s['channels'])),
            ('geometry', s['texels'] * 8),
            ('weights', s['texels'] * 8),
            ('concavity', s['texels']),
        ]:
            p = f / (prefix + '-' + str(t) + '.f32')
            if not p.exists() or p.stat().st_size != count * 4:
                errors.append('snapshot shape ' + k + ' ' + str(p))
    z = scale(k)[s['channels'].index('waterfilm')]
    if s['max_mass_residual'] / z > 1e-5:
        errors.append('material budget ' + k)
    # Independent final channel accounting, in addition to the harness's material invariant.
    profile = read(ROOT / read(f / 'request.json')['profile'])
    expected_total = (
        states(k, 'state-0').sum(axis=0)
        + np.asarray(s['cumulative_input'])
        - np.asarray(s['cumulative_decay'])
    )
    enabled = [r for r in profile['reactions'] if r.get('enabled', True)]
    for reaction, extent in zip(enabled, s['cumulative_extents']):
        for state, coefficient in reaction.get('consumes', {}).items():
            expected_total[s['channels'].index(state)] -= coefficient * extent
        for state, coefficient in reaction.get('produces', {}).items():
            expected_total[s['channels'].index(state)] += coefficient * extent
    relative = np.abs(states(k).sum(axis=0) - expected_total) / scale(k)
    channel_budget_checks.append(
        dict(id=k, channels=s['channels'], relative_residual=relative.tolist())
    )
    if relative.max() > 1e-5:
        errors.append('independent channel accounting ' + k)
    # This study has a full square graph and unchanged analytic fields for every meso comparison.
    request = read(f / 'request.json')
    res = request['resolution']
    if request['case'] == 'meso':
        if s['valid_texels'] != res * res or s['texels'] != res * res or s['mapping_warnings']:
            errors.append('quad mapping ' + k)
        area = arr(f, 'areas')
        expected_area = 1.65 * 2.2
        if abs(area.sum() - expected_area) > 1e-5:
            errors.append('world footprint ' + k)

comparisons = []
if not missing:
    for mode in ['analytic', 'png', 'brick']:
        comparisons.append(compare(f'gpu-{mode}-128-D', f'gpu-{mode}-ls-128-D', 'production_vs_LS'))
    comparisons.append(
        compare('gpu-analytic-128-D', 'gpu-analytic-truth-128-D', 'production_vs_known_height')
    )
    comparisons.append(
        compare('gpu-analytic-ls-128-D', 'gpu-analytic-truth-128-D', 'LS_vs_known_height')
    )
    comparisons.append(compare('gpu-brick-256-D', 'gpu-brick-ls-256-D', 'production_vs_LS_256'))
    comparisons.append(
        compare('gpu-brick-cavity-128-D', 'gpu-brick-cavity-ls-128-D', 'production_vs_LS_cavity')
    )
    comparisons.append(compare('gpu-flat-128-D', 'gpu-brick-128-D', 'flat_vs_real_meso'))
    for mode in ['png', 'brick']:
        comparisons.append(
            compare(f'gpu-{mode}-128-D', f'gpu-{mode}-128-E', 'dynamic_vs_frozen_G0')
        )
    comparisons.append(
        compare('gpu-brick-cavity-128-D', 'gpu-brick-cavity-128-E', 'dynamic_vs_frozen_G0_cavity')
    )
    for strength in ['0.5', '2']:
        comparisons.append(
            compare('gpu-brick-128-D', f'gpu-brick-128-x{strength}-D', 'slope_strength_change')
        )
    comparisons.append(compare('gpu-brick-128-D', 'gpu-brick-128-120-D', 'dt_sensitivity_60_120'))
    comparisons.append(
        compare('gpu-brick-64-D', 'gpu-brick-128-D', 'grid_sensitivity_64_128', True)
    )
    comparisons.append(
        compare('gpu-brick-128-D', 'gpu-brick-256-D', 'grid_sensitivity_128_256', True)
    )
    identity = []
    a, b = 'gpu-brick-128-D', 'gpu-brick-passthrough-128-D'
    for t in [0, 1, 2, 4]:
        for prefix in ['state', 'geometry', 'weights', 'concavity'] + (
            ['flux', 'alpha'] if t else []
        ):
            name = prefix + '-' + str(t) + '.f32'
            equal = (folder(a) / name).read_bytes() == (folder(b) / name).read_bytes()
            identity.append(dict(file=name, bit_equal=equal))
            if not equal:
                errors.append('identity postprocess ' + name)
    write(
        EXP / 'postprocess-control.json',
        dict(a=a, b=b, all_bit_equal=all(x['bit_equal'] for x in identity), arrays=identity),
    )
    for mode in ['png', 'brick', 'brick-cavity']:
        a, b = f'gpu-{mode}-128-D', f'gpu-{mode}-128-E'
        for prefix in ['state', 'geometry', 'weights', 'concavity']:
            if (folder(a) / (prefix + '-0.f32')).read_bytes() != (
                folder(b) / (prefix + '-0.f32')
            ).read_bytes():
                errors.append('D/E initial ' + mode + ' ' + prefix)
    replay_checks = []
    if (EXP / 'shader-capture-replay-plan.json').exists():
        for a, b in [
            ('gpu-brick-128-D', 'gpu-brick-replay-128-D'),
            ('gpu-brick-ls-128-D', 'gpu-brick-ls-replay-128-D'),
        ]:
            for t in [0, 1, 2, 4]:
                for prefix in ['state', 'geometry', 'weights', 'concavity'] + (
                    ['flux', 'alpha'] if t else []
                ):
                    name = prefix + '-' + str(t) + '.f32'
                    same = (folder(a) / name).read_bytes() == (folder(b) / name).read_bytes()
                    replay_checks.append(dict(a=a, b=b, file=name, bit_equal=same))
                    if not same:
                        errors.append('shader capture replay ' + a + ' ' + name)
        write(
            EXP / 'shader-capture-replay.json',
            dict(all_bit_equal=all(x['bit_equal'] for x in replay_checks), arrays=replay_checks),
        )
    for c in expected:
        if c['id'] not in runs or c.get('stage') != 'gpu':
            continue
        k = c['id']
        f = folder(k)
        if k != 'gpu-flat-128-D' and c['resolution'] == 128:
            if (f / 'state-0.f32').read_bytes() != (
                folder('gpu-flat-128-D') / 'state-0.f32'
            ).read_bytes():
                errors.append('common S0 ' + k)
        request = read(f / 'request.json')
        if (
            'height_file' in request['meso']
            and pathlib.Path(request['meso']['height_file']).read_bytes()
            != (f / 'meso-height.f32').read_bytes()
        ):
            errors.append('loaded reference ' + k)

reconstruction = read(EXP / 'reconstruction.json')
for c in reconstruction['results']:
    if c['reference_linear_relative'] > 1e-8:
        errors.append('LS residual ' + c['id'])
    reported = summary(c['id'])['meso']['production_relative_edge_residual']
    if abs(reported - c['production_edge_relative']) > 1e-6:
        errors.append('independent edge residual ' + c['id'])

code = ROOT / 'Source/SurfaceState/Geometry/MesoGeometryBuilder.cpp'
generated = ROOT / 'Build/research/MesoGeometryStudy.cpp'
copied_verbatim = generated.read_text(encoding='utf-8').startswith(code.read_text(encoding='utf-8'))
if not copied_verbatim:
    errors.append('production geometry body changed in research copy')
numeric = [r for k, r in runs.items() if not k.startswith('cpu-')]
audit = dict(
    created_at=stamp(),
    expected=len(expected),
    completed=len(runs),
    missing=missing,
    attempt_statuses=[
        dict(id=r['id'], attempt=r['attempt'], status=r['status']) for r in manifest['runs']
    ],
    evaluation_steps=sum(r['summary'].get('steps', 0) for r in runs.values()),
    max_relative_material_budget=max(
        (
            r['summary']['max_mass_residual']
            / scale(r['id'])[r['summary']['channels'].index('waterfilm')]
            for r in numeric
        ),
        default=0,
    ),
    production_geometry_copied_verbatim=copied_verbatim,
    unique_executable_hashes=sorted({r['executable_sha256'] for r in runs.values()}),
    max_independent_channel_relative_budget=max(
        (max(x['relative_residual']) for x in channel_budget_checks), default=0
    ),
    independent_channel_checks=channel_budget_checks,
    unit_tests=dict(
        normal_map_transfer='passed',
        shared_surface_geometry='failed: unchanged render mesh invalid-texel triangle-count expectation',
        log='raw/unit-tests.log',
    ),
    provenance_limit='First 33 runs archive exe, GLSL and generated CPU body, but initially omit production SPIR-V directory. Two representative replays capture production SPIR-V before execution and compare bitwise; original missing per-run shader capture is not retroactively filled.',
    errors=errors,
)
write(EXP / 'study-audit.json', audit)
write(
    EXP / 'comparisons.json', dict(created_at=stamp(), scope=plan['scope'], comparisons=comparisons)
)
for c in comparisons:
    print(
        c['a'],
        c['b'],
        'water L1 %',
        format(c['state_l1_pct_supply'][c['channels'].index('waterfilm')], '.7g'),
    )
print('audit errors', len(errors), 'missing', missing, 'steps', audit['evaluation_steps'])
if missing or errors:
    raise SystemExit(1)

figures = EXP / 'figures'
figures.mkdir(exist_ok=True)
plt.rcParams.update({'font.size': 10, 'figure.dpi': 130})
fig, axes = plt.subplots(2, 3, figsize=(13, 7.7), layout='constrained')
d = folder('cpu-analytic-128')
h = arr(d, 'meso-height').reshape(128, 128)
ls = arr(d, 'reference-ls').reshape(128, 128)
truth = arr(d, 'reference-truth').reshape(128, 128)
for ax, data, title in zip(
    axes[0],
    [truth, h, ls],
    ['Known height', 'Production: fixed 128 iterations', 'Same-constraint least squares'],
):
    im = ax.imshow(
        data,
        origin='lower',
        extent=(-0.5, 0.5, -0.5, 0.5),
        cmap='coolwarm',
        vmin=-0.015,
        vmax=0.015,
    )
    ax.set_title(title)
    fig.colorbar(im, ax=ax, shrink=0.75, label='mesh-local height')
d = folder('cpu-brick-128')
p = arr(d, 'meso-height').reshape(128, 128)
r = arr(d, 'reference-ls').reshape(128, 128)
for ax, data, title in zip(
    axes[1],
    [p, r, p - r],
    [
        'Real brick map: production',
        'Real brick map: least squares',
        'Production minus least squares',
    ],
):
    vmax = max(abs(p).max(), abs(r).max()) if ax is not axes[1, 2] else abs(p - r).max()
    im = ax.imshow(
        data, origin='lower', extent=(-0.5, 0.5, -0.5, 0.5), cmap='coolwarm', vmin=-vmax, vmax=vmax
    )
    ax.set_title(title)
    fig.colorbar(im, ax=ax, shrink=0.75, label='mesh-local height')
for ax in axes.flat:
    ax.set_xlabel('local x')
    ax.set_ylabel('local y')
fig.suptitle(
    'Reconstruction comparison | one flat quad, 128 x 128\nReal brick input has no known physical height',
    fontsize=14,
)
fig.savefig(figures / 'reconstruction.png')
plt.close(fig)

fig, axes = plt.subplots(2, 3, figsize=(13, 7.7), layout='constrained')
for row, base in enumerate(['gpu-brick-128-D', 'gpu-brick-cavity-128-D']):
    other = 'gpu-brick-ls-128-D' if row == 0 else 'gpu-brick-cavity-ls-128-D'
    water = summary(base)['channels'].index('waterfilm')
    area = arr(folder(base), 'areas') / (1 / 256**2)
    p = (states(base)[:, water] / area).reshape(128, 128)
    r = (states(other)[:, water] / area).reshape(128, 128)
    for col, (data, title) in enumerate(
        [
            (p, 'Production height'),
            (r, 'Least-squares height'),
            (p - r, 'Signed density difference'),
        ]
    ):
        ax = axes[row, col]
        if col < 2:
            im = ax.imshow(data, origin='lower', cmap='viridis', vmin=0, vmax=max(p.max(), r.max()))
        else:
            im = ax.imshow(
                data, origin='lower', cmap='coolwarm', vmin=-abs(data).max(), vmax=abs(data).max()
            )
        ax.set_title(('No cavity\n' if row == 0 else 'Ice cavity enabled\n') + title)
        fig.colorbar(im, ax=ax, shrink=0.75, label='waterfilm amount / area scale')
        ax.set_xlabel('UV column')
        ax.set_ylabel('UV row')
fig.suptitle(
    'Downstream state after 4 seconds | same profile, fields, graph and GPU solver\nOnly reconstructed meso height and its derived geometry differ',
    fontsize=13,
)
fig.savefig(figures / 'transport.png')
plt.close(fig)

# Single-shape comparisons are distinct interventions, not a pooled accuracy/performance ranking.
selected = [
    c
    for c in comparisons
    if c['label']
    in [
        'production_vs_LS',
        'production_vs_LS_256',
        'production_vs_LS_cavity',
        'dt_sensitivity_60_120',
        'grid_sensitivity_64_128',
        'grid_sensitivity_128_256',
    ]
]
labels = [c['label'] + ' | ' + c['a'].replace('gpu-', '') for c in selected]
values = [c['state_l1_pct_supply'][c['channels'].index('waterfilm')] for c in selected]
fig, ax = plt.subplots(figsize=(12, 5.2), layout='constrained')
ax.barh(labels, values, color=['#4779a8'] * 5 + ['#b58639'] * (len(values) - 5))
ax.invert_yaxis()
ax.set_xlabel('Waterfilm L1 difference / supplied ice + water (%)')
ax.set_title('Observed sensitivities at 4 s | differences are not all numerical errors')
for i, v in enumerate(values):
    ax.text(v, i, f' {v:.4g}%', va='center')
ax.set_xlim(0, max(values) * 1.18)
fig.savefig(figures / 'sensitivities.png')
plt.close(fig)

# endregion
