# LoRa -> ThingsBoard data flow

How a detection travels from a sensor node to the "Forest ATV Detection" dashboard.

```
 sensor node                 receiver / gateway                    ThingsBoard (79.76.56.250)
┌──────────────────┐  LoRa  ┌─────────────────────┐  Wi-Fi/MQTT  ┌──────────────────────────────┐
│ XIAO ESP32S3 +   │ 868.1  │ Pycom LoPy 1.0      │  port 1883   │ gateway device "lopy"        │
│ Wio-SX1262       │ ─────► │ (ESP32 + SX1272)    │ ───────────► │  └► device "sensor-<id>"     │
│ lora/node_xiao   │  3 B   │ lora/receiver_lopy  │ gateway API  │     profile "Sensor"         │
└──────────────────┘        └─────────────────────┘              │     rule chain "Sensors"     │
                                                                 │      ├ saves telemetry        │
                                                                 │      └ alarm "ATV detected"   │
                                                                 │ dashboard "Forest ATV         │
                                                                 │ Detection"                    │
                                                                 └──────────────────────────────┘
```

## LoRa packet (`lora/common/lora_link.h`)

Radio: LoRa P2P (not LoRaWAN), 868.1 MHz, BW 125 kHz, SF7, CR 4/5, sync word 0x12, preamble 8, 14 dBm.
Both firmwares include the same header, so the parameters cannot drift apart.

| Byte | Meaning | ThingsBoard key |
|---|---|---|
| 0 | sensor id (1..254) | device name `sensor-<id>` |
| 1 | event type | `typ` |
| 2 | counter (wraps at 255) | `licznik` |

Event types: `0` test/heartbeat (no alarm), `1` / `2` single signal (WARNING alarm), `3` vibration and
sound together (CRITICAL alarm). The receiver adds `rssi` and `snr` of the received packet.

## What the receiver does (`lora/receiver_lopy/src/main.cpp`)

1. Receives a packet (interrupt on DIO0), validates length and CRC.
2. Puts it in a 32-packet queue with a timestamp (SNTP over Wi-Fi), so nothing is lost while Wi-Fi or
   the server is down. The queue drops the oldest packet when full and counts it.
3. Once MQTT is connected, announces the sensor with `v1/gateway/connect`
   (`{"device":"sensor-4","type":"Sensor"}`; creates the device with the Sensor profile if missing)
   and publishes `v1/gateway/telemetry`:
   `{"sensor-4":[{"ts":...,"values":{"typ":3,"licznik":7,"rssi":-91.0,"snr":9.2}}]}`.
4. Every 60 s publishes its own health to the gateway device (`odebrane`, `odrzucone`, `w_kolejce`,
   `wifi_rssi`, `uptime_s`).

MQTT socket timeout is 3 s, so a dead server cannot stall radio reception for long.

## ThingsBoard configuration (already present on the server)

- Gateway device `lopy` (ACCESS_TOKEN credentials, "Is gateway" enabled).
- Device profile `Sensor` with rule chain `Sensors` and alarm rules: `typ` 1 or 2 -> WARNING,
  `typ` 3 -> CRITICAL, alarm type "ATV detected". The rule chain mirrors the alarm into the
  `alarmActive` / `alarmSeverity` attributes that the dashboard shows.
- Device `sensor-4` (latitude/longitude, location name, inactivity timeout attributes).
- Dashboard "Forest ATV Detection": map, sensor table, alarm history, detections over time, status,
  last event, battery, LoRa link quality (RSSI/SNR), event history.

A new sensor needs no server work: the first packet from a new id creates `sensor-<id>` with the Sensor
profile. Set its location attributes afterwards (dashboard -> sensor -> "Sensor info").

## Setup

Receiver:

```bash
cp lora/receiver_lopy/include/secrets.example.h lora/receiver_lopy/include/secrets.h
# edit secrets.h: Wi-Fi name/password (2.4 GHz) and the access token of the gateway device "lopy"
pio run -d lora/receiver_lopy -t upload     # bridge G23 to GND first, remove it and reset afterwards
pio device monitor -d lora/receiver_lopy
```

Node (set the sensor id with `-DSENSOR_ID=<n>` in `lora/node_xiao/platformio.ini`):

```bash
pio run -d lora/node_xiao -t upload
```

`secrets.h` is git-ignored. Do not commit tokens or Wi-Fi passwords.

## Testing without hardware

`tools/thingsboard/gateway_publish_test.py` does exactly what the receiver does, from your PC:

```bash
export TB_GATEWAY_TOKEN=<access token of the gateway device "lopy">
python3 tools/thingsboard/gateway_publish_test.py                    # typ 0, no alarm
python3 tools/thingsboard/gateway_publish_test.py --sensor 4 --typ 3 # CRITICAL alarm on sensor-4
```

Then check the dashboard: Sensor 4 becomes Online, a new row appears in Event history, and typ 3 raises
the alarm (clear it in Alarm history when done).

## Troubleshooting

| Symptom | Cause |
|---|---|
| Receiver prints `radio.begin() error: -2` | SX1272 not found: SPI pins or the board is not a LoPy 1.0 |
| `Packet rejected (code -7 ...)` | CRC mismatch: weak signal or different radio parameters |
| Nothing received, no errors | Frequency / SF / sync word differ, antenna missing, or the node is not transmitting |
| MQTT `state=5` (or 4) | Wrong gateway access token |
| MQTT `state=-2` | Server unreachable: check Wi-Fi, host and port 1883 |
| Packets arrive but no `sensor-<id>` data | Gateway device is not marked as a gateway, or a typo in the token of another device |
| Sensor shows Offline | No data within the sensor's `inactivityTimeout` (default 1 h) |

## Status

Verified: server reachable, MQTT broker rejects a bad token, device profile -> rule chain -> alarm ->
attributes chain (tested with a temporary device through the REST API, deleted afterwards), dashboard renders.
Not verified (no hardware or toolchain here): the firmware has not been compiled or flashed, and the LoRa link
and gateway MQTT publish were not exercised end to end.
