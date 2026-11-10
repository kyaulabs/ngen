"""One-off owner presentation derivation; preserve original and reuse the v1 assembler.
Trusted local build inputs, not an asset sandbox or a general material editor.
"""
from pathlib import Path
import hashlib,importlib.util,json,struct,subprocess,sys,tempfile,zlib
sys.dont_write_bytecode=True
root=Path.cwd();work=root/'build/m2-engine-color';recipe_path=Path(sys.argv[1]).resolve();recipe=json.loads(recipe_path.read_text())
base=root/'build/cooked-candidate/frigate-canonical-first/scene.ngas';source=root/'planning/models/frigate/frigate-01.glb';out=Path(sys.argv[2]).resolve()
def sha(p):
 with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
assert base.is_file() and not base.is_symlink() and base.stat().st_size<=256*1024**2
assert source.is_file() and not source.is_symlink() and source.stat().st_size<=256*1024**2
assert sha(base)==recipe['base_package_sha256'] and sha(source)==recipe['original_source_sha256']
assert not out.exists() and not out.is_symlink()
raw=base.read_bytes();magic,version,header,total,crc,count=struct.unpack_from('<8sIIQII',raw)
assert (magic,version,header,total,count)==(b'NGASSET\0',1,64,len(raw),13) and zlib.crc32(raw[64:])&0xffffffff==crc
parts={};catalog={}
for i in range(13):
 kind,stride,n,reserved,offset,size=struct.unpack_from('<4I2Q',raw,64+32*i)
 assert kind==i+1 and reserved==0 and stride*n==size and offset+size<=len(raw)
 parts[kind]=raw[offset:offset+size];catalog[kind]=(stride,n)
with source.open('rb') as f:
 assert struct.unpack('<III',f.read(12))==(0x46546c67,2,source.stat().st_size)
 size,kind=struct.unpack('<II',f.read(8));assert kind==0x4e4f534a and size<=8*1024**2
 gltf=json.loads(f.read(size))
mat=bytearray(parts[4]);names=parts[12];changed_materials=[]
for i in range(catalog[4][1]):
 at=i*128;off,n=struct.unpack_from('<II',mat,at+100);name=names[off:off+n].decode()
 if name not in recipe['materials']:continue
 color=recipe['materials'][name];assert all(0<=x<=1 for x in color['base_color']+color['emissive'])
 struct.pack_into('<4f',mat,at,*color['base_color']);struct.pack_into('<3f',mat,at+16,*color['emissive']);changed_materials.append(name)
assert set(changed_materials)==set(recipe['materials'])
lights=bytearray(parts[10]);definitions=gltf['extensions']['KHR_lights_punctual']['lights'];assert len(definitions)==catalog[10][1]==612
changed_lights=[];drive=core=0
for i,light in enumerate(definitions):
 at=i*32;old=struct.pack('<3f',*light.get('color',[1,1,1]));assert old==parts[10][at+4:at+16]
 name=light.get('name','')
 if '_luminous_collar' in name:
  color=recipe['drive_light_color'];group='luminous_collar';drive+=1
 elif '_inner_coil' in name:
  color=recipe['drive_light_color'];group='inner_coil';drive+=1
 elif '_core' in name:
  color=recipe['core_light_color'];group='core';core+=1
 else:continue
 struct.pack_into('<3f',lights,at+4,*color)
 setting=recipe['light_groups'][group];scale=setting['intensity_scale'];reach=setting['range'];assert 0<scale<=1 and 0<reach<=35
 intensity=struct.unpack_from('<f',lights,at+16)[0]
 struct.pack_into('<2f',lights,at+16,intensity*scale,reach);changed_lights.append(i)
assert (drive,core)==(recipe['drive_light_count'],recipe['core_light_count'])
parts[4]=bytes(mat);parts[10]=bytes(lights)
# A separate byte-level verification checks every allowed/unchanged region.
provenance=json.loads(parts[13]);provenance['presentation_variant']={**recipe,'recipe_sha256':sha(recipe_path),'derivation_sha256':sha(Path(__file__)),'reference_image_sha256':sha(Path('/home/kyau/downloads/6ymad35uskva1.jpg'))}
parts[13]=json.dumps(provenance,sort_keys=True,separators=(',',':')).encode();assert len(parts[13])<=65536
spec=importlib.util.spec_from_file_location('ngen_package_assembler',root/'tools/assetc/cook.py');assembler=importlib.util.module_from_spec(spec);spec.loader.exec_module(assembler)
assert tuple(catalog[i][0] for i in range(1,14))==assembler.STRIDES
package=assembler.assemble(parts)
with tempfile.TemporaryDirectory(prefix='.palette-',dir=out.parent) as temporary:
 staged=Path(temporary)/'package';staged.mkdir()
 with (staged/'scene.ngas').open('xb') as f:f.write(package)
 probe=root/'build/inspector-release/asset_package_probe';run=subprocess.run([str(probe),str(staged/'scene.ngas')],capture_output=True,text=True,timeout=60)
 assert run.returncode==0 and not run.stderr,run.stderr
 runtime=json.loads(run.stdout);assert runtime['package_valid']==1
 manifest=json.loads((base.parent/'manifest.json').read_text());manifest.update(package_sha256=sha(staged/'scene.ngas'),runtime=runtime,presentation_variant=provenance['presentation_variant'])
 with (staged/'manifest.json').open('x') as f:json.dump(manifest,f,indent=2,sort_keys=True);f.write('\n')
 assert sha(base)==recipe['base_package_sha256'] and sha(source)==recipe['original_source_sha256']
 staged.rename(out)
(work/(recipe_path.stem+'-derivation.json')).write_text(json.dumps({'output_sha256':manifest['package_sha256'],'changed_materials':changed_materials,'drive_lights':drive,'core_lights':core,'changed_light_indices':changed_lights,'runtime':runtime,'original_source_and_package_unchanged':True},indent=2)+'\n')
print(json.dumps({'package':str(out/'scene.ngas'),'sha256':manifest['package_sha256'],'runtime':runtime},indent=2))
