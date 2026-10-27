#ifndef FILTER_H
#define FILTER_H

class BiquadFilter
{
public:
    void setLowPass( float sampleRateHz, float cutoffHz );
    void setHighPass( float sampleRateHz, float cutoffHz );
    float process( float sample );
    void reset();

private:
    float _b0 = 1.0f;
    float _b1 = 0.0f;
    float _b2 = 0.0f;
    float _a1 = 0.0f;
    float _a2 = 0.0f;
    float _x1 = 0.0f;
    float _x2 = 0.0f;
    float _y1 = 0.0f;
    float _y2 = 0.0f;
};

#endif