#include "mic_stream.h"

#include "../../utils/logger/logger.h"

void MicStream::begin()
{
    _highPass.setHighPass( MIC_SAMPLE_RATE_HZ, MIC_STREAM_HIGH_PASS_HZ );
    _lowPass.setLowPass( MIC_SAMPLE_RATE_HZ, MIC_STREAM_LOW_PASS_HZ );

    // Text before the marker is skipped by the capture script, after it only samples may follow
    Logger::setEnabled( false );
    Serial.print( "MICSTREAM\n" );
}

void MicStream::send( const int32_t *samples )
{
    for ( size_t i = 0; i < MIC_BLOCK_SIZE; i++ )
    {
        const float normalized = static_cast<float>( samples[i] ) / 8388608.0f;
        const float filtered = _lowPass.process( _highPass.process( normalized ) );
        _filtered[i] = static_cast<int32_t>( constrain( filtered, -1.0f, 1.0f ) * 8388607.0f );
    }

#ifdef MIC_DUAL_STREAM
    Serial.write( reinterpret_cast<const uint8_t *>( samples ), MIC_BLOCK_SIZE * sizeof( int32_t ) );
#endif
    Serial.write( reinterpret_cast<const uint8_t *>( _filtered ), MIC_BLOCK_SIZE * sizeof( int32_t ) );
}
