"""Compare state arrays after one actual transport step, excluding initial injection and drain steps."""
from pathlib import Path
import argparse,json
from analyze_suite import load,collect,quality,COMPARISONS

def main():
 ap=argparse.ArgumentParser();ap.add_argument('--result',type=Path,required=True);args=ap.parse_args()
 base=args.result.resolve();manifest=load(base/'manifest.json');items={};errors=[]
 for run in manifest['runs']:
  if run['status']!='complete':errors.append(run);continue
  item=collect(base,run)
  if item['validation'].get('simulation_step_serial')!=2:
   errors.append({'id':run['id'],'error':'Final step serial is not 2'});continue
  items[(run['scene_name'],run['resolution'],run['variant'])]=item
 rows=[]
 for group,name,a,b,metric in COMPARISONS:
  for scene,res in sorted({(s,r) for s,r,v in items}):
   if (scene,res,a) in items and (scene,res,b) in items:
    rows.append({'group':group,'comparison':name,'scene':scene,'resolution':res,
                 'base_variant':a,'candidate_variant':b,'quality':quality(base,items[(scene,res,a)],items[(scene,res,b)])})
 result={'scope':'Initial injection step plus exactly one subsequent transport step; drain frames do not advance simulation',
         'status':manifest['status'],'valid_runs':len(items),'errors':errors,'comparisons':rows}
 (base/'validation-summary.json').write_text(json.dumps(result,indent=2,ensure_ascii=False)+'\n',encoding='utf8')
 print('Validated single-transport cases:',len(items),'paired comparisons:',len(rows),'errors:',len(errors))

if __name__=='__main__':main()
