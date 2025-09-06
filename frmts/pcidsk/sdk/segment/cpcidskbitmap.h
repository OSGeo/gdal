/******************************************************************************
 *
 * Purpose:  Declaration of the CPCIDSKBitmap class.
 *
 ******************************************************************************
 * Copyright (c) 2010
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
 *
 * SPDX-License-Identifier: MIT
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

    class CPCIDSKBitmap final: virtual public CPCIDSKSegment,
                          public PCIDSKChannel
    {
    public:
        CPCIDSKBitmap(PCIDSKFile *file,int segment,const char*segment_pointer);
        ~CPCIDSKBitmap() override;

        void Initialize() override;

        // Channel interface
        int GetBlockWidth() const override;
        int GetBlockHeight() const override;
        int GetBlockCount() const override;
        int GetWidth() const override;
        int GetHeight() const override;
        eChanType GetType() const override;
        virtual int ReadBlock( int block_index, void *buffer,
            int win_xoff=-1, int win_yoff=-1,
            int win_xsize=-1, int win_ysize=-1 ) override;
        int WriteBlock( int block_index, void *buffer ) override;
        int GetOverviewCount() override;
        PCIDSKChannel *GetOverview( int i ) override;
        bool IsOverviewValid( int i ) override;
        std::string GetOverviewResampling( int i ) override;
        void SetOverviewValidity( int i, bool validity ) override;
        std::vector<int> GetOverviewLevelMapping() const override;

        std::string GetMetadataValue( const std::string &key ) const override;
        void SetMetadataValue( const std::string &key, const std::string &value ) override;
        std::vector<std::string> GetMetadataKeys() const override;

        void Synchronize() override;

        std::string GetDescription() override;
        void SetDescription( const std::string &description ) override;

        std::vector<std::string> GetHistoryEntries() const override;
        void SetHistoryEntries( const std::vector<std::string> &entries ) override;
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
