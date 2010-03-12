/******************************************************************************
 *
 * Purpose:  PCIDSK PCT segment interface class.
 * 
 ******************************************************************************
 * Copyright (c) 2009
 * PCI Geomatics, 50 West Wilmot Street, Richmond Hill, Ont, Canada
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
#ifndef __INCLUDE_PCIDSK_PCT_H
#define __INCLUDE_PCIDSK_PCT_H

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
        virtual	~PCIDSK_PCT() {}

/**
\brief Read a PCT Segment (SEG_PCT).

@param pct	 Pseudo-Color Table buffer (768 entries) into which the
pseudo-color table is read.  It consists of the red gun output
values (pct[0-255]), followed by the green gun output values (pct[256-511]) 
and ends with the blue gun output values (pct[512-767]).

*/
        virtual void ReadPCT( unsigned char pct[768] ) = 0;

/**
\brief Write a PCT Segment.

@param pct	 Pseudo-Color Table buffer (768 entries) from which the
pseudo-color table is written.  It consists of the red gun output
values (pct[0-255]), followed by the green gun output values (pct[256-511]) 
and ends with the blue gun output values (pct[512-767]).

*/
        virtual void WritePCT( unsigned char pct[768] ) = 0;
    };
} // end namespace PCIDSK

#endif // __INCLUDE_PCIDSK_PCT_H
