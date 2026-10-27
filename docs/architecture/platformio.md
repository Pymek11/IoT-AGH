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
serial_baud = 115200 ; for normal modes
; serial_baud = 921600 ; when recording raw data thru serial
```

Values shared by all environments. `serial_baud` is the single place where the serial speed is
set; it is used for the serial monitor (`monitor_speed`) and for the code (`SERIAL_BAUD`). Keep
only one `serial_baud` line active.

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
| `MIC_STREAM` | optional, commented out | Debug mode that streams raw samples over serial (see [main.md](main.md)) |

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

## Normal mode and stream mode

| | Normal mode | Stream mode (debug) |
|---|---|---|
| `serial_baud` | `115200` | `921600` |
| `-DMIC_STREAM` | commented out | enabled |
| Serial output | text logs | raw samples (no text) |
| Tool | serial monitor | `external_tools/mic_tests/capture.py` |

To switch, change both settings above, then build and upload again. Switch back for normal use.

## Capturing the microphone on a PC (debug tool)

`Source/external_tools/mic_tests/` has a Python script that records the stream and saves it as a
24-bit mono `.wav` file:

```
cd Source/external_tools/mic_tests
source .venv/bin/activate
python capture.py --seconds 10 -o recordings/name.wav
```

Defaults match the firmware (12 000 Hz, 921600 baud). The virtual environment (`.venv/`) is not
committed; create it with `python3 -m venv .venv` and `pip install -r requirements.txt`.
See the known issue about glitches in [main.md](main.md).
