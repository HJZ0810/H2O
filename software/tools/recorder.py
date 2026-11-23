#!/usr/bin/env python3
"""遥测记录器：把设备数据滚动存下来，页面打开就能看到历史和最新值。

- 订阅 h2o/+/telemetry，每个设备一个定长环形缓冲（默认 720 点 ≈ 1 小时@5s）
- 每 10 秒落盘一次，重启不丢历史
- HTTP 查询接口：
    GET /history?dev=<设备名>&n=<条数>   返回 {"dev","latest","points":[...]}
    GET /status                          各设备最后上报距今多少秒
    GET /control/status?dev=...          指定设备是否配置控制
    POST /control                        默认关闭，仅接受经授权的同源请求
"""
import json
import os
import math
import re
import threading
import time
from collections import deque
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlparse

from r10_control import Control

control = Control()

MQTT_HOST, MQTT_PORT = "127.0.0.1", 1884
HTTP_HOST, HTTP_PORT = "127.0.0.1", 9003
STORE = "/opt/h2o-bridge/history.json"
MAX_POINTS = 720
SAVE_EVERY = 10

lock = threading.Lock()
data = {}  # dev -> {"points": deque, "latest": dict, "at": float}


def load():
    global data
    if not os.path.exists(STORE):
        return
    try:
        with open(STORE, encoding="utf-8") as f:
            raw = json.load(f)
        for dev, item in raw.items():
            d = deque(item.get("points", []), maxlen=MAX_POINTS)
            data[dev] = {"points": d, "latest": item.get("latest", {}),
                         "at": item.get("at", 0)}
        print(f"[store] loaded {len(data)} device(s), "
              f"{sum(len(v['points']) for v in data.values())} points")
    except Exception as exc:  # noqa: BLE001
        print("[store] load failed:", exc)


def save():
    with lock:
        payload = {dev: {"points": list(v["points"]), "latest": v["latest"], "at": v["at"]}
                   for dev, v in data.items()}
    tmp = STORE + ".tmp"
    with open(tmp, "w", encoding="utf-8") as f:
        json.dump(payload, f, ensure_ascii=False)
    os.replace(tmp, STORE)


def on_connect(c, u, flags, rc, props=None):
    print("[mqtt] connected rc=", rc)
    c.subscribe("h2o/+/telemetry", qos=1)


def on_message(c, u, msg):
    if msg.retain:
        return
    parts = msg.topic.split("/")
    if len(parts) != 3 or parts[0] != "h2o" or parts[2] != "telemetry" or not re.fullmatch(r"[A-Za-z0-9_-]{1,64}", parts[1]) or len(msg.payload) > 4096:
        return
    dev = parts[1]
    try:
        payload = json.loads(msg.payload.decode("utf-8", "replace"))
    except (ValueError, RecursionError):
        return
    if not isinstance(payload, dict):
        return
    point = {k: v for k, v in payload.items() if k in {"temperature", "humidity", "air", "smoke", "fan_level", "servo", "onenet"} and type(v) in (int, float) and -1e6 <= v <= 1e6 and math.isfinite(v)}
    if not point:
        return
    point["ts"] = time.time()  # Device millis() is uptime, not Unix time.

    with lock:
        item = data.setdefault(dev, {"points": deque(maxlen=MAX_POINTS), "latest": {}, "at": 0})
        item["points"].append(point)
        item["latest"] = point
        item["at"] = time.time()


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *args):  # 安静点
        pass

    def _json(self, obj, code=200):
        body = json.dumps(obj, ensure_ascii=False).encode()
        self.send_response(code)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("X-Content-Type-Options", "nosniff")
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def do_POST(self):
        if self.path == "/control":
            return control.post(self)
        return self._json({"error": "not found"}, 404)

    def do_GET(self):
        u = urlparse(self.path)
        if u.path == "/control/status":
            dev = parse_qs(u.query).get("dev", [""])[0]
            return self._json({"enabled": control.available(dev)})
        if u.path.rstrip("/") == "/status":
            now = time.time()
            with lock:
                out = {dev: {"age": round(now - v["at"], 1),
                             "points": len(v["points"]),
                             "onenet": (bool(v["latest"]["onenet"]) if v["latest"].get("onenet") in (0, 1) else None),
                             "source": "history"}
                       for dev, v in data.items()}
            return self._json({"devices": out, "now": now})

        if u.path.rstrip("/") == "/history":
            q = parse_qs(u.query)
            dev = (q.get("dev") or [""])[0]
            try:
                n = max(1, min(MAX_POINTS, int((q.get("n") or ["300"])[0])))
            except ValueError:
                return self._json({"error": "invalid count"}, 400)
            with lock:
                if dev:
                    item = data.get(dev)
                    response = {"dev": dev, "latest": None, "points": []} if not item else {
                        "dev": dev, "latest": item["latest"], "at": item["at"], "points": list(item["points"])[-n:]}
                else:
                    response = {"devices": {d: {"points": len(v["points"])} for d, v in data.items()}}
            return self._json(response)

        return self._json({"error": "not found"}, 404)


def saver():
    while True:
        time.sleep(SAVE_EVERY)
        try:
            save()
        except Exception as exc:  # noqa: BLE001
            print("[store] save failed:", exc)


def main():
    import paho.mqtt.client as mqtt
    load()
    threading.Thread(target=saver, daemon=True).start()
    c = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="h2o-recorder")
    c.username_pw_set(os.environ["H2O_MQTT_USER"], os.environ["H2O_MQTT_PASSWORD"])
    c.on_connect = on_connect
    c.on_message = on_message
    c.connect(MQTT_HOST, MQTT_PORT, 60)
    c.loop_start()
    srv = ThreadingHTTPServer((HTTP_HOST, HTTP_PORT), Handler)
    print(f"[http] listen {HTTP_HOST}:{HTTP_PORT}")
    srv.serve_forever()


if __name__ == "__main__":
    main()
