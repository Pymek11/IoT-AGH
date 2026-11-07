# Engine detector

Decides from the microphone blocks whether a vehicle engine (quad, cross, motorcycle) is audible
nearby. It does not tell the vehicle types apart, and it uses no machine learning: a few spectrum
and periodicity features per block, fixed thresholds and a decision over the last ~2 s.

Files: `Source/modules/detector/detector.h`, `Source/modules/detector/detector.cpp`,
FFT in `Source/utils/fft/fft.h`, `Source/utils/fft/fft.cpp`

The detector is plain C++ (no Arduino headers), so the same files can also be compiled on a PC
to run them on recordings.

## Why these features

Engine sound from a single-cylinder engine is a train of firing pulses: a 4-stroke at 6000 RPM
fires at 50 Hz, a 2-stroke at 100 Hz, idle is down to ~12 Hz. Most of the energy is in the
harmonics up to a few hundred Hz, which is also what travels furthest through a forest.
In the reference recordings 70–95 % of the energy is in 60–250 Hz.

Wind is also mostly low frequency, so loudness in the low band is not enough. What separates an
engine from wind is that it is **periodic** (repeats every firing period) and that it lasts for
seconds. Rain, birds and insects are mostly above 2 kHz and spectrally flat.

## Processing of one block

`process( samples )` takes one block of `DETECTOR_BLOCK_SIZE` (1024) signed 24-bit samples from
`Microphone::read()`, ~85 ms at 12 kHz.

1. **High-pass at 35 Hz** (biquad), normalizes to −1…1. Removes wind rumble below the firing
   frequencies of running engines.
2. **Spectrum:** Hann window, 1024-point FFT (11.7 Hz per bin). From the bin powers:
   - `lowBandDbfs` — RMS level of 40–400 Hz, dB re full scale
   - `lowRatio` — share of the energy (above 40 Hz) that is in 40–400 Hz
   - `highRatio` — share above 2 kHz
   - `flatness` — geometric / arithmetic mean of the bin powers in 40–2000 Hz:
     ~0.5+ for noise, close to 0 for a spectrum made of harmonics
3. **Periodicity:** the block is low-passed at 500 Hz (two biquads) and decimated by 8 to
   1.5 kHz. The last 3 decimated blocks (384 samples) are kept. A 256-sample window (~170 ms) is
   compared with the same signal shifted by every lag up to 1/12 Hz, as a normalized
   autocorrelation (−1…1). The peak after the first negative value, for pitches 12–300 Hz, is
   `periodicity`, and its lag gives `pitchHz` (the firing frequency or a multiple of it).
   Waiting for the first negative value matters: any smooth, low-passed signal such as wind
   correlates well with itself at very short lags, without being periodic.
4. **Noise floor and SNR:** the noise floor tracks `lowBandDbfs`. It follows a quieter level
   quickly (20 % of the gap per block), a louder one slowly (0.02 dB per block, ~10 dB in 40 s),
   and does not rise while a vehicle is detected. `snrDb` is the low band level above it.
5. **Block decision:** the block is engine sound (`engine`) when all of these pass:

   | Criterion | Constant | Value |
   |---|---|---|
   | low band level | `DETECTOR_MIN_LOW_BAND_DBFS` | ≥ −85 dBFS |
   | low band above noise floor | `DETECTOR_MIN_SNR_DB` | ≥ 6 dB |
   | periodicity | `DETECTOR_MIN_PERIODICITY` | ≥ 0.45 |
   | flatness | `DETECTOR_MAX_FLATNESS` | ≤ 0.35 |
   | low band share | `DETECTOR_MIN_LOW_RATIO` | ≥ 0.4 |
   | share above 2 kHz | `DETECTOR_MAX_HIGH_RATIO` | ≤ 0.25 |

6. **Detection over time:** of the last 24 blocks (~2 s), a vehicle is detected when at least 12
   are engine blocks, and is gone again when 4 or fewer are. The hysteresis stops the state from
   flickering while a vehicle changes gear or passes behind trees.

The first 2 blocks after `begin()` / `reset()` have no periodicity yet (the history is filling),
and the noise floor starts at the first block's level.

## API

```cpp
EngineDetector detector;

detector.begin();                         // filters, window, clears state
bool detected = detector.process( samples );
const DetectorFeatures &f = detector.features();   // features of the last block
detector.reset();                         // e.g. after the microphone was stopped
```

## Tuning

All thresholds are `constexpr` in `detector.h`. Built with `-DDETECTOR_DEBUG` (in `mode_flags` in
`platformio.ini`), `main.cpp` logs the features every 12 blocks (~1 s) at `DEBUG` level:

```
level=-62.3 floor=-78.0 snr=15.7 per=0.71 pitch=55 flat=0.04 low=0.93 high=0.01 E DETECTED
```

The current values were set on the 13 reference recordings in
`external_tools/mic_tests/quad sounds` and on synthetic wind, rain and bird noise.
With the engines on their own, every recording is detected and the detector is in the detected state
for 39–96 % of each file. The synthetic noise is never detected. With gusty wind (−50 dBFS) mixed in and the recordings
turned down, 13 of 13 are still detected at −20 dB, 12 at −30 dB and 9 at −40 dB.

These recordings are close-range and mostly AI-generated, so the thresholds are a starting
point. They should be re-checked with field recordings from the device's own microphone.
The first thing to look at is `DETECTOR_MIN_PERIODICITY`: synthetic wind reaches 0.40 in 10 % of
blocks, while engines have a median of 0.4–0.95.

Known limitation: a 2-stroke chainsaw or brush cutter is acoustically very close to a 2-stroke
bike and will be detected as a vehicle.

## Cost

Per block: one 1024-point FFT, ~165 `logf` calls and ~64k multiply-adds for the autocorrelation.
This is a few milliseconds at most on the ESP32, far below the 85 ms block period.
Memory: ~17 KB of buffers inside the `EngineDetector` object.
