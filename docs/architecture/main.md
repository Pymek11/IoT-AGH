# main.cpp

Entry point of the firmware. It owns the module objects, starts them, reports failures, shuts
everything down when something goes wrong, and runs the main loop.

File: `Source/main.cpp`

## Objects

```cpp
static Logger logger( "main" );
static Microphone microphone;
static EngineDetector detector;
#ifdef MIC_STREAMING
static MicStream micStream;     // only in the streaming debug modes
#endif
```

All are file-level statics. Module objects are created here once and live for the whole run.

## `setup()`

Runs once after boot.

1. `logger.begin()` opens the serial port (the speed is `SERIAL_BAUD`, see
   [platformio.md](platformio.md)).
2. `detector.begin()` sets up the detector (it cannot fail).
3. A flag `bool anyFailures = false;` tracks whether any module failed to start.
4. Each module is started in turn. If it fails, the error is logged and `anyFailures` is set to
   `true`. The microphone is started by `startMicrophone()`:

   ```cpp
   if ( !startMicrophone() )
   {
       anyFailures = true;
   }
   ```

   `startMicrophone()` calls `microphone.begin()` and, in the streaming modes, `micStream.begin()`,
   which turns logging off and sends the `MICSTREAM` marker
   (see [microphone.md](microphone.md#serial-streaming-micstream)).
   Setup does not stop at the first failure, so one run reports every module that is broken.
5. If anything failed, `shutdown()` is called. `loop()` then retries the microphone.

## `shutdown()`

```cpp
static void shutdown()
```

Graceful shutdown of every module. It logs a warning and calls `end()` on each module. Today that
is only `microphone.end()`.

It stops the modules but does **not** halt the board: after a failed setup `loop()` still runs
and tries to start the microphone again.

To add a module: start it in `setup()` (and set `anyFailures` on error), then call its `end()`
in `shutdown()`.

## `loop()`

Runs forever after `setup()`.

```cpp
static int32_t samples[MIC_BLOCK_SIZE];
size_t n = microphone.read( samples );
...
bool detected = detector.process( samples );
if ( detected )
{
    logger.log( LogLevel::INFO, "Vehicle detected" );
}
```

- If the microphone is not running (it failed to start), the loop waits `MIC_RETRY_DELAY_MS`
  (5 s) and calls `startMicrophone()` again. After a successful restart `detector.reset()` clears
  the detector's history, since the old samples no longer follow on.

- `samples` is `static`, so the 4 KB buffer is in static memory and not on the stack. It costs
  nothing extra per call.
- `microphone.read()` blocks until the block is full (~85 ms at 12 kHz), so there is no `delay()`
  in the loop. The block rate is set by the microphone.
- If fewer than `MIC_BLOCK_SIZE` samples arrive, a warning is logged and the block is skipped.
- A full block is handled in the normal on-board analysis path:
  - **Normal mode:** each block goes to `EngineDetector::process()`. The sound is analyzed
    continuously, block after block. While a vehicle is detected, `Vehicle detected` is logged at
    `INFO` for every block (~12 lines per second). With `-DDETECTOR_DEBUG`, `logFeatures()` also
    logs the detector features every 12 blocks (~1 s) at `DEBUG`. See [detector.md](detector.md).
  - **`MIC_STREAM` / `MIC_DUAL_STREAM`:** the block goes to `micStream.send()`, which writes it to
    the serial port as binary for `capture.py`.

The whole loop has to finish within one block period (~85 ms), otherwise samples are lost between
reads. If the processing becomes heavy, read and process in separate FreeRTOS tasks with a queue
between them (the buffer would then move to file scope or into a class).

## On-board audio analysis

The firmware does not require a `.wav` file. `Microphone::read()` supplies signed 24-bit values
directly from the INMP441 and `EngineDetector` (`modules/detector`) analyzes them on the board:
low band level against an adaptive noise floor, periodicity from an autocorrelation, spectral
flatness and band shares from an FFT, and a decision over the last ~2 s. Details and thresholds
are in [detector.md](detector.md). The PC stream/capture workflow is not part of the detection
path.
