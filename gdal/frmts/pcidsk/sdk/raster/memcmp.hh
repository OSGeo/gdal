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

#ifndef RASTER_MEMCMP_HH
#define RASTER_MEMCMP_HH

#include "raster/rastertypes.hh"
#include <cstdint>

RASTER_NAMESPACE_BEGIN

/************************************************************************/
/*                              _memcmp_head()                          */
/************************************************************************/
template <typename TType>
bool _memcmp_head(const TType * pnBuffer, TType & nValue, size_t nBytes)
{
    // We need to bitwise shift the value if the memory isn't aligned.
    const uint32_t nByteOffset = nBytes % sizeof(TType);

    if (nByteOffset != 0)
    {
        if (*pnBuffer != nValue)
            return false;

        const uint32_t nBitOffset = nByteOffset * 8;

        nValue = ((nValue >> nBitOffset) |
                  (nValue << (sizeof(TType) * 8 - nBitOffset)));

        pnBuffer = (const TType *)
            ((const char *) pnBuffer + nByteOffset);
    }

    const TType * pnEnd = pnBuffer + nBytes / sizeof(TType);

    while (pnBuffer < pnEnd)
    {
        if (*pnBuffer++ != nValue)
            return false;
    }

    return true;
}

/************************************************************************/
/*                              _memcmp_tail()                          */
/************************************************************************/
template <typename TType>
bool _memcmp_tail(const TType * pnBuffer, TType & nValue, size_t nBytes)
{
    const TType * pnEnd = pnBuffer + nBytes / sizeof(TType);

    while (pnBuffer < pnEnd)
    {
        if (*pnBuffer++ != nValue)
            return false;
    }

    // We need to bitwise shift the value if the memory isn't aligned.
    const uint32_t nByteOffset = nBytes % sizeof(TType);

    if (nByteOffset != 0)
    {
        const uint32_t nBitOffset = nByteOffset * 8;

        pnBuffer = (const TType *)
            ((const char *) pnBuffer - sizeof(TType) + nByteOffset);

        nValue = ((nValue >> nBitOffset) |
                  (nValue << (sizeof(TType) * 8 - nBitOffset)));

        if (*pnBuffer != nValue)
            return false;
    }

    return true;
}

/************************************************************************/
/*                             _mm_memcmp128()                          */
/************************************************************************/
inline bool _mm_memcmp128(const __m128i * pnBuffer,
                          __m128i nValue, size_t nCount)
{
    const __m128i * pnEnd = pnBuffer + nCount;

    while (pnBuffer < pnEnd)
    {
        const int nResult =
            _mm_movemask_epi8(_mm_cmpeq_epi32(_mm_load_si128(pnBuffer), nValue));

        if (nResult != (int) 0xFFFF)
            return false;

        ++pnBuffer;
    }

    return true;
}

/************************************************************************/
/*                                memcmp8()                             */
/************************************************************************/
inline bool memcmp8(const uint8_t * pnBuffer, uint8_t nValue, size_t nCount)
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
            {
                if (*pnBuffer++ != nValue)
                    return false;
            }
        }

        if (!_mm_memcmp128((const __m128i *) pnBuffer,
                           _mm_set1_epi8(nValue), nAlign))
        {
            return false;
        }

        const size_t nTail = nTemp % 16;

        if (nTail > 0)
        {
            pnBuffer += nAlign * 16;

            const uint8_t * pnEnd = pnBuffer + nTail;

            while (pnBuffer < pnEnd)
            {
                if (*pnBuffer++ != nValue)
                    return false;
            }
        }
    }
    else
    {
        const uint8_t * pnEnd = pnBuffer + nCount;

        while (pnBuffer < pnEnd)
        {
            if (*pnBuffer++ != nValue)
                return false;
        }
    }

    return true;
}

/************************************************************************/
/*                                memcmp16()                            */
/************************************************************************/
inline bool memcmp16(const uint16_t * pnBuffer, uint16_t nValue, size_t nCount)
{
    const size_t nMisaligned = (size_t) pnBuffer % 16;
    const size_t nHead = nMisaligned ? 16 - nMisaligned : 0;
    const int64_t nTemp = nCount * sizeof(uint16_t) - nHead;
    const int64_t nAlign = nTemp / 16;

    if (nAlign > 0)
    {
        const uint8_t * pbyIter = (uint8_t *) pnBuffer;

        if (nHead > 0)
        {
            if (!_memcmp_head((const uint16_t *) pbyIter, nValue, nHead))
                return false;

            pbyIter += nHead;
        }

        if (!_mm_memcmp128((const __m128i *) pbyIter,
                           _mm_set1_epi16(nValue), nAlign))
        {
            return false;
        }

        const size_t nTail = nTemp % 16;

        if (nTail > 0)
        {
            pbyIter += nAlign * 16;

            if (!_memcmp_tail((const uint16_t *) pbyIter, nValue, nTail))
                return false;
        }
    }
    else
    {
        const uint16_t * pnEnd = pnBuffer + nCount;

        while (pnBuffer < pnEnd)
        {
            if (*pnBuffer++ != nValue)
                return false;
        }
    }

    return true;
}

/************************************************************************/
/*                                memcmp32()                            */
/************************************************************************/
inline bool memcmp32(const uint32_t * pnBuffer, uint32_t nValue, size_t nCount)
{
    const size_t nMisaligned = (size_t) pnBuffer % 16;
    const size_t nHead = nMisaligned ? 16 - nMisaligned : 0;
    const int64_t nTemp = nCount * sizeof(uint32_t) - nHead;
    const int64_t nAlign = nTemp / 16;

    if (nAlign > 0)
    {
        const uint8_t * pbyIter = (uint8_t *) pnBuffer;

        if (nHead > 0)
        {
            if (!_memcmp_head((const uint32_t *) pbyIter, nValue, nHead))
                return false;

            pbyIter += nHead;
        }

        if (!_mm_memcmp128((const __m128i *) pbyIter,
                           _mm_set1_epi32(nValue), nAlign))
        {
            return false;
        }

        const size_t nTail = nTemp % 16;

        if (nTail > 0)
        {
            pbyIter += nAlign * 16;

            if (!_memcmp_tail((const uint32_t *) pbyIter, nValue, nTail))
                return false;
        }
    }
    else
    {
        const uint32_t * pnEnd = pnBuffer + nCount;

        while (pnBuffer < pnEnd)
        {
            if (*pnBuffer++ != nValue)
                return false;
        }
    }

    return true;
}

/************************************************************************/
/*                                memcmp64()                            */
/************************************************************************/
inline bool memcmp64(const uint64_t * pnBuffer, uint64_t nValue, size_t nCount)
{
    const size_t nMisaligned = (size_t) pnBuffer % 16;
    const size_t nHead = nMisaligned ? 16 - nMisaligned : 0;
    const int64_t nTemp = nCount * sizeof(uint64_t) - nHead;
    const int64_t nAlign = nTemp / 16;

    if (nAlign > 0)
    {
        const uint8_t * pbyIter = (uint8_t *) pnBuffer;

        if (nHead > 0)
        {
            if (!_memcmp_head((const uint64_t *) pbyIter, nValue, nHead))
                return false;

            pbyIter += nHead;
        }

        if (!_mm_memcmp128((const __m128i *) pbyIter,
                           _mm_set1_epi64x(nValue), nAlign))
        {
            return false;
        }

        const size_t nTail = nTemp % 16;

        if (nTail > 0)
        {
            pbyIter += nAlign * 16;

            if (!_memcmp_tail((const uint64_t *) pbyIter, nValue, nTail))
                return false;
        }
    }
    else
    {
        const uint64_t * pnEnd = pnBuffer + nCount;

        while (pnBuffer < pnEnd)
        {
            if (*pnBuffer++ != nValue)
                return false;
        }
    }

    return true;
}

RASTER_NAMESPACE_END

#endif
