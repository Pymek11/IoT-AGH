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
- A full block is handled in the normal on-board analysis path:
  - **Normal mode:** each block is normalized from signed 24-bit samples, passed through a
    20 Hz high-pass and 1200 Hz low-pass filter, and analyzed with a 1024-point FFT. The serial
    log contains raw/filtered RMS, peak level, sample range, strongest frequency and classification.

The whole loop has to finish within one block period (~85 ms), otherwise samples are lost between
reads. If the processing becomes heavy, read and process in separate FreeRTOS tasks with a queue
between them (the buffer would then move to file scope or into a class).

## On-board audio analysis

The firmware does not require a `.wav` file. `Microphone::read()` supplies signed 24-bit values
directly from the INMP441. Each block is converted to the `-1..1` range, filtered with cascaded
biquad high-pass/low-pass filters, windowed with a Hamming window, and transformed using an
in-place radix-2 FFT. The log reports raw bit-level diagnostics and prints `[ENGINE QUAD/MX]`
when the filtered level and the 50–1200 Hz energy ratio meet the classification thresholds.

Other audible blocks are reported as `[SOUND]`. The old PC stream/capture workflow is not part of
the detection path.
