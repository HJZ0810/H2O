"""Same-origin actuator gateway. The browser never chooses the cloud resource or URL."""
import hashlib
import hmac
import http.client
import json
import os
import re
import threading
import time

DEVICE = re.compile(r"[A-Za-z0-9_-]{1,64}")
LIMITS = {"fan_level": 100, "servo": 180}


def write_property(product, device, key, value, authorization):
    conn = http.client.HTTPSConnection("iot-api.heclouds.com", timeout=8)
    try:
        body = json.dumps({"product_id": product, "device_name": device, "params": {key: value}})
        conn.request("POST", "/thingmodel/set-device-property", body,
                     {"Content-Type": "application/json", "authorization": authorization})
        response = conn.getresponse()
        if response.status != 200:
            return False
        raw = response.read(16385)
        if len(raw) > 16384:
            return False
        out = json.loads(raw)
        return (isinstance(out, dict) and type(out.get("code")) is int and out["code"] == 0
                and isinstance(out.get("data"), dict) and type(out["data"].get("code")) is int
                and out["data"]["code"] == 200)
    finally:
        conn.close()


class Control:
    def __init__(self, env=None, writer=write_property):
        env = os.environ if env is None else env
        self.origin = env.get("H2O_CONTROL_ORIGIN", "")
        self.product = env.get("H2O_ONENET_PRODUCT", "")
        self.authorization = env.get("H2O_ONENET_AUTHORIZATION", "")
        self.digest = env.get("H2O_OPERATOR_SHA256", "")
        self.devices = frozenset(d.strip() for d in env.get("H2O_CONTROL_DEVICES", "").split(","))
        self.enabled = bool(re.fullmatch(r"https://[a-zA-Z0-9.-]+(:\d+)?|http://127\.0\.0\.1:\d+", self.origin)
                            and DEVICE.fullmatch(self.product) and all(DEVICE.fullmatch(d) for d in self.devices)
                            and self.authorization and self.authorization.isascii() and self.authorization.isprintable()
                            and re.fullmatch(r"[a-f0-9]{64}", self.digest))
        self.writer = writer
        self.lock = threading.Lock()
        self.busy = set()
        self.next_write = {}

    def available(self, dev):
        return self.enabled and dev in self.devices

    def post(self, handler):
        def reply(code, message):
            return handler._json({"message": message}, code)
        if not self.enabled:
            return reply(503, "网关处于只读模式")
        if handler.headers.get("Origin") != self.origin:
            return reply(403, "来源未获授权")
        auth = handler.headers.get("Authorization", "")
        if not auth.startswith("Bearer ") or not hmac.compare_digest(
                hashlib.sha256(auth[7:].encode()).hexdigest(), self.digest):
            return reply(401, "操作令牌无效")
        if handler.headers.get_content_type() != "application/json":
            return reply(415, "需要 JSON 请求")
        length = handler.headers.get("Content-Length", "")
        if not re.fullmatch(r"[0-9]{1,4}", length) or not 0 < int(length) <= 512 or 'Transfer-Encoding' in handler.headers:
            return reply(400, "请求长度无效")
        try:
            msg = json.loads(handler.rfile.read(int(length)))
        except (ValueError, RecursionError):
            return reply(400, "JSON 格式无效")
        if not isinstance(msg, dict) or set(msg) != {"dev", "key", "value"}:
            return reply(400, "指令格式无效")
        dev, key, value = msg['dev'], msg['key'], msg['value']
        if not isinstance(dev, str) or not self.available(dev):
            return reply(403, "设备未获授权")
        if not isinstance(key, str) or key not in LIMITS or type(value) is not int or not 0 <= value <= LIMITS[key]:
            return reply(400, "属性或设定值无效")
        with self.lock:
            blocked = dev in self.busy or time.monotonic() < self.next_write.get(dev, 0)
            if not blocked:
                self.busy.add(dev)
        if blocked:
            return reply(429, "设备指令处理中，请稍后再试")
        try:
            ok = self.writer(self.product, dev, key, value, self.authorization)
        except (OSError, http.client.HTTPException, ValueError, RecursionError):
            ok = False
        finally:
            with self.lock:
                self.busy.discard(dev)
                self.next_write[dev] = time.monotonic() + 1
        return reply(200, "设备已确认设定值") if ok else reply(502, "未获设备成功回执，请核对遥测后再操作")
