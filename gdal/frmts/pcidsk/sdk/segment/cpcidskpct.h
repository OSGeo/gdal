/******************************************************************************
 *
 * Purpose:  Declaration of the CPCIDSK_PCT class.
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
#ifndef __INCLUDE_SEGMENT_PCIDSK_PCT_H
#define __INCLUDE_SEGMENT_PCIDSK_PCT_H

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

        virtual void ReadPCT( unsigned char pct[768] );
        virtual void WritePCT( unsigned char pct[768] );
    };
} // end namespace PCIDSK

#endif // __INCLUDE_SEGMENT_PCIDSKGEOREF_H
