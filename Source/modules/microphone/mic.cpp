#include "mic.h"

#include "../../config.h"
#include "../../utils/logger/logger.h"

static Logger logger( "mic" );

Microphone::~Microphone()
{
    end();
}

bool Microphone::begin()
{
    if ( _running )
    {
        return true;
    }

    // INMP441: L/R pin to GND -> data on the left slot
    // It has no data output (dout) and needs no MCLK
    _i2s.setPins( MICR_SCK_PIN, MICR_WS_PIN, -1, MICR_SD_PIN, -1 );

    // 24-bit samples come in a 32-bit slot
    if ( !_i2s.begin( I2S_MODE_STD,
                      MIC_SAMPLE_RATE_HZ,
                      I2S_DATA_BIT_WIDTH_32BIT,
                      I2S_SLOT_MODE_MONO,
                      I2S_STD_SLOT_LEFT ) )
    {
        logger.log( LogLevel::ERR, "Failed to start I2S driver" );
        return false;
    }

    _running = true;
    _dcPrimed = false;
    logger.log( LogLevel::INFO, "I2S started: %lu Hz, %u-bit samples, block of %u samples",
                (unsigned long) MIC_SAMPLE_RATE_HZ, (unsigned) MIC_BITS_PER_SAMPLE,
                (unsigned) MIC_BLOCK_SIZE );
    return true;
}

void Microphone::end()
{
    if ( !_running )
    {
        return;
    }

    _i2s.end();
    _running = false;
    logger.log( LogLevel::INFO, "I2S Microphone stopped" );
}

size_t Microphone::read( int32_t *buffer, size_t count )
{
    if ( !_running )
    {
        return 0;
    }

    size_t bytes = _i2s.readBytes( reinterpret_cast<char *>( buffer ), count * sizeof( int32_t ) );
    size_t samples = bytes / sizeof( int32_t );

    for ( size_t i = 0; i < samples; i++ )
    {
        // Data is left aligned in the 32-bit slot, shift it down to a signed 24-bit value
        int32_t value = buffer[i] >> 8;

        // Start the estimate at the first sample to avoid a startup transient
        if ( !_dcPrimed )
        {
            _dcAccumulator = (int64_t) value << MIC_DC_FILTER_SHIFT;
            _dcPrimed = true;
        }

        // One-pole DC blocker: track the slow average and subtract it
        _dcAccumulator += value - ( _dcAccumulator >> MIC_DC_FILTER_SHIFT );
        buffer[i] = value - (int32_t) ( _dcAccumulator >> MIC_DC_FILTER_SHIFT );
    }

    return samples;
}
