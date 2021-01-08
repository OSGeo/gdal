/******************************************************************************
 *
 * Purpose:  PCIDSK LUT segment interface class.
 *
 ******************************************************************************
 * Copyright (c) 2015
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
#ifndef INCLUDE_PCIDSK_LUT_H
#define INCLUDE_PCIDSK_LUT_H

#include <string>
#include <vector>

namespace PCIDSK
{
/************************************************************************/
/*                              PCIDSK_LUT                              */
/************************************************************************/

//! Interface to PCIDSK lookup table segment.

    class PCIDSK_DLL PCIDSK_LUT
    {
    public:
        virtual ~PCIDSK_LUT() {}

/**
\brief Read a LUT Segment (SEG_LUT).

@param lut      Lookup Table buffer (256 entries) into which the
lookup table is read. It consists of grey output values (lut[0-255].

*/
        virtual void ReadLUT(std::vector<unsigned char>& lut) = 0;

/**
\brief Write a LUT Segment.

@param lut      Lookup Table buffer (256 entries) from which the
lookup table is written. It consists of grey output values (lut[0-255].

*/
        virtual void WriteLUT(const std::vector<unsigned char>& lut) = 0;
    };
} // end namespace PCIDSK

#endif // INCLUDE_PCIDSK_LUT_H
