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

// =====================================
// Triggered recording (MIC_REC on this branch): the 801S starts and extends each recording
// =====================================

// 0 = I2S runs all the time into a pre-trigger buffer, the start of a pass is kept (default)
// 1 = I2S is started only on a trigger: less current, but the first ~0.1-0.2 s after it are lost
//     (microphone start-up) and there is no audio from before the trigger
#ifndef TRIG_I2S_ON_DEMAND
#define TRIG_I2S_ON_DEMAND 0
#endif

// Spike filter: a recording starts after TRIG_PULSES vibration pulses within TRIG_WINDOW_MS
// (a pulse = the 801S going to its active level). 1 = start on the first pulse
#define TRIG_PULSES 1
#define TRIG_WINDOW_MS 500

// 1 = when armed, the pin simply being at the active level starts a recording (no edge needed,
// the spike filter is skipped). For testing; 0 = only the pulse filter above starts it
#define TRIG_ON_LEVEL 1

// The recording ends after this long without vibration. Every pulse, and the pin staying at the
// active level, extends it
#define TRIG_HOLD_MS 5000

// Audio kept from before the trigger (pre-trigger buffer, only with TRIG_I2S_ON_DEMAND 0)
// Rounded up to whole audio blocks (1024 samples, ~85 ms at 12 kHz)
#define TRIG_PRE_MS 1000

// 1 = armed (waiting for vibration) right after boot, 0 = only after "START" from rec_capture.py
#define TRIG_ARMED_AT_BOOT 0

#endif
