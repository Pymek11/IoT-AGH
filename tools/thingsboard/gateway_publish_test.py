#!/usr/bin/env python3
"""Emulates the LoPy receiver: publishes one LoRa packet to ThingsBoard through the gateway MQTT API.

Use it to check the server side (token, gateway device, device creation, rule chain, dashboard)
without any hardware. Uses only the Python standard library.

    export TB_GATEWAY_TOKEN=<access token of the gateway device "lopy">
    python3 tools/thingsboard/gateway_publish_test.py                 # typ 0 = test, raises no alarm
    python3 tools/thingsboard/gateway_publish_test.py --sensor 4 --typ 3   # CRITICAL alarm on sensor-4

Exit code 0 = broker accepted the token and acknowledged the telemetry (PUBACK).
"""
import argparse
import json
import os
import socket
import struct
import sys
import time

CONNACK_ERRORS = {
    1: "unacceptable protocol version",
    2: "client id rejected",
    3: "server unavailable",
    4: "bad credentials",
    5: "not authorized - the access token is wrong or the device does not exist",
}


def enc_str(s: str) -> bytes:
    b = s.encode()
    return struct.pack("!H", len(b)) + b


def enc_len(n: int) -> bytes:
    out = bytearray()
    while True:
        digit, n = n % 128, n // 128
        out.append(digit | (0x80 if n else 0))
        if not n:
            return bytes(out)


def read_exact(sock: socket.socket, n: int) -> bytes:
    data = b""
    while len(data) < n:
        chunk = sock.recv(n - len(data))
        if not chunk:
            raise ConnectionError("connection closed by the server")
        data += chunk
    return data


def read_packet(sock: socket.socket):
    first = read_exact(sock, 1)[0]
    mult, length = 1, 0
    while True:
        b = read_exact(sock, 1)[0]
        length += (b & 0x7F) * mult
        mult *= 128
        if not b & 0x80:
            break
    return first, read_exact(sock, length)


def publish_qos1(sock: socket.socket, topic: str, payload: dict, pid: int):
    body = enc_str(topic) + struct.pack("!H", pid) + json.dumps(payload, separators=(",", ":")).encode()
    sock.sendall(bytes([0x32]) + enc_len(len(body)) + body)
    first, data = read_packet(sock)
    if first >> 4 != 4:
        raise RuntimeError(f"expected PUBACK, got packet type {first >> 4}")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--host", default=os.environ.get("TB_HOST", "79.76.56.250"))
    ap.add_argument("--port", type=int, default=int(os.environ.get("TB_PORT", "1883")))
    ap.add_argument("--sensor", type=int, default=4, help="sensor id -> device sensor-<id> (default 4)")
    ap.add_argument("--typ", type=int, default=0, help="event type: 0 test, 1/2 warning, 3 critical (default 0)")
    ap.add_argument("--counter", type=int, default=int(time.time()) % 256)
    ap.add_argument("--rssi", type=float, default=-91.0)
    ap.add_argument("--snr", type=float, default=9.2)
    args = ap.parse_args()

    token = os.environ.get("TB_GATEWAY_TOKEN")
    if not token:
        print("Set TB_GATEWAY_TOKEN to the access token of the gateway device 'lopy'", file=sys.stderr)
        return 2

    sock = socket.create_connection((args.host, args.port), timeout=10)
    try:
        # CONNECT, MQTT 3.1.1, username = token, clean session
        vh = enc_str("MQTT") + bytes([4, 0x82]) + struct.pack("!H", 30)
        body = vh + enc_str("lopy-test") + enc_str(token)
        sock.sendall(bytes([0x10]) + enc_len(len(body)) + body)
        first, data = read_packet(sock)
        if first >> 4 != 2:
            print("No CONNACK from the server", file=sys.stderr)
            return 1
        if data[1] != 0:
            print(f"Connection refused: {CONNACK_ERRORS.get(data[1], data[1])}", file=sys.stderr)
            return 1
        print(f"Connected to {args.host}:{args.port}")

        name = f"sensor-{args.sensor}"
        publish_qos1(sock, "v1/gateway/connect", {"device": name, "type": "Sensor"}, 1)
        print(f"Announced {name} (profile Sensor)")

        ts = int(time.time() * 1000)
        values = {"typ": args.typ, "licznik": args.counter, "rssi": args.rssi, "snr": args.snr}
        publish_qos1(sock, "v1/gateway/telemetry", {name: [{"ts": ts, "values": values}]}, 2)
        print(f"Published {values} for {name} at ts={ts}, acknowledged")

        sock.sendall(bytes([0xE0, 0x00]))  # DISCONNECT
    finally:
        sock.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
