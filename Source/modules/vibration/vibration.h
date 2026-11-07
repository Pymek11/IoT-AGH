#ifndef VIBRATION_H
#define VIBRATION_H

#include <Arduino.h>

// One debounced change of the 801S digital output
struct VibEvent
{
    int64_t timeUs;     // esp_timer time of the edge
    uint8_t level;      // pin level after the edge (1 = HIGH)
    bool polled;        // found by poll() after the debounce, not directly by the interrupt
};

// 801S vibration sensor read digitally on VIB_PIN (config.h)
// A CHANGE interrupt timestamps every edge, edges closer than VIB_DEBOUNCE_US to the previous one
// are dropped. poll() catches a final level the debounce may have hidden. Optionally drives an
// indicator LED (VIB_LED_PIN) for setting the potentiometer
class VibrationSensor
{
public:
    void begin();
    void end();

    // Current raw pin level (1 = HIGH)
    uint8_t level() const;

    // True when the pin is at the "vibration" level (VIB_ACTIVE_HIGH)
    static bool isActive( uint8_t level );

    // Takes the oldest edge out of the queue, returns false if the queue is empty
    bool popEvent( VibEvent &event );

    // Call often from loop(): adds an event if the pin level differs from the last reported one
    // (an edge hidden by the debounce) and updates the LED
    void poll();

    // Level of the last reported edge (what the event stream says the pin is at)
    uint8_t reportedLevel() const { return _reportedLevel; }

    // Edges lost because the queue was full
    uint32_t droppedEvents() const;

private:
    void updateLed( int64_t nowUs );

    bool _running = false;
    uint8_t _reportedLevel = 0;
    int64_t _lastActiveUs = INT64_MIN / 2;
};

#endif // VIBRATION_H
