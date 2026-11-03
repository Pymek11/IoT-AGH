#ifndef MIC_STREAM_H
#define MIC_STREAM_H

#include <Arduino.h>

#include "mic.h"
#include "../../utils/filter/filter.h"

// Debug mode that records the microphone on a PC (external_tools/mic_tests/capture.py)
// -DMIC_STREAM sends the filtered blocks, -DMIC_DUAL_STREAM sends each raw block followed by its
// filtered copy. In both modes the serial port carries only binary data, so logging is turned off
#if defined( MIC_STREAM ) || defined( MIC_DUAL_STREAM )
#define MIC_STREAMING
#endif

// Filters of the streamed "filtered" signal
constexpr float MIC_STREAM_HIGH_PASS_HZ = 35.0f;    // same as DETECTOR_HIGH_PASS_HZ
constexpr float MIC_STREAM_LOW_PASS_HZ = 1500.0f;

class MicStream
{
public:
    // Turns logging off and sends the "MICSTREAM" marker the capture script waits for
    void begin();

    // Sends one block of MIC_BLOCK_SIZE samples from Microphone::read()
    void send( const int32_t *samples );

private:
    BiquadFilter _highPass;
    BiquadFilter _lowPass;
    int32_t _filtered[MIC_BLOCK_SIZE];
};

#endif // MIC_STREAM_H
