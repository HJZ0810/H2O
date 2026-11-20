"""Connect the native R9 simulator to a ThingsCloud test device over MQTT."""

import argparse
import json
import select
import socket
import struct
import subprocess
import time
from pathlib import Path


def mqtt_text(text):
    data = text.encode("utf-8")
    return struct.pack(">H", len(data)) + data


def send_packet(sock, header, body=b""):
    size = len(body)
    length = bytearray()
    while size >= 128:
        length.append((size % 128) | 128)
        size //= 128
    length.append(size)
    sock.sendall(bytes([header]) + length + body)


def read_exact(sock, length):
    result = bytearray()
    while len(result) < length:
        chunk = sock.recv(length - len(result))
        if not chunk:
            raise ConnectionError("MQTT broker disconnected")
        result.extend(chunk)
    return bytes(result)


def read_packet(sock):
    header = read_exact(sock, 1)[0]
    length = 0
    for shift in range(0, 28, 7):
        digit = read_exact(sock, 1)[0]
        length |= (digit & 127) << shift
        if not digit & 128:
            if length > 32768:
                raise ValueError("MQTT packet exceeded simulator buffer")
            return header, read_exact(sock, length)
    raise ValueError("Invalid MQTT packet length")


def connect(sock, access_token, project_key):
    options = mqtt_text("MQTT") + b"\x04\xc2\x00\x3c"
    login = mqtt_text("h2o-r9-simulator") + mqtt_text(access_token) + mqtt_text(project_key)
    send_packet(sock, 0x10, options + login)
    header, response = read_packet(sock)
    if header != 0x20 or response != b"\x00\x00":
        raise ConnectionError("ThingsCloud rejected the MQTT connection")
    for packet_id, topic in enumerate(("attributes/push", "attributes/response"), 1):
        identifier = struct.pack(">H", packet_id)
        send_packet(sock, 0x82, identifier + mqtt_text(topic) + b"\x00")
        if read_packet(sock) != (0x90, identifier + b"\x00"):
            raise ConnectionError(f"ThingsCloud rejected subscription to {topic}")


def unpack_publish(header, body):
    if len(body) < 2:
        raise ValueError("Truncated MQTT PUBLISH")
    topic_length = struct.unpack_from(">H", body)[0]
    offset = 2 + topic_length
    if offset > len(body) or header & 0x06:
        raise ValueError("Expected a ThingsCloud QoS 0 PUBLISH")
    return body[2:offset].decode("utf-8"), body[offset:].decode("utf-8")


def simulate(sock, driver, seconds):
    state = json.loads(driver.stdout.readline())
    dirty = True
    deadline = time.monotonic() + seconds
    last_report = 0
    last_ping = time.monotonic()
    print("Connected to ThingsCloud using the R9 native PWM simulator", flush=True)
    while time.monotonic() < deadline:
        now = time.monotonic()
        if dirty and now - last_report >= 5:
            send_packet(sock, 0x30, mqtt_text("attributes") + json.dumps(state).encode("utf-8"))
            print("TX attributes", json.dumps(state), flush=True)
            last_report = now
            dirty = False
        if now - last_ping >= 40:
            send_packet(sock, 0xC0)
            last_ping = now
        if not select.select([sock], [], [], 0.25)[0]:
            continue
        header, body = read_packet(sock)
        if header & 0xF0 != 0x30:
            continue
        topic, payload = unpack_publish(header, body)
        if topic == "attributes/push":
            print("RX attributes/push", payload, flush=True)
            driver.stdin.write(payload + "\n")
            driver.stdin.flush()
            state = json.loads(driver.stdout.readline())
            dirty = True
        if topic == "attributes/response":
            print("RX attributes/response", payload, flush=True)
            if json.loads(payload).get("result") != 1:
                raise RuntimeError("ThingsCloud rejected an attribute report")
    send_packet(sock, 0xE0)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("driver", type=Path, help="test_thingscloud.exe built by test.ps1")
    parser.add_argument("credentials", type=Path, help="private JSON with host, accessToken, projectKey")
    parser.add_argument("--seconds", type=int, default=120)
    args = parser.parse_args()
    credentials = json.loads(args.credentials.read_text(encoding="utf-8-sig"))
    if args.seconds <= 0:
        parser.error("--seconds must be positive")
    with socket.create_connection((credentials["host"], 1883), timeout=8) as sock:
        connect(sock, credentials["accessToken"], credentials["projectKey"])
        with subprocess.Popen([str(args.driver.resolve()), "--simulate"], stdin=subprocess.PIPE,
                              stdout=subprocess.PIPE, text=True, encoding="utf-8") as driver:
            try:
                simulate(sock, driver, args.seconds)
            finally:
                driver.stdin.close()
                driver.wait(timeout=5)


if __name__ == "__main__":
    main()
