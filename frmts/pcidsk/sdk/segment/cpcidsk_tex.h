/******************************************************************************
 *
 * Purpose:  Declaration of the CPCIDSK_TEX class.
 *
 ******************************************************************************
 * Copyright (c) 2010
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/
#ifndef INCLUDE_SEGMENT_PCIDSK_TEX_H
#define INCLUDE_SEGMENT_PCIDSK_TEX_H

#include "pcidsk_config.h"
#include "pcidsk_types.h"
#include "pcidsk_tex.h"
#include "pcidsk_buffer.h"
#include "segment/cpcidsksegment.h"

#include <string>

namespace PCIDSK
{
    class PCIDSKFile;

    /************************************************************************/
    /*                            CPCIDSK_TEX                               */
    /************************************************************************/

    class CPCIDSK_TEX : virtual public CPCIDSKSegment,
                        public PCIDSK_TEX
    {
    public:
        CPCIDSK_TEX( PCIDSKFile *file, int segment,const char *segment_pointer);

        virtual     ~CPCIDSK_TEX();

        // PCIDSK_TEX

        std::string ReadText() override;
        void WriteText( const std::string &text ) override;
    };
} // end namespace PCIDSK

#endif // INCLUDE_SEGMENT_PCIDSK_TEX_H
