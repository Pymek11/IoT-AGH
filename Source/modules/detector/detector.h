#ifndef DETECTOR_H
#define DETECTOR_H

#include <cstddef>
#include <cstdint>

#include "../../utils/filter/filter.h"

// Plain C++ (no Arduino headers), so the same code can also be compiled on a PC for tuning

// Must match MIC_SAMPLE_RATE_HZ and MIC_BLOCK_SIZE in modules/microphone/mic.h (checked in main.cpp)
constexpr uint32_t DETECTOR_SAMPLE_RATE_HZ = 12000;
constexpr size_t DETECTOR_BLOCK_SIZE = 1024;

// Removes wind rumble below the lowest engine firing frequencies
constexpr float DETECTOR_HIGH_PASS_HZ = 35.0f;

// Spectrum bands (FFT of one block, 11.7 Hz per bin)
constexpr float DETECTOR_LOW_BAND_START_HZ = 40.0f;     // engine fundamentals and first harmonics
constexpr float DETECTOR_LOW_BAND_END_HZ = 400.0f;
constexpr float DETECTOR_FLATNESS_END_HZ = 2000.0f;     // flatness is measured over 40 .. 2000 Hz
constexpr float DETECTOR_HIGH_BAND_START_HZ = 2000.0f;  // birds, rain, insects

// Periodicity: normalized autocorrelation of the signal decimated to 1.5 kHz
constexpr size_t DETECTOR_DECIMATION = 8;
constexpr float DETECTOR_DECIMATION_LOW_PASS_HZ = 500.0f;
constexpr float DETECTOR_PITCH_MIN_HZ = 12.0f;          // single-cylinder 4-stroke at idle
constexpr float DETECTOR_PITCH_MAX_HZ = 300.0f;
constexpr size_t DETECTOR_HISTORY_BLOCKS = 3;           // decimated blocks kept for the autocorrelation
constexpr size_t DETECTOR_PERIODICITY_WINDOW = 256;     // decimated samples compared at each lag (~170 ms)

// A block counts as engine sound only if every criterion passes
constexpr float DETECTOR_MIN_LOW_BAND_DBFS = -85.0f;    // absolute level, just above the mic self-noise
constexpr float DETECTOR_MIN_SNR_DB = 6.0f;             // low band above the ambient noise floor
constexpr float DETECTOR_MIN_PERIODICITY = 0.45f;
constexpr float DETECTOR_MAX_FLATNESS = 0.35f;
constexpr float DETECTOR_MIN_LOW_RATIO = 0.4f;
constexpr float DETECTOR_MAX_HIGH_RATIO = 0.25f;

// Decision over the last blocks (24 blocks ~ 2 s), with hysteresis
constexpr uint8_t DETECTOR_DECISION_BLOCKS = 24;
constexpr uint8_t DETECTOR_ON_BLOCKS = 12;
constexpr uint8_t DETECTOR_OFF_BLOCKS = 4;

// Ambient noise floor of the low band: follows quiet quickly, loud slowly, held while detected
constexpr float DETECTOR_FLOOR_FALL = 0.2f;             // fraction of the gap closed per block
constexpr float DETECTOR_FLOOR_RISE_DB = 0.02f;         // dB per block (~10 dB in 40 s)

// Features of the last processed block
struct DetectorFeatures
{
    float lowBandDbfs = 0.0f;       // RMS level of 40 .. 400 Hz, dB re full scale
    float noiseFloorDbfs = 0.0f;
    float snrDb = 0.0f;
    float periodicity = 0.0f;       // 0 .. 1, autocorrelation peak
    float pitchHz = 0.0f;           // period of that peak, firing frequency or a multiple of it
    float flatness = 1.0f;          // 0 (peaky) .. 1 (white noise)
    float lowRatio = 0.0f;          // share of energy in the low band
    float highRatio = 0.0f;         // share of energy above 2 kHz
    bool engine = false;            // every criterion passed
};

// Detects that a vehicle engine (quad, cross, motorcycle) is audible
// Feed it consecutive blocks from Microphone::read()
class EngineDetector
{
public:
    // Sets up the filters and the window and clears all state
    void begin();

    // Forgets the history and the noise floor, e.g. after the microphone was stopped
    void reset();

    // Processes one block of DETECTOR_BLOCK_SIZE signed 24-bit samples
    // Returns true while a vehicle is detected
    bool process( const int32_t *samples );

    bool isDetected() const { return _detected; }
    const DetectorFeatures &features() const { return _features; }

private:
    void analyzeSpectrum();
    void analyzePeriodicity();
    void updateDecision();
    void updateNoiseFloor();

    BiquadFilter _highPass;
    BiquadFilter _decimationLowPass[2];

    float _block[DETECTOR_BLOCK_SIZE];      // high-passed samples, -1 .. 1
    float _window[DETECTOR_BLOCK_SIZE];
    float _windowPower = 0.0f;
    float _real[DETECTOR_BLOCK_SIZE];
    float _imag[DETECTOR_BLOCK_SIZE];

    float _history[DETECTOR_HISTORY_BLOCKS * DETECTOR_BLOCK_SIZE / DETECTOR_DECIMATION];
    size_t _historyBlocks = 0;

    float _noiseFloorDbfs = 0.0f;
    bool _floorPrimed = false;

    uint32_t _decisions = 0;                // bit per block, newest in bit 0
    bool _detected = false;

    DetectorFeatures _features;
};

#endif // DETECTOR_H
