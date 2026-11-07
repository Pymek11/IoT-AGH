"""Records microphone audio and 801S vibration events from the MIC_REC firmware.

Sends START to the board, saves everything until Ctrl+C (or --seconds), then sends STOP.
Every recording gives three files in --out:
    rec_YYYYMMDD_HHMMSS.wav          16-bit mono PCM
    rec_YYYYMMDD_HHMMSS.csv          801S changes: sample_index, t_s, state, note
    rec_YYYYMMDD_HHMMSS_blocks.csv   801S level and audio level for every audio block

Close the PlatformIO serial monitor first, only one program can use the port.

Examples:
    python rec_capture.py
    python rec_capture.py COM5 --seconds 30 --out recordings
"""

import argparse
import csv
import math
import sys
import time
import wave
from array import array
from datetime import datetime
from pathlib import Path

import serial
from serial.tools import list_ports

import rec_protocol as rp

DEFAULT_BAUD = 921600
QUIET_PEAK_DBFS = -30.0     # quieter than this -> suggest more gain
START_RETRY_S = 2.0         # resend START if the board did not answer (e.g. it was rebooting)
STOP_TIMEOUT_S = 3.0


def find_port():
    """Returns the only USB serial port found, exits if there are none or several."""
    ports = [p.device for p in list_ports.comports()
             if any(key in p.device for key in ("usbserial", "usbmodem", "ttyUSB", "ttyACM", "COM"))]
    if len(ports) == 1:
        return ports[0]
    if not ports:
        sys.exit("No USB serial port found, connect the board or pass the port name")
    sys.exit("Several serial ports found, pass one of them: " + ", ".join(ports))


def dbfs(value):
    return 20 * math.log10(value / 32768) if value > 0 else float("-inf")


class Recording:
    """One START..STOP: writes the WAV as audio arrives, the CSV files at the end."""

    def __init__(self, start, out_dir):
        self.start = start
        self.rate = start.sample_rate
        stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        base = out_dir / f"rec_{stamp}"
        suffix = 1
        while base.with_suffix(".wav").exists():
            base = out_dir / f"rec_{stamp}_{suffix}"
            suffix += 1
        self.base = base

        self.wav = wave.open(str(base.with_suffix(".wav")), "wb")
        self.wav.setnchannels(1)
        self.wav.setsampwidth(2)
        self.wav.setframerate(self.rate)

        self.written = 0            # samples in the WAV, including filled gaps
        self.gap_samples = 0
        self.events = []            # (sample_index, state, note)
        self.blocks = []            # (sample_index, state, vib_seen, peak, clipped)
        self.peak = 0
        self.clipped = 0
        self.sum_squares = 0.0
        self.state = start.vib_level

        note = "start"
        if start.mode == rp.MODE_TRIGGERED and start.pre_trigger_samples:
            note = f"start (pre-trigger {start.pre_trigger_samples / self.rate:.2f} s)"
        self.events.append((0, start.vib_level, note))

    def add_audio(self, audio):
        if audio.first_sample < self.written:
            return      # duplicate, already written
        if audio.first_sample > self.written:
            # A frame was lost (CRC error): keep the time axis right by filling silence
            gap = audio.first_sample - self.written
            self.wav.writeframes(bytes(gap * 2))
            self.gap_samples += gap
            self.written += gap

        samples = array("h")
        samples.frombytes(audio.pcm)
        if sys.byteorder != "little":
            samples.byteswap()
        self.wav.writeframes(audio.pcm)
        self.sum_squares += sum(s * s for s in samples)

        self.blocks.append((audio.first_sample, audio.vib_level,
                            int(bool(audio.flags & rp.AUDIO_FLAG_VIB_SEEN)), audio.peak, audio.clipped))
        self.written += len(samples)
        self.peak = max(self.peak, audio.peak)
        self.clipped += audio.clipped
        self.state = audio.vib_level

    def add_vib(self, vib):
        notes = []
        if vib.flags & rp.VIB_FLAG_TRIGGER:
            notes.append("trigger")
        notes.append("polled" if vib.flags & rp.VIB_FLAG_POLLED else "edge")
        self.events.append((vib.sample_index, vib.level, " ".join(notes)))
        self.state = vib.level

    def finish(self, stop):
        self.wav.close()
        end = self.written
        self.events.append((end, self.state, "end"))
        self.events.sort(key=lambda e: e[0])

        with open(self.base.with_suffix(".csv"), "w", newline="") as f:
            writer = csv.writer(f)
            writer.writerow(["sample_index", "t_s", "state", "note"])
            for index, state, note in self.events:
                writer.writerow([index, f"{index / self.rate:.6f}", state, note])

        with open(self.base.with_name(self.base.name + "_blocks.csv"), "w", newline="") as f:
            writer = csv.writer(f)
            writer.writerow(["sample_index", "t_s", "state", "vib_seen", "peak", "clipped"])
            for index, state, seen, peak, clipped in self.blocks:
                writer.writerow([index, f"{index / self.rate:.6f}", state, seen, peak, clipped])

        self.report(stop)

    def report(self, stop):
        seconds = self.written / self.rate
        rms = math.sqrt(self.sum_squares / self.written) if self.written else 0
        changes = sum(1 for e in self.events if e[2] not in ("end",) and not e[2].startswith("start"))
        print(f"\nSaved {self.base.with_suffix('.wav')} ({seconds:.2f} s), "
              f"{self.base.with_suffix('.csv').name}, {self.base.name}_blocks.csv")
        print(f"  801S changes: {changes}, active level is {'HIGH' if self.start.vib_active_high else 'LOW'}")
        print(f"  Level: peak {self.peak} ({dbfs(self.peak):.1f} dBFS), RMS {dbfs(rms):.1f} dBFS, "
              f"clipped samples {self.clipped}")
        if self.clipped:
            print("  WARNING: clipping - raise REC_PCM_SHIFT in config.h (less gain)")
        elif self.peak and dbfs(self.peak) < QUIET_PEAK_DBFS:
            print("  WARNING: quiet recording - lower REC_PCM_SHIFT in config.h (+6 dB per step)")
        if self.gap_samples:
            print(f"  WARNING: {self.gap_samples} samples lost on the link, filled with silence")
        if stop is None:
            print("  WARNING: no STOP frame from the board, the end of the recording may be cut")
        else:
            if stop.total_samples != self.written:
                print(f"  WARNING: board sent {stop.total_samples} samples, file has {self.written}")
            if stop.dropped_vib_events:
                print(f"  WARNING: {stop.dropped_vib_events} 801S edges dropped on the board "
                      "(queue full, raise VIB_DEBOUNCE_US or VIB_EVENT_QUEUE_SIZE)")


def print_text(text, pending):
    """Prints complete log lines from the board, returns the unfinished rest."""
    pending += text.decode("utf-8", "replace")
    *lines, pending = pending.split("\n")
    for line in lines:
        line = line.strip("\r")
        if line.strip():
            print(f"[board] {line}")
    return pending


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("port", nargs="?", help="serial port, found automatically if only one exists")
    parser.add_argument("-b", "--baud", type=int, default=DEFAULT_BAUD)
    parser.add_argument("-s", "--seconds", type=float, default=0,
                        help="stop after this many seconds (0 = until Ctrl+C)")
    parser.add_argument("-o", "--out", default=".", help="output folder")
    parser.add_argument("--no-start", action="store_true",
                        help="do not send START/STOP, only save what the board sends")
    args = parser.parse_args()

    out_dir = Path(args.out)
    out_dir.mkdir(parents=True, exist_ok=True)
    port = args.port or find_port()

    frames = rp.FrameParser()
    recording = None
    pending_text = ""
    last_start_sent = 0.0
    status_time = 0.0

    with serial.Serial(port, args.baud, timeout=0.05) as ser:
        print(f"Port {port} at {args.baud} baud, Ctrl+C to stop")

        def send(command):
            ser.write(command.encode() + b"\n")
            ser.flush()

        if not args.no_start:
            send("STOP")        # in case a previous run left the board recording
            send("START")
            last_start_sent = time.monotonic()

        try:
            while True:
                now = time.monotonic()
                data = ser.read(max(ser.in_waiting, 1))
                messages, text = frames.feed(data)
                if text:
                    pending_text = print_text(text, pending_text)

                for message in messages:
                    if isinstance(message, rp.Start):
                        if recording:
                            recording.finish(None)
                        recording = Recording(message, out_dir)
                        print(f"Recording to {recording.base.with_suffix('.wav')} "
                              f"({message.sample_rate} Hz, {message.bits}-bit)")
                    elif recording is None:
                        continue    # data from a recording this script did not see start
                    elif isinstance(message, rp.Audio):
                        recording.add_audio(message)
                    elif isinstance(message, rp.Vib):
                        recording.add_vib(message)
                    elif isinstance(message, rp.Stop):
                        recording.finish(message)
                        recording = None

                if recording and now - status_time > 0.5:
                    status_time = now
                    print(f"\r  {recording.written / recording.rate:7.1f} s  801S "
                          f"{'HIGH' if recording.state else 'LOW '}  peak {recording.peak:5d}  ",
                          end="", flush=True)

                # The board may have been rebooting when START was sent
                if not args.no_start and recording is None and now - last_start_sent > START_RETRY_S:
                    send("START")
                    last_start_sent = now

                if args.seconds and recording and recording.written >= args.seconds * recording.rate:
                    raise KeyboardInterrupt
        except KeyboardInterrupt:
            if not args.no_start:
                send("STOP")
                stop_requested_at = time.monotonic()
                # Keep reading until the STOP frame arrives
                try:
                    while time.monotonic() - stop_requested_at < STOP_TIMEOUT_S and recording:
                        messages, text = frames.feed(ser.read(max(ser.in_waiting, 1)))
                        for message in messages:
                            if isinstance(message, rp.Audio):
                                recording.add_audio(message)
                            elif isinstance(message, rp.Vib):
                                recording.add_vib(message)
                            elif isinstance(message, rp.Stop):
                                recording.finish(message)
                                recording = None
                                break
                except KeyboardInterrupt:
                    pass
        finally:
            if recording:
                recording.finish(None)
            if frames.crc_errors:
                print(f"{frames.crc_errors} bytes skipped after a bad CRC (damaged frame or noise)")


if __name__ == "__main__":
    main()
