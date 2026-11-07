#ifndef REC_UTIL_H
#define REC_UTIL_H

#include <Arduino.h>

// Maps esp_timer time (us) to an audio sample index
// After every block, (time the read returned - block end index / rate) is an estimate of the time
// of sample 0. A read can only return late (the DMA buffer was waiting), never early, so the
// smallest estimate of the last few seconds is the best one. The error stays around one I2S DMA
// buffer (a few ms), which is far below what the 801S can resolve
class SampleClock
{
public:
    explicit SampleClock( uint32_t sampleRate ) : _rate( sampleRate ) {}

    void reset();

    // Call right after a block was read. `endIndex` = samples read so far, including this block
    void onBlock( uint32_t endIndex, int64_t nowUs );

    // Sample index at time `timeUs`, 0 before the first block
    uint32_t indexAt( int64_t timeUs ) const;

private:
    static constexpr size_t WINDOW = 32;    // blocks, ~2.7 s at 12 kHz / 1024

    uint32_t _rate;
    int64_t _origins[WINDOW];
    size_t _count = 0;
    size_t _next = 0;
    int64_t _originUs = 0;
};

struct PcmLevels
{
    uint16_t peak;      // largest |sample| in the block (16-bit)
    uint16_t clipped;   // samples that hit the 16-bit limit
};

// Converts signed 24-bit samples (from Microphone::read) to 16-bit PCM: shift right by
// REC_PCM_SHIFT, saturate, and measure the block level
PcmLevels toPcm16( const int32_t *in, int16_t *out, size_t count );

enum class RecCommand
{
    NONE,
    START,
    STOP,
};

// Reads text commands ("START", "STOP", one per line, any case) from Serial without blocking
class CommandReader
{
public:
    RecCommand poll();

private:
    char _line[16];
    size_t _length = 0;
    bool _overflow = false;
};

#endif // REC_UTIL_H
