# main.cpp

Entry point of the firmware. It owns the module objects, starts them, reports failures, shuts
everything down when something goes wrong, and runs the main loop.

File: `Source/main.cpp`

## Objects

```cpp
static Logger logger( "main" );
static Microphone microphone;
```

Both are file-level statics. Module objects are created here once and live for the whole run.

## `setup()`

Runs once after boot.

1. `logger.begin()` opens the serial port (the speed is `SERIAL_BAUD`, see
   [platformio.md](platformio.md)).
2. A flag `bool anyFailures = false;` tracks whether any module failed to start.
3. Each module is started in turn. If `begin()` returns `false`, the error is logged and
   `anyFailures` is set to `true`:

   ```cpp
   if ( !microphone.begin() )
   {
       logger.log( LogLevel::ERR, "Microphone setup failed" );
       anyFailures = true;
   }
   ```

   Setup does not stop at the first failure, so one run reports every module that is broken.
4. At the end, `if ( anyFailures ) shutdown();`.

## `shutdown()`

```cpp
static void shutdown()
```

Graceful shutdown of every module. It logs a warning and calls `end()` on each module. Today that
is only `microphone.end()`.

It stops the modules but does **not** halt the board: after a failed setup `loop()` still runs.
If the board should stop, add a `while ( true ) delay( 1000 );` or deep sleep at the end of the
`if ( anyFailures )` block.

To add a module: start it in `setup()` (and set `anyFailures` on error), then call its `end()`
in `shutdown()`.

## `loop()`

Runs forever after `setup()`.

```cpp
static int32_t samples[MIC_BLOCK_SIZE];
size_t n = microphone.read( samples );
```

- `samples` is `static`, so the 4 KB buffer is in static memory and not on the stack. It costs
  nothing extra per call.
- `microphone.read()` blocks until the block is full (~85 ms at 12 kHz), so there is no `delay()`
  in the loop. The block rate is set by the microphone.
- If fewer than `MIC_BLOCK_SIZE` samples arrive, a warning is logged and the block is skipped.
- A full block is handled in one of two ways, chosen at build time:
  - **Normal mode:** min and max of the block are logged. This is a placeholder for the real
    processing (filtering / engine detection).
  - **`MIC_STREAM` mode:** the raw block is written to the serial port (see below).

The whole loop has to finish within one block period (~85 ms), otherwise samples are lost between
reads. If the processing becomes heavy, read and process in separate FreeRTOS tasks with a queue
between them (the buffer would then move to file scope or into a class).

## `MIC_STREAM` mode (debug only)

Enabled with the build flag `-DMIC_STREAM` (see [platformio.md](platformio.md)). It is meant for
recording the microphone on a PC with `external_tools/mic_tests/capture.py`.

- `setup()` calls `Logger::setEnabled( false )` first, so no text lines get mixed into the data.
- After a successful setup it prints the line `MICSTREAM`. Boot messages come before it, so the
  PC script uses it to find where the data starts.
- `loop()` writes each block to serial as raw bytes: 4 bytes per sample, little-endian `int32_t`,
  holding a signed 24-bit value.
- Needs a higher serial speed: 12 000 samples/s × 4 bytes is about 480 kbit/s, so use
  `serial_baud = 921600`.

Known issue: recordings made this way contain occasional short bursts of zeros (16 or 32 samples,
1–3 per 10 s). The data is correct in the board's memory right before sending (checked), so the
zeros appear on the way to the PC. Changing the baud rate, the write size or the UART TX buffer
did not help. The mode is not used for the actual product.
