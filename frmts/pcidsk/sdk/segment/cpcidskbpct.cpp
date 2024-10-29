/******************************************************************************
 *
 * Purpose:  Implementation of the CPCIDSK_BPCT class.
 *
 ******************************************************************************
 * Copyright (c) 2015
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
*
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "pcidsk_exception.h"
#include "segment/cpcidskbpct.h"
#include <cassert>
#include <cstring>
#include <sstream>
#include <cmath>
#include <stdlib.h>
#include "core/pcidsk_utils.h"

using namespace PCIDSK;

/************************************************************************/
/*                            CPCIDSK_BPCT()                            */
/************************************************************************/
CPCIDSK_BPCT::CPCIDSK_BPCT( PCIDSKFile *fileIn, int segmentIn,
                          const char *segment_pointer )
        : CPCIDSKSegment( fileIn, segmentIn, segment_pointer )

{
}

/************************************************************************/
/*                           ~CPCIDSK_BPCT()                            */
/************************************************************************/
CPCIDSK_BPCT::~CPCIDSK_BPCT()

{
}

/************************************************************************/
/*                              ReadBPCT()                              */
/************************************************************************/
void CPCIDSK_BPCT::ReadBPCT( std::vector<BPCTEntry>& vBPCT )

{
    PCIDSKBuffer seg_data;

    seg_data.SetSize( (int) GetContentSize() );

    ReadFromFile( seg_data.buffer, 0, seg_data.buffer_size );

    std::istringstream ss(seg_data.buffer);

    vBPCT.clear();

    // the first token is interpolation type (not used)
    std::size_t nInterp;
    if(!(ss >> nInterp))
        throw PCIDSKException("Invalid BPCT segment.");

    // the second token is the number of entries
    std::size_t nCount;
    if(!(ss >> nCount)|| nCount > 1024 * 1024 /* arbitrary limit */)
        throw PCIDSKException("Invalid BPCT segment.");


    for(std::size_t n=0; n<nCount; ++n)
    {
        BPCTEntry oEntry;

        if(!(ss >> oEntry.boundary))
            throw PCIDSKException("Invalid BPCT segment.");

        int nTemp;
        if(!(ss >> nTemp) || nTemp < 0 || nTemp > 255)
            throw PCIDSKException("Invalid BPCT segment.");
        oEntry.red = (unsigned char) nTemp;

        if(!(ss >> nTemp) || nTemp < 0 || nTemp > 255)
            throw PCIDSKException("Invalid BPCT segment.");
        oEntry.green = (unsigned char) nTemp;

        if(!(ss >> nTemp) || nTemp < 0 || nTemp > 255)
            throw PCIDSKException("Invalid BPCT segment.");
        oEntry.blue = (unsigned char) nTemp;

        vBPCT.push_back(oEntry);

    }
}

/************************************************************************/
/*                              WriteBPCT()                             */
/************************************************************************/
void CPCIDSK_BPCT::WriteBPCT( const std::vector<BPCTEntry>& vBPCT )

{
    std::stringstream oSS;

    oSS << INTERP_LINEAR << " " << vBPCT.size();
    oSS.precision(15);

    for(std::vector<BPCTEntry>::const_iterator it = vBPCT.begin();
        it != vBPCT.end();
        ++it)
    {
        if(it->boundary == std::floor(it->boundary))
            oSS << " " << (int) it->boundary;
        else
            oSS << " " << it->boundary;
        oSS << " " << (unsigned int) it->red;
        oSS << " " << (unsigned int) it->green;
        oSS << " " << (unsigned int) it->blue;
    }

    std::string sData = oSS.str();
    WriteToFile( sData.c_str(), 0, sData.size() );
}
