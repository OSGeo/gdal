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
 
#include "xtiffiop.h"
#include <stdio.h>

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
    { TIFFTAG_GEOPIXELSCALE,	-1,-1, TIFF_DOUBLE,	FIELD_GEOPIXELSCALE,
      TRUE,	TRUE,	"GeoPixelScale" },
    { TIFFTAG_INTERGRAPH_MATRIX,-1,-1, TIFF_DOUBLE,	FIELD_INTERGRAPH_MATRIX,
      TRUE,	TRUE,	"Intergraph TransformationMatrix" },
    { TIFFTAG_GEOTRANSMATRIX,	-1,-1, TIFF_DOUBLE,	FIELD_GEOTRANSMATRIX,
      TRUE,	TRUE,	"GeoTransformationMatrix" },
    { TIFFTAG_GEOTIEPOINTS,	-1,-1, TIFF_DOUBLE,	FIELD_GEOTIEPOINTS,
      TRUE,	TRUE,	"GeoTiePoints" },
    { TIFFTAG_GEOKEYDIRECTORY,-1,-1, TIFF_SHORT,	FIELD_GEOKEYDIRECTORY,
      TRUE,	TRUE,	"GeoKeyDirectory" },
    { TIFFTAG_GEODOUBLEPARAMS,	-1,-1, TIFF_DOUBLE,	FIELD_GEODOUBLEPARAMS,
      TRUE,	TRUE,	"GeoDoubleParams" },
    { TIFFTAG_GEOASCIIPARAMS,	-1,-1, TIFF_ASCII,	FIELD_GEOASCIIPARAMS,
      TRUE,	FALSE,	"GeoASCIIParams" },
#ifdef JPL_TAG_SUPPORT
    { TIFFTAG_JPL_CARTO_IFD,	 1, 1, TIFF_LONG,	FIELD_JPL_CARTO_IFD,
      TRUE,	TRUE,	"JPL Carto IFD offset" },  /** Don't use this! **/
#endif
};
#define	N(a)	(sizeof (a) / sizeof (a[0]))


static void
_XTIFFPrintDirectory(TIFF* tif, FILE* fd, long flags)
{
	xtiff *xt = XTIFFDIR(tif);
	XTIFFDirectory *xd = &xt->xtif_dir;
	int i,j,num;

	/* call the inherited method */
	if (PARENT(xt,printdir))
		(PARENT(xt,printdir))(tif,fd,flags);

	/* XXX Add field printing here */

	fprintf(fd,"--GeoTIFF Tags--\n");

	if (TIFFFieldSet(tif,FIELD_GEOTIEPOINTS))
	{
		num = xd->xd_geodimensions[GEO_NUM_TIEPOINT];
		fprintf(fd, "  Geo Tiepoints:");
		if (num>6) fprintf(fd,"\n    ");
		for (i=0;i<num;i+=6)
		{
		   fprintf(fd," (");
		   for (j=0;j<3;j++)
			fprintf(fd, " %lf", xd->xd_geotiepoints[i+j]);
		   fprintf(fd,")->(");
		   for (j=3;j<6;j++)
			fprintf(fd, " %lf", xd->xd_geotiepoints[i+j]);
		   fprintf(fd,")\n");
		}
	}

	if (TIFFFieldSet(tif,FIELD_GEOPIXELSCALE))
	{
		num = xd->xd_geodimensions[GEO_NUM_PIXELSCALE];
		fprintf(fd, "  Geo Pixel Scale: (");
		for (j=0;j<num;j++)
		   fprintf(fd, " %lf", xd->xd_geopixelscale[j]);
		fprintf(fd, " )\n");
	}

	if (TIFFFieldSet(tif,FIELD_INTERGRAPH_MATRIX))
	{
		num = xd->xd_geodimensions[GEO_NUM_IG_MATRIX];
		fprintf(fd, "  Intergraph Transformation Matrix:\n");
		for (i=0;num>3;num-=4)
		{
		   for (j=0;j<4;j++)
			fprintf(fd, " %8.2lf", xd->xd_geomatrix[i++]);
		   fprintf(fd, "\n");
		}
		if (num)
		{
		   for (j=0;j<num;j++)
			fprintf(fd, " %8.2lf", xd->xd_geomatrix[i++]);
		   fprintf(fd, "\n");
		}
	}

	if (TIFFFieldSet(tif,FIELD_GEOTRANSMATRIX))
	{
		num = xd->xd_geodimensions[GEO_NUM_MATRIX];
		fprintf(fd, "  Geo Transformation Matrix:\n");
		for (i=0;i<num;i+=4)
		{
		   for (j=0;j<4;j++)
			fprintf(fd, " %8.2lf", xd->xd_geomatrix[i+j]);
		   fprintf(fd, "\n");
		}
	}

	if (TIFFFieldSet(tif,FIELD_GEOKEYDIRECTORY))
	{
		num = xd->xd_geodimensions[GEO_NUM_DIR];
		fprintf(fd, "  GeoKey Directory:");
		if (flags & TIFFPRINT_GEOKEYDIRECTORY) 
		{
			fprintf(fd, "\n");
			for (i=0;i<num;i+=4)
			{
			   for (j=0;j<4;j++)
				fprintf(fd, "  %8hu", xd->xd_geokeydirectory[i+j]);
			   fprintf(fd, "\n");
			}
		} else
			fprintf(fd, "(present)\n");

	}

	if (TIFFFieldSet(tif,FIELD_GEODOUBLEPARAMS))
	{
		num = xd->xd_geodimensions[GEO_NUM_DOUBLE];
		fprintf(fd, "  GeoKey Double Params:");
		if (flags & TIFFPRINT_GEOKEYPARAMS) 
		{
			fprintf(fd, "\n");
			for (i=0;i<num;i++) 
				fprintf(fd, "  %8.2lf", xd->xd_geodoubleparams[i]);
			fprintf(fd, "\n");
		} else
			fprintf(fd, "(present)\n");

	}

	if (TIFFFieldSet(tif,FIELD_GEOASCIIPARAMS))
	{
		if (flags & TIFFPRINT_GEOKEYPARAMS) 
		{
			_TIFFprintAsciiTag(fd,"GeoKey ASCII Parameters",
				 xd->xd_geoasciiparams);
		} else
			fprintf(fd, "  GeoKey ASCII Parameters:(present)\n");
	}
}

static int
_XTIFFVSetField(TIFF* tif, ttag_t tag, va_list ap)
{
	xtiff *xt = XTIFFDIR(tif);
	XTIFFDirectory* xd = &xt->xtif_dir;
	int status = 1;
	uint32 v32=0;
	int i=0, v=0;
	uint16 num;

	/* va_start is called by the calling routine */
	
	switch (tag) {
		/* XXX put extended tags here */
	case TIFFTAG_GEOKEYDIRECTORY:
		xd->xd_geodimensions[GEO_NUM_DIR] = (uint16) va_arg(ap, int);
		_TIFFsetShortArray(&xd->xd_geokeydirectory, va_arg(ap, uint16*),
			(long) xd->xd_geodimensions[GEO_NUM_DIR]);
		break;
	case TIFFTAG_GEODOUBLEPARAMS:
		xd->xd_geodimensions[GEO_NUM_DOUBLE] = (uint16) va_arg(ap, int);
		_TIFFsetDoubleArray(&xd->xd_geodoubleparams, va_arg(ap, double*),
			(long) xd->xd_geodimensions[GEO_NUM_DOUBLE]);
		break;
	case TIFFTAG_GEOTIEPOINTS:
		xd->xd_geodimensions[GEO_NUM_TIEPOINT] = (uint16) va_arg(ap, int);
		_TIFFsetDoubleArray(&xd->xd_geotiepoints, va_arg(ap, double*),
			(long) xd->xd_geodimensions[GEO_NUM_TIEPOINT]);
	        break;
	case TIFFTAG_GEOTRANSMATRIX:
		xd->xd_geodimensions[GEO_NUM_MATRIX] = (uint16) va_arg(ap, int);
		_TIFFsetDoubleArray(&xd->xd_geomatrix, va_arg(ap, double*),
			(long) xd->xd_geodimensions[GEO_NUM_MATRIX]);
		break;
	case TIFFTAG_INTERGRAPH_MATRIX:
		num = (uint16) va_arg(ap, int);
		xd->xd_geodimensions[GEO_NUM_IG_MATRIX] = num;
		_TIFFsetDoubleArray(&xd->xd_intergraph_matrix, va_arg(ap, double*),
			(long) num);
		/* For backward compatibility; note that we only
                 * allow the Intergraph tag to affect the GeoTIFF tag
                 * if the count is correct (Intergraph's version uses 17).
                 */
		if (!TIFFFieldSet(tif,FIELD_GEOTRANSMATRIX) && num==16)
			TIFFSetField(tif,TIFFTAG_GEOTRANSMATRIX,
				num, xd->xd_intergraph_matrix);
		break;
	case TIFFTAG_GEOPIXELSCALE:
		xd->xd_geodimensions[GEO_NUM_PIXELSCALE] = (uint16) va_arg(ap, int);
		_TIFFsetDoubleArray(&xd->xd_geopixelscale, va_arg(ap, double*),
			(long) xd->xd_geodimensions[GEO_NUM_PIXELSCALE]);
		break;
	case TIFFTAG_GEOASCIIPARAMS:
		_TIFFsetString(&xd->xd_geoasciiparams, va_arg(ap, char*));
		break;
#ifdef JPL_TAG_SUPPORT
	case TIFFTAG_JPL_CARTO_IFD:
		xd->xd_jpl_ifd_offset = va_arg(ap, uint32);
		break;
#endif
	default:
		/* call the inherited method */
		return (PARENT(xt,vsetfield))(tif,tag,ap);
		break;
	}
	if (status) {
		/* we have to override the print method here,
 		 * after the compression tags have gotten to it.
		 * This makes sense because the only time we would
		 * need the extended print method is if an extended
		 * tag is set by either the reader or writer.
		 */
		if (!(xt->xtif_flags & XTIFFP_PRINT))
		{
	        	PARENT(xt,printdir) =  TIFFMEMBER(tif,printdir);
      		  	TIFFMEMBER(tif,printdir) = _XTIFFPrintDirectory;
			xt->xtif_flags |= XTIFFP_PRINT;
		}
		TIFFSetFieldBit(tif, _TIFFFieldWithTag(tif, tag)->field_bit);
		tif->tif_flags |= TIFF_DIRTYDIRECT;
	}
	va_end(ap);
	return (status);
badvalue:
	TIFFError(tif->tif_name, "%d: Bad value for \"%s\"", v,
	    _TIFFFieldWithTag(tif, tag)->field_name);
	va_end(ap);
	return (0);
badvalue32:
	TIFFError(tif->tif_name, "%ld: Bad value for \"%s\"", v32,
	    _TIFFFieldWithTag(tif, tag)->field_name);
	va_end(ap);
	return (0);
}


static int
_XTIFFVGetField(TIFF* tif, ttag_t tag, va_list ap)
{
	xtiff *xt = XTIFFDIR(tif);
	XTIFFDirectory* xd = &xt->xtif_dir;

	switch (tag) {
		/* XXX insert your tags here */
	case TIFFTAG_GEOKEYDIRECTORY:
		*va_arg(ap, uint16*) = xd->xd_geodimensions[GEO_NUM_DIR];
		*va_arg(ap, uint16**) = xd->xd_geokeydirectory;
		break;
	case TIFFTAG_GEODOUBLEPARAMS:
		*va_arg(ap, uint16*) = xd->xd_geodimensions[GEO_NUM_DOUBLE];
		*va_arg(ap, double**) = xd->xd_geodoubleparams;
		break;
	case TIFFTAG_GEOTIEPOINTS:
		*va_arg(ap, uint16*) = xd->xd_geodimensions[GEO_NUM_TIEPOINT];
		*va_arg(ap, double**) = xd->xd_geotiepoints;
		break;
	case TIFFTAG_GEOTRANSMATRIX:
		*va_arg(ap, uint16*) = xd->xd_geodimensions[GEO_NUM_MATRIX];
		*va_arg(ap, double**) = xd->xd_geomatrix;
		break;
	case TIFFTAG_INTERGRAPH_MATRIX:
		*va_arg(ap, uint16*) = xd->xd_geodimensions[GEO_NUM_IG_MATRIX];
		*va_arg(ap, double**) = xd->xd_intergraph_matrix;
		break;
	case TIFFTAG_GEOPIXELSCALE:
		*va_arg(ap, uint16*) = xd->xd_geodimensions[GEO_NUM_PIXELSCALE];
		*va_arg(ap, double**) = xd->xd_geopixelscale;
		break;
	case TIFFTAG_GEOASCIIPARAMS:
		*va_arg(ap, char**) = xd->xd_geoasciiparams;
		break;
#ifdef JPL_TAG_SUPPORT
	case TIFFTAG_JPL_CARTO_IFD:
		*va_arg(ap, uint32*) = xd->xd_jpl_ifd_offset;
		break;
#endif
	default:
		/* return inherited method */
		return (PARENT(xt,vgetfield))(tif,tag,ap);
		break;
	}
	return (1);
}

#define	CleanupField(member) {		\
    if (xd->member) {			\
	_TIFFfree(xd->member);		\
	xd->member = 0;			\
    }					\
}
/*
 * Release storage associated with a directory.
 */
static void
_XTIFFFreeDirectory(xtiff* xt)
{
	XTIFFDirectory* xd = &xt->xtif_dir;

	/* 
	 *  Purge all allocated memory except
	 *  for the xtiff directory itself. This includes
	 *  all fields that require a _TIFFsetXXX call in
	 *  _XTIFFVSetField().
	 */
	
	CleanupField(xd_geokeydirectory);
	CleanupField(xd_geodoubleparams);
	CleanupField(xd_geoasciiparams);
	CleanupField(xd_geomatrix);
	CleanupField(xd_geopixelscale);
	CleanupField(xd_geotiepoints);
	CleanupField(xd_intergraph_matrix);
	
}
#undef CleanupField

static void _XTIFFLocalDefaultDirectory(TIFF *tif)
{
	xtiff *xt = XTIFFDIR(tif);
	XTIFFDirectory* xd = &xt->xtif_dir;

	/* Install the extended Tag field info */
	_TIFFMergeFieldInfo(tif, xtiffFieldInfo, N(xtiffFieldInfo));

	/*
	 *  free up any dynamically allocated arrays
	 *  before the new directory is read in.
	 */
	 
	_XTIFFFreeDirectory(xt);	
	_TIFFmemset(xt,0,sizeof(xtiff));

	/* Override the tag access methods */

	PARENT(xt,vsetfield) =  TIFFMEMBER(tif,vsetfield);
	TIFFMEMBER(tif,vsetfield) = _XTIFFVSetField;
	PARENT(xt,vgetfield) =  TIFFMEMBER(tif,vgetfield);
	TIFFMEMBER(tif,vgetfield) = _XTIFFVGetField;

	/* 
	 * XXX Set up any default values here.
	 */
	
	/* NO DEFAULT GEOTIFF Values !*/

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
	xtiff *xt;
	
	/* Allocate Directory Structure if first time, and install it */
	if (!(tif->tif_flags & XTIFF_INITIALIZED))
	{
		xt = _TIFFmalloc(sizeof(xtiff));
		if (!xt)
		{
			/* handle memory allocation failure here ! */
			return;
		}
		_TIFFmemset(xt,0,sizeof(xtiff));
		/*
		 * Install into TIFF structure.
		 */
		TIFFMEMBER(tif,clientdir) = (tidata_t)xt;
		tif->tif_flags |= XTIFF_INITIALIZED; /* dont do this again! */
	}
	
	/* set up our own defaults */
	_XTIFFLocalDefaultDirectory(tif);

	/* Since an XTIFF client module may have overridden
	 * the default directory method, we call it now to
	 * allow it to set up the rest of its own methods.
         */

	if (_ParentExtender) 
		(*_ParentExtender)(tif);

}

/*
 *  XTIFF Initializer -- sets up the callback
 *   procedure for the TIFF module.
 */

static
void _XTIFFInitialize(void)
{
	static first_time=1;
	
	if (! first_time) return; /* Been there. Done that. */
	first_time = 0;
	
	/* Grab the inherited method and install */
	_ParentExtender = TIFFSetTagExtender(_XTIFFDefaultDirectory);
}


/*
 * Public File I/O Routines.
 */
TIFF*
XTIFFOpen(const char* name, const char* mode)
{
	TIFF *tif;

	/* Set up the callback */
	_XTIFFInitialize();	
	
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
	_XTIFFInitialize();	

	/* Open the file; the callback will set everything up
	 */
	tif = TIFFFdOpen(fd, name, mode);
	if (!tif) return tif;
	
	return tif;
}


void
XTIFFClose(TIFF *tif)
{
	xtiff *xt = XTIFFDIR(tif);
	
	/* call inherited function first */
	TIFFClose(tif);
	
	/* Free up extended allocated memory */
	_XTIFFFreeDirectory(xt);
	_TIFFfree(xt);
}
