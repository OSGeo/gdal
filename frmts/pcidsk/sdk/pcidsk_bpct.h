/******************************************************************************
 *
 * Purpose:  PCIDSK PCT segment interface class.
 *
 ******************************************************************************
 * Copyright (c) 2015
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
 *
 * SPDX-License-Identifier: MIT
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
