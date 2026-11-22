// Battery sensor node: XIAO ESP32S3 + Wio-SX1262 + INMP441 microphone + 801S vibration sensor.
//
// The node sleeps (deep sleep, tens of microamps for the MCU) and wakes for one of three reasons:
//   - VIBRATION: the 801S digital output on D0 (GPIO1) goes HIGH   -> wakes immediately
//   - TIMER:     every SOUND_PERIOD_MS, to listen to the microphone briefly (it cannot wake the MCU itself)
//   - POWER-ON:  first start
// On every wake it listens to the microphone for a short window, then decides:
//   vibration + sound -> EVT_BOTH (3, CRITICAL alarm)    vibration only -> EVT_VIBRATION (1, WARNING)
//   sound only        -> EVT_SOUND (2, WARNING)          nothing        -> no packet (heartbeat if due)
// and sends one 3-byte LoRa packet [sensor id, event type, counter] (see lora/common/lora_link.h).
// A heartbeat (type 0, no alarm) goes out every HEARTBEAT_MS so the dashboard shows the node as online.
//
// Build environments (platformio.ini):
//   xiao_esp32s3        real behaviour, deep sleep. The USB serial port disappears while asleep.
//   xiao_esp32s3_bench  same logic but no deep sleep (waits in a loop), serial log stays connected.

#include <Arduino.h>
#include <ESP_I2S.h>    // used by Source/modules/microphone; included here so PlatformIO's library scan finds it
#include <RadioLib.h>
#include <driver/rtc_io.h>
#include <esp_sleep.h>
#include <sys/time.h>

#include "lora_link.h"
#include "sound_detector.h"
#include "../../../Source/modules/microphone/mic.h"

// ---- Settings (override with -D flags in platformio.ini) ----
// Sensor id, becomes device "sensor-<id>" in ThingsBoard
#ifndef SENSOR_ID
#define SENSOR_ID 4
#endif
// Filtered microphone RMS (0..1 of full scale) at or above which a block counts as "sound".
// Roughly: 0.004 ~ 73 dB SPL at the microphone. The value is printed every cycle, tune it from the log.
#ifndef SOUND_RMS_THRESHOLD
#define SOUND_RMS_THRESHOLD 0.004f
#endif
// 1 = additionally require the engine-band energy ratio (stricter, ignores claps/voices), 0 = any loud sound
#ifndef SOUND_REQUIRE_ENGINE_BAND
#define SOUND_REQUIRE_ENGINE_BAND 0
#endif
constexpr float ENGINE_BAND_RATIO = 0.42f;

constexpr int PIN_VIBRATION = 1;                    // D0 = GPIO1, RTC-capable pin (needed for deep-sleep wake)

constexpr uint32_t SOUND_PERIOD_MS = 3000;          // sleep between microphone checks
constexpr uint32_t HEARTBEAT_MS = 10UL * 60 * 1000; // must stay well below the sensor's 1 h inactivity timeout
constexpr uint32_t ALARM_COOLDOWN_MS = 8000;        // min time between alarms of the same kind (1% duty cycle)
constexpr uint8_t ALARM_COPIES = 2;                 // alarm packets are sent twice, the receiver drops the duplicate
constexpr uint32_t COPY_GAP_MS = 250;

// INMP441 needs a few hundred ms after its clock starts before the data is valid
constexpr size_t MIC_SETTLE_BLOCKS = 4;             // ~85 ms each at 12 kHz, discarded
constexpr size_t MIC_ANALYZE_BLOCKS = 2;            // analysed, the loudest one counts

// XIAO ESP32S3 + Wio-SX1262 kit pins: NSS, DIO1, RST, BUSY
SX1262 radio = new Module( 41, 39, 42, 40 );
constexpr float TCXO_V = 1.8f;                      // Wio-SX1262: TCXO powered from DIO3 (1.7-3.3 V)

static Microphone microphone;
static SoundDetector detector;

enum class Wake { PowerOn, Timer, Vibration };

// State kept in RTC memory, survives deep sleep (reset on power loss)
RTC_DATA_ATTR static uint8_t g_counter = 0;
RTC_DATA_ATTR static int64_t g_lastAlarmMs[4] = { -1000000, -1000000, -1000000, -1000000 };   // per event type
RTC_DATA_ATTR static int64_t g_lastPacketMs = -1000000000LL;

static bool g_radioReady = false;

// Wall-clock milliseconds since power-on; unlike millis() it keeps counting through deep sleep
static int64_t nowMs()
{
    struct timeval tv;
    gettimeofday( &tv, nullptr );
    return (int64_t) tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

// ---------------------------------------------------------------- radio

static bool ensureRadio()
{
    if ( g_radioReady )
    {
        return true;
    }
    for ( int attempt = 0; attempt < 3; attempt++ )
    {
        int st = radio.begin( LORA_FREQ_MHZ, LORA_BW_KHZ, LORA_SF, LORA_CR, LORA_SYNC_WORD,
                              LORA_POWER_DBM, LORA_PREAMBLE, TCXO_V );
        if ( st == RADIOLIB_ERR_NONE )
        {
            radio.setDio2AsRfSwitch( true );   // DIO2 drives the antenna switch
            g_radioReady = true;
            return true;
        }
        Serial.printf( "radio.begin() error: %d\n", st );
        delay( 50 );
    }
    return false;
}

static void radioSleep()
{
    if ( g_radioReady )
    {
        radio.sleep( false );   // lowest power, configuration is lost and redone by ensureRadio()
        g_radioReady = false;
    }
}

// One event = one counter value; the same packet is repeated `copies` times for reliability
static void sendEvent( uint8_t eventType, uint8_t copies )
{
    if ( !ensureRadio() )
    {
        Serial.println( "  -> NOT sent: radio not available" );
        return;
    }
    uint8_t payload[LORA_PACKET_LEN] = { (uint8_t) SENSOR_ID, eventType, g_counter };
    for ( uint8_t i = 0; i < copies; i++ )
    {
        int st = radio.transmit( payload, sizeof( payload ) );
        if ( st == RADIOLIB_ERR_NONE )
        {
            Serial.printf( "  -> sent sensor=%u type=%u #%u (copy %u/%u)\n", payload[0], payload[1],
                           payload[2], (unsigned) ( i + 1 ), (unsigned) copies );
        }
        else
        {
            Serial.printf( "  -> transmit() error: %d\n", st );
        }
        if ( i + 1 < copies )
        {
            delay( COPY_GAP_MS );
        }
    }
    g_counter++;
}

// ---------------------------------------------------------------- microphone

struct SoundReading
{
    bool valid = false;     // false if the microphone could not be started
    bool loud = false;
    SoundStats stats;
};

static SoundReading listen()
{
    static int32_t block[MIC_BLOCK_SIZE];
    SoundReading reading;

    if ( !microphone.begin() )
    {
        Serial.println( "  microphone not available, sound check skipped" );
        return reading;
    }
    detector.reset();

    for ( size_t i = 0; i < MIC_SETTLE_BLOCKS + MIC_ANALYZE_BLOCKS; i++ )
    {
        size_t n = microphone.read( block );
        if ( n != MIC_BLOCK_SIZE )
        {
            continue;
        }
        bool analyze = i >= MIC_SETTLE_BLOCKS;
        SoundStats s = detector.process( block, n, analyze );
        if ( analyze )
        {
            reading.valid = true;
            if ( s.filteredRms >= reading.stats.filteredRms )
            {
                reading.stats = s;
            }
        }
    }
    microphone.end();

    reading.loud = reading.valid && reading.stats.filteredRms >= SOUND_RMS_THRESHOLD;
#if SOUND_REQUIRE_ENGINE_BAND
    reading.loud = reading.loud && reading.stats.engineRatio >= ENGINE_BAND_RATIO;
#endif
    return reading;
}

// ---------------------------------------------------------------- decision

static const char *wakeName( Wake w )
{
    switch ( w )
    {
    case Wake::Vibration: return "vibration";
    case Wake::Timer:     return "timer";
    default:              return "power-on";
    }
}

static bool cooldownElapsed( uint8_t eventType, int64_t now )
{
    return now - g_lastAlarmMs[eventType] >= (int64_t) ALARM_COOLDOWN_MS;
}

static void runCycle( Wake wake )
{
    const bool vibration = wake == Wake::Vibration;
    SoundReading sound = listen();
    const bool heardSound = sound.valid && sound.loud;

    uint8_t eventType = EVT_TEST;
    if ( vibration && heardSound )   eventType = EVT_BOTH;
    else if ( vibration )            eventType = EVT_VIBRATION;
    else if ( heardSound )           eventType = EVT_SOUND;

    Serial.printf( "[%8lld ms] wake=%s | sound rms=%.5f (thr %.4f) band=%.2f f0=%.0f Hz%s | event=%u\n",
                   (long long) nowMs(), wakeName( wake ), sound.stats.filteredRms,
                   (double) SOUND_RMS_THRESHOLD, sound.stats.engineRatio, sound.stats.strongestHz,
                   sound.valid ? "" : " (no mic)", (unsigned) eventType );

    const int64_t now = nowMs();
    bool sent = false;

    if ( eventType != EVT_TEST )
    {
        if ( cooldownElapsed( eventType, now ) )
        {
            sendEvent( eventType, ALARM_COPIES );
            g_lastAlarmMs[eventType] = now;
            sent = true;
        }
        else
        {
            Serial.println( "  alarm suppressed (cooldown)" );
        }
    }

    if ( !sent && ( wake == Wake::PowerOn || now - g_lastPacketMs >= (int64_t) HEARTBEAT_MS ) )
    {
        sendEvent( EVT_TEST, 1 );   // heartbeat, raises no alarm
        sent = true;
    }

    if ( sent )
    {
        g_lastPacketMs = nowMs();
    }
}

// ---------------------------------------------------------------- sleeping

// The vibration wake is only armed when a new vibration alarm could actually be sent. While the sensor is
// still chattering during the cooldown it stays disarmed, otherwise the node would wake over and over.
static bool vibrationArmed()
{
    int64_t now = nowMs();
    return cooldownElapsed( EVT_VIBRATION, now ) && cooldownElapsed( EVT_BOTH, now );
}

#ifdef NODE_BENCH
static volatile bool vibrationFlag = false;
static void IRAM_ATTR onVibrationEdge() { vibrationFlag = true; }

// Bench mode: wait in a loop instead of deep sleep, so the USB serial log stays connected
static Wake waitForNextCycle()
{
    radioSleep();
    const bool armed = vibrationArmed();
    vibrationFlag = false;
    uint32_t start = millis();
    while ( millis() - start < SOUND_PERIOD_MS )
    {
        if ( armed && ( vibrationFlag || digitalRead( PIN_VIBRATION ) ) )
        {
            return Wake::Vibration;
        }
        delay( 2 );
    }
    return Wake::Timer;
}
#else
[[noreturn]] static void deepSleep()
{
    radioSleep();

    esp_sleep_disable_wakeup_source( ESP_SLEEP_WAKEUP_ALL );
    esp_sleep_enable_timer_wakeup( (uint64_t) SOUND_PERIOD_MS * 1000ULL );

    if ( vibrationArmed() )
    {
        esp_sleep_enable_ext1_wakeup_io( 1ULL << PIN_VIBRATION, ESP_EXT1_WAKEUP_ANY_HIGH );
        rtc_gpio_pullup_dis( (gpio_num_t) PIN_VIBRATION );
        rtc_gpio_pulldown_en( (gpio_num_t) PIN_VIBRATION );   // defined LOW if the sensor output floats
    }

    Serial.flush();
    esp_deep_sleep_start();
    while ( true ) {}   // not reached
}
#endif

#ifndef NODE_BENCH
static Wake detectWake()
{
    if ( esp_sleep_get_ext1_wakeup_status() & ( 1ULL << PIN_VIBRATION ) )
    {
        return Wake::Vibration;
    }
    if ( esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER )
    {
        return Wake::Timer;
    }
    return Wake::PowerOn;
}
#endif

void setup()
{
    Serial.begin( 115200 );
    setCpuFrequencyMhz( 80 );   // plenty for a 12 kHz audio window, saves power while awake
    pinMode( PIN_VIBRATION, INPUT );
    detector.begin();

#ifdef NODE_BENCH
    delay( 2000 );   // time to open the serial monitor
    Serial.printf( "\n=== BENCH mode (no deep sleep), sensor id %u ===\n", (unsigned) SENSOR_ID );
    attachInterrupt( digitalPinToInterrupt( PIN_VIBRATION ), onVibrationEdge, RISING );
    runCycle( Wake::PowerOn );
#else
    runCycle( detectWake() );
    deepSleep();
#endif
}

void loop()
{
#ifdef NODE_BENCH
    runCycle( waitForNextCycle() );
#endif
}
