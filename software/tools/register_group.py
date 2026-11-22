#!/usr/bin/env python
"""班级组号授权工具（在 124.70.71.221 上运行）

用途：把「组号 + 开发板 MAC」登记进账户表，并同步 mosquitto 密码文件。
每次改动都会先备份，失败可回滚。

用法：
    python3 register.py 24014 AA:BB:CC:DD:EE:FF
    python3 register.py --csv groups.csv          # 每行：组号,MAC
    python3 register.py --list                     # 看已登记
    python3 register.py --show 24014               # 看单组详情
    python3 register.py --remove 24014
    python3 register.py --check 24014 AA:BB:CC:DD:EE:FF   # 只算不写

MAC 必须是 6 字节 12 位十六进制；组号必须能转成十进制整数。
"""
import argparse
import json
import os
import shutil
import subprocess
import sys
import time

ACCOUNTS = "/opt/iot-game/accounts.json"
PASSWD = "/etc/mosquitto/passwd"
BACKUP_DIR = "/opt/iot-game/backups"


def gen_password(group: str, mac: str) -> str:
    h = mac.replace(":", "").replace("-", "").replace(" ", "").upper()
    if len(h) != 12 or any(c not in "0123456789ABCDEF" for c in h):
        raise ValueError(f"MAC 必须是 12 位十六进制，实际 {len(h)} 位：{h}")
    if not group.isdigit():
        raise ValueError(f"组号必须能转成十进制整数，实际：{group!r}")
    total = int(group) + 2026
    for i in range(0, 12, 4):
        total += int(h[i:i + 4], 16)
    return format(total % (1 << 24), "08x")


def load_accounts():
    if not os.path.exists(ACCOUNTS):
        return {}
    with open(ACCOUNTS, encoding="utf-8") as f:
        return json.load(f)


def backup():
    os.makedirs(BACKUP_DIR, exist_ok=True)
    tag = time.strftime("%Y%m%d-%H%M%S")
    for path in (ACCOUNTS, PASSWD):
        if os.path.exists(path):
            shutil.copy2(path, f"{BACKUP_DIR}/{os.path.basename(path)}.{tag}")
    return tag


def write_accounts(data):
    tmp = ACCOUNTS + ".tmp"
    with open(tmp, "w", encoding="utf-8") as f:
        json.dump(data, f, ensure_ascii=False, indent=2, sort_keys=True)
    os.replace(tmp, ACCOUNTS)


def sync_passwd(groups):
    """把组号写进 mosquitto 密码文件；teacher/game-server 等已有账号保持不动。"""
    for group, item in groups.items():
        subprocess.run(["mosquitto_passwd", "-b", PASSWD, group, item["password"]],
                       check=True, capture_output=True)
    os.chmod(PASSWD, 0o640)
    # mosquitto 2.x 收到 SIGHUP 会重读密码文件，不用断开现有连接
    subprocess.run(["systemctl", "reload", "mosquitto"], check=False, capture_output=True)


def cmd_add(group, mac, dry=False):
    pwd = gen_password(group, mac)
    data = load_accounts()
    old = data.get(group)
    data[group] = {"mac": mac, "password": pwd}
    if dry:
        print(f"[dry] {group} MAC={mac} -> {pwd}（未写入）")
        return
    tag = backup()
    write_accounts(data)
    sync_passwd({group: data[group]})
    action = "更新" if old else "新增"
    print(f"{action} {group}: MAC={mac} 密码={pwd}"
          + (f"（原密码 {old['password']}）" if old else "") + f"  备份={tag}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("group", nargs="?")
    ap.add_argument("mac", nargs="?")
    ap.add_argument("--csv")
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--show")
    ap.add_argument("--remove")
    ap.add_argument("--check", nargs=2, metavar=("GROUP", "MAC"))
    a = ap.parse_args()

    if a.check:
        print(gen_password(*a.check))
        return 0
    if a.list:
        data = load_accounts()
        print(f"已登记 {len(data)} 组：")
        for g in sorted(data):
            print(f"  {g:>8s}  MAC={data[g]['mac']:<18s} 密码={data[g]['password']}")
        return 0
    if a.show:
        data = load_accounts()
        print(json.dumps(data.get(a.show, {"error": "未登记"}), ensure_ascii=False, indent=2))
        return 0
    if a.remove:
        data = load_accounts()
        if a.remove not in data:
            print(f"{a.remove} 未登记")
            return 1
        tag = backup()
        del data[a.remove]
        write_accounts(data)
        subprocess.run(["mosquitto_passwd", "-D", PASSWD, a.remove], check=False,
                       capture_output=True)
        subprocess.run(["systemctl", "reload", "mosquitto"], check=False, capture_output=True)
        print(f"已移除 {a.remove}（备份 {tag}）")
        return 0
    if a.csv:
        ok = bad = 0
        with open(a.csv, encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith("#"):
                    continue
                parts = [p.strip() for p in line.replace("\t", ",").split(",")]
                if len(parts) < 2:
                    print(f"  跳过格式不对的行：{line}")
                    bad += 1
                    continue
                try:
                    cmd_add(parts[0], parts[1])
                    ok += 1
                except Exception as exc:  # noqa: BLE001
                    print(f"  失败 {parts[0]}：{exc}")
                    bad += 1
        print(f"完成：成功 {ok} 组，失败 {bad} 组")
        return 0 if bad == 0 else 1
    if a.group and a.mac:
        cmd_add(a.group, a.mac)
        return 0
    ap.print_help()
    return 2


if __name__ == "__main__":
    sys.exit(main())
