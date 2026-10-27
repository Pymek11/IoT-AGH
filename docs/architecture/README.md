# Firmware architecture

Firmware for an ESP32 that listens to a microphone and will be used to detect the sound of a
quad / motorcycle / cross engine. This folder describes how the code in `Source/` is built.
Research and component choices are in [`../design`](../design).

## Overview

The firmware is a small set of independent modules coordinated by `main.cpp`. Each module is a
class that owns one piece of hardware or one service, and has a `begin()` / `end()` pair so that
`main.cpp` can start it and shut it down gracefully.

```
                  ┌──────────────────────────┐
                  │         main.cpp         │  setup(): start modules, collect failures
                  │  setup() / loop() /      │  loop():  read data, hand it on
                  │  shutdown()              │  shutdown(): stop every module
                  └──────┬────────────┬──────┘
                         │            │
              uses       │            │      uses
          ┌──────────────▼───┐   ┌────▼──────────────┐
          │    Microphone    │   │      Logger       │
          │ modules/microphone│  │ utils/logger      │
          │ INMP441 over I2S │   │ text logs → Serial │
          └────────┬─────────┘   └───────────────────┘
                   │ pins                 ▲
          ┌────────▼─────────┐            │ every module has its own Logger (tag)
          │     config.h     │            │
          │ pins per board   │            │
          └──────────────────┘
```

Build settings (boards, serial speed, build flags) live in `platformio.ini`.

## Data flow

1. `Microphone::read()` fills a buffer with one block of samples (1024 samples, ~85 ms at 12 kHz).
   The samples are signed 24-bit values with the DC offset already removed.
2. `loop()` in `main.cpp` receives the block. For now it only logs the block's min/max; this is the
   place where the filtering / engine detection will be connected.
3. All text goes to the serial port through `Logger`.

## Table of contents

| Document | What it describes |
|---|---|
| [main.md](main.md) | `main.cpp`: startup sequence, failure handling, shutdown, the main loop |
| [microphone.md](microphone.md) | `Microphone` class: INMP441 wiring, I2S setup, sample format, DC offset removal |
| [logger.md](logger.md) | `Logger` class: log format, levels, enabling / disabling, baud rate |
| [platformio.md](platformio.md) | `platformio.ini`: environments, build flags, definitions, how to build and flash |

## Source layout

```
Source/
├── main.cpp                    entry point (setup / loop / shutdown)
├── config.h                    pin definitions per board
├── platformio.ini              build configuration
├── modules/
│   └── microphone/
│       ├── mic.h               Microphone class + audio constants
│       └── mic.cpp
├── utils/
│   └── logger/
│       ├── logger.h            Logger class
│       └── logger.cpp
└── external_tools/
    └── mic_tests/              Python tools on the PC (not part of the firmware)
```

## Conventions

- One class per module, in its own folder under `modules/` (hardware, services) or `utils/`
  (helpers shared by many modules).
- A module starts with `begin()` (returns `true` on success) and stops with `end()`.
  `end()` is safe to call when the module is not running.
- Each `.cpp` file creates its own `static Logger logger( "tag" )` so every log line shows where
  it came from.
- Pins are never written in module code; they come from `config.h`.
- Adding a module: create the class, start it in `setup()` (set `anyFailures` on error) and add
  its `end()` to `shutdown()`. See [main.md](main.md).
