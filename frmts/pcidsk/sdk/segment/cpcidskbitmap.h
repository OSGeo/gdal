/******************************************************************************
 *
 * Purpose:  Declaration of the CPCIDSKBitmap class.
 *
 ******************************************************************************
 * Copyright (c) 2010
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

#ifndef INCLUDE_SEGMENT_PCIDSKBITMAP_H
#define INCLUDE_SEGMENT_PCIDSKBITMAP_H

#include "pcidsk_config.h"
#include "pcidsk_types.h"
#include "pcidsk_buffer.h"
#include "segment/cpcidsksegment.h"
#include "pcidsk_channel.h"

#include <string>

namespace PCIDSK
{
    class PCIDSKFile;

    /************************************************************************/
    /*                            CPCIDSKGeoref                             */
    /************************************************************************/

    class CPCIDSKBitmap : virtual public CPCIDSKSegment,
                          public PCIDSKChannel
    {
    public:
        CPCIDSKBitmap(PCIDSKFile *file,int segment,const char*segment_pointer);
        virtual     ~CPCIDSKBitmap();

        virtual void Initialize() override;

        // Channel interface
        virtual int GetBlockWidth() const override;
        virtual int GetBlockHeight() const override;
        virtual int GetBlockCount() const override;
        virtual int GetWidth() const override;
        virtual int GetHeight() const override;
        virtual eChanType GetType() const override;
        virtual int ReadBlock( int block_index, void *buffer,
            int win_xoff=-1, int win_yoff=-1,
            int win_xsize=-1, int win_ysize=-1 ) override;
        virtual int WriteBlock( int block_index, void *buffer ) override;
        virtual int GetOverviewCount() override;
        virtual PCIDSKChannel *GetOverview( int i ) override;
        virtual bool IsOverviewValid( int i ) override;
        virtual std::string GetOverviewResampling( int i ) override;
        virtual void SetOverviewValidity( int i, bool validity ) override;
        virtual std::vector<int> GetOverviewLevelMapping() const override;

        virtual std::string GetMetadataValue( const std::string &key ) const override;
        virtual void SetMetadataValue( const std::string &key, const std::string &value ) override;
        virtual std::vector<std::string> GetMetadataKeys() const override;

        virtual void Synchronize() override;

        virtual std::string GetDescription() override;
        virtual void SetDescription( const std::string &description ) override;

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

    private:
        bool      loaded;

        int       width;
        int       height;
        int       block_width;
        int       block_height;

        void      Load() const;
    };
} // end namespace PCIDSK

#endif // INCLUDE_SEGMENT_PCIDSKBITMAP_H
