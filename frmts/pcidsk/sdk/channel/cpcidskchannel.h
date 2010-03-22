/******************************************************************************
 *
 * Purpose:  Declaration of the CPCIDSKChannel Abstract class.
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
#ifndef __INCLUDE_CHANNEL_CPCIDSKCHANNEL_H
#define __INCLUDE_CHANNEL_CPCIDSKCHANNEL_H

#include "pcidsk_config.h"
#include "pcidsk_buffer.h"
#include "pcidsk_channel.h"
#include "core/metadataset.h"
#include "core/mutexholder.h"
#include <vector>
#include <string>

namespace PCIDSK
{
    class CPCIDSKFile;
    class CTiledChannel;
/************************************************************************/
/*                            CPCIDSKChannel                            */
/*                                                                      */
/* Abstract class that helps implement some of the more mundane details */
/* required for when implementing an imagery channel I/O strategy.  If  */
/* you are using this to implement those details, use virtual           */
/* inheritance to attempt to avoid the fragile base class problem and   */
/* then implement the Imagery I/O functions.                            */
/************************************************************************/
    class CPCIDSKChannel : public PCIDSKChannel
    {
        friend class PCIDSKFile;

    public:
        CPCIDSKChannel( PCIDSKBuffer &image_header, uint64 ih_offset,
            CPCIDSKFile *file, eChanType pixel_type,
            int channel_number );
        virtual   ~CPCIDSKChannel();

        virtual int GetBlockWidth() const { return block_width; }
        virtual int GetBlockHeight() const { return block_height; }
        virtual int GetBlockCount() const;

        virtual int GetWidth() const { return width; }
        virtual int GetHeight() const { return height; }
        virtual eChanType GetType() const { return pixel_type; }

        int       GetOverviewCount();
        PCIDSKChannel  *GetOverview( int i );
        bool IsOverviewValid( int i );
        void SetOverviewValidity( int i, bool validity );
        std::string GetOverviewResampling( int i );

        int         GetChannelNumber() { return channel_number; }

        std::string GetMetadataValue( const std::string &key ) 
            { return metadata.GetMetadataValue(key); }
        void        SetMetadataValue( const std::string &key, const std::string &value ) 
            { metadata.SetMetadataValue(key,value); }
        std::vector<std::string> GetMetadataKeys() 
            { return metadata.GetMetadataKeys(); }

        virtual void Synchronize() {}

        std::string GetDescription();
        void SetDescription( const std::string &description );

        virtual std::vector<std::string> GetHistoryEntries() const;
        virtual void SetHistoryEntries( const std::vector<std::string> &entries );
        virtual void PushHistory(const std::string &app,
                                 const std::string &message);

    // Just for CPCIDSKFile.
        void      InvalidateOverviewInfo();

    protected:
        CPCIDSKFile *file;
        MetadataSet  metadata;

        void LoadHistory( const PCIDSKBuffer &image_header );
        std::vector<std::string> history_;

        int       channel_number;
        uint64    ih_offset;
        mutable eChanType pixel_type;
        char      byte_order; // 'S': littleendian, 'N': bigendian
        mutable int       needs_swap;

    // width/height, and block size.
        mutable int       width;
        mutable int       height;
        mutable int       block_width;
        mutable int       block_height;

    // info about overviews;
        void      EstablishOverviewInfo();

        bool                         overviews_initialized;
        std::vector<std::string>     overview_infos;
        std::vector<CTiledChannel *> overview_bands;
        std::vector<int>             overview_decimations;
    };
} // end namespace PCIDSK

#endif // __INCLUDE_CHANNEL_CPCIDSKCHANNEL_H
