// 801S vibration sensor bring-up: digital output (HIGH/LOW) on D0 and/or D1 of the XIAO ESP32S3.
// Watches both pins and prints every level change, plus a one-line summary each second:
//   t=<s> D0=<level> edges=<changes in the last second>  D1=<level> edges=<...>
// Tap or shake the sensor: the pin that is wired to the sensor output toggles ("edges" > 0).
// Adjust the sensitivity potentiometer on the module if it never toggles or never stops toggling.

#include <Arduino.h>

constexpr int PIN_D0 = 1;   // GPIO1
constexpr int PIN_D1 = 2;   // GPIO2

struct Watched
{
    const char *name;
    int pin;
    int last;
    uint32_t edges;
};

static Watched watched[] = { { "D0", PIN_D0, 0, 0 }, { "D1", PIN_D1, 0, 0 } };

void setup()
{
    Serial.begin( 115200 );
    delay( 2000 );
    Serial.println( "\n=== 801S vibration sensor test (D0=GPIO1, D1=GPIO2) ===" );
    for ( Watched &w : watched )
    {
        pinMode( w.pin, INPUT );
    }
    delay( 20 );
    for ( Watched &w : watched )
    {
        w.last = digitalRead( w.pin );
        Serial.printf( "%s idle level: %d\n", w.name, w.last );
    }
}

void loop()
{
    static uint32_t lastReport = 0;

    for ( Watched &w : watched )
    {
        int level = digitalRead( w.pin );
        if ( level != w.last )
        {
            w.last = level;
            w.edges++;
            Serial.printf( "[%8lu ms] %s -> %s\n", (unsigned long) millis(), w.name, level ? "HIGH" : "LOW" );
        }
    }

    if ( millis() - lastReport >= 1000 )
    {
        lastReport = millis();
        Serial.printf( "t=%lu D0=%d edges=%lu  D1=%d edges=%lu\n", (unsigned long) ( millis() / 1000 ),
                       watched[0].last, (unsigned long) watched[0].edges,
                       watched[1].last, (unsigned long) watched[1].edges );
        watched[0].edges = 0;
        watched[1].edges = 0;
    }
    delayMicroseconds( 200 );
}
