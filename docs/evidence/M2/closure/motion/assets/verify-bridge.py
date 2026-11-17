"""Independent byte-scope and unchanged forward-light-tier check; does not import the author."""
from pathlib import Path
import hashlib,itertools,json,math,struct,zlib
root=Path.cwd();base=root/'build/cooked-candidate/frigate-hull-focus-v1/scene.ngas';target=root/'build/cooked-candidate/frigate-hull-focus-bridge-v2/scene.ngas'
def digest(path):
 with path.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def sections(path):
 raw=path.read_bytes();assert raw[:8]==b'NGASSET\0' and zlib.crc32(raw[64:])==struct.unpack_from('<I',raw,24)[0]
 p={}
 for i in range(13):
  kind,stride,count,reserved,at,size=struct.unpack_from('<4I2Q',raw,64+i*32);assert kind==i+1 and reserved==0 and stride*count==size;p[kind]=raw[at:at+size]
 return p
a=sections(base);b=sections(target);bridge=set(range(294,300));window=9
for key in (1,2,3,5,6,7,8,9,12):assert a[key]==b[key],('protected section',key)
assert a[4][:window*128+28]==b[4][:window*128+28] and a[4][window*128+32:]==b[4][window*128+32:]
assert abs(struct.unpack_from('<f',b[4],window*128+28)[0]-.06)<1e-8
for light in range(len(a[10])//32):
 old=a[10][light*32:(light+1)*32];new=b[10][light*32:(light+1)*32]
 if light in bridge:assert old[:20]==new[:20] and old[24:]==new[24:] and abs(struct.unpack_from('<f',new,20)[0]-.6)<1e-7
 else:assert old==new,('unrelated light',light)
changed=[]
for instance in range(len(a[11])//72):
 old=a[11][instance*72:(instance+1)*72];new=b[11][instance*72:(instance+1)*72];light,node=struct.unpack_from('<2I',old)
 if light in bridge:
  assert old[:56]==new[:56] and old[64:]==new[64:];ox,oy=struct.unpack_from('<2f',old,56);nx,ny=struct.unpack_from('<2f',new,56)
  assert -.001<ny-(oy-.75)<.001 and nx>-4.88 and ny<12.09;changed.append(instance)
 else:assert old==new,('unrelated light instance',instance)
assert len(changed)==6
# Match the current public scene's bounded forward-tier inputs, independent of the author.
minimum=[math.inf]*3;maximum=[-math.inf]*3
for at in range(0,len(a[9]),72):
 primitive,node,*m=struct.unpack_from('<2I16f',a[9],at);bounds=struct.unpack_from('<6f',a[3],primitive*80+16)
 for corner in itertools.product(*[(bounds[j],bounds[j+3]) for j in range(3)]):
  for axis in range(3):
   value=sum(m[j*4+axis]*corner[j] for j in range(3))+m[12+axis];minimum[axis]=min(minimum[axis],value);maximum[axis]=max(maximum[axis],value)
center=[(x+y)/2 for x,y in zip(minimum,maximum)];radius=math.sqrt(sum(((y-x)/2)**2 for x,y in zip(minimum,maximum)))
def selected(p):
 candidates=[]
 for i,at in enumerate(range(0,len(p[11]),72)):
  light,node,*m=struct.unpack_from('<2I16f',p[11],at);intensity,reach=struct.unpack_from('<2f',p[10],light*32+16);distance2=sum((m[12+j]-center[j])**2 for j in range(3))
  if reach>0 and math.sqrt(distance2)>reach+radius:continue
  score=intensity/max(1.,distance2)
  if score>0:candidates.append((score,i,light))
 return [light for score,i,light in sorted(candidates,key=lambda x:(-x[0],x[1]))[:16]]
before=selected(a);after=selected(b);assert set(before)==set(after),('forward light set changed',before,after)
assert before[:6]==after[:6] and set(before[:6])=={0,10,20,30,40,50}
assert bridge.issubset(after)
report={'base_sha256':digest(base),'package_sha256':digest(target),'accepted_exhaust_material_geometry_images_unchanged':True,'unrelated_materials_lights_transforms_unchanged':True,'bridge_light_instances_repositioned':changed,'forward_light_definitions_before':before,'forward_light_definitions_after':after,'forward_light_selection_set_unchanged':True,'drive_light_prefix_order_unchanged':True,'entire_selection_order_unchanged':before==after,'scope':'Only F01_Window emission, six bridge-light ranges and their interior XY positions; provenance/CRC updated. No global render changes.'}
(root/'build/m2-engine-color/bridge-v2-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
