/******************************************************************************
 *
 * Purpose:  PCIDSK PCT segment interface class.
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
#ifndef INCLUDE_PCIDSK_BPCT_H
#define INCLUDE_PCIDSK_BPCT_H

#include <string>
#include <vector>
#include "pcidsk_types.h"

namespace PCIDSK
{
    struct BPCTEntry
    {
        BPCTEntry():
        boundary(0.0), red(0), green(0), blue(0)
        {
        }
        BPCTEntry(double bound,
                  unsigned char r,
                  unsigned char g,
                  unsigned char b):
        boundary(bound), red(r), green(g), blue(b)
        {
        }
        double boundary;
        unsigned char red;
        unsigned char green;
        unsigned char blue;
    };
/************************************************************************/
/*                              PCIDSK_PCT                              */
/************************************************************************/

//! Interface to PCIDSK pseudo-color segment.

    class PCIDSK_DLL PCIDSK_BPCT
    {
    public:
        virtual ~PCIDSK_BPCT() {}

/**
\brief Read a PCT Segment (SEG_BPCT).

@param vBPCT  Breakpoint Pseudo-Color Table buffer into which the breakpoint
pseudo-color table is read.  It consists of a vector of BPCTEntry.

*/
        virtual void ReadBPCT( std::vector<BPCTEntry>& vBPCT) = 0;

/**
\brief Write a BPCT Segment.

@param vBPCT  Breakpoint Pseudo-Color Table buffer from which the breakpoint
pseudo-color table is written.  It consists of a vector of BPCTEntry.

*/
        virtual void WriteBPCT( const std::vector<BPCTEntry>& vBPCT) = 0;
    };
} // end namespace PCIDSK

#endif // INCLUDE_PCIDSK_BPCT_H
