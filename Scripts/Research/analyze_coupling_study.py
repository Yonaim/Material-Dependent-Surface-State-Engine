"""Compute reproducible paired effects and numerical sensitivities, without promoting them to novelty."""

# ============================================================
# 00. Imports and Module Configuration
# ============================================================
# region 00_Imports_and_Module_Configuration

import json
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2] / "Build/ResearchPython"))
import numpy as np
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from run_coupling_study import ROOT, STUDY, EXPERIMENT, write

manifest = json.loads((EXPERIMENT / "manifest.json").read_text(encoding="utf-8"))
runs = {r["id"]: r for r in manifest["runs"] if r["status"] == "completed"}


# endregion

# ============================================================
# 01. Processing Helpers
# ============================================================
# region 01_Processing_Helpers


def folder(key):
    return ROOT / runs[key]["output"]


def states(key, filename="final.f32"):
    s = runs[key]["summary"]
    return (
        np.fromfile(folder(key) / filename, dtype="<f4")
        .reshape(s["texels"], len(s["channels"]))
        .astype(np.float64)
    )


def series(key):
    return [
        json.loads(line)
        for line in (folder(key) / "series.jsonl").read_text(encoding="utf-8").splitlines()
    ]


def compare(a, b):
    x, y = states(a), states(b)
    s = runs[a]["summary"]
    names = s["channels"]
    if names != runs[b]["summary"]["channels"] or x.shape != y.shape:
        raise ValueError("Paired comparison requires identical channel layout and graph")
    initial = states(a, "bootstrap.f32")
    denom = initial.sum(axis=0) + np.array(s["cumulative_input"])
    mass = s["initial_ice_water"] + s["cumulative_input"][names.index("waterfilm")]
    for name in ["ice", "waterfilm"]:
        denom[names.index(name)] = mass
    l1 = np.abs(x - y).sum(axis=0)
    end_a, end_b = series(a)[-1], series(b)[-1]
    return dict(
        a=a,
        b=b,
        bit_identical=bool(np.array_equal(x, y)),
        channels=names,
        l1=l1.tolist(),
        l1_pct_supply=(100 * l1 / np.maximum(denom, 1e-20)).tolist(),
        max_abs=np.abs(x - y).max(axis=0).tolist(),
        total_difference=(x.sum(axis=0) - y.sum(axis=0)).tolist(),
        downstream_difference=(
            np.array(end_a["downstream_sums"]) - end_b["downstream_sums"]
        ).tolist(),
        extents_a=end_a["cumulative_extents"],
        extents_b=end_b["cumulative_extents"],
    )


# endregion

# ============================================================
# 02. Script Execution
# ============================================================
# region 02_Script_Execution

comparisons = []
for case in ["plate", "bunny", "mud"]:
    prefix = f"{case}-128-60-"
    for v in "BCEF":
        if prefix + "D" in runs and prefix + v in runs:
            comparisons.append(compare(prefix + "D", prefix + v))
for hz1, hz2 in [(60, 120), (120, 240)]:
    for v in "ABCDEF":
        a, b = f"plate-128-{hz1}-{v}", f"plate-128-{hz2}-{v}"
        if a in runs and b in runs:
            comparisons.append(compare(a, b))
for b in ["plate-128-60-D-repeat", "plate-128-60-D-dirty"]:
    if "plate-128-60-D" in runs and b in runs:
        comparisons.append(compare("plate-128-60-D", b))
if "smoke-D" in runs and "smoke-D-ledger-off" in runs:
    comparisons.append(compare("smoke-D", "smoke-D-ledger-off"))

initial_checks = {}
for prefix in [
    "smoke-",
    "plate-128-60-",
    "plate-128-120-",
    "plate-128-240-",
    "plate-256-60-",
    "bunny-128-60-",
    "mud-128-60-",
]:
    selected = [runs[prefix + v] for v in "ABCDEF" if prefix + v in runs]
    if selected:
        initial_checks[prefix] = dict(
            initial_states_equal=len({r["summary"]["initial_hash"] for r in selected}) == 1
        )
    if prefix + "D" in runs and prefix + "E" in runs:
        initial_checks[prefix]["DE_initial_geometry_equal"] = (
            runs[prefix + "D"]["summary"]["initial_geometry_hash"]
            == runs[prefix + "E"]["summary"]["initial_geometry_hash"]
        )

spatial = []
for variant in "ABCDEF":
    coarse, fine = f"plate-128-60-{variant}", f"plate-256-60-{variant}"
    if coarse not in runs or fine not in runs:
        continue
    s = runs[coarse]["summary"]
    k = len(s["channels"])
    # MarbleCube has the same six full-square UV surfaces at both resolutions.
    restriction = states(fine).reshape(6, 128, 2, 128, 2, k).sum(axis=(2, 4)).reshape(-1, k)
    delta = np.abs(states(coarse) - restriction).sum(axis=0)
    mass = s["initial_ice_water"] + s["cumulative_input"][s["channels"].index("waterfilm")]
    denom = np.array(s["cumulative_input"])
    denom[s["channels"].index("ice")] = mass
    denom[s["channels"].index("waterfilm")] = mass
    initial_fine = (
        states(fine, "bootstrap.f32").reshape(6, 128, 2, 128, 2, k).sum(axis=(2, 4)).reshape(-1, k)
    )
    area_coarse = np.fromfile(folder(coarse) / "areas.f32", dtype="<f4").astype(np.float64)
    area_fine = (
        np.fromfile(folder(fine) / "areas.f32", dtype="<f4")
        .astype(np.float64)
        .reshape(6, 128, 2, 128, 2)
        .sum(axis=(2, 4))
        .reshape(-1)
    )
    spatial.append(
        dict(
            variant=variant,
            channels=s["channels"],
            restriction="2x2 area-cell sums on matching full-square MarbleCube UV surfaces",
            l1_pct_supply=(100 * delta / np.maximum(denom, 1e-20)).tolist(),
            initial_l1_pct_supply=(
                100
                * np.abs(states(coarse, "bootstrap.f32") - initial_fine).sum(axis=0)
                / np.maximum(denom, 1e-20)
            ).tolist(),
            area_l1_relative=float(np.abs(area_coarse - area_fine).sum() / area_coarse.sum()),
            input_total_difference=(
                np.array(s["cumulative_input"]) - runs[fine]["summary"]["cumulative_input"]
            ).tolist(),
        )
    )

terminal = []
for key, r in runs.items():
    s = r["summary"]
    row = series(key)[-1]
    terminal.append(
        dict(
            id=key,
            channels=s["channels"],
            sums=row["state_sums"],
            downstream=row["downstream_sums"],
            extents=row["cumulative_extents"],
            crossing=row["cumulative_crossing"],
            geometry_max_height_change=row["geometry_max_height_change"],
            mass_relative=r["relative_mass_residual"],
            local_budget=s["max_local_budget_residual"],
            invalid=sum(
                s[k]
                for k in [
                    "nonfinite",
                    "negative",
                    "reaction_budget",
                    "reaction_arithmetic",
                    "frozen_failures",
                ]
            ),
            gpu_median_ms=s["gpu_median_ms"],
            gpu_p95_ms=s["gpu_p95_ms"],
        )
    )

regional = []
for case in ["plate", "bunny", "mud"]:
    d, e = f"{case}-128-60-D", f"{case}-128-60-E"
    if d not in runs or e not in runs:
        continue
    names = runs[d]["summary"]["channels"]
    wi = names.index("waterfilm")
    ii = names.index("ice")
    region = np.fromfile(folder(d) / "v.f32", dtype="<f4") < -0.1
    xd, xe = states(d), states(e)
    rd, re = series(d)[-1], series(e)[-1]
    water_delta = float((xd[region, wi] - xe[region, wi]).sum())
    # Ice has no transport, decay or post-bootstrap input in these exact profiles.
    local_melt_delta = float((xe[region, ii] - xd[region, ii]).sum())
    crossing_delta = rd["cumulative_crossing"][wi] - re["cumulative_crossing"][wi]
    regional.append(
        dict(
            case=case,
            water_downstream_D_minus_E=water_delta,
            net_transport_D_minus_E=crossing_delta,
            local_melting_D_minus_E=local_melt_delta,
            residual=water_delta - local_melt_delta - crossing_delta,
            caveat="A regional water budget, not proof that water alone mediates the heat effect",
        )
    )

contrast_sensitivity = []
for ra, ha, rb, hb in [(128, 60, 128, 120), (128, 120, 128, 240), (128, 60, 256, 60)]:
    keys = [f"plate-{r}-{h}-{v}" for r, h in [(ra, ha), (rb, hb)] for v in "DE"]
    if not all(key in runs for key in keys):
        continue
    arrays = [states(key) for key in keys]
    s = runs[keys[0]]["summary"]
    names = s["channels"]
    k = len(names)
    if rb != ra:
        arrays[2:] = [
            x.reshape(6, 128, 2, 128, 2, k).sum(axis=(2, 4)).reshape(-1, k) for x in arrays[2:]
        ]
    ca, cb = arrays[0] - arrays[1], arrays[2] - arrays[3]
    denom = states(keys[0], "bootstrap.f32").sum(axis=0) + np.array(s["cumulative_input"])
    for name in ["ice", "waterfilm"]:
        denom[names.index(name)] = (
            s["initial_ice_water"] + s["cumulative_input"][names.index("waterfilm")]
        )
    error = np.abs(ca - cb).sum(axis=0)
    contrast_sensitivity.append(
        dict(
            coarse=keys[:2],
            refined=keys[2:],
            channels=names,
            contrast_l1_pct_supply_coarse=(100 * np.abs(ca).sum(axis=0) / denom).tolist(),
            contrast_l1_pct_supply_refined=(100 * np.abs(cb).sum(axis=0) / denom).tolist(),
            contrast_change_l1_pct_supply=(100 * error / denom).tolist(),
            change_relative_to_coarse_contrast=(
                error / np.maximum(np.abs(ca).sum(axis=0), 1e-20)
            ).tolist(),
        )
    )

extension = []
for variant in "BDE":
    a, b = f"plate-128-60-{variant}", f"mud-128-60-{variant}"
    if a not in runs or b not in runs:
        continue
    sa, sb = runs[a]["summary"], runs[b]["summary"]
    x, y = states(a), states(b)
    aligned = y[:, [sb["channels"].index(n) for n in sa["channels"]]]
    denom = np.array(sa["cumulative_input"])
    for n in ["ice", "waterfilm"]:
        denom[sa["channels"].index(n)] = (
            sa["initial_ice_water"] + sa["cumulative_input"][sa["channels"].index("waterfilm")]
        )
    extension.append(
        dict(
            variant=variant,
            channels=sa["channels"],
            shared_channels_bit_identical=bool(np.array_equal(x, aligned)),
            shared_channels_l1_pct_supply=(100 * np.abs(x - aligned).sum(axis=0) / denom).tolist(),
            note="Mud added with predeclared existing profile coefficients; no new reaction rule tested",
        )
    )

selected = json.loads((EXPERIMENT / "execution-amendment.json").read_text())["selected"]
write(
    EXPERIMENT / "comparisons.json",
    dict(
        status="completed_scoped_subset" if all(key in runs for key in selected) else "partial",
        selected_conditions=len(selected),
        remaining_selected=[key for key in selected if key not in runs],
        completed_runs=len(runs),
        attempts=len(manifest["runs"]),
        initial_checks=initial_checks,
        comparisons=comparisons,
        spatial=spatial,
        terminal=terminal,
        regional=regional,
        contrast_sensitivity=contrast_sensitivity,
        extension=extension,
        limitations=[
            "No modular baseline implemented; no H3 superiority result",
            "One analytical deposition design; no authoring study",
            "No normal-map meso, no rendering quality assessment",
            "Diagnostic GPU timings are not independent repeated app performance",
        ],
    ),
)

images = EXPERIMENT / "figures"
images.mkdir(exist_ok=True)
plt.rcParams.update({"font.size": 10, "axes.spines.top": False, "axes.spines.right": False})
colors = {v: plt.get_cmap("tab10")(i) for i, v in enumerate("ABCDEF")}
for case in ["plate", "bunny", "mud"]:
    prefix = f"{case}-128-60-"
    variants = [v for v in "ABCDEF" if prefix + v in runs]
    if not all(v in variants for v in "BDE"):
        continue
    fig, axes = plt.subplots(1, 3, figsize=(14, 4))
    for v in variants:
        key = prefix + v
        rows = series(key)
        names = runs[key]["summary"]["channels"]
        ts = [x["time"] for x in rows]
        for ax, name, field in zip(
            axes, ["ice", "waterfilm", "heat"], ["state_sums", "downstream_sums", "state_sums"]
        ):
            ax.plot(
                ts, [x[field][names.index(name)] for x in rows], label=v, lw=1.6, color=colors[v]
            )
            ax.set(title=f"{name} / {field}", xlabel="Simulation time (s)", ylabel="State amount")
    axes[-1].legend(ncol=2)
    fig.suptitle(f"{case}: matched-input {','.join(variants)}, 128 grid, 60 Hz")
    fig.tight_layout()
    fig.savefig(images / f"{case}-trajectories.png", dpi=150)
    plt.close(fig)

    rows = (len(variants) + 2) // 3
    fig, axes = plt.subplots(
        rows, 3, figsize=(12, 3.5 * rows), sharex=True, sharey=True, squeeze=False
    )
    key = prefix + "D"
    base = folder(key)
    u = np.fromfile(base / "u.f32", dtype="<f4")
    vcoord = np.fromfile(base / "v.f32", dtype="<f4")
    patch = np.fromfile(base / "patch.f32", dtype="<f4") > 0
    area = np.fromfile(base / "areas.f32", dtype="<f4") / (1 / 256**2)
    water = runs[key]["summary"]["channels"].index("waterfilm")
    fields = [states(prefix + v)[:, water] / np.maximum(area, 1e-20) for v in variants]
    vmax = max(float(f[patch].max()) for f in fields)
    for ax, label, field in zip(axes.flat, variants, fields):
        im = ax.scatter(
            u[patch],
            vcoord[patch],
            c=field[patch],
            s=2,
            cmap="viridis",
            vmin=0,
            vmax=vmax,
            rasterized=True,
        )
        ax.axhline(-0.1, color="white", ls="--", lw=0.7)
        ax.set(title=label, xlabel="u", ylabel="v")
    for ax in list(axes.flat)[len(variants) :]:
        ax.set_visible(False)
    fig.suptitle(f"{case}: Waterfilm per reference area, t=14 s (deposition patch view)")
    fig.subplots_adjust(right=0.86, hspace=0.3)
    cax = fig.add_axes([0.9, 0.17, 0.02, 0.65])
    fig.colorbar(im, cax=cax)
    fig.savefig(images / f"{case}-waterfilm.png", dpi=160)
    plt.close(fig)

if all(f"plate-128-60-{v}" in runs for v in "DE"):
    d, e = "plate-128-60-D", "plate-128-60-E"
    wi = runs[d]["summary"]["channels"].index("waterfilm")
    rd, re = series(d), series(e)
    ts = np.array([r["time"] for r in rd])
    fig, axes = plt.subplots(1, 2, figsize=(12, 4))
    for label, rows in [("D: dynamic", rd), ("E: frozen", re)]:
        axes[0].plot(
            ts, [r["cumulative_crossing"][wi] for r in rows], label=label, color=colors[label[0]]
        )
    axes[0].set(
        title="Net Waterfilm crossing v = -0.1",
        xlabel="Simulation time (s)",
        ylabel="Cumulative State amount",
    )
    axes[0].legend()
    labels = ["D vs E (60 Hz)"]
    values = [next(c for c in comparisons if c["a"] == d and c["b"] == e)["l1_pct_supply"][wi]]
    for b, label in [
        ("plate-128-120-D", "D: 60 to 120 Hz"),
        ("plate-128-60-D-dirty", "D: full to dirty"),
    ]:
        found = next((c for c in comparisons if c["a"] == d and c["b"] == b), None)
        if found:
            labels.append(label)
            values.append(found["l1_pct_supply"][wi])
    if spatial:
        labels.append("D: 128 to 256 grid")
        values.append(next(s for s in spatial if s["variant"] == "D")["l1_pct_supply"][wi])
    axes[1].barh(labels, values, color=[colors['D']] + ['#777777'] * (len(values) - 1))
    axes[1].set_xscale("log")
    axes[1].invert_yaxis()
    for i, value in enumerate(values):
        axes[1].text(value * 1.25, i, f"{value:.6g}%", va="center", fontsize=9)
    axes[1].set(
        xlim=(1e-6, 30),
        xlabel="Waterfilm L1 / available material (%)",
        title="Effect and observed numerical sensitivity",
    )
    fig.tight_layout()
    fig.savefig(images / "plate-contrast.png", dpi=160)
    plt.close(fig)

lines = [
    "# Automatic comparison tables",
    "",
    "These are diagnostic calculations, not a novelty verdict.",
    "",
    "| Pair | Heat L1 / input (%) | Ice L1 / available material (%) | Waterfilm L1 / available material (%) | Bit identical |",
    "|---|---:|---:|---:|---|",
]
for c in comparisons:
    values = {n: v for n, v in zip(c["channels"], c["l1_pct_supply"])}
    lines.append(
        f"| {c['a']} vs {c['b']} | {values['heat']:.6g} | {values['ice']:.6g} | {values['waterfilm']:.6g} | {c['bit_identical']} |"
    )
(EXPERIMENT / "comparisons.md").write_text("\n".join(lines) + "\n", encoding="utf-8")
print(
    json.dumps(
        dict(completed=len(runs), initial_checks=initial_checks, comparisons=comparisons), indent=2
    )
)

# endregion
