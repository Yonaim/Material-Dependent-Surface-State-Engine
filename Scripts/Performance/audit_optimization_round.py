"""Audit reproducibility of completed, frozen optimization comparisons."""
from __future__ import annotations
import hashlib,json
from collections import Counter
from pathlib import Path
from optimization_round import RESULT,LAB,save

def audit():
    queue=json.loads((LAB/'queue.json').read_text())
    names=['baseline','PERF028','PERF007']+queue['plans'];issues=[];runs=frames=excluded=0
    snapshots={stage:json.loads((LAB/stage/'snapshot.json').read_text()) for stage in ['S00','S01','S02','S03','S04']}
    settings_groups={};comparisons=0;unavailable=[];scene_checks=0;order_changes=[]
    for name in names:
        folder=RESULT/name;manifest=json.loads((folder/'manifest.json').read_text())
        if manifest['status']!='complete':issues.append(f'{name}: {manifest["status"]}')
        planned=json.loads((folder/'plan.json').read_text())['runs']
        actual_ids=[r['id'] for r in manifest['runs']];planned_ids=[r['id'] for r in planned]
        if Counter(actual_ids)!=Counter(planned_ids):issues.append(f'{name}: planned run IDs missing/duplicated')
        elif actual_ids!=planned_ids:
            if not manifest.get('previous_attempts'):issues.append(f'{name}: unexplained execution order change')
            order_changes.append({'suite':name,'reason':'failed attempts archived; successful runs retained before retries',
                                  'planned':planned_ids,'actual':actual_ids})
        planned_by_id={r['id']:r for r in planned}
        excluded+=len(manifest.get('previous_attempts',[]))
        for record in manifest['runs']:
            if record['status']!='complete':issues.append(f'{record["id"]}: {record["status"]}');continue
            if any(record.get(k)!=v for k,v in planned_by_id[record['id']].items()):issues.append(f'{record["id"]}: planned configuration changed')
            runs+=1;frames+=record['samples'];raw=folder/record['raw']
            binary=Path(record['binary'])
            if hashlib.sha256(binary.read_bytes()).hexdigest()!=record['binary_sha256']:issues.append(f'{record["id"]}: binary changed')
            expected_shaders={k.replace('\\','/').removeprefix('Shaders/'):v for k,v in snapshots[binary.parent.name]['files'].items() if k.replace('\\','/').startswith('Shaders/') and k.endswith('.spv')}
            if record.get('shader_sha256')!=expected_shaders:issues.append(f'{record["id"]}: shader archive mismatch')
            copied_scene=raw.with_suffix('.scene.json')
            if record.get('scene_sha256'):
                scene_checks+=1
                if not copied_scene.exists() or hashlib.sha256(copied_scene.read_bytes()).hexdigest()!=record['scene_sha256']:
                    issues.append(f'{record["id"]}: saved Scene hash mismatch')
            else:unavailable.append(f'{record["id"]}: no per-run Scene hash; original assets retained in stage inputs.zip')
            settings=json.loads(Path(str(raw)+'.settings.json').read_text())
            if not settings['geometry_feedback']:issues.append(f'{record["id"]}: feedback off')
            if settings['surface_mesh_resolution']!=record['resolution'] and name!='PERF037-lowmesh':issues.append(f'{record["id"]}: mesh resolution')
            cpu=json.loads(Path(str(raw)+'.cpu.json').read_text())
            if len(cpu)!=record['warmup']+record['measure']+2:issues.append(f'{record["id"]}: CPU sample count')
            allowed={'sparse_solver'} if name=='PERF007' else set()
            shared={k:v for k,v in settings.items() if k not in allowed}
            key=(record.get('experiment',name),record['scene'],record['resolution'])
            signature=json.dumps(shared,sort_keys=True)
            if key in settings_groups and settings_groups[key]!=signature:issues.append(f'{record["id"]}: camera/viewport/options mismatch')
            settings_groups[key]=signature
            validation=json.loads(Path(str(raw)+'.validation.json').read_text())
            requested=record.get('settings',{})
            if validation.get('experiment')!=requested:issues.append(f'{record["id"]}: applied experiment settings mismatch')
            for flag in ['runtime_solver','compact_storage','adaptive_geometry']:
                if flag in requested and flag in validation and requested[flag]!=validation[flag]:
                    issues.append(f'{record["id"]}: actual {flag} differs from request')
            if validation['simulation_step_serial']!=662:issues.append(f'{record["id"]}: step count')
            if any(i['nonfinite'] or i['negative'] for i in validation['instances']):issues.append(f'{record["id"]}: State invalid')
        comparison_file=folder/'comparisons.json'
        if comparison_file.exists():
            data=json.loads(comparison_file.read_text());issues+=data['issues'];comparisons+=sum(c['state'] is not None for c in data['comparisons'])
    stages=['S00','S01','S02','S03','S04']
    for stage in stages:
        path=LAB/stage;snapshot=json.loads((path/'snapshot.json').read_text())
        for relative,expected in snapshot['files'].items():
            file=path/relative
            if not file.exists() or hashlib.sha256(file.read_bytes()).hexdigest()!=expected:issues.append(f'{stage}: changed archive {relative}')
    result={'status':'pass' if not issues else 'fail','successful_runs':runs,'measured_frames':frames,
            'paired_conditions':comparisons,'excluded_initial_attempts':excluded,'issues':issues,
            'per_run_scene_hash_checks':scene_checks,'unavailable_checks':unavailable,
            'execution_order_changes':order_changes,
            'scope':'frozen S00-S04 only; not the blocked final integrated app, pixel quality, physical conservation or VRAM residency'}
    save(RESULT/'study-audit.json',result);print(json.dumps(result,ensure_ascii=False));return result
if __name__=='__main__':audit()
