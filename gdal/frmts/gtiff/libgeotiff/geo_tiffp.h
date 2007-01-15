/**********************************************************************
 *
 *  geo_tiffp.h - Private interface for TIFF tag parsing.
 *
 *   Written by: Niles D. Ritter
 *
 *   This interface file encapsulates the interface to external TIFF
 *   file-io routines and definitions. The current configuration
 *   assumes that the "libtiff" module is used, but if you have your
 *   own TIFF reader, you may replace the definitions with your own
 *   here, and replace the implementations in geo_tiffp.c. No other
 *   modules have any explicit dependence on external TIFF modules.
 *
 *  Revision History;
 *
 *    20 June, 1995      Niles D. Ritter         New
 *    6 July,  1995      Niles D. Ritter         Fix prototypes
 *
 **********************************************************************/

#ifndef __geo_tiffp_h_
#define __geo_tiffp_h_

/**********************************************************************
 *
 *                        Private includes
 *
 *   If you are not using libtiff and XTIFF, replace this include file
 *    with the appropriate one for your own TIFF parsing routines.
 *
 *   Revision History
 * 
 *      19 September 1995   ndr    Demoted Intergraph trans matrix.
 *
 **********************************************************************/

#include "geotiff.h"
#include "xtiffio.h"
#include "cpl_serv.h"

/*
 * dblparam_t is the type that a double precision
 * floating point value will have on the parameter
 * stack (when coerced by the compiler). You shouldn't
 * have to change this.
 */
#ifdef applec
typedef extended dblparam_t;
#else
typedef double dblparam_t;
#endif


/**********************************************************************
 *
 *                        Private defines
 *
 *   If you are not using "libtiff"/LIBXTIFF, replace these definitions
 *   with the appropriate definitions to access the geo-tags
 *
 **********************************************************************/
 
typedef unsigned short pinfo_t;    /* SHORT ProjectionInfo tag type */
typedef TIFF    tiff_t;            /* TIFF file descriptor          */
typedef tdata_t  gdata_t;          /* pointer to data */
typedef tsize_t  gsize_t;          /* data allocation size */
 
#define GTIFF_GEOKEYDIRECTORY   TIFFTAG_GEOKEYDIRECTORY /* from xtiffio.h */
#define GTIFF_DOUBLEPARAMS      TIFFTAG_GEODOUBLEPARAMS
#define GTIFF_ASCIIPARAMS       TIFFTAG_GEOASCIIPARAMS
#define GTIFF_PIXELSCALE        TIFFTAG_GEOPIXELSCALE
#define GTIFF_TRANSMATRIX       TIFFTAG_GEOTRANSMATRIX
#define GTIFF_INTERGRAPH_MATRIX TIFFTAG_INTERGRAPH_MATRIX
#define GTIFF_TIEPOINTS         TIFFTAG_GEOTIEPOINTS
#define GTIFF_LOCAL          0

#if defined(__cplusplus)
extern "C" {
#endif

/*
 * Method function pointer types
 */
typedef int        (*GTGetFunction) (tiff_t *tif, pinfo_t tag, int *count, void *value );
typedef int        (*GTSetFunction) (tiff_t *tif, pinfo_t tag, int  count, void *value );
typedef tagtype_t  (*GTTypeFunction) (tiff_t *tif, pinfo_t tag);
typedef struct     _TIFFMethod {
	GTGetFunction get;
	GTSetFunction set;
	GTTypeFunction type;
} TIFFMethod;

/**********************************************************************
 *
 *               Protected Function Declarations  
 *
 *   These routines are exposed implementations, and should not
 *   be used by external GEOTIFF client programs.
 *
 **********************************************************************/

extern gsize_t _gtiff_size[]; /* TIFF data sizes */
extern void CPL_DLL _GTIFSetDefaultTIFF(TIFFMethod *method);
extern gdata_t CPL_DLL _GTIFcalloc(gsize_t);
extern gdata_t CPL_DLL _GTIFrealloc(gdata_t,gsize_t);
extern void CPL_DLL _GTIFFree(gdata_t data);
extern void CPL_DLL _GTIFmemcpy(gdata_t out,gdata_t in,gsize_t size);

#if defined(__cplusplus)
} 
#endif


#endif /* __geo_tiffp_h_ */
