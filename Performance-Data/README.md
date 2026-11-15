# Performance Data Index

This directory stores generated performance experiment data. Read the linked experiment summary in the agent vault for conclusions; open manifests and raw captures only when reproducing or auditing that experiment.

## Study index

| Study or run group | Contents and status | Entry points |
|---|---|---|
| `20261004` solver resolution | Four runs. The 23:11 and 23:17 attempts failed; 23:20 and 23:54 completed. The latter two have summaries and 36 tracked JSONL captures each. | [23:20 summary](20261004-232011-solver-resolution/summary.md), [23:54 summary](20261004-235404-solver-resolution/summary.md) |
| `20261005` solver resolution | Two completed runs with summaries. Their local frame captures are not present in this archive. | [03:25 summary](20261005-032552-solver-resolution/summary.md), [03:48 summary](20261005-034834-solver-resolution/summary.md) |
| `20261022` solver resolution | A startup failure followed by a completed 12-run resolution measurement. | [completed summary](20261022-220109-solver-resolution/summary.md), [manifest](20261022-220109-solver-resolution/manifest.json) |
| `20261022` development history | 48 completed snapshot runs used to compare historical development states. | [history summary](20261022-development-history/history-summary.md), [manifest](20261022-development-history/manifest.json) |
| `20261022` optimization suite pilot | Stopped pilot with status `pilot_stopped`; retained as historical evidence and excluded from the final comparison. | [manifest](20261022-optimization-suite/manifest.json), [plan](20261022-optimization-suite/plan.json) |
| `20261022` optimization suite main | 432 completed runs with comparisons and audit records. | [comparison summary](20261022-optimization-suite-main/comparisons.md), [audit](20261022-optimization-suite-main/study-audit.json) |
| `20261022` single transport validation | 152 completed runs focused on one transport step. | [validation summary](20261022-single-transport-validation/validation-summary.json), [manifest](20261022-single-transport-validation/manifest.json) |
| `20261022` workload supplement | 357 completed runs adding workloads to the optimization study. | [comparison summary](20261022-workload-supplement/comparisons.md), [manifest](20261022-workload-supplement/manifest.json) |
| `20261025` optimization round | Baseline and seven completed experiment folders: PERF007, PERF028, PERF036, PERF036-formats, PERF037, PERF037-lowmesh, and PERF038. | [baseline](20261025-optimization-round/baseline/comparisons.md), [PERF007](20261025-optimization-round/PERF007/comparisons.md), [PERF028](20261025-optimization-round/PERF028/comparisons.md), [PERF036](20261025-optimization-round/PERF036/comparisons.md), [PERF036-formats](20261025-optimization-round/PERF036-formats/comparisons.md), [PERF037](20261025-optimization-round/PERF037/comparisons.md), [PERF037-lowmesh](20261025-optimization-round/PERF037-lowmesh/comparisons.md), [PERF038](20261025-optimization-round/PERF038/comparisons.md), [study audit](20261025-optimization-round/study-audit.json) |

The interpretation and adoption decision for performance work is recorded in [Performance Measurements](../Docs/Agent-Vault/05_Experiments/02_Performance-Measurements.md). The storage and ignore rules are described in [Raw Archive](00_Raw-Archive.md).

## Restored historical studies

The following tracked records were restored from `af8085be762bdf3a17d487ee6fe4a1d563f219a0` on 2026-11-10. This restores archived evidence, not a new measurement; locally ignored raw captures are not included in that commit.

| Study | Entry point |
|---|---|
| State and geometry coupling | [20261026 study](20261026-coupling-study/study.json) |
| Authoring readiness | [20261027 study](20261027-authoring-readiness/study.json) |
| Meso reconstruction | [20261027 study](20261027-meso-study/study.json) |
| Reaction comparison and Sparse Solver defaults | [20261108 study](20261108-reaction-transition-fps/study.json) |

## Layout for new studies

Keep the existing historical folders intact. For new work, group related questions in one dated study folder and give each question its own experiment folder:

```text
YYYYMMDD-study-name/
├── study.json
├── baseline/
│   ├── plan.json
│   ├── manifest.json
│   ├── comparisons.json
│   └── raw/
└── PERF-NNN-short-name/
    ├── plan.json
    ├── manifest.json
    ├── comparisons.json
    └── raw/
```

- `study.json` identifies the study question, date, status, included experiments, and report link. Start from `Study-Record-Template.json`.
- `plan.json` records the conditions prepared before execution.
- `manifest.json` records what actually ran, its status, environment, and output files.
- `comparisons.json` contains calculated results and correctness checks.
- `raw/` contains original frame data, logs, captures, and other reproducibility inputs.
- Audits or specialized validation output may be added when the experiment needs them. Do not create a separate prose report per execution; put interpretation in the linked experiment document.

Runner-specific fields in existing `plan.json` and `manifest.json` files differ. Preserve each historical schema. Apply this layout to new studies and update this index when a study is added.

## Storage notes

Most `raw/` directories are ignored by Git. The 36 JSONL captures from the two completed 2026-10-04 runs remain eligible for Git tracking. A repository clone therefore does not contain all local raw data; preserve a separate backup when those captures are needed for long-term reproduction.

