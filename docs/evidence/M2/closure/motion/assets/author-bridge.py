"""Localized bridge-glazing lookdev; preserve accepted exhaust and all other authored surfaces."""
from pathlib import Path
import hashlib,importlib.util,itertools,json,struct,subprocess,sys,tempfile
sys.dont_write_bytecode=True
root=Path.cwd();base=root/'build/cooked-candidate/frigate-hull-focus-v1/scene.ngas';out=root/'build/cooked-candidate/frigate-hull-focus-bridge-v2';source=root/'planning/models/frigate/frigate-01.glb'
def require(value,message):
 if not value:raise ValueError(message)
def sha(path):
 with path.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
require(not out.exists() and not out.is_symlink(),'output exists')
require(sha(base)=='a1f70ef7b3e6b7e67341e36b5828a759e9ca3502859d3fc6dba7c9a02688929f','unexpected hull/exhaust base')
require(sha(source)=='e68cd409c479036556446ed1753b287c0e7a40260109b3cd442abe7f533941fe','unexpected original source')
with source.open('rb') as f:
 require(f.read(4)==b'glTF','not GLB');f.read(8);length,kind=struct.unpack('<2I',f.read(8));require(kind==0x4e4f534a and length<=8*1024*1024,'invalid JSON chunk');gltf=json.loads(f.read(length))
light_ids={i for i,x in enumerate(gltf['extensions']['KHR_lights_punctual']['lights']) if x.get('name','').startswith('F01_Light_bridge_forward_glazing.')}
require(len(light_ids)==6,'unexpected bridge light membership')
raw=base.read_bytes();parts={}
for i in range(13):
 kind,stride,count,zero,at,size=struct.unpack_from('<4I2Q',raw,64+i*32);require(kind==i+1 and zero==0 and stride*count==size,'invalid section');parts[kind]=raw[at:at+size]
original=dict(parts);window=[]
for i in range(len(parts[4])//128):
 at,n=struct.unpack_from('<2I',parts[4],i*128+100)
 if parts[12][at:at+n]==b'F01_Window':window.append(i)
require(len(window)==1,'ambiguous bridge material');material_id=window[0];x=[]
for at in range(0,len(parts[9]),72):
 primitive,node,*m=struct.unpack_from('<2I16f',parts[9],at)
 if struct.unpack_from('<I',parts[3],primitive*80+8)[0]!=material_id:continue
 bounds=struct.unpack_from('<6f',parts[3],primitive*80+16)
 for corner in itertools.product(*[(bounds[j],bounds[j+3]) for j in range(3)]):x.append(sum(m[j*4]*corner[j] for j in range(3))+m[12])
require(x and min(x)<-5<max(x),'unexpected glazing depth');interior_x=(min(x)+max(x))*.5+.575
material=bytearray(parts[4]);old_emission=struct.unpack_from('<f',material,material_id*128+28)[0];struct.pack_into('<f',material,material_id*128+28,.06);parts[4]=bytes(material)
lights=bytearray(parts[10]);changes=[]
for light in sorted(light_ids):
 old_intensity,old_range=struct.unpack_from('<2f',lights,light*32+16)
 require(old_intensity>6000 and old_range==9,'unexpected bridge lamp profile')
 struct.pack_into('<f',lights,light*32+20,.6)
 changes.append({'light':light,'old_intensity_cd':old_intensity,'new_intensity_cd':old_intensity,'old_range_m':old_range,'new_range_m':.6})
parts[10]=bytes(lights);instances=bytearray(parts[11]);changed_instances=[]
for at in range(0,len(instances),72):
 light,node=struct.unpack_from('<2I',instances,at)
 if light in light_ids:
  old_x,old_y=struct.unpack_from('<2f',instances,at+56);require(old_x<min(x),'expected original proxy outside front face');interior_y=old_y-.75;struct.pack_into('<2f',instances,at+56,interior_x,interior_y);changed_instances.append({'instance':at//72,'light':light,'old_xy':[old_x,old_y],'new_xy':[interior_x,interior_y]})
require(len(changed_instances)==6,'unexpected lamp instances');parts[11]=bytes(instances)
for key in (1,2,3,5,6,7,8,9,12):require(parts[key]==original[key],f'protected section changed: {key}')
require(parts[4][13*128:14*128]==original[4][13*128:14*128],'accepted jet material changed')
provenance=json.loads(parts[13]);profile={'name':'subtle-bridge-glazing-v2','base_sha256':sha(base),'source_sha256':sha(source),'author_sha256':sha(Path(__file__)),'material':material_id,'old_emissive_strength':old_emission,'new_emissive_strength':.06,'bridge_lights':changes,'bridge_light_instances':changed_instances,'glazing_x_bounds':[min(x),max(x)],'scope':'Only bridge glazing emission and its six proxy lights. Move proxies behind the raked glazing into the pressure compartment; constrain reach so they cannot wash exterior plating. Preserve original point powers/selection, cyan hue, geometry, all images and other materials/lights, global exposure/bloom and accepted exhaust. No new glass/transmission shader.'};provenance['bridge_glazing']=profile;parts[13]=json.dumps(provenance,sort_keys=True,separators=(',',':')).encode();require(len(parts[13])<=65536,'provenance limit')
spec=importlib.util.spec_from_file_location('ngen_assembler',root/'tools/assetc/cook.py');cook=importlib.util.module_from_spec(spec);spec.loader.exec_module(cook)
with tempfile.TemporaryDirectory(prefix='.bridge-',dir=out.parent) as temporary:
 staged=Path(temporary)/'package';staged.mkdir();(staged/'scene.ngas').write_bytes(cook.assemble(parts))
 result=subprocess.run([str(root/'build/inspector-release/asset_package_probe'),str(staged/'scene.ngas')],capture_output=True,text=True,timeout=60);require(result.returncode==0 and not result.stderr,result.stderr);runtime=json.loads(result.stdout)
 require(runtime['lod_triangles']==[574376,291044,238320] and runtime['texture_bytes']==106343076,'runtime profile differs')
 manifest=json.loads((base.parent/'manifest.json').read_text());manifest.update(package_sha256=sha(staged/'scene.ngas'),runtime=runtime,bridge_glazing=profile);(staged/'manifest.json').write_text(json.dumps(manifest,sort_keys=True,indent=2)+'\n')
 require(sha(base)==profile['base_sha256'],'input changed');staged.rename(out)
print(json.dumps({'package':str(out/'scene.ngas'),'sha256':manifest['package_sha256'],'runtime':runtime,'bridge_glazing':profile},indent=2))
