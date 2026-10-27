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

#elif defined(BOARD_ESP32_DEVKIT)
// =====================================
// For localdev on ESP32DEVKIT
// =====================================
// Microphone definitions (GPIO numbers)
#define MICR_SD_PIN 18      // D18 PIN
#define MICR_WS_PIN 19      // D19 PIN
#define MICR_SCK_PIN 21     // D21 PIN

#else
#error "Unknown board: define BOARD_XIAO_ESP32S3 or BOARD_ESP32_DEVKIT in platformio.ini build_flags"
#endif

#endif
