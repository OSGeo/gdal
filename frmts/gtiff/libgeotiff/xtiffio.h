/*
 *  xtiffio.h -- Public interface to Extended GEO TIFF tags
 *
 *    written by: Niles D. Ritter
 */

#ifndef __xtiffio_h
#define __xtiffio_h

#include "tiffio.h"

/* 
 *  Define public Tag names and values here 
 */

/* tags 33550 is a private tag registered to SoftDesk, Inc */
#define TIFFTAG_GEOPIXELSCALE       33550
/* tags 33920-33921 are private tags registered to Intergraph, Inc */
#define TIFFTAG_INTERGRAPH_MATRIX    33920   /* $use TIFFTAG_GEOTRANSMATRIX ! */
#define TIFFTAG_GEOTIEPOINTS         33922
/* tags 34263-34264 are private tags registered to NASA-JPL Carto Group */
#ifdef JPL_TAG_SUPPORT
#define TIFFTAG_JPL_CARTO_IFD        34263    /* $use GeoProjectionInfo ! */
#endif
#define TIFFTAG_GEOTRANSMATRIX       34264    /* New Matrix Tag replaces 33920 */
/* tags 34735-3438 are private tags registered to SPOT Image, Inc */
#define TIFFTAG_GEOKEYDIRECTORY      34735
#define TIFFTAG_GEODOUBLEPARAMS      34736
#define TIFFTAG_GEOASCIIPARAMS       34737

/* 
 *  Define Printing method flags. These
 *  flags may be passed in to TIFFPrintDirectory() to
 *  indicate that those particular field values should
 *  be printed out in full, rather than just an indicator
 *  of whether they are present or not.
 */
#define	TIFFPRINT_GEOKEYDIRECTORY	0x80000000
#define	TIFFPRINT_GEOKEYPARAMS		0x40000000

/**********************************************************************
 *    Nothing below this line should need to be changed by the user.
 **********************************************************************/

#if defined(__cplusplus)
extern "C" {
#endif

extern TIFF* XTIFFOpen(const char* name, const char* mode);
extern TIFF* XTIFFFdOpen(int fd, const char* name, const char* mode);
extern void  XTIFFClose(TIFF *tif);

#if defined(__cplusplus)
}
#endif

#endif /* __xtiffio_h */

