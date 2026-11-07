#ifndef REC_PROTOCOL_H
#define REC_PROTOCOL_H

#include <Arduino.h>

// Binary frames sent over Serial to external_tools/recorder/rec_capture.py
// Everything is little-endian. A frame is:
//
//   0xA5 0x5A | type u8 | length u16 | payload (length bytes) | crc16 u16
//
// crc16 is CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF) over type, length and payload.
// Text (logs) may appear between frames; the PC side skips everything that is not a valid frame.
// Keep the types and layouts in sync with rec_capture.py

constexpr uint8_t REC_SYNC_0 = 0xA5;
constexpr uint8_t REC_SYNC_1 = 0x5A;

enum class RecFrame : uint8_t
{
    // u32 sample_rate, u8 bits_per_sample, u16 block_size, u8 vib_level, u8 vib_active_high,
    // u8 mode (0 = continuous, 1 = triggered), u32 pre_trigger_samples
    START = 0x10,
    // u32 total_samples, u32 dropped_vib_events
    STOP = 0x11,
    // u8 armed (1 = waiting for vibration, 0 = disarmed), u8 mode - answer to START / STOP in the
    // triggered mode, where a recording starts later, on vibration
    ARMED = 0x12,
    // u32 first_sample_index, u8 vib_level (at the end of the block), u8 flags (bit0: vibration seen
    // in the block), u16 peak_abs, u16 clipped_count, int16 samples[]
    AUDIO = 0x20,
    // u32 sample_index, u8 level, u8 flags (bit0: trigger, bit1: found by polling, not by interrupt)
    VIB = 0x30,
};

constexpr uint8_t REC_MODE_CONTINUOUS = 0;
constexpr uint8_t REC_MODE_TRIGGERED = 1;

constexpr uint8_t REC_AUDIO_FLAG_VIB_SEEN = 0x01;
constexpr uint8_t REC_VIB_FLAG_TRIGGER = 0x01;
constexpr uint8_t REC_VIB_FLAG_POLLED = 0x02;

// Writes frames to Serial
class RecProtocol
{
public:
    static void sendStart( uint32_t sampleRate, uint8_t bits, uint16_t blockSize, uint8_t vibLevel,
                           uint8_t mode, uint32_t preTriggerSamples );
    static void sendStop( uint32_t totalSamples, uint32_t droppedVibEvents );
    static void sendArmed( bool armed, uint8_t mode );
    static void sendAudio( uint32_t firstSample, uint8_t vibLevel, uint8_t flags, uint16_t peak,
                           uint16_t clipped, const int16_t *samples, size_t count );
    static void sendVib( uint32_t sampleIndex, uint8_t level, uint8_t flags );

private:
    // Sends a frame whose payload is `head` followed by `tail` (tail may be empty)
    static void sendFrame( RecFrame type, const uint8_t *head, size_t headLength,
                           const uint8_t *tail = nullptr, size_t tailLength = 0 );
};

#endif // REC_PROTOCOL_H
