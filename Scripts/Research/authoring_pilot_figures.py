"""저작 pilot의 비교 자료에서 분포와 결과 그림을 생성한다.

Display original checkpoints and numeric masks; no independent quality scoring.
"""

import sys
from analyze_authoring_pilot import load, OUT, np
from authoring_study import write
from PIL import Image, ImageDraw
import matplotlib

matplotlib.use('Agg')
import matplotlib.pyplot as plt


def main():
    plan, prefix, names, states, initial, cov, area, top, audit = load('candidate-01-retry')
    folder = OUT / 'figures'
    folder.mkdir(exist_ok=True)
    sheet = Image.new('RGB', (1280, 782), '#eeeeee')
    draw = ImageDraw.Draw(sheet)
    for k, s in enumerate([60, 360, 720, 960]):
        p = str(prefix) + f'.step-{s}.ppm'
        im = Image.open(p)
        im.save(folder / f'candidate-01-{s//60:02d}s.png')
        x = (k % 2) * 640
        y = (k // 2) * 386
        sheet.paste(im.resize((640, 360)), (x, y + 22))
        draw.text((x + 8, y + 5), f'{s/60:g}s / actual solver step {s}', fill='black')
    draw.text(
        (8, 773),
        'AI-assisted candidate 01. Original lit views; no human quality assessment.',
        fill='black',
    )
    sheet.save(folder / 'candidate-01-sequence.png')
    ice = names.index('ice')
    water = names.index('waterfilm')
    wet = names.index('absorbedwetness')
    steps = list(cov)
    times = np.array(steps) / 60
    outside = top & (cov[0][:, ice] < 0.1)
    metrics = []
    for s in steps:
        metrics.append(
            [
                area[outside & (cov[s][:, water] >= 0.1)].sum() / area[top].sum() * 100,
                area[outside & (cov[s][:, wet] >= 0.25)].sum() / area[top].sum() * 100,
            ]
        )
    values = np.array(metrics)
    fig, axs = plt.subplots(1, 2, figsize=(10, 3.5))
    q = np.array(
        [initial[:, ice].sum() if s == 0 else states[s][:, ice].sum(dtype='float64') for s in steps]
    )
    axs[0].plot(times, q / q[0], marker='o')
    axs[0].axvline(4, color='gray', ls='--', label='Supply ends')
    axs[0].plot([6, 6], [0.05, 0.80], color='green', lw=4, label='6s target interval')
    axs[0].set(
        xlabel='Simulation time (s)', ylabel='Total Ice / initial Ice', title='B: Ice progression'
    )
    axs[0].legend(fontsize=8)
    axs[1].plot(times, values[:, 0], '-o', label='Water coverage >= .10')
    axs[1].plot(times, values[:, 1], '-o', label='Wetness coverage >= .25')
    axs[1].axhline(1, color='gray', ls='--', label='1% task target')
    axs[1].set(
        xlabel='Simulation time (s)',
        ylabel='% of reference top-face area',
        title='Outside initial Ice footprint',
    )
    axs[1].legend(fontsize=8)
    fig.suptitle('Candidate 01: task predicates, not physical validation or comparative quality')
    fig.tight_layout()
    fig.savefig(folder / 'candidate-01-metrics.png', dpi=150)
    plt.close(fig)
    m = np.load(prefix.parent / 'masks.npz')
    idx = audit['target_instance']
    pos = np.fromfile(str(prefix) + f'.step-0.instance-{idx}-position.bin', dtype='<f4').reshape(
        -1, 4
    )
    fig, axs = plt.subplots(1, 3, figsize=(10, 3.4))
    for ax, channel, title in zip(axs, [ice, water, wet], ['Ice', 'Waterfilm', 'Absorbed wetness']):
        scat = ax.scatter(
            pos[top, 0], pos[top, 1], c=cov[960][top, channel], s=2, vmin=0, vmax=1, cmap='viridis'
        )
        mask = m['persistent_trace']
        if np.any(mask):
            ax.scatter(
                pos[mask, 0], pos[mask, 1], s=4, facecolors='none', edgecolors='red', linewidths=0.4
            )
        ax.set(aspect='equal', title=title, xlabel='Mesh-local x', ylabel='Mesh-local y')
    fig.colorbar(scat, ax=axs.ravel().tolist(), fraction=0.02, label='Clamped State / Capacity')
    fig.suptitle(
        '16s numerical maps; red = common 12/14/16s qualifying trace. Not a lit appearance.'
    )
    fig.savefig(folder / 'candidate-01-state-maps.png', dpi=150, bbox_inches='tight')
    plt.close(fig)
    write(
        OUT / 'figures.json',
        dict(
            scope='Original views and numeric plots only; not independent quality judgement',
            files=[str(p) for p in folder.iterdir() if p.is_file()],
        ),
    )
    print('Original checkpoint images and numeric figures saved')


if __name__ == '__main__':
    main()
