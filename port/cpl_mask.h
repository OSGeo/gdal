/**********************************************************************
 * $Id$
 *
 * Name:     cpl_mask.h
 * Project:  CPL - Common Portability Library
 * Purpose:  Bitmask manipulation functions
 * Author:   Daniel Baston, dbaston@gmail.com
 *
 **********************************************************************
 * Copyright (c) 2022, ISciences LLC
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#ifndef CPL_MASK_H_INCLUDED
#define CPL_MASK_H_INCLUDED

#include "cpl_port.h"
#include "cpl_vsi.h"

#ifdef __cplusplus

#include <cstring>

/**
 * Allocates a buffer to store a given number of bits
 *
 * @param size number of bits
 * @param default_value initial value of bits
 */
inline GUInt32 *CPLMaskCreate(std::size_t size, bool default_value)
{
    std::size_t nBytes = (size + 31) / 8;
    void *buf = VSI_MALLOC_VERBOSE(nBytes);
    if (buf == nullptr)
    {
        return nullptr;
    }
    std::memset(buf, default_value ? 0xff : 0, nBytes);
    return static_cast<GUInt32 *>(buf);
}

/**
 * Get the value of a bit
 *
 * @param mask bit mask
 * @param i index of bit
 * @return `true` if bit is set
 */
inline bool CPLMaskGet(GUInt32 *mask, std::size_t i)
{
    return mask[i >> 5] & (0x01 << (i & 0x1f));
}

/**
 * Clear the value of a bit (set to false)
 *
 * @param mask bit mask
 * @param i index of bit to clear
 */
inline void CPLMaskClear(GUInt32 *mask, std::size_t i)
{
    mask[i >> 5] &= ~(0x01 << (i & 0x1f));
}

/**
 * Clear all bits in a mask
 *
 * @param mask bit mask
 * @param size number of bits in mask
 */
inline void CPLMaskClearAll(GUInt32 *mask, std::size_t size)
{
    auto nBytes = (size + 31) / 8;
    std::memset(mask, 0, nBytes);
}

/**
 * Set the value of a bit to true
 *
 * @param mask bit mask
 * @param i index of bit to set
 */
inline void CPLMaskSet(GUInt32 *mask, std::size_t i)
{
    mask[i >> 5] |= (0x01 << (i & 0x1f));
}

/**
 * Set all bits in a mask
 *
 * @param mask bit mask
 * @param size number of bits in mask
 */
inline void CPLMaskSetAll(GUInt32 *mask, std::size_t size)
{
    auto nBytes = (size + 31) / 8;
    std::memset(mask, 0xff, nBytes);
}

/**
 * Set a mask to true wherever a second mask is true
 *
 * @param mask1 destination mask
 * @param mask2 source mask
 * @param n number of bits in masks (must be same)
 */
inline void CPLMaskMerge(GUInt32 *mask1, GUInt32 *mask2, std::size_t n)
{
    std::size_t nBytes = (n + 31) / 8;
    std::size_t nIter = nBytes / 4;
    for (std::size_t i = 0; i < nIter; i++)
    {
        mask1[i] |= mask2[i];
    }
}

#endif  // __cplusplus

#endif  // CPL_MASK_H
