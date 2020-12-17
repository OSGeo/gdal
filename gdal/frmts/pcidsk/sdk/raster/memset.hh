/******************************************************************************
 *
 * Purpose:  Optimized functions using SSE instructions.
 *
 ******************************************************************************
 * Copyright (c) 2009
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#ifndef RASTER_MEMSET_HH
#define RASTER_MEMSET_HH

#include "raster/rastertypes.hh"
#include <cstdint>

RASTER_NAMESPACE_BEGIN

/************************************************************************/
/*                              _memset_head()                          */
/************************************************************************/
template <typename TType>
void _memset_head(TType * pnBuffer, TType & nValue, size_t nBytes)
{
    // We need to bitwise shift the value if the memory isn't aligned.
    const uint32_t nByteOffset = nBytes % sizeof(TType);

    if (nByteOffset != 0)
    {
        const uint32_t nBitOffset = nByteOffset * 8;

        *pnBuffer = nValue;

        nValue = ((nValue >> nBitOffset) |
                  (nValue << (sizeof(TType) * 8 - nBitOffset)));

        pnBuffer = (TType *)
            ((char *) pnBuffer + nByteOffset);
    }

    const TType * pnEnd = pnBuffer + nBytes / sizeof(TType);

    while (pnBuffer < pnEnd)
        *pnBuffer++ = nValue;
}

/************************************************************************/
/*                              _memset_tail()                          */
/************************************************************************/
template <typename TType>
void _memset_tail(TType * pnBuffer, TType & nValue, size_t nBytes)
{
    const TType * pnEnd = pnBuffer + nBytes / sizeof(TType);

    while (pnBuffer < pnEnd)
        *pnBuffer++ = nValue;

    // We need to bitwise shift the value if the memory isn't aligned.
    const uint32_t nByteOffset = nBytes % sizeof(TType);

    if (nByteOffset != 0)
    {
        const uint32_t nBitOffset = nByteOffset * 8;

        pnBuffer = (TType *)
            ((char *) pnBuffer - sizeof(TType) + nByteOffset);

        *pnBuffer = ((nValue >> nBitOffset) |
                     (nValue << (sizeof(TType) * 8 - nBitOffset)));
    }
}

/************************************************************************/
/*                             _mm_memset128()                          */
/************************************************************************/
inline void _mm_memset128(__m128i * pnBuffer, __m128i nValue, size_t nCount)
{
    const __m128i * pnEnd = pnBuffer + nCount;

    while (pnBuffer < pnEnd)
    {
        _mm_store_si128(pnBuffer, nValue);

        ++pnBuffer;
    }
}

/************************************************************************/
/*                                memset8()                             */
/************************************************************************/
inline void memset8(uint8_t * pnBuffer, uint8_t nValue, size_t nCount)
{
    const size_t nMisaligned = (size_t) pnBuffer % 16;
    const size_t nHead = nMisaligned ? 16 - nMisaligned : 0;
    const int64_t nTemp = nCount - nHead;
    const int64_t nAlign = nTemp / 16;

    if (nAlign > 0)
    {
        if (nHead > 0)
        {
            const uint8_t * pnEnd = pnBuffer + nHead;

            while (pnBuffer < pnEnd)
                *pnBuffer++ = nValue;
        }

        _mm_memset128((__m128i *) pnBuffer, _mm_set1_epi8(nValue), nAlign);

        const size_t nTail = nTemp % 16;

        if (nTail > 0)
        {
            pnBuffer += nAlign * 16;

            const uint8_t * pnEnd = pnBuffer + nTail;

            while (pnBuffer < pnEnd)
                *pnBuffer++ = nValue;
        }
    }
    else
    {
        const uint8_t * pnEnd = pnBuffer + nCount;

        while (pnBuffer < pnEnd)
            *pnBuffer++ = nValue;
    }
}

/************************************************************************/
/*                                memset16()                            */
/************************************************************************/
inline void memset16(uint16_t * pnBuffer, uint16_t nValue, size_t nCount)
{
    const size_t nMisaligned = (size_t) pnBuffer % 16;
    const size_t nHead = nMisaligned ? 16 - nMisaligned : 0;
    const int64_t nTemp = nCount * sizeof(uint16_t) - nHead;
    const int64_t nAlign = nTemp / 16;

    if (nAlign > 0)
    {
        uint8_t * pbyIter = (uint8_t *) pnBuffer;

        if (nHead > 0)
        {
            _memset_head((uint16_t *) pbyIter, nValue, nHead);

            pbyIter += nHead;
        }

        _mm_memset128((__m128i *) pbyIter, _mm_set1_epi16(nValue), nAlign);

        const size_t nTail = nTemp % 16;

        if (nTail > 0)
        {
            pbyIter += nAlign * 16;

            _memset_tail((uint16_t *) pbyIter, nValue, nTail);
        }
    }
    else
    {
        const uint16_t * pnEnd = pnBuffer + nCount;

        while (pnBuffer < pnEnd)
            *pnBuffer++ = nValue;
    }
}

/************************************************************************/
/*                                memset32()                            */
/************************************************************************/
inline void memset32(uint32_t * pnBuffer, uint32_t nValue, size_t nCount)
{
    const size_t nMisaligned = (size_t) pnBuffer % 16;
    const size_t nHead = nMisaligned ? 16 - nMisaligned : 0;
    const int64_t nTemp = nCount * sizeof(uint32_t) - nHead;
    const int64_t nAlign = nTemp / 16;

    if (nAlign > 0)
    {
        uint8_t * pbyIter = (uint8_t *) pnBuffer;

        if (nHead > 0)
        {
            _memset_head((uint32_t *) pbyIter, nValue, nHead);

            pbyIter += nHead;
        }

        _mm_memset128((__m128i *) pbyIter, _mm_set1_epi32(nValue), nAlign);

        const size_t nTail = nTemp % 16;

        if (nTail > 0)
        {
            pbyIter += nAlign * 16;

            _memset_tail((uint32_t *) pbyIter, nValue, nTail);
        }
    }
    else
    {
        const uint32_t * pnEnd = pnBuffer + nCount;

        while (pnBuffer < pnEnd)
            *pnBuffer++ = nValue;
    }
}

/************************************************************************/
/*                                memset64()                            */
/************************************************************************/
inline void memset64(uint64_t * pnBuffer, uint64_t nValue, size_t nCount)
{
    const size_t nMisaligned = (size_t) pnBuffer % 16;
    const size_t nHead = nMisaligned ? 16 - nMisaligned : 0;
    const int64_t nTemp = nCount * sizeof(uint64_t) - nHead;
    const int64_t nAlign = nTemp / 16;

    if (nAlign > 0)
    {
        uint8_t * pbyIter = (uint8_t *) pnBuffer;

        if (nHead > 0)
        {
            _memset_head((uint64_t *) pbyIter, nValue, nHead);

            pbyIter += nHead;
        }

        _mm_memset128((__m128i *) pbyIter, _mm_set1_epi64x(nValue), nAlign);

        const size_t nTail = nTemp % 16;

        if (nTail > 0)
        {
            pbyIter += nAlign * 16;

            _memset_tail((uint64_t *) pbyIter, nValue, nTail);
        }
    }
    else
    {
        const uint64_t * pnEnd = pnBuffer + nCount;

        while (pnBuffer < pnEnd)
            *pnBuffer++ = nValue;
    }
}

RASTER_NAMESPACE_END

#endif
