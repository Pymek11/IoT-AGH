# PlatformIO configuration

Build settings for the firmware are in `Source/platformio.ini`. This page explains the sections,
the definitions passed to the code, and the common commands.

## `[platformio]`

```ini
[platformio]
src_dir = .
```

The sources are in the project folder itself (`Source/`), not in a `src/` folder.

## `[common]`

```ini
[common]
serial_baud = 115200 ; for normal mode
; serial_baud = 921600 ; when recording raw data thru serial (MIC_STREAM / MIC_DUAL_STREAM)
mode_flags =
```

Values shared by all environments.

- `serial_baud` is the single place where the serial speed is set. It is used for the serial
  monitor (`monitor_speed`) and for the code (`SERIAL_BAUD`). Keep only one `serial_baud` line
  active.
- `mode_flags` picks the firmware mode for both boards and is added to each environment's
  `build_flags`. Empty means engine detection. It can hold `-DDETECTOR_DEBUG`, `-DMIC_STREAM` or
  `-DMIC_DUAL_STREAM`, see the table below. The streaming modes need `serial_baud = 921600`.

## Environments

| Environment | Board | Notes |
|---|---|---|
| `esp32dev` | Classic ESP32 DevKit V1 (ESP32-WROOM-32) | Logs go through the onboard USB-UART bridge. Used for local development |
| `esp32-s3-devkitc-1` | Seeed XIAO ESP32-S3 (final board) | Logs go through the native USB port (USB CDC) |

Both use the Arduino framework on the `espressif32` platform.

`build_src_filter = +<*> +<modules/*> +<utils/*>` tells PlatformIO to compile the files in the
project folder plus everything under `modules/` and `utils/`. A new module folder has to be
placed in one of those two folders (or the filter extended). `external_tools/` is not compiled.

## Definitions passed to the code (`build_flags`)

| Define | Set in | Meaning |
|---|---|---|
| `BOARD_ESP32_DEVKIT` | `esp32dev` | Selects the pin set for the DevKit in `config.h` |
| `BOARD_XIAO_ESP32S3` | `esp32-s3-devkitc-1` | Selects the pin set for the XIAO in `config.h` |
| `SERIAL_BAUD` | both | Serial speed, taken from `common.serial_baud`; used as the default of `Logger` |
| `ARDUINO_USB_CDC_ON_BOOT=1`, `ARDUINO_USB_MODE=1` | `esp32-s3-devkitc-1` | Route `Serial` through the native USB port |
| `DETECTOR_DEBUG` | `mode_flags`, off | Also logs the detector features once a second, for tuning |
| `MIC_STREAM` | `mode_flags`, off | Streams filtered microphone blocks over serial instead of running the detector |
| `MIC_DUAL_STREAM` | `mode_flags`, off | Streams raw + filtered blocks over serial instead of running the detector |

`config.h` stops the build with an error if neither board define is set.

## Common tasks

Run these from the `Source/` folder (or use the PlatformIO buttons in VS Code).

| Task | Command |
|---|---|
| Build everything | `pio run` |
| Build one environment | `pio run -e esp32dev` |
| Upload | `pio run -e esp32dev -t upload` |
| Serial monitor | `pio device monitor -e esp32dev` |
| List serial ports | `pio device list` |

If `pio` is not found in the shell, it is installed at `~/.platformio/penv/bin/pio`.

Only one program can use a serial port at a time. Close the serial monitor before uploading or
before running a capture script.

## Normal mode

With an empty `mode_flags` the firmware runs the engine detector and the serial monitor shows
`Vehicle detected` for every block while a vehicle is heard. Add `-DDETECTOR_DEBUG` to see the
detector features once a second as well (see [detector.md](detector.md)). No WAV recording or serial streaming mode is needed for
engine detection.
