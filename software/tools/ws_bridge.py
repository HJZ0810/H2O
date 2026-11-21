#!/usr/bin/env python3
"""WS↔MQTT 桥：给浏览器一条能用的 WebSocket 通道。

为什么需要它：本机 mosquitto 2.0.11 的 websockets listener 虽然显示 LISTEN，
但 IPv4/IPv6 都连不进去（502），不修 broker 了，改用独立进程桥接。

协议（JSON 文本帧）：
  浏览器 -> 桥   {"sub": "h2o/#"}                  订阅（含通配符）
                 {"pub": "h2o/<dev>/cmd", "data": "..."}  发布
  桥 -> 浏览器   {"t":"msg","topic":"...","data":"..."}
                 {"t":"ready"} / {"t":"error","msg":"..."}
"""
import asyncio
import json
import logging

import paho.mqtt.client as mqtt
import websockets

MQTT_HOST, MQTT_PORT = "127.0.0.1", 1884
MQTT_USER, MQTT_PASS = "h2o", "H2oDemo2026"
LISTEN = ("127.0.0.1", 9002)

logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")
log = logging.getLogger("bridge")

clients = {}          # ws -> set(topics)
mq = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="h2o-ws-bridge")


def topic_match(pattern, topic):
    if pattern.endswith("/#"):
        return topic == pattern[:-2] or topic.startswith(pattern[:-1])
    if pattern.endswith("/+"):
        return topic.rsplit("/", 1)[0] == pattern[:-2]
    return pattern == topic


def on_connect(c, u, flags, rc, props=None):
    log.info("mqtt connected rc=%s", rc)
    c.subscribe("h2o/#", qos=1)


def on_message(c, u, msg):
    payload = msg.payload.decode("utf-8", "replace")
    frame = json.dumps({"t": "msg", "topic": msg.topic, "data": payload}, ensure_ascii=False)
    for ws, subs in list(clients.items()):
        if any(topic_match(p, msg.topic) for p in subs):
            try:
                asyncio.run_coroutine_threadsafe(ws.send(frame), LOOP)
            except Exception as exc:  # noqa: BLE001
                log.warning("push failed: %s", exc)


async def handler(ws):
    clients[ws] = set()
    await ws.send(json.dumps({"t": "ready"}))
    try:
        async for raw in ws:
            try:
                msg = json.loads(raw)
            except json.JSONDecodeError:
                await ws.send(json.dumps({"t": "error", "msg": "bad json"}))
                continue
            if "sub" in msg:
                clients[ws].add(str(msg["sub"]))
                log.info("sub %s (%d clients)", msg["sub"], len(clients))
            elif "pub" in msg:
                mq.publish(str(msg["pub"]), str(msg.get("data", "")), qos=1)
    except websockets.ConnectionClosed:
        pass
    finally:
        clients.pop(ws, None)


async def main():
    mq.username_pw_set(MQTT_USER, MQTT_PASS)
    mq.on_connect = on_connect
    mq.on_message = on_message
    mq.connect(MQTT_HOST, MQTT_PORT, 60)
    mq.loop_start()
    async with websockets.serve(handler, *LISTEN, ping_interval=30):
        log.info("ws bridge on %s:%s", *LISTEN)
        await asyncio.Future()


if __name__ == "__main__":
    LOOP = asyncio.new_event_loop()
    asyncio.set_event_loop(LOOP)
    LOOP.run_until_complete(main())
