/******************************************************************************
 *
 * Purpose:  Declaration of the CPCIDSK_BLUT class.
 *
 ******************************************************************************
 * Copyright (c) 2015
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/
#ifndef INCLUDE_SEGMENT_PCIDSK_BLUT_H
#define INCLUDE_SEGMENT_PCIDSK_BLUT_H

#include "pcidsk_config.h"
#include "pcidsk_blut.h"
#include "pcidsk_buffer.h"
#include "segment/cpcidsksegment.h"


namespace PCIDSK
{
    class PCIDSKFile;

    /************************************************************************/
    /*                            CPCIDSK_BLUT                               */
    /************************************************************************/

    class CPCIDSK_BLUT : virtual public CPCIDSKSegment,
                        public PCIDSK_BLUT
    {
    public:
        CPCIDSK_BLUT( PCIDSKFile *file, int segment, const char *segment_pointer);

        virtual     ~CPCIDSK_BLUT();

        virtual void ReadBLUT( std::vector<BLUTEntry>& vBLUT ) override;
        virtual void WriteBLUT( const std::vector<BLUTEntry>& vBLUT ) override;
    };
} // end namespace PCIDSK

#endif // INCLUDE_SEGMENT_PCIDSK_BLUT_H
