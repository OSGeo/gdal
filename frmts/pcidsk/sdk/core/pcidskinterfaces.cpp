/******************************************************************************
 *
 * Purpose:  Implementation of the PCIDSKInterfaces class.
 *
 ******************************************************************************
 * Copyright (c) 2009
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "pcidsk_config.h"
#include "pcidsk_types.h"
#include "pcidsk_utils.h"
#include "pcidsk_interfaces.h"
#include "pcidsk_mutex.h"
#include "core/pcidsk_utils.h"

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
    OpenEDB = DefaultOpenEDB;
    MergeRelativePath = DefaultMergeRelativePath;
    CreateMutex = DefaultCreateMutex;
    Debug = DefaultDebug;

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

