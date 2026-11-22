#ifndef SOUND_DETECTOR_H
#define SOUND_DETECTOR_H

#include <stddef.h>
#include <stdint.h>

#include "../../../Source/utils/filter/filter.h"

// Level / spectrum analysis of one block of microphone samples (signed 24-bit in int32_t).
// Same processing as the standalone microphone firmware in Source/main.cpp:
// 200 Hz high-pass + 1200 Hz low-pass, Hamming window, 1024-point FFT, engine band 50-1200 Hz.
struct SoundStats
{
    float rawRms = 0.0f;        // RMS of the unfiltered block, 0..1 of full scale
    float filteredRms = 0.0f;   // RMS after the band filters, 0..1 of full scale
    float engineRatio = 0.0f;   // share of the spectrum energy inside 50-1200 Hz (0 when not analysed)
    float strongestHz = 0.0f;   // strongest bin in 30-220 Hz, the engine firing frequency range
};

class SoundDetector
{
public:
    // Configures the filters for MIC_SAMPLE_RATE_HZ
    void begin();

    // Clears the filter state, call before every new listening window
    void reset();

    // Processes one block of MIC_BLOCK_SIZE samples.
    // analyzeSpectrum = false only runs the filters (used while the microphone settles), which is cheaper.
    SoundStats process( const int32_t *samples, size_t count, bool analyzeSpectrum );

private:
    BiquadFilter _highPass;
    BiquadFilter _lowPass;
};

#endif // SOUND_DETECTOR_H
