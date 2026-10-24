"""Regenerate A/B timing and final-state/image comparison tables from run_suite outputs."""
from __future__ import annotations
import argparse,json,math,statistics as st,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'Build/PerformanceLab/PythonPackages'))
import numpy as np
from PIL import Image
from summarize import percentile

COMPARISONS=[
 ('G1','disk_cache','cold_load','reference','scene_load_ms'),
 ('G1','buffer_pool','reload_no_pool','reload_pool','reload_ms'),
 ('G1','shared_profiles','unshared_profiles','reference','profile_bytes'),
 ('G2','raw_flux_cache_fp32','cache_off','cache_fp32','simulation_gpu_elapsed_ms'),
 ('G2','inactive_zero_write_elision','zero_writes','reference','simulation_gpu_elapsed_ms'),
 ('G3','sparse_solver','solver_dense','reference','simulation_gpu_elapsed_ms'),
 ('G3','sparse_height','height_dense','reference','simulation_gpu_elapsed_ms'),
 ('G3','sparse_geometry','geometry_dense','reference','simulation_gpu_elapsed_ms'),
 ('G3','sparse_all','dense_all','reference','simulation_gpu_elapsed_ms'),
 ('G3','global_channel_mask','mask_none','mask_global','simulation_gpu_elapsed_ms'),
 ('G3','workgroup_channel_mask','mask_global','reference','simulation_gpu_elapsed_ms'),
 ('G4','instance_batching','serial','reference','simulation_gpu_elapsed_ms'),
 ('G5','coalesced_fp32','legacy_layout','cache_fp32','simulation_gpu_elapsed_ms'),
 ('G5','fp16_flux','cache_fp32','reference','simulation_gpu_elapsed_ms'),
 ('G5','fp16_weights','reference','weights_fp16','simulation_gpu_elapsed_ms'),
 ('G6','state_texture','fragment_ssbo','fragment_texture','render_gpu_ms'),
 ('G6','precompute_coverage','fragment_texture','reference','render_gpu_ms'),
 ('G6','separable_coverage','coverage_direct','reference','render_gpu_ms'),
 ('G6','height_halo','reference','height_halo','render_gpu_ms'),
 ('G6','coverage_halo','reference','coverage_halo','render_gpu_ms'),
 ('G6','state_texture_halo','reference','texture_halo','render_gpu_ms'),
 ('G7','dirty_overlay','overlay_full','reference','render_gpu_ms'),
 ('G7','tile_culling','tile_culling_off','reference','render_gpu_ms'),
 ('G7','mesh128','reference','mesh128','render_gpu_ms'),
 ('G7','frustum_culling','no_frustum','reference','render_gpu_ms'),
 ('G8','gpu_validation_elision','validation','reference','frame_gpu_elapsed_ms'),
 ('G8','compile_time_solver','runtime','reference','simulation_gpu_elapsed_ms'),
 ('ROT','rotation_invariant_cache','matrix_cache','scale_cache','cpu_static_cache_ms'),
 ('G6','sparse_render_height','height_smoothing_full','reference','render_gpu_ms'),
 ('G7','tile8','reference','tile8','render_gpu_ms'),
 ('G7','tile32','reference','tile32','render_gpu_ms'),
 ('G7','mesh64','reference','mesh64','render_gpu_ms'),
 ('G7','mesh256','reference','mesh256','render_gpu_ms'),
 ('G7','mesh512','reference','mesh512','render_gpu_ms'),
 ('G2','source_reuse','solver_reuse_off','reference','simulation_gpu_elapsed_ms'),
 ('G6','render_math_reuse','render_reuse_off','reference','render_gpu_ms'),
 ('G6','render_math_reuse_ssbo','render_ssbo_reuse_off','render_ssbo_reuse_on','render_gpu_ms'),
 ('G7','top_triangle_compaction','full_top','compact_top','render_gpu_ms'),
]

def load(path):return json.loads(path.read_text(encoding='utf8'))
def sample_paths(base,run):return base/run['raw']
def collect(base,run):
    path=sample_paths(base,run)
    samples=[json.loads(line) for line in path.read_text().splitlines() if line.strip()]
    metrics={}
    for field in samples[0] if samples else []:
        if field.endswith('_ms'):
            values=[x[field] for x in samples if isinstance(x.get(field),(int,float)) and math.isfinite(x[field])]
            if values:metrics[field]={'median':st.median(values),'p95':percentile(values,.95)}
    cpu=Path(str(path)+'.cpu.json')
    if cpu.exists():
        values=load(cpu)[run.get('warmup',60):run.get('warmup',60)+run.get('measure',600)]
        metrics['frame_cpu_elapsed_ms']={'median':st.median(values),'p95':percentile(values,.95)}
    p=Path(str(path)+'.load.json')
    if p.exists():metrics['scene_load_ms']={'median':load(p)['scene_load_ms']}
    p=Path(str(path)+'.reload.json')
    if p.exists():
        reloads=load(p);values=[x['milliseconds'] for x in reloads]
        metrics['reload_ms']={'median':st.median(values),'p95':percentile(values,.95)}
        for field in ['reuse_count','reused_bytes','retained_bytes']:
            metrics['reload_'+field]={'median':reloads[-1][field]}
    validation=load(Path(str(path)+'.validation.json'))
    if 'buffer_category_bytes' in validation:
        metrics['profile_bytes']={'median':validation['buffer_category_bytes'][2]}
        metrics['allocated_bytes']={'median':validation['allocated_total_bytes']}
        if 'peak_allocated_total_bytes' in validation:
            metrics['peak_allocated_bytes']={'median':validation['peak_allocated_total_bytes']}
    if 'cpu_static_cache_ms' in validation:
        metrics['cpu_static_cache_ms']={'median':validation['cpu_static_cache_ms']}
    settings=load(Path(str(path)+'.settings.json'))
    activity={}
    for numerator,denominator,label in [('overlay_active_tiles','overlay_total_tiles','overlay_tile_fraction'),
                                        ('overlay_active_top_triangles','overlay_total_top_triangles','overlay_top_fraction')]:
        values=[s[numerator]/s[denominator] for s in samples if s.get(denominator,0)>0]
        if values:activity[label]=st.median(values)
    return {'run':run,'metrics':metrics,'validation':validation,'settings':settings,'activity':activity,
            'state_channels':samples[0].get('state_channels') if samples else None}

def image_pixels(base,item):
    path=sample_paths(base,item['run'])
    data=Path(str(path)+'.bgra')
    if not data.exists():return None
    settings=item['settings'];w,h=settings['swapchain_extent']
    array=np.fromfile(data,dtype=np.uint8).reshape(h,w,4)[:,:, [2,1,0]]
    r=settings['viewports'][0]['normalized_rect']
    x,y=int(round(r[0]*w)),int(round(r[1]*h))
    width,height=int(round(r[2]*w)),int(round(r[3]*h))
    return array[y:y+height,x:x+width]

def quality(base,a,b):
    if any(x['nonfinite'] or x['negative'] for item in [a,b] for x in item['validation']['instances']):
        return {'status':'invalid_state_values'}
    total_diff=0.;total_a=0.;max_abs=0.;max_a=0.;changed=0;count=0;exact=True
    ids=[x['instance'] for x in a['validation']['instances']]
    for index in ids:
        pa=Path(str(sample_paths(base,a['run']))+f'.state-{index}.f32')
        pb=Path(str(sample_paths(base,b['run']))+f'.state-{index}.f32')
        if not pa.exists() or not pb.exists():return {'status':'state_unavailable'}
        va=np.fromfile(pa,dtype='<f4');vb=np.fromfile(pb,dtype='<f4')
        if va.shape!=vb.shape:return {'status':'shape_mismatch'}
        diff=np.abs(va.astype(np.float64)-vb.astype(np.float64));absolute=np.abs(va.astype(np.float64))
        total_diff+=float(diff.sum());total_a+=float(absolute.sum());max_abs=max(max_abs,float(diff.max(initial=0)))
        max_a=max(max_a,float(absolute.max(initial=0)));changed+=int(np.count_nonzero(diff));count+=diff.size
        exact=exact and np.array_equal(va,vb)
    result={'status':'complete','state_exact':exact,'state_l1_relative':total_diff/max(total_a,1e-30),
            'state_max_absolute':max_abs,'state_max_relative_to_global_peak':max_abs/max(max_a,1e-30),
            'state_changed_fraction':changed/max(count,1)}
    ia=image_pixels(base,a);ib=image_pixels(base,b)
    if ia is not None and ib is not None and ia.shape==ib.shape:
        diff=np.abs(ia.astype(np.float64)-ib.astype(np.float64));rmse=float(np.sqrt(np.mean(diff**2)))
        result.update({'image_exact':bool(np.array_equal(ia,ib)),'image_mae_u8':float(diff.mean()),
                       'image_p99_u8':float(np.quantile(diff,.99)),'image_max_u8':float(diff.max()),
                       'image_changed_pixel_fraction':float(np.mean(np.any(diff!=0,axis=2))),
                       'image_psnr_db':20*math.log10(255/rmse) if rmse else None})
    return result

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--result',type=Path,required=True);args=ap.parse_args()
    base=args.result.resolve();m=load(base/'manifest.json');groups={};bad=[]
    for run in m['runs']:
        if run['status']!='complete':bad.append(run);continue
        try:
            item=collect(base,run)
            groups.setdefault((run['group'],run['scene_name'],run['resolution'],run['variant']),[]).append(item)
        except Exception as e:bad.append({'id':run['id'],'analysis_error':repr(e)})
    image_noise={}
    for key,items in groups.items():
        reference=image_pixels(base,items[0])
        if reference is None:continue
        differences=[]
        for other in items[1:]:
            pixels=image_pixels(base,other)
            if pixels is not None and pixels.shape==reference.shape:
                differences.append(float(np.mean(np.abs(reference.astype(np.float32)-pixels.astype(np.float32)))))
        image_noise[key]=max(differences,default=0.0)
    rows=[]
    for group,name,base_arm,candidate,metric in COMPARISONS:
        cases=sorted({(scene,resolution) for g,scene,resolution,variant in groups if g==group})
        for scene,resolution in cases:
            aa=groups.get((group,scene,resolution,base_arm),[]);bb=groups.get((group,scene,resolution,candidate),[])
            if len(aa)!=3 or len(bb)!=3:continue
            av=[x['metrics'][metric]['median'] for x in aa];bv=[x['metrics'][metric]['median'] for x in bb]
            ba=st.median(av);ca=st.median(bv)
            same_repeat=all(x['validation']['instances']==aa[0]['validation']['instances'] for x in aa) and all(x['validation']['instances']==bb[0]['validation']['instances'] for x in bb)
            q=quality(base,aa[0],bb[0])
            if group=='G1':
                q['image_scope']='Loading arms snapshot the initial viewport before UI layout settles. Saved-rectangle image metrics are archived but excluded from visual-quality conclusions.'
            q['base_repeat_image_mae_max']=image_noise.get((group,scene,resolution,base_arm))
            q['candidate_repeat_image_mae_max']=image_noise.get((group,scene,resolution,candidate))
            verdict='decrease_observed' if max(bv)<min(av) else 'increase_observed' if min(bv)>max(av) else 'overlapping_repeat_ranges'
            row={'group':group,'comparison':name,'scene':scene,'resolution':resolution,'base_variant':base_arm,'candidate_variant':candidate,
                 'metric':metric,'base_median':ba,'candidate_median':ca,'reduction_percent':(ba-ca)/ba*100 if ba else None,
                 'base_repeat_medians':av,'candidate_repeat_medians':bv,'base_range':[min(av),max(av)],'candidate_range':[min(bv),max(bv)],
                 'repeat_state_reproducible':same_repeat,'quality':q,'speed_verdict':verdict}
            for prefix,items in [('base',aa),('candidate',bb)]:
                p95=[x['metrics'][metric].get('p95') for x in items]
                row[prefix+'_p95']=st.median(p95) if all(isinstance(x,(int,float)) for x in p95) else None
            row['secondary_metrics']={}
            for secondary in ['frame_gpu_elapsed_ms','simulation_gpu_elapsed_ms','render_gpu_ms','frame_cpu_elapsed_ms',
                              'command_record_cpu_ms','fence_wait_cpu_ms',
                              'profile_bytes','allocated_bytes','peak_allocated_bytes','reload_reuse_count','reload_reused_bytes','reload_retained_bytes',
                              'pass1_gpu_ms','pass2_gpu_ms','geometry_gpu_ms',
                              'transfer_weight_gpu_ms','render_preparation_gpu_ms','overlay_preparation_gpu_ms',
                              'base_draw_gpu_ms','overlay_draw_gpu_ms']:
                if all(secondary in item['metrics'] for item in aa+bb):
                    row['secondary_metrics'][secondary]={
                        'base_median':st.median(x['metrics'][secondary]['median'] for x in aa),
                        'candidate_median':st.median(x['metrics'][secondary]['median'] for x in bb)}
            if name=='shared_profiles' and scene!='Mountain-DistinctRuntime':
                row['historical_base_valid']=False
                row['method_note']='Counterfactual per-instance duplication; the pre-sharing implementation already shared profiles within one runtime. Use the distinct-runtime scene for historical-policy attribution.'
            rows.append(row)
    workloads=[]
    for (group,scene,resolution,variant),items in sorted(groups.items()):
        if group!='G3' or variant!='reference' or len(items)!=3:continue
        channels=items[0]['state_channels']
        if not channels:continue
        positive=0;texels=0;active_channels=np.zeros(channels,dtype=bool)
        for instance in items[0]['validation']['instances']:
            raw=sample_paths(base,items[0]['run'])
            values=np.fromfile(Path(str(raw)+f".state-{instance['instance']}.f32"),dtype='<f4').reshape(-1,channels)
            positive+=int(np.count_nonzero(np.any(values>0,axis=1)));texels+=values.shape[0]
            active_channels|=np.any(values>0,axis=0)
        workloads.append({'scene':scene,'resolution':resolution,'allocated_texels':texels,
                          'positive_state_texels_after_662_steps':positive,'positive_state_texel_fraction':positive/texels,
                          'nonzero_state_channels':int(np.count_nonzero(active_channels)),'registered_channels':channels,
                          **{key:st.median(item['activity'][key] for item in items) for key in ['overlay_tile_fraction','overlay_top_fraction'] if all(key in item['activity'] for item in items)},
                          'solver_active_workgroup_fraction':None})
    audit={'status':m['status'],'planned_runs':len(m['plan']['runs']),'captured_runs':len(m['runs']),
           'complete_runs':sum(r['status']=='complete' for r in m['runs']),'failed_runs':bad,
           'nonfinite_state':sum(i['nonfinite'] for v in groups.values() for x in v for i in x['validation']['instances']),
           'negative_state':sum(i['negative'] for v in groups.values() for x in v for i in x['validation']['instances'])}
    (base/'comparisons.json').write_text(json.dumps({'audit':audit,'comparisons':rows,'workloads':workloads},indent=2,ensure_ascii=False)+'\n',encoding='utf8')
    lines=['# Optimization A/B summary','',f"Completed runs: {audit['complete_runs']}/{audit['planned_runs']}; status={m['status']}",'',
           'Time values are median of 3 per-run medians. Percent reductions use the indicated scope, not FPS. State/image differences are measured at the final drain frame.','']
    for group in dict.fromkeys(x['group'] for x in rows):
        lines += [f'## {group}','','| Comparison | Scene / resolution | Base | Candidate | Reduction % | State relative L1 | Image MAE (0–255) | Repeat ranges |', '|---|---|---:|---:|---:|---:|---:|---|']
        for row in [x for x in rows if x['group']==group]:
            q=row['quality'];err=q.get('state_l1_relative');img=q.get('image_mae_u8')
            reduction='—' if row['reduction_percent'] is None else f"{row['reduction_percent']:.2f}"
            lines.append(f"| {row['comparison']} | {row['scene']} / {row['resolution']} | {row['base_median']:.4f} | {row['candidate_median']:.4f} | {reduction} | {err:.3e} | {img:.3f} | {row['speed_verdict']} |" if err is not None and img is not None else f"| {row['comparison']} | {row['scene']} / {row['resolution']} | {row['base_median']:.4f} | {row['candidate_median']:.4f} | {reduction} | {err} | {img} | {row['speed_verdict']} |")
        lines.append('')
    (base/'comparisons.md').write_text('\n'.join(lines)+'\n',encoding='utf8')
    print(f"Analyzed {audit['complete_runs']} runs; {len(rows)} paired case comparisons; errors={len(bad)}")

if __name__=='__main__':main()
