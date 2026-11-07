#include "vibration.h"

#include "../../config.h"

// Boards without a 801S pin (XIAO) compile nothing here
#ifdef VIB_PIN

#include <esp_timer.h>
#include <soc/gpio_reg.h>

static_assert( VIB_PIN < 32, "the interrupt reads GPIO_IN_REG, which covers GPIO0..31 only" );
static_assert( ( VIB_EVENT_QUEUE_SIZE & ( VIB_EVENT_QUEUE_SIZE - 1 ) ) == 0,
               "VIB_EVENT_QUEUE_SIZE must be a power of two" );

// State shared by the interrupt and loop(), guarded by s_lock
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static VibEvent s_queue[VIB_EVENT_QUEUE_SIZE];
static uint32_t s_head = 0;              // next slot to write
static uint32_t s_tail = 0;              // next slot to read
static uint32_t s_dropped = 0;
static uint8_t s_lastLevel = 0;          // level of the last accepted edge
static int64_t s_lastEdgeUs = 0;         // time of the last accepted edge
static int64_t s_lastActiveUs = INT64_MIN / 2;
static int64_t s_lastRawEdgeUs = 0;      // time of the last interrupt, accepted or not

static inline uint8_t IRAM_ATTR readPin()
{
    return ( REG_READ( GPIO_IN_REG ) >> VIB_PIN ) & 1;
}

// Must be called with s_lock held. `eventUs` is the time stored with the edge
static inline void IRAM_ATTR acceptLevel( uint8_t level, int64_t nowUs, int64_t eventUs, bool polled )
{
    if ( level == ( VIB_ACTIVE_HIGH ? 1 : 0 ) )
    {
        s_lastActiveUs = nowUs;
    }

    if ( level == s_lastLevel || nowUs - s_lastEdgeUs < VIB_DEBOUNCE_US )
    {
        return;
    }

    s_lastLevel = level;
    s_lastEdgeUs = eventUs;

    if ( s_head - s_tail >= VIB_EVENT_QUEUE_SIZE )
    {
        s_dropped++;
        return;
    }
    s_queue[s_head & ( VIB_EVENT_QUEUE_SIZE - 1 )] = { eventUs, level, polled };
    s_head++;
}

static void IRAM_ATTR onVibEdge()
{
    const int64_t now = esp_timer_get_time();
    const uint8_t level = readPin();

    portENTER_CRITICAL_ISR( &s_lock );
    s_lastRawEdgeUs = now;
    acceptLevel( level, now, now, false );
    portEXIT_CRITICAL_ISR( &s_lock );
}

bool VibrationSensor::isActive( uint8_t level )
{
    return level == ( VIB_ACTIVE_HIGH ? 1 : 0 );
}

void VibrationSensor::begin()
{
    if ( _running )
    {
        return;
    }

    // The 801S module drives DO itself (comparator output with its own pull-up)
    pinMode( VIB_PIN, INPUT );
#if VIB_LED_PIN >= 0
    pinMode( VIB_LED_PIN, OUTPUT );
    digitalWrite( VIB_LED_PIN, LOW );
#endif

    portENTER_CRITICAL( &s_lock );
    s_head = s_tail = 0;
    s_dropped = 0;
    s_lastLevel = readPin();
    s_lastEdgeUs = esp_timer_get_time();
    s_lastActiveUs = INT64_MIN / 2;
    s_lastRawEdgeUs = s_lastEdgeUs;
    _reportedLevel = s_lastLevel;
    portEXIT_CRITICAL( &s_lock );

    attachInterrupt( digitalPinToInterrupt( VIB_PIN ), onVibEdge, CHANGE );
    _running = true;
}

void VibrationSensor::end()
{
    if ( !_running )
    {
        return;
    }
    detachInterrupt( digitalPinToInterrupt( VIB_PIN ) );
#if VIB_LED_PIN >= 0
    digitalWrite( VIB_LED_PIN, LOW );
#endif
    _running = false;
}

uint8_t VibrationSensor::level() const
{
    return readPin();
}

bool VibrationSensor::popEvent( VibEvent &event )
{
    bool found = false;
    portENTER_CRITICAL( &s_lock );
    if ( s_tail != s_head )
    {
        event = s_queue[s_tail & ( VIB_EVENT_QUEUE_SIZE - 1 )];
        s_tail++;
        found = true;
    }
    portEXIT_CRITICAL( &s_lock );

    if ( found )
    {
        _reportedLevel = event.level;
    }
    return found;
}

void VibrationSensor::poll()
{
    const int64_t now = esp_timer_get_time();
    const uint8_t level = readPin();

    portENTER_CRITICAL( &s_lock );
    // A level change the debounce hid: the pin really settled at the last interrupt, but no sooner
    // than the debounce allowed. Using that instead of `now` keeps the error below a block
    int64_t settledUs = s_lastRawEdgeUs;
    if ( settledUs < s_lastEdgeUs + VIB_DEBOUNCE_US )
    {
        settledUs = s_lastEdgeUs + VIB_DEBOUNCE_US;
    }
    if ( settledUs > now )
    {
        settledUs = now;
    }
    acceptLevel( level, now, settledUs, true );
    portEXIT_CRITICAL( &s_lock );

    updateLed( now );
}

uint32_t VibrationSensor::droppedEvents() const
{
    portENTER_CRITICAL( &s_lock );
    uint32_t dropped = s_dropped;
    portEXIT_CRITICAL( &s_lock );
    return dropped;
}

void VibrationSensor::updateLed( int64_t nowUs )
{
#if VIB_LED_PIN >= 0
    portENTER_CRITICAL( &s_lock );
    const int64_t lastActive = s_lastActiveUs;
    portEXIT_CRITICAL( &s_lock );
    digitalWrite( VIB_LED_PIN, nowUs - lastActive < (int64_t) VIB_LED_HOLD_MS * 1000 ? HIGH : LOW );
#else
    (void) nowUs;
#endif
}

#endif // VIB_PIN
