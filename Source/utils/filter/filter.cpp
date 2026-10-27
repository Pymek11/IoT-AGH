#include "filter.h"

#include <cmath>

namespace
{
constexpr float PI = 3.14159265358979323846f;
constexpr float Q = 0.70710678118f;
}

void BiquadFilter::setLowPass( float sampleRateHz, float cutoffHz )
{
    const float omega = 2.0f * PI * cutoffHz / sampleRateHz;
    const float alpha = sinf( omega ) / ( 2.0f * Q );
    const float cosine = cosf( omega );
    const float a0 = 1.0f + alpha;

    _b0 = ( 1.0f - cosine ) / ( 2.0f * a0 );
    _b1 = ( 1.0f - cosine ) / a0;
    _b2 = _b0;
    _a1 = -2.0f * cosine / a0;
    _a2 = ( 1.0f - alpha ) / a0;
    reset();
}

void BiquadFilter::setHighPass( float sampleRateHz, float cutoffHz )
{
    const float omega = 2.0f * PI * cutoffHz / sampleRateHz;
    const float alpha = sinf( omega ) / ( 2.0f * Q );
    const float cosine = cosf( omega );
    const float a0 = 1.0f + alpha;

    _b0 = ( 1.0f + cosine ) / ( 2.0f * a0 );
    _b1 = -( 1.0f + cosine ) / a0;
    _b2 = _b0;
    _a1 = -2.0f * cosine / a0;
    _a2 = ( 1.0f - alpha ) / a0;
    reset();
}

float BiquadFilter::process( float sample )
{
    const float output = _b0 * sample + _b1 * _x1 + _b2 * _x2
                       - _a1 * _y1 - _a2 * _y2;
    _x2 = _x1;
    _x1 = sample;
    _y2 = _y1;
    _y1 = output;
    return output;
}

void BiquadFilter::reset()
{
    _x1 = 0.0f;
    _x2 = 0.0f;
    _y1 = 0.0f;
    _y2 = 0.0f;
}
