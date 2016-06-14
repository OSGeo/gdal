/**********************************************************************
 *
 *  geo_tiffp.c  Private TIFF interface module for GEOTIFF
 *
 *    This module implements the interface between the GEOTIFF
 *    tag parser and the TIFF i/o module. The current setup
 *    relies on the "libtiff" code, but if you use your own
 *    TIFF reader software, you may replace the module implementations
 *    here with your own calls. No "libtiff" dependencies occur
 *    anywhere else in this code.
 *
 * copyright (c) 1995   Niles D. Ritter
 *
 * Permission granted to use this software, so long as this copyright
 * notice accompanies any products derived therefrom.
 *
 **********************************************************************/

#include "geotiff.h"    /* public GTIFF interface */

#include "geo_tiffp.h"  /* Private TIFF interface */
#include "geo_keyp.h"   /* Private GTIFF interface */

/* tiff size array global */
gsize_t _gtiff_size[] = { 0, 1, 2, 4, 8, 1, 4, 8, 1, 2, 4, 1 };

static int        _GTIFGetField (tiff_t *tif, pinfo_t tag, int *count, void *value );
static int        _GTIFSetField (tiff_t *tif, pinfo_t tag, int  count, void *value );
static tagtype_t  _GTIFTagType  (tiff_t *tif, pinfo_t tag);

/*
 * Set up default TIFF handlers.
 */
void _GTIFSetDefaultTIFF(TIFFMethod *method)
{
	if (!method) return;

	method->get = _GTIFGetField;
	method->set = _GTIFSetField;
	method->type = _GTIFTagType;
}

gdata_t _GTIFcalloc(gsize_t size)
{
    gdata_t data=(gdata_t)_TIFFmalloc((tsize_t)size);
	if (data) _TIFFmemset((tdata_t)data,0,(tsize_t)size);
	return data;
}

gdata_t _GTIFrealloc(gdata_t ptr, gsize_t size)
{
    return _TIFFrealloc((tdata_t)ptr, (tsize_t) size);
}

void _GTIFmemcpy(gdata_t out,gdata_t in,gsize_t size)
{
	_TIFFmemcpy((tdata_t)out,(tdata_t)in,(tsize_t)size);
}

void _GTIFFree(gdata_t data)
{
	if (data) _TIFFfree((tdata_t)data);
}



/* returns the value of TIFF tag <tag>, or if
 * the value is an array, returns an allocated buffer
 * containing the values. Allocate a copy of the actual
 * buffer, sized up for updating.
 */
static int _GTIFGetField (tiff_t *tif, pinfo_t tag, int *count, void *val )
{
	int status;
	unsigned short scount=0;
	char *tmp;
	char *value;
	gsize_t size = _gtiff_size[_GTIFTagType (tif,tag)];

	if (_GTIFTagType(tif,  tag) == TYPE_ASCII)
	{
		status = TIFFGetField((TIFF *)tif,tag,&tmp);
		if (!status) return status;
		scount = (unsigned short) (strlen(tmp)+1);
	}
	else status = TIFFGetField((TIFF *)tif,tag,&scount,&tmp);
	if (!status) return status;

	*count = scount;

	value = (char *)_GTIFcalloc( (scount+MAX_VALUES)*size);
	if (!value) return 0;

	_TIFFmemcpy( value, tmp,  size * scount);

	*(char **)val = value;
	return status;
}

/*
 * Set a GeoTIFF TIFF field.
 */
static int _GTIFSetField (tiff_t *tif, pinfo_t tag, int count, void *value )
{
	int status;
	unsigned short scount = (unsigned short) count;

	/* libtiff ASCII uses null-delimiter */
	if (_GTIFTagType(tif,  tag) == TYPE_ASCII)
		status = TIFFSetField((TIFF *)tif,tag,value);
	else
		status = TIFFSetField((TIFF *)tif,tag,scount,value);
	return status;
}


/*
 *  This routine is supposed to return the TagType of the <tag>
 *  TIFF tag. Unfortunately, "libtiff" does not provide this
 *  service by default, so we just have to "know" what type of tags
 *  we've got, and how many. We only define the ones Geotiff
 *  uses here, and others return UNKNOWN. The "tif" parameter
 *  is provided for those TIFF implementations that provide
 *  for tag-type queries.
 */
static tagtype_t  _GTIFTagType  (tiff_t *tif, pinfo_t tag)
{
	tagtype_t ttype;

	(void) tif; /* dummy reference */

	switch (tag)
	{
		case GTIFF_ASCIIPARAMS:    ttype=TYPE_ASCII; break;
		case GTIFF_PIXELSCALE:
		case GTIFF_TRANSMATRIX:
		case GTIFF_TIEPOINTS:
		case GTIFF_DOUBLEPARAMS:   ttype=TYPE_DOUBLE; break;
		case GTIFF_GEOKEYDIRECTORY: ttype=TYPE_SHORT; break;
		default: ttype = TYPE_UNKNOWN;
	}

	return ttype;
}
