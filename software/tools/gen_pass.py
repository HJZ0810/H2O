#!/usr/bin/env python
"""密码算法核对工具（对照《MQTT通信协议规范_学生版_v2.0》第三节 + 附录）

用法：
    python gen_pass.py 24001 A0:B1:C2:D3:E4:F5
    python gen_pass.py --selftest
    python gen_pass.py --probe 24001 A0:B1:C2:D3:E4:F5     # 顺带实测连服务器

算法（原文五步）：
    1. MAC 去掉冒号/横线/空格，得到 12 位十六进制 H
    2. SUM   = 组号(十进制) + 2026
    3. H 每 4 位一组，按十六进制转十进制，得 M1 M2 M3
    4. TOTAL = SUM + M1 + M2 + M3
    5. 密码  = TOTAL mod 2^24，转 8 位小写十六进制，左补 0
"""
import struct
import sys


def normalise(mac: str) -> str:
    return mac.replace(":", "").replace("-", "").replace(" ", "").upper()


def gen_password(group: str, mac: str, verbose: bool = False) -> str:
    h = normalise(mac)
    if len(h) != 12:
        raise ValueError(
            f"MAC 去掉分隔符后应为 12 位十六进制，实际 {len(h)} 位（{h}）。"
            "多半是把 7 个字节打出来了，或者读的是 AP 模式 MAC。"
        )
    group = group.strip()
    if not group.isdigit():
        raise ValueError(f"组号应为十进制数字，实际是 {group!r}")
    total = int(group) + 2026
    parts = []
    for i in range(0, 12, 4):
        val = int(h[i:i + 4], 16)
        parts.append(val)
        total += val
    password = format(total % (1 << 24), "08x")
    if verbose:
        print(f"  H      = {h}")
        print(f"  SUM    = {group} + 2026 = {int(group) + 2026}")
        print(f"  M1..M3 = {parts[0]}, {parts[1]}, {parts[2]}")
        print(f"  TOTAL  = {total}")
        print(f"  取余   = {total} mod 16777216 = {total % (1 << 24)} = 0x{password}")
    return password


# ---------- 极简 MQTT CONNECT，只读 CONNACK ----------
def probe(host, port, client_id, username, password, timeout=8):
    def s(x: str) -> bytes:
        b = x.encode()
        return struct.pack("!H", len(b)) + b

    body = (s("MQTT") + bytes([4]) + bytes([0b11000010]) + struct.pack("!H", 60)
            + s(client_id) + s(username) + s(password))
    rl, enc = len(body), b""
    while True:
        d, rl = rl % 128, rl // 128
        enc += bytes([d | (0x80 if rl else 0)])
        if not rl:
            break
    import socket
    sock = socket.create_connection((host, port), timeout=timeout)
    sock.sendall(b"\x10" + enc + body)
    resp = sock.recv(4)
    sock.close()
    return resp[3] if len(resp) >= 4 and resp[0] == 0x20 else -1


CODES = {0: "ACCEPTED 通过", 1: "协议版本不接受", 2: "Client ID 被拒",
         3: "服务不可用", 4: "用户名或密码错误", 5: "未授权（账号未开通/ID 不合规）"}


def selftest():
    cases = [
        ("24001", "A0:B1:C2:D3:E4:F5", "0002ae24", "规范 3.2 算例"),
        ("240401", "64:E8:33:7F:4D:B4", "00049916", "本组验证板"),
    ]
    ok = True
    for group, mac, want, note in cases:
        got = gen_password(group, mac)
        flag = "PASS" if got == want else "FAIL"
        ok = ok and got == want
        print(f"{flag}  {note}: 组号={group} MAC={mac} -> {got} (期望 {want})")
    # 14 位 MAC 应按前 12 位算，不应崩
    try:
        gen_password("24017", "2C:BC:BB:8A:5D:32:11")
        print("FAIL  14 位 MAC 未被拦截")
        ok = False
    except ValueError as exc:
        print(f"PASS  14 位 MAC 被正确拦截：{exc}")
    return 0 if ok else 1


def main():
    args = sys.argv[1:]
    if not args or args[0] in ("-h", "--help"):
        print(__doc__)
        return 0
    if args[0] == "--selftest":
        return selftest()
    if args[0] == "--probe":
        args = args[1:]
        do_probe = True
    else:
        do_probe = False
    if len(args) < 2:
        print("用法: python gen_pass.py <组号> <MAC>")
        return 2
    group, mac = args[0], args[1]
    print(f"组号 {group}  MAC {mac}")
    try:
        pwd = gen_password(group, mac, verbose=True)
    except ValueError as exc:
        print("错误:", exc)
        return 1
    print(f"  密码   = {pwd}")

    if do_probe:
        print("\n实测连接 124.70.71.221:1883 ...")
        rc = probe("124.70.71.221", 1883, f"esp32-{group}", group, pwd)
        print(f"  Client ID = esp32-{group}  -> CONNACK {rc} {CODES.get(rc, '')}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
