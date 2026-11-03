#include <Arduino.h>

#include "utils/logger/logger.h"
#include "modules/microphone/mic.h"
#include "modules/microphone/mic_stream.h"
#include "modules/detector/detector.h"

static_assert( DETECTOR_SAMPLE_RATE_HZ == MIC_SAMPLE_RATE_HZ, "detector and microphone sample rates differ" );
static_assert( DETECTOR_BLOCK_SIZE == MIC_BLOCK_SIZE, "detector and microphone block sizes differ" );

// Detector features are logged every this many blocks (12 blocks ~ 1 s)
constexpr unsigned FEATURE_LOG_BLOCKS = 12;

static Logger logger( "main" );
static Microphone microphone;
static EngineDetector detector;
#ifdef MIC_STREAMING
static MicStream micStream;
#endif

// Runs the detector on one block and logs its state changes and, now and then, its features
// (unused in the streaming modes)
[[maybe_unused]] static void detectEngine( const int32_t *samples )
{
    static unsigned blockCount = 0;
    const bool wasDetected = detector.isDetected();
    const bool detected = detector.process( samples );
    const DetectorFeatures &f = detector.features();

    if ( detected && !wasDetected )
    {
        logger.log( LogLevel::INFO, "[ENGINE] Vehicle detected: pitch~%0.0f Hz, SNR=%0.1f dB, level=%0.1f dBFS",
                    f.pitchHz, f.snrDb, f.lowBandDbfs );
    }
    else if ( !detected && wasDetected )
    {
        logger.log( LogLevel::INFO, "[ENGINE] Vehicle gone" );
    }

    if ( ++blockCount % FEATURE_LOG_BLOCKS == 0 )
    {
        logger.log( LogLevel::DEBUG,
                    "level=%0.1f floor=%0.1f snr=%0.1f per=%0.2f pitch=%0.0f flat=%0.2f low=%0.2f high=%0.2f %s%s",
                    f.lowBandDbfs, f.noiseFloorDbfs, f.snrDb, f.periodicity, f.pitchHz,
                    f.flatness, f.lowRatio, f.highRatio, f.engine ? "E" : ".",
                    detected ? " DETECTED" : "" );
    }
}

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

    bool anyFailures = false;

    logger.log( LogLevel::INFO, "Attempting to setup microphone" );
    if ( !microphone.begin() )
    {
        logger.log( LogLevel::ERR, "Microphone setup failed" );
        anyFailures = true;
    }

    detector.begin();

    if ( anyFailures )
    {
        shutdown();
        return;
    }

#ifdef MIC_STREAMING
    micStream.begin();
#endif
}

void loop()
{
    static int32_t samples[MIC_BLOCK_SIZE];

    // Blocks until the block is filled (~85 ms at 12 kHz), so no delay is needed
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
    detectEngine( samples );
#endif
}
