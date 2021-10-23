/******************************************************************************
 *
 * Purpose:  Functions to convert an ascii string to an integer value.
 *
 ******************************************************************************
 * Copyright (c) 2011
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

#ifndef PCIDSK_SCANINT_H
#define PCIDSK_SCANINT_H

#include "pcidsk_config.h"
#include <cassert>
#include <cmath>

namespace PCIDSK
{

extern const int16 ganCharTo1[256];
extern const int16 ganCharTo10[256];
extern const int16 ganCharTo100[256];
extern const int16 ganCharTo1000[256];
extern const int32 ganCharTo10000[256];
extern const int32 ganCharTo100000[256];
extern const int32 ganCharTo1000000[256];
extern const int32 ganCharTo10000000[256];
extern const int32 ganCharTo100000000[256];
extern const int64 ganCharTo1000000000[256];
extern const int64 ganCharTo10000000000[256];
extern const int64 ganCharTo100000000000[256];

inline int16 ScanInt1(const uint8 * string)
{
    if (*string != '-')
    {
        int16 nValue =
            (ganCharTo1 [*(string)]);

        return nValue;
    }
    else
    {
        return 0;
    }
}

inline int16 ScanInt2(const uint8 * string)
{
    if (*string != '-')
    {
        int16 nValue =
            (ganCharTo10[*(string    )] +
             ganCharTo1 [*(string + 1)]);

        return nValue;
    }
    else
    {
        return ganCharTo1[*(string + 1)] * -1;
    }
}

inline int16 ScanInt3(const uint8 * string)
{
    int16 nValue =
        (ganCharTo100[*(string    )] +
         ganCharTo10 [*(string + 1)] +
         ganCharTo1  [*(string + 2)]);

    if (nValue < 0)
    {
        const uint8 * pbyIter = string;
        while (*pbyIter != '-')
            ++pbyIter;
        return (int16) -std::pow((double) 10,
                                 2 - (int) (pbyIter - string)) - nValue;
    }

    return nValue;
}

inline int16 ScanInt4(const uint8 * string)
{
    int16 nValue =
        (ganCharTo1000[*(string    )] +
         ganCharTo100 [*(string + 1)] +
         ganCharTo10  [*(string + 2)] +
         ganCharTo1   [*(string + 3)]);

    if (nValue < 0)
    {
        const uint8 * pbyIter = string;
        while (*pbyIter != '-')
            ++pbyIter;
        return (int16) -std::pow((double) 10,
                                 3 - (int) (pbyIter - string)) - nValue;
    }

    return nValue;
}

inline int32 ScanInt5(const uint8 * string)
{
    int32 nValue =
        (ganCharTo10000[*(string    )] +
         ganCharTo1000 [*(string + 1)] +
         ganCharTo100  [*(string + 2)] +
         ganCharTo10   [*(string + 3)] +
         ganCharTo1    [*(string + 4)]);

    if (nValue < 0)
    {
        const uint8 * pbyIter = string;
        while (*pbyIter != '-')
            ++pbyIter;
        return (int32) -std::pow((double) 10,
                                 4 - (int) (pbyIter - string)) - nValue;
    }

    return nValue;
}

inline int32 ScanInt6(const uint8 * string)
{
    int32 nValue =
        (ganCharTo100000[*(string    )] +
         ganCharTo10000 [*(string + 1)] +
         ganCharTo1000  [*(string + 2)] +
         ganCharTo100   [*(string + 3)] +
         ganCharTo10    [*(string + 4)] +
         ganCharTo1     [*(string + 5)]);

    if (nValue < 0)
    {
        const uint8 * pbyIter = string;
        while (*pbyIter != '-')
            ++pbyIter;
        return (int32) -std::pow((double) 10,
                                 5 - (int) (pbyIter - string)) - nValue;
    }

    return nValue;
}

inline int32 ScanInt7(const uint8 * string)
{
    int32 nValue =
        (ganCharTo1000000[*(string    )] +
         ganCharTo100000 [*(string + 1)] +
         ganCharTo10000  [*(string + 2)] +
         ganCharTo1000   [*(string + 3)] +
         ganCharTo100    [*(string + 4)] +
         ganCharTo10     [*(string + 5)] +
         ganCharTo1      [*(string + 6)]);

    if (nValue < 0)
    {
        const uint8 * pbyIter = string;
        while (*pbyIter != '-')
            ++pbyIter;
        return (int32) -std::pow((double) 10,
                                 6 - (int) (pbyIter - string)) - nValue;
    }

    return nValue;
}

inline int32 ScanInt8(const uint8 * string)
{
    int32 nValue =
        (ganCharTo10000000[*(string    )] +
         ganCharTo1000000 [*(string + 1)] +
         ganCharTo100000  [*(string + 2)] +
         ganCharTo10000   [*(string + 3)] +
         ganCharTo1000    [*(string + 4)] +
         ganCharTo100     [*(string + 5)] +
         ganCharTo10      [*(string + 6)] +
         ganCharTo1       [*(string + 7)]);

    if (nValue < 0)
    {
        const uint8 * pbyIter = string;
        while (*pbyIter != '-')
            ++pbyIter;
        return (int32) -std::pow((double) 10,
                                 7 - (int) (pbyIter - string)) - nValue;
    }

    return nValue;
}

inline int32 ScanInt9(const uint8 * string)
{
    int32 nValue =
        (ganCharTo100000000[*(string    )] +
         ganCharTo10000000 [*(string + 1)] +
         ganCharTo1000000  [*(string + 2)] +
         ganCharTo100000   [*(string + 3)] +
         ganCharTo10000    [*(string + 4)] +
         ganCharTo1000     [*(string + 5)] +
         ganCharTo100      [*(string + 6)] +
         ganCharTo10       [*(string + 7)] +
         ganCharTo1        [*(string + 8)]);

    if (nValue < 0)
    {
        const uint8 * pbyIter = string;
        while (*pbyIter != '-')
            ++pbyIter;
        return (int32) -std::pow((double) 10,
                                 8 - (int) (pbyIter - string)) - nValue;
    }

    return nValue;
}

inline int64 ScanInt10(const uint8 * string)
{
    int64 nValue =
        (ganCharTo1000000000[*(string    )] +
         ganCharTo100000000 [*(string + 1)] +
         ganCharTo10000000  [*(string + 2)] +
         ganCharTo1000000   [*(string + 3)] +
         ganCharTo100000    [*(string + 4)] +
         ganCharTo10000     [*(string + 5)] +
         ganCharTo1000      [*(string + 6)] +
         ganCharTo100       [*(string + 7)] +
         ganCharTo10        [*(string + 8)] +
         ganCharTo1         [*(string + 9)]);

    if (nValue < 0)
    {
        const uint8 * pbyIter = string;
        while (*pbyIter != '-')
            ++pbyIter;
        return (int64) -std::pow((double) 10,
                                 9 - (int) (pbyIter - string)) - nValue;
    }

    return nValue;
}

inline int64 ScanInt11(const uint8 * string)
{
    int64 nValue =
        (ganCharTo10000000000[*(string    )] +
         ganCharTo1000000000 [*(string + 1)] +
         ganCharTo100000000  [*(string + 2)] +
         ganCharTo10000000   [*(string + 3)] +
         ganCharTo1000000    [*(string + 4)] +
         ganCharTo100000     [*(string + 5)] +
         ganCharTo10000      [*(string + 6)] +
         ganCharTo1000       [*(string + 7)] +
         ganCharTo100        [*(string + 8)] +
         ganCharTo10         [*(string + 9)] +
         ganCharTo1          [*(string + 10)]);

    if (nValue < 0)
    {
        const uint8 * pbyIter = string;
        while (*pbyIter != '-')
            ++pbyIter;
        return (int64) -std::pow((double) 10,
                                 10 - (int) (pbyIter - string)) - nValue;
    }

    return nValue;
}

inline int64 ScanInt12(const uint8 * string)
{
    int64 nValue =
        (ganCharTo100000000000[*(string    )] +
         ganCharTo10000000000 [*(string + 1)] +
         ganCharTo1000000000  [*(string + 2)] +
         ganCharTo100000000   [*(string + 3)] +
         ganCharTo10000000    [*(string + 4)] +
         ganCharTo1000000     [*(string + 5)] +
         ganCharTo100000      [*(string + 6)] +
         ganCharTo10000       [*(string + 7)] +
         ganCharTo1000        [*(string + 8)] +
         ganCharTo100         [*(string + 9)] +
         ganCharTo10          [*(string + 10)] +
         ganCharTo1           [*(string + 11)]);

    if (nValue < 0)
    {
        const uint8 * pbyIter = string;
        while (*pbyIter != '-')
            ++pbyIter;
        return (int64) -std::pow((double) 10,
                                 11 - (int) (pbyIter - string)) - nValue;
    }

    return nValue;
}

} // namespace PCIDSK

#endif
