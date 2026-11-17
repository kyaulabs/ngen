from pathlib import Path
import hashlib,json,struct
import numpy as np
base=Path('build/cooked-candidate/frigate-droplet-jet-v2/scene.ngas');target=Path('build/cooked-candidate/frigate-hull-focus-v1/scene.ngas')
def load(p):
 raw=p.read_bytes();parts={}
 for i in range(13):
  kind,stride,count,zero,offset,size=struct.unpack_from('<4I2Q',raw,64+i*32);assert zero==0 and count*stride==size;parts[kind]=raw[offset:offset+size]
 return parts,hashlib.sha256(raw).hexdigest(),len(raw)
a,sa,na=load(base);b,sb,nb=load(target)
for k in (1,6,10,11):assert a[k]==b[k],k
for k in (2,4,5,7,8,9,12):assert b[k].startswith(a[k]),k
assert len(b[7])//32==9 and len(b[4])//128==17 and len(b[5])//8==20
assert b[3][188*80:189*80]==a[3][188*80:189*80], 'accepted jet primitive changed'
assert b[4][13*128:14*128]==a[4][13*128:14*128], 'accepted jet material changed'
# Preserve ordered triangle corners/winding, model bytes and source-node identity.
def geometry(parts,lod):
 groups={};indices=np.frombuffer(parts[2],dtype='<u4')
 for at in range(0,len(parts[9]),72):
  primitive,node=struct.unpack_from('<2I',parts[9],at);key=parts[9][at+4:at+72]
  pa=primitive*80;first,n,material,levels=struct.unpack_from('<4I',parts[3],pa)
  start,count=struct.unpack_from('<2I',parts[3],pa+40+min(lod,levels-1)*12)
  triangles=(indices[start:start+count].reshape(-1,3)+first).astype('<u4')
  groups.setdefault(key,[]).append(triangles)
 return {key:np.sort(np.concatenate(value).view('V12').reshape(-1)) for key,value in groups.items()}
counts=[]
for lod in range(3):
 ga,gb=geometry(a,lod),geometry(b,lod);assert set(ga)==set(gb)
 for key in ga:assert np.array_equal(ga[key],gb[key]),f'LOD{lod} geometry differs'
 counts.append(sum(len(x) for x in gb.values()))
assert counts[2]==239520
# Every remapped source draw is hull plating or forward drive cladding only.
manifest=json.loads((target.parent/'manifest.json').read_text());assert manifest['package_sha256']==sb
for item in manifest['hull_finish']['assignments']:
 assert item['source_name']=='F01_EXPORT_Armor' or item['source_name'].startswith('F01_EXPORT_Drive_')
 if not item['whole_primitive']:assert item['preserved_aft_cutoff_x']==142
assert manifest['runtime']['texture_bytes']==106343076
report={'base_sha256':sa,'derived_sha256':sb,'derived_bytes':nb,'positions_normals_uvs_tangents_unchanged':True,'original_indices_materials_textures_images_instances_names_preserved_as_prefixes':True,'all_three_lods_world_models_and_oriented_triangle_multisets_identical':True,'instanced_lod_triangles':counts,'visible_workload_triangles':counts[2]+8000,'draw_records':len(b[9])//72+1,'accepted_exhaust_data_unchanged':True,'runtime':manifest['runtime'],'scope':'Data-only independent reparse; no reference pixels, generated author script or returned executable executed.'}
Path('build/m2-engine-color/hull-focus-v1-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
