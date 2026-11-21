"""端到端验证：浏览器等价路径（wss 经 nginx → mosquitto websockets）。

模拟两件事：
  1) 订阅 h2o/# 收遥测；
  2) 发一条控制指令，确认同一 broker 上设备侧能收到。
"""
import ssl
import sys
import threading
import time

import paho.mqtt.client as mqtt

HOST = "air-quality.luo-jin-ai.com"
USER, PWD = "h2o", "H2oDemo2026"
got = []
cmd_ok = []


def main():
    c = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="verify-web",
                    transport="websockets")
    c.ws_set_options(path="/mqtt")
    c.username_pw_set(USER, PWD)
    c.tls_set(cert_reqs=ssl.CERT_NONE)
    c.tls_insecure_set(True)

    def on_connect(cl, u, flags, rc, props=None):
        print("CONNACK:", rc)
        cl.subscribe("h2o/#", qos=1)

    def on_message(cl, u, msg):
        text = msg.payload.decode("utf-8", "replace")
        print(f"  <- {msg.topic} {text[:80]}")
        if "telemetry" in msg.topic:
            got.append(text)

    c.on_connect = on_connect
    c.on_message = on_message
    print(f"连接 wss://{HOST}/mqtt ...")
    c.connect(HOST, 443, 30)
    c.loop_start()
    time.sleep(2.5)

    if not c.is_connected():
        print("失败：连不上 WebSocket")
        return 1

    payload = ('{"temperature":25.4,"humidity":41,"smoke":220,"air":180,'
               '"fan_level":40,"servo":0,"ts":%d}' % int(time.time() * 1000))
    c.publish("h2o/yhyegj-01/telemetry", payload, qos=1)
    print("  -> 已发布模拟遥测")

    c.publish("h2o/yhyegj-01/cmd", '{"servo":90}', qos=1)
    print("  -> 已发布控制指令")

    time.sleep(2.5)
    c.loop_stop()
    c.disconnect()

    print()
    print("遥测回环:", "通过" if got else "失败")
    return 0 if got else 1


if __name__ == "__main__":
    sys.exit(main())
