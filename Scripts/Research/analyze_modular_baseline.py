"""Apply frozen v1 equality gates. No novelty/productivity claim is inferred from implementation counts."""
# ============================================================
# 00. Imports and Module Configuration
# ============================================================
#region 00_Imports_and_Module_Configuration

import json
import pathlib
import sys
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[2]/"Build/ResearchPython"))
import numpy as np
from run_modular_baseline import EXP, ROOT, SPEC_PATH, write, stamp

spec=json.loads(SPEC_PATH.read_text(encoding="utf-8"))
plan=json.loads((EXP/"plan.json").read_text(encoding="utf-8"))
manifest=json.loads((EXP/"manifest.json").read_text(encoding="utf-8"))
runs={r["id"]:r for r in manifest["runs"] if r["status"]=="completed"}
requests={c["id"]:c for c in plan["conditions"]}
for name in ["cost-reference-plan.json","order-control-plan.json","timing-control-plan.json"]:
    if (EXP/name).exists():requests.update({c["id"]:c for c in json.loads((EXP/name).read_text(encoding="utf-8"))["conditions"]})

#endregion

# ============================================================
# 01. Processing Helpers
# ============================================================
#region 01_Processing_Helpers

def base(key):return ROOT/runs[key]["output"]
def array(key,file):return np.fromfile(base(key)/file,dtype="<f4").astype(np.float64)
def rows(key):return [json.loads(s) for s in (base(key)/"series.jsonl").read_text(encoding="utf-8").splitlines()]
def state(key,file):
    s=runs[key]["summary"];return array(key,file).reshape(s["texels"],len(s["channels"]))

def pair(a,b,seconds=None):
    sa,sb=runs[a]["summary"],runs[b]["summary"];names=sa["channels"];k=len(names)
    failures=[];checks=[]
    if names!=sb["channels"]:return dict(a=a,b=b,passed=False,failures=["channel layout"])
    ra,rb=rows(a),rows(b)
    if seconds is not None:
        ra=[r for r in ra if r["time"]<=seconds];rb=[r for r in rb if r["time"]<=seconds]
    init=state(a,"bootstrap.f32");inp=np.array(ra[-1]["cumulative_input"])
    material=sum(init[:,i].sum()+inp[i] for i,n in enumerate(names) if n!="heat")
    denom=np.array([init[:,i].sum()+inp[i] if n=="heat" else material for i,n in enumerate(names)])
    denom=np.maximum(denom,1e-20)
    areas=array(a,"areas.f32")/(1/256**2);valid=areas>0
    cfg=requests[a];pos=array(a,"positions.f32").reshape(-1,3);L=max(float(np.linalg.norm(np.ptp(pos[valid],axis=0))),1.)
    rad=np.deg2rad(cfg["rotation_x_degrees"]);co,si=np.cos(rad),np.sin(rad)
    model=np.array([[1,0,0],[0,co,-si],[0,si,co]])@np.diag(cfg["scale"])
    available=[t for t in [0,1,2,4,8,14] if (base(a)/f"state-{t}.f32").exists() and (base(b)/f"state-{t}.f32").exists()]
    if seconds is not None:available=[t for t in available if t<=seconds]
    expected=[t for t in [0,1,2,4,8,14] if t<=min(ra[-1]["time"],rb[-1]["time"])]
    if available!=expected:failures.append("missing required snapshots")
    for file in ["areas.f32","positions.f32","neighbors.f32","u.f32","v.f32","patch.f32"]:
        if (base(a)/file).read_bytes()!=(base(b)/file).read_bytes():failures.append("graph/input field differs: "+file)
    all_identical=True
    for t in available:
        x,y=state(a,f"state-{t}.f32"),state(b,f"state-{t}.f32")
        delta=np.abs(x-y);l1=delta.sum(axis=0)/denom
        dx,dy=x[valid]/areas[valid,None],y[valid]/areas[valid,None]
        bound=1e-4+1e-5*np.maximum(np.abs(dx),np.abs(dy))
        density_ratio=float(np.max(np.abs(dx-dy)/bound))
        gx,gy=array(a,f"geometry-{t}.f32").reshape(-1,8),array(b,f"geometry-{t}.f32").reshape(-1,8)
        world=float(np.linalg.norm((gx[:,:3]-gy[:,:3])@model.T,axis=1).max())
        normal=float(np.linalg.norm(gx[:,4:7]-gy[:,4:7],axis=1).max())
        wx,wy=array(a,f"weights-{t}.f32"),array(b,f"weights-{t}.f32")
        weight=float(np.abs(wx-wy).max())
        same=bool(np.array_equal(x,y) and np.array_equal(gx,gy) and np.array_equal(wx,wy));all_identical&=same
        auxiliary={}
        for field in ["flux","alpha","concavity"]:
            name=f"{field}-{t}.f32"
            if (base(a)/name).exists() and (base(b)/name).exists():
                auxiliary[field]=bool(np.array_equal(array(a,name),array(b,name)))
        all_identical=all_identical and all(auxiliary.values())
        ok=bool(np.all(l1<=1e-5) and density_ratio<=1 and world<=1e-5*L and normal<=1e-4 and weight<=2e-5)
        if not ok:failures.append(f"snapshot {t}")
        checks.append(dict(seconds=t,bit_identical=same,state_l1_relative=l1.tolist(),max_density_gate_ratio=density_ratio,
                           position_world_max=world,normal_max=normal,weights_max=weight,auxiliary_arrays_equal=auxiliary,passed=ok))
    for key in [a,b]:
        s=runs[key]["summary"]
        for label in ["nonfinite","negative","reaction_budget","reaction_arithmetic","frozen_failures"]:
            if s[label]:failures.append(key+":"+label)
        z=state(key,"bootstrap.f32")[:,[i for i,n in enumerate(names) if n in ["ice","waterfilm","wetness"]]].sum()
        z+=sum(s["cumulative_input"][i] for i,n in enumerate(names) if n in ["ice","waterfilm","wetness"])
        if s["max_mass_residual"]/max(z,1e-20)>1e-5:failures.append(key+":mass budget")
    if not np.array_equal(state(a,"bootstrap.f32"),state(b,"bootstrap.f32")):failures.append("initial State differs")
    if not np.array_equal(array(a,"geometry-0.f32"),array(b,"geometry-0.f32")):failures.append("initial geometry differs")
    series_equal=ra[-1]["time"]==rb[-1]["time"] and len(ra)==len(rb)
    if not series_equal:failures.append("step/time alignment")
    diagnostic_fields=["state_sums","downstream_sums","cumulative_input","cumulative_decay","cumulative_crossing"]
    terminal={}
    for field in diagnostic_fields:
        diff=np.abs(np.array(ra[-1][field])-np.array(rb[-1][field]));terminal[field]=(diff/denom).tolist()
        if np.any(diff/denom>1e-5):failures.append(field)
    ea,eb=np.array(ra[-1]["cumulative_extents"]),np.array(rb[-1]["cumulative_extents"])
    ledger=sa["reaction_ledger_enabled"] and sb["reaction_ledger_enabled"]
    profile_path=base(a)/"effective-profile.SRProfile"
    profile=json.loads(profile_path.read_text(encoding="utf-8")) if profile_path.exists() else json.loads((ROOT/cfg["profile"]).read_text(encoding="utf-8"))
    rules=[r for r in profile["reactions"] if r.get("enabled",True)]
    if cfg["variant"] in ["A","C"]:rules=[]
    rule_z=np.array([material if any(n!="heat" for n in r["consumes"]) else denom[names.index("heat")] for r in rules])
    extents=np.abs(ea-eb)/np.maximum(rule_z,1e-20) if ledger else np.array([])
    if np.any(extents>1e-5):failures.append("rule extents")
    cumulative_budget=[]
    stoich=np.zeros((len(rules),k))
    for j,r in enumerate(rules):
        for n,v in r["consumes"].items():stoich[j,names.index(n)]-=v
        for n,v in r["produces"].items():stoich[j,names.index(n)]+=v
    for key,data in [(a,ra),(b,rb)]:
        if not runs[key]["summary"]["reaction_ledger_enabled"]:continue
        initial_sum=state(key,"bootstrap.f32").sum(axis=0);maximum=np.zeros(k)
        for row in data:
            expected=initial_sum+np.array(row["cumulative_input"])-np.array(row["cumulative_decay"])
            expected+=np.array(row["cumulative_extents"])@stoich
            maximum=np.maximum(maximum,np.abs(np.array(row["state_sums"])-expected)/denom)
        if np.any(maximum>1e-5):failures.append(key+":cumulative channel budget")
        cumulative_budget.append(dict(id=key,max_relative_per_channel=maximum.tolist()))
    for x,y in zip(ra,rb):
        if x["step"]!=y["step"] or x["time"]!=y["time"]:series_equal=False
        for field in diagnostic_fields:
            series_equal=series_equal and x[field]==y[field]
        if ledger:series_equal=series_equal and x["cumulative_extents"]==y["cumulative_extents"]
    return dict(a=a,b=b,channels=names,passed=not failures,failures=failures,
                state_geometry_weights_bit_identical=all_identical,entire_numeric_series_equal=bool(series_equal),
                snapshots=checks,terminal_relative=terminal,extents_relative=extents.tolist(),cumulative_channel_budgets=cumulative_budget)

#endregion

# ============================================================
# 02. Script Execution
# ============================================================
#region 02_Script_Execution

comparisons=[]
for c in requests.values():
    if c["implementation"]!="U" or c["stage"] not in ["smoke","diagnostic","cost-reference"]:continue
    a=c["id"];b=a.replace("-U-","-M-")
    if a in runs and b in runs:comparisons.append(pair(a,b))
for a,b,seconds in [
    ("smoke-T0-D-U-128","smoke-T0-D-M-separate-128",None),
    ("diagnostic-T0-D-M-128","separate-T0-D-M-separate-128",None),
    ("smoke-T0-D-U-128","ledger-off-T0-D-U-128",1),
    ("smoke-T0-D-M-128","ledger-off-T0-D-M-128",1),
    ("diagnostic-T3-D-U-128","reverse-T3-D-U-128",1),
    ("diagnostic-T3-D-M-128","reverse-T3-D-M-128",1),
    ("diagnostic-T3-D-M-128","order-control-T3-D-M-separate-normal-128",1),
    ("order-control-T3-D-M-separate-normal-128","order-control-T3-D-M-separate-reverse-128",1)]:
    if a in runs and b in runs:comparisons.append(pair(a,b,seconds))

cost=[]
timing_controls=[]
for imp in ["U","M"]:
    control=f"timing-smoke-T0-D-{imp}-128";reference=f"smoke-T0-D-{imp}-128"
    if control in runs and reference in runs:
        ok=(base(control)/"final.f32").read_bytes()==(base(reference)/"final.f32").read_bytes() and runs[control]["summary"].get("warmup_reset_verified",False)
        timing_controls.append(dict(id=control,reference=reference,passed=ok,final_bit_equal_and_reset_verified=ok))
for task in ["T0","T3"]:
    for resolution in [128,256]:
        groups={imp:[r for key,r in runs.items() if requests[key]["stage"]=="cost" and requests[key]["task"]==task
                     and requests[key]["resolution"]==resolution and requests[key]["implementation"]==imp] for imp in ["U","M"]}
        if not all(groups.values()):continue
        row=dict(task=task,resolution=resolution,implementations={})
        for imp,rs in groups.items():
            diag=f"diagnostic-{task}-D-{imp}-128"
            ref=base(diag)/"final.f32" if resolution==128 and diag in runs else None
            xs=[]
            for r in rs:
                file=ROOT/r["output"]/"final.f32";s=r["summary"]
                # A 256 diagnostic reference is captured before cost collection.
                if resolution==256:
                    refkey=f"cost-reference-{task}-D-{imp}-256"
                    if refkey in runs:ref=base(refkey)/"final.f32"
                xs.append(dict(id=r["id"],gpu_median_ms=s["gpu_median_ms"],gpu_p95_ms=s["gpu_p95_ms"],
                               cpu_record_ms=s["record_cpu_median_ms"],allocated_buffer_bytes=s["allocated_buffer_bytes"],
                               final_bit_equal_diagnostic=file.read_bytes()==ref.read_bytes() if ref else None,
                               warmup_reset=s.get("warmup_reset_verified",False)))
            row["implementations"][imp]=xs
        u=np.median([x["gpu_median_ms"] for x in row["implementations"]["U"]]);m=np.median([x["gpu_median_ms"] for x in row["implementations"]["M"]])
        row["M_over_U_gpu_median_ratio"]=float(m/u);cost.append(row)

remaining=[c["id"] for c in requests.values() if c["stage"] in ["diagnostic","separate","ledger-off","reverse","order-control","timing-smoke","cost-reference"] and c["id"] not in runs]
decision=json.loads((EXP/"decision.json").read_text(encoding="utf-8")) if (EXP/"decision.json").exists() else None
deferred=decision["deferred_run_ids"] if decision else []
remaining=[key for key in remaining if key not in deferred]
result=dict(analyzed_at=stamp(),status="diagnostics_complete" if not remaining else "partial",completed_attempts=sum(r["status"]=="completed" for r in manifest["runs"]),
            remaining_diagnostics=remaining,comparisons=comparisons,failed_comparisons=sum(not c["passed"] for c in comparisons)+sum(not c["passed"] for c in timing_controls),cost=cost,
            timing_controls=timing_controls,
            decision=decision,deferred_run_ids=deferred,
            limitation="Same numerical services/provider code, independent command coordinator; not independent physics validation or a user productivity study")
write(EXP/"comparisons.json",result)
print(json.dumps(dict(completed=result["completed_attempts"],pairs=len(comparisons),failed=result["failed_comparisons"],remaining=remaining,cost_conditions=len(cost)),indent=2))

#endregion
