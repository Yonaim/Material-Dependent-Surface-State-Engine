"""Describe historical snapshots; do not attribute their differences to isolated optimizations."""
from pathlib import Path
import json,argparse,statistics as st
from analyze_suite import load,collect,quality
from summarize import percentile

def main():
 ap=argparse.ArgumentParser();ap.add_argument('--result',type=Path,required=True);args=ap.parse_args()
 base=args.result.resolve();manifest=load(base/'manifest.json');groups={};bad=[]
 for run in manifest['runs']:
  if run['status']!='complete':bad.append(run);continue
  try:groups.setdefault((run['scene_name'],run['resolution'],run['variant']),[]).append(collect(base,run))
  except Exception as exc:bad.append({'id':run['id'],'error':repr(exc)})
 rows=[]
 for (scene,res,version),items in sorted(groups.items()):
  if len(items)!=3:continue
  reference=groups.get((scene,res,'V4'))
  row={'scene':scene,'resolution':res,'version':version,'runs':3,'metrics':{},
       'state_repeat_reproducible':all(x['validation']['instances']==items[0]['validation']['instances'] for x in items),
       'settings':items[0]['settings']}
  for name in ['frame_gpu_elapsed_ms','simulation_gpu_elapsed_ms','render_gpu_ms']:
   med=[x['metrics'][name]['median'] for x in items];p95=[x['metrics'][name]['p95'] for x in items]
   row['metrics'][name]={'median':st.median(med),'p95':st.median(p95),'repeat_range':[min(med),max(med)]}
  if reference:row['difference_from_V4']=quality(base,reference[0],items[0])
  rows.append(row)
 output={'status':manifest['status'],'comparability':'Common assets, camera, nominal resolution, fixed timestep, feedback ON and height smoothing ON. Solver/preprocessing/rendering semantics differ; no isolated optimization percentage is reported.',
         'excluded_versions':manifest['plan'].get('excluded_versions',{}),'failed_runs':bad,'rows':rows}
 (base/'history-summary.json').write_text(json.dumps(output,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
 lines=['# Development snapshot measurements','','Whole GPU command-buffer elapsed time, ms. Values are median of three run medians. No isolated improvement percentages.','',
        '| Snapshot | Scene | Resolution | Whole GPU median / p95 | Simulation median | Render median | State relative L1 difference vs V4 |',
        '|---|---|---:|---:|---:|---:|---:|']
 for row in rows:
  t=row['metrics'];q=row.get('difference_from_V4',{});diff=q.get('state_l1_relative')
  lines.append(f"| {row['version']} | {row['scene']} | {row['resolution']} | {t['frame_gpu_elapsed_ms']['median']:.3f} / {t['frame_gpu_elapsed_ms']['p95']:.3f} | {t['simulation_gpu_elapsed_ms']['median']:.3f} | {t['render_gpu_ms']['median']:.3f} | {diff} |")
 lines+=['','V2: OS Application Control blocked the executable; not measured.']
 (base/'history-summary.md').write_text('\n'.join(lines)+'\n',encoding='utf8')
 print('Historical case rows:',len(rows),'errors:',len(bad))

if __name__=='__main__':main()
