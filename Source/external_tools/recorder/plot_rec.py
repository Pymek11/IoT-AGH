"""Plots a recording from rec_capture.py: waveform with the 801S state laid over it.

Example:
    python plot_rec.py rec_20261107_120000.wav
    python plot_rec.py rec_20261107_120000.wav --save plot.png
"""

import argparse
import csv
import wave
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("wav", help="rec_*.wav, the .csv next to it is read too")
    parser.add_argument("--save", help="save the plot to this file instead of showing it")
    args = parser.parse_args()

    wav_path = Path(args.wav)
    with wave.open(str(wav_path)) as wav:
        rate = wav.getframerate()
        audio = np.frombuffer(wav.readframes(wav.getnframes()), dtype="<i2") / 32768.0
    t_audio = np.arange(len(audio)) / rate

    times, states, triggers = [], [], []
    with open(wav_path.with_suffix(".csv"), newline="") as f:
        for row in csv.DictReader(f):
            times.append(float(row["t_s"]))
            states.append(int(row["state"]))
            if "trigger" in row.get("note", ""):
                triggers.append(float(row["t_s"]))

    fig, ax = plt.subplots(figsize=(14, 5))
    ax.plot(t_audio, audio, linewidth=0.4, color="tab:blue", label="audio")
    ax.set_ylim(-1.05, 1.05)
    ax.set_xlabel("time [s]")
    ax.set_ylabel("audio [full scale]")

    ax2 = ax.twinx()
    ax2.step(times, states, where="post", color="tab:red", linewidth=1.2, label="801S")
    ax2.set_ylim(-0.1, 3.0)     # keep the state line in the lower part of the plot
    ax2.set_yticks([0, 1])
    ax2.set_yticklabels(["LOW", "HIGH"])
    for t in triggers:
        ax.axvline(t, color="tab:green", linestyle="--", linewidth=1)

    ax.set_title(wav_path.name)
    fig.legend(loc="upper right")
    fig.tight_layout()
    if args.save:
        fig.savefig(args.save, dpi=120)
    else:
        plt.show()


if __name__ == "__main__":
    main()
