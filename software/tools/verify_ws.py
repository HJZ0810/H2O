"""Read-only browser bridge check. Run with the deployment Python environment."""
import asyncio
import json
import os
import re
import websockets

async def check():
    device = os.environ["H2O_DEVICE"]
    if not re.fullmatch(r"[A-Za-z0-9_-]{1,64}", device):
        raise ValueError("invalid device name")
    topic = f"h2o/{device}/telemetry"
    async with websockets.connect(os.environ["H2O_WS_URL"], open_timeout=8) as ws:
        if json.loads(await asyncio.wait_for(ws.recv(), 5)).get("t") != "ready":
            raise RuntimeError("bridge not ready")
        await ws.send(json.dumps({"sub": topic}))
        frame = json.loads(await asyncio.wait_for(ws.recv(), 30))
        if (frame.get("t") != "msg" or frame.get("topic") != topic
                or not isinstance(json.loads(frame["data"]), dict)):
            raise RuntimeError("telemetry unavailable")
        print("Telemetry received")

if __name__ == "__main__":
    asyncio.run(check())
