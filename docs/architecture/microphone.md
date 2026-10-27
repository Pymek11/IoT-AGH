# Microphone module

Reads sound from an INMP441 digital MEMS microphone over I2S and gives the rest of the firmware
clean, ready-to-use samples.

Files: `Source/modules/microphone/mic.h`, `Source/modules/microphone/mic.cpp`

## Hardware

INMP441 outputs 24-bit samples over I2S. The ESP32 is the I2S master (it generates the clock);
the microphone needs no MCLK and has no data input.

| INMP441 | `esp32dev` | XIAO ESP32-S3 | Constant in `config.h` |
|---|---|---|---|
| SD (data) | GPIO 18 | D1 (GPIO 2) | `MICR_SD_PIN` |
| WS (word select) | GPIO 19 | D2 (GPIO 3) | `MICR_WS_PIN` |
| SCK (bit clock) | GPIO 21 | D3 (GPIO 4) | `MICR_SCK_PIN` |
| VDD | 3.3 V | 3.3 V | |
| GND | GND | GND | |
| L/R | GND | GND | |

L/R must be tied to GND: the module then sends its data in the **left** slot, which is the slot
the firmware reads. If L/R is floating or high, the readings are silence.

Which pin set is used depends on the board define from `platformio.ini`
(`BOARD_ESP32_DEVKIT` or `BOARD_XIAO_ESP32S3`), see [platformio.md](platformio.md).

## Constants (`mic.h`)

| Constant | Value | Meaning |
|---|---|---|
| `MIC_SAMPLE_RATE_HZ` | 12000 | Sample rate. The research ([`../design/microphone`](../design/microphone)) targets 100–2000 Hz (max 4000 Hz), which 12 kHz covers (Nyquist 6 kHz) |
| `MIC_BITS_PER_SAMPLE` | 24 | Resolution of the INMP441 (sent in a 32-bit I2S slot) |
| `MIC_BLOCK_SIZE` | 1024 | Samples per block, ~85 ms at 12 kHz. A power of two, so it fits a radix-2 FFT. Frequency resolution is 12000 / 1024 ≈ 11.7 Hz per bin |
| `MIC_DC_FILTER_SHIFT` | 10 | Time constant of the DC offset filter (see below) |

## `Microphone` class

```cpp
class Microphone
{
public:
    bool begin();
    void end();
    bool isRunning() const;
    size_t read( int32_t *buffer, size_t count = MIC_BLOCK_SIZE );
};
```

The class owns the I2S driver (`I2SClass _i2s`), so it cannot be copied.

### `begin()`

- Does nothing and returns `true` if already running.
- Sets the pins (`setPins( SCK, WS, -1, SD, -1 )`; no data output, no MCLK) and starts I2S in
  standard mode: `MIC_SAMPLE_RATE_HZ`, 32-bit slot, mono, left slot.
- Returns `false` and logs an error if the driver cannot start (typically a pin problem).
- Resets the DC filter state.

### `end()`

Stops the I2S driver. Safe to call when not running. The destructor calls it too, so the driver
is released if the object is destroyed.

### `read( buffer, count )`

Reads up to `count` samples into `buffer` and returns how many were actually read (0 if the
microphone is not running). `buffer` must have room for `count` `int32_t` values.

- The call **blocks** until `count` samples are available, so it paces the caller: 1024 samples
  take ~85 ms at 12 kHz. The underlying read has a timeout (1 s by default), after which fewer
  samples than requested can be returned. Callers should check the returned count.
- The values are **signed 24-bit** (−8388608 … 8388607) stored in an `int32_t`, with the DC offset
  removed. Consumers need no further conversion. To get a float in −1…1, divide by 8388608.

## What `read()` does to the data

For every sample, in place in the caller's buffer:

1. **Shift:** the I2S slot is 32 bits wide and the 24-bit value is left aligned in it. The
   sample is shifted right by 8 bits (an arithmetic shift, so the sign is kept).
2. **DC offset removal:** the INMP441 has a small constant offset. A one-pole DC blocker tracks
   the slow average of the signal and subtracts it:

   ```
   acc += x - (acc >> N)         // acc is a 64-bit running estimate, scaled by 2^N
   out  = x - (acc >> N)
   ```

   With `N = MIC_DC_FILTER_SHIFT = 10` the time constant is 1024 samples (~85 ms) and the cutoff
   about 2 Hz at 12 kHz, far below the engine band, so the useful signal is not affected.

Details:
- The filter state is kept between `read()` calls, so consecutive blocks are consistent.
- The estimate starts at the first sample after `begin()`, which avoids a large step at startup.
- After a very loud transient (a clap) the filter needs about 0.3 s to settle, and blocks right
  after it can look lopsided around zero. Raise `MIC_DC_FILTER_SHIFT` (for example to 12) for a
  slower, steadier filter at the cost of a slower initial settle.

## Typical values

Measured with the real microphone:

- Quiet room: samples within about ±5 000 … ±12 000 (~0.1 % of full scale, about −57 dB).
- Hand clap next to the microphone: up to ±8 000 000, close to full scale (±8 388 608).
  A signal pinned near full scale means the microphone is overloaded; move it away or shield it.

## Usage

```cpp
Microphone microphone;

void setup()
{
    if ( !microphone.begin() ) { /* handle the failure */ }
}

void loop()
{
    static int32_t samples[MIC_BLOCK_SIZE];
    size_t n = microphone.read( samples );
    if ( n != MIC_BLOCK_SIZE ) return;
    // samples[0 .. n-1] are ready for filtering
}
```

## Logging

Uses a `Logger` with the tag `mic`. It logs the start (`I2S started: …`), the stop, and driver
errors. See [logger.md](logger.md).
