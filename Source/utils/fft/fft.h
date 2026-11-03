#ifndef FFT_H
#define FFT_H

#include <cstddef>

// In-place radix-2 complex FFT
// `size` must be a power of two, `real` and `imag` must each hold `size` values
// For a real signal fill `imag` with zeros; bins 0 .. size / 2 then hold the spectrum
void fft( float *real, float *imag, size_t size );

#endif // FFT_H
