#ifndef MIC_H
#define MIC_H

#include <Arduino.h>
#include <ESP_I2S.h>

// Audio parameters, tune here if needed
constexpr uint32_t MIC_SAMPLE_RATE_HZ = 12000;
constexpr uint8_t MIC_BITS_PER_SAMPLE = 24;     // INMP441 resolution, sent in a 32-bit I2S slot
constexpr size_t MIC_BLOCK_SIZE = 1024;          // samples per block (~85 ms at 12 kHz)

// DC offset removal: time constant of 2^N samples (10 -> ~85 ms, cutoff ~2 Hz at 12 kHz)
// Well below the engine band, so it does not affect the useful signal
constexpr uint8_t MIC_DC_FILTER_SHIFT = 10;

// INMP441 I2S microphone (I2S driver and pins from config.h)
class Microphone
{
public:
    Microphone() = default;
    ~Microphone();

    // Not copyable, it owns the I2S driver
    Microphone( const Microphone & ) = delete;
    Microphone &operator=( const Microphone & ) = delete;

    // Starts the I2S driver
    // Returns true on success, false if the driver could not be started
    bool begin();

    // Stops the I2S driver, safe to call when not running
    void end();

    bool isRunning() const { return _running; }

    // Reads up to `count` samples into `buffer`, each a signed 24-bit value (-8388608..8388607)
    // stored in an int32_t, with the microphone's DC offset removed
    // `buffer` must have room for `count` samples
    // Returns number of samples actually read, 0 if not running
    size_t read( int32_t *buffer, size_t count = MIC_BLOCK_SIZE );

private:
    I2SClass _i2s;
    bool _running = false;

    // Running DC offset estimate, scaled by 2^MIC_DC_FILTER_SHIFT
    int64_t _dcAccumulator = 0;
    bool _dcPrimed = false;
};

#endif // MIC_H
