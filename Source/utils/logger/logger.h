#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>

// Serial speed, set in platformio.ini (common.serial_baud) together with monitor_speed
#ifndef SERIAL_BAUD
#define SERIAL_BAUD 115200
#endif

enum class LogLevel
{
    DEBUG,
    INFO,
    WARN,
    ERR
};

class Logger
{
public:
    // Module name to use in logs (i.e. main.cpp) and optional speed
    explicit Logger( const char *tag, unsigned long baudRate = SERIAL_BAUD );

    void begin();

    // Turns logging on or off for every Logger instance (i.e. to free the serial port for raw data)
    static void setEnabled( bool enabled );
    static bool isEnabled();

    // Does nothing while logging is disabled
    void log( LogLevel level, const char *format, ... );

private:
    static bool _enabled;

    const char *_tag;
    unsigned long _baudRate;
    const char *levelToString( LogLevel level );
};

#endif // LOGGER_H