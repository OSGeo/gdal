/******************************************************************************
 *
 * Purpose:  Declaration of the CPCIDSKChannel Abstract class.
 *
 ******************************************************************************
 * Copyright (c) 2009
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
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
#ifndef INCLUDE_CHANNEL_CPCIDSKCHANNEL_H
#define INCLUDE_CHANNEL_CPCIDSKCHANNEL_H

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
    class PCIDSK_DLL CPCIDSKChannel : public PCIDSKChannel
    {
        friend class PCIDSKFile;

    public:
        CPCIDSKChannel( PCIDSKBuffer &image_header, uint64 ih_offset,
            CPCIDSKFile *file, eChanType pixel_type,
            int channel_number );
        virtual   ~CPCIDSKChannel();

        virtual int GetBlockWidth() const override { return block_width; }
        virtual int GetBlockHeight() const override { return block_height; }
        virtual int GetBlockCount() const override;

        virtual int GetWidth() const override { return width; }
        virtual int GetHeight() const override { return height; }
        virtual eChanType GetType() const override { return pixel_type; }

        int       GetOverviewCount() override;
        PCIDSKChannel  *GetOverview( int i ) override;
        bool IsOverviewValid( int i ) override;
        void SetOverviewValidity( int i, bool validity ) override;
        std::string GetOverviewResampling( int i ) override;
        std::vector<int> GetOverviewLevelMapping() const override;

        int         GetChannelNumber() { return channel_number; }

        bool        IsLocked() const { return is_locked; }

        std::string GetFilename() const;

        std::string GetMetadataValue( const std::string &key ) const override
            { return metadata.GetMetadataValue(key); }
        void        SetMetadataValue( const std::string &key, const std::string &value ) override
            { metadata.SetMetadataValue(key,value); }
        std::vector<std::string> GetMetadataKeys() const override
            { return metadata.GetMetadataKeys(); }

        virtual void Synchronize() override {}

        std::string GetDescription() override;
        void SetDescription( const std::string &description ) override;

        virtual std::vector<std::string> GetHistoryEntries() const override;
        virtual void SetHistoryEntries( const std::vector<std::string> &entries ) override;
        virtual void PushHistory(const std::string &app,
                                 const std::string &message) override;

        virtual void GetChanInfo( std::string &filename, uint64 &image_offset,
                                  uint64 &pixel_offset, uint64 &line_offset,
                                  bool &little_endian ) const override;
        virtual void SetChanInfo( std::string filename, uint64 image_offset,
                                  uint64 pixel_offset, uint64 line_offset,
                                  bool little_endian ) override;
        virtual void GetEChanInfo( std::string &filename, int &echannel,
                                   int &exoff, int &eyoff,
                                   int &exsize, int &eysize ) const override;
        virtual void SetEChanInfo( std::string filename, int echannel,
                                   int exoff, int eyoff,
                                   int exsize, int eysize ) override;

    // Just for CPCIDSKFile.
        void      InvalidateOverviewInfo();
        void      UpdateOverviewInfo(const char *pszOverviewMDKey,
                                     int nFactor);

    protected:
        CPCIDSKFile *file;
        mutable MetadataSet  metadata;

        void LoadHistory( const PCIDSKBuffer &image_header );
        std::vector<std::string> history_;

        int       channel_number;
        uint64    ih_offset;
        mutable eChanType pixel_type;
        bool      is_locked;
        char      byte_order; // 'S': littleendian, 'N': bigendian
        mutable int       needs_swap;

    // width/height, and block size.
        mutable int       width;
        mutable int       height;
        mutable int       block_width;
        mutable int       block_height;

    // info about overviews;
        void      EstablishOverviewInfo() const;

        mutable bool                         overviews_initialized;
        mutable std::vector<std::string>     overview_infos;
        mutable std::vector<CTiledChannel *> overview_bands;
        mutable std::vector<int>             overview_decimations;

        void      InvalidateOverviews();
    };
} // end namespace PCIDSK

#endif // INCLUDE_CHANNEL_CPCIDSKCHANNEL_H
