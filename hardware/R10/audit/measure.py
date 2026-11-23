"""Read this exporter's metric GKO/Excellon and cross-reference its mil flying probe."""
from pathlib import Path
from zipfile import ZipFile
from math import dist
import csv, json, re, hashlib
import matplotlib.pyplot as plt

ROOT = Path(__file__).resolve().parents[1]
REF = json.loads((ROOT.parent/'R9-1-4-RC1/verification.json').read_text(encoding='utf8'))['wide_candidate']
OUT = ROOT/'evidence'
report = {}; records = []; idc = []
fig, axes = plt.subplots(1,2,figsize=(13,7))
for board, filename in enumerate(['wide-PCB1-after-silk-gerber.zip','wide-PCB2-probe-gerber.zip'],1):
    path = ROOT/'gerber-baseline'/filename
    sha = hashlib.sha256(path.read_bytes()).hexdigest()
    assert sha == REF[str(board)]['traceable_gerber_sha256']
    with ZipFile(path) as z:
        assert z.testzip() is None
        outline = z.read('Gerber_BoardOutlineLayer.GKO').decode(); assert '%FSLAX45Y45*%' in outline and '%MOMM*%' in outline
        drill = z.read('Drill_PTH_Through.DRL').decode(); assert 'METRIC,LZ,0000.00000' in drill
        probe = json.loads(z.read('FlyingProbeTesting.json')); assert probe['lengthUnit']=='mil'
    # Assembly view: positive X right, positive Y down. GKO/DRL file Y is negative.
    loops = []
    for x,y,mode in re.findall(r'G01X(-?\d+)Y(-?\d+)D(01|02)\*',outline):
        if mode == '02': loops.append([])
        loops[-1].append((int(x)/1e5,-int(y)/1e5))
    assert all(dist(loop[0],loop[-1])<0.001 for loop in loops)
    holes = []; tools = dict(re.findall(r'^(T\d+)C([\d.]+)',drill,re.M))
    for line in drill.splitlines():
        if line in tools: dia = float(tools[line]); continue
        hit = re.fullmatch(r'X([-\d.]+)Y([-\d.]+)',line)
        if hit: holes.append((float(hit[1]),-float(hit[2]),dia))
    ax=axes[board-1]; bounds=[]
    for loop in loops:
        xs,ys=zip(*loop); box=(min(xs),min(ys),max(xs),max(ys));bounds.append(box)
        ax.plot(xs,ys,color='#273c52',lw=1.5)
    ax.scatter(*zip(*[(x,y) for x,y,d in holes]),s=4,color='#aab7bc')
    connectors = ['J1','J2','J8'] if board==1 else ['J10','J3']; parts={}
    for r in probe['pins']['rows']:
        comp,_,pin=r[1].partition('_');
        if comp not in connectors: continue
        nominal=(r[2]*.0254,-r[3]*.0254); actual=min(holes,key=lambda h:dist(h[:2],nominal)); assert dist(actual[:2],nominal)<.004
        val=(*actual,r[6]); existed=parts.setdefault(comp,{}).setdefault(int(pin),val); assert existed==val
    for comp,pins in parts.items():
        ax.scatter(*zip(*[v[:2] for v in pins.values()]),s=12,label=comp)
        for pin,(x,y,dia,net) in sorted(pins.items()):records.append([board,comp,pin,x,y,dia,net])
    conn=parts['J8' if board==1 else 'J10']; assert set(conn)==set(range(1,11)); idc.append([conn[i][3] for i in range(1,11)])
    grid=[sorted({round(v[axis],3) for v in conn.values()}) for axis in (0,1)];assert sorted(map(len,grid))==[2,5]
    assert all(abs(b-a-2.54)<.003 for group in grid for a,b in zip(group,group[1:]))
    x1,y1,x2,y2=bounds[0]; size=[round(x2-x1,4),round(y2-y1,4)]; assert size==([99.6,99.6] if board==1 else [86,64]);
    result={'zip_sha256':sha,'size_mm':size,'profile_loops':len(loops),'profiles_xyxy_mm':bounds,'idc_grid_xy':grid,'idc_pin_1':conn[1]}; report[str(board)]=result
    if board==1:
        rows=[sorted(parts[k].values()) for k in ('J1','J2')]; assert all(len(p)==22 for p in rows)
        for row in rows:
            assert all(abs(p[2]-1.05)<.001 for p in row) and max(p[1] for p in row)-min(p[1] for p in row)<.002
            assert all(abs(b[0]-a[0]-2.54)<.003 for a,b in zip(row,row[1:]))
        assert all(abs(a[0]-b[0])<.002 and abs(b[1]-a[1]-25.4)<.002 for a,b in zip(*rows));result['header_row_centers_y_mm']=[row[0][1] for row in rows]; result['header_span_mm']=round(rows[0][-1][0]-rows[0][0][0],3)
        result['M3_cutout_count']=len(loops)-1;result['drill_max_mm']=max(d for x,y,d in holes)
    else:
        circles=[]
        for a,b,c,d in bounds[1:]:
            if abs(c-a-3.2)<.002 and abs(d-b-3.2)<.002: circles.append((round((a+c)/2,3),round((b+d)/2,3)))
        servo=sorted((x,y) for x,y in circles if 10<y<25);assert servo==[(29.75,12),(29.75,22),(78.25,12),(78.25,22)];result['servo_mounts_mm']=servo
        result['support_mounts_mm']=sorted(set(circles)-set(servo));assert result['support_mounts_mm']==[(3.8,3.9),(3.8,45),(73.8,53.2),(82.2,3.9)]
        ax.scatter(*zip(*servo),s=60,facecolors='none',edgecolors='#e05342',label='servo M3')
    ax.set_aspect('equal');ax.invert_yaxis(); ax.legend(loc='lower center',bbox_to_anchor=(.5,-.28),ncol=2);ax.set_title(f'PCB{board}   /   {size[0]:g} x {size[1]:g} mm');ax.set_xlabel('X / mm');ax.set_ylabel('Y down / mm');ax.grid(alpha=.14)
    endpoints=sorted({p[1] for p in probe['pins']['rows'] if p[6]==conn[10][3]});result['IDC_pin10_probe_endpoints']=endpoints
    assert endpoints==(['J2_16','J8_10'] if board==1 else ['J10_10','J13_7','J24_1'])
assert idc[0][:9] == idc[1][:9] and (idc[0][9],idc[1][9])==('NET_9','NET_2')
with (ROOT.parent/'R9-1-4-RC1/pin-drill-audit.csv').open(encoding='utf-8-sig') as f:
    alias=[r for r in csv.DictReader(f) if (r['PCB'],r['位号'],r['脚号']) in [('1','J8','10'),('2','J10','10')]]
assert len(alias)==2 and all(r['原理图网络']==r['PCB网络']=='MOTOR_B_PWM' for r in alias)
report['IDC_1_to_10']=idc[0][:9]+['MOTOR_B_PWM'];report['pin10_local_aliases']=[idc[0][9],idc[1][9]]
with (OUT/'drill-net.csv').open('w',newline='',encoding='utf-8-sig') as f:
    w=csv.writer(f);w.writerow(['PCB','Ref','Pin','Drill X mm','Drill Y down mm','Tool mm','Probe net']);w.writerows(records)
(OUT/'cam-audit.json').write_text(json.dumps(report,indent=2),encoding='utf8');fig.suptitle('H2O  /  Wide manufacturing baseline     —     profile and drill readback',fontsize=17);fig.tight_layout();fig.savefig(OUT/'cam-readback.png',dpi=200);print('SHA 2/2 | header 44/44 | IDC 10/10:',idc[0]);print(report['2']['support_mounts_mm'])
