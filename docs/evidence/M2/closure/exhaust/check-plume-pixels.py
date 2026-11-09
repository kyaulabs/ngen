"""Aft of the unchanged nozzle exit in a fixed native 1920x1080 side camera."""
from pathlib import Path
import struct,json,sys
raw=Path(sys.argv[1]).read_bytes();off=struct.unpack_from('<I',raw,10)[0];w,h=struct.unpack_from('<ii',raw,18);assert(w,h)==(1920,1080)
blue=white=bright=0
for y in range(200,900):
 for x in range(1110,1260):
  i=off+((h-1-y)*w+x)*4;b,g,r,a=raw[i:i+4]
  bright+=max(r,g,b)>80;blue+=b>120 and b>r*1.15;white+=min(r,g,b)>210
print(json.dumps({'aft_of_nozzle_roi':[1110,200,1260,900],'bright_pixels':bright,'blue_pixels':blue,'white_core_pixels':white}))
assert bright>250 and blue>100 and white>10,'directional blue-white plume does not extend beyond nozzle'
