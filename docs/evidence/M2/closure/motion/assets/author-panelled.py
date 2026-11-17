"""Private original panelled/coated-metal art; visual references are not sampled."""
from pathlib import Path
import hashlib,importlib.util,json,re,struct,subprocess,sys,tempfile
import numpy as np
sys.dont_write_bytecode=True
root=Path.cwd();work=root/'build/m2-engine-color';base=root/'build/cooked-candidate/frigate-droplet-jet-v2/scene.ngas';out=root/'build/cooked-candidate/frigate-hull-focus-v1'
def require(value,message):
 if not value:raise ValueError(message)
def sha(path):
 with path.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
require(not out.exists() and not out.is_symlink(),'output already exists')
require(sha(base)=='9b645de103ea92677c3cfe97cb8bd88ede8e7109f34c0f926888ba65c7522e63','unexpected base')
source=root/'planning/models/frigate/frigate-01.glb'
require(sha(source)=='e68cd409c479036556446ed1753b287c0e7a40260109b3cd442abe7f533941fe','unexpected source')
with source.open('rb') as f:
 f.read(12);size,kind=struct.unpack('<2I',f.read(8));require(kind==0x4e4f534a and size<=8*1024**2,'source JSON admission');gltf=json.loads(f.read(size))
raw=base.read_bytes();parts={};counts={}
for i in range(13):
 kind,stride,count,zero,at,size=struct.unpack_from('<4I2Q',raw,64+i*32);require(kind==i+1 and zero==0 and stride*count==size,'section mismatch');parts[kind]=raw[at:at+size];counts[kind]=count
original=dict(parts);n=1024;rng=np.random.default_rng(0x4e47454e)
axis=np.arange(n,dtype=np.float32)/n;x=axis[None,:];y=axis[:,None]
def smooth(a,b,value):
 t=np.clip((value-a)/(b-a),0,1);return t*t*(3-2*t)
def noise(cx,cy):
 grid=rng.random((cy,cx),dtype=np.float32);ax=axis*cx;ay=axis*cy;ix=ax.astype(np.int32);iy=ay.astype(np.int32);fx=(ax-ix).astype(np.float32);fy=(ay-iy).astype(np.float32);fx=fx*fx*(3-2*fx);fy=fy*fy*(3-2*fy)
 lo=grid[iy[:,None],ix[None,:]]*(1-fx)[None,:]+grid[iy[:,None],(ix+1)[None,:]%cx]*fx[None,:]
 hi=grid[(iy+1)[:,None]%cy,ix[None,:]]*(1-fx)[None,:]+grid[(iy+1)[:,None]%cy,(ix+1)[None,:]%cx]*fx[None,:]
 return lo*(1-fy)[:,None]+hi*fy[:,None]
# Six-metre world-UV tile: thin straight 3 x 2 metre armor/access seams,
# not raised, staggered bricks. Only selected hull cladding receives these maps.
rows,cols=3,2;row=np.floor(y*rows).astype(np.int32);v=np.mod(y*rows,1)
cell_x=x*cols;column=np.floor(cell_x).astype(np.int32)%cols;u=np.mod(cell_x,1)
edge=np.minimum(np.minimum(u,1-u)*3.0,np.minimum(v,1-v)*2.0)
seam=1-smooth(.003,.012,edge)
rim=(1-smooth(.012,.045,edge))*(1-seam)
per_panel=rng.uniform(.96,1.04,(rows,cols)).astype(np.float32)[row,column]
patch=noise(16,16);fine=noise(128,128);brush=noise(4,128)
# Small, periodic corner fasteners; shallow normal relief and ambient cavity.
dx=np.minimum(abs(u-.045),abs(u-.955))*3.0;dy=np.minimum(abs(v-.065),abs(v-.935))*2.0
radius=np.sqrt(dx*dx+dy*dy);fastener=1-smooth(.018,.030,radius)
fastener_rim=(1-smooth(.030,.046,radius))*(1-fastener)
# Controlled original scoring and edge chips, not reference logos or decals.
scratches=np.zeros((n,n),dtype=np.float32)
for i in range(36):
 cx,cy=rng.random(2);length=rng.uniform(.014,.09);width=rng.uniform(.0006,.0018);slope=rng.uniform(-.15,.15)
 sx=(x-cx+.5)%1-.5;sy=(y-cy+.5)%1-.5
 line=np.maximum(0,1-(sx/length)**2)**2*np.exp(-((sy-slope*sx)/width)**2)
 np.maximum(scratches,line.astype(np.float32),out=scratches)
chips=rim*smooth(.47,.70,noise(64,64))
wear=np.clip(chips*.8+scratches*.5+fastener_rim*.35,0,1)
# Paint is dielectric; exposed edges/fasteners reveal metal. Metallic-gray
# conductor everywhere was the wrong material model for the supplied references.
rough=np.clip(.48+.15*(patch-.5)+.07*(fine-.5)-.17*wear+.1*seam,.28,.68)
metal=np.clip(.08+.88*wear+.55*fastener,.04,.98)
ao=np.clip(1-.25*seam-.20*fastener-.04*(1-smooth(.015,.06,edge)),.55,1)
height=.0004*(1-seam)-.0003*fastener+.00018*(brush-.5)+.00012*(fine-.5)
du=(np.roll(height,-1,axis=1)-np.roll(height,1,axis=1))*(n/12);dv=(np.roll(height,-1,axis=0)-np.roll(height,1,axis=0))*(n/12)
normal=np.stack((-du,-dv,np.ones_like(du)),axis=2);normal/=np.linalg.norm(normal,axis=2,keepdims=True)
# Neutralize the source's blue tint with warm-neutral texels, preserving the
# authored gray/red factors. Panel tone, seams and edge wear carry the detail.
coating=per_panel*(1+.09*(patch-.5)+.025*(fine-.5))
shade=(1-.22*seam)*(1-.15*fastener)
gray=np.clip(coating*shade,0,1.3)
graphite=np.clip(gray[:,:,None]*np.array([.83,.79,.74],np.float32),0,1)
graphite=graphite*(1-wear[:,:,None]*.45)+np.array([.88,.86,.82],np.float32)*wear[:,:,None]*.45
orm=np.stack((ao,rough,metal),axis=2);images=[graphite,orm,normal*.5+.5]
spec=importlib.util.spec_from_file_location('ngen_assembler',root/'tools/assetc/cook.py');cook=importlib.util.module_from_spec(spec);spec.loader.exec_module(cook)
cache=(root/'build/asset-release/CMakeCache.txt').read_text();ktx=Path(re.search(r'^NGEN_KTX_EXECUTABLE:STRING=(.+)$',cache,re.M)[1]);require(sha(ktx)=='415cc81715d1b5e4661b5077ecfde9ba207c7fbd997f0b78a44c57dfe6ceca97','unexpected KTX')
metrics=[]
with tempfile.TemporaryDirectory(prefix='.panelled-',dir=out.parent) as temporary:
 scratch=Path(temporary);native=scratch/'native';native.mkdir();reports=[];texels=0
 for index,image in enumerate(images):
  srgb=index==0;normal_map=index==2;data=image.astype(np.float32)
  if srgb:data=np.where(data<=.04045,data/12.92,((data+.055)/1.055)**2.4)
  names=[];level=0
  while True:
   encoded=data
   if srgb:encoded=np.where(data<=.0031308,data*12.92,1.055*np.maximum(data,0)**(1/2.4)-.055)
   pixels=np.empty((*data.shape[:2],4),dtype=np.uint8);pixels[:,:,:3]=np.rint(np.clip(encoded,0,1)*255).astype(np.uint8);pixels[:,:,3]=255
   name=f'image-{index}-mip-{level}.rgba';(native/name).write_bytes(pixels.tobytes());names.append(name);texels+=pixels.nbytes
   if level==0:metrics.append({'image':index,'channels':[{'p10':float(np.percentile(pixels[:,:,c],10)),'p50':float(np.percentile(pixels[:,:,c],50)),'p90':float(np.percentile(pixels[:,:,c],90)),'sd':float(np.std(pixels[:,:,c]))} for c in range(3)]})
   if data.shape[0]==1:break
   data=data.reshape(data.shape[0]//2,2,data.shape[1]//2,2,3).mean(axis=(1,3))
   if normal_map:
    vector=data*2-1;vector/=np.maximum(np.linalg.norm(vector,axis=2,keepdims=True),1e-10);data=vector*.5+.5
   level+=1
  reports.append({'width':n,'height':n,'format':2 if srgb else 1,'role':1 if srgb else 3 if normal_map else 2,'files':names})
 encoded=cook.texture_sections(native,{'images':reports,'texture_bytes':texels},ktx,scratch)
 # Preserve every original image/material, including accepted dark engine housings.
 blob=parts[8]+b'\0'*(-len(parts[8])%8);extra_records=bytearray(encoded[7])
 for i in range(3):
  offset=struct.unpack_from('<Q',extra_records,i*32)[0];struct.pack_into('<Q',extra_records,i*32,offset+len(blob))
 parts[7]+=extra_records;parts[8]=blob+encoded[8]
 base_texture=struct.unpack_from('<I',parts[4],60)[0];sampler=struct.unpack_from('<I',parts[5],base_texture*8+4)[0]
 for i in range(3):parts[5]+=struct.pack('<2I',counts[7]+i,sampler)
 materials=bytearray(parts[4]);affected=[]
 for i,name in enumerate(('M2_HullPlatingLight','M2_HullPlating','M2_HullPlatingDark')):
  material=bytearray(original[4][i*128:(i+1)*128]);struct.pack_into('<f',material,40,.75)
  for slot,texture in ((0,counts[5]),(1,counts[5]+1),(2,counts[5]+2),(3,counts[5]+1)):
   struct.pack_into('<2I',material,60+slot*8,texture,0)
  encoded_name=name.encode();struct.pack_into('<2I',material,100,len(parts[12]),len(encoded_name));parts[12]+=encoded_name;materials+=material;affected.append(name)
 parts[4]=bytes(materials)
 # Exported drive meshes join housing and cladding. Partition draw metadata by
 # source triangle, not geometry generation or a runtime asset-name special case.
 vertices=np.frombuffer(original[1],dtype='<f4').reshape(-1,14)
 primitives=bytearray(parts[3]);instances=bytearray(parts[9]);indices=bytearray(parts[2]);assignments=[]
 for instance_index in range(counts[9]):
  instance=original[9][instance_index*72:(instance_index+1)*72]
  primitive,node,*model=struct.unpack('<2I16f',instance);at=primitive*80
  first,vertex_count,material,lods=struct.unpack_from('<4I',original[3],at)
  if material not in (0,1,2) or node>=len(gltf['nodes']):continue
  name=gltf['nodes'][node].get('name','');whole=name=='F01_EXPORT_Armor';drive=name.startswith('F01_EXPORT_Drive_')
  if not (whole or drive):continue
  levels=[]
  for lod in range(lods):
   start,count,error=struct.unpack_from('<IIf',original[3],at+40+lod*12)
   tri=np.frombuffer(original[2],dtype='<u4',count=count,offset=start*4).reshape(-1,3)
   center=vertices[tri+first,:3].astype(np.float64).mean(axis=1)
   world_x=center[:,0]*model[0]+center[:,1]*model[4]+center[:,2]*model[8]+model[12]
   mask=np.ones(len(tri),dtype=bool) if whole else world_x<142.0
   levels.append((tri,mask,error))
  if not any(mask.any() for tri,mask,error in levels):continue
  new_material=counts[4]+material
  if all(mask.all() for tri,mask,error in levels):
   struct.pack_into('<I',primitives,at+8,new_material);assignments.append({'instance':instance_index,'source_node':node,'source_name':name,'whole_primitive':True});continue
  # Keep tiny LOD-disappearing features authored rather than inventing triangles.
  if not all(mask.any() and (~mask).any() for tri,mask,error in levels):continue
  plated=bytearray(original[3][at:at+80]);struct.pack_into('<I',plated,8,new_material)
  for lod,(tri,mask,error) in enumerate(levels):
   for record,record_at,subset in ((primitives,at,tri[~mask]),(plated,0,tri[mask])):
    start=len(indices)//4;indices+=subset.astype('<u4').tobytes();struct.pack_into('<IIf',record,record_at+40+lod*12,start,subset.size,error)
  plated_index=len(primitives)//80;primitives+=plated;added=bytearray(instance);struct.pack_into('<I',added,0,plated_index);instances+=added
  assignments.append({'instance':instance_index,'source_node':node,'source_name':name,'whole_primitive':False,'plated_primitive':plated_index,'preserved_aft_cutoff_x':142})
 parts[2]=bytes(indices);parts[3]=bytes(primitives);parts[9]=bytes(instances)
 for key in (1,6,10,11):require(parts[key]==original[key],f'changed protected section {key}')
 for key in (2,4,5,7,8,9,12):require(parts[key].startswith(original[key]),f'changed protected prefix {key}')
 require(assignments and counts[7]==6,'unexpected hull inventory')
 provenance=json.loads(parts[13]);provenance['hull_finish']={'name':'hull-focus-v1-with-accepted-droplet','base_sha256':sha(base),'source_sha256':sha(source),'author_sha256':sha(Path(__file__)),'numpy_version':np.__version__,'ktx_sha256':sha(ktx),'seed':'0x4e47454e','texture_extent':[n,n],'tile_metres':6,'panel_metres':[3,2],'appended_images':3,'original_images_and_materials_preserved':True,'material_normal_scale':.75,'affected_materials':affected,'assignments':assignments,'scope':'Selective hull and forward drive cladding; thin original armor seams/coating wear, not repeated bricks on machinery. No reference pixels/logos copied. Original vertex/index prefixes, models, palette factors, emission, lights, aft drive housings and plume remain unchanged; triangle partitions preserve visible topology.'}
 parts[13]=json.dumps(provenance,sort_keys=True,separators=(',',':')).encode();require(len(parts[13])<=65536,'provenance limit')
 staged=scratch/'package';staged.mkdir();(staged/'scene.ngas').write_bytes(cook.assemble(parts))
 result=subprocess.run([str(root/'build/inspector-release/asset_package_probe'),str(staged/'scene.ngas')],capture_output=True,text=True,timeout=60);require(result.returncode==0 and not result.stderr,result.stderr);runtime=json.loads(result.stdout)
 require(runtime['lod_triangles']==[574376,291044,238320] and runtime['texture_bytes']==106343076,'topology/texture budget differs')
 visible=0
 for at in range(0,len(parts[9]),72):
  primitive=struct.unpack_from('<I',parts[9],at)[0];pa=primitive*80;lod=min(2,struct.unpack_from('<I',parts[3],pa+12)[0]-1);visible+=struct.unpack_from('<I',parts[3],pa+40+lod*12+4)[0]//3
 require(visible==239520 and len(parts[9])//72+1<=256,'visible workload changed')
 manifest=json.loads((base.parent/'manifest.json').read_text());manifest.update(package_sha256=sha(staged/'scene.ngas'),runtime=runtime,hull_finish=provenance['hull_finish']);(staged/'manifest.json').write_text(json.dumps(manifest,sort_keys=True,indent=2)+'\n')
 require(sha(base)==provenance['hull_finish']['base_sha256'],'original changed');staged.rename(out)
(work/'hull-focus-v1-metrics.json').write_text(json.dumps(metrics,indent=2)+'\n');print(json.dumps({'package':str(out/'scene.ngas'),'sha256':manifest['package_sha256'],'runtime':runtime,'hull_finish':manifest['hull_finish']},indent=2))
