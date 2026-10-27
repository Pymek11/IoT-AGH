# Logger

Writes timestamped, levelled text lines to the serial port. Every module has its own `Logger`
with a tag, so each line shows where it came from.

Files: `Source/utils/logger/logger.h`, `Source/utils/logger/logger.cpp`

## Output format

```
00:00:08.560 | INFO  -> Block of 1024 samples, min -6431260, max 7752945 [main]
^time since boot  ^level   ^message                                      ^tag
```

- **Time:** `HH:MM:SS.mmm` since boot, from `millis()`. The hours wrap after 24 h.
- **Level:** `DEBUG`, `INFO `, `WARN `, `ERROR` (padded to 5 characters so the columns line up).
- **Message:** `printf`-style formatting. Messages are cut at 255 characters.
- **Tag:** the module name given to the constructor.

## `LogLevel`

| Level | Use for |
|---|---|
| `DEBUG` | Detailed information while developing |
| `INFO` | Normal events (module started, status) |
| `WARN` | Something unexpected that does not stop the work |
| `ERR` | Something failed |

There is no level filtering: every call that is made gets printed (unless logging is disabled).

## `Logger` class

```cpp
class Logger
{
public:
    explicit Logger( const char *tag, unsigned long baudRate = SERIAL_BAUD );
    void begin();
    static void setEnabled( bool enabled );
    static bool isEnabled();
    void log( LogLevel level, const char *format, ... );
};
```

### Constructor

`Logger( tag, baudRate )` only stores the values. `tag` must be a string that stays valid (a
literal is fine). The baud rate defaults to `SERIAL_BAUD`.

### `begin()`

Opens the serial port with `Serial.begin( baudRate )` and waits 500 ms. It must be called **once**,
by the logger in `main.cpp`. The loggers in other modules never call it; they use the port that
`main.cpp` opened.

### `log( level, format, ... )`

Builds the time stamp and the message and prints one line. Example:

```cpp
logger.log( LogLevel::INFO, "I2S started: %lu Hz", (unsigned long) MIC_SAMPLE_RATE_HZ );
```

Use the right `printf` casts for the types (`%lu` with `unsigned long`, `%u` with `unsigned`).
Output goes to `Serial` only. The class is not protected against being called from several
FreeRTOS tasks at once, so lines from different tasks can interleave.

### `setEnabled( bool )` / `isEnabled()`

Static, so one call affects every `Logger` instance. While logging is disabled, `log()` returns
immediately, before anything is formatted. Logging is enabled by default.

It can be disabled by callers that need an exclusive serial data channel. The microphone analysis
path keeps it enabled so diagnostic and detection messages are visible (see [main.md](main.md)).

## Baud rate

The default speed comes from the define `SERIAL_BAUD`:

```cpp
#ifndef SERIAL_BAUD
#define SERIAL_BAUD 115200
#endif
```

`platformio.ini` sets `SERIAL_BAUD` and the serial monitor speed (`monitor_speed`) from the same
value (`common.serial_baud`), so the two cannot get out of sync. To change it, edit
`serial_baud` in `platformio.ini`; no code changes are needed. See
[platformio.md](platformio.md).

A per-instance speed can still be passed to the constructor, but only the instance that calls
`begin()` matters.

On the ESP32-S3 with native USB (CDC) the baud rate setting is ignored by the USB link.

## Adding logging to a module

```cpp
#include "../../utils/logger/logger.h"

static Logger logger( "mymodule" );

// ...
logger.log( LogLevel::ERR, "Something failed: %d", code );
```
