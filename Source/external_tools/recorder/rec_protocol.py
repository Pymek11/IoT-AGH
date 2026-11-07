"""Frame parser for the recorder firmware (modules/recorder/rec_protocol.h).

Frame: 0xA5 0x5A | type u8 | length u16 LE | payload | crc16 u16 LE
crc16 = CRC-16/CCITT-FALSE over type, length and payload.
Bytes that are not part of a valid frame (log text, garbage) are returned as text.
"""

import struct
from dataclasses import dataclass

SYNC = b"\xA5\x5A"
HEADER_LEN = 5      # sync (2) + type (1) + length (2)
CRC_LEN = 2
MAX_PAYLOAD = 8192  # anything longer is a false sync

T_START = 0x10
T_STOP = 0x11
T_ARMED = 0x12
T_AUDIO = 0x20
T_VIB = 0x30

MODE_CONTINUOUS = 0
MODE_TRIGGERED = 1

AUDIO_FLAG_VIB_SEEN = 0x01
VIB_FLAG_TRIGGER = 0x01
VIB_FLAG_POLLED = 0x02


def _crc_table():
    table = []
    for byte in range(256):
        crc = byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) if crc & 0x8000 else (crc << 1)
        table.append(crc & 0xFFFF)
    return table


_CRC_TABLE = _crc_table()


def crc16(data, crc=0xFFFF):
    """CRC-16/CCITT-FALSE, same as crc16Update() in the firmware."""
    for byte in data:
        crc = ((crc << 8) & 0xFFFF) ^ _CRC_TABLE[(crc >> 8) ^ byte]
    return crc


@dataclass
class Start:
    sample_rate: int
    bits: int
    block_size: int
    vib_level: int
    vib_active_high: int
    mode: int
    pre_trigger_samples: int


@dataclass
class Stop:
    total_samples: int
    dropped_vib_events: int


@dataclass
class Armed:
    armed: int
    mode: int


@dataclass
class Audio:
    first_sample: int
    vib_level: int
    flags: int
    peak: int
    clipped: int
    pcm: bytes          # int16 LE samples


@dataclass
class Vib:
    sample_index: int
    level: int
    flags: int


def decode(frame_type, payload):
    """Turns a payload into a message object, None for unknown types."""
    if frame_type == T_START:
        return Start(*struct.unpack("<IBHBBBI", payload[:14]))
    if frame_type == T_STOP:
        return Stop(*struct.unpack("<II", payload[:8]))
    if frame_type == T_ARMED:
        return Armed(*struct.unpack("<BB", payload[:2]))
    if frame_type == T_AUDIO:
        first, level, flags, peak, clipped = struct.unpack("<IBBHH", payload[:10])
        return Audio(first, level, flags, peak, clipped, bytes(payload[10:]))
    if frame_type == T_VIB:
        return Vib(*struct.unpack("<IBB", payload[:6]))
    return None


class FrameParser:
    """Feed bytes with feed(), get back (messages, text, crc_errors)."""

    def __init__(self):
        self._buf = bytearray()
        self.crc_errors = 0

    def feed(self, data):
        self._buf += data
        messages = []
        text = bytearray()
        buf = self._buf
        while True:
            index = buf.find(SYNC)
            if index < 0:
                # Keep a trailing 0xA5, it may be the start of the next sync
                keep = 1 if buf.endswith(SYNC[:1]) else 0
                text += buf[:len(buf) - keep]
                del buf[:len(buf) - keep]
                break
            text += buf[:index]
            del buf[:index]
            if len(buf) < HEADER_LEN:
                break
            frame_type = buf[2]
            length = buf[3] | (buf[4] << 8)
            if length > MAX_PAYLOAD:
                text += buf[:1]
                del buf[:1]
                continue
            total = HEADER_LEN + length + CRC_LEN
            if len(buf) < total:
                break
            body = bytes(buf[2:HEADER_LEN + length])
            crc = buf[HEADER_LEN + length] | (buf[HEADER_LEN + length + 1] << 8)
            if crc16(body) != crc:
                # False sync inside text or a damaged frame: skip one byte and look again
                self.crc_errors += 1
                text += buf[:1]
                del buf[:1]
                continue
            message = decode(frame_type, body[3:])
            if message is not None:
                messages.append(message)
            del buf[:total]
        return messages, bytes(text)
