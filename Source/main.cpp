#include <Arduino.h>

#include "utils/logger/logger.h"
#include "modules/microphone/mic.h"

static Logger logger( "main" );
static Microphone microphone;

// Gracefully stops every module, add new modules here
static void shutdown()
{
    logger.log( LogLevel::WARN, "Gracefully shutting down all modules" );

    microphone.end();
}

void setup() {
#ifdef MIC_STREAM
    // Serial carries raw samples, no text logs allowed
    Logger::setEnabled( false );
#endif
    logger.begin();
    logger.log( LogLevel::INFO, "Starting setup process" );

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

#ifdef MIC_STREAM
    // Marks where the raw samples start, boot messages may come before it
    else
    {
        Serial.print( "MICSTREAM\n" );
    }
#endif
}

void loop() {
    static int32_t samples[MIC_BLOCK_SIZE];

    // Blocks until the block is filled (~85 ms at 12 kHz), so no delay is needed
    size_t n = microphone.read( samples );
    if ( n != MIC_BLOCK_SIZE )
    {
        logger.log( LogLevel::WARN, "Incomplete block: %u of %u samples",
                    (unsigned) n, (unsigned) MIC_BLOCK_SIZE );
        return;
    }

#ifdef MIC_STREAM
    // Raw samples, 4 bytes each (little endian int32_t, signed 24-bit values)
    Serial.write( reinterpret_cast<const uint8_t *>( samples ), n * sizeof( int32_t ) );
#else
    int32_t minValue = INT32_MAX;
    int32_t maxValue = INT32_MIN;
    for ( size_t i = 0; i < n; i++ )
    {
        minValue = min( minValue, samples[i] );
        maxValue = max( maxValue, samples[i] );
    }

    logger.log( LogLevel::INFO, "Block of %u samples, min %ld, max %ld",
                (unsigned) n, (long) minValue, (long) maxValue );
#endif
}