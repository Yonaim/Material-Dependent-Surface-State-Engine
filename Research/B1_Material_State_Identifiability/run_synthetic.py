"""Fit established model families to reproducible synthetic interventions."""

from __future__ import annotations

import argparse
import json
import platform
from dataclasses import dataclass
from pathlib import Path
from time import perf_counter_ns
from typing import Callable

import numpy as np
import scipy
from numpy.typing import NDArray
from scipy.optimize import least_squares

Array = NDArray[np.float64]
Simulator = Callable[[Array, Array], Array]
Action = tuple[int, str, float]
DT = 0.25
STEPS = 25
SEEDS = tuple(range(601, 609))
NOISE_LEVELS = (0.0, 0.01, 0.03)
TRAIN_REPEATS = 3

# Tuple entries are (step, action, nominal intensity). All runs start with dose 1.
TRAIN = {
    "dry": (),
    "wipe_early_light": ((4, "wipe", 0.60),),
    "wipe_early_heavy": ((4, "wipe", 1.0),),
    "wipe_late_light": ((12, "wipe", 0.60),),
    "wipe_late_heavy": ((12, "wipe", 1.0),),
    "rewet_early_small": ((4, "wet", 0.15),),
    "rewet_early_large": ((4, "wet", 0.40),),
    "rewet_late_small": ((12, "wet", 0.15),),
    "rewet_late_large": ((12, "wet", 0.40),),
    "rewet_then_wipe": ((4, "wet", 0.25), (12, "wipe", 0.80)),
}
TEST = {
    "dry_then_rewet": ((8, "wet", 0.25),),
    "wipe_then_rewet": ((8, "wipe", 0.80), (12, "wet", 0.25)),
    "wipe_rewet_wipe": ((8, "wipe", 0.80), (12, "wet", 0.25), (16, "wipe", 0.80)),
}


def encode(schedules: dict[str, tuple[Action, ...]]) -> Array:
    actions = np.zeros((len(schedules), STEPS, 2))
    for row, schedule in enumerate(schedules.values()):
        for step, action, strength in schedule:
            actions[row, step, int(action == "wipe")] = strength
    return actions


def single_state(params: Array, actions: Array) -> Array:
    decay, wipe_gain, wipe_slope, wet_gain = params
    water = np.ones(len(actions))
    result = [water]
    retain = np.exp(-DT * decay)
    for wet, wipe in actions[:, 1:, :].transpose(1, 2, 0):
        water = water * retain
        fraction = np.clip(wipe_gain + wipe_slope * (water - 0.5), 0, 1)
        water = water * (1 - wipe * fraction) + wet_gain * wet
        result.append(water)
    return np.stack(result, axis=1)


def reservoirs(params: Array, actions: Array) -> Array:
    film_decay, pore_decay, absorption, wipe_gain, wet_gain = params
    film = np.ones(len(actions))
    pore = np.zeros(len(actions))
    result = [film]
    film_retain = np.exp(-DT * (film_decay + absorption))
    pore_retain = np.exp(-DT * pore_decay)
    rate_gap = film_decay + absorption - pore_decay
    transfer_integral = (
        DT if abs(rate_gap) < 1e-12 else -np.expm1(-DT * rate_gap) / rate_gap
    )
    transfer = absorption * pore_retain * transfer_integral
    for wet, wipe in actions[:, 1:, :].transpose(1, 2, 0):
        pore = pore * pore_retain + transfer * film
        film = film * film_retain * (1 - wipe_gain * wipe) + wet_gain * wet
        result.append(film + pore)
    return np.stack(result, axis=1)


def play_constitutive(params: Array, actions: Array) -> Array:
    decay, wipe_gain, wet_gain, radius, fraction_offset, fraction_slope = params
    water = np.ones(len(actions))
    memory = water - radius
    result = [water]
    retain = np.exp(-DT * decay)
    for wet, wipe in actions[:, 1:, :].transpose(1, 2, 0):
        water = water * retain
        memory = np.clip(memory, water - radius, water + radius)
        fraction = np.clip(fraction_offset + fraction_slope * (water - memory), 0, 1)
        water = water * (1 - wipe * wipe_gain * fraction) + wet_gain * wet
        memory = np.clip(memory, water - radius, water + radius)
        result.append(water)
    return np.stack(result, axis=1)


@dataclass(frozen=True)
class ParametricModel:
    simulator: Simulator
    start: tuple[float, ...]
    lower: tuple[float, ...]
    upper: tuple[float, ...]
    states: int

    def fit(self, actions: Array, observations: Array) -> Array:
        def residual(params: Array) -> Array:
            return (self.simulator(params, actions) - observations).ravel()

        bounds = (np.asarray(self.lower), np.asarray(self.upper))
        low = bounds[0] + 0.25 * (bounds[1] - bounds[0])
        high = bounds[0] + 0.65 * (bounds[1] - bounds[0])
        fits = [
            least_squares(residual, start, bounds=bounds, max_nfev=120)
            for start in (np.asarray(self.start), low, high)
        ]
        converged = [fit for fit in fits if fit.success]
        if not converged:
            raise RuntimeError("All calibration starts failed to converge.")
        return min(converged, key=lambda fit: fit.cost).x


MODELS = {
    "single": ParametricModel(
        single_state, (0.2, 0.5, 0.0, 0.9), (0, 0, -2, 0), (1, 1, 2, 2), 1
    ),
    "reservoirs": ParametricModel(
        reservoirs, (0.2, 0.2, 0.4, 0.7, 1.0), (0, 0, 0, 0, 0), (1, 1, 2, 1, 2), 2
    ),
    "play": ParametricModel(
        play_constitutive,
        (0.2, 0.8, 0.9, 0.2, 0.5, 1.0),
        (0, 0.1, 0, 0.02, 0.05, -3),
        (1, 1, 2, 0.45, 0.95, 3),
        2,
    ),
}


@dataclass
class HistoryLUT:
    """Interpolate per-action transitions and compose them for unseen orders."""

    retain: float
    tables: tuple[Array, Array]
    BANDWIDTH = (0.24, 1.5, 0.35, 0.3, 1.5)

    @classmethod
    def fit(cls, actions: Array, observations: Array) -> HistoryLUT:
        before = observations[:, :-1][np.all(actions[:, 1:] == 0, axis=2)]
        after = observations[:, 1:][np.all(actions[:, 1:] == 0, axis=2)]
        retain = float(np.clip((before @ after) / (before @ before), 0, 1))

        # Aggregate repeats of a single action knot to one LUT record.
        rows: tuple[list[list[float]], list[list[float]]] = ([], [])
        for schedule, trajectory in zip(actions, observations):
            last_wet, added, wipes = 0, 0.0, 0
            for step in np.flatnonzero(schedule.any(axis=1)):
                wet, wipe = schedule[step]
                water = max(0.02, retain * trajectory[step - 1])
                if wet:
                    gain = (trajectory[step] - water) / wet
                    rows[0].append(
                        [water, (step - last_wet) * DT, added, wet, wipes, gain]
                    )
                    last_wet, added = step, added + wet
                if wipe:
                    fraction = (water - trajectory[step]) / (water * wipe)
                    rows[1].append(
                        [water, (step - last_wet) * DT, added, wipe, wipes, fraction]
                    )
                    wipes += 1
        return cls(retain, (np.asarray(rows[0]), np.asarray(rows[1])))

    def lookup(self, table: Array, context: list[float]) -> float:
        distance = np.sum(((table[:, :-1] - context) / self.BANDWIDTH) ** 2, axis=1)
        weights = np.exp(-0.5 * (distance - distance.min()))
        return float(np.average(table[:, -1], weights=weights))

    def predict(self, actions: Array) -> Array:
        predictions = []
        for schedule in actions:
            water, last_wet, added, wipes = 1.0, 0, 0.0, 0
            trajectory = [water]
            for step, (wet, wipe) in enumerate(schedule[1:], start=1):
                water *= self.retain
                age = (step - last_wet) * DT
                if wet:
                    gain = self.lookup(self.tables[0], [water, age, added, wet, wipes])
                    water += float(np.clip(gain, 0, 2)) * wet
                    last_wet, added = step, added + wet
                if wipe:
                    fraction = self.lookup(
                        self.tables[1], [water, age, added, wipe, wipes]
                    )
                    water *= 1 - float(np.clip(fraction, 0, 1)) * wipe
                    wipes += 1
                trajectory.append(water)
            predictions.append(trajectory)
        return np.asarray(predictions)


def summarize_predictions(
    predictions: Array, truth: Array, steps: dict[str, tuple[Action, ...]]
) -> dict:
    errors = np.abs(predictions - truth)
    by_sequence = {}
    for row, (name, actions) in enumerate(steps.items()):
        first, last = actions[0][0], actions[-1][0]
        by_sequence[name] = {
            "post_first_mae": float(np.mean(errors[row, first:])),
            "post_final_mae": float(np.mean(errors[row, last:])),
            "max_post_first_error": float(np.max(errors[row, first:])),
        }
    return {
        "post_first_macro_mae": float(
            np.mean([sequence["post_first_mae"] for sequence in by_sequence.values()])
        ),
        "sequences": by_sequence,
    }


def compare_models() -> dict:
    train = encode(TRAIN)
    test = encode(TEST)
    assert set(TRAIN.values()).isdisjoint(TEST.values())
    plants = {
        "single_control": (single_state, (0.15, 0.55, 0.18, 1.0)),
        "reservoirs_control": (reservoirs, (0.15, 0.15, 0.65, 0.85, 1.0)),
        "play_control": (play_constitutive, (0.15, 0.92, 1.0, 0.32, 0.52, 1.40)),
    }
    results = {}
    example_curves = {}
    for plant_name, (generator, params) in plants.items():
        truth = generator(np.asarray(params), test)
        clean = generator(np.asarray(params), train)
        for sigma in NOISE_LEVELS:
            for seed in SEEDS if sigma else SEEDS[:1]:
                rng = np.random.default_rng(seed)
                captures = clean + rng.normal(0, sigma, (TRAIN_REPEATS, *clean.shape))
                observations = np.mean(captures, axis=0)
                current = {}
                for name in (*MODELS, "history_lut"):
                    started = perf_counter_ns()
                    if name == "history_lut":
                        lut = HistoryLUT.fit(train, observations)
                        predict = lut.predict
                        stats = {
                            "fitted_numeric_values": 1
                            + sum(table.size for table in lut.tables),
                            "lut_rows": [len(table) for table in lut.tables],
                            "running_state_values": 4,
                            "parameters_and_state_float64_bytes": 8
                            * (5 + sum(table.size for table in lut.tables)),
                        }
                    else:
                        model = MODELS[name]
                        fitted = model.fit(train, observations)

                        def predict(actions: Array) -> Array:
                            return model.simulator(fitted, actions)

                        stats = {
                            "parameters": fitted.tolist(),
                            "fitted_numeric_values": len(fitted),
                            "running_state_values": model.states,
                            "parameters_and_state_float64_bytes": 8
                            * (len(fitted) + model.states),
                        }
                    fit_ms = (perf_counter_ns() - started) / 1e6
                    prediction = predict(test)
                    elapsed = []
                    for _ in range(20):
                        started = perf_counter_ns()
                        predict(test)
                        elapsed.append((perf_counter_ns() - started) / 1e3 / len(test))
                    current[name] = {
                        **stats,
                        **summarize_predictions(prediction, truth, TEST),
                        "fit_ms": fit_ms,
                        "calibration_max_error_to_latent_truth": float(
                            np.max(np.abs(predict(train) - clean))
                        ),
                        "predict_us_per_trajectory_p50": float(np.median(elapsed)),
                        "predict_us_per_trajectory_p95": float(
                            np.percentile(elapsed, 95)
                        ),
                    }
                    if sigma == 0.01 and seed == SEEDS[0]:
                        example_curves.setdefault(plant_name, {"truth": truth.tolist()})
                        example_curves[plant_name][name] = prediction.tolist()
                results.setdefault(plant_name, {}).setdefault(str(sigma), {})[seed] = (
                    current
                )
    return {
        "train_schedules": TRAIN,
        "heldout_schedules": TEST,
        "time_step": DT,
        "samples_per_trajectory": STEPS,
        "train_repeats": TRAIN_REPEATS,
        "plant_parameters": {name: params for name, (_, params) in plants.items()},
        "per_run": results,
        "example_sigma_0.01_seed_601": example_curves,
    }


def same_material_control() -> dict:
    """Match total dose and passive observations using two prior dosing histories."""
    decay, absorption, efficiency = 0.15, 0.65, 0.8
    early_fraction = 1 / (1 + np.exp(-decay))
    histories = (((1.0, 1.0),), ((0.0, early_fraction), (2.0, 1 - early_fraction)))
    times = np.array([2.0, 2.5, 3.0, 3.5, 4.0])
    before_wipe = []
    for history in histories:
        total = sum(dose * np.exp(-decay * (times - at)) for at, dose in history)
        film = sum(
            dose * np.exp(-(decay + absorption) * (times - at)) for at, dose in history
        )
        before_wipe.append((total, film))
    totals, films = np.asarray(before_wipe).transpose(1, 0, 2)
    passive_gap = float(np.max(np.abs(totals[0] - totals[1])))
    post_wipe = totals[:, -1] - efficiency * films[:, -1]
    assert passive_gap < 1e-14 and abs(post_wipe[0] - post_wipe[1]) > 0.01
    return {
        "dosing_time_and_amount": histories,
        "drying_times": times.tolist(),
        "passive_linf_gap": passive_gap,
        "film_and_pore_before_wipe": np.column_stack(
            (films[:, -1], totals[:, -1] - films[:, -1])
        ).tolist(),
        "post_wipe_totals": post_wipe.tolist(),
        "post_wipe_gap": float(np.ptp(post_wipe)),
    }


def audit_observations() -> dict:
    """Compare one extra scalar reading for H's two passive-equivalent hypotheses."""
    decay, efficiency = 0.15, 0.8
    rates = np.array([0.1, 1.0])
    time = 2.0
    water = np.exp(-decay * time)
    film = water * np.exp(-rates * time)
    passive = np.tile(np.exp(-decay * np.linspace(0, 4, 41)), (2, 1))
    target = (water - efficiency * film) * np.exp(-decay * 2) + 0.25 * np.exp(-decay)
    gap = float(np.ptp(target))
    assert np.isclose(gap, 0.3000443087223103, atol=1e-14, rtol=0)
    assert np.allclose(
        -np.log((water - (water - efficiency * film)) / (efficiency * water)) / time,
        rates,
    )

    candidates = {
        "passive_total_at_t2": np.repeat(water, 2),
        "independent_mass_at_t2": np.repeat(water, 2),
        "after_additive_rewet_at_t2": np.repeat(water + 0.25, 2),
        "after_capacity_limited_rewet_at_t2": water
        + np.minimum(0.25, np.maximum(0, 0.70 - (water - film))),
        "film_sensitive_at_t2": film,
        "after_known_wipe_at_t2": water - efficiency * film,
    }
    observations = {}
    for name, means in candidates.items():
        rng = np.random.default_rng(602)
        noise = rng.standard_normal((2, 20_000))
        observations[name] = {"means": means.tolist(), "noise": {}}
        for sigma in (0.01, 0.05, 0.15):
            samples = means[:, None] + sigma * noise
            distances = np.abs(samples[:, :, None] - means[None, None, :])
            # A tie leaves both hypotheses equally plausible.
            if means[0] == means[1]:
                error = 0.5
                prediction_mae = gap / 2
            else:
                error = float(
                    np.mean(np.argmin(distances, axis=2) != np.arange(2)[:, None])
                )
                log_weights = -0.5 * (distances / sigma) ** 2
                log_weights -= np.max(log_weights, axis=2, keepdims=True)
                weights = np.exp(log_weights)
                forecast = (weights @ target) / np.sum(weights, axis=2)
                prediction_mae = float(np.mean(np.abs(forecast - target[:, None])))
            observations[name]["noise"][str(sigma)] = {
                "misclassification_rate": error,
                "posterior_mean_target_mae": prediction_mae,
            }

    # Sensitivities of wipe loss eta*exp(-(k+a)t) to (a, eta).
    def wipe_row(at: float) -> Array:
        mass = np.exp(-(decay + 0.55) * at)
        return np.array([-at * efficiency * mass, mass])

    def film_row(at: float) -> Array:
        return np.array([-at * np.exp(-(decay + 0.55) * at), 0])

    parameter_rows = {
        "one_wipe": np.array([wipe_row(2)]),
        "repeat_same_wipe": np.array([wipe_row(2), wipe_row(2)]),
        "two_independent_wipe_times": np.array([wipe_row(1), wipe_row(3)]),
        "wipe_and_film": np.array([wipe_row(2), film_row(2)]),
    }
    sensitivities = {
        name: {
            "rank": int(np.linalg.matrix_rank(matrix)),
            "singular_values_zero_padded": np.pad(
                np.linalg.svd(matrix, compute_uv=False), (0, max(0, 2 - len(matrix)))
            ).tolist(),
        }
        for name, matrix in parameter_rows.items()
    }
    state_rank = {}
    for separation in (0, 0.001, 0.01, 0.05):
        # q(t) = retain_pore*(f0+p0) + (kf-kp)*f0 times an integral.
        times = np.array([0.0, 1.0, 2.0, 3.0, 4.0])
        absorption = 0.55
        retain = np.exp(-decay * times)
        delta = separation + absorption
        film_column = retain * (1 - separation * (-np.expm1(-delta * times)) / delta)
        matrix = np.column_stack((film_column, retain))
        wipe_row_state = matrix[2] - [efficiency * np.exp(-(decay + delta) * 2), 0]
        augmented = np.vstack((matrix, wipe_row_state))
        state_rank[str(separation)] = {
            "passive_singular_values": np.linalg.svd(matrix, compute_uv=False).tolist(),
            "with_wipe_singular_values": np.linalg.svd(
                augmented, compute_uv=False
            ).tolist(),
        }
    return {
        "same_material_dosing_histories": same_material_control(),
        "rates": rates.tolist(),
        "passive_linf_gap": float(np.max(np.abs(passive[0] - passive[1]))),
        "heldout_targets": target.tolist(),
        "heldout_gap": gap,
        "one_prediction_minimax_error": gap / 2,
        "one_extra_reading": observations,
        "unknown_absorption_and_wipe_efficiency": sensitivities,
        "initial_state_evaporation_rate_sensitivity": state_rank,
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output", type=Path, default=Path(__file__).with_name("Results.json")
    )
    args = parser.parse_args()
    report = {
        "environment": {
            "python": platform.python_version(),
            "platform": platform.platform(),
            "numpy": np.__version__,
            "scipy": scipy.__version__,
        },
        "observation_audit": audit_observations(),
        "model_comparison": compare_models(),
    }
    args.output.write_text(
        json.dumps(report, indent=2, allow_nan=False) + "\n", encoding="utf-8"
    )


if __name__ == "__main__":
    main()
