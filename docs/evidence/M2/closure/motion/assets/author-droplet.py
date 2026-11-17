"""Fit original procedural jet geometry to actual recessed emitter openings."""
from pathlib import Path
import hashlib,importlib.util,json,math,struct,subprocess,sys,tempfile
import numpy as np
sys.dont_write_bytecode=True
root=Path.cwd();base=root/'build/cooked-candidate/frigate-turbulent-jet-v2/scene.ngas';out=root/'build/cooked-candidate/frigate-droplet-jet-v2'
def require(value,message):
 if not value:raise ValueError(message)
def sha(p):
 with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
require(not out.exists() and not out.is_symlink(),'output exists')
require(sha(base)=='5e69d51b48539b17b00b15d9b8e71d1d705b625bbfceab7cb539390aa24e988b','unexpected styled-jet base')
raw=base.read_bytes();parts={}
for i in range(13):
 k,s,n,z,a,size=struct.unpack_from('<4I2Q',raw,64+i*32);require(k==i+1 and z==0 and s*n==size,'section mismatch');parts[k]=raw[a:a+size]
original=dict(parts);vertices=np.frombuffer(parts[1],dtype='<f4').reshape(-1,14);draws=[]
for at in range(0,len(parts[9]),72):
 p,node,*matrix=struct.unpack_from('<2I16f',parts[9],at);first,count,material,lods=struct.unpack_from('<4I',parts[3],p*80)
 model=np.array(matrix).reshape(4,4).T;world=vertices[first:first+count,:3]@model[:3,:3].T+model[:3,3]
 draws.append({'offset':at,'primitive':p,'node':node,'material':material,'world':world})
cores=[x for x in draws if x['material']==7];plumes=[x for x in draws if x['material']==13];require(len(cores)==len(plumes)==6,'unexpected emitters')
primitive=plumes[0]['primitive'];require(all(x['primitive']==primitive for x in plumes),'jet mesh is not shared')
first_vertex=struct.unpack_from('<I',parts[3],primitive*80)[0];first_index=struct.unpack_from('<I',parts[3],primitive*80+40)[0]
# The real model supplies the luminous base. Remove the old fake base disc and
# replace only the owned appended plume mesh; the original ship prefix stays exact.
profile=[(u,max(.006,(1-u)**1.3*(1+3*u))) for u in (0,.02,.05,.09,.14,.20,.27,.35,.44,.54,.64,.74,.83,.90,.96,1)]
new_vertices=[];new_indices=[]
for slice_index in range(8):
 angle=math.pi*slice_index/8;cy,sz=math.cos(angle),math.sin(angle);start=len(new_vertices)
 for u,radius in profile:
  for sign,v in ((-1,0),(1,1)):
   new_vertices.append(struct.pack('<14f',u,sign*radius*cy,sign*radius*sz,0,-sz,cy,u,v,u,v,1,0,0,1))
 for j in range(len(profile)-1):
  a=start+j*2;b=a+2;new_indices.extend((a,b,b+1,a,b+1,a+1))
require(len(new_vertices)==256 and len(new_indices)==720,'unexpected droplet topology')
parts[1]=parts[1][:first_vertex*56]+b''.join(new_vertices);parts[2]=parts[2][:first_index*4]+struct.pack('<720I',*new_indices)
maximum_radius=max(radius for u,radius in profile)
record=bytearray(80);struct.pack_into('<4I6f',record,0,first_vertex,len(new_vertices),13,1,0,-maximum_radius,-maximum_radius,1,maximum_radius,maximum_radius);struct.pack_into('<2If',record,40,first_index,len(new_indices),0)
primitives=bytearray(parts[3]);primitives[primitive*80:(primitive+1)*80]=record;parts[3]=bytes(primitives)
instances=bytearray(parts[9]);receipts=[];assigned=set()
for core in cores:
 node=core['node'];c=core['world'];center=(c.min(axis=0)+c.max(axis=0))*.5
 ring=next(x['world'] for x in draws if x['node']==node and x['material']==6);ring=ring[(ring[:,0]>149)&(ring[:,0]<150)];require(len(ring)>16,'inner coil not found')
 outer=np.max(np.abs(ring[:,1:]-center[1:]),axis=0);rho=np.sqrt(np.sum(((ring[:,1:]-center[1:])/outer)**2,axis=1));opening=outer*rho.min()
 emitter=max(c[:,0].max(),ring[:,0].max());start=float(emitter+.02)
 old=min(plumes,key=lambda x:np.linalg.norm(x['world'][:,1:].mean(axis=0)-center[1:]));require(old['offset'] not in assigned,'ambiguous engine assignment');assigned.add(old['offset'])
 bell=max(x['world'][:,0].max() for x in draws if x['node']==node and x['material'] in (2,3));end=float(bell+2*(old['world'][:,0].max()-bell));length=end-start
 ry,rz=(opening*.98).tolist();model=[length,0,0,0,0,ry,0,0,0,0,rz,0,start,float(center[1]),float(center[2]),1]
 struct.pack_into('<16f',instances,old['offset']+8,*model)
 receipts.append({'source_node':node,'plume_source_node':old['node'],'emitter_face_x':float(emitter),'plume_start_x':start,'bell_end_x':float(bell),'old_plume_end_x':float(old['world'][:,0].max()),'plume_end_x':end,'base_radii':[ry,rz],'inner_opening_radii':opening.tolist()})
parts[9]=bytes(instances)
for key in (4,5,6,7,8,10,11,12):require(parts[key]==original[key],f'changed frozen jet style/material section {key}')
require(parts[1].startswith(original[1][:first_vertex*56]) and parts[2].startswith(original[2][:first_index*4]),'ship geometry prefix changed')
require(parts[3][:primitive*80]==original[3][:primitive*80],'ship primitives changed')
provenance=json.loads(parts[13]);provenance['droplet_jet']={'name':'recessed-droplet-v2','base_sha256':sha(base),'author_sha256':sha(Path(__file__)),'numpy_version':np.__version__,'shared_vertices':256,'shared_triangles':240,'instances':6,'visible_jet_triangles':1440,'base_disc_removed':True,'profile':profile,'emitters':receipts,'scope':'Latest owner request: originate at recessed white-core/inner-blue-coil opening, narrow to it, extend twice the earlier exposed tail and taper nearly to a point. Preserve the improved white-core/blue-fringe texture and turbulent shader style; no original ship geometry, materials, images, light or reference pixels changed.'}
parts[13]=json.dumps(provenance,sort_keys=True,separators=(',',':')).encode();require(len(parts[13])<=65536,'provenance limit')
spec=importlib.util.spec_from_file_location('ngen_assembler',root/'tools/assetc/cook.py');cook=importlib.util.module_from_spec(spec);spec.loader.exec_module(cook)
with tempfile.TemporaryDirectory(prefix='.droplet-',dir=out.parent) as temporary:
 staged=Path(temporary)/'package';staged.mkdir();(staged/'scene.ngas').write_bytes(cook.assemble(parts))
 result=subprocess.run([str(root/'build/inspector-release/asset_package_probe'),str(staged/'scene.ngas')],capture_output=True,text=True,timeout=60);require(result.returncode==0 and not result.stderr,result.stderr);runtime=json.loads(result.stdout)
 require(runtime['lod_triangles']==[574376,291044,238320] and runtime['instances']==194,'geometry accounting differs')
 require(runtime['texture_bytes']==89565864,'frozen jet texture changed')
 manifest=json.loads((base.parent/'manifest.json').read_text());manifest.update(package_sha256=sha(staged/'scene.ngas'),runtime=runtime,droplet_jet=provenance['droplet_jet'],visible_ship_lod2_triangles=239520)
 (staged/'manifest.json').write_text(json.dumps(manifest,sort_keys=True,indent=2)+'\n');require(sha(base)==provenance['droplet_jet']['base_sha256'],'original changed');staged.rename(out)
print(json.dumps({'package':str(out/'scene.ngas'),'sha256':manifest['package_sha256'],'runtime':runtime,'droplet_jet':manifest['droplet_jet']},indent=2))
