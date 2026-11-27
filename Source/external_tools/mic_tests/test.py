from pathlib import Path
import numpy as np
import matplotlib.pyplot as plt
from scipy.signal import find_peaks
from pydub import AudioSegment

BASE_DIR = Path(__file__).resolve().parent
audio = AudioSegment.from_file(BASE_DIR / "combined_atv2.mp3").set_channels(1)

fs = audio.frame_rate
samples = np.array(audio.get_array_of_samples(), dtype=np.float32)
samples /= np.max(np.abs(samples))

# 250 ms snapshot at t = 7.0s
start = int(7.0 * fs)
win_len = int(0.25 * fs)
segment = samples[start:start + win_len] * np.hanning(win_len)

nfft = 65536
mag = np.abs(np.fft.rfft(segment, n=nfft))
freqs = np.fft.rfftfreq(nfft, d=1/fs)

# Log-Harmonic Product Spectrum (sum of log magnitudes = geometric mean in dB)
num_harmonics = 4
max_len = len(mag) // num_harmonics
log_hps = np.log10(mag[:max_len] + 1e-12)
for h in range(2, num_harmonics + 1):
    log_hps += np.log10(mag[::h][:max_len] + 1e-12)

hps_freqs = freqs[:max_len]

# Search 25 Hz to 95 Hz
mask = (hps_freqs >= 25) & (hps_freqs <= 95)
f_sub = hps_freqs[mask]
hps_db = 20 * (log_hps[mask] / num_harmonics)  # Average harmonic level in dB
hps_db -= np.max(hps_db)  # Normalize peak to 0 dB

# Detect peaks at least 8 Hz apart with >= 6 dB prominence above local floor
min_dist_bins = int(8.0 / (fs / nfft))
peaks, _ = find_peaks(hps_db, prominence=6.0, distance=min_dist_bins)

# Filter out 1.5x (3/2 subharmonic) or 2x octave aliases
true_f0s = []
for f in f_sub[peaks]:
    if not any(np.isclose(f, ratio * base_f, rtol=0.05) 
               for base_f in true_f0s for ratio in [1.5, 2.0]):
        true_f0s.append(f)

print(f"Detected {len(true_f0s)} distinct engines at t = 7.0s:")
for i, f0 in enumerate(true_f0s, 1):
    print(f"  engine #{i}: f₀ = {f0:.1f} Hz (~{f0 * 120:.0f} RPM)")

plt.figure(figsize=(10, 4))
plt.plot(f_sub, hps_db, label="Log-HPS (dB, normalized to 0 dB)")
plt.plot(true_f0s, [hps_db[np.argmin(np.abs(f_sub - f))] for f in true_f0s],
         "ro", markersize=9, label=f"Detected Fundamentals ({len(true_f0s)})")
plt.xlabel("Fundamental Firing Frequency f₀ [Hz]")
plt.ylabel("Relative Harmonic Strength [dB]")
plt.title(" Engine Separation via Log-Harmonic Product Spectrum (t = 7.0s)")
plt.legend()
plt.grid(True, alpha=0.3)
plt.tight_layout()
plt.savefig(BASE_DIR / "atv_log_hps2.png", dpi=150)