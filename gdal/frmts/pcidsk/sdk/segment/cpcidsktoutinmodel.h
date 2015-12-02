/******************************************************************************
 *
 * Purpose: Support for reading and manipulating PCIDSK Toutin Segments
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
#ifndef INCLUDE_PCIDSK_SEGMENT_PCIDSKTOUTINMODEL_H
#define INCLUDE_PCIDSK_SEGMENT_PCIDSKTOUTINMODEL_H
 
#include "pcidsk_toutin.h"
#include "segment/cpcidsksegment.h"
#include "segment/cpcidskephemerissegment.h"

namespace PCIDSK {
    class PCIDSKFile;

    class CPCIDSKToutinModelSegment : public PCIDSKToutinSegment,
                                      public CPCIDSKEphemerisSegment
    {
    public:
        CPCIDSKToutinModelSegment(PCIDSKFile *file, int segment,const char *segment_pointer);
        ~CPCIDSKToutinModelSegment();

        SRITInfo_t GetInfo() const;
        void SetInfo(const SRITInfo_t& poInfo);

        //synchronize the segment on disk.
        void Synchronize();
    private:
        
        // Helper housekeeping functions
        void Load();
        void Write();

    //functions to read/write binary information
    private:
        //Toutin information.
        SRITInfo_t* mpoInfo;

        SRITInfo_t *BinaryToSRITInfo();
        void SRITInfoToBinary( SRITInfo_t *SRITModel);

        int GetSensor( EphemerisSeg_t *OrbitPtr);
        int GetModel( int nSensor );
    };
}

#endif // INCLUDE_PCIDSK_SEGMENT_PCIDSKTOUTINMODEL_H
