/******************************************************************************
 *
 * Purpose: Interface representing access to a PCIDSK Ephemeris segment
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
#ifndef INCLUDE_PCIDSK_PCIDSK_EPHEMERIS_H
#define INCLUDE_PCIDSK_PCIDSK_EPHEMERIS_H

#include <vector>
#include <string>
#include "segment/orbitstructures.h"

namespace PCIDSK {
//! Interface to PCIDSK RPC segment.
    class PCIDSKEphemerisSegment
    {
    public:

        // Virtual destructor
        virtual ~PCIDSKEphemerisSegment() {}

        virtual const EphemerisSeg_t& GetEphemeris() const=0;
        virtual void SetEphemeris(const EphemerisSeg_t& oEph) =0;
    };
}

#endif // INCLUDE_PCIDSK_PCIDSK_EPHEMERIS_H
