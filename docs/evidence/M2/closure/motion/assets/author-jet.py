"""Original long white-hot jet sprite; no reference image pixels are used."""
from pathlib import Path
import hashlib,importlib.util,json,math,re,struct,subprocess,sys,tempfile
sys.dont_write_bytecode=True
root=Path.cwd();work=root/'build/m2-engine-color';base=root/'build/cooked-candidate/frigate-blue-white-plume-wide/scene.ngas';out=root/'build/cooked-candidate/frigate-turbulent-jet-v2'
def require(value,message):
 if not value:raise ValueError(message)
def sha(path):
 with path.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
require(not out.exists() and not out.is_symlink(),'output exists')
require(sha(base)=='067407fb5fa2527771212cc7280a345b02ad3dd2a823ae8ba81b3553689ec52b','unexpected base')
raw=base.read_bytes();parts={};counts={}
for i in range(13):
 kind,stride,count,zero,at,size=struct.unpack_from('<4I2Q',raw,64+i*32);require(kind==i+1 and zero==0 and stride*count==size,'invalid section');parts[kind]=raw[at:at+size];counts[kind]=count
original=dict(parts)
def smooth(a,b,x):
 t=max(0.,min(1.,(x-a)/(b-a)));return t*t*(3-2*t)
w,h=128,64;linear=[]
for y in range(h):
 row=[];v=2*y/(h-1)-1
 for x in range(w):
  u=x/(w-1);transition=smooth(.12,.45,u)
  radius=.75-.40*transition;radial=(1-smooth(radius,1,abs(v)))**(1+2*transition)
  hot=(1-smooth(.60,.95,u))*radial
  alpha=.25*smooth(0,.03,u)*(1-smooth(.45,1,u))*math.exp(-1.6*max(0,u-.08))*(1-smooth(.65,1,abs(v)))
  row.append((.015+.985*hot,.22+.78*hot,1.,alpha))
 linear.append(row)
levels=[]
while True:
 levels.append(linear);height=len(linear);width=len(linear[0])
 if width==height==1:break
 resized=[]
 for y in range(max(1,height//2)):
  row=[]
  for x in range(max(1,width//2)):
   samples=[linear[yy][xx] for yy in range(y*2,min(height,y*2+2)) for xx in range(x*2,min(width,x*2+2))];a=sum(p[3] for p in samples)
   row.append((*[sum(p[c]*p[3] for p in samples)/a if a>0 else 0 for c in range(3)],a/len(samples)))
  resized.append(row)
 linear=resized
spec=importlib.util.spec_from_file_location('ngen_assembler',root/'tools/assetc/cook.py');cook=importlib.util.module_from_spec(spec);spec.loader.exec_module(cook)
cache=(root/'build/asset-release/CMakeCache.txt').read_text();ktx=Path(re.search(r'^NGEN_KTX_EXECUTABLE:STRING=(.+)$',cache,re.M)[1]);require(sha(ktx)=='415cc81715d1b5e4661b5077ecfde9ba207c7fbd997f0b78a44c57dfe6ceca97','unexpected KTX')
with tempfile.TemporaryDirectory(prefix='.jet-',dir=out.parent) as temporary:
 scratch=Path(temporary);native=scratch/'native';native.mkdir();filenames=[];texture_bytes=0
 for i,level in enumerate(levels):
  data=bytearray()
  for row in level:
   for pixel in row:
    for value in pixel[:3]:
     srgb=12.92*value if value<=.0031308 else 1.055*value**(1/2.4)-.055;data.append(round(max(0,min(1,srgb))*255))
    data.append(round(max(0,min(1,pixel[3]))*255))
  name=f'image-0-mip-{i}.rgba';(native/name).write_bytes(data);filenames.append(name);texture_bytes+=len(data)
 encoded=cook.texture_sections(native,{'images':[{'width':w,'height':h,'format':2,'role':4,'files':filenames}],'texture_bytes':texture_bytes},ktx,scratch)
 blob=parts[8]+b'\0'*(-len(parts[8])%8);record=bytearray(encoded[7]);struct.pack_into('<Q',record,0,len(blob));parts[7]+=record;parts[8]=blob+encoded[8]
 material=bytearray(parts[4]);plume=13*128;old_texture=struct.unpack_from('<I',material,plume+60)[0];sampler=struct.unpack_from('<I',parts[5],old_texture*8+4)[0]
 texture=counts[5];parts[5]+=struct.pack('<2I',counts[7],sampler)
 for slot in (0,4):struct.pack_into('<I',material,plume+60+slot*8,texture)
 parts[4]=bytes(material)
 for key in (1,2,3,6,9,10,11,12):require(parts[key]==original[key],f'changed protected section {key}')
 for key in (5,7,8):require(parts[key].startswith(original[key]),f'changed protected prefix {key}')
 provenance=json.loads(parts[13]);provenance['turbulent_jet']={'name':'long-white-core-blue-fringe-v2','base_sha256':sha(base),'author_sha256':sha(Path(__file__)),'ktx_sha256':sha(ktx),'texture_extent':[w,h],'texture_bytes':texture_bytes,'white_core_full_until_u':.60,'white_core_fades_until_u':.95,'scope':'Owner explicitly chose a white-hot center along most of the jet, blue fringes and irregular fast streaks. Sprite authored mathematically; no reference pixels or logos. Source geometry, transforms, overall length/radius, original images, other materials, dark housings and light definitions are preserved.'}
 parts[13]=json.dumps(provenance,sort_keys=True,separators=(',',':')).encode();require(len(parts[13])<=65536,'provenance limit')
 staged=scratch/'package';staged.mkdir();(staged/'scene.ngas').write_bytes(cook.assemble(parts))
 result=subprocess.run([str(root/'build/inspector-release/asset_package_probe'),str(staged/'scene.ngas')],capture_output=True,text=True,timeout=60);require(result.returncode==0 and not result.stderr,result.stderr);runtime=json.loads(result.stdout)
 require(runtime['lod_triangles']==[574168,290836,238112] and runtime['texture_bytes']==89565864,'runtime profile differs')
 manifest=json.loads((base.parent/'manifest.json').read_text());manifest.update(package_sha256=sha(staged/'scene.ngas'),runtime=runtime,turbulent_jet=provenance['turbulent_jet']);(staged/'manifest.json').write_text(json.dumps(manifest,sort_keys=True,indent=2)+'\n')
 require(sha(base)==provenance['turbulent_jet']['base_sha256'],'original changed');staged.rename(out)
print(json.dumps({'package':str(out/'scene.ngas'),'sha256':manifest['package_sha256'],'runtime':runtime,'turbulent_jet':manifest['turbulent_jet']},indent=2))
