/*
 * Private Extended TIFF library interface.
 *
 *  uses private LIBTIFF interface.
 *
 *  written by: Niles D. Ritter
 *
 *  Revisions:
 *    18 Sep 1995   -- Deprecated Integraph Matrix tag with new one.
 *                     Backward compatible support provided.  --NDR.
 *
 */

#ifndef __xtiffiop_h
#define __xtiffiop_h

#include "tiffiop.h"
#include "xtiffio.h"

/**********************************************************************
 *               User Configuration
 **********************************************************************/

/* Define number of extended tags here */
#define NUM_XFIELD 8
#define XFIELD_BASE (FIELD_LAST-NUM_XFIELD)

/*  Define Fields here  */
#define	FIELD_GEOPIXELSCALE     (XFIELD_BASE+0)
#define	FIELD_INTERGRAPH_MATRIX (XFIELD_BASE+1)
#define	FIELD_GEOTRANSMATRIX    (XFIELD_BASE+2)
#define	FIELD_GEOTIEPOINTS      (XFIELD_BASE+3)
#define	FIELD_GEOASCIIPARAMS    (XFIELD_BASE+4)
#define	FIELD_GEOKEYDIRECTORY   (XFIELD_BASE+5)
#define	FIELD_GEODOUBLEPARAMS   (XFIELD_BASE+6)
#ifdef JPL_TAG_SUPPORT
#define	FIELD_JPL_CARTO_IFD     (XFIELD_BASE+7)   /* unsupported */
#endif

/* Used for GEO tags having variable counts */
typedef enum {
	GEO_NUM_DIR=0,
	GEO_NUM_DOUBLE,
	GEO_NUM_TIEPOINT,
	GEO_NUM_PIXELSCALE,
	GEO_NUM_MATRIX,
	GEO_NUM_IG_MATRIX,
	GEO_NUM_TAGS
} geo_count_t;

/* Define Private directory structure here */
struct XTIFFDirectory {
	uint16	 xd_geodimensions[GEO_NUM_TAGS]; /* dir-count for the geo tags */
	uint16*  xd_geokeydirectory;
	double*  xd_geodoubleparams;
	char*    xd_geoasciiparams;
	double*  xd_geotiepoints;
	double*  xd_geopixelscale;
	double*  xd_geomatrix;
	double*  xd_intergraph_matrix;
#ifdef JPL_TAG_SUPPORT
	uint32   xd_jpl_ifd_offset; /* dont use */
#endif
};
typedef struct XTIFFDirectory XTIFFDirectory;

/**********************************************************************
 *    Nothing below this line should need to be changed by the user.
 **********************************************************************/

struct xtiff {
	TIFF 		*xtif_tif;	/* parent TIFF pointer */
	uint32		xtif_flags;
#define       XTIFFP_PRINT   0x00000001
	XTIFFDirectory	xtif_dir;	/* internal rep of current directory */
	TIFFVSetMethod	xtif_vsetfield;	/* inherited tag set routine */
	TIFFVGetMethod	xtif_vgetfield;	/* inherited tag get routine */
	TIFFPrintMethod	xtif_printdir;  /* inherited dir print method */
};
typedef struct xtiff xtiff;


#define PARENT(xt,pmember) ((xt)->xtif_ ## pmember) 
#define TIFFMEMBER(tf,pmember) ((tf)->tif_ ## pmember) 
#define XTIFFDIR(tif) ((xtiff *)TIFFMEMBER(tif,clientdir))
	
/* Extended TIFF flags */
#define XTIFF_INITIALIZED 0x80000000
	
#endif /* __xtiffiop_h */
