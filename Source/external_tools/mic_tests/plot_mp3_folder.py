"""Plot frequency spectra for every MP3 file in a folder.

The script uses SoundFile to decode MP3 files.

Examples:
    python plot_mp3_folder.py "quad sounds"
    python plot_mp3_folder.py recordings
    python plot_mp3_folder.py recordings -o plots --max-frequency 5000
    python plot_mp3_folder.py recordings --recursive
"""

import argparse
from pathlib import Path

import matplotlib

matplotlib.use("Agg")

import matplotlib.pyplot as plt
import numpy as np
import soundfile as sf


def read_mp3(path: Path) -> tuple[np.ndarray, int]:
    """Decode an MP3 file and return mono samples normalized to [-1, 1]."""
    samples, sample_rate = sf.read(path, dtype="float64", always_2d=True)
    if samples.size == 0:
        raise ValueError("The MP3 file contains no audio")

    return np.mean(samples, axis=1), sample_rate


def plot_spectrum(
    input_path: Path,
    output_path: Path,
    max_frequency: float | None = None,
) -> None:
    """Create a PNG frequency-spectrum plot for one MP3 file."""
    samples, sample_rate = read_mp3(input_path)
    window = np.hanning(len(samples))
    spectrum = np.fft.rfft((samples - np.mean(samples)) * window)
    frequencies = np.fft.rfftfreq(len(samples), 1.0 / sample_rate)
    magnitude = 20.0 * np.log10(np.maximum(np.abs(spectrum), 1e-12))

    frequency_limit = sample_rate / 2
    if max_frequency is not None:
        frequency_limit = min(max_frequency, frequency_limit)
    visible = frequencies <= frequency_limit

    figure, axis = plt.subplots(figsize=(12, 6), constrained_layout=True)
    axis.plot(frequencies[visible], magnitude[visible], linewidth=0.8)
    axis.set_title(f"Frequency spectrum: {input_path.name}")
    axis.set_xlabel("Frequency [Hz]")
    axis.set_ylabel("Magnitude [dB]")
    axis.set_xlim(0, frequency_limit)
    axis.grid(True, alpha=0.3)
    figure.savefig(output_path, dpi=150)
    plt.close(figure)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument(
        "input",
        type=Path,
        nargs="?",
        default=Path("quad sounds"),
        help='Folder containing MP3 files (default: "quad sounds")',
    )
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        help="Folder for PNG files (default: a 'plots' folder inside input)",
    )
    parser.add_argument(
        "--max-frequency",
        type=float,
        help="Maximum frequency shown on each plot; defaults to Nyquist frequency",
    )
    parser.add_argument(
        "--recursive",
        action="store_true",
        help="Also search for MP3 files in subfolders",
    )
    args = parser.parse_args()

    if not args.input.is_dir():
        parser.error(f"Input folder does not exist: {args.input}")
    if args.max_frequency is not None and args.max_frequency <= 0:
        parser.error("--max-frequency must be greater than zero")

    search = args.input.rglob("*") if args.recursive else args.input.glob("*")
    input_files = sorted(
        path for path in search if path.is_file() and path.suffix.lower() == ".mp3"
    )
    if not input_files:
        parser.error(f"No MP3 files found in {args.input}")

    output_folder = args.output or args.input / "plots"
    output_folder.mkdir(parents=True, exist_ok=True)

    for input_path in input_files:
        relative_path = input_path.relative_to(args.input)
        output_path = output_folder / relative_path.with_suffix(".png")
        output_path.parent.mkdir(parents=True, exist_ok=True)
        plot_spectrum(input_path, output_path, args.max_frequency)
        print(f"Saved plot to {output_path}")


if __name__ == "__main__":
    main()
