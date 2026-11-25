"""Recompute closed-form counterexamples for the research triage.

These are dimensionless synthetic controls except for the illustrative time
unit in the two-reservoir model. They do not fit material or game data.
"""
from __future__ import annotations

import json
from math import exp, isclose, log
from platform import python_version
from statistics import fmean


def dry(film: float, pore: float, a: float, decay: float,
        elapsed: float) -> tuple[float, float]:
    retain = exp(-decay * elapsed)
    now_film = retain * exp(-a * elapsed) * film
    return now_film, retain * (film + pore) - now_film


def history_control() -> dict:
    decay = .15
    a_values = (.1, 1.)
    wipe_at, rewet_at, evaluate_at = 2., 3., 4.
    wipe_efficiency, rewet_mass = .8, .25
    calibration_times = [i / 10 for i in range(41)]
    curves = [
        [sum(dry(1., 0., a, decay, t)) for t in calibration_times]
        for a in a_values
    ]
    passive_difference = max(abs(left-right) for left, right in zip(*curves))
    assert passive_difference <= 2e-15

    histories = []
    for a in a_values:
        film, pore = dry(1., 0., a, decay, wipe_at)
        wiped_film = (1-wipe_efficiency)*film
        immediately_after_wipe = wiped_film+pore
        film_pre, pore_pre = dry(wiped_film,pore,a,decay,rewet_at-wipe_at)
        film_e, pore_e = dry(film_pre+rewet_mass,pore_pre,a,decay,evaluate_at-rewet_at)
        film_from_wipe = (film+pore-immediately_after_wipe)/wipe_efficiency
        inferred = -log(film_from_wipe/exp(-decay*wipe_at))/wipe_at
        assert isclose(inferred,a,rel_tol=0.,abs_tol=3e-14)
        histories.append(dict(a=a, film_at_wipe=film, pore_at_wipe=pore,
                              total_after_wipe=immediately_after_wipe,
                              total_at_evaluation=film_e+pore_e,
                              film_at_evaluation=film_e,pore_at_evaluation=pore_e,
                              inferred_from_known_wipe=inferred))
    gap = abs(histories[0]["total_at_evaluation"]-histories[1]["total_at_evaluation"])
    analytic = wipe_efficiency*exp(-decay*evaluate_at)*abs(
        exp(-a_values[0]*wipe_at)-exp(-a_values[1]*wipe_at))
    assert isclose(gap,analytic,rel_tol=0.,abs_tol=2e-15)
    return dict(decay=decay, initial_film=1., initial_pore=0., wipe_at=wipe_at,
                wipe_efficiency=wipe_efficiency, rewet_at=rewet_at, rewet_mass=rewet_mass,
                evaluate_at=evaluate_at, calibration_times=calibration_times,
                passive_linf_difference=passive_difference, histories=histories,
                heldout_gap=gap, minimax_absolute_error_lower_bound=gap/2)


def coarse_flux_control() -> dict:
    fine_states = ((1., 1.), (0., 2.))
    mean_depth = [fmean(state) for state in fine_states]
    fine_average_flux = [fmean(v**3 for v in state) for state in fine_states]
    assert mean_depth[0] == mean_depth[1] and fine_average_flux == [1.,4.]
    gap = fine_average_flux[1]-fine_average_flux[0]
    minimax = fmean(fine_average_flux)
    return dict(fine_heights=fine_states, coarse_height=mean_depth[0],
                fine_average_flux=fine_average_flux, shared_coarse_cubic_flux=1.,
                minimax_single_prediction=minimax,
                minimax_absolute_error_lower_bound=gap/2)


def main() -> None:
    print(json.dumps(dict(
        scope="synthetic closed-form controls; no capture, visual or engine benchmark",
        python=python_version(), material_history=history_control(),
        coarse_flux=coarse_flux_control()),indent=2,allow_nan=False))


if __name__=="__main__":
    main()
