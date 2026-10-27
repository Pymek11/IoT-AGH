"""Plot a microphone WAV file in the time and frequency domains.

Example:
    python plot_wav.py filtered.wav
    python plot_wav.py filtered.wav -o filtered.png
"""

import argparse
import wave
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np


HIGH_PASS_HZ = 200.0
LOW_PASS_HZ = 1200.0


def read_wav(path):
    with wave.open(str(path), "rb") as wav:
        channels = wav.getnchannels()
        sample_width = wav.getsampwidth()
        sample_rate = wav.getframerate()
        frame_count = wav.getnframes()
        raw = wav.readframes(frame_count)

    if channels != 1:
        raise ValueError(f"Expected mono WAV, got {channels} channels")
    if sample_width != 3:
        raise ValueError(f"Expected 24-bit WAV, got {sample_width * 8}-bit audio")

    packed = np.frombuffer(raw, dtype=np.uint8).reshape(-1, 3)
    unsigned = (
        packed[:, 0].astype(np.int32)
        | (packed[:, 1].astype(np.int32) << 8)
        | (packed[:, 2].astype(np.int32) << 16)
    )
    samples = np.where(
        unsigned & 0x800000,
        unsigned - 0x1000000,
        unsigned,
    )
    return samples.astype(np.float64) / 8388608.0, sample_rate


def plot_wav(path, output_path=None):
    samples, sample_rate = read_wav(path)
    if len(samples) == 0:
        raise ValueError("The WAV file contains no samples")

    time = np.arange(len(samples)) / sample_rate
    window = np.hanning(len(samples))
    spectrum = np.fft.rfft(samples * window)
    frequencies = np.fft.rfftfreq(len(samples), 1.0 / sample_rate)
    magnitude = 20.0 * np.log10(np.maximum(np.abs(spectrum), 1e-12))

    figure, (waveform_axis, spectrum_axis, low_frequency_axis) = plt.subplots(
        3, 1, figsize=(12, 11), constrained_layout=True
    )

    waveform_axis.plot(time, samples, linewidth=0.7)
    waveform_axis.set_title(f"Waveform: {path.name}")
    waveform_axis.set_xlabel("Time [s]")
    waveform_axis.set_ylabel("Amplitude")
    waveform_axis.grid(True, alpha=0.3)

    spectrum_axis.plot(frequencies, magnitude, linewidth=0.8)
    spectrum_axis.axvline(
        HIGH_PASS_HZ,
        color="tab:orange",
        linestyle="--",
        label=f"High-pass cutoff: {HIGH_PASS_HZ:g} Hz",
    )
    spectrum_axis.axvline(
        LOW_PASS_HZ,
        color="tab:red",
        linestyle="--",
        label=f"Low-pass cutoff: {LOW_PASS_HZ:g} Hz",
    )
    spectrum_axis.axvspan(0, HIGH_PASS_HZ, color="tab:orange", alpha=0.12)
    spectrum_axis.axvspan(
        LOW_PASS_HZ,
        sample_rate / 2,
        color="tab:red",
        alpha=0.12,
    )
    spectrum_axis.set_xlim(0, sample_rate / 2)
    spectrum_axis.set_title("Magnitude spectrum")
    spectrum_axis.set_xlabel("Frequency [Hz]")
    spectrum_axis.set_ylabel("Magnitude [dB]")
    spectrum_axis.grid(True, alpha=0.3)
    spectrum_axis.legend()

    low_frequency_limit = min(200.0, sample_rate / 2)
    low_frequency_mask = frequencies <= low_frequency_limit
    low_frequency_axis.plot(
        frequencies[low_frequency_mask],
        magnitude[low_frequency_mask],
        linewidth=0.8,
        label="Measured WAV spectrum",
    )
    low_frequency_axis.axvline(
        HIGH_PASS_HZ,
        color="tab:orange",
        linestyle="--",
        label=f"High-pass cutoff: {HIGH_PASS_HZ:g} Hz",
    )
    low_frequency_axis.axvspan(
        0,
        HIGH_PASS_HZ,
        color="tab:orange",
        alpha=0.15,
        label="High-pass stopband",
    )
    low_frequency_axis.set_xlim(0, low_frequency_limit)
    low_frequency_axis.set_title("Low-frequency zoom")
    low_frequency_axis.set_xlabel("Frequency [Hz]")
    low_frequency_axis.set_ylabel("Magnitude [dB]")
    low_frequency_axis.grid(True, alpha=0.3)
    low_frequency_axis.legend()

    if output_path is None:
        plt.show()
    else:
        figure.savefig(output_path, dpi=150)
        print(f"Saved plot to {output_path}")


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("input", type=Path, help="24-bit mono WAV file")
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        help="PNG output path; if omitted, open an interactive window",
    )
    args = parser.parse_args()
    plot_wav(args.input, args.output)


if __name__ == "__main__":
    main()
