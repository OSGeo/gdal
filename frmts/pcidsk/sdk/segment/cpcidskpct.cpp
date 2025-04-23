/******************************************************************************
 *
 * Purpose:  Implementation of the CPCIDSK_PCT class.
 *
 ******************************************************************************
 * Copyright (c) 2009
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
 *
 * SPDX-License-Identifier: MIT
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
