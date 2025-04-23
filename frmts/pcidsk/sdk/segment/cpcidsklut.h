/******************************************************************************
 *
 * Purpose:  Declaration of the CPCIDSK_LUT class.
 *
 ******************************************************************************
 * Copyright (c) 2015
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/
#ifndef INCLUDE_SEGMENT_PCIDSK_LUT_H
#define INCLUDE_SEGMENT_PCIDSK_LUT_H

#include "pcidsk_config.h"
#include "pcidsk_types.h"
#include "pcidsk_lut.h"
#include "pcidsk_buffer.h"
#include "segment/cpcidsksegment.h"

#include <string>

namespace PCIDSK
{
    class PCIDSKFile;

    /************************************************************************/
    /*                            CPCIDSK_LUT                               */
    /************************************************************************/

    class CPCIDSK_LUT : virtual public CPCIDSKSegment,
                        public PCIDSK_LUT
    {
    public:
        CPCIDSK_LUT( PCIDSKFile *file, int segment,const char *segment_pointer);

        virtual     ~CPCIDSK_LUT();

        virtual void ReadLUT(std::vector<unsigned char>& lut) override;
        virtual void WriteLUT(const std::vector<unsigned char>& lut) override;
    };
} // end namespace PCIDSK

#endif // INCLUDE_SEGMENT_PCIDSKGEOREF_H
