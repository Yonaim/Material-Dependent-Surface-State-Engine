"""Record the staged execution subset before inspecting timestep/resolution/held-out results."""

from run_coupling_study import EXPERIMENT, write, stamp

selected = [f"plate-128-60-{v}" for v in "ABCDEF"]
selected += [f"plate-128-{hz}-{v}" for hz in [120, 240] for v in "DE"]
selected += [f"plate-256-60-{v}" for v in "DE"]
selected += ["plate-128-60-D-repeat", "plate-128-60-D-dirty"]
selected += [f"{case}-128-60-{v}" for case in ["bunny", "mud"] for v in "BDE"]
write(
    EXPERIMENT / "execution-amendment.json",
    dict(
        created_at=stamp(),
        selected=selected,
        scope="Primary A-F; D-E numerical sensitivity; B-D-E exploratory transfer",
        reason="Dense diagnostic trajectories take minutes per condition. Focus higher-resolution/time-refinement work on the predeclared primary D-E contrast, not every secondary ablation.",
        decision_timing="After primary 60 Hz D/E, before any refined-timestep, higher-resolution or held-out outcome",
        unchanged="Profile parameters, field design, input times, duration, numerical budget gate and primary contrast unchanged",
        execution="Use existing sparse workgroup scheduling only after matching dense D/E controls; retain full every-step geometry and FP32",
        deferred="A/B/C/F temporal and spatial refinements; A/C/F held-out ablations; modular-baseline implementation and independent application-performance study",
        reporting="Report all earlier attempts including interruption; this subset does not establish general convergence or H3/H4",
    ),
)
print(" ".join(selected))
