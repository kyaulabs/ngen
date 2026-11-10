"""Native 1920x1080 fixed bridge-quarter view: hull plating above the six glazing strips."""
from pathlib import Path
import hashlib,json,sys
import numpy as np
from PIL import Image
path=Path(sys.argv[1]);image=np.array(Image.open(path).convert('RGB'),dtype=np.int16)
assert image.shape==(1080,1920,3),'not a native inspection capture'
# This is hull surface, not the glass strips below it or a navigation lamp.
patch=image[470:500,850:1070]
clipped=(patch[:,:,1]>=240)&(patch[:,:,2]>=245)
result={'image_sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'native_extent':[1920,1080],'region':'front citadel hull above the six narrow glazing strips','roi_xyxy':[850,470,1070,500],'pixels':int(clipped.size),'nearly_clipped_cyan_pixels':int(clipped.sum()),'nearly_clipped_cyan_fraction':float(clipped.mean()),'median_rgb':np.median(patch.reshape(-1,3),axis=0).tolist(),'maximum_allowed_fraction':0.01}
print(json.dumps(result,indent=2))
assert result['nearly_clipped_cyan_fraction']<=0.01,'bridge window lighting washes out adjacent hull plating'
