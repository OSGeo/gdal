/******************************************************************************
 *
 * Purpose:  Declaration of hookable interfaces for the library
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

#ifndef __INCLUDE_PCIDSK_INTERFACES_H
#define __INCLUDE_PCIDSK_INTERFACES_H

#include "pcidsk_io.h"
#include "pcidsk_mutex.h"

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

        const IOInterfaces 	*io;

        Mutex               *(*CreateMutex)(void);

        void                (*JPEGDecompressBlock)
            ( uint8 *src_data, int src_bytes, uint8 *dst_data, int dst_bytes,
              int xsize, int ysize, eChanType pixel_type );
        void                (*JPEGCompressBlock)
            ( uint8 *src_data, int src_bytes, uint8 *dst_data, int &dst_bytes,
              int xsize, int ysize, eChanType pixel_type, int quality );

    //    DBInterfaces 	db_interfaces;
    };
} // end namespace PCIDSK

#endif // __INCLUDE_PCIDSK_INTERFACES_H
