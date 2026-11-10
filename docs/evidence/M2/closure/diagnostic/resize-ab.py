"""Predeclared three-pair diagnostic; instrumentation is not a clean budget run."""
from pathlib import Path
import csv,hashlib,json,os,subprocess,time
root=Path.cwd();exe=root/'build/inspector-release/scene_workload'
package=root/'build/cooked-candidate/frigate-canonical-first/scene.ngas'
def sha(p):
    with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
identity={'exe_sha256':sha(exe),'package_sha256':sha(package)}
result=[]
# Fixed order/count chosen before observing any result; retain all outcomes.
for pair,order in enumerate((('baseline','handoff'),('handoff','baseline'),('baseline','handoff')),1):
    for mode in order:
        prefix=root/f'build/m2-close-check/resize-ab-{pair:02}-{mode}'
        paths={key:Path(str(prefix)+suffix) for key,suffix in (('stdout','.txt'),('stderr','.err'),('report','.csv'))}
        assert not any(p.exists() or p.is_symlink() for p in paths.values())
        env=os.environ.copy()
        libs=[str(root/'build/bloom-check/latency.so')]
        if mode=='handoff':libs.insert(0,str(root/'build/m2-close-check/swapchain-handoff.so'))
        env['LD_PRELOAD']=':'.join(libs)
        arguments=[str(exe),str(package),'--inspector','--walkthrough','--reload-cycles=4','--frames=600','--warmup=60',f'--report={paths["report"]}']
        started=time.monotonic()
        with paths['stdout'].open('x') as output,paths['stderr'].open('x') as errors:
            run=subprocess.run(arguments,env=env,stdout=output,stderr=errors,timeout=100)
        rows=list(csv.DictReader(paths['report'].open())) if paths['report'].exists() else []
        item={'pair':pair,'mode':mode,'exit':run.returncode,'seconds':time.monotonic()-started,'rows':len(rows),
              'max_frame_ns':max((int(r['frame_ns']) for r in rows),default=None),
              'max_resize_window_ns':max((int(r['frame_ns']) for r in rows if 20<=int(r['frame'])<65),default=None),
              'slow_resize_rows':[{k:r[k] for k in ('frame','frame_ns','present_ns','surface_width','surface_height')} for r in rows if 20<=int(r['frame'])<65 and int(r['frame_ns'])>40000000],
              'artifacts':{key:{'bytes':p.stat().st_size,'sha256':sha(p)} for key,p in paths.items() if p.exists()}}
        result.append(item)
        print(json.dumps(item),flush=True)
        assert identity=={'exe_sha256':sha(exe),'package_sha256':sha(package)}
        if run.returncode:break
    if result[-1]['exit']:break
out=root/'build/m2-close-check/resize-ab.json'
with out.open('x') as f:json.dump({'scope':'diagnostic only, three pairs fixed in advance','identity':identity,'runs':result},f,indent=2);f.write('\n')
assert len(result)==6 and all(r['exit']==0 for r in result)
