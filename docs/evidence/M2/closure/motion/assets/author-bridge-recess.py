"""Owner-authorized six local bridge openings and shallow recesses; frozen hull/exhaust materials."""
from pathlib import Path
import hashlib,importlib.util,json,struct,subprocess,sys,tempfile
import numpy as np
sys.dont_write_bytecode=True
root=Path.cwd();base=root/'build/cooked-candidate/frigate-charcoal-glazing-v1/scene.ngas';out=root/'build/cooked-candidate/frigate-bridge-recess-v1'
def require(value,message):
 if not value:raise ValueError(message)
def sha(path):
 with path.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
require(not out.exists() and not out.is_symlink(),'output exists')
require(sha(base)=='27de456e8f29b6392a50099dd65f69b2bd42a8ce2a73b3b3b0cc2c716811ee81','unexpected accepted hull base')
raw=base.read_bytes();parts={}
for i in range(13):
 k,s,n,z,at,size=struct.unpack_from('<4I2Q',raw,64+i*32);require(k==i+1 and z==0 and s*n==size,'section mismatch');parts[k]=raw[at:at+size]
original=dict(parts);vertices=np.frombuffer(parts[1],dtype='<f4').reshape(-1,14);indices=np.frombuffer(parts[2],dtype='<u4');source=[]
for i in range(len(parts[3])//80):
 record=parts[3][i*80:(i+1)*80];first,count,material,levels=struct.unpack_from('<4I',record);lods=[]
 for lod in range(levels):
  start,n,error=struct.unpack_from('<2If',record,40+lod*12);lods.append((indices[start:start+n].copy(),error))
 source.append({'vertices':vertices[first:first+count].copy(),'material':material,'lods':lods})
hull=1;glass=62
require(source[hull]['material']==15 and source[glass]['material']==9,'unexpected bridge primitives')
instances=[]
for at in range(0,len(parts[9]),72):
 p,node,*m=struct.unpack_from('<2I16f',parts[9],at);instances.append((p,node,np.array(m).reshape(4,4).T))
for p in (hull,glass):
 uses=[x for x in instances if x[0]==p];require(len(uses)==1 and np.array_equal(uses[0][2],np.eye(4)),'expected one identity bridge draw')
g=source[glass]['vertices'];centers_z=sorted(struct.unpack_from('<f',parts[11],at+64)[0] for at in range(0,len(parts[11]),72) if struct.unpack_from('<I',parts[11],at)[0] in range(294,300));require(len(centers_z)==6,'unexpected glazing count')
nominal=np.array([-.51436,.85757,0.]);nominal/=np.linalg.norm(nominal);direction=-nominal
roof=source[hull]['vertices'][source[hull]['lods'][0][0].reshape(-1,3),:3].astype(float)
e1=roof[:,1]-roof[:,0];e2=roof[:,2]-roof[:,0];h=np.cross(np.broadcast_to(direction,e2.shape),e2);det=np.einsum('ij,ij->i',e1,h);valid=np.abs(det)>1e-10;inv=np.zeros_like(det);inv[valid]=1/det[valid]
windows=[]
for z in centers_z:
 original_center=np.array([(g[:,0].min()+g[:,0].max())*.5,(g[:,1].min()+g[:,1].max())*.5,z]);origin=original_center+nominal*.5;s=origin-roof[:,0];u=inv*np.einsum('ij,ij->i',s,h);q=np.cross(s,e1);v=inv*(q@direction);distance=inv*np.einsum('ij,ij->i',e2,q);hit=valid&(u>=0)&(v>=0)&(u+v<=1)&(distance>0);require(hit.any(),'window does not project onto pressure hull');chosen=np.flatnonzero(hit)[np.argmin(distance[hit])]
 center=origin+direction*distance[chosen];normal=np.cross(e1[chosen],e2[chosen]);normal/=np.linalg.norm(normal);normal*=1 if normal[1]>0 else -1
 vertical=np.array([normal[1],-normal[0],0]);vertical/=np.linalg.norm(vertical);across=np.cross(vertical,normal);across/=np.linalg.norm(across)
 local=g[np.abs(g[:,2]-z)<1.,:3];hu=float((local[:,2].max()-local[:,2].min())*.5/across[2]+.04);hv=float((local[:,1].max()-local[:,1].min())*.5/vertical[1]+.04)
 windows.append({'center':center,'normal':normal,'across':across,'vertical':vertical,'half':np.array([hu,hv,.25])})
# Subtract each bounded rectangular prism from only the intersecting roof triangles.
def split(poly,axis,center,limit,sign):
 inside=[];outside=[]
 for a,b in zip(poly,np.roll(poly,-1,axis=0)):
  da=sign*np.dot(a[:3]-center,axis)-limit;db=sign*np.dot(b[:3]-center,axis)-limit;ia=da<=0;ib=db<=0
  (inside if ia else outside).append(a)
  if ia!=ib:
   point=a+(b-a)*(da/(da-db));inside.append(point);outside.append(point)
 return np.array(inside),np.array(outside)
def area(poly):
 return sum(np.linalg.norm(np.cross(poly[j,:3]-poly[0,:3],poly[j+1,:3]-poly[0,:3])) for j in range(1,len(poly)-1)) if len(poly)>=3 else 0.
def subtract(poly,w):
 axes=np.stack((w['across'],w['vertical'],w['normal']));coords=(poly[:,:3]-w['center'])@axes.T
 if np.any(coords.min(axis=0)>w['half']) or np.any(coords.max(axis=0)<-w['half']):return [poly]
 remaining=poly;outside=[]
 for axis,limit in zip(axes,w['half']):
  for sign in (-1,1):
   remaining,away=split(remaining,axis,w['center'],limit,sign)
   if len(away)>=3 and area(away)>1e-10:outside.append(away)
   if len(remaining)<3:return [poly]
 if area(remaining)<=1e-10:return [poly]
 return outside
hull_vertices=list(source[hull]['vertices']);cache={v.tobytes():i for i,v in enumerate(hull_vertices)}
def vertex_id(value):
 value=np.asarray(value,dtype='<f4');key=value.tobytes()
 if key in cache:return cache[key]
 n=value[3:6].astype(float);n/=np.linalg.norm(n);t=value[10:13].astype(float);t-=n*np.dot(n,t);t/=np.linalg.norm(t);value[3:6]=n;value[10:13]=t;value[13]=1 if value[13]>=0 else -1;key=value.tobytes()
 if key not in cache:cache[key]=len(hull_vertices);hull_vertices.append(value)
 return cache[key]
new_lods=[];cut_counts=[]
for old,error in source[hull]['lods']:
 triangles=old.reshape(-1,3);positions=source[hull]['vertices'][triangles,:3].astype(float);candidates=np.zeros(len(triangles),dtype=bool)
 for w in windows:
  axes=np.stack((w['across'],w['vertical'],w['normal']));c=(positions-w['center'])@axes.T;candidates|=np.all(c.min(axis=1)<=w['half'],axis=1)&np.all(c.max(axis=1)>=-w['half'],axis=1)
 chunks=[];cursor=0;changed=0
 for i in np.flatnonzero(candidates):
  chunks.append(old[cursor:i*3]);polygons=[source[hull]['vertices'][triangles[i]].astype(float)]
  for w in windows:polygons=[piece for poly in polygons for piece in subtract(poly,w)]
  if len(polygons)==1 and polygons[0].shape==(3,14) and np.array_equal(polygons[0],source[hull]['vertices'][triangles[i]]):chunks.append(old[i*3:i*3+3])
  else:
   changed+=1;added=[]
   for poly in polygons:
    for j in range(1,len(poly)-1):
     tri=poly[[0,j,j+1]]
     if area(tri)>1e-10:added.extend(vertex_id(v) for v in tri)
   chunks.append(np.asarray(added,dtype='<u4'))
  cursor=i*3+3
 chunks.append(old[cursor:]);new_lods.append((np.concatenate(chunks),error));cut_counts.append(changed)
source[hull]['vertices']=np.array(hull_vertices,dtype='<f4');source[hull]['lods']=new_lods
# Simple single-surface panes and opaque recess walls/backing: no full cockpit.
panes=[];pane_indices=[];room=[];room_indices=[]
def quad(storage,index_storage,points,hint):
 points=np.array(points);n=np.cross(points[1]-points[0],points[2]-points[0]);n/=np.linalg.norm(n)
 if np.dot(n,hint)<0:points=points[[0,3,2,1]];n=-n
 tangent=points[1]-points[0];tangent/=np.linalg.norm(tangent);first=len(storage)
 for p,uv in zip(points,((0,0),(1,0),(1,1),(0,1))):storage.append(np.array([*p,*n,*uv,*uv,*tangent,1],dtype='<f4'))
 index_storage.extend((first,first+1,first+2,first,first+2,first+3))
for w in windows:
 c=w['center'];n=w['normal'];u=w['across'];v=w['vertical'];hu,hv,_=w['half'];inner=np.array([hu-.06,hv-.06]);outer=np.array([hu+.04,hv+.04]);signs=((-1,-1),(1,-1),(1,1),(-1,1))
 point=lambda a,b,d:c+u*a+v*b+n*d
 pane=[point(a*inner[0],b*inner[1],-.065) for a,b in signs];quad(panes,pane_indices,pane,n)
 top=[point(a*outer[0],b*outer[1],.035) for a,b in signs];lip=[point(a*inner[0],b*inner[1],-.07) for a,b in signs];back=[point(a*inner[0],b*inner[1],-.9) for a,b in signs]
 quad(room,room_indices,back,n)
 for j in range(4):
  k=(j+1)%4;hint=-(signs[j][0]+signs[k][0])*u-(signs[j][1]+signs[k][1])*v
  quad(room,room_indices,[top[j],top[k],lip[k],lip[j]],n+hint)
  quad(room,room_indices,[lip[j],lip[k],back[k],back[j]],hint)
source[glass]={'vertices':np.array(panes,dtype='<f4'),'material':9,'lods':[(np.array(pane_indices,dtype='<u4'),0.)]}
room_primitive=len(source);source.append({'vertices':np.array(room,dtype='<f4'),'material':4,'lods':[(np.array(room_indices,dtype='<u4'),0.)]})
# Repack with unchanged local attributes/index order for every unrelated primitive.
vertex_parts=[];index_parts=[];records=[];vc=ic=0
for p in source:
 v=p['vertices'];record=bytearray(80);struct.pack_into('<4I6f',record,0,vc,len(v),p['material'],len(p['lods']),*v[:,:3].min(axis=0),*v[:,:3].max(axis=0))
 for lod,(idx,error) in enumerate(p['lods']):struct.pack_into('<2If',record,40+lod*12,ic,len(idx),error);index_parts.append(idx.tobytes());ic+=len(idx)
 vertex_parts.append(v.tobytes());records.append(bytes(record));vc+=len(v)
parts[1]=b''.join(vertex_parts);parts[2]=b''.join(index_parts);parts[3]=b''.join(records);parts[9]+=struct.pack('<2I16f',room_primitive,492,*np.eye(4).T.reshape(-1))
lights=bytearray(parts[11]);moved=[]
for at in range(0,len(lights),72):
 light,node=struct.unpack_from('<2I',lights,at)
 if light in range(294,300):
  old_z=struct.unpack_from('<f',lights,at+64)[0];w=min(windows,key=lambda w:abs(w['center'][2]-old_z));position=w['center']-w['normal']*3;struct.pack_into('<3f',lights,at+56,*position);moved.append({'light':light,'position':position.tolist()})
parts[11]=bytes(lights)
for k in (4,5,6,7,8,10,12):require(parts[k]==original[k],f'frozen finish/style section changed {k}')
require(parts[9].startswith(original[9]),'original instance/model changed')
visible=sum(len(source[p]['lods'][min(2,len(source[p]['lods'])-1)][0])//3 for p,node,m in instances)+len(room_indices)//3+8000;require(visible<=250000,'triangle budget exceeded')
receipts=[{k:(value.tolist() if isinstance(value,np.ndarray) else value) for k,value in w.items()} for w in windows]
profile={'name':'six-recessed-bridge-windows-v1','base_sha256':sha(base),'author_sha256':sha(Path(__file__)),'numpy_version':np.__version__,'modified_hull_primitive':hull,'replaced_glazing_primitive':glass,'new_interior_primitive':room_primitive,'cut_source_triangles_per_lod':cut_counts,'windows':receipts,'recess_depth_m':.9,'interior_proxy_positions':moved,'visible_workload_triangles':visible,'scope':'Owner authorized localized geometry openings and shallow dark backing behind the six bridge windows. Cut only bounded roof patches; replace raised solid window boxes with recessed single glass panes following the actual raked/chamfered surface. Reuse original black material for frames/interiors. Preserve accepted charcoal coating/materials/maps, all unrelated geometry and accepted exhaust. Move existing bridge-light proxies deeper inside so their unchanged short range cannot wash visible recesses. No full cockpit, new light definitions, dependencies, shader or pass.'}
provenance=json.loads(parts[13]);provenance['bridge_recess']=profile;parts[13]=json.dumps(provenance,sort_keys=True,separators=(',',':')).encode();require(len(parts[13])<=65536,'provenance limit')
spec=importlib.util.spec_from_file_location('ngen_assembler',root/'tools/assetc/cook.py');cook=importlib.util.module_from_spec(spec);spec.loader.exec_module(cook)
with tempfile.TemporaryDirectory(prefix='.bridge-recess-',dir=out.parent) as temporary:
 staged=Path(temporary)/'package';staged.mkdir();(staged/'scene.ngas').write_bytes(cook.assemble(parts));result=subprocess.run([str(root/'build/inspector-release/asset_package_probe'),str(staged/'scene.ngas')],capture_output=True,text=True,timeout=60);require(result.returncode==0 and not result.stderr,result.stderr);runtime=json.loads(result.stdout)
 require(runtime['instances']==210 and runtime['texture_bytes']==106343076,'runtime profile differs');manifest=json.loads((base.parent/'manifest.json').read_text());manifest.update(package_sha256=sha(staged/'scene.ngas'),runtime=runtime,bridge_recess=profile);(staged/'manifest.json').write_text(json.dumps(manifest,sort_keys=True,indent=2)+'\n');require(sha(base)==profile['base_sha256'],'input changed');staged.rename(out)
print(json.dumps({'package':str(out/'scene.ngas'),'sha256':manifest['package_sha256'],'runtime':runtime,'bridge_recess':profile},indent=2))
