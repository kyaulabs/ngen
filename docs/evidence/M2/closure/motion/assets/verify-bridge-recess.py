"""Independent geometry/style scope checks for the six authorized bridge recesses."""
from pathlib import Path
import hashlib,json,struct,zlib
import numpy as np
base=Path('build/cooked-candidate/frigate-charcoal-glazing-v1/scene.ngas');target=Path('build/cooked-candidate/frigate-bridge-recess-v1/scene.ngas')
def read(path):
 raw=path.read_bytes();assert zlib.crc32(raw[64:])==struct.unpack_from('<I',raw,24)[0];p={}
 for i in range(13):
  k,s,n,z,at,size=struct.unpack_from('<4I2Q',raw,64+i*32);assert k==i+1 and z==0 and s*n==size;p[k]=raw[at:at+size]
 return p,hashlib.sha256(raw).hexdigest()
a,sa=read(base);b,sb=read(target)
for k in (4,5,6,7,8,10,12):assert a[k]==b[k],('frozen material/image/light section',k)
assert len(b[3])//80==len(a[3])//80+1 and len(b[9])==len(a[9])+72 and b[9].startswith(a[9])
va=np.frombuffer(a[1],dtype='<f4').reshape(-1,14);vb=np.frombuffer(b[1],dtype='<f4').reshape(-1,14);ia=np.frombuffer(a[2],dtype='<u4');ib=np.frombuffer(b[2],dtype='<u4')
def primitive(p,v,indices,i):
 first,count,mat,levels=struct.unpack_from('<4I',p[3],i*80);lods=[]
 for lod in range(levels):
  start,n,error=struct.unpack_from('<2If',p[3],i*80+40+lod*12);lods.append((indices[start:start+n],error))
 return v[first:first+count],mat,lods
for i in range(len(a[3])//80):
 x,m,old=primitive(a,va,ia,i);y,n,new=primitive(b,vb,ib,i);assert m==n
 if i not in (1,62):
  assert x.tobytes()==y.tobytes(),('unrelated attributes',i);assert len(old)==len(new)
  for left,right in zip(old,new):assert left[0].tobytes()==right[0].tobytes() and left[1]==right[1],('unrelated topology',i)
roof_before,mat,old=primitive(a,va,ia,1);roof_after,mat,new=primitive(b,vb,ib,1);assert roof_after[:len(roof_before)].tobytes()==roof_before.tobytes()
areas=[]
for lod,((left,error),(right,new_error)) in enumerate(zip(old,new)):
 assert error==new_error;old_tri=left.reshape(-1,3);new_tri=right.reshape(-1,3);position=roof_before[old_tri,:3].astype(float);lo=position.min(axis=1);hi=position.max(axis=1)
 outside=np.any(hi<[-6.8,10.5,-8.2],axis=1)|np.any(lo>[-3.,13.8,8.2],axis=1);unchanged=np.isin(np.ascontiguousarray(old_tri[outside]).view('V12').reshape(-1),np.ascontiguousarray(new_tri).view('V12').reshape(-1));assert unchanged.all(),('roof topology changed outside bridge',lod)
 added=new_tri[np.any(new_tri>=len(roof_before),axis=1)];p=roof_after[added,:3].astype(float);cross=np.linalg.norm(np.cross(p[:,1]-p[:,0],p[:,2]-p[:,0]),axis=1);assert np.all(cross>0),('quantized cut produced degenerate triangles',lod)
 def area(v,t):
  p=v[t,:3].astype(float);return float(np.linalg.norm(np.cross(p[:,1]-p[:,0],p[:,2]-p[:,0]),axis=1).sum()*.5)
 removed=area(roof_before,old_tri)-area(roof_after,new_tri);assert 5<removed<15,('unexpected removed surface area',lod,removed);areas.append(removed)
changed_lights=[]
for at in range(0,len(a[11]),72):
 old=a[11][at:at+72];new=b[11][at:at+72]
 if old!=new:
  light=struct.unpack_from('<I',old)[0];assert light in range(294,300) and old[:56]==new[:56] and old[68:]==new[68:];changed_lights.append(light)
assert sorted(changed_lights)==list(range(294,300))
visible=8000
for at in range(0,len(b[9]),72):
 p=struct.unpack_from('<I',b[9],at)[0];v,m,lods=primitive(b,vb,ib,p);visible+=len(lods[min(2,len(lods)-1)][0])//3
assert visible<=250000
manifest=json.loads((target.parent/'manifest.json').read_text());assert manifest['package_sha256']==sb
result={'base_sha256':sa,'package_sha256':sb,'accepted_hull_materials_maps_unchanged':True,'accepted_exhaust_local_geometry_materials_models_unchanged':True,'all_unrelated_primitive_attributes_and_three_lod_topology_unchanged':True,'roof_original_vertex_attributes_preserved':True,'roof_topology_outside_bridge_region_unchanged':True,'removed_roof_area_m2_per_lod':areas,'new_cut_triangles_nondegenerate_after_float32_quantization':True,'only_six_existing_bridge_proxies_repositioned':changed_lights,'visible_workload_triangles':visible,'runtime':manifest['runtime'],'scope':'Bounded six local openings, replacement recessed panes and one black interior/frame primitive; original instance prefix and all images/materials/light definitions unchanged.'}
Path('build/m2-engine-color/bridge-recess-v1-verification.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
