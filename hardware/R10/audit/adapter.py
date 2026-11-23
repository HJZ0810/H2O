"""Produce a mm-only cutting file, check its actual geometry, and plot a 1:1 template."""
from pathlib import Path
import json, math
import ezdxf
import matplotlib.pyplot as plt
from matplotlib.patches import Circle, Polygon
from matplotlib import font_manager

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT/'mechanical'
cam = json.loads((ROOT/'evidence/cam-audit.json').read_text())['2']
# Local center is the center of the drilled servo pattern. Y up for CAD.
anchors = [(x-54,17-y) for x,y in cam['servo_mounts_mm']]
doc = ezdxf.new('R2010'); doc.units = 4
for name in ('OUTLINE','WINDOW','M3_CLEARANCE'): doc.layers.new(name)
m = doc.modelspace()
def rect(width,height,layer):
    pts=[(-width/2,-height/2),(width/2,-height/2),(width/2,height/2),(-width/2,height/2)]
    m.add_lwpolyline(pts,close=True,dxfattribs={'layer':layer})
rect(55,18,'OUTLINE'); rect(24.4,13.6,'WINDOW')
for anchor in anchors: m.add_circle(anchor,1.6,dxfattribs={'layer':'M3_CLEARANCE'})
path = OUT/'SG90-adapter-cut-mm.dxf';doc.saveas(path)
rd=ezdxf.readfile(path); model=rd.modelspace(); assert rd.units==4 and not rd.audit().has_errors
polys=list(model.query('LWPOLYLINE')); circs=list(model.query('CIRCLE'))
assert len(circs)==4 and len(polys)==2 and all(p.closed for p in polys)
assert all(c.dxf.radius==1.6 for c in circs)
assert sorted((c.dxf.center.x,c.dxf.center.y) for c in circs)==sorted(anchors)
for poly,w,h in zip(polys,[55,24.4],[18,13.6]):
    xs,ys=zip(*poly.get_points('xy')); assert (max(xs)-min(xs),max(ys)-min(ys))==(w,h)
assert min(27.5-abs(x)-1.6 for x,y in anchors)>=1.64
font=font_manager.FontProperties(fname='C:/Windows/Fonts/msyh.ttc');plt.rcParams['font.family']=font.get_name();plt.rcParams['pdf.fonttype']=42
fig=plt.figure(figsize=(210/25.4,148/25.4)); ax=fig.add_axes([0,0,1,1]); ax.set_xlim(0,210);ax.set_ylim(0,148);ax.axis('off');ax.set_aspect('equal'); col='#163946'; accent='#177969'
def text(x,y,s,sz=10,c=col):ax.text(x,y,s,fontsize=sz,color=c,va='center')
text(13,134,'H2O R10    SG90 舵机转接板',20);text(13,122,'PCB2  /  顶视配钻样板     ·     A5 横向     ·     毫米',10,accent)
ox,oy=55,87
for poly in polys:ax.add_patch(Polygon([(x+ox,y+oy) for x,y in poly.get_points('xy')],fill=False,color=col,lw=.8))
for c in circs:
    x,y=c.dxf.center.x+ox,c.dxf.center.y+oy;ax.add_patch(Circle((x,y),c.dxf.radius,fill=False,lw=.6));ax.plot([x-2.4,x+2.4],[y,y],lw=.35,color=accent);ax.plot([x,x],[y-2.4,y+2.4],lw=.35,color=accent)
text(93,99,'外廓   55 × 18     /     绝缘板建议厚 2',10)
text(93,89,'开窗   24.4 × 13.6     /     先穿舵机线',10)
text(93,79,'四孔   Ø3.2     /     孔矩 48.5 × 10',10)
text(13,63,'两只舵机耳孔留白。把到货件插入开窗，贴合安装耳，再用实物转印孔心。',10)
text(13,53,'先用纸样核对锁耳、出线与轴心。配钻只在转接板上进行，勿钻主 PCB。',10)
text(13,43,'用四个绝缘隔柱架高，按耳底至机壳最低点、线束弯曲及板上最高点确定净高。',10)
ax.plot([13,63],[23,23],color=col,lw=.8);ax.plot([13,13],[21,25],color=col);ax.plot([63,63],[21,25],color=col);text(71,23,'50 mm 校准线  /  实际大小 100% 打印，关闭适应页面',9,accent)
fig.savefig(OUT/'SG90-template-1to1.pdf');fig.savefig(ROOT/'evidence/adapter-readback.png',dpi=240);print('DXF readback: 2 closed rectangles and 4 matching M3 holes. PDF A5.')
