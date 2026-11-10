"""Public cooked geometry: six glazing sightlines must reach a recessed interior at all LODs."""
from pathlib import Path
import json,struct,sys
import numpy as np
base=Path('build/cooked-candidate/frigate-charcoal-glazing-v1/scene.ngas');target=Path(sys.argv[1])
def load(path):
 raw=path.read_bytes();p={}
 for i in range(13):
  kind,stride,count,reserved,at,size=struct.unpack_from('<4I2Q',raw,64+i*32);assert kind==i+1 and reserved==0 and stride*count==size;p[kind]=raw[at:at+size]
 return p
a=load(base);b=a if target==base else load(target)
def draws(p):
 v=np.frombuffer(p[1],dtype='<f4').reshape(-1,14)
 for at in range(0,len(p[9]),72):
  primitive,node,*matrix=struct.unpack_from('<2I16f',p[9],at);first,count,material,levels=struct.unpack_from('<4I',p[3],primitive*80);m=np.array(matrix).reshape(4,4).T
  yield primitive,material,levels,v[first:first+count,:3].astype(float)@m[:3,:3].T+m[:3,3]
glass=np.concatenate([world for primitive,material,levels,world in draws(a) if material==9]);zs=sorted(struct.unpack_from('<f',a[11],at+64)[0] for at in range(0,len(a[11]),72) if struct.unpack_from('<I',a[11],at)[0] in range(294,300));assert len(zs)==6
centers=[np.array([(glass[:,0].min()+glass[:,0].max())*.5,(glass[:,1].min()+glass[:,1].max())*.5,z]) for z in zs]
normal=np.array([-.51436,.85757,0]);normal/=np.linalg.norm(normal);direction=-normal
origins=np.array([c+normal*.5 for c in centers])
def triangles(p,lod):
 result=[];indices=np.frombuffer(p[2],dtype='<u4')
 for primitive,material,levels,world in draws(p):
  alpha=struct.unpack_from('<I',p[4],material*128+52)[0]
  if alpha==2:continue
  if np.any(world.max(axis=0)<[-12,8,-10]) or np.any(world.min(axis=0)>[4,18,10]):continue
  start,count=struct.unpack_from('<2I',p[3],primitive*80+40+min(lod,levels-1)*12);t=world[indices[start:start+count].reshape(-1,3)];lo=t.min(axis=1);hi=t.max(axis=1)
  keep=np.all(hi>[-12,8,-10],axis=1)&np.all(lo<[4,18,10],axis=1);result.append(t[keep])
 return np.concatenate(result)
def nearest(t,origin):
 edge1=t[:,1]-t[:,0];edge2=t[:,2]-t[:,0];h=np.cross(np.broadcast_to(direction,edge2.shape),edge2);det=np.einsum('ij,ij->i',edge1,h);valid=np.abs(det)>1e-10;inv=np.zeros_like(det);inv[valid]=1/det[valid];s=origin-t[:,0];u=inv*np.einsum('ij,ij->i',s,h);q=np.cross(s,edge1);v=inv*(q@direction);distance=inv*np.einsum('ij,ij->i',edge2,q);hit=valid&(u>=-1e-8)&(v>=-1e-8)&(u+v<=1+1e-8)&(distance>1e-6)
 return float(distance[hit].min()) if hit.any() else None
report={'source':str(base),'candidate':str(target),'ray_scope':'Opaque cooked geometry; transparent pane/jet ignored, no author helpers imported','lods':[]};failed=[]
for lod in range(3):
 old=triangles(a,lod);new=old if b is a else triangles(b,lod);rows=[]
 for i,origin in enumerate(origins):
  before=nearest(old,origin);after=nearest(new,origin);gain=None if before is None or after is None else after-before
  rows.append({'window':i,'origin':origin.tolist(),'direction':direction.tolist(),'old_opaque_distance':before,'new_opaque_distance':after,'visible_depth_gain':gain})
  if gain is None or not .55<=gain<=1.6:failed.append(f'LOD{lod} window{i}: glazing has no bounded visible interior recess')
 report['lods'].append({'lod':lod,'windows':rows})
print(json.dumps(report,indent=2));assert not failed,'; '.join(failed)
