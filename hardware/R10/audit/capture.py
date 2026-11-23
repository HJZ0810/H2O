from pathlib import Path
import base64, shutil, json
from playwright.sync_api import sync_playwright
import fitz
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
E=ROOT/'evidence'; target=Path.home()/'Pictures/Agent截图/H2O-R10-20261123';target.mkdir(parents=True,exist_ok=True)
views=[('01-CAM', '制造归档 · 双板实测孔阵','核对原始 Gerber / Excellon · 毫米 · 2026-11-23',['cam-readback.png']),('02-R10-source','修订工程 · 双板顶层定位','从可编辑工程提取走线和丝印位置 · 非原生 DRC 画面',['PCB1-top.png','PCB2-top.png']),('03-SG90','机械附件 · SG90 转接板','从交付 PDF 重新渲染 · 安装耳按实物配钻',['pdf'])]
with sync_playwright() as p:
    browser=p.chromium.launch(channel='msedge',headless=True);tab=browser.new_page(viewport={'width':1920,'height':1080},device_scale_factor=1)
    for ident,title,caption,imgs in views:
        src=[]
        for img in imgs:
            if img=='pdf':
                with fitz.open(ROOT/'mechanical/SG90-template-1to1.pdf') as doc:
                    page=doc[0];assert abs(page.rect.width*25.4/72-210)<.02 and abs(page.rect.height*25.4/72-148)<.02
                    data=page.get_pixmap(matrix=fitz.Matrix(2,2)).tobytes('png')
            else: data=(E/img).read_bytes()
            src.append('<img src="data:image/png;base64,'+base64.b64encode(data).decode()+'">')
        tab.set_content('<meta charset="utf-8"><style>*{box-sizing:border-box}body{margin:48px 64px;font:26px Microsoft YaHei;color:#223b4f;background:#f4f6f5}header{font:bold 18px Arial;color:#307a70}h1{margin:12px 0 5px;font-size:42px}p{margin:0 0 14px}main{display:flex;align-items:center;justify-content:center;height:825px;background:white}img{object-fit:contain;width:'+('49%' if len(src)>1 else '94%')+';height:98%}</style><header>H2O ENGINEERING  /  R10</header><h1>'+title+'</h1><p>'+caption+'</p><main>'+''.join(src)+'</main>');tab.evaluate('Promise.all(Array.from(document.images, i=>i.decode()))'); shot=E/(ident+'.png');tab.screenshot(path=shot); assert Image.open(shot).size==(1920,1080);shutil.copyfile(shot,target/shot.name)
    browser.close()
print('3 x 1920x1080 browser captures; PDF A5 dimensions verified')
