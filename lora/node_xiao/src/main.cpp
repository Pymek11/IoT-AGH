// LoRa P2P transmitter - XIAO ESP32S3 + Wio-SX1262 kit
// Sends a 3-byte packet [sensor id, event type, counter] (see lora/common/lora_link.h):
//   - EVT_VIBRATION when the 801S vibration sensor (digital output on D0) triggers
//   - EVT_TEST (heartbeat, no alarm) every HEARTBEAT_MS so the dashboard shows the sensor as online
// The microphone / engine-sound detection is not connected yet (EVT_SOUND / EVT_BOTH are unused).

#include <Arduino.h>
#include <RadioLib.h>

#include "lora_link.h"

// Sensor id, override with -DSENSOR_ID=<n> in platformio.ini. Becomes device "sensor-<id>" in ThingsBoard
#ifndef SENSOR_ID
#define SENSOR_ID 4
#endif

// XIAO ESP32S3 + Wio-SX1262 kit pins: NSS, DIO1, RST, BUSY
SX1262 radio = new Module( 41, 39, 42, 40 );

constexpr float TCXO_V = 1.8f;                    // Wio-SX1262: TCXO powered from DIO3 (1.7-3.3 V)

// 801S vibration sensor: digital output on D0 (GPIO1). It is silent at rest and chatters (many HIGH/LOW
// changes per second) while shaken, so vibration = enough edges inside a short window.
constexpr int PIN_VIBRATION = 1;                  // D0
constexpr uint32_t VIB_WINDOW_MS = 200;
constexpr uint32_t VIB_MIN_EDGES = 3;             // edges per window that count as vibration (idle = 0)
constexpr uint32_t VIB_COOLDOWN_MS = 5000;        // min time between vibration packets (duty cycle, no flooding)
constexpr uint32_t HEARTBEAT_MS = 60000;

static uint8_t counter = 0;
static volatile uint32_t vibEdges = 0;

void IRAM_ATTR onVibrationEdge()
{
    vibEdges = vibEdges + 1;   // single writer (this ISR), so no atomic needed
}

static bool startRadio()
{
    int st = radio.begin( LORA_FREQ_MHZ, LORA_BW_KHZ, LORA_SF, LORA_CR, LORA_SYNC_WORD,
                          LORA_POWER_DBM, LORA_PREAMBLE, TCXO_V );
    if ( st != RADIOLIB_ERR_NONE )
    {
        Serial.printf( "radio.begin() error: %d\n", st );
        return false;
    }
    radio.setDio2AsRfSwitch( true );   // DIO2 drives the antenna switch
    return true;
}

static void sendEvent( uint8_t eventType )
{
    uint8_t payload[LORA_PACKET_LEN] = { (uint8_t) SENSOR_ID, eventType, counter++ };

    int st = radio.transmit( payload, sizeof( payload ) );
    if ( st == RADIOLIB_ERR_NONE )
    {
        Serial.printf( "Sent sensor=%u type=%u #%u\n", payload[0], payload[1], payload[2] );
    }
    else
    {
        Serial.printf( "transmit() error: %d\n", st );
    }
}

void setup()
{
    Serial.begin( 115200 );
    delay( 2000 );

    // Retry instead of hanging forever, so a loose module does not need a manual reset
    while ( !startRadio() )
    {
        delay( 2000 );
    }
    pinMode( PIN_VIBRATION, INPUT );
    attachInterrupt( digitalPinToInterrupt( PIN_VIBRATION ), onVibrationEdge, CHANGE );

    Serial.printf( "Transmitter ready, sensor id %u\n", (unsigned) SENSOR_ID );
    sendEvent( EVT_TEST );   // announce ourselves right away
}

void loop()
{
    static uint32_t windowStart = 0;
    static uint32_t lastVibrationTx = 0;
    static bool vibrationSentOnce = false;
    static uint32_t lastTx = millis();

    uint32_t now = millis();

    if ( now - windowStart >= VIB_WINDOW_MS )
    {
        windowStart = now;
        noInterrupts();
        uint32_t edges = vibEdges;
        vibEdges = 0;
        interrupts();

        if ( edges >= VIB_MIN_EDGES &&
             ( !vibrationSentOnce || now - lastVibrationTx >= VIB_COOLDOWN_MS ) )
        {
            Serial.printf( "Vibration detected (%lu edges in %lu ms)\n", (unsigned long) edges,
                           (unsigned long) VIB_WINDOW_MS );
            sendEvent( EVT_VIBRATION );
            vibrationSentOnce = true;
            lastVibrationTx = now;
            lastTx = now;
        }
    }

    if ( now - lastTx >= HEARTBEAT_MS )
    {
        sendEvent( EVT_TEST );   // type 0 = heartbeat, raises no alarm
        lastTx = now;
    }

    delay( 10 );
}
