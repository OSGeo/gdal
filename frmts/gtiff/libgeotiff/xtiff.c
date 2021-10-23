/*
 * xtiff.c
 *
 * Extended TIFF Directory GEO Tag Support.
 *
 *  You may use this file as a template to add your own
 *  extended tags to the library. Only the parts of the code
 *  marked with "XXX" require modification.
 *
 *  Author: Niles D. Ritter
 *
 *  Revisions:
 *    18 Sep 1995   -- Deprecated Integraph Matrix tag with new one.
 *                     Backward compatible support provided.  --NDR.
 */

#include "xtiffio.h"
#include <stdio.h>
#include "cpl_serv.h"

/*  Tiff info structure.
 *
 *     Entry format:
 *        { TAGNUMBER, ReadCount, WriteCount, DataType, FIELDNUM,
 *          OkToChange, PassDirCountOnSet, AsciiName }
 *
 *     For ReadCount, WriteCount, -1 = unknown.
 */

static const TIFFFieldInfo xtiffFieldInfo[] = {

  /* XXX Insert Your tags here */
    { TIFFTAG_GEOPIXELSCALE,	-1,-1, TIFF_DOUBLE,	FIELD_CUSTOM,
      TRUE,	TRUE,	"GeoPixelScale" },
    { TIFFTAG_INTERGRAPH_MATRIX,-1,-1, TIFF_DOUBLE,	FIELD_CUSTOM,
      TRUE,	TRUE,	"Intergraph TransformationMatrix" },
    { TIFFTAG_GEOTRANSMATRIX,	-1,-1, TIFF_DOUBLE,	FIELD_CUSTOM,
      TRUE,	TRUE,	"GeoTransformationMatrix" },
    { TIFFTAG_GEOTIEPOINTS,	-1,-1, TIFF_DOUBLE,	FIELD_CUSTOM,
      TRUE,	TRUE,	"GeoTiePoints" },
    { TIFFTAG_GEOKEYDIRECTORY,-1,-1, TIFF_SHORT,	FIELD_CUSTOM,
      TRUE,	TRUE,	"GeoKeyDirectory" },
    { TIFFTAG_GEODOUBLEPARAMS,	-1,-1, TIFF_DOUBLE,	FIELD_CUSTOM,
      TRUE,	TRUE,	"GeoDoubleParams" },
    { TIFFTAG_GEOASCIIPARAMS,	-1,-1, TIFF_ASCII,	FIELD_CUSTOM,
      TRUE,	FALSE,	"GeoASCIIParams" },
#ifdef JPL_TAG_SUPPORT
    { TIFFTAG_JPL_CARTO_IFD,	 1, 1, TIFF_LONG,	FIELD_CUSTOM,
      TRUE,	TRUE,	"JPL Carto IFD offset" },  /** Don't use this! **/
#endif
};

#define	N(a)	(sizeof (a) / sizeof (a[0]))
static void _XTIFFLocalDefaultDirectory(TIFF *tif)
{
    /* Install the extended Tag field info */
    TIFFMergeFieldInfo(tif, xtiffFieldInfo, N(xtiffFieldInfo));
}


/**********************************************************************
 *    Nothing below this line should need to be changed.
 **********************************************************************/

static TIFFExtendProc _ParentExtender;

/*
 *  This is the callback procedure, and is
 *  called by the DefaultDirectory method
 *  every time a new TIFF directory is opened.
 */

static void
_XTIFFDefaultDirectory(TIFF *tif)
{
    /* set up our own defaults */
    _XTIFFLocalDefaultDirectory(tif);

    /* Since an XTIFF client module may have overridden
     * the default directory method, we call it now to
     * allow it to set up the rest of its own methods.
     */

    if (_ParentExtender)
        (*_ParentExtender)(tif);
}


/**
Registers an extension with libtiff for adding GeoTIFF tags.
After this one-time initialization, any TIFF open function may be called in
the usual manner to create a TIFF file that compatible with libgeotiff.
The XTIFF open functions are simply for convenience: they call this
and then pass their parameters on to the appropriate TIFF open function.

<p>This function may be called any number of times safely, since it will
only register the extension the first time it is called.
**/

void XTIFFInitialize(void)
{
    static int first_time=1;

    if (! first_time) return; /* Been there. Done that. */
    first_time = 0;

    /* Grab the inherited method and install */
    _ParentExtender = TIFFSetTagExtender(_XTIFFDefaultDirectory);
}


/**
 * GeoTIFF compatible TIFF file open function.
 *
 * @param name The filename of a TIFF file to open.
 * @param mode The open mode ("r", "w" or "a").
 *
 * @return a TIFF * for the file, or NULL if the open failed.
 *
This function is used to open GeoTIFF files instead of TIFFOpen() from
libtiff.  Internally it calls TIFFOpen(), but sets up some extra hooks
so that GeoTIFF tags can be extracted from the file.  If XTIFFOpen() isn't
used, GTIFNew() won't work properly.  Files opened
with XTIFFOpen() should be closed with XTIFFClose().

The name of the file to be opened should be passed as <b>name</b>, and an
opening mode ("r", "w" or "a") acceptable to TIFFOpen() should be passed as the
<b>mode</b>.<p>

If XTIFFOpen() fails it will return NULL.  Otherwise, normal TIFFOpen()
error reporting steps will have already taken place.<p>
 */

TIFF*
XTIFFOpen(const char* name, const char* mode)
{
    TIFF *tif;

    /* Set up the callback */
    XTIFFInitialize();

    /* Open the file; the callback will set everything up
     */
    tif = TIFFOpen(name, mode);
    if (!tif) return tif;

    return tif;
}

TIFF*
XTIFFFdOpen(int fd, const char* name, const char* mode)
{
    TIFF *tif;

    /* Set up the callback */
    XTIFFInitialize();

    /* Open the file; the callback will set everything up
     */
    tif = TIFFFdOpen(fd, name, mode);
    if (!tif) return tif;

    return tif;
}

TIFF*
XTIFFClientOpen(const char* name, const char* mode, thandle_t thehandle,
	    TIFFReadWriteProc RWProc, TIFFReadWriteProc RWProc2,
	    TIFFSeekProc SProc, TIFFCloseProc CProc,
	    TIFFSizeProc SzProc,
	    TIFFMapFileProc MFProvc, TIFFUnmapFileProc UMFProc )
{
    TIFF *tif;

    /* Set up the callback */
    XTIFFInitialize();

    /* Open the file; the callback will set everything up
     */
    tif = TIFFClientOpen(name, mode, thehandle,
                         RWProc, RWProc2,
                         SProc, CProc,
                         SzProc,
                         MFProvc, UMFProc);

    if (!tif) return tif;

    return tif;
}

/**
 * Close a file opened with XTIFFOpen().
 *
 * @param tif The file handle returned by XTIFFOpen().
 *
 * If a GTIF structure was created with GTIFNew()
 * for this file, it should be freed with GTIFFree()
 * <i>before</i> calling XTIFFClose().
*/

void
XTIFFClose(TIFF *tif)
{
    TIFFClose(tif);
}
