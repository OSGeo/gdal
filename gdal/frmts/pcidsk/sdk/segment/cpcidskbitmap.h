/******************************************************************************
 *
 * Purpose:  Declaration of the CPCIDSKBitmap class.
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

        virtual void Initialize();

        // Channel interface
        virtual int GetBlockWidth() const;
        virtual int GetBlockHeight() const;
        virtual int GetBlockCount() const;
        virtual int GetWidth() const;
        virtual int GetHeight() const;
        virtual eChanType GetType() const;
        virtual int ReadBlock( int block_index, void *buffer,
            int win_xoff=-1, int win_yoff=-1,
            int win_xsize=-1, int win_ysize=-1 );
        virtual int WriteBlock( int block_index, void *buffer );
        virtual int GetOverviewCount();
        virtual PCIDSKChannel *GetOverview( int i );
        virtual bool IsOverviewValid( int i );
        virtual std::string GetOverviewResampling( int i );
        virtual void SetOverviewValidity( int i, bool validity );
        virtual std::vector<int> GetOverviewLevelMapping() const;

        virtual std::string GetMetadataValue( const std::string &key ) const;
        virtual void SetMetadataValue( const std::string &key, const std::string &value );
        virtual std::vector<std::string> GetMetadataKeys() const;

        virtual void Synchronize();

        virtual std::string GetDescription();
        virtual void SetDescription( const std::string &description );

        virtual std::vector<std::string> GetHistoryEntries() const;
        virtual void SetHistoryEntries( const std::vector<std::string> &entries );
        virtual void PushHistory(const std::string &app,
                                 const std::string &message);

        virtual void GetChanInfo( std::string &filename, uint64 &image_offset, 
                                  uint64 &pixel_offset, uint64 &line_offset, 
                                  bool &little_endian ) const;
        virtual void SetChanInfo( std::string filename, uint64 image_offset, 
                                  uint64 pixel_offset, uint64 line_offset, 
                                  bool little_endian );
        virtual void GetEChanInfo( std::string &filename, int &echannel,
                                   int &exoff, int &eyoff, 
                                   int &exsize, int &eysize ) const;
        virtual void SetEChanInfo( std::string filename, int echannel,
                                   int exoff, int eyoff, 
                                   int exsize, int eysize );

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
