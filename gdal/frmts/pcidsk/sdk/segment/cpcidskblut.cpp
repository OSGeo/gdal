/******************************************************************************
 *
 * Purpose:  Implementation of the CPCIDSK_BLUT class.
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

#include "pcidsk_exception.h"
#include "segment/cpcidskblut.h"
#include <cassert>
#include <cstring>
#include <sstream>
#include <cmath>
#include <stdlib.h>

using namespace PCIDSK;

/************************************************************************/
/*                            CPCIDSK_BLUT()                            */
/************************************************************************/
CPCIDSK_BLUT::CPCIDSK_BLUT( PCIDSKFile *fileIn, int segmentIn,
                          const char *segment_pointer )
        : CPCIDSKSegment( fileIn, segmentIn, segment_pointer )

{
}

/************************************************************************/
/*                           ~CPCIDSK_BLUT()                            */
/************************************************************************/
CPCIDSK_BLUT::~CPCIDSK_BLUT()

{
}

/************************************************************************/
/*                              ReadBLUT()                              */
/************************************************************************/
void CPCIDSK_BLUT::ReadBLUT( std::vector<BLUTEntry>& vBLUT )

{
    PCIDSKBuffer seg_data;

    seg_data.SetSize( (int) GetContentSize() );

    ReadFromFile( seg_data.buffer, 0, seg_data.buffer_size );

    std::istringstream ss(seg_data.buffer);

    vBLUT.clear();

    // the first token is interpolation type (not used)
    std::size_t nInterp;
    if(!(ss >> nInterp))
        throw PCIDSKException("Invalid BLUT segment.");

    // the second token is the number of entries
    std::size_t nCount;
    if(!(ss >> nCount))
        throw PCIDSKException("Invalid BLUT segment.");

    for(std::size_t n=0; n<nCount; ++n)
    {
        BLUTEntry oEntry;

        if(!(ss >> oEntry.first))
            throw PCIDSKException("Invalid BLUT segment.");

        if(!(ss >> oEntry.second))
            throw PCIDSKException("Invalid BLUT segment.");

        vBLUT.push_back(oEntry);

    }
}

/************************************************************************/
/*                              WriteBLUT()                             */
/************************************************************************/
void CPCIDSK_BLUT::WriteBLUT( const std::vector<BLUTEntry>& vBLUT )

{
    std::stringstream oSS;

    oSS << INTERP_LINEAR << " " << vBLUT.size();
    oSS.precision(15);

    for(std::vector<BLUTEntry>::const_iterator it = vBLUT.begin();
        it != vBLUT.end();
        ++it)
    {
        if(it->first == std::floor(it->first))
            oSS << " " << (int) it->first;
        else
            oSS << " " << it->first;

        if(it->second == std::floor(it->second))
            oSS << " " << (int) it->second;
        else
            oSS << " " << it->second;
    }

    std::string sData = oSS.str();
    WriteToFile( sData.c_str(), 0, sData.size() );
}
