/*
* Copyright (c) 2018, Even Rouault
* Author: <even.rouault at spatialys.com>
*
* Permission to use, copy, modify, distribute, and sell this software and
* its documentation for any purpose is hereby granted without fee, provided
* that (i) the above copyright notices and this permission notice appear in
* all copies of the software and related documentation, and (ii) the names of
* Sam Leffler and Silicon Graphics may not be used in any advertising or
* publicity relating to the software without the specific, prior written
* permission of Sam Leffler and Silicon Graphics.
*
* THE SOFTWARE IS PROVIDED "AS-IS" AND WITHOUT WARRANTY OF ANY KIND,
* EXPRESS, IMPLIED OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY
* WARRANTY OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
*
* IN NO EVENT SHALL SAM LEFFLER OR SILICON GRAPHICS BE LIABLE FOR
* ANY SPECIAL, INCIDENTAL, INDIRECT OR CONSEQUENTIAL DAMAGES OF ANY KIND,
* OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
* WHETHER OR NOT ADVISED OF THE POSSIBILITY OF DAMAGE, AND ON ANY THEORY OF
* LIABILITY, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
* OF THIS SOFTWARE.
*/

#ifndef TIFF_LERC_H_DEFINED
#define TIFF_LERC_H_DEFINED

#include "tiffiop.h"

#ifndef TIFFTAG_LERC_VERSION
#define TIFFTAG_LERC_PARAMETERS         50674   /* Stores LERC version and additional compression method */
#endif

#ifndef TIFFTAG_LERC_VERSION
/* Pseudo tags */
#define TIFFTAG_LERC_VERSION            65565 /* LERC version */
#define     LERC_VERSION_2_4            4
#define TIFFTAG_LERC_ADD_COMPRESSION    65566 /* LERC additional compression */
#define     LERC_ADD_COMPRESSION_NONE    0
#define     LERC_ADD_COMPRESSION_DEFLATE 1
#define     LERC_ADD_COMPRESSION_ZSTD    2
#define TIFFTAG_LERC_MAXZERROR      65567    /* LERC maximum error */
#endif

#if defined(__cplusplus)
extern "C" {
#endif
int TIFFInitLERC(TIFF* tif, int scheme);

#if defined(__cplusplus)
}
#endif

#endif /* TIFF_LERC_H_DEFINED */