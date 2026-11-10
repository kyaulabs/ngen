"""Owner-requested darker coated hull and non-emissive translucent glazing; material-only derivative."""
from pathlib import Path
import hashlib,importlib.util,json,struct,subprocess,sys,tempfile
sys.dont_write_bytecode=True
root=Path.cwd();base=root/'build/cooked-candidate/frigate-hull-focus-bridge-v2/scene.ngas';out=root/'build/cooked-candidate/frigate-charcoal-glazing-v1'
def require(value,message):
 if not value:raise ValueError(message)
def sha(path):
 with path.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
require(not out.exists() and not out.is_symlink(),'output exists')
require(sha(base)=='bce7f71bfa18eea6c73d425565be71144915e9b3fde66063a263bcc424d09cc5','unexpected bridge/hull base')
raw=base.read_bytes();parts={}
for i in range(13):
 kind,stride,count,reserved,at,size=struct.unpack_from('<4I2Q',raw,64+i*32);require(kind==i+1 and reserved==0 and stride*count==size,'invalid section');parts[kind]=raw[at:at+size]
original=dict(parts);materials=bytearray(parts[4]);coating=[];window=[]
for i in range(len(materials)//128):
 at,n=struct.unpack_from('<2I',materials,i*128+100);name=parts[12][at:at+n].decode()
 if name.startswith('M2_HullPlating'):
  old=struct.unpack_from('<4f',materials,i*128);new=tuple(c*.4 for c in old[:3]);struct.pack_into('<3f',materials,i*128,*new);coating.append({'material':i,'name':name,'old_base_rgb':old[:3],'new_base_rgb':new})
 if name=='F01_Window':window.append(i)
require(len(coating)==3 and len(window)==1,'unexpected material membership');glass=window[0]
struct.pack_into('<4f',materials,glass*128,.015,.020,.025,.28)
struct.pack_into('<f',materials,glass*128+28,0.)
struct.pack_into('<2f',materials,glass*128+32,0.,.06)
struct.pack_into('<I',materials,glass*128+52,2)
parts[4]=bytes(materials)
for key in range(1,13):
 if key!=4:require(parts[key]==original[key],f'protected section changed: {key}')
require(parts[4][13*128:14*128]==original[4][13*128:14*128],'accepted exhaust material changed')
profile={'name':'charcoal-coated-hull-smoky-glazing-v1','base_sha256':sha(base),'author_sha256':sha(Path(__file__)),'coating_linear_scale':.4,'coating_materials':coating,'glazing_material':glass,'glazing_base_rgba':[.015,.020,.025,.28],'glazing_emissive_strength':0,'glazing_metallic':0,'glazing_roughness':.06,'glazing_alpha_mode':'blend','scope':'Owner rejected the light gray hull and cyan dashed glazing. Darken only the three coating material base colors; preserve existing wear/normal/ORM maps and all geometry. Use existing dielectric/glossy alpha-blend PBR for the six original glazing strips, not cyan emission. No cockpit interior, cut openings, refraction shader or new render pass. Preserve bridge-light correction, accepted exhaust, dark engine housings, all other materials and global lighting/exposure/bloom.'}
provenance=json.loads(parts[13]);provenance['charcoal_glazing']=profile;parts[13]=json.dumps(provenance,sort_keys=True,separators=(',',':')).encode();require(len(parts[13])<=65536,'provenance limit')
spec=importlib.util.spec_from_file_location('ngen_assembler',root/'tools/assetc/cook.py');cook=importlib.util.module_from_spec(spec);spec.loader.exec_module(cook)
with tempfile.TemporaryDirectory(prefix='.charcoal-',dir=out.parent) as temporary:
 staged=Path(temporary)/'package';staged.mkdir();(staged/'scene.ngas').write_bytes(cook.assemble(parts))
 result=subprocess.run([str(root/'build/inspector-release/asset_package_probe'),str(staged/'scene.ngas')],capture_output=True,text=True,timeout=60);require(result.returncode==0 and not result.stderr,result.stderr);runtime=json.loads(result.stdout)
 require(runtime['lod_triangles']==[574376,291044,238320] and runtime['texture_bytes']==106343076,'runtime profile differs')
 manifest=json.loads((base.parent/'manifest.json').read_text());manifest.update(package_sha256=sha(staged/'scene.ngas'),runtime=runtime,charcoal_glazing=profile);(staged/'manifest.json').write_text(json.dumps(manifest,sort_keys=True,indent=2)+'\n');require(sha(base)==profile['base_sha256'],'input changed');staged.rename(out)
print(json.dumps({'package':str(out/'scene.ngas'),'sha256':manifest['package_sha256'],'runtime':runtime,'charcoal_glazing':profile},indent=2))
