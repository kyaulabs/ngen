from pathlib import Path
import concurrent.futures,json,os,subprocess
root=Path.cwd();work=root/'build/m2-close-check';configs=('inspector-debug','inspector-release','inspector-asan','inspector-gcc','scene-shipping','scene-no-shaders')
def run(config):
 build=root/'build'/config;env=os.environ.copy();env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
 row={'configuration':config};prefix=work/f'rtx-final-{config}'
 with prefix.with_name(prefix.name+'-build.txt').open('w') as out:r=subprocess.run(['cmake','--build',str(build),'-j','2'],stdout=out,stderr=subprocess.STDOUT,env=env,timeout=600)
 row['build']=r.returncode
 if r.returncode:return row
 selected=subprocess.run(['ctest','--test-dir',str(build),'--show-only=json-v1'],capture_output=True,text=True,env=env,timeout=60);assert selected.returncode==0
 selection=json.loads(selected.stdout);assert selection['tests'];prefix.with_name(prefix.name+'-selection.json').write_text(json.dumps(selection,indent=2)+'\n');row['selected']=len(selection['tests'])
 with prefix.with_name(prefix.name+'-tests.txt').open('w') as out:r=subprocess.run(['ctest','--test-dir',str(build),'--output-on-failure','--no-tests=error'],stdout=out,stderr=subprocess.STDOUT,env=env,timeout=600)
 row['tests']=r.returncode;return row
with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:results=list(pool.map(run,configs))
(work/'rtx-final-graphs.json').write_text(json.dumps(results,indent=2)+'\n');print(json.dumps(results,indent=2))
if any(r.get('build') or r.get('tests',1) for r in results):raise SystemExit(1)
# Native GPU tests are serial, not timing measurements.
for row in results[:4]:
 config=row['configuration'];env=os.environ.copy();env.update(VK_LAYER_PATH=str(root/'build/validation-layer/usr/share/vulkan/explicit_layer.d'),LD_LIBRARY_PATH=str(root/'build/validation-layer/usr/lib')+(':'+env['LD_LIBRARY_PATH'] if env.get('LD_LIBRARY_PATH') else ''),ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
 for app in ('resource_probe','inspector_probe'):
  prefix=work/f'rtx-final-{config}-{app}'
  with prefix.with_suffix('.txt').open('w') as out,prefix.with_suffix('.err').open('w') as err:r=subprocess.run(['timeout','--kill-after=5s','120s',str(root/'build'/config/app)],stdout=out,stderr=err,env=env,timeout=135)
  row[app]=r.returncode;row[app+'_stderr_bytes']=prefix.with_suffix('.err').stat().st_size
  if r.returncode or row[app+'_stderr_bytes']:raise RuntimeError(f'{config} {app} failed: {r.returncode}; inspect owned native records')
(work/'rtx-final-verification.json').write_text(json.dumps(results,indent=2)+'\n');print('Six full build/CTest graphs and eight serial native probe runs pass.')
