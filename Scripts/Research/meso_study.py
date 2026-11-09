"""Frozen meso reconstruction/transport sensitivity study; no product algorithm changes."""
import argparse, datetime as dt, hashlib, json, pathlib, shutil, subprocess, sys, time
ROOT=pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'Build/ResearchPython'))
import numpy as np
from PIL import Image
from scipy import sparse
from scipy.sparse.linalg import spsolve

STUDY=ROOT/'Performance-Data/20261027-meso-study'
EXP=STUDY/'PERF-043-meso'
RAW=EXP/'raw'
EXE=ROOT/'Build/Release/MDSS_CouplingStudy.exe'
def stamp():return dt.datetime.now(dt.timezone(dt.timedelta(hours=9))).isoformat()
def write(p,x):p.parent.mkdir(parents=True,exist_ok=True);p.write_text(json.dumps(x,indent=2,ensure_ascii=False,allow_nan=False),encoding='utf-8')
def read(p):return json.loads(p.read_text(encoding='utf-8'))
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def arr(d,n,cols=1):
    x=np.fromfile(d/(n+'.f32'),dtype='<f4').astype(np.float64)
    return x.reshape(-1,cols) if cols>1 else x

def prepare():
    if (EXP/'plan.json').exists():return read(EXP/'plan.json')
    inputs=RAW/'fixtures';inputs.mkdir(parents=True,exist_ok=True)
    (inputs/'quad.obj').write_text('o MesoPlane\nv -0.5 -0.5 0\nv 0.5 -0.5 0\nv 0.5 0.5 0\nv -0.5 0.5 0\nvt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\nvn 0 0 1\nf 1/1/1 2/2/1 3/3/1\nf 3/3/1 4/4/1 1/1/1\n',encoding='ascii')
    x=(np.arange(512)+.5)/512-.5;y=x[:,None];x=x[None,:];w=4*np.pi;a=.015
    hx=-a*w*np.sin(w*x)*np.cos(w*y);hy=-a*w*np.cos(w*x)*np.sin(w*y)
    n=np.stack([-hx,hy,np.ones((512,512))],axis=-1);n/=np.linalg.norm(n,axis=-1,keepdims=True)
    rgba=np.concatenate([np.rint((n+1)*127.5).clip(0,255).astype(np.uint8),np.full((512,512,1),255,dtype=np.uint8)],axis=-1)
    Image.fromarray(rgba).save(inputs/'analytic-normal.png')
    Image.fromarray(np.full((4,4,4),[128,128,255,255],dtype=np.uint8)).save(inputs/'flat-normal.png')
    configs={'analytic':dict(mode='analytic',amplitude=.015),
        'png':dict(mode='normal_map',path=str(inputs/'analytic-normal.png'),flip_y=True),
        'brick':dict(mode='normal_map',path=str(ROOT/'Assets/Meshes/BrickCube/BrickCube_512Normal.png'),flip_y=True),
        'flat':dict(mode='flat'),
        'flat8':dict(mode='normal_map',path=str(inputs/'flat-normal.png'),flip_y=True)}
    conditions=[]
    base=dict(case='meso',variant='D',resolution=128,hz=60,seconds=4,
        mesh=str(inputs/'quad.obj'),profile='Assets/SurfaceProfiles/IceReactionLab.SRProfile',
        scale=[1.65,2.2,.22],rotation_x_degrees=8,ledger=True,full_geometry=True,sparse_solver=False,implementation='U')
    def add(id,stage,mode='brick',**extra):
        c=dict(base,id=id,stage=stage,meso=dict(configs[mode]));c.update(extra);conditions.append(c)
    for res in [64,128,256]:
        for mode in ['analytic','png','brick']:add(f'cpu-{mode}-{res}','cpu',mode,resolution=res,geometry_only=True)
    for strength in [.5,2.0]:
        add(f'cpu-brick-128-x{strength:g}','cpu',geometry_only=True)
        conditions[-1]['meso']['strength']=strength
    add('cpu-flat8-128','cpu','flat8',geometry_only=True)
    for mode in ['flat','analytic','png','brick']:
        add(f'gpu-{mode}-128-D','gpu',mode)
        if mode!='flat':add(f'gpu-{mode}-ls-128-D','gpu',mode,reference=f'cpu-{mode}-128')
    add('gpu-analytic-truth-128-D','gpu','analytic',truth_reference='cpu-analytic-128')
    for strength in [.5,2.0]:
        add(f'gpu-brick-128-x{strength:g}-D','gpu');conditions[-1]['meso']['strength']=strength
    for mode in ['png','brick']:add(f'gpu-{mode}-128-E','gpu',mode,variant='E')
    add('gpu-brick-128-120-D','gpu',hz=120)
    for res in [64,256]:add(f'gpu-brick-{res}-D','gpu',resolution=res)
    add('gpu-brick-ls-256-D','gpu',resolution=256,reference='cpu-brick-256')
    # One controlled cavity setting; original profile values otherwise unchanged, no tuning after results.
    profile=read(ROOT/'Assets/SurfaceProfiles/IceReactionLab.SRProfile')
    cavity=json.loads(json.dumps(profile));cavity['states']['ice']['cavityFillFactor']=1
    write(inputs/'cavity-profile.SRProfile',cavity)
    for variant in ['D','E']:add(f'gpu-brick-cavity-128-{variant}','gpu',variant=variant,profile=str(inputs/'cavity-profile.SRProfile'))
    add('gpu-brick-cavity-ls-128-D','gpu',profile=str(inputs/'cavity-profile.SRProfile'),reference='cpu-brick-128')
    add('legacy-smoke-D','control',case='plate',mesh='Assets/Meshes/MarbleCube/MarbleCube.obj',seconds=1,meso=dict(mode='flat'))
    plan=dict(schema_version=1,created_at=stamp(),source_commit=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
        status='frozen_before_first_study_execution',question='Do normal reconstruction/scale choices materially affect subsequent shared-geometry transport, and does a standard least-squares solve suffice?',
        conditions=conditions,
        reconstruction='same production graph and sampled normals; float64 Laplacian direct solve of endpoint-averaged slope constraints; pin first vertex then component zero mean',
        known_height='h=.015*cos(4*pi*x)*cos(4*pi*y), analytic float normals and 512 RGBA8 normal texture; renderer flip_y=true',
        outputs='height/edge/objective errors, State L1 divided by supplied material, temporal/spatial and frozen-G contrasts, budget/finite checks; GPU times descriptive only',
        gate=dict(nonfinite=0,negative=0,reaction_errors=0,frozen_changes=0,relative_material_budget=1e-5,reference_linear_residual=1e-8,reciprocal_graph_required=True),
        effect_interpretation='Compare reconstruction effect to observed dt/grid sensitivity; no statistical significance or physical ground truth for real asset',
        stop='Stop on execution/numeric-gate failure. No parameter tuning, new reconstruction algorithm, or automatic solver adoption. Standard reference resolving error is an implementation limitation, not novelty.',
        scope='Single +Z square, one real normal asset, no seam/rechart/general mesh claim; 4-second transient; fixed model coefficients and analytic injection; no rendered quality/productivity/performance superiority')
    write(EXP/'plan.json',plan)
    write(STUDY/'study.json',dict(schema_version=1,study_id=STUDY.name,created_at=stamp(),updated_at=stamp(),status='planned',
        question=plan['question'],experiments=[dict(id=EXP.name,status='planned',path=EXP.name+'/')],report='Docs/MDSSP-Agent-Vault/04_Experiments/02_Performance-Measurements.md'))
    return plan

def archive():
    files=[EXE,ROOT/'CMakeLists.txt']
    for base,pattern in [('Scripts/Research','*'),('Source/SurfaceState','*'),('Source/AssetManager','*'),('Source/GPU','*'),('Source/Logger','*'),('Shaders/Simulation','*'),('Build/generated','*'),('Tests/Fixtures','*')]:
        files.extend(p for p in (ROOT/base).rglob(pattern) if p.is_file() and '__pycache__' not in p.parts)
    files.extend((ROOT/'Assets/SurfaceProfiles').glob('*.SRProfile'))
    files.extend((RAW/'fixtures').rglob('*'))
    files+=[ROOT/'Assets/Meshes/BrickCube/BrickCube_512Normal.png',ROOT/'Build/research/MesoGeometryStudy.cpp',ROOT/'Build/CMakeCache.txt',
            ROOT/'Tests/NormalMapTransferTests.cpp',ROOT/'Tests/SharedSurfaceGeometryTests.cpp']
    for base in [ROOT/'Build/bin/Shaders/Release',ROOT/'Build/research/Release']:
        if base.exists():files.extend(base.rglob('*.spv'))
    records={str(p.relative_to(ROOT)):digest(p) for p in set(files) if p.is_file()}
    key=hashlib.sha256(json.dumps(records,sort_keys=True).encode()).hexdigest()[:12];out=RAW/'inputs'/key
    if not (out/'archive.json').exists():
        for name,sha in records.items():
            target=out/name;target.parent.mkdir(parents=True,exist_ok=True)
            shutil.copy2('\\\\?\\'+str((ROOT/name).resolve()),'\\\\?\\'+str(target.resolve()))
            if digest(target)!=sha:raise RuntimeError('Archive copy mismatch')
        write(out/'archive.json',dict(created_at=stamp(),files=records))
        (out/'working-tree.patch').write_bytes(subprocess.check_output(['git','diff'],cwd=ROOT))
    return str(out.relative_to(ROOT))

def run(stage,ids):
    plan=prepare();mp=EXP/'manifest.json';manifest=read(mp) if mp.exists() else dict(runs=[])
    supplemental=EXP/'postprocess-control-plan.json'
    if supplemental.exists():plan['conditions']+=read(supplemental)['conditions']
    replay=EXP/'shader-capture-replay-plan.json'
    if replay.exists():plan['conditions']+=read(replay)['conditions']
    inputs=archive()
    for c in plan['conditions']:
        if (c['id'] not in ids) if ids else (c['stage']!=stage):continue
        previous=[r for r in manifest['runs'] if r['id']==c['id']]
        if previous and previous[-1]['status']=='completed':continue
        out=RAW/(c['id']+f'-attempt{len(previous)+1}');out.mkdir(parents=True,exist_ok=False)
        request=json.loads(json.dumps(c));request['output']=str(out)
        reference=c.get('reference') or c.get('truth_reference')
        if reference:
            r=next((r for r in reversed(manifest['runs']) if r['id']==reference and r['status']=='completed'),None)
            if not r:raise RuntimeError('Missing CPU reference '+reference)
            folder=ROOT/r['output'];name='reference-truth.f32' if 'truth_reference' in c else 'reference-ls.f32'
            request['meso']['height_file']=str(folder/name)
        write(out/'request.json',request)
        r=dict(id=c['id'],attempt=len(previous)+1,status='running',started_at=stamp(),output=str(out.relative_to(ROOT)),
            request_sha256=digest(out/'request.json'),executable_sha256=digest(EXE),plan_sha256=digest(EXP/'plan.json'),archive=inputs)
        if supplemental.exists():r['postprocess_control_plan_sha256']=digest(supplemental)
        if replay.exists():r['shader_capture_replay_plan_sha256']=digest(replay)
        manifest['runs'].append(r);write(mp,manifest);print('RUN',c['id'],flush=True);start=time.monotonic()
        try:
            with (out/'run.log').open('w',encoding='utf-8') as log:p=subprocess.run([str(EXE),str(out/'request.json')],cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,timeout=1200)
            r.update(exit_code=p.returncode,status='completed' if p.returncode==0 else 'failed')
        except (OSError,subprocess.TimeoutExpired) as e:r.update(status='execution_failed',error=str(e))
        if (out/'summary.json').exists():r['summary']=read(out/'summary.json')
        r.update(finished_at=stamp(),elapsed_seconds=time.monotonic()-start);write(mp,manifest)
        print(r['status'],c['id'],round(r['elapsed_seconds'],2),flush=True)
        if r['status']!='completed':print((out/'run.log').read_text(encoding='utf-8',errors='replace')[-2500:]);return 1
        if stage=='gpu' or c['stage']=='gpu':
            s=r['summary'];bad=any(s[k] for k in ['nonfinite','negative','reaction_budget','reaction_arithmetic','frozen_failures'])
            z=s['initial_ice_water']+sum(s['cumulative_input'][s['channels'].index(k)] for k in ['ice','waterfilm'])
            if bad or s['max_mass_residual']/max(z,1e-30)>1e-5:raise RuntimeError('Numerical gate failed '+c['id'])
    return 0

def reconstruct():
    results=[]
    for r in read(EXP/'manifest.json')['runs']:
        if r['status']!='completed' or not r['id'].startswith('cpu-'):continue
        folder=ROOT/r['output'];h=arr(folder,'meso-height');pos=arr(folder,'meso-positions',3);normal=arr(folder,'meso-transfer-normals',3)
        nb=arr(folder,'meso-neighbors',8).astype(int);valid=arr(folder,'meso-valid')>0
        if not valid.all() or np.max(np.abs(pos[:,2]))>1e-7 or r['summary']['meso']['production_active']!=len(h) or r['summary']['meso']['production_components']!=1:
            raise RuntimeError('Reference supports only complete, active, single-component flat quad')
        ii,jj=np.where(nb>=0);targets=nb[ii,jj]
        directed={(int(i),int(j)) for i,j in zip(ii,targets)}
        missing=sum((j,i) not in directed for i,j in directed)
        if missing:raise RuntimeError('Nonreciprocal reference graph')
        keep=ii<targets;i=ii[keep];j=targets[keep]
        slope=-normal[:,:2]/normal[:,2:3]
        b=np.sum(.5*(slope[i]+slope[j])*(pos[j,:2]-pos[i,:2]),axis=1)
        m=len(i);B=sparse.coo_matrix((np.r_[-np.ones(m),np.ones(m)],(np.r_[np.arange(m),np.arange(m)],np.r_[i,j])),shape=(m,len(h))).tocsr()
        L=(B.T@B).tocsc();rhs=B.T@b;ref=np.zeros(len(h));ref[1:]=spsolve(L[1:,1:],rhs[1:]);ref-=ref.mean()
        residual=float(np.linalg.norm((L@ref-rhs)[1:])/max(np.linalg.norm(rhs[1:]),1e-30))
        if residual>1e-8 or not np.isfinite(ref).all():raise RuntimeError('Reference solve gate failed')
        ref.astype('<f4').tofile(folder/'reference-ls.f32')
        truth=arr(folder,'meso-truth');truth-=truth.mean();truth.astype('<f4').tofile(folder/'reference-truth.f32')
        production_error=B@h-b;ref_error=B@ref-b
        record=dict(id=r['id'],vertices=len(h),edges=m,components=r['summary']['meso']['production_components'],
            production_iterations=r['summary']['meso']['production_iterations'],
            production_edge_relative=float(np.linalg.norm(production_error)/max(np.linalg.norm(b),1e-30)),
            reference_edge_relative=float(np.linalg.norm(ref_error)/max(np.linalg.norm(b),1e-30)),reference_linear_relative=residual,
            height_rmse_vs_ls=float(np.sqrt(np.mean((h-ref)**2))),reference_height_rms=float(np.sqrt(np.mean(ref**2))),
            height_relative_rmse_vs_ls=float(np.linalg.norm(h-ref)/max(np.linalg.norm(ref),1e-30)),
            production_range=[float(h.min()),float(h.max())],reference_range=[float(ref.min()),float(ref.max())],
            reference_file_sha256=digest(folder/'reference-ls.f32'),reciprocal_missing=missing)
        if 'analytic' in r['id'] or r['id'].startswith('cpu-png-'):
            record.update(production_relative_rmse_vs_truth=float(np.linalg.norm(h-truth)/np.linalg.norm(truth)),reference_relative_rmse_vs_truth=float(np.linalg.norm(ref-truth)/np.linalg.norm(truth)))
        results.append(record)
    write(EXP/'reconstruction.json',dict(created_at=stamp(),method='float64 direct solve of same unweighted graph slope least squares; first node pinned, then zero mean',results=results))
    for r in results:print(r['id'],'height error vs LS',format(r['height_relative_rmse_vs_ls'],'.5g'),'edge',format(r['production_edge_relative'],'.5g'),'LS edge',format(r['reference_edge_relative'],'.5g'))

if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('stage',choices=['prepare','cpu','reconstruct','gpu','control']);ap.add_argument('--ids',nargs='*',default=[]);a=ap.parse_args()
    if a.stage=='prepare':p=prepare();print(EXP/'plan.json',len(p['conditions']))
    elif a.stage=='reconstruct':reconstruct()
    else:raise SystemExit(run(a.stage,a.ids))
