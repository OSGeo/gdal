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
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
 *
 * SPDX-License-Identifier: MIT
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
            const std::string& filename,
            int channelnum,
            CPCIDSKFile *file,
            eChanType pixel_type );
        virtual ~CExternalChannel();

        virtual eChanType GetType() const override;
        virtual int GetBlockWidth() const override;
        virtual int GetBlockHeight() const override;
        virtual int ReadBlock( int block_index, void *buffer,
            int xoff=-1, int yoff=-1,
            int xsize=-1, int ysize=-1 ) override;
        virtual int WriteBlock( int block_index, void *buffer ) override;

        virtual void GetEChanInfo( std::string &filename, int &echannel,
                                   int &exoff, int &eyoff,
                                   int &exsize, int &eysize ) const override;
        virtual void SetEChanInfo( std::string filename, int echannel,
                                   int exoff, int eyoff,
                                   int exsize, int eysize ) override;

        std::string GetExternalFilename(){return filename;}
        int         GetExternalChanNum(){return echannel;}

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
