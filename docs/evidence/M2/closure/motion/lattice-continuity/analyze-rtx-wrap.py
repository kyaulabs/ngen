"""Independent double-precision check of periodic field at the reported pixel; not a GPU substitute."""
import math,struct,json
from pathlib import Path
MASK=(1<<32)-1
def h(x,y,mask):
 x=int(x)&mask;y=int(y)&mask;b=((x*0x8da6b343)^(y*0xd8163841))&MASK;b^=b>>13;b=(b*0x85ebca6b)&MASK;b^=b>>16;return(b>>8)/16777216
def noise(x,y,mask):
 ix=math.floor(x);iy=math.floor(y);fx=x-ix;fy=y-iy;fx=fx*fx*(3-2*fx);fy=fy*fy*(3-2*fy)
 a=h(ix,iy,mask)*(1-fx)+h(ix+1,iy,mask)*fx;b=h(ix,iy+1,mask)*(1-fx)+h(ix+1,iy+1,mask)*fx;return a*(1-fy)+b*fy
def field(x,y):return .60*noise(x,y,511)+.25*noise(x*2+11,y*2+19,511)+.15*noise(x*.125+2,y*.125+5,63)
def smooth(a,b,x):
 t=min(1,max(0,(x-a)/(b-a)));return t*t*(3-2*t)
def radiance(u,v,phase):
 adv=u*3-phase*512;f=sum(field(adv+s*.2,v*24) for s in range(4))*.25;tail=smooth(.12,.28,u);density=1+tail*.9*(f-.5);skin=tail*smooth(.5,.95,abs(v*2-1));tip=smooth(.60,.93,u);w=max(skin,tip);density*=1-w+w*smooth(.15,.75,f);density*=1-smooth(.68,.995,u);return density*min(1,max(0,density))
def bits(x):return struct.unpack('<H',struct.pack('<e',x))[0]
rows=[]
for v in ((13+.5)/16,1-(13+.5)/16):
 raw=[radiance((43+.5)/192,v,((t+65536)%65536)/65536) for t in (-2,-1,0,1,2)]
 extended=[radiance((43+.5)/192,v,t/65536) for t in (-2,-1,0,1,2)]
 rows.append({'u':(43+.5)/192,'v':v,'linear_double':raw,'nearest_half':[f'{bits(x):04x}' for x in raw],'wrapped_vs_unwrapped_max':max(abs(a-b) for a,b in zip(raw,extended))})
r={'scope':'CPU double analytical field at pixel-center UV, zero lights/black environment/white emissive texture and alpha blend. Does not model device float interpolation, shader arithmetic or R16F blend rounding.','rows':rows,'actual_half':['38cb','38c9','38c6','38c6','38c4']};Path('build/m2-close-check/rtx-wrap-double-analysis.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))
