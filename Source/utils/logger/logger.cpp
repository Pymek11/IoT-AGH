#include "logger.h"

#include <cstdio>
#include <cstdarg>

bool Logger::_enabled = true;

Logger::Logger( const char *tag, unsigned long baudRate )
    : _tag( tag ), _baudRate( baudRate ) {}

void Logger::setEnabled( bool enabled )
{
    _enabled = enabled;
}

bool Logger::isEnabled()
{
    return _enabled;
}

void Logger::begin()
{
    Serial.begin( _baudRate );
    delay( 500 );
}

const char *Logger::levelToString( LogLevel level )
{
    switch ( level )
    {
    case LogLevel::DEBUG:
        return "DEBUG";
    case LogLevel::INFO:
        return "INFO ";
    case LogLevel::WARN:
        return "WARN ";
    case LogLevel::ERR:
        return "ERROR";
    }
    return "UNKNOWN";
}

void Logger::log( LogLevel level, const char *format, ... )
{
    if ( !_enabled )
    {
        return;
    }

    // Time from startup
    unsigned long current_time_ms = millis();
    unsigned long ms = current_time_ms % 1000;
    unsigned long seconds = current_time_ms / 1000;
    unsigned long minutes = seconds / 60;
    unsigned long hours = minutes / 60;

    char time_str[ 25 ];
    snprintf( time_str, sizeof( time_str ), "%02lu:%02lu:%02lu.%03lu",
             hours % 24, minutes % 60, seconds % 60, ms );

    char message_buffer[ 256 ];
    va_list args;
    va_start( args, format );
    vsnprintf( message_buffer, sizeof(message_buffer), format, args );
    va_end( args );

    Serial.printf( "%s | %s -> %s [%s]\n", 
        time_str, levelToString( level ), 
        message_buffer, _tag );
}