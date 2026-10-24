"""Execute a saved A/B plan serially; preserve each run and final numerical/visual data."""
from __future__ import annotations
import argparse,datetime as dt,hashlib,json,os,subprocess,sys,time,zipfile
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]

def save(path,value):
    temporary=path.with_name(path.name+'.tmp')
    temporary.write_text(json.dumps(value,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    temporary.replace(path)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--plan',type=Path,required=True)
    parser.add_argument('--result',type=Path,required=True)
    parser.add_argument('--resume',action='store_true')
    parser.add_argument('--retry-failed',action='store_true',help='Archive failed attempts before retrying them; requires --resume')
    args=parser.parse_args()
    plan=json.loads(args.plan.read_text(encoding='utf8'))
    result=args.result.resolve(); result.mkdir(parents=True,exist_ok=True)
    raw=result/'raw';raw.mkdir(exist_ok=True)
    manifest_path=result/'manifest.json'
    if manifest_path.exists() and not args.resume:
        parser.error('Result already contains a manifest; use --resume or a new result directory to preserve the existing measurements.')
    assert len({r['id'] for r in plan['runs']})==len(plan['runs']), 'Duplicate planned run IDs'
    manifest=json.loads(manifest_path.read_text(encoding='utf8')) if args.resume and manifest_path.exists() else {
        'schema_version':1,'created_at':dt.datetime.now().astimezone().isoformat(),'status':'running',
        'plan':plan,'runs':[]}
    if args.retry_failed:
        assert args.resume, '--retry-failed requires --resume'
        failed=[r for r in manifest['runs'] if r['status']!='complete']
        if failed:
            archive=raw/'failed-attempts'/dt.datetime.now().strftime('%Y%m%d-%H%M%S')
            archive.mkdir(parents=True,exist_ok=False)
            for run in failed:
                for path in raw.glob(run['id']+'.*'):
                    path.rename(archive/path.name)
                run['archived_outputs']=archive.relative_to(result).as_posix()
            save(archive/'attempts.json',failed)
            manifest.setdefault('previous_attempts',[]).extend(failed)
            manifest['runs']=[r for r in manifest['runs'] if r['status']=='complete']
    save(result/'plan.json',plan)
    done={r['id'] for r in manifest['runs']}
    manifest['status']='running';save(manifest_path,manifest)
    for run in plan['runs']:
        if run['id'] in done: continue
        while (ROOT/'Build/performance-suite.pause').exists(): time.sleep(1)
        path=raw/(run['id']+'.jsonl')
        settings_path=raw/(run['id']+'.input.json')
        save(settings_path,run.get('settings',{}))
        env=dict(os.environ,MDSS_PERF_SETTINGS=str(settings_path))
        binary=Path(run['binary'])
        if run.get('historical_env'):
            env.update({str(k):str(v) for k,v in run['historical_env'].items()})
            env['MDSS_PERF_HISTORY_OUTPUT']=str(path)
            command=[str(binary),'--frames',str(run.get('warmup',60)+run.get('measure',600)+2)]
        else:
            command=[str(binary),'--benchmark-scene',run['scene'],'--benchmark-resolution',str(run['resolution']),
                     '--benchmark-warmup-frames',str(run.get('warmup',60)),
                     '--benchmark-measure-frames',str(run.get('measure',600)),'--benchmark-output',str(path)]
        record={**run,'command':command,'started_at':dt.datetime.now().astimezone().isoformat(),
                'raw':path.relative_to(result).as_posix(),'binary_sha256':hashlib.sha256(binary.read_bytes()).hexdigest()}
        if run.get('historical_env'):
            record['effective_historical_env']={k:env[k] for k in run['historical_env']}
        started=time.perf_counter()
        try:
            with path.with_suffix('.log').open('w',encoding='utf8') as log:
                completed=subprocess.run(command,cwd=run.get('cwd',ROOT),env=env,stdout=log,stderr=subprocess.STDOUT,
                                         timeout=run.get('timeout',900))
            record['exit_code']=completed.returncode
            samples=[json.loads(line) for line in path.read_text(encoding='utf8').splitlines() if line.strip()] if path.exists() else []
            record['samples']=len(samples)
            assert completed.returncode==0,f'exit={completed.returncode}'
            assert len(samples)==run.get('measure',600),f'sample count={len(samples)}'
            warmup=run.get('warmup',60)
            assert [x['frame_index'] for x in samples]==list(range(warmup,warmup+run.get('measure',600)))
            record['status']='complete'
        except Exception as exc:
            record['status']='failed';record['error']=str(exc)
        record['finished_at']=dt.datetime.now().astimezone().isoformat()
        record['process_seconds']=time.perf_counter()-started
        manifest['runs'].append(record);save(manifest_path,manifest)
        print(f"{len(manifest['runs'])}/{len(plan['runs'])} {run['id']}: {record['status']} ({record['process_seconds']:.1f}s)",flush=True)
    manifest['status']='complete' if all(r['status']=='complete' for r in manifest['runs']) else 'completed_with_failures'
    manifest['finished_at']=dt.datetime.now().astimezone().isoformat()
    save(manifest_path,manifest)
    return 0

if __name__=='__main__': raise SystemExit(main())
