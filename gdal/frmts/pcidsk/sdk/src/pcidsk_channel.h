/******************************************************************************
 *
 * Purpose:  Declaration of the PCIDSKChannel interface.
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
#ifndef __INCLUDE_PCIDSK_CHANNEL_H
#define __INCLUDE_PCIDSK_CHANNEL_H

#include "pcidsk_types.h"
#include <string>
#include <vector>

namespace PCIDSK
{
/************************************************************************/
/*                            PCIDSKChannel                             */
/************************************************************************/

//! Interface to one PCIDSK channel (band).

    class PCIDSK_DLL PCIDSKChannel 
    {
    public:
        virtual ~PCIDSKChannel() {};
        virtual int GetBlockWidth() = 0;
        virtual int GetBlockHeight() = 0;
        virtual int GetBlockCount() = 0;
        virtual int GetWidth() = 0;
        virtual int GetHeight() = 0;
        virtual eChanType GetType() = 0;
        virtual int ReadBlock( int block_index, void *buffer,
            int win_xoff=-1, int win_yoff=-1,
            int win_xsize=-1, int win_ysize=-1 ) = 0;
        virtual int WriteBlock( int block_index, void *buffer ) = 0;
        virtual int GetOverviewCount() = 0;
        virtual PCIDSKChannel *GetOverview( int i ) = 0;

        virtual std::string GetMetadataValue( std::string key ) = 0;
        virtual void SetMetadataValue( std::string key, std::string value ) = 0;
        virtual std::vector<std::string> GetMetadataKeys() = 0;

        virtual void Synchronize() = 0;
    };
} // end namespace PCIDSK

#endif // __INCLUDE_PCIDSK_CHANNEL_H
