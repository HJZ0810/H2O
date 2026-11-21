"""监听镜像通道，确认板子真的在往 broker 发数据（不是面板造假）。"""
import asyncio
import json
import ssl
import sys

import websockets

URL = "wss://air-quality.luo-jin-ai.com/mqtt"


async def main():
    ctx = ssl.create_default_context()
    ctx.check_hostname = False
    ctx.verify_mode = ssl.CERT_NONE
    async with websockets.connect(URL, ssl=ctx, open_timeout=10) as ws:
        await ws.recv()  # ready
        await ws.send(json.dumps({"sub": "h2o/#"}))
        print("订阅 h2o/# ，等待 25 秒看板子有没有数据...")
        deadline = asyncio.get_event_loop().time() + 25
        n = 0
        while asyncio.get_event_loop().time() < deadline:
            try:
                raw = await asyncio.wait_for(ws.recv(), 6)
            except asyncio.TimeoutError:
                continue
            m = json.loads(raw)
            if m.get("t") != "msg":
                continue
            n += 1
            print(f"[{n}] {m['topic']}  {m['data'][:120]}")
            if m["topic"].endswith("/telemetry"):
                d = json.loads(m["data"])
                print(f"     温度={d.get('temperature')} 湿度={d.get('humidity')} "
                      f"烟雾={d.get('smoke')} 空气={d.get('air')} "
                      f"风扇={d.get('fan_level')} 舵机={d.get('servo')}")
        print()
        print("收到遥测条数:", n, "→", "通过" if n else "失败（板子没发上来）")
        return 0 if n else 1


if __name__ == "__main__":
    sys.exit(asyncio.run(main()))
