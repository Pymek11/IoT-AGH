#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>

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
    explicit Logger( const char *tag, unsigned long baudRate = 115200 );

    void begin();

    void log( LogLevel level, const char *format, ... );

private:
    const char *_tag;
    unsigned long _baudRate;
    const char *levelToString( LogLevel level );
};

#endif // LOGGER_H