from pathlib import Path
import concurrent.futures,hashlib,json,subprocess
root=Path.cwd();work=root/'build/m2-close-check';tasks=[]
for config in ('inspector-debug','inspector-release','inspector-asan','scene-shipping','scene-no-shaders'):
 db=root/'build'/config/'compile_commands.json';rows=json.loads(db.read_text());selected=[r for r in rows if Path(r['file'])==root/'apps/resource_probe/main.cpp'];assert len(selected)==1;out=work/'rtx-final-ast'/config;out.mkdir(parents=True,exist_ok=True);(out/'compile_commands.json').write_text(json.dumps(selected,indent=2)+'\n');tasks.append((config,out,hashlib.sha256(db.read_bytes()).hexdigest()))
def run(task):
 config,out,identity=task
 with (out/'analysis.txt').open('w') as f:r=subprocess.run(['clang-tidy','-p',str(out),str(root/'apps/resource_probe/main.cpp')],stdout=f,stderr=subprocess.STDOUT,timeout=240)
 return {'configuration':config,'source':'apps/resource_probe/main.cpp','status':r.returncode,'compile_database_sha256':identity,'flags':'Actual complete native invocation; no flags removed'}
with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:results=list(pool.map(run,tasks))
(work/'rtx-final-ast-results.json').write_text(json.dumps(results,indent=2)+'\n');print(json.dumps(results,indent=2));raise SystemExit(int(any(r['status'] for r in results)))
