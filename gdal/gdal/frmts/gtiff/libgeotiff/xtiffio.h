/*
 *  xtiffio.h -- Public interface to Extended GEO TIFF tags
 *
 *    written by: Niles D. Ritter
 */

#ifndef LIBGEOTIFF_XTIFFIO_H_
#define LIBGEOTIFF_XTIFFIO_H_

#include "tiffio.h"
#include "geo_config.h"

/**
 * \file xtiffio.h
 *
 * Definitions relating GeoTIFF functions from geotiff.h to the TIFF
 * library (usually libtiff).
 */

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
#define TIFFTAG_JPL_CARTO_IFD        34263   /* $use GeoProjectionInfo ! */
#endif
#define TIFFTAG_GEOTRANSMATRIX       34264   /* New Matrix Tag replaces 33920 */
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

/**********************************************************************
 * Do we want to build as a DLL on windows?
 **********************************************************************/
#if !defined(CPL_DLL)
#  if defined(_WIN32) && defined(BUILD_AS_DLL)
#    define CPL_DLL     __declspec(dllexport)
#  else
#    define CPL_DLL
#  endif
#endif

extern void CPL_DLL XTIFFInitialize(void);
extern TIFF CPL_DLL * XTIFFOpen(const char* name, const char* mode);
extern TIFF CPL_DLL * XTIFFFdOpen(int fd, const char* name, const char* mode);
extern void CPL_DLL XTIFFClose(TIFF *tif);

extern TIFF CPL_DLL * XTIFFClientOpen(const char* name, const char* mode,
                                      thandle_t thehandle,
                                      TIFFReadWriteProc, TIFFReadWriteProc,
                                      TIFFSeekProc, TIFFCloseProc,
                                      TIFFSizeProc,
                                      TIFFMapFileProc, TIFFUnmapFileProc);
#if defined(__cplusplus)
}
#endif

#endif /* LIBGEOTIFF_XTIFFIO_H_ */
