/******************************************************************************
 *
 * Purpose:  PCIDSK PCT segment interface class.
 *
 ******************************************************************************
 * Copyright (c) 2009
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/
#ifndef INCLUDE_PCIDSK_PCT_H
#define INCLUDE_PCIDSK_PCT_H

#include <string>
#include <vector>

namespace PCIDSK
{
/************************************************************************/
/*                              PCIDSK_PCT                              */
/************************************************************************/

//! Interface to PCIDSK pseudo-color segment.

    class PCIDSK_DLL PCIDSK_PCT
    {
    public:
        virtual ~PCIDSK_PCT() {}

/**
\brief Read a PCT Segment (SEG_PCT).

@param pct  Pseudo-Color Table buffer (768 entries) into which the
pseudo-color table is read.  It consists of the red gun output
values (pct[0-255]), followed by the green gun output values (pct[256-511])
and ends with the blue gun output values (pct[512-767]).

*/
        virtual void ReadPCT( unsigned char pct[768] ) = 0;

/**
\brief Write a PCT Segment.

@param pct  Pseudo-Color Table buffer (768 entries) from which the
pseudo-color table is written.  It consists of the red gun output
values (pct[0-255]), followed by the green gun output values (pct[256-511])
and ends with the blue gun output values (pct[512-767]).

*/
        virtual void WritePCT( unsigned char pct[768] ) = 0;
    };
} // end namespace PCIDSK

#endif // INCLUDE_PCIDSK_PCT_H
