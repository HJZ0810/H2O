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
    print("connecting", URL)
    async with websockets.connect(URL, ssl=ctx, open_timeout=10) as ws:
        hello = await asyncio.wait_for(ws.recv(), 5)
        print("hello:", hello)
        await ws.send(json.dumps({"sub": "h2o/#"}))
        await asyncio.sleep(0.5)
        tele = json.dumps({"temperature": 26.8, "humidity": 44, "smoke": 210,
                           "air": 160, "fan_level": 40, "servo": 0})
        await ws.send(json.dumps({"pub": "h2o/yhyegj-01/telemetry", "data": tele}))
        print("published telemetry")
        await ws.send(json.dumps({"pub": "h2o/yhyegj-01/cmd", "data": "{\"servo\":90}"}))
        print("published cmd")
        got = []
        deadline = asyncio.get_event_loop().time() + 5
        while asyncio.get_event_loop().time() < deadline and len(got) < 2:
            try:
                msg = await asyncio.wait_for(ws.recv(), 3)
                print("recv:", msg[:110])
                got.append(msg)
            except asyncio.TimeoutError:
                break
        print()
        print("回环:", "通过" if got else "失败")
        return 0 if got else 1


if __name__ == "__main__":
    sys.exit(asyncio.run(main()))
