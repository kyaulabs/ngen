"""Independent material-only scope and native-view comparison for the proposed finish."""
from pathlib import Path
import hashlib,json,struct
import numpy as np
from PIL import Image
root=Path('build/m2-engine-color');base=Path('build/cooked-candidate/frigate-hull-focus-bridge-v2/scene.ngas');target=Path('build/cooked-candidate/frigate-charcoal-glazing-v1/scene.ngas')
def parts(path):
 raw=path.read_bytes();p={}
 for i in range(13):
  kind,stride,count,reserved,at,size=struct.unpack_from('<4I2Q',raw,64+i*32);assert kind==i+1 and reserved==0 and stride*count==size;p[kind]=raw[at:at+size]
 return p
a=parts(base);b=parts(target)
for k in range(1,13):
 if k!=4:assert a[k]==b[k],('protected section',k)
changed=[]
for i in range(len(a[4])//128):
 old=a[4][i*128:(i+1)*128];new=b[4][i*128:(i+1)*128]
 if old!=new:
  changed.append(i)
  if i in (14,15,16):assert old[12:]==new[12:]
  else:
   assert i==9 and old[16:28]==new[16:28] and old[40:52]==new[40:52] and old[56:]==new[56:]
assert changed==[9,14,15,16]
old=np.array(Image.open(root/'bridge-v2-quarter.bmp').convert('RGB'),dtype=np.int16);new=np.array(Image.open(root/'charcoal-glazing-v1-quarter.bmp').convert('RGB'),dtype=np.int16)
regions={}
for name,box in {'sunlit_coating':[1350,250,1450,290],'citadel_hull':[850,470,1070,500]}.items():
 x0,y0,x1,y1=box;a_rgb=np.median(old[y0:y1,x0:x1].reshape(-1,3),axis=0);b_rgb=np.median(new[y0:y1,x0:x1].reshape(-1,3),axis=0);regions[name]={'roi_xyxy':box,'before_median_rgb':a_rgb.tolist(),'after_median_rgb':b_rgb.tolist()}
report={'package_sha256':hashlib.sha256(target.read_bytes()).hexdigest(),'changed_materials':changed,'all_geometry_images_samplers_models_lights_unchanged':True,'accepted_exhaust_material_unchanged':True,'native_regions':regions,'glazing':'existing alpha-blend dielectric PBR; no emission, no cockpit interior/refraction claim','acceptance':'not yet shown or accepted'}
(root/'charcoal-glazing-v1-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
