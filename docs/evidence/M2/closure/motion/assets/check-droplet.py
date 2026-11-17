from pathlib import Path
import json,struct,sys
import numpy as np
base=Path('build/cooked-candidate/frigate-turbulent-jet-v2/scene.ngas');target=Path(sys.argv[1])
def load(path):
 raw=path.read_bytes();parts={}
 for i in range(13):
  k,s,n,z,a,size=struct.unpack_from('<4I2Q',raw,64+i*32);parts[k]=raw[a:a+size]
 return parts
def instances(parts):
 vertices=np.frombuffer(parts[1],dtype='<f4').reshape(-1,14)
 result=[]
 for at in range(0,len(parts[9]),72):
  p,node,*values=struct.unpack_from('<2I16f',parts[9],at);first,count,material,lods=struct.unpack_from('<4I',parts[3],p*80)
  model=np.array(values).reshape(4,4).T;world=vertices[first:first+count,:3]@model[:3,:3].T+model[:3,3]
  result.append({'primitive':p,'node':node,'material':material,'world':world})
 return result
original=load(base);actual=load(target);reference=instances(original);draws=instances(actual);cores=[x for x in reference if x['material']==7];report=[]
assert len(cores)==6
for core in cores:
 node=core['node'];c=core['world'];center=(c.min(axis=0)+c.max(axis=0))*.5
 rings=[x for x in reference if x['node']==node and x['material']==6];assert len(rings)==1
 ring=rings[0]['world'];ring=ring[(ring[:,0]>149)&(ring[:,0]<150)];assert len(ring)>16
 outer=np.max(np.abs(ring[:,1:]-center[1:]),axis=0)
 inner=np.sqrt(np.sum(((ring[:,1:]-center[1:])/outer)**2,axis=1)).min()*outer
 emitter=max(c[:,0].max(),ring[:,0].max())
 old=min((x for x in reference if x['material']==13),key=lambda x:np.linalg.norm(x['world'][:,1:].mean(axis=0)-center[1:]))
 plume=min((x for x in draws if x['material']==13),key=lambda x:np.linalg.norm(x['world'][:,1:].mean(axis=0)-center[1:]))
 world=plume['world'];start=world[:,0].min();end=world[:,0].max();root=world[np.isclose(world[:,0],start,atol=1e-4)];tip=world[np.isclose(world[:,0],end,atol=1e-4)]
 assert emitter<=start<=emitter+.10,'plume origin is not at the recessed inner emitter'
 root_radius=np.max(np.abs(root[:,1:]-center[1:]),axis=0)
 assert np.all(root_radius<=inner*1.01),'plume root exceeds the inner coil opening'
 assert np.all(root_radius>=np.max(np.abs(c[:,1:]-center[1:]),axis=0)),'plume chokes the white emitter'
 # Original bell geometry reaches x160; extend twice the earlier exposed tail.
 bell=max(x['world'][:,0].max() for x in reference if x['node']==node and x['material'] in (2,3))
 assert end>=bell+2*(old['world'][:,0].max()-bell)-.1,'plume still does not extend far enough behind the bell'
 max_radius=np.max(np.abs(world[:,1:]-center[1:]),axis=0);tip_radius=np.max(np.abs(tip[:,1:]-center[1:]),axis=0)
 assert np.all(tip_radius<=max_radius*.02),'droplet jet does not taper almost to a point'
 report.append({'source_node':node,'emitter_x':float(emitter),'plume_start_x':float(start),'plume_end_x':float(end),'root_radii':root_radius.tolist(),'inner_opening_radii':inner.tolist(),'tip_to_max_radius':(tip_radius/max_radius).tolist()})
plume_primitive=next(x['primitive'] for x in reference if x['material']==13);first=struct.unpack_from('<I',original[3],plume_primitive*80)[0];start=struct.unpack_from('<I',original[3],plume_primitive*80+40)[0]
assert actual[1][:first*56]==original[1][:first*56]
assert actual[2][:start*4]==original[2][:start*4]
assert actual[3][:plume_primitive*80]==original[3][:plume_primitive*80]
for key in (4,5,6,7,8,10,11,12):assert actual[key]==original[key],key
triangles=0
for draw in draws:
 p=draw['primitive']*80;lod=min(2,struct.unpack_from('<I',actual[3],p+12)[0]-1);triangles+=struct.unpack_from('<I',actual[3],p+40+lod*12+4)[0]//3
assert triangles+8000<=250000
print(json.dumps({'geometry_matches_recessed_emitters':True,'engines':report,'ship_triangles':triangles,'workload_triangles':triangles+8000,'ship_geometry_materials_images_lights_unchanged':True},indent=2))
