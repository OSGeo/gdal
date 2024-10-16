/******************************************************************************
 *
 * Purpose:  PCIDSK LUT segment interface class.
 *
 ******************************************************************************
 * Copyright (c) 2015
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/
#ifndef INCLUDE_PCIDSK_BLUT_H
#define INCLUDE_PCIDSK_BLUT_H

#include <string>
#include <vector>
#include <utility>

namespace PCIDSK
{
    typedef std::pair<double, double> BLUTEntry;
/************************************************************************/
/*                              PCIDSK_LUT                              */
/************************************************************************/

//! Interface to PCIDSK pseudo-color segment.

    class PCIDSK_DLL PCIDSK_BLUT
    {
    public:
        virtual ~PCIDSK_BLUT() {}

/**
\brief Read a LUT Segment (SEG_BLUT).

@param vBLUT  Breakpoint Pseudo-Color Table buffer into which the breakpoint
pseudo-color table is read.  It consists of a vector of BLUTEntry.

*/
        virtual void ReadBLUT(std::vector<BLUTEntry>& vBLUT) = 0;

/**
\brief Write a BLUT Segment.

@param vBLUT  Breakpoint Pseudo-Color Table buffer from which the breakpoint
pseudo-color table is written.  It consists of a vector of BLUTEntry.

*/
        virtual void WriteBLUT(const std::vector<BLUTEntry>& vBLUT) = 0;
    };
} // end namespace PCIDSK

#endif // INCLUDE_PCIDSK_BLUT_H
