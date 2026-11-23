"""Plot selected native R10 objects and original CAM hole locations. All board views are from above."""
import json, zipfile, re
from pathlib import Path
import matplotlib.pyplot as plt
from matplotlib.collections import LineCollection
from matplotlib.patches import Circle
from matplotlib import font_manager

ROOT=Path(__file__).resolve().parents[1]
font=font_manager.FontProperties(fname='C:/Windows/Fonts/msyh.ttc');plt.rcParams['font.family']=font.get_name()
with zipfile.ZipFile(ROOT/'source/H2O-R10-YD254.epro2') as z:
    raw=z.read(next(n for n in z.namelist() if n.endswith('.epru'))).decode()
boards={}; title=None
for line in raw.split('\n'):
    if '||' not in line: continue
    head,tail=line.split('||',1); kind=json.loads(head)['type']
    if kind=='DOCHEAD':
        is_pcb=json.loads(tail[:-1])['docType']=='PCB';title=None
    elif is_pcb and kind=='META':
        title=json.loads(tail[:-1])['title'];boards[title]=[]
    elif title and kind in ('STRING','LINE','VIA'):
        boards[title].append((kind,json.loads(tail[:-1])))
for board,objects in boards.items():
    archive=next((ROOT/'gerber-baseline').glob(f'*{board}*.zip'))
    with zipfile.ZipFile(archive) as z:
        profile=z.read('Gerber_BoardOutlineLayer.GKO').decode()
        pins=json.loads(z.read('FlyingProbeTesting.json'))['pins']['rows']
    loops=[]
    for x,y,mode in re.findall(r'G01X(-?\d+)Y(-?\d+)D(01|02)\*',profile):
        if mode=='02':loops.append([])
        loops[-1].append((int(x)/1e5,-int(y)/1e5))
    for layer in (1,2):
        fig,ax=plt.subplots(figsize=(11,8));side='top' if layer==1 else 'bottom'
        for loop in loops:ax.plot(*zip(*loop),color='#293c4c',lw=.85)
        traces=[];text_items=[]
        for kind,obj in objects:
            if kind=='LINE' and obj['layerId']==layer:
                traces.append([(obj[k+'X']*.0254,-obj[k+'Y']*.0254) for k in ('start','end')])
            elif kind=='STRING' and obj['layerId']==layer+2:
                text_items.append(obj)
            elif kind=='VIA':ax.add_patch(Circle((obj['centerX']*.0254,-obj['centerY']*.0254),obj['holeDiameter']*.0127,color='#829c9f'))
        ax.add_collection(LineCollection(traces,colors='#3d9694' if layer==1 else '#9074a0',linewidths=.87))
        for x,y in {(p[2],p[3]) for p in pins if p[5]=='DIP'}:ax.add_patch(Circle((x*.0254,-y*.0254),.43,ec='#754e26',fc='white',lw=.55))
        for obj in text_items:
            ax.text(obj['x']*.0254,-obj['y']*.0254,obj['text'],size=min(7,obj['fontSize']*.065),va='top',rotation=obj['angle'],color='#1e3141')
        ax.set_aspect('equal');ax.invert_yaxis();ax.autoscale_view();ax.set_xlabel('装配坐标 X / mm');ax.set_ylabel('Y 向下 / mm');ax.grid(alpha=.1)
        ax.set_title(f'{board} · R10 '+('正面' if layer==1 else '底层（从正面透视）')+'    |    工程对象读回',fontsize=16,pad=16)
        fig.text(.5,.035,f'{len(traces)} 段走线  ·  钻孔与板框来自已核对的宽版 CAM  ·  文字为定位示意  ·  未显示覆铜与封装图形',ha='center',size=9)
        fig.savefig(ROOT/f'evidence/{board}-{side}.png',dpi=200);plt.close(fig)
