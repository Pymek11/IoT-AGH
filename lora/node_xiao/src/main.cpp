// LoRa P2P transmitter - XIAO ESP32S3 + Wio-SX1262 kit
// Sends a 3-byte packet [sensor id, event type, counter] (see lora/common/lora_link.h).
// Currently sends a test packet every 10 s; replace sendEvent( EVT_TEST ) with the real
// vibration / microphone detection result when it is integrated.

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
constexpr uint32_t TEST_PERIOD_MS = 10000;        // fits in the 1% duty cycle at SF7 (3-byte packet ~ 30 ms)

static uint8_t counter = 0;

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
    Serial.printf( "Transmitter ready, sensor id %u\n", (unsigned) SENSOR_ID );
}

void loop()
{
    sendEvent( EVT_TEST );   // type 0 = heartbeat/test, raises no alarm on the dashboard
    delay( TEST_PERIOD_MS );
}
