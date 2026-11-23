"""Telemetry-only WebSocket service. Device control belongs to OneNET."""
import asyncio
import json
import logging
import os
import re


DEVICE = re.compile(r'h2o/[A-Za-z0-9_-]{1,64}/telemetry\Z')
LISTEN = ('127.0.0.1', 9002)
log = logging.getLogger('h2o.bridge')


def subscription(raw):
    msg = json.loads(raw)
    if not isinstance(msg, dict) or set(msg) != {'sub'}:
        return None
    topic = msg['sub']
    return topic if isinstance(topic, str) and DEVICE.fullmatch(topic) else None


async def main():
    import paho.mqtt.client as mqtt
    import websockets

    loop = asyncio.get_running_loop()
    clients = {}
    queue = asyncio.Queue(maxsize=64)
    broker = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id='h2o-ws-bridge')
    broker.username_pw_set(os.environ['H2O_MQTT_USER'], os.environ['H2O_MQTT_PASSWORD'])

    def on_connect(client, userdata, flags, rc, props):
        if not rc.is_failure:
            client.subscribe('h2o/+/telemetry', qos=1)

    def enqueue(topic, payload):
        if queue.full():
            queue.get_nowait()
        queue.put_nowait((topic, payload))

    def on_message(client, userdata, message):
        if not message.retain and DEVICE.fullmatch(message.topic) and len(message.payload) <= 4096:
            loop.call_soon_threadsafe(enqueue, message.topic, message.payload.decode('utf-8', 'replace'))

    async def handler(ws, path=None):
        clients[ws] = None
        try:
            await ws.send('{"t":"ready"}')
            async for raw in ws:
                try:
                    topic = subscription(raw)
                except (ValueError, UnicodeError, RecursionError):
                    topic = None
                if topic is None:
                    await ws.send('{"t":"error","msg":"telemetry subscription required"}')
                else:
                    clients[ws] = topic
        except websockets.ConnectionClosed:
            pass
        finally:
            clients.pop(ws, None)

    async def send(ws, frame):
        try:
            await asyncio.wait_for(ws.send(frame), timeout=2)
        except (TimeoutError, websockets.ConnectionClosed):
            clients.pop(ws, None)
            await ws.close()

    broker.on_connect = on_connect
    broker.on_message = on_message
    broker.connect_async('127.0.0.1', 1884, 60)
    broker.loop_start()
    try:
        async with websockets.serve(handler, *LISTEN, max_size=1024, close_timeout=1):
            while True:
                topic, payload = await queue.get()
                frame = json.dumps({'t':'msg', 'topic':topic, 'data':payload}, ensure_ascii=False)
                await asyncio.gather(*(send(ws, frame) for ws, sub in list(clients.items()) if sub == topic))
    finally:
        broker.disconnect()
        broker.loop_stop()


if __name__ == '__main__':
    asyncio.run(main())
