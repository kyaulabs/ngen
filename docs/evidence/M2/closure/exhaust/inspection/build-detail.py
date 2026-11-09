"""Reuse the actual declared scene_probe compiler/link argument lists; no shell execution."""
from pathlib import Path
import json,shlex,subprocess
root=Path.cwd();work=root/'build/m2-engine-color';row=json.loads((work/'detail-compile-template.json').read_text());args=shlex.split(row['command']);source=str(work/'detail.cpp');obj=str(work/'detail.cpp.o');exe=str(work/'detail')
assert row['file'] in args and '-o' in args
args[args.index(row['file'])]=source;args[args.index('-o')+1]=obj
for flag,value in (('-MT',obj),('-MF',obj+'.d')):
 if flag in args:args[args.index(flag)+1]=value
link=shlex.split((work/'detail-link-template.txt').read_text())
if link[:2]==[':','&&']:link=link[2:]
if link[-2:]==['&&',':']:link=link[:-2]
assert not any(x in {'&&',';','|'} for x in link)
main='CMakeFiles/scene_probe.dir/apps/scene_probe/main.cpp.o';draw='CMakeFiles/scene_probe.dir/apps/scene_probe/draw.cpp.o'
assert main in link and draw in link
index=link.index(main);link[index]=obj;link.insert(index+1,str(root/'build/inspector-release/libscene_workload_fixture.a'));link.remove(draw);link[link.index('-o')+1]=exe
(work/'detail-build-arguments.json').write_text(json.dumps({'scope':'native temporary inspection tool, actual inspector-release scene_probe commands; output/object/source substituted, existing fixture archive added','directory':row['directory'],'compile':args,'link':link},indent=2)+'\n')
for command in (args,link):
 result=subprocess.run(command,cwd=row['directory'],capture_output=True,text=True,timeout=120)
 print(result.stdout,end='');print(result.stderr,end='');assert result.returncode==0
