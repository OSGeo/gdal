/******************************************************************************
 *
 * Purpose:  Implementation of the CPCIDSK_LUT class.
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
#include "segment/cpcidsklut.h"
#include <cassert>
#include <cstring>

using namespace PCIDSK;

/************************************************************************/
/*                            CPCIDSK_LUT()                             */
/************************************************************************/

CPCIDSK_LUT::CPCIDSK_LUT( PCIDSKFile *fileIn, int segmentIn,
                          const char *segment_pointer )
        : CPCIDSKSegment( fileIn, segmentIn, segment_pointer )

{
}

/************************************************************************/
/*                           ~CPCIDSKGeoref()                           */
/************************************************************************/

CPCIDSK_LUT::~CPCIDSK_LUT()

{
}

/************************************************************************/
/*                              ReadLUT()                               */
/************************************************************************/

void CPCIDSK_LUT::ReadLUT(std::vector<unsigned char>& lut)

{
    PCIDSKBuffer seg_data;

    seg_data.SetSize(256*4);

    ReadFromFile( seg_data.buffer, 0, 256*4);

    lut.resize(256);
    for( int i = 0; i < 256; i++ )
    {
        lut[i] = (unsigned char) seg_data.GetInt(0+i*4, 4);
    }
}

/************************************************************************/
/*                              WriteLUT()                              */
/************************************************************************/

void CPCIDSK_LUT::WriteLUT(const std::vector<unsigned char>& lut)

{
    if(lut.size() != 256)
    {
        throw PCIDSKException("LUT must contain 256 entries (%d given)", static_cast<int>(lut.size()));
    }

    PCIDSKBuffer seg_data;

    seg_data.SetSize(256*4);

    ReadFromFile( seg_data.buffer, 0, 256*4 );

    int i;
    for( i = 0; i < 256; i++ )
    {
        seg_data.Put( (int) lut[i], i*4, 4);
    }

    WriteToFile( seg_data.buffer, 0, 256*4 );
}
