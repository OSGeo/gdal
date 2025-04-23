/******************************************************************************
 *
 * Purpose:  Declaration of hookable interfaces for the library
 *
 ******************************************************************************
 * Copyright (c) 2009
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef INCLUDE_PCIDSK_INTERFACES_H
#define INCLUDE_PCIDSK_INTERFACES_H

#include "pcidsk_io.h"
#include "pcidsk_mutex.h"
#include "pcidsk_edb.h"

namespace PCIDSK
{
    /************************************************************************/
    /*                           PCIDSKInterfaces                           */
    /************************************************************************/

    //! Collection of PCIDSK hookable interfaces.

    class PCIDSK_DLL PCIDSKInterfaces
    {
      public:
        PCIDSKInterfaces();

        const IOInterfaces *io;

        std::string       (*MergeRelativePath)(const PCIDSK::IOInterfaces *,
                                               const std::string& base,
                                               const std::string& filename);

        EDBFile           *(*OpenEDB)(const std::string& filename, const std::string& access);

        Mutex             *(*CreateMutex)(void);

        void              (*JPEGDecompressBlock)
            ( uint8 *src_data, int src_bytes, uint8 *dst_data, int dst_bytes,
              int xsize, int ysize, eChanType pixel_type );
        void              (*JPEGCompressBlock)
            ( uint8 *src_data, int src_bytes, uint8 *dst_data, int &dst_bytes,
              int xsize, int ysize, eChanType pixel_type, int quality );

        void              (*Debug)( const char * );
    };
} // end namespace PCIDSK

#endif // INCLUDE_PCIDSK_INTERFACES_H
