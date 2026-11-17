"""Author a small static plume asset through existing native sections/KTX tools.
No runtime shader/backend changes; original mesh/image/light records are preserved.
"""
from pathlib import Path
import hashlib,importlib.util,json,math,re,struct,subprocess,sys,tempfile
sys.dont_write_bytecode=True
root=Path.cwd();work=root/'build/m2-engine-color';base=root/'build/cooked-candidate/frigate-blue-white-localized/scene.ngas';out=root/'build/cooked-candidate/frigate-blue-white-plume-wide'
def sha(p):
 with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
assert not out.exists() and not out.is_symlink()
manifest=json.loads((base.parent/'manifest.json').read_text());assert sha(base)==manifest['package_sha256']
raw=base.read_bytes();parts={};counts={}
for i in range(13):
 kind,stride,count,zero,offset,size=struct.unpack_from('<4I2Q',raw,64+i*32);assert kind==i+1 and zero==0 and stride*count==size
 parts[kind]=raw[offset:offset+size];counts[kind]=count
original=dict(parts)
spec=importlib.util.spec_from_file_location('ngen_assembler',root/'tools/assetc/cook.py');assembler=importlib.util.module_from_spec(spec);spec.loader.exec_module(assembler)
cache=(root/'build/asset-release/CMakeCache.txt').read_text();ktx=Path(re.search(r'^NGEN_KTX_EXECUTABLE:STRING=(.+)$',cache,re.M)[1]);assert sha(ktx)==manifest['tool_sha256']['ktx']
# Independent original procedural sprite: soft transverse density and fading aft tail.
w,h=128,64;linear=[]
def smooth(a,b,x):
 t=max(0.,min(1.,(x-a)/(b-a)));return t*t*(3-2*t)
for y in range(h):
 row=[];v=2*y/(h-1)-1
 for x in range(w):
  u=x/(w-1);hot=math.exp(-((u-.08)/.0525)**2)*(1-smooth(.75,1,abs(v)))
  alpha=.25*smooth(0,.03,u)*(1-smooth(.45,1,u))*math.exp(-1.6*max(0,u-.08))*(1-smooth(.65,1,abs(v)))
  row.append((.015+.985*hot,.22+.78*hot,1.,alpha))
 linear.append(row)
levels=[]
while True:
 levels.append(linear)
 height=len(linear);width=len(linear[0])
 if width==height==1:break
 resized=[]
 for y in range(max(1,height//2)):
  row=[]
  for x in range(max(1,width//2)):
   samples=[linear[yy][xx] for yy in range(y*2,min(height,y*2+2)) for xx in range(x*2,min(width,x*2+2))]
   a=sum(p[3] for p in samples);color=[sum(p[c]*p[3] for p in samples)/a if a>0 else 0 for c in range(3)]
   row.append((*color,a/len(samples)))
  resized.append(row)
 linear=resized
assert len(levels)==8
with tempfile.TemporaryDirectory(prefix='.plume-',dir=out.parent) as temporary:
 scratch=Path(temporary);native=scratch/'native';native.mkdir();filenames=[];texture_bytes=0
 for i,level in enumerate(levels):
  data=bytearray()
  for row in level:
   for pixel in row:
    for channel in pixel[:3]:
     srgb=12.92*channel if channel<=.0031308 else 1.055*channel**(1/2.4)-.055
     data.append(round(max(0,min(1,srgb))*255))
    data.append(round(max(0,min(1,pixel[3]))*255))
  name=f'image-0-mip-{i}.rgba';(native/name).write_bytes(data);filenames.append(name);texture_bytes+=len(data)
 report={'images':[{'width':w,'height':h,'format':2,'role':4,'files':filenames}],'texture_bytes':texture_bytes}
 encoded=assembler.texture_sections(native,report,ktx,scratch)
 blob=parts[8]+b'\0'*(-len(parts[8])%8);blob_offset=len(blob)
 record=bytearray(encoded[7]);struct.pack_into('<Q',record,0,blob_offset)
 parts[7]+=record;parts[8]=blob+encoded[8]
 sampler_index=counts[6];texture_index=counts[5];image_index=counts[7];material_index=counts[4];primitive_index=counts[3]
 parts[6]+=struct.pack('<4I',9987,9729,33071,33071)
 parts[5]+=struct.pack('<2I',image_index,sampler_index)
 material=bytearray(128);struct.pack_into('<4f',material,0,0,0,0,1);struct.pack_into('<3f',material,16,1,1,1)
 struct.pack_into('<6f',material,28,3,1,1,1,1,.5);struct.pack_into('<2I',material,52,2,1)
 for i in range(5):struct.pack_into('<2I',material,60+i*8,texture_index if i in (0,4) else 0xffffffff,0)
 name=b'M2_BlueWhiteDrivePlume';struct.pack_into('<2I',material,100,len(parts[12]),len(name));parts[12]+=name;parts[4]+=material
 vertices=[];indices=[]
 def vertex(position,normal,uv,tangent):
  vertices.append(struct.pack('<14f',*position,*normal,*uv,*uv,*tangent))
 # Disc is drawn before axial cards; all use ordinary alpha blending/depth-readonly.
 segments=16;vertex((.065,0,0),(1,0,0),(.08,.5),(0,1,0,1))
 for i in range(segments):
  angle=math.tau*i/segments;vertex((.065,math.cos(angle),math.sin(angle)),(1,0,0),(.08,1),(0,1,0,1))
 for i in range(segments):indices.extend((0,1+i,1+(i+1)%segments))
 for slice_index in range(8):
  angle=math.pi*slice_index/8;cy=math.cos(angle);sz=math.sin(angle)
  positions=[(0,-cy,-sz),(1,-cy,-sz),(1,cy,sz),(0,cy,sz)];normal=(0,-sz,cy)
  first=len(vertices)
  for p,uv in zip(positions,[(0,0),(1,0),(1,1),(0,1)]):vertex(p,normal,uv,(1,0,0,1))
  indices.extend(first+i for i in (0,1,2,0,2,3))
 assert len(vertices)==49 and len(indices)==96
 primitive=bytearray(80);struct.pack_into('<4I6f',primitive,0,counts[1],len(vertices),material_index,1,0,-1,-1,1,1,1);struct.pack_into('<2If',primitive,40,counts[2],len(indices),0)
 parts[1]+=b''.join(vertices);parts[2]+=struct.pack('<96I',*indices);parts[3]+=primitive
 with (root/'planning/models/frigate/frigate-01.glb').open('rb') as f:
  f.read(12);size,kind=struct.unpack('<II',f.read(8));gltf=json.loads(f.read(size))
 light_names=[x.get('name','') for x in gltf['extensions']['KHR_lights_punctual']['lights']]
 core_instances=[]
 for i in range(counts[11]):
  light,node,*matrix=struct.unpack_from('<2I16f',parts[11],i*72)
  if '_core' in light_names[light]:core_instances.append((light_names[light],matrix))
 assert len(core_instances)==6
 node_base=manifest['source_nodes'];assert node_base+6<4096
 for i,(name,matrix) in enumerate(core_instances):
  radius=6.1 if 'Dorsal' in name or 'Ventral' in name else 7.2
  model=[radius*3.6,0,0,0, 0,radius*.8*.981468915939331,0,0, 0,0,radius*.8*.9505835771560669,0, 159.5,matrix[13],matrix[14],1]
  parts[9]+=struct.pack('<2I16f',primitive_index,node_base+i,*model)
 assert parts[10]==original[10] and parts[11]==original[11]
 provenance=json.loads(parts[13]);provenance['exhaust_plume']={'schema':1,'name':'wider-soft-blue-white-v4','base_localized_package_sha256':sha(base),'derivation_sha256':sha(Path(__file__)),'ktx_sha256':sha(ktx),'mesh':'eight low-alpha radial slices plus radial soft base disc per drive','shared_vertices':49,'shared_triangles':32,'instances':6,'additional_visible_triangles':192,'texture_extent':[w,h],'texture_bytes':texture_bytes,'new_point_lights':0,'tail_length_multiplier':2,'plume_radius_fraction_of_nominal_bell':.8,'white_base_world_offset_preserved':True,'source_geometry_prefix_unchanged':True,'scope':'Owner explicitly requested a short directional plume; original procedural texture, no reference-image pixels or new renderer/backend.'}
 parts[13]=json.dumps(provenance,sort_keys=True,separators=(',',':')).encode();assert len(parts[13])<=65536
 scene=assembler.assemble(parts);staged=scratch/'package';staged.mkdir()
 with (staged/'scene.ngas').open('xb') as f:f.write(scene)
 probe=root/'build/inspector-release/asset_package_probe';result=subprocess.run([str(probe),str(staged/'scene.ngas')],capture_output=True,text=True,timeout=60)
 assert result.returncode==0 and not result.stderr,result.stderr
 runtime=json.loads(result.stdout);assert runtime['package_valid']==1 and runtime['instances']==194 and runtime['materials']==14 and runtime['images']==5
 assert runtime['lod_triangles'][2]==238112 # Catalog counts the shared primitive once.
 visible=0
 for i in range(len(parts[9])//72):
  primitive=struct.unpack_from('<I',parts[9],i*72)[0];at=primitive*80;lod=min(2,struct.unpack_from('<I',parts[3],at+12)[0]-1)
  visible+=struct.unpack_from('<I',parts[3],at+40+lod*12+4)[0]//3
 assert visible+8000==246272 # Draw accounting includes all six plume instances.
 manifest['visible_ship_lod2_triangles']=visible
 manifest.update(package_sha256=sha(staged/'scene.ngas'),runtime=runtime,exhaust_plume=provenance['exhaust_plume'])
 with (staged/'manifest.json').open('x') as f:json.dump(manifest,f,indent=2,sort_keys=True);f.write('\n')
 assert sha(base)==provenance['exhaust_plume']['base_localized_package_sha256']
 staged.rename(out)
(work/'plume-wide-derivation.json').write_text(json.dumps({'package_sha256':manifest['package_sha256'],'runtime':runtime,'exhaust_plume':manifest['exhaust_plume']},indent=2)+'\n')
print(json.dumps({'package':str(out/'scene.ngas'),'sha256':manifest['package_sha256'],'runtime':runtime},indent=2))
