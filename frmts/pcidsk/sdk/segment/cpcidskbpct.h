/******************************************************************************
 *
 * Purpose:  Declaration of the CPCIDSK_BPCT class.
 *
 ******************************************************************************
 * Copyright (c) 2015
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/
#ifndef INCLUDE_SEGMENT_PCIDSK_BPCT_H
#define INCLUDE_SEGMENT_PCIDSK_BPCT_H

#include "pcidsk_config.h"
#include "pcidsk_bpct.h"
#include "pcidsk_buffer.h"
#include "segment/cpcidsksegment.h"


namespace PCIDSK
{
    class PCIDSKFile;

    /************************************************************************/
    /*                            CPCIDSK_BPCT                               */
    /************************************************************************/

    class CPCIDSK_BPCT : virtual public CPCIDSKSegment,
                        public PCIDSK_BPCT
    {
    public:
        CPCIDSK_BPCT( PCIDSKFile *file, int segment, const char *segment_pointer);

        virtual     ~CPCIDSK_BPCT();

        virtual void ReadBPCT( std::vector<BPCTEntry>& vBPCT ) override;
        virtual void WriteBPCT( const std::vector<BPCTEntry>& vBPCT ) override;
    };
} // end namespace PCIDSK

#endif // INCLUDE_SEGMENT_PCIDSK_BPCT_H
