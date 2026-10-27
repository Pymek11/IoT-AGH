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

    int32_t minValue = INT32_MAX;
    int32_t maxValue = INT32_MIN;
    for ( size_t i = 0; i < n; i++ )
    {
        minValue = min( minValue, samples[i] );
        maxValue = max( maxValue, samples[i] );
    }

    logger.log( LogLevel::INFO, "Block of %u samples, min %ld, max %ld",
                (unsigned) n, (long) minValue, (long) maxValue );
}