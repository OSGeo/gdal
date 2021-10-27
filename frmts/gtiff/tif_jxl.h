/*
* Copyright (c) 2021, Airbus DS Intelligence
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

#ifndef TIFF_JXL_H_DEFINED
#define TIFF_JXL_H_DEFINED

#ifndef COMPRESSION_JXL
#define     COMPRESSION_JXL             50002 /* JPEGXL: WARNING not registered in Adobe-maintained registry */
#endif

#ifndef TIFFTAG_JXL_LOSSYNESS

/* Pseudo tags */
#define TIFFTAG_JXL_LOSSYNESS           65565 /* Whether the compression should be lossless (default) or lossy */
#define     JXL_LOSSLESS                0
#define     JXL_LOSSY                   1
#define TIFFTAG_JXL_EFFORT              65536 /* Level of effort for compressor: 1:lightning 2: thunder 3:falcon 4:cheetah 5:hare 6:wombat 7:squirrel 8:kitten 9:tortoise. Default is 5 */
#define TIFFTAG_JXL_DISTANCE            65537 /* Sets the distance level for lossy compression: target max butteraugli distance, lower = higher quality. Range: 0 .. 15. */
#endif

#if defined(__cplusplus)
extern "C" {
#endif
int TIFFInitJXL(TIFF* tif, int scheme);

#if defined(__cplusplus)
}
#endif

#endif /* TIFF_JXL_H_DEFINED */