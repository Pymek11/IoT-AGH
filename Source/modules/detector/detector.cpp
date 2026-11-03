#include "detector.h"

#include <cmath>
#include <cstring>

#include "../../utils/fft/fft.h"

namespace
{
constexpr float TWO_PI = 6.28318530717958647692f;
constexpr float FULL_SCALE = 8388608.0f;    // 2^23, signed 24-bit
constexpr float EPSILON = 1e-20f;

constexpr float BIN_WIDTH_HZ = static_cast<float>( DETECTOR_SAMPLE_RATE_HZ ) / DETECTOR_BLOCK_SIZE;
constexpr size_t BIN_COUNT = DETECTOR_BLOCK_SIZE / 2;

constexpr float DECIMATED_RATE_HZ = static_cast<float>( DETECTOR_SAMPLE_RATE_HZ ) / DETECTOR_DECIMATION;
constexpr size_t DECIMATED_BLOCK = DETECTOR_BLOCK_SIZE / DETECTOR_DECIMATION;
constexpr size_t HISTORY_SIZE = DETECTOR_HISTORY_BLOCKS * DECIMATED_BLOCK;
constexpr size_t MIN_LAG = static_cast<size_t>( DECIMATED_RATE_HZ / DETECTOR_PITCH_MAX_HZ );
constexpr size_t MAX_LAG = static_cast<size_t>( DECIMATED_RATE_HZ / DETECTOR_PITCH_MIN_HZ );

static_assert( ( DETECTOR_BLOCK_SIZE & ( DETECTOR_BLOCK_SIZE - 1 ) ) == 0,
               "FFT needs a power of two block" );
static_assert( DETECTOR_BLOCK_SIZE % DETECTOR_DECIMATION == 0, "block must split into whole decimated samples" );
static_assert( DETECTOR_PERIODICITY_WINDOW + MAX_LAG <= HISTORY_SIZE,
               "history too short for the lowest pitch, raise DETECTOR_HISTORY_BLOCKS" );
static_assert( DETECTOR_DECISION_BLOCKS <= 32, "decisions are kept in a 32-bit mask" );

size_t binOf( float frequencyHz )
{
    return static_cast<size_t>( ceilf( frequencyHz / BIN_WIDTH_HZ ) );
}

// RMS level in dB re full scale from the summed power of some bins
float levelDbfs( float binPower, float windowPower )
{
    // Parseval for a one-sided spectrum, corrected for the window's energy
    const float meanSquare = 2.0f * binPower / ( DETECTOR_BLOCK_SIZE * windowPower );
    return 10.0f * log10f( meanSquare + EPSILON );
}

int popCount( uint32_t value )
{
    int count = 0;
    for ( ; value; value &= value - 1 )
    {
        count++;
    }
    return count;
}
}

void EngineDetector::begin()
{
    _highPass.setHighPass( DETECTOR_SAMPLE_RATE_HZ, DETECTOR_HIGH_PASS_HZ );
    for ( BiquadFilter &filter : _decimationLowPass )
    {
        filter.setLowPass( DETECTOR_SAMPLE_RATE_HZ, DETECTOR_DECIMATION_LOW_PASS_HZ );
    }

    // Hann window, sum of squares kept to turn bin power back into a level
    _windowPower = 0.0f;
    for ( size_t i = 0; i < DETECTOR_BLOCK_SIZE; i++ )
    {
        _window[i] = 0.5f - 0.5f * cosf( TWO_PI * i / DETECTOR_BLOCK_SIZE );
        _windowPower += _window[i] * _window[i];
    }

    reset();
}

void EngineDetector::reset()
{
    _highPass.reset();
    for ( BiquadFilter &filter : _decimationLowPass )
    {
        filter.reset();
    }
    memset( _history, 0, sizeof( _history ) );
    _historyBlocks = 0;
    _floorPrimed = false;
    _decisions = 0;
    _detected = false;
    _features = DetectorFeatures();
}

bool EngineDetector::process( const int32_t *samples )
{
    for ( size_t i = 0; i < DETECTOR_BLOCK_SIZE; i++ )
    {
        _block[i] = _highPass.process( static_cast<float>( samples[i] ) / FULL_SCALE );
    }

    analyzeSpectrum();
    analyzePeriodicity();

    if ( !_floorPrimed )
    {
        _noiseFloorDbfs = _features.lowBandDbfs;
        _floorPrimed = true;
    }
    _features.noiseFloorDbfs = _noiseFloorDbfs;
    _features.snrDb = _features.lowBandDbfs - _noiseFloorDbfs;

    _features.engine = _features.lowBandDbfs >= DETECTOR_MIN_LOW_BAND_DBFS &&
                       _features.snrDb >= DETECTOR_MIN_SNR_DB &&
                       _features.periodicity >= DETECTOR_MIN_PERIODICITY &&
                       _features.flatness <= DETECTOR_MAX_FLATNESS &&
                       _features.lowRatio >= DETECTOR_MIN_LOW_RATIO &&
                       _features.highRatio <= DETECTOR_MAX_HIGH_RATIO;

    updateDecision();
    updateNoiseFloor();
    return _detected;
}

void EngineDetector::analyzeSpectrum()
{
    for ( size_t i = 0; i < DETECTOR_BLOCK_SIZE; i++ )
    {
        _real[i] = _block[i] * _window[i];
        _imag[i] = 0.0f;
    }
    fft( _real, _imag, DETECTOR_BLOCK_SIZE );

    const size_t lowStart = binOf( DETECTOR_LOW_BAND_START_HZ );
    const size_t lowEnd = binOf( DETECTOR_LOW_BAND_END_HZ );
    const size_t flatnessEnd = binOf( DETECTOR_FLATNESS_END_HZ );
    const size_t highStart = binOf( DETECTOR_HIGH_BAND_START_HZ );

    float lowPower = 0.0f;
    float totalPower = 0.0f;
    float highPower = 0.0f;
    float flatnessSum = 0.0f;
    float flatnessLogSum = 0.0f;

    for ( size_t bin = lowStart; bin < BIN_COUNT; bin++ )
    {
        const float power = _real[bin] * _real[bin] + _imag[bin] * _imag[bin];
        totalPower += power;
        if ( bin < lowEnd )
        {
            lowPower += power;
        }
        if ( bin >= highStart )
        {
            highPower += power;
        }
        if ( bin < flatnessEnd )
        {
            flatnessSum += power + EPSILON;
            flatnessLogSum += logf( power + EPSILON );
        }
    }

    // Flatness: geometric mean / arithmetic mean of the bin powers
    const float flatnessBins = static_cast<float>( flatnessEnd - lowStart );
    _features.flatness = expf( flatnessLogSum / flatnessBins ) / ( flatnessSum / flatnessBins );
    _features.lowBandDbfs = levelDbfs( lowPower, _windowPower );
    _features.lowRatio = lowPower / ( totalPower + EPSILON );
    _features.highRatio = highPower / ( totalPower + EPSILON );
}

void EngineDetector::analyzePeriodicity()
{
    // Low-pass and keep every DETECTOR_DECIMATION-th sample, appended to the history
    memmove( _history, _history + DECIMATED_BLOCK, ( HISTORY_SIZE - DECIMATED_BLOCK ) * sizeof( float ) );
    float *decimated = _history + HISTORY_SIZE - DECIMATED_BLOCK;
    for ( size_t i = 0; i < DETECTOR_BLOCK_SIZE; i++ )
    {
        const float value = _decimationLowPass[1].process( _decimationLowPass[0].process( _block[i] ) );
        if ( i % DETECTOR_DECIMATION == 0 )
        {
            decimated[i / DETECTOR_DECIMATION] = value;
        }
    }

    if ( _historyBlocks < DETECTOR_HISTORY_BLOCKS )
    {
        _historyBlocks++;
    }
    _features.periodicity = 0.0f;
    _features.pitchHz = 0.0f;
    if ( _historyBlocks < DETECTOR_HISTORY_BLOCKS )
    {
        return;
    }

    // Normalized autocorrelation of the oldest window against windows shifted by each lag
    const float *head = _history;
    float headEnergy = 0.0f;
    for ( size_t n = 0; n < DETECTOR_PERIODICITY_WINDOW; n++ )
    {
        headEnergy += head[n] * head[n];
    }

    bool crossedZero = false;
    for ( size_t lag = 1; lag <= MAX_LAG; lag++ )
    {
        const float *tail = _history + lag;
        float product = 0.0f;
        float tailEnergy = 0.0f;
        for ( size_t n = 0; n < DETECTOR_PERIODICITY_WINDOW; n++ )
        {
            product += head[n] * tail[n];
            tailEnergy += tail[n] * tail[n];
        }
        const float correlation = product / sqrtf( headEnergy * tailEnergy + EPSILON );

        // Look for the pitch peak only after the first negative dip, so the short-lag
        // correlation of any smooth (low-passed) signal, e.g. wind, is not taken for a pitch
        if ( correlation < 0.0f )
        {
            crossedZero = true;
        }
        if ( crossedZero && lag >= MIN_LAG && correlation > _features.periodicity )
        {
            _features.periodicity = correlation;
            _features.pitchHz = DECIMATED_RATE_HZ / lag;
        }
    }
}

void EngineDetector::updateDecision()
{
    const uint32_t mask = ( DETECTOR_DECISION_BLOCKS == 32 ) ? 0xFFFFFFFFu
                                                             : ( 1u << DETECTOR_DECISION_BLOCKS ) - 1u;
    _decisions = ( ( _decisions << 1 ) | ( _features.engine ? 1u : 0u ) ) & mask;

    const int passed = popCount( _decisions );
    if ( !_detected && passed >= DETECTOR_ON_BLOCKS )
    {
        _detected = true;
    }
    else if ( _detected && passed <= DETECTOR_OFF_BLOCKS )
    {
        _detected = false;
    }
}

void EngineDetector::updateNoiseFloor()
{
    const float level = _features.lowBandDbfs;
    if ( level < _noiseFloorDbfs )
    {
        _noiseFloorDbfs += DETECTOR_FLOOR_FALL * ( level - _noiseFloorDbfs );
    }
    else if ( !_detected )
    {
        _noiseFloorDbfs += fminf( DETECTOR_FLOOR_RISE_DB, level - _noiseFloorDbfs );
    }
}
