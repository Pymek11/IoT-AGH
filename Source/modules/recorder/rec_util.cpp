#include "rec_util.h"

#include <strings.h>

#include "../../config.h"

void SampleClock::reset()
{
    _count = 0;
    _next = 0;
    _originUs = 0;
}

void SampleClock::onBlock( uint32_t endIndex, int64_t nowUs )
{
    const int64_t origin = nowUs - (int64_t) endIndex * 1000000LL / _rate;
    _origins[_next] = origin;
    _next = ( _next + 1 ) % WINDOW;
    if ( _count < WINDOW )
    {
        _count++;
    }

    int64_t best = _origins[0];
    for ( size_t i = 1; i < _count; i++ )
    {
        if ( _origins[i] < best )
        {
            best = _origins[i];
        }
    }
    _originUs = best;
}

uint32_t SampleClock::indexAt( int64_t timeUs ) const
{
    if ( _count == 0 || timeUs <= _originUs )
    {
        return 0;
    }
    return (uint32_t) ( ( timeUs - _originUs ) * _rate / 1000000LL );
}

PcmLevels toPcm16( const int32_t *in, int16_t *out, size_t count )
{
    PcmLevels levels = { 0, 0 };
    for ( size_t i = 0; i < count; i++ )
    {
        int32_t value = in[i] >> REC_PCM_SHIFT;
        if ( value >= INT16_MAX )
        {
            value = INT16_MAX;
            levels.clipped++;
        }
        else if ( value <= INT16_MIN )
        {
            value = INT16_MIN;
            levels.clipped++;
        }
        out[i] = (int16_t) value;

        const uint16_t magnitude = (uint16_t) ( value < 0 ? -value : value );
        if ( magnitude > levels.peak )
        {
            levels.peak = magnitude;
        }
    }
    return levels;
}

RecCommand CommandReader::poll()
{
    while ( Serial.available() > 0 )
    {
        const char c = (char) Serial.read();
        if ( c != '\n' && c != '\r' )
        {
            if ( _length < sizeof( _line ) - 1 )
            {
                _line[_length++] = c;
            }
            else
            {
                _overflow = true;
            }
            continue;
        }

        _line[_length] = '\0';
        const bool valid = !_overflow && _length > 0;
        _length = 0;
        _overflow = false;
        if ( !valid )
        {
            continue;
        }

        if ( strcasecmp( _line, "START" ) == 0 )
        {
            return RecCommand::START;
        }
        if ( strcasecmp( _line, "STOP" ) == 0 )
        {
            return RecCommand::STOP;
        }
    }
    return RecCommand::NONE;
}
