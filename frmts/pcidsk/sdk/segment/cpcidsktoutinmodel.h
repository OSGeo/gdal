/******************************************************************************
 *
 * Purpose: Support for reading and manipulating PCIDSK Toutin Segments
 *
 ******************************************************************************
 * Copyright (c) 2009
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
 *
 * SPDX-License-Identifier: MIT
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

        SRITInfo_t GetInfo() const override;
        void SetInfo(const SRITInfo_t& poInfo) override;

        //synchronize the segment on disk.
        void Synchronize() override;
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
