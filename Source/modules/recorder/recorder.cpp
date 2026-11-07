#include "recorder.h"

#include "../../config.h"

#if defined( MIC_REC ) || defined( VIB_DIAG )

#ifndef VIB_PIN
#error "MIC_REC / VIB_DIAG need VIB_PIN in config.h for this board"
#endif

#include <Arduino.h>
#include <esp_timer.h>

#include "../../utils/logger/logger.h"
#include "../vibration/vibration.h"
#include "rec_protocol.h"
#include "rec_util.h"

static Logger logger( "rec" );
static VibrationSensor vib;

#ifdef VIB_DIAG
// =====================================
// 801S diagnostics: text only, no microphone
// =====================================

constexpr unsigned long DIAG_PRINT_MS = 250;

void recorderSetup()
{
    logger.begin();
    vib.begin();
    logger.log( LogLevel::INFO, "VIB_DIAG: 801S on GPIO%d, VIB_ACTIVE_HIGH=%d, debounce %d us",
                VIB_PIN, VIB_ACTIVE_HIGH, VIB_DEBOUNCE_US );
    logger.log( LogLevel::INFO, "Tap the sensor: the 'active' level should show up as vibration" );
}

void recorderLoop()
{
    static unsigned long lastPrint = 0;
    static uint32_t highEdges = 0;
    static uint32_t lowEdges = 0;

    vib.poll();
    VibEvent event;
    while ( vib.popEvent( event ) )
    {
        ( event.level ? highEdges : lowEdges )++;
    }

    const unsigned long now = millis();
    if ( now - lastPrint >= DIAG_PRINT_MS )
    {
        lastPrint = now;
        const uint8_t level = vib.level();
        logger.log( LogLevel::INFO, "pin=%s %s  edges ->HIGH %lu ->LOW %lu  dropped %lu",
                    level ? "HIGH" : "LOW ", VibrationSensor::isActive( level ) ? "VIBRATION" : "quiet    ",
                    (unsigned long) highEdges, (unsigned long) lowEdges,
                    (unsigned long) vib.droppedEvents() );
        highEdges = lowEdges = 0;
    }
    delay( 2 );
}

#else
// =====================================
// Continuous recording
// =====================================

#include "../microphone/mic.h"

// Wait before trying to start the microphone again after a failure
constexpr unsigned long MIC_RETRY_DELAY_MS = 5000;

// Room for a few audio frames, so Serial.write() returns at once and the I2S reads stay on time
constexpr size_t SERIAL_TX_BUFFER = 8192;

static Microphone microphone;
static SampleClock sampleClock( MIC_SAMPLE_RATE_HZ );
static CommandReader commands;

static int32_t raw[MIC_BLOCK_SIZE];
static int16_t pcm[MIC_BLOCK_SIZE];

static uint32_t samplesRead = 0;        // since the microphone started
static bool recording = false;
static uint32_t recStart = 0;           // samplesRead when the recording started
static uint32_t droppedAtStart = 0;

// Recording statistics, logged after STOP
static uint16_t recPeak = 0;
static uint32_t recClipped = 0;

static bool startMicrophone()
{
    if ( !microphone.begin() )
    {
        logger.log( LogLevel::ERR, "Microphone setup failed, retrying in %lu ms", MIC_RETRY_DELAY_MS );
        return false;
    }
    samplesRead = 0;
    sampleClock.reset();
    return true;
}

static void startRecording()
{
    if ( recording )
    {
        return;
    }
    logger.log( LogLevel::INFO, "START: recording, logs are off until STOP" );
    Logger::setEnabled( false );

    recording = true;
    recStart = samplesRead;
    droppedAtStart = vib.droppedEvents();
    recPeak = 0;
    recClipped = 0;
    RecProtocol::sendStart( MIC_SAMPLE_RATE_HZ, 16, MIC_BLOCK_SIZE, vib.reportedLevel(),
                            REC_MODE_CONTINUOUS, 0 );
}

static void stopRecording()
{
    if ( !recording )
    {
        return;
    }
    recording = false;
    RecProtocol::sendStop( samplesRead - recStart, vib.droppedEvents() - droppedAtStart );
    Serial.flush();

    Logger::setEnabled( true );
    logger.log( LogLevel::INFO, "STOP: %lu samples, peak %u / 32767, %lu clipped samples",
                (unsigned long) ( samplesRead - recStart ), (unsigned) recPeak,
                (unsigned long) recClipped );
}

// Sends the queued 801S edges, returns true if any of them went to the active level
static bool forwardVibEvents()
{
    bool active = false;
    VibEvent event;
    while ( vib.popEvent( event ) )
    {
        active |= VibrationSensor::isActive( event.level );
        if ( !recording )
        {
            continue;
        }
        // Edges from before START (still in the queue) are put at the first sample
        const uint32_t index = sampleClock.indexAt( event.timeUs );
        const uint32_t relative = index > recStart ? index - recStart : 0;
        RecProtocol::sendVib( relative, event.level, event.polled ? REC_VIB_FLAG_POLLED : 0 );
    }
    return active;
}

void recorderSetup()
{
    Serial.setTxBufferSize( SERIAL_TX_BUFFER );   // must come before Serial.begin()
    logger.begin();
    logger.log( LogLevel::INFO, "MIC_REC: continuous recording, %lu Hz, 16-bit, 801S on GPIO%d",
                (unsigned long) MIC_SAMPLE_RATE_HZ, VIB_PIN );

    vib.begin();
    startMicrophone();
    logger.log( LogLevel::INFO, "Ready, send START / STOP (rec_capture.py does it)" );
}

void recorderLoop()
{
    if ( !microphone.isRunning() )
    {
        stopRecording();
        delay( MIC_RETRY_DELAY_MS );
        startMicrophone();
        return;
    }

    // Blocks until the block is filled (~85 ms at 12 kHz)
    const size_t n = microphone.read( raw );
    const int64_t now = esp_timer_get_time();
    if ( n != MIC_BLOCK_SIZE )
    {
        // A short block would leave a gap in the sample indices, restart the microphone instead
        logger.log( LogLevel::WARN, "Incomplete block: %u of %u samples, restarting the microphone",
                    (unsigned) n, (unsigned) MIC_BLOCK_SIZE );
        stopRecording();
        microphone.end();
        return;
    }

    const uint32_t blockStart = samplesRead;
    samplesRead += n;
    sampleClock.onBlock( samplesRead, now );

    vib.poll();
    const bool vibSeen = forwardVibEvents();

    if ( recording && blockStart >= recStart )
    {
        const PcmLevels levels = toPcm16( raw, pcm, n );
        if ( levels.peak > recPeak )
        {
            recPeak = levels.peak;
        }
        recClipped += levels.clipped;

        const uint8_t level = vib.reportedLevel();
        const uint8_t flags = ( vibSeen || VibrationSensor::isActive( level ) ) ? REC_AUDIO_FLAG_VIB_SEEN : 0;
        RecProtocol::sendAudio( blockStart - recStart, level, flags, levels.peak, levels.clipped, pcm, n );
    }

    switch ( commands.poll() )
    {
    case RecCommand::START:
        startRecording();
        break;
    case RecCommand::STOP:
        stopRecording();
        break;
    case RecCommand::NONE:
        break;
    }
}

#endif // VIB_DIAG

#endif // MIC_REC || VIB_DIAG
