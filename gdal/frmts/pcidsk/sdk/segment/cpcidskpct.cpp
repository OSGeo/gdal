/******************************************************************************
 *
 * Purpose:  Implementation of the CPCIDSK_PCT class.
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

#include "pcidsk_exception.h"
#include "segment/cpcidskpct.h"
#include <cassert>
#include <cstring>

using namespace PCIDSK;

/************************************************************************/
/*                            CPCIDSK_PCT()                             */
/************************************************************************/

CPCIDSK_PCT::CPCIDSK_PCT( PCIDSKFile *fileIn, int segmentIn,
                          const char *segment_pointer )
        : CPCIDSKSegment( fileIn, segmentIn, segment_pointer )

{
}

/************************************************************************/
/*                           ~CPCIDSKGeoref()                           */
/************************************************************************/

CPCIDSK_PCT::~CPCIDSK_PCT()

{
}

/************************************************************************/
/*                              ReadPCT()                               */
/************************************************************************/

void CPCIDSK_PCT::ReadPCT( unsigned char pct[768] )

{
    PCIDSKBuffer seg_data;

    seg_data.SetSize( 768*4 );

    ReadFromFile( seg_data.buffer, 0, 768*4 );

    int i;
    for( i = 0; i < 256; i++ )
    {
        pct[  0+i] = (unsigned char) seg_data.GetInt(   0+i*4, 4 );
        pct[256+i] = (unsigned char) seg_data.GetInt(1024+i*4, 4 );
        pct[512+i] = (unsigned char) seg_data.GetInt(2048+i*4, 4 );
    }
}

/************************************************************************/
/*                              WritePCT()                              */
/************************************************************************/

void CPCIDSK_PCT::WritePCT( unsigned char pct[768] )

{
    PCIDSKBuffer seg_data;

    seg_data.SetSize( 768*4 );

    ReadFromFile( seg_data.buffer, 0, 768*4 );

    int i;
    for( i = 0; i < 256; i++ )
    {
        seg_data.Put( (int) pct[  0+i],   0+i*4, 4 );
        seg_data.Put( (int) pct[256+i],1024+i*4, 4 );
        seg_data.Put( (int) pct[512+i],2048+i*4, 4 );
    }

    WriteToFile( seg_data.buffer, 0, 768*4 );
}
