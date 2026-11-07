#ifndef CONFIG_H
#define CONFIG_H

#if defined(BOARD_XIAO_ESP32S3)
// =====================================
// For final Xiao ESP32-S3
// =====================================
// Microphone definitions (GPIO numbers)
#define MICR_SD_PIN 2       // D1 PIN
#define MICR_WS_PIN 3       // D2 PIN
#define MICR_SCK_PIN 4      // D3 PIN

// No 801S pin on the XIAO yet (GPIO23 does not exist on the ESP32-S3), so MIC_REC is not available

#elif defined(BOARD_ESP32_DEVKIT)
// =====================================
// For localdev on ESP32 DevKit V1 (DevKit labels = GPIO numbers)
// =====================================
// Microphone definitions (GPIO numbers), INMP441 L/R tied to GND -> left slot
// SD is not on GPIO2: that is a strapping pin with the onboard blue LED, which would load the line
#define MICR_SD_PIN 19      // D19 PIN
#define MICR_WS_PIN 4       // D4 PIN
#define MICR_SCK_PIN 5      // D5 PIN (strapping pin, fine as an I2S clock output)

// 801S vibration sensor, digital output (DO) only - GPIO23 has no ADC
#define VIB_PIN 23          // D23 PIN

#else
#error "Unknown board: define BOARD_XIAO_ESP32S3 or BOARD_ESP32_DEVKIT in platformio.ini build_flags"
#endif

// =====================================
// Recorder (-DMIC_REC): microphone + 801S streamed to external_tools/recorder/rec_capture.py
// =====================================

// 1 = the 801S DO goes HIGH on vibration, 0 = it goes LOW
#define VIB_ACTIVE_HIGH 1

// Edges closer than this to the previous accepted edge are ignored (contact bounce)
#define VIB_DEBOUNCE_US 2000

// LED that lights while the 801S reports vibration, for setting the potentiometer
// -1 = off. 2 = onboard blue LED of the DevKit V1 (GPIO2 is a strapping pin, but the firmware
// drives it only after boot, so uploads are not affected)
#define VIB_LED_PIN 2

// Keep the LED on this long after the last vibration, short 801S pulses are invisible otherwise
#define VIB_LED_HOLD_MS 100

// Size of the queue of 801S edges between the interrupt and loop(), power of two
#define VIB_EVENT_QUEUE_SIZE 256

// 24-bit microphone samples are shifted right by this to get 16-bit PCM (8 = no gain)
// Lower it (7, 6, ...) to add 6 dB of gain per step if the recording is too quiet
#define REC_PCM_SHIFT 8

#endif
