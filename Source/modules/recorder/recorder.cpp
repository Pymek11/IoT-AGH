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
// Triggered recording: the 801S starts a recording, every pulse extends it, TRIG_HOLD_MS without
// vibration ends it. START / STOP from the PC arm and disarm the board
// =====================================

#include <cstring>

#include "../microphone/mic.h"

// Wait before trying to start the microphone again after a failure
constexpr unsigned long MIC_RETRY_DELAY_MS = 5000;

// Room for a few audio frames, so Serial.write() returns at once and the I2S reads stay on time
constexpr size_t SERIAL_TX_BUFFER = 8192;

// Loop period while the microphone is off (the I2S read paces the loop otherwise)
constexpr unsigned long IDLE_POLL_MS = 2;

// Pre-trigger buffer, in whole audio blocks
constexpr size_t PRE_BLOCKS =
    TRIG_I2S_ON_DEMAND ? 0
                       : ( (uint64_t) TRIG_PRE_MS * MIC_SAMPLE_RATE_HZ / 1000 + MIC_BLOCK_SIZE - 1 ) / MIC_BLOCK_SIZE;
constexpr size_t PRE_SLOTS = PRE_BLOCKS > 0 ? PRE_BLOCKS : 1;

// 801S edges kept while waiting, sent with the pre-trigger audio
constexpr size_t EVENT_HISTORY = 64;

static_assert( TRIG_PULSES >= 1 && TRIG_PULSES <= 64, "TRIG_PULSES must be 1..64" );

enum class State
{
    DISARMED,   // ignores vibration
    ARMED,      // waiting for vibration
    RECORDING,
};

struct PreBlock
{
    uint32_t first;     // sample index (since the microphone started)
    uint8_t level;
    uint8_t flags;
    PcmLevels levels;
    int16_t pcm[MIC_BLOCK_SIZE];
};

struct PastEvent
{
    uint32_t index;     // sample index (since the microphone started)
    uint8_t level;
    uint8_t flags;
};

static Microphone microphone;
static SampleClock sampleClock( MIC_SAMPLE_RATE_HZ );
static CommandReader commands;
static State state = State::DISARMED;

static int32_t raw[MIC_BLOCK_SIZE];
static int16_t pcm[MIC_BLOCK_SIZE];

static PreBlock preBlocks[PRE_SLOTS];
static size_t preCount = 0;
static size_t preNext = 0;

static PastEvent history[EVENT_HISTORY];
static size_t historyCount = 0;
static size_t historyNext = 0;

static int64_t pulseTimes[TRIG_PULSES];
static size_t pulseCount = 0;
static size_t pulseNext = 0;
static int64_t lastActivityUs = 0;

static uint32_t samplesRead = 0;        // since the microphone started
static uint32_t recStart = 0;           // sample index of the first sample of the recording
static uint32_t droppedAtStart = 0;
static unsigned long micRetryAt = 0;
static bool micRetryPending = false;

// Recording statistics, logged after it ends
static uint16_t recPeak = 0;
static uint32_t recClipped = 0;
static uint32_t recordingCount = 0;

static void clearWaitingState()
{
    preCount = preNext = 0;
    historyCount = historyNext = 0;
    pulseCount = pulseNext = 0;
}

// Starts or stops the I2S driver depending on the state
static void updateMicrophone()
{
    const bool wanted = state == State::RECORDING || ( !TRIG_I2S_ON_DEMAND && state == State::ARMED );

    if ( wanted && !microphone.isRunning() )
    {
        if ( micRetryPending && (long) ( millis() - micRetryAt ) < 0 )
        {
            return;
        }
        if ( !microphone.begin() )
        {
            logger.log( LogLevel::ERR, "Microphone setup failed, retrying in %lu ms", MIC_RETRY_DELAY_MS );
            micRetryPending = true;
            micRetryAt = millis() + MIC_RETRY_DELAY_MS;
            return;
        }
        micRetryPending = false;
        samplesRead = 0;
        sampleClock.reset();
        // Kept audio and edge indices belong to the previous I2S session (unless I2S starts for a
        // trigger, then the kept edges are at index 0 already)
        preCount = preNext = 0;
        if ( state != State::RECORDING )
        {
            historyCount = historyNext = 0;
        }
    }
    else if ( !wanted && microphone.isRunning() )
    {
        microphone.end();
    }
}

// Adds one vibration pulse, returns true when the spike filter lets a recording start
static bool pulseTriggers( int64_t timeUs )
{
    pulseTimes[pulseNext] = timeUs;
    pulseNext = ( pulseNext + 1 ) % TRIG_PULSES;
    if ( pulseCount < TRIG_PULSES )
    {
        pulseCount++;
    }
    if ( pulseCount < TRIG_PULSES )
    {
        return false;
    }
    // After the write, pulseNext points at the oldest of the last TRIG_PULSES pulses
    return timeUs - pulseTimes[pulseNext] <= (int64_t) TRIG_WINDOW_MS * 1000;
}

static void rememberEvent( uint32_t index, uint8_t level, uint8_t flags )
{
    history[historyNext] = { index, level, flags };
    historyNext = ( historyNext + 1 ) % EVENT_HISTORY;
    if ( historyCount < EVENT_HISTORY )
    {
        historyCount++;
    }
}

static void pushPreBlock( uint32_t first, uint8_t level, uint8_t flags, const PcmLevels &levels )
{
    PreBlock &block = preBlocks[preNext];
    block.first = first;
    block.level = level;
    block.flags = flags;
    block.levels = levels;
    memcpy( block.pcm, pcm, sizeof( block.pcm ) );
    preNext = ( preNext + 1 ) % PRE_SLOTS;
    if ( preCount < PRE_BLOCKS )
    {
        preCount++;
    }
}

static void sendAudioBlock( uint32_t first, uint8_t level, uint8_t flags, const PcmLevels &levels,
                            const int16_t *samples )
{
    if ( levels.peak > recPeak )
    {
        recPeak = levels.peak;
    }
    recClipped += levels.clipped;
    RecProtocol::sendAudio( first - recStart, level, flags, levels.peak, levels.clipped, samples,
                            MIC_BLOCK_SIZE );
}

// Starts a recording: sends START, the pre-trigger audio and the 801S edges kept while waiting
static void startRecording()
{
    logger.log( LogLevel::INFO, "Trigger: recording #%lu, logs are off until it ends",
                (unsigned long) ( recordingCount + 1 ) );
    Logger::setEnabled( false );
    state = State::RECORDING;
    recordingCount++;
    recPeak = 0;
    recClipped = 0;
    droppedAtStart = vib.droppedEvents();

    const size_t oldestBlock = ( preNext + PRE_SLOTS - preCount ) % PRE_SLOTS;
    if ( TRIG_I2S_ON_DEMAND )
    {
        updateMicrophone();     // starts I2S, samplesRead = 0
        recStart = 0;
    }
    else
    {
        recStart = preCount ? preBlocks[oldestBlock].first : samplesRead;
    }

    // Level at the first sample: accepted edges always flip the level, so it is the opposite of the
    // first kept edge, then the level of every edge from before the recording start
    const size_t oldestEvent = ( historyNext + EVENT_HISTORY - historyCount ) % EVENT_HISTORY;
    uint8_t startLevel = vib.reportedLevel();
    if ( historyCount )
    {
        startLevel = history[oldestEvent].level ? 0 : 1;
    }
    for ( size_t i = 0; i < historyCount; i++ )
    {
        const PastEvent &e = history[( oldestEvent + i ) % EVENT_HISTORY];
        if ( e.index < recStart )
        {
            startLevel = e.level;
        }
    }

    RecProtocol::sendStart( MIC_SAMPLE_RATE_HZ, 16, MIC_BLOCK_SIZE, startLevel, REC_MODE_TRIGGERED,
                            samplesRead - recStart );

    for ( size_t i = 0; i < preCount; i++ )
    {
        const PreBlock &b = preBlocks[( oldestBlock + i ) % PRE_SLOTS];
        sendAudioBlock( b.first, b.level, b.flags, b.levels, b.pcm );
    }
    for ( size_t i = 0; i < historyCount; i++ )
    {
        const PastEvent &e = history[( oldestEvent + i ) % EVENT_HISTORY];
        if ( e.index >= recStart )
        {
            RecProtocol::sendVib( e.index - recStart, e.level, e.flags );
        }
    }
    clearWaitingState();
}

// Ends the recording, the board goes to `next` (ARMED or DISARMED)
static void finishRecording( State next )
{
    if ( state != State::RECORDING )
    {
        state = next;
        return;
    }
    const uint32_t total = samplesRead >= recStart ? samplesRead - recStart : 0;
    RecProtocol::sendStop( total, vib.droppedEvents() - droppedAtStart );
    Serial.flush();
    state = next;
    clearWaitingState();

    Logger::setEnabled( true );
    logger.log( LogLevel::INFO, "Recording #%lu ended: %.2f s, peak %u / 32767, %lu clipped samples",
                (unsigned long) recordingCount, total / (float) MIC_SAMPLE_RATE_HZ, (unsigned) recPeak,
                (unsigned long) recClipped );
}

static void handleCommand( RecCommand command )
{
    switch ( command )
    {
    case RecCommand::START:
        if ( state == State::DISARMED )
        {
            state = State::ARMED;
            clearWaitingState();
            logger.log( LogLevel::INFO, "Armed, waiting for %d pulses within %d ms", TRIG_PULSES,
                        TRIG_WINDOW_MS );
        }
        RecProtocol::sendArmed( true, REC_MODE_TRIGGERED );
        break;
    case RecCommand::STOP:
        finishRecording( State::DISARMED );
        logger.log( LogLevel::INFO, "Disarmed" );
        RecProtocol::sendArmed( false, REC_MODE_TRIGGERED );
        break;
    case RecCommand::NONE:
        break;
    }
}

void recorderSetup()
{
    Serial.setTxBufferSize( SERIAL_TX_BUFFER );   // must come before Serial.begin()
    logger.begin();
    logger.log( LogLevel::INFO,
                "MIC_REC triggered: %lu Hz, 16-bit, 801S on GPIO%d, %d pulses / %d ms%s, hold %d ms, %s",
                (unsigned long) MIC_SAMPLE_RATE_HZ, VIB_PIN, TRIG_PULSES, TRIG_WINDOW_MS,
                TRIG_ON_LEVEL ? " or active level" : "", TRIG_HOLD_MS,
                TRIG_I2S_ON_DEMAND ? "I2S on demand (no pre-trigger)" : "pre-trigger buffer" );
    if ( PRE_BLOCKS > 0 )
    {
        logger.log( LogLevel::INFO, "Pre-trigger buffer: %u blocks (%lu ms)", (unsigned) PRE_BLOCKS,
                    (unsigned long) ( PRE_BLOCKS * MIC_BLOCK_SIZE * 1000UL / MIC_SAMPLE_RATE_HZ ) );
    }

    vib.begin();
    state = TRIG_ARMED_AT_BOOT ? State::ARMED : State::DISARMED;
    updateMicrophone();
    if ( state == State::ARMED )
    {
        RecProtocol::sendArmed( true, REC_MODE_TRIGGERED );
    }
    logger.log( LogLevel::INFO, "%s, send START / STOP to arm / disarm (rec_capture.py does it)",
                state == State::ARMED ? "Armed" : "Disarmed" );
}

void recorderLoop()
{
    handleCommand( commands.poll() );
    updateMicrophone();

    // 1. Audio: blocks until a block is filled (~85 ms at 12 kHz), or a short pause if I2S is off
    bool haveBlock = false;
    uint32_t blockStart = 0;
    if ( microphone.isRunning() )
    {
        const size_t n = microphone.read( raw );
        const int64_t readUs = esp_timer_get_time();
        if ( n != MIC_BLOCK_SIZE )
        {
            // A short block would leave a gap in the sample indices: end the recording, restart I2S
            const bool wasRecording = state == State::RECORDING;
            finishRecording( state == State::DISARMED ? State::DISARMED : State::ARMED );
            logger.log( LogLevel::WARN, "Incomplete block: %u of %u samples, restarting the microphone%s",
                        (unsigned) n, (unsigned) MIC_BLOCK_SIZE, wasRecording ? " (recording ended)" : "" );
            microphone.end();
            return;
        }
        blockStart = samplesRead;
        samplesRead += n;
        sampleClock.onBlock( samplesRead, readUs );
        haveBlock = true;
    }
    else
    {
        delay( IDLE_POLL_MS );
    }

    // 2. 801S edges: count pulses for the trigger, extend the recording, send or keep them
    vib.poll();
    bool vibSeen = false;
    bool triggerNow = false;
    VibEvent event;
    while ( vib.popEvent( event ) )
    {
        const bool active = VibrationSensor::isActive( event.level );
        uint8_t flags = event.polled ? REC_VIB_FLAG_POLLED : 0;
        if ( active )
        {
            vibSeen = true;
            lastActivityUs = event.timeUs;
            if ( state == State::ARMED && !triggerNow && pulseTriggers( event.timeUs ) )
            {
                triggerNow = true;
                flags |= REC_VIB_FLAG_TRIGGER;
            }
        }

        // With I2S off the edges have no sample index yet, they go to the start of the recording
        const uint32_t index = microphone.isRunning() ? sampleClock.indexAt( event.timeUs ) : 0;
        if ( state == State::RECORDING )
        {
            RecProtocol::sendVib( index > recStart ? index - recStart : 0, event.level, flags );
        }
        else if ( state == State::ARMED )
        {
            rememberEvent( index, event.level, flags );
        }
    }

    const int64_t now = esp_timer_get_time();
    const bool levelActive = VibrationSensor::isActive( vib.level() );
    if ( levelActive )
    {
        lastActivityUs = now;   // the pin held at the active level keeps the recording going
    }

    // Test mode: the active level alone starts a recording, without the spike filter
    bool levelTrigger = false;
    if ( TRIG_ON_LEVEL && state == State::ARMED && !triggerNow && levelActive )
    {
        triggerNow = true;
        levelTrigger = true;
    }

    // 3. Audio block: send it, or keep it in the pre-trigger buffer
    if ( haveBlock && state != State::DISARMED )
    {
        const PcmLevels levels = toPcm16( raw, pcm, MIC_BLOCK_SIZE );
        const uint8_t level = vib.reportedLevel();
        const uint8_t flags = ( vibSeen || VibrationSensor::isActive( level ) ) ? REC_AUDIO_FLAG_VIB_SEEN : 0;
        if ( state == State::RECORDING )
        {
            sendAudioBlock( blockStart, level, flags, levels, pcm );
        }
        else if ( PRE_BLOCKS > 0 )
        {
            pushPreBlock( blockStart, level, flags, levels );
        }
    }

    // 4. Start or end a recording
    if ( triggerNow )
    {
        startRecording();
        if ( levelTrigger )
        {
            // No edge carried the trigger flag: mark the moment in the CSV with a polled event
            const uint32_t index = microphone.isRunning() ? sampleClock.indexAt( now ) : 0;
            RecProtocol::sendVib( index > recStart ? index - recStart : 0, vib.level(),
                                  REC_VIB_FLAG_TRIGGER | REC_VIB_FLAG_POLLED );
        }
    }
    else if ( state == State::RECORDING && now - lastActivityUs >= (int64_t) TRIG_HOLD_MS * 1000 )
    {
        finishRecording( State::ARMED );
        updateMicrophone();     // on demand: I2S off until the next trigger
    }
}

#endif // VIB_DIAG

#endif // MIC_REC || VIB_DIAG
