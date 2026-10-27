#include <Arduino.h>
#include <cmath>

#include "utils/logger/logger.h"
#include "utils/filter/filter.h"
#include "modules/microphone/mic.h"

static Logger logger( "main" );
static Microphone microphone;

namespace
{
constexpr float kPi = 3.14159265358979323846f;
constexpr float SAMPLE_RATE = static_cast<float>( MIC_SAMPLE_RATE_HZ );
constexpr float MIN_ENGINE_HZ = 30.0f;
constexpr float MAX_ENGINE_HZ = 220.0f;
constexpr float ENGINE_LOW_HZ = 50.0f;
constexpr float ENGINE_HIGH_HZ = 1200.0f;
constexpr float MIN_AUDIO_RMS = 0.0015f;
constexpr float ENGINE_RMS = 0.0040f;
constexpr float ENGINE_BAND_RATIO = 0.42f;
constexpr float HIGH_PASS_CUTOFF_HZ = 200.0f;
constexpr float kTwoPi = 2.0f * kPi;

BiquadFilter highPass;
BiquadFilter lowPass;
float fftReal[MIC_BLOCK_SIZE];
float fftImag[MIC_BLOCK_SIZE];

void calculateFft()
{
    for ( size_t i = 1, j = 0; i < MIC_BLOCK_SIZE; i++ )
    {
        size_t bit = MIC_BLOCK_SIZE >> 1;
        for ( ; j & bit; bit >>= 1 )
        {
            j ^= bit;
        }
        j ^= bit;
        if ( i < j )
        {
            const float real = fftReal[i];
            fftReal[i] = fftReal[j];
            fftReal[j] = real;
        }
    }

    for ( size_t length = 2; length <= MIC_BLOCK_SIZE; length <<= 1 )
    {
        const float angle = -kTwoPi / static_cast<float>( length );
        const float stepReal = cosf( angle );
        const float stepImag = sinf( angle );

        for ( size_t start = 0; start < MIC_BLOCK_SIZE; start += length )
        {
            float currentReal = 1.0f;
            float currentImag = 0.0f;
            const size_t halfLength = length >> 1;
            for ( size_t offset = 0; offset < halfLength; offset++ )
            {
                const size_t even = start + offset;
                const size_t odd = even + halfLength;
                const float oddReal = currentReal * fftReal[odd] - currentImag * fftImag[odd];
                const float oddImag = currentReal * fftImag[odd] + currentImag * fftReal[odd];
                const float evenReal = fftReal[even];
                const float evenImag = fftImag[even];

                fftReal[even] = evenReal + oddReal;
                fftImag[even] = evenImag + oddImag;
                fftReal[odd] = evenReal - oddReal;
                fftImag[odd] = evenImag - oddImag;

                const float nextReal = currentReal * stepReal - currentImag * stepImag;
                currentImag = currentReal * stepImag + currentImag * stepReal;
                currentReal = nextReal;
            }
        }
    }
}

float binEnergy( size_t bin )
{
    return fftReal[bin] * fftReal[bin] + fftImag[bin] * fftImag[bin];
}

void applyFilters( int32_t *samples )
{
    for ( size_t i = 0; i < MIC_BLOCK_SIZE; i++ )
    {
        const float normalized = static_cast<float>( samples[i] ) / 8388608.0f;
        const float filtered = lowPass.process( highPass.process( normalized ) );
        const float limited = max( -1.0f, min( 1.0f, filtered ) );
        samples[i] = static_cast<int32_t>( limited * 8388607.0f );
    }
}

void processAudio( const int32_t *samples )
{
    int32_t minimum = INT32_MAX;
    int32_t maximum = INT32_MIN;
    int32_t peak = 0;
    double rawSum = 0.0;
    double filteredSum = 0.0;

    for ( size_t i = 0; i < MIC_BLOCK_SIZE; i++ )
    {
        minimum = min( minimum, samples[i] );
        maximum = max( maximum, samples[i] );
        peak = max( peak, abs( samples[i] ) );

        const float normalized = static_cast<float>( samples[i] ) / 8388608.0f;
        const float filtered = lowPass.process( highPass.process( normalized ) );
        const float window = 0.54f - 0.46f * cosf( kTwoPi * i / MIC_BLOCK_SIZE );
        fftReal[i] = filtered * window;
        fftImag[i] = 0.0f;
        rawSum += normalized * normalized;
        filteredSum += filtered * filtered;
    }

    const float rawRms = sqrtf( static_cast<float>( rawSum / MIC_BLOCK_SIZE ) );
    const float filteredRms = sqrtf( static_cast<float>( filteredSum / MIC_BLOCK_SIZE ) );
    const float peakPercent = 100.0f * static_cast<float>( peak ) / 8388607.0f;

    calculateFft();

    const float binWidth = SAMPLE_RATE / MIC_BLOCK_SIZE;
    float totalEnergy = 0.0f;
    float engineEnergy = 0.0f;
    float strongestEnergy = 0.0f;
    float strongestFrequency = 0.0f;
    for ( size_t bin = 1; bin < MIC_BLOCK_SIZE / 2; bin++ )
    {
        const float frequency = bin * binWidth;
        const float energy = binEnergy( bin );
        totalEnergy += energy;
        if ( frequency >= ENGINE_LOW_HZ && frequency <= ENGINE_HIGH_HZ )
        {
            engineEnergy += energy;
        }
        if ( frequency >= MIN_ENGINE_HZ && frequency <= MAX_ENGINE_HZ &&
             energy > strongestEnergy )
        {
            strongestEnergy = energy;
            strongestFrequency = frequency;
        }
    }

    const float engineRatio = totalEnergy > 0.0f ? engineEnergy / totalEnergy : 0.0f;
    logger.log( LogLevel::INFO,
                "Audio: raw=%0.5f filtered=%0.5f peak=%ld (%0.1f%%) bits=signed24 min=%ld max=%ld",
                rawRms, filteredRms, (long) peak, peakPercent,
                (long) minimum, (long) maximum );

    if ( filteredRms < MIN_AUDIO_RMS )
    {
        return;
    }

    if ( filteredRms >= ENGINE_RMS && engineRatio >= ENGINE_BAND_RATIO &&
         strongestFrequency > 0.0f )
    {
        logger.log( LogLevel::INFO,
                    "[ENGINE QUAD/MX] f0~%0.1f Hz, RMS=%0.5f, band=%0.2f",
                    strongestFrequency, filteredRms, engineRatio );
    }
    else
    {
        logger.log( LogLevel::INFO,
                    "[SOUND] strongest=%0.1f Hz, RMS=%0.5f, engine-band=%0.2f",
                    strongestFrequency, filteredRms, engineRatio );
    }
}
}

// Gracefully stops every module, add new modules here
static void shutdown()
{
    logger.log( LogLevel::WARN, "Gracefully shutting down all modules" );

    microphone.end();
}

void setup() {
#if defined( MIC_STREAM ) || defined( MIC_DUAL_STREAM )
    // The capture script expects a binary-only serial stream after this point.
    Logger::setEnabled( false );
#endif
    logger.begin();
#if defined( MIC_STREAM ) || defined( MIC_DUAL_STREAM )
    Serial.setTimeout( 1000 );
#endif
    logger.log( LogLevel::INFO, "Starting setup process" );
    highPass.setHighPass( SAMPLE_RATE, HIGH_PASS_CUTOFF_HZ );
    lowPass.setLowPass( SAMPLE_RATE, ENGINE_HIGH_HZ );
    logger.log( LogLevel::INFO,
                "Audio processing: signed 24-bit, HPF=%0.0f Hz, LPF=%0.0f Hz, FFT=%u",
                HIGH_PASS_CUTOFF_HZ, ENGINE_HIGH_HZ, (unsigned) MIC_BLOCK_SIZE );

    bool anyFailures = false;

    logger.log( LogLevel::INFO, "Attempting to setup microphone" );
    if ( !microphone.begin() )
    {
        logger.log( LogLevel::ERR, "Microphone setup failed" );
        anyFailures = true;
    }

    if ( anyFailures )
    {
        shutdown();
    }
#if defined( MIC_STREAM ) || defined( MIC_DUAL_STREAM )
    else
    {
        Serial.print( "MICSTREAM\n" );
    }
#endif
}

void loop() {
    static int32_t samples[MIC_BLOCK_SIZE];
#ifdef MIC_DUAL_STREAM
    static int32_t filteredSamples[MIC_BLOCK_SIZE];
#endif

    // Blocks until the block is filled (~85 ms at 12 kHz), so no delay is needed
    size_t n = microphone.read( samples );
    if ( n != MIC_BLOCK_SIZE )
    {
        logger.log( LogLevel::WARN, "Incomplete block: %u of %u samples",
                    (unsigned) n, (unsigned) MIC_BLOCK_SIZE );
        return;
    }

#ifdef MIC_DUAL_STREAM
    memcpy( filteredSamples, samples, n * sizeof( int32_t ) );
    applyFilters( filteredSamples );
    Serial.write( reinterpret_cast<const uint8_t *>( samples ),
                  n * sizeof( int32_t ) );
    Serial.write( reinterpret_cast<const uint8_t *>( filteredSamples ),
                  n * sizeof( int32_t ) );
#elif defined( MIC_STREAM )
    applyFilters( samples );
    Serial.write( reinterpret_cast<const uint8_t *>( samples ),
                  n * sizeof( int32_t ) );
#else
    processAudio( samples );
#endif
}