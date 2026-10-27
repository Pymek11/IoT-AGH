"""Records microphone samples streamed by the ESP32 and saves 24-bit mono WAV files.

With --dual, the firmware must be built with -DMIC_DUAL_STREAM. It sends the line
"MICSTREAM", then one raw block followed by its filtered block.

Close the PlatformIO serial monitor first, only one program can use the port.

Example:
    python capture.py --seconds 10 -o engine.wav
    python capture.py COM3 --dual --seconds 10 -o engine.wav
    python capture.py /dev/cu.usbserial-0001 --seconds 5
"""

import argparse
import sys
import time
import wave
from array import array
from datetime import datetime
from pathlib import Path

import serial
from serial.tools import list_ports

MARKER = b"MICSTREAM\n"
BYTES_PER_SAMPLE = 4        # int32 on the wire
WAV_BYTES_PER_SAMPLE = 3    # 24-bit in the file

# Keep in sync with MIC_SAMPLE_RATE_HZ in modules/microphone/mic.h and serial_baud in platformio.ini
DEFAULT_RATE_HZ = 12000
DEFAULT_BAUD = 921600
DUAL_BLOCK_SAMPLES = 1024


def find_port():
    """Returns the only USB serial port found, exits if there are none or several."""
    ports = [p.device for p in list_ports.comports()
             if any(key in p.device for key in ("usbserial", "usbmodem", "ttyUSB", "ttyACM", "COM"))]
    if len(ports) == 1:
        return ports[0]
    if not ports:
        sys.exit("No USB serial port found, connect the board or pass the port name")
    sys.exit("Several serial ports found, pass one of them: " + ", ".join(ports))


def reset_board(ser):
    """Pulses EN through RTS so the firmware starts over and prints the marker."""
    try:
        ser.dtr = False
        ser.rts = True
        time.sleep(0.1)
        ser.rts = False
    except (OSError, serial.SerialException):
        pass    # port without control lines, the board is probably already streaming


def wait_for_marker(ser, timeout_s):
    """Reads until the marker, returns the bytes that came right after it."""
    buffer = bytearray()
    deadline = time.monotonic() + timeout_s
    while time.monotonic() < deadline:
        buffer += ser.read(max(ser.in_waiting, 1))
        index = buffer.find(MARKER)
        if index != -1:
            return bytes(buffer[index + len(MARKER):])
    sys.exit(f"No {MARKER.strip().decode()} marker in {timeout_s} s. Is the firmware built with "
             "-DMIC_STREAM, and is the baud rate the same as serial_baud in platformio.ini?")


def read_samples(ser, first_bytes, seconds, rate_hz):
    """Reads `seconds` of samples, returns (raw bytes, elapsed seconds)."""
    wanted = int(seconds * rate_hz) * BYTES_PER_SAMPLE
    data = bytearray(first_bytes[:wanted])
    start = time.monotonic()
    while len(data) < wanted:
        data += ser.read(min(max(ser.in_waiting, 1), wanted - len(data)))
        print(f"\rRecording: {len(data) / wanted * 100:5.1f}%", end="", flush=True)
    print()
    return bytes(data), time.monotonic() - start


def read_dual_samples(ser, first_bytes, seconds, rate_hz):
    """Reads block-paired raw and filtered samples from the serial stream."""
    wanted_samples = int(seconds * rate_hz)
    block_count = (wanted_samples + DUAL_BLOCK_SAMPLES - 1) // DUAL_BLOCK_SAMPLES
    block_bytes = DUAL_BLOCK_SAMPLES * BYTES_PER_SAMPLE
    wanted_bytes = block_count * block_bytes * 2
    data = bytearray(first_bytes[:wanted_bytes])
    start = time.monotonic()
    while len(data) < wanted_bytes:
        data += ser.read(min(max(ser.in_waiting, 1), wanted_bytes - len(data)))
        print(f"\rRecording: {len(data) / wanted_bytes * 100:5.1f}%", end="", flush=True)
    print()

    raw = bytearray()
    filtered = bytearray()
    for offset in range(0, wanted_bytes, block_bytes * 2):
        raw += data[offset:offset + block_bytes]
        filtered += data[offset + block_bytes:offset + block_bytes * 2]
    sample_bytes = wanted_samples * BYTES_PER_SAMPLE
    return bytes(raw[:sample_bytes]), bytes(filtered[:sample_bytes]), time.monotonic() - start


def save_wav(path, raw, rate_hz):
    """Converts int32 samples to packed 24-bit and writes them as a mono wav."""
    # Samples are sign extended, so the lowest 3 bytes of each one are the 24-bit value
    packed = bytearray(len(raw) // BYTES_PER_SAMPLE * WAV_BYTES_PER_SAMPLE)
    for byte in range(WAV_BYTES_PER_SAMPLE):
        packed[byte::WAV_BYTES_PER_SAMPLE] = raw[byte::BYTES_PER_SAMPLE]

    with wave.open(str(path), "wb") as wav:
        wav.setnchannels(1)
        wav.setsampwidth(WAV_BYTES_PER_SAMPLE)
        wav.setframerate(rate_hz)
        wav.writeframes(bytes(packed))


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("port", nargs="?", help="serial port, found automatically if only one exists")
    parser.add_argument("-b", "--baud", type=int, default=DEFAULT_BAUD)
    parser.add_argument("-s", "--seconds", type=float, default=10, help="length of the recording")
    parser.add_argument("-r", "--rate", type=int, default=DEFAULT_RATE_HZ,
                        help="sample rate of the firmware (MIC_SAMPLE_RATE_HZ)")
    parser.add_argument("-o", "--output", help="output file, default mic_<date>_<time>.wav")
    parser.add_argument("--no-reset", action="store_true",
                        help="do not reset the board, wait for the marker as is")
    parser.add_argument("--marker-timeout", type=float, default=10)
    parser.add_argument("--dual", action="store_true",
                        help="read raw and filtered blocks from MIC_DUAL_STREAM firmware")
    args = parser.parse_args()

    if sys.byteorder != "little":
        sys.exit("This script expects a little-endian computer")

    port = args.port or find_port()
    output = args.output or datetime.now().strftime("mic_%Y%m%d_%H%M%S.wav")

    with serial.Serial(port, args.baud, timeout=1) as ser:
        print(f"Port {port} at {args.baud} baud")
        ser.reset_input_buffer()    # drop old data before the board starts over
        if not args.no_reset:
            reset_board(ser)

        print("Waiting for the stream...")
        first_bytes = wait_for_marker(ser, args.marker_timeout)
        if args.dual:
            raw, filtered, elapsed = read_dual_samples(
                ser, first_bytes, args.seconds, args.rate
            )
        else:
            raw, elapsed = read_samples(ser, first_bytes, args.seconds, args.rate)

    raw = raw[:len(raw) // BYTES_PER_SAMPLE * BYTES_PER_SAMPLE]
    samples = array("i")
    samples.frombytes(raw)
    if not samples:
        sys.exit("No samples received")

    if args.dual:
        output_path = Path(output)
        raw_output = output_path.with_name(
            output_path.stem + "_raw" + output_path.suffix
        )
        filtered_output = output_path.with_name(
            output_path.stem + "_filtered" + output_path.suffix
        )
        save_wav(raw_output, raw, args.rate)
        save_wav(filtered_output, filtered, args.rate)
        print(f"Saved raw: {raw_output}")
        print(f"Saved filtered: {filtered_output}")
    else:
        save_wav(output, raw, args.rate)

    # Time the data took to arrive should match the sample rate, a difference means lost samples
    # (or a rate different from the firmware's)
    actual_rate = len(samples) / elapsed if elapsed > 0 else 0
    if not args.dual:
        print(f"Saved {len(samples)} samples ({len(samples) / args.rate:.2f} s) to {output}")
    print(f"Min {min(samples)}, max {max(samples)} (24-bit range is +-8388608)")
    if elapsed > 0.5 and abs(actual_rate - args.rate) > 0.05 * args.rate:
        print(f"WARNING: samples arrived at about {actual_rate:.0f} Hz, expected {args.rate} Hz. "
              "Check --rate and whether the serial link keeps up.")


if __name__ == "__main__":
    main()
