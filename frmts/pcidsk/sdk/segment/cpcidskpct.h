/******************************************************************************
 *
 * Purpose:  Declaration of the CPCIDSK_PCT class.
 *
 ******************************************************************************
 * Copyright (c) 2009
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/
#ifndef INCLUDE_SEGMENT_PCIDSK_PCT_H
#define INCLUDE_SEGMENT_PCIDSK_PCT_H

#include "pcidsk_config.h"
#include "pcidsk_types.h"
#include "pcidsk_pct.h"
#include "pcidsk_buffer.h"
#include "segment/cpcidsksegment.h"

#include <string>

namespace PCIDSK
{
    class PCIDSKFile;

    /************************************************************************/
    /*                            CPCIDSK_PCT                               */
    /************************************************************************/

    class CPCIDSK_PCT : virtual public CPCIDSKSegment,
                        public PCIDSK_PCT
    {
    public:
        CPCIDSK_PCT( PCIDSKFile *file, int segment,const char *segment_pointer);

        virtual     ~CPCIDSK_PCT();

        virtual void ReadPCT( unsigned char pct[768] ) override;
        virtual void WritePCT( unsigned char pct[768] ) override;
    };
} // end namespace PCIDSK

#endif // INCLUDE_SEGMENT_PCIDSKGEOREF_H
