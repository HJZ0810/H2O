#!/usr/bin/env python
"""最小 MQTT 3.1.1 连接测试：只发 CONNECT，读 CONNACK 返回码。

返回码含义：0=成功 1=协议版本不接受 2=clientId 被拒 3=服务不可用
           4=用户名或密码错误 5=未授权
"""
import socket
import struct
import sys


def enc_str(s):
    b = s.encode()
    return struct.pack("!H", len(b)) + b


def connect(host, port, client_id, username, password, keepalive=60):
    vh = (
        enc_str("MQTT")
        + bytes([4])                      # protocol level 3.1.1
        + bytes([0b11000010])             # clean session + username + password
        + struct.pack("!H", keepalive)
    )
    payload = enc_str(client_id) + enc_str(username) + enc_str(password)
    body = vh + payload
    # remaining length 变长编码
    rl, out = len(body), b""
    while True:
        d = rl % 128
        rl //= 128
        out += bytes([d | (0x80 if rl else 0)])
        if not rl:
            break
    pkt = b"\x10" + out + body

    s = socket.create_connection((host, port), timeout=8)
    s.sendall(pkt)
    resp = s.recv(4)
    s.close()
    if len(resp) >= 4 and resp[0] == 0x20:
        return resp[3]
    return -1


if __name__ == "__main__":
    cid, user, pwd = sys.argv[1], sys.argv[2], sys.argv[3]
    rc = connect("mqtts.heclouds.com", 1883, cid, user, pwd)
    names = {0: "ACCEPTED", 1: "BAD_PROTOCOL", 2: "ID_REJECTED",
             3: "SERVER_UNAVAILABLE", 4: "BAD_USERNAME_OR_PASSWORD", 5: "NOT_AUTHORIZED"}
    print(f"CONNACK={rc} {names.get(rc, 'NO_RESPONSE')}")
