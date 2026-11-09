"""Standalone scientific figures from completed paired trajectories."""
import json
import pathlib
import sys
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[2]/"Build/ResearchPython"))
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from run_modular_baseline import EXP,ROOT

manifest=json.loads((EXP/"manifest.json").read_text(encoding="utf-8"))
runs={r["id"]:r for r in manifest["runs"] if r["status"]=="completed"}
output=EXP/"figures";output.mkdir(exist_ok=True)
plt.rcParams.update({"font.size":10,"axes.spines.top":False,"axes.spines.right":False})

for task in ["T2","T3"]:
    keys=[f"diagnostic-{task}-D-{imp}-128" for imp in ["U","M"]]
    if not all(k in runs for k in keys):continue
    fig,axes=plt.subplots(1,2,figsize=(11,4))
    for key,style,label in zip(keys,["-","--"],["U: MDSSP","M: modular"]):
        r=runs[key];base=ROOT/r["output"];names=r["summary"]["channels"]
        rows=[json.loads(line) for line in (base/"series.jsonl").read_text(encoding="utf-8").splitlines()]
        for ax,name in zip(axes,["waterfilm","wetness"]):
            ax.plot([x["time"] for x in rows],[x["state_sums"][names.index(name)] for x in rows],style,label=label,lw=2)
            ax.set(title=name,xlabel="Simulation time (s)",ylabel="Total State amount")
    axes[0].legend();fig.suptitle(f"{task}: same-model U/M trajectories (overlap is expected if equal)")
    fig.tight_layout();fig.savefig(output/f"{task}-trajectories.png",dpi=160);plt.close(fig)

keys=[f"diagnostic-T3-D-{imp}-128" for imp in ["U","M"]]
if all(k in runs for k in keys):
    a=ROOT/runs[keys[0]]["output"];u=np.fromfile(a/"u.f32",dtype="<f4");v=np.fromfile(a/"v.f32",dtype="<f4")
    patch=np.fromfile(a/"patch.f32",dtype="<f4")>0;area=np.fromfile(a/"areas.f32",dtype="<f4")/(1/256**2)
    names=runs[keys[0]]["summary"]["channels"]
    fields=[np.fromfile(ROOT/runs[key]["output"]/"final.f32",dtype="<f4").reshape(-1,len(names)) for key in keys]
    fig,axes=plt.subplots(2,2,figsize=(10,8),layout="constrained")
    for col,name in enumerate(["waterfilm","wetness"]):
        ix=names.index(name);density=[f[:,ix]/np.maximum(area,1e-20) for f in fields]
        vmax=max(float(x[patch].max()) for x in density)
        for row,label in enumerate(["U: MDSSP","M: modular"]):
            ax=axes[row,col];im=ax.scatter(u[patch],v[patch],c=density[row][patch],s=2,cmap="viridis",vmin=0,vmax=vmax,rasterized=True)
            ax.axhline(-.1,c="white",ls="--",lw=.7);ax.set(title=f"{label} / {name}",xlabel="u",ylabel="v")
        fig.colorbar(im,ax=axes[:,col],label="State per reference area",shrink=.75)
    fig.suptitle("T3 at 14 s: prescribed 5-state composition, X rotation 20 degrees")
    fig.savefig(output/"T3-distributions.png",dpi=160);plt.close(fig)
print(output)
