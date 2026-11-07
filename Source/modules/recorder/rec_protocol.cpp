#include "rec_protocol.h"

#include <cstring>

#include "../../config.h"

namespace
{

uint16_t crc16Update( uint16_t crc, const uint8_t *data, size_t length )
{
    for ( size_t i = 0; i < length; i++ )
    {
        crc ^= static_cast<uint16_t>( data[i] ) << 8;
        for ( int bit = 0; bit < 8; bit++ )
        {
            crc = ( crc & 0x8000 ) ? static_cast<uint16_t>( ( crc << 1 ) ^ 0x1021 )
                                   : static_cast<uint16_t>( crc << 1 );
        }
    }
    return crc;
}

// Little-endian field writers, return the position after the field
uint8_t *put8( uint8_t *p, uint8_t v )
{
    *p = v;
    return p + 1;
}

uint8_t *put16( uint8_t *p, uint16_t v )
{
    p[0] = v & 0xFF;
    p[1] = v >> 8;
    return p + 2;
}

uint8_t *put32( uint8_t *p, uint32_t v )
{
    for ( int i = 0; i < 4; i++ )
    {
        p[i] = ( v >> ( 8 * i ) ) & 0xFF;
    }
    return p + 4;
}

} // namespace

void RecProtocol::sendFrame( RecFrame type, const uint8_t *head, size_t headLength,
                             const uint8_t *tail, size_t tailLength )
{
    const uint16_t length = static_cast<uint16_t>( headLength + tailLength );

    uint8_t header[5];
    header[0] = REC_SYNC_0;
    header[1] = REC_SYNC_1;
    header[2] = static_cast<uint8_t>( type );
    put16( &header[3], length );

    uint16_t crc = crc16Update( 0xFFFF, &header[2], 3 );
    crc = crc16Update( crc, head, headLength );
    if ( tailLength )
    {
        crc = crc16Update( crc, tail, tailLength );
    }
    uint8_t crcBytes[2];
    put16( crcBytes, crc );

    Serial.write( header, sizeof( header ) );
    Serial.write( head, headLength );
    if ( tailLength )
    {
        Serial.write( tail, tailLength );
    }
    Serial.write( crcBytes, sizeof( crcBytes ) );
}

void RecProtocol::sendStart( uint32_t sampleRate, uint8_t bits, uint16_t blockSize, uint8_t vibLevel,
                             uint8_t mode, uint32_t preTriggerSamples )
{
    uint8_t payload[14];
    uint8_t *p = payload;
    p = put32( p, sampleRate );
    p = put8( p, bits );
    p = put16( p, blockSize );
    p = put8( p, vibLevel );
    p = put8( p, VIB_ACTIVE_HIGH ? 1 : 0 );
    p = put8( p, mode );
    p = put32( p, preTriggerSamples );
    sendFrame( RecFrame::START, payload, p - payload );
}

void RecProtocol::sendStop( uint32_t totalSamples, uint32_t droppedVibEvents )
{
    uint8_t payload[8];
    uint8_t *p = payload;
    p = put32( p, totalSamples );
    p = put32( p, droppedVibEvents );
    sendFrame( RecFrame::STOP, payload, p - payload );
}

void RecProtocol::sendArmed( bool armed, uint8_t mode )
{
    const uint8_t payload[2] = { static_cast<uint8_t>( armed ? 1 : 0 ), mode };
    sendFrame( RecFrame::ARMED, payload, sizeof( payload ) );
}

void RecProtocol::sendAudio( uint32_t firstSample, uint8_t vibLevel, uint8_t flags, uint16_t peak,
                             uint16_t clipped, const int16_t *samples, size_t count )
{
    uint8_t head[10];
    uint8_t *p = head;
    p = put32( p, firstSample );
    p = put8( p, vibLevel );
    p = put8( p, flags );
    p = put16( p, peak );
    p = put16( p, clipped );
    // int16 samples go as they are, the ESP32 is little-endian
    sendFrame( RecFrame::AUDIO, head, p - head, reinterpret_cast<const uint8_t *>( samples ),
               count * sizeof( int16_t ) );
}

void RecProtocol::sendVib( uint32_t sampleIndex, uint8_t level, uint8_t flags )
{
    uint8_t payload[6];
    uint8_t *p = payload;
    p = put32( p, sampleIndex );
    p = put8( p, level );
    p = put8( p, flags );
    sendFrame( RecFrame::VIB, payload, p - payload );
}
