#ifndef LORA_LINK_H
#define LORA_LINK_H

// Radio parameters and packet format shared by the sensor node (lora/node_xiao)
// and the receiver / ThingsBoard gateway (lora/receiver_lopy).
// Both sides MUST be built from this file, otherwise they will not hear each other.

#include <stddef.h>
#include <stdint.h>

// ---- Radio (EU868, LoRa P2P, no LoRaWAN) ----
constexpr float   LORA_FREQ_MHZ     = 868.1f;
constexpr float   LORA_BW_KHZ       = 125.0f;
constexpr uint8_t LORA_SF           = 7;      // 7 = fast/short, 12 = max range
constexpr uint8_t LORA_CR           = 5;      // coding rate 4/5
constexpr uint8_t LORA_SYNC_WORD    = 0x12;   // "private" network word, same value for SX126x and SX127x in RadioLib
constexpr int8_t  LORA_POWER_DBM    = 14;     // EU868 limit for this sub-band
constexpr uint16_t LORA_PREAMBLE    = 8;

// ---- Packet: 3 bytes, no addressing ----
//   [0] sensor id  (1..254)   -> ThingsBoard device "sensor-<id>"
//   [1] event type            -> telemetry key "typ" (see below)
//   [2] counter (wraps at 255) -> telemetry key "licznik", lets the dashboard spot lost packets
constexpr size_t LORA_PACKET_LEN = 3;

// Event types. The numeric values are used by the ThingsBoard "Sensor" device profile alarm rules:
//   1 or 2 -> WARNING  alarm "ATV detected" (single signal: vibration or sound)
//   3      -> CRITICAL alarm "ATV detected" (vibration and sound together)
//   0      -> no alarm (heartbeat / test packet)
enum LoraEvent : uint8_t
{
    EVT_TEST      = 0,
    EVT_VIBRATION = 1,
    EVT_SOUND     = 2,
    EVT_BOTH      = 3
};

#endif // LORA_LINK_H
