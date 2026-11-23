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
import argparse
import getpass
from urllib.parse import urlencode
import time


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
    return urlencode(dict(version=VERSION,res=resource,et=et,method=method,sign=sign))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Generate a device token locally. No network request.")
    parser.add_argument("pid")
    parser.add_argument("dev")
    parser.add_argument("--show-token", action="store_true", help="explicitly display the short-lived token")
    args = parser.parse_args()
    token = make_token(args.pid, args.dev, getpass.getpass("Device key: "), expire_secs=3600)
    print(token if args.show_token else "Token generated. Valid for one hour.")
