#include "fft.h"

#include <cmath>

namespace
{
constexpr float TWO_PI = 6.28318530717958647692f;
}

void fft( float *real, float *imag, size_t size )
{
    // Bit-reversal permutation
    for ( size_t i = 1, j = 0; i < size; i++ )
    {
        size_t bit = size >> 1;
        for ( ; j & bit; bit >>= 1 )
        {
            j ^= bit;
        }
        j ^= bit;
        if ( i < j )
        {
            float tmp = real[i];
            real[i] = real[j];
            real[j] = tmp;
            tmp = imag[i];
            imag[i] = imag[j];
            imag[j] = tmp;
        }
    }

    // Butterflies, the twiddle factor is rotated by one step per offset
    for ( size_t length = 2; length <= size; length <<= 1 )
    {
        const float angle = -TWO_PI / static_cast<float>( length );
        const float stepReal = cosf( angle );
        const float stepImag = sinf( angle );
        const size_t halfLength = length >> 1;

        for ( size_t start = 0; start < size; start += length )
        {
            float currentReal = 1.0f;
            float currentImag = 0.0f;
            for ( size_t offset = 0; offset < halfLength; offset++ )
            {
                const size_t even = start + offset;
                const size_t odd = even + halfLength;
                const float oddReal = currentReal * real[odd] - currentImag * imag[odd];
                const float oddImag = currentReal * imag[odd] + currentImag * real[odd];

                real[odd] = real[even] - oddReal;
                imag[odd] = imag[even] - oddImag;
                real[even] += oddReal;
                imag[even] += oddImag;

                const float nextReal = currentReal * stepReal - currentImag * stepImag;
                currentImag = currentReal * stepImag + currentImag * stepReal;
                currentReal = nextReal;
            }
        }
    }
}
