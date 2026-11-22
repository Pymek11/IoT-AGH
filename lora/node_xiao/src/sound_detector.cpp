#include "sound_detector.h"

#include <Arduino.h>
#include <math.h>

#include "../../../Source/modules/microphone/mic.h"

namespace
{
constexpr float kTwoPi = 6.28318530717958647692f;
constexpr float SAMPLE_RATE = static_cast<float>( MIC_SAMPLE_RATE_HZ );
constexpr float HIGH_PASS_CUTOFF_HZ = 200.0f;
constexpr float ENGINE_LOW_HZ = 50.0f;
constexpr float ENGINE_HIGH_HZ = 1200.0f;     // also the low-pass cutoff
constexpr float MIN_ENGINE_HZ = 30.0f;
constexpr float MAX_ENGINE_HZ = 220.0f;

float fftReal[MIC_BLOCK_SIZE];
float fftImag[MIC_BLOCK_SIZE];

// In-place radix-2 FFT (same algorithm as Source/main.cpp)
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
}

void SoundDetector::begin()
{
    _highPass.setHighPass( SAMPLE_RATE, HIGH_PASS_CUTOFF_HZ );
    _lowPass.setLowPass( SAMPLE_RATE, ENGINE_HIGH_HZ );
}

void SoundDetector::reset()
{
    _highPass.reset();
    _lowPass.reset();
}

SoundStats SoundDetector::process( const int32_t *samples, size_t count, bool analyzeSpectrum )
{
    SoundStats stats;
    if ( count == 0 )
    {
        return stats;
    }
    if ( count > MIC_BLOCK_SIZE )
    {
        count = MIC_BLOCK_SIZE;
    }

    double rawSum = 0.0;
    double filteredSum = 0.0;
    for ( size_t i = 0; i < count; i++ )
    {
        const float normalized = static_cast<float>( samples[i] ) / 8388608.0f;
        const float filtered = _lowPass.process( _highPass.process( normalized ) );
        rawSum += normalized * normalized;
        filteredSum += filtered * filtered;

        if ( analyzeSpectrum )
        {
            const float window = 0.54f - 0.46f * cosf( kTwoPi * i / MIC_BLOCK_SIZE );
            fftReal[i] = filtered * window;
            fftImag[i] = 0.0f;
        }
    }

    stats.rawRms = sqrtf( static_cast<float>( rawSum / count ) );
    stats.filteredRms = sqrtf( static_cast<float>( filteredSum / count ) );

    if ( !analyzeSpectrum || count != MIC_BLOCK_SIZE )
    {
        return stats;
    }

    calculateFft();

    const float binWidth = SAMPLE_RATE / MIC_BLOCK_SIZE;
    float totalEnergy = 0.0f;
    float engineEnergy = 0.0f;
    float strongestEnergy = 0.0f;
    for ( size_t bin = 1; bin < MIC_BLOCK_SIZE / 2; bin++ )
    {
        const float frequency = bin * binWidth;
        const float energy = fftReal[bin] * fftReal[bin] + fftImag[bin] * fftImag[bin];
        totalEnergy += energy;
        if ( frequency >= ENGINE_LOW_HZ && frequency <= ENGINE_HIGH_HZ )
        {
            engineEnergy += energy;
        }
        if ( frequency >= MIN_ENGINE_HZ && frequency <= MAX_ENGINE_HZ && energy > strongestEnergy )
        {
            strongestEnergy = energy;
            stats.strongestHz = frequency;
        }
    }
    stats.engineRatio = totalEnergy > 0.0f ? engineEnergy / totalEnergy : 0.0f;
    return stats;
}
