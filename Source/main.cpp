#include <Arduino.h>

#include "utils/logger/logger.h"
#include "modules/microphone/mic.h"
#include "modules/microphone/mic_stream.h"
#include "modules/detector/detector.h"

static_assert( DETECTOR_SAMPLE_RATE_HZ == MIC_SAMPLE_RATE_HZ, "detector and microphone sample rates differ" );
static_assert( DETECTOR_BLOCK_SIZE == MIC_BLOCK_SIZE, "detector and microphone block sizes differ" );

// Wait before trying to start the microphone again after a failure
constexpr unsigned long MIC_RETRY_DELAY_MS = 5000;

#ifdef DETECTOR_DEBUG
// Detector features are logged every this many blocks (12 blocks ~ 1 s)
constexpr unsigned FEATURE_LOG_BLOCKS = 12;
#endif

static Logger logger( "main" );
static Microphone microphone;
static EngineDetector detector;
#ifdef MIC_STREAMING
static MicStream micStream;
#endif

// Starts the microphone (and the serial stream in the streaming modes)
// Returns true on success
static bool startMicrophone()
{
    logger.log( LogLevel::INFO, "Attempting to setup microphone" );
    if ( !microphone.begin() )
    {
        logger.log( LogLevel::ERR, "Microphone setup failed, retrying in %lu ms", MIC_RETRY_DELAY_MS );
        return false;
    }

#ifdef MIC_STREAMING
    micStream.begin();
#endif
    return true;
}

#ifdef DETECTOR_DEBUG
// Logs the features of the last block now and then, for tuning the thresholds
static void logFeatures( bool detected )
{
    static unsigned blockCount = 0;
    if ( ++blockCount % FEATURE_LOG_BLOCKS != 0 )
    {
        return;
    }

    const DetectorFeatures &f = detector.features();
    logger.log( LogLevel::DEBUG,
                "level=%0.1f floor=%0.1f snr=%0.1f per=%0.2f pitch=%0.0f flat=%0.2f low=%0.2f high=%0.2f %s%s",
                f.lowBandDbfs, f.noiseFloorDbfs, f.snrDb, f.periodicity, f.pitchHz,
                f.flatness, f.lowRatio, f.highRatio, f.engine ? "E" : ".",
                detected ? " DETECTED" : "" );
}
#endif

// Gracefully stops every module, add new modules here
static void shutdown()
{
    logger.log( LogLevel::WARN, "Gracefully shutting down all modules" );

    microphone.end();
}

void setup()
{
    logger.begin();
    logger.log( LogLevel::INFO, "Starting setup process" );

    detector.begin();

    bool anyFailures = false;

    if ( !startMicrophone() )
    {
        anyFailures = true;
    }

    // loop() keeps retrying the modules that did not start
    if ( anyFailures )
    {
        shutdown();
    }
}

void loop()
{
    static int32_t samples[MIC_BLOCK_SIZE];

    if ( !microphone.isRunning() )
    {
        delay( MIC_RETRY_DELAY_MS );
        if ( startMicrophone() )
        {
            detector.reset();
        }
        return;
    }

    // Blocks until the block is filled (~85 ms at 12 kHz), so the sound is analyzed continuously
    size_t n = microphone.read( samples );
    if ( n != MIC_BLOCK_SIZE )
    {
        logger.log( LogLevel::WARN, "Incomplete block: %u of %u samples",
                    (unsigned) n, (unsigned) MIC_BLOCK_SIZE );
        return;
    }

#ifdef MIC_STREAMING
    micStream.send( samples );
#else
    bool detected = detector.process( samples );
    if ( detected )
    {
        logger.log( LogLevel::INFO, "Vehicle detected" );
    }

#ifdef DETECTOR_DEBUG
    logFeatures( detected );
#endif
#endif
}
