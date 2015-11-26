/******************************************************************************
 *
 * Purpose:  Declaration of the CExternalChannel class.
 *
 * This class is used to implement band interleaved channels that are
 * references to an external image database that is not just a raw file.
 * It uses the application supplied EDB interface to access non-PCIDSK files.
 * 
 ******************************************************************************
 * Copyright (c) 2010
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

#ifndef INCLUDE_CHANNEL_CEXTERNALCHANNEL_H
#define INCLUDE_CHANNEL_CEXTERNALCHANNEL_H

#include "pcidsk_config.h"
#include "pcidsk_types.h"
#include "pcidsk_buffer.h"
#include "channel/cpcidskchannel.h"
#include <string>

namespace PCIDSK
{
    class CPCIDSKFile;

/************************************************************************/
/*                           CExternalChannel                           */
/************************************************************************/

    class CExternalChannel : public CPCIDSKChannel
    {
    public:
        CExternalChannel( PCIDSKBuffer &image_header, 
            uint64 ih_offset,
            PCIDSKBuffer &file_header,
            std::string filename,              
            int channelnum,
            CPCIDSKFile *file,
            eChanType pixel_type );
        virtual ~CExternalChannel();

        virtual int GetBlockWidth() const;
        virtual int GetBlockHeight() const;
        virtual int ReadBlock( int block_index, void *buffer,
            int xoff=-1, int yoff=-1,
            int xsize=-1, int ysize=-1 );
        virtual int WriteBlock( int block_index, void *buffer );

        virtual void GetEChanInfo( std::string &filename, int &echannel,
                                   int &exoff, int &eyoff, 
                                   int &exsize, int &eysize ) const;
        virtual void SetEChanInfo( std::string filename, int echannel,
                                   int exoff, int eyoff, 
                                   int exsize, int eysize );
    private:
        int      exoff;
        int      eyoff;
        int      exsize;
        int      eysize;
        
        int      echannel;

        mutable int blocks_per_row;

        mutable EDBFile  *db;
        mutable Mutex    *mutex;
        mutable bool     writable;

        void     AccessDB() const;

        mutable std::string filename;
    };
} // end namespace PCIDSK

#endif // INCLUDE_CHANNEL_CEXTERNALCHANNEL_H
