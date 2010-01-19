/******************************************************************************
 *
 * Purpose:  Implementation of the PCIDSKInterfaces class.
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

#include "pcidsk_config.h"
#include "pcidsk_types.h"
#include "pcidsk_utils.h"
#include "pcidsk_interfaces.h"
#include "pcidsk_mutex.h"

using namespace PCIDSK;

/************************************************************************/
/*                          PCIDSKInterfaces()                          */
/*                                                                      */
/*      This constructor just defaults all the interfaces and           */
/*      functions to the default implementation.                        */
/************************************************************************/

PCIDSKInterfaces::PCIDSKInterfaces()

{
    io = GetDefaultIOInterfaces();
    CreateMutex = DefaultCreateMutex;

#if defined(HAVE_LIBJPEG)
    JPEGDecompressBlock = LibJPEG_DecompressBlock;
    JPEGCompressBlock = LibJPEG_CompressBlock;
#else
    JPEGDecompressBlock = NULL;
    JPEGCompressBlock = NULL;
#endif
}

/**
 
\var const IOInterfaces *PCIDSKInterfaces::io;

\brief Pointer to IO Interfaces.

***************************************************************************/

/**

\var Mutex *(*PCIDSKInterfaces::CreateMutex)(void);

\brief Function to create a mutex

***************************************************************************/

/**

\var void (*PCIDSKInterfaces::JPEGDecompressBlock)(uint8 *src_data, int src_bytes, uint8 *dst_data, int dst_bytes, int xsize, int ysize, eChanType pixel_type);

\brief Function to decompress a jpeg block

This function may be NULL if there is no jpeg interface available. 

The default implementation is implemented using libjpeg.

The function decodes the jpeg compressed image in src_data (src_bytes long) 
into dst_data (dst_bytes long) as image data.  The result should be exactly
dst_bytes long, and will be an image of xsize x ysize of type pixel_type 
(currently on CHN_8U is allowed). 

Errors should be thrown as exceptions.

***************************************************************************/

/**

\var void (*PCIDSKInterfaces::JPEGCompressBlock)(uint8 *src_data, int src_bytes, uint8 *dst_data, int &dst_bytes, int xsize, int ysize, eChanType pixel_type);

\brief Function to compress a jpeg block

This function may be NULL if there is no jpeg interface available. 

The default implementation is implemented using libjpeg.

The function encodes the image in src_data (src_bytes long) 
into dst_data as compressed jpeg data.  The passed in value of dst_bytes is the
size of the passed in dst_data array (it should be large enough to hold
any compressed result0 and dst_bytes will be returned with the resulting 
actual number of bytes used.

Errors should be thrown as exceptions.

***************************************************************************/

