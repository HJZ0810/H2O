#!/usr/bin/env python
"""串口取屏：发 key/shot 命令，把 framebuf 还原成 PNG。

用法：
  python ui_shot.py COM6 menu             只取当前屏
  python ui_shot.py COM6 key ok           注入按键后再取
  python ui_shot.py COM6 walk             按菜单顺序走一遍
"""
import base64
import sys
import time

import serial
from PIL import Image

PORT = sys.argv[1] if len(sys.argv) > 1 else "COM6"
OUT_DIR = r"C:\cu\ui"


def open_port():
    s = serial.Serial(PORT, 115200, timeout=1.2)
    s.setDTR(False)
    s.setRTS(False)
    time.sleep(0.3)
    s.reset_input_buffer()
    return s


def send(s, cmd, wait=0.4):
    s.write((cmd + "\n").encode())
    time.sleep(wait)
    out = []
    t0 = time.time()
    while time.time() - t0 < wait:
        line = s.readline().decode("utf-8", "replace").strip()
        if line:
            out.append(line)
    return out


def shot(s, name, wait=3.0):
    s.reset_input_buffer()
    s.write(b"shot\n")
    w = h = 0
    chunks = []
    t0 = time.time()
    while time.time() - t0 < wait + 8:
        line = s.readline().decode("ascii", "replace").strip()
        if line.startswith("SHOT "):
            _, ws, hs = line.split()
            w, h = int(ws), int(hs)
        elif line.startswith("D "):
            chunks.append(line[2:])
        elif line == "END":
            break
    if not chunks:
        print(f"  {name}: 没有收到画面")
        return None
    raw = base64.b64decode("".join(chunks))
    if len(raw) < w * h * 2:
        print(f"  {name}: 数据不完整 {len(raw)}/{w*h*2}")
        return None
    img = Image.new("RGB", (w, h))
    px = img.load()
    for i in range(w * h):
        v = (raw[i * 2] << 8) | raw[i * 2 + 1]  # 大端 RGB565
        r = ((v >> 11) & 0x1F) * 255 // 31
        g = ((v >> 5) & 0x3F) * 255 // 63
        b = (v & 0x1F) * 255 // 31
        px[i % w, i // w] = (r, g, b)
    path = f"{OUT_DIR}\\{name}.png"
    img.resize((w * 4, h * 4), Image.NEAREST).save(path)
    print(f"  {name}: {w}x{h} -> {path}")
    return path


if __name__ == "__main__":
    import os
    os.makedirs(OUT_DIR, exist_ok=True)
    s = open_port()
    try:
        mode = sys.argv[2] if len(sys.argv) > 2 else "shot"
        if mode == "key":
            keys = sys.argv[3:]
            for k in keys:
                print(" ".join(send(s, f"key {k}", 0.9)))
            shot(s, "after-" + "-".join(keys))
        elif mode == "walk":
            shot(s, "00-menu")
            names = ["开始考试", "环境采样", "设备信息", "配网设置", "执行器", "云端监测"]
            for i in range(len(names)):
                print(" ".join(send(s, "key down", 0.8)))
                shot(s, f"{i+1:02d}-sel{i+1}")
        else:
            print(" ".join(send(s, "page", 0.6)))
            shot(s, mode)
    finally:
        s.close()
