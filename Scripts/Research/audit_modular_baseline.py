"""Audit provenance, steps, state validity and explicit skipped conditions without inventing measurements."""
import collections
import json
import pathlib
import sys
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[2]/"Build/ResearchPython"))
import numpy as np
from run_modular_baseline import EXP,ROOT,SPEC_PATH,digest,write,stamp

manifest=json.loads((EXP/"manifest.json").read_text(encoding="utf-8"));spec=json.loads(SPEC_PATH.read_text(encoding="utf-8"))
comparisons=json.loads((EXP/"comparisons.json").read_text(encoding="utf-8"))
records=[];unsupported=[]
for r in manifest["runs"]:
    if r["status"]!="completed":continue
    base=ROOT/r["output"];c=json.loads((base/"request.json").read_text(encoding="utf-8"));s=r["summary"];fail=[]
    if digest(base/"request.json")!=r["request_sha256"]:fail.append("request digest")
    archive=json.loads((ROOT/r["archive"]/"archive.json").read_text(encoding="utf-8"))
    exe=next(v for k,v in archive["files"].items() if k.replace('\\','/')=="Build/Release/MDSS_CouplingStudy.exe")
    if exe!=r["executable_sha256"]:fail.append("archived executable digest")
    final=np.fromfile(base/"final.f32",dtype="<f4")
    if final.size!=s["texels"]*len(s["channels"]):fail.append("final array size")
    if not np.isfinite(final).all() or np.any(final<0):fail.append("final array validity")
    series=[json.loads(x) for x in (base/"series.jsonl").read_text(encoding="utf-8").splitlines()]
    timing=s.get("timing_only",False)
    if not timing:
        if len(series)!=s["steps"]:fail.append("diagnostic step count")
        if [x["step"] for x in series]!=list(range(1,s["steps"]+1)):fail.append("diagnostic step sequence")
        if abs(series[-1]["time"]-c["seconds"])>1e-9:fail.append("diagnostic duration")
    if (base/"timings.jsonl").exists():
        times=[json.loads(x) for x in (base/"timings.jsonl").read_text(encoding="utf-8").splitlines()]
        if len(times)!=s["steps"]:fail.append("timestamp row count")
    else:unsupported.append(dict(id=r["id"],item="per-step timing export absent in first smoke binary"))
    if c["variant"]=="E":
        g0=(base/"geometry-0.f32").read_bytes()
        if any(f.read_bytes()!=g0 for f in base.glob("geometry-*.f32")):fail.append("frozen geometry")
    for field in ["nonfinite","negative","reaction_budget","reaction_arithmetic","frozen_failures"]:
        if s[field]:fail.append(field)
    records.append(dict(id=r["id"],attempt=r["attempt"],steps=s["steps"],diagnostic_steps=0 if timing else s["steps"],
        final_sha256=digest(base/"final.f32"),failures=fail,archive=r["archive"]))

intentional_changes=["Scripts/Research/CouplingStudy.cpp"]
snapshot_changes=[]
for rel,sha in spec["source_snapshot"].items():
    if digest(ROOT/rel)!=sha:snapshot_changes.append(dict(path=rel,intended_research_adapter_change=rel in intentional_changes))
write(EXP/"study-audit.json",dict(audited_at=stamp(),attempt_status_counts=dict(collections.Counter(r["status"] for r in manifest["runs"])),
    completed_attempts=len(records),main_steps=sum(r["steps"] for r in records),diagnostic_steps=sum(r["diagnostic_steps"] for r in records),
    audit_failures=sum(len(r["failures"]) for r in records),comparison_gate_failures=comparisons["failed_comparisons"],
    deferred_run_ids=comparisons["deferred_run_ids"],records=records,unsupported=unsupported,reference_snapshot_changes=snapshot_changes,
    limitations=["No independent numerical implementation: common numerical providers and services are shared",
                 "No human productivity/appearance or prior-framework novelty result",
                 "Cost stages may be explicitly unexecuted under the structural stop; support controls are not repeated performance evidence",
                 "Driver pipeline/program memory and setup/compile CPU time are not included in tracked buffer-byte/record-time fields"] ))
print(json.dumps(dict(completed=len(records),steps=sum(r["steps"] for r in records),audit_failures=sum(len(r["failures"]) for r in records),
                      comparison_failures=comparisons["failed_comparisons"],snapshot_changes=snapshot_changes),indent=2))
