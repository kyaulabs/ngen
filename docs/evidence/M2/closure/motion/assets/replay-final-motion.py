"""Exact current-host replay from the immutable accepted static base; only owned temporary output is removed."""
from pathlib import Path
import hashlib,json,os,re,subprocess,tempfile
root=Path.cwd();work=root/'build/m2-engine-color'
def digest(path):
 with path.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
authors=('author-jet.py','author-droplet.py','author-panelled.py','author-bridge.py','author-charcoal-glazing.py','author-bridge-recess.py')
expected=('5e69d51b48539b17b00b15d9b8e71d1d705b625bbfceab7cb539390aa24e988b','9b645de103ea92677c3cfe97cb8bd88ede8e7109f34c0f926888ba65c7522e63','a1f70ef7b3e6b7e67341e36b5828a759e9ca3502859d3fc6dba7c9a02688929f','bce7f71bfa18eea6c73d425565be71144915e9b3fde66063a263bcc424d09cc5','27de456e8f29b6392a50099dd65f69b2bd42a8ce2a73b3b3b0cc2c716811ee81','a02d5438ad32d9c8fb917ce847d5a822688ca5b51d69125f6da71cf2ba2116ed')
cache=(root/'build/asset-release/CMakeCache.txt').read_text();ktx=re.search(r'^NGEN_KTX_EXECUTABLE:STRING=(.+)$',cache,re.M)[1]
originals={p:digest(root/p) for p in ('planning/models/frigate/frigate-01.glb','build/cooked-candidate/frigate-blue-white-plume-wide/scene.ngas','build/cooked-candidate/frigate-bridge-recess-v1/scene.ngas')};results=[]
with tempfile.TemporaryDirectory(prefix='m2-motion-replay-',dir=root/'build') as temporary:
 scratch=Path(temporary)
 def link(relative,source=None):
  target=scratch/relative;target.parent.mkdir(parents=True,exist_ok=True);os.link(root/(source or relative),target)
 for path in ('planning/models/frigate/frigate-01.glb','build/cooked-candidate/frigate-blue-white-plume-wide/scene.ngas','build/cooked-candidate/frigate-blue-white-plume-wide/manifest.json','tools/assetc/cook.py','build/inspector-release/asset_package_probe'):link(path)
 for author in authors:link('build/m2-engine-color/'+author)
 (scratch/'build/asset-release').mkdir();(scratch/'build/asset-release/CMakeCache.txt').write_text('NGEN_KTX_EXECUTABLE:STRING='+ktx+'\n')
 for author,wanted in zip(authors,expected):
  result=subprocess.run(['python',str(scratch/'build/m2-engine-color'/author)],cwd=scratch,capture_output=True,text=True,timeout=180)
  (work/('replay-'+author+'.txt')).write_text(result.stdout);(work/('replay-'+author+'.err')).write_text(result.stderr)
  assert result.returncode==0,(author,result.returncode)
  report=json.loads(result.stdout);assert report['sha256']==wanted,(author,report['sha256'],wanted)
  results.append({'author':author,'author_sha256':digest(work/author),'package_sha256':report['sha256'],'expected_sha256':wanted,'exact_match':True})
for path,value in originals.items():assert digest(root/path)==value,('immutable input changed',path)
report={'scope':'Exact current-host Release-native/tool-pinned replay in a new owned scratch directory, not a cross-platform bitwise reproducibility claim','all_six_exact_package_identities_match':True,'originals_preserved':originals,'steps':results}
(work/'final-motion-exact-replay.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
