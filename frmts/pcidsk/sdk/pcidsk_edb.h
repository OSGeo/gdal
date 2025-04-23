/******************************************************************************
 *
 * Purpose:  PCIDSK External Database Interface declaration.  This provides
 *           mechanisms for access to external linked image file formats.
 *
 ******************************************************************************
 * Copyright (c) 2010
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/
#ifndef INCLUDE_PCIDSK_EDB_H
#define INCLUDE_PCIDSK_EDB_H

#include "pcidsk_config.h"

#include <string>

namespace PCIDSK
{
/************************************************************************/
/*                               EDBFile                                */
/************************************************************************/

//! External Database Interface class.

    class EDBFile
    {
    public:
        virtual ~EDBFile() {}
        virtual int Close() const = 0;

        virtual int GetWidth() const = 0;
        virtual int GetHeight() const = 0;
        virtual int GetChannels() const = 0;
        virtual int GetBlockWidth(int channel ) const = 0;
        virtual int GetBlockHeight(int channel ) const = 0;
        virtual eChanType GetType(int channel ) const = 0;
        virtual int ReadBlock(int channel,
            int block_index, void *buffer,
            int win_xoff=-1, int win_yoff=-1,
            int win_xsize=-1, int win_ysize=-1 ) = 0;
        virtual int WriteBlock( int channel, int block_index, void *buffer) = 0;
    };

    EDBFile PCIDSK_DLL *DefaultOpenEDB(const std::string& filename,
                                       const std::string& access);
} // end namespace PCIDSK

#endif // INCLUDE_PCIDSK_EDB_H
