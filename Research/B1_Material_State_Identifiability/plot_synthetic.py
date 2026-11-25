"""Render the synthetic controls from Results.json."""

import json
from pathlib import Path

import matplotlib

matplotlib.use("Agg")

import matplotlib.pyplot as plt
import numpy as np

FOLDER = Path(__file__).parent
COLORS = ["#0072B2", "#D55E00", "#009E73", "#8B519C"]


def plot_history(axis: plt.Axes, audit: dict) -> None:
    time = np.arange(0, 4.005, 0.01)
    total = np.exp(-0.15 * time)
    for rate, color in zip(audit["rates"], COLORS):
        branch = total.copy()
        branch[time >= 2] -= 0.8 * np.exp(-2 * rate) * total[time >= 2]
        branch[time >= 3] += 0.25 * np.exp(-0.15 * (time[time >= 3] - 3))
        axis.plot(time, branch, color=color, lw=2, label=f"Intervention, a = {rate:g}")
    axis.plot(time, total, "--", color="#444444", lw=1.5, label="Passive, both")
    for position, label in [(2, "Wipe"), (3, "Rewet")]:
        axis.axvline(position, color="#AAAAAA", linestyle=":", zorder=0)
        axis.text(position + 0.04, 1.015, label, color="#444444", fontsize=9)
    axis.set(
        title="A   Same passive signal, different responses",
        xlabel="Synthetic time",
        ylabel="Observed total (normalized)",
        xlim=(0, 4.04),
        ylim=(0.2, 1.10),
    )
    axis.legend(frameon=False, fontsize=9, loc="lower left")


def plot_errors(axis: plt.Axes, comparison: dict) -> None:
    names = ["single", "history_lut", "reservoirs", "play"]
    labels = ["Single state", "History LUT", "Two reservoirs", "Play hysteresis"]
    centers = np.arange(3)
    width = 0.18
    for index, (name, label, color) in enumerate(zip(names, labels, COLORS)):
        seed_errors = np.array(
            [
                [run[name]["post_first_macro_mae"] for run in levels["0.01"].values()]
                for levels in comparison.values()
            ]
        )
        median = np.median(seed_errors, axis=1)
        extent = [median - seed_errors.min(axis=1), seed_errors.max(axis=1) - median]
        axis.bar(
            centers + (index - 1.5) * width,
            median,
            width * 0.9,
            yerr=extent,
            capsize=2,
            color=color,
            label=label,
        )
    axis.set(
        title="B   Prediction on unseen action schedules",
        ylabel="Post-action MAE (normalized)",
        xticks=centers,
        xticklabels=[
            "Single-state\ncontrol",
            "Reservoir\ncontrol",
            "Hysteresis\ncontrol",
        ],
    )
    axis.legend(frameon=False, fontsize=9, loc="upper left")
    axis.grid(axis="y", color="#E0E0E0")
    axis.set_axisbelow(True)


def main() -> None:
    results = json.loads((FOLDER / "Results.json").read_text(encoding="utf-8"))
    figure, (history, errors) = plt.subplots(
        1, 2, figsize=(11, 4.2), layout="constrained"
    )
    plot_history(history, results["observation_audit"])
    plot_errors(errors, results["model_comparison"]["per_run"])
    for axis in (history, errors):
        axis.spines[["right", "top"]].set_visible(False)
    figure.savefig(str(FOLDER / "Synthetic_Results.png"), dpi=180)
    plt.close(figure)


if __name__ == "__main__":
    main()
