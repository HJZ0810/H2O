#!/usr/bin/env python
"""OneNET 安全鉴权：用设备密钥生成 MQTT 密码 token，并实测 CONNACK。

算法（OneNET 安全鉴权 2018-10-31）：
  resource = products/{pid}/devices/{dev}
  stringForSignature = et + "\n" + method + "\n" + resource + "\n" + version
  sign = base64( HMAC-SHA1( base64decode(key), stringForSignature ) )
  token = version=2018-10-31&res={resource}&et={et}&method={method}&sign={sign}
"""
import base64
import hashlib
import hmac
import sys
import time

sys.path.insert(0, ".")
from mqtt_probe import connect  # noqa: E402

VERSION = "2018-10-31"


def make_token(pid, dev, key, expire_secs=86400 * 30, method="sha1"):
    resource = f"products/{pid}/devices/{dev}"
    et = str(int(time.time()) + expire_secs)
    for_sign = f"{et}\n{method}\n{resource}\n{VERSION}"
    digestmod = hashlib.sha1 if method == "sha1" else hashlib.md5
    key_bytes = base64.b64decode(key)
    sign = base64.b64encode(
        hmac.new(key_bytes, for_sign.encode(), digestmod=digestmod).digest()
    ).decode()
    return f"version={VERSION}&res={resource}&et={et}&method={method}&sign={sign}"


if __name__ == "__main__":
    pid, dev, key = sys.argv[1], sys.argv[2], sys.argv[3]
    token = make_token(pid, dev, key)
    print("token =", token)
    rc = connect("mqtts.heclouds.com", 1883, dev, pid, token)
    names = {0: "ACCEPTED", 4: "BAD_USERNAME_OR_PASSWORD", 5: "NOT_AUTHORIZED"}
    print(f"CONNACK={rc} {names.get(rc, 'OTHER')}")
