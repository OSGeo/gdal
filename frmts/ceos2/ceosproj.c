/* Copyright (c) 1997
 * Atlantis Scientific Inc, 20 Colonnade, Suite 110
 * Nepean, Ontario, K2E 7M6, Canada
 *
 * All rights reserved.  Not to be used, reproduced
 * or disclosed without permission.
 */

/* +---------------------------------------------------------------------+
 * |@@@@@@@@@@    @@@| EASI/PACE V6.0, Copyright (c) 1997.               |
 * |@@@@@@ ***      @|                                                   |
 * |@@@  *******    @| PCI Inc., 50 West Wilmot Street,                  |
 * |@@  *********  @@| Richmond Hill, Ontario, L4B 1M5, Canada.          |
 * |@    *******  @@@|                                                   |
 * |@      *** @@@@@@| All rights reserved. Not to be used, reproduced   |
 * |@@@    @@@@@@@@@@| or disclosed without permission.                  |
 * +---------------------------------------------------------------------+
 */

#include "ceos.h"
#include "cclproj.h"

#define MAP_PROJ_RECORD_TYPECODE   { 18, 20, 18, 20 }

#define PROCESSED_DATA_RECORD_PREFIX_LENGTH  192

typedef struct keyval
{
    char * key;
    int    value;

} key_value_pair;

static key_value_pair projection_list[] =
{
    { "UNIVERSAL TRANSVERSE MERCATOR",    GEO_UTM_ZONE },
    { "UTM",                              GEO_UTM_ZONE },
    { "UNIVERSAL POLAR STEREOGRAPHIC",    GEO_UPS      },
    { "UPS",                              GEO_UPS      },
    { "ALBERS CONICAL EQUAL-AREA",        GEO_ACEA     },
    { "AZIMUTHAL EQUIDISTANT",            GEO_AE       },
    { "EQUADISTANT CONIC",                GEO_EC       },
    { "EQUIRECTANGULAR",                  GEO_ER       },
    { "GENERAL VERTICAL NEAR SIDE PERSP", GEO_GVNP     },
    { "GNOMONIC",                         GEO_GNO      },
    { "LAMBERT AZIMUTHAL EQUAL-AREA",     GEO_LAEA     },
    { "LAMBERT CONFORMAL",                GEO_LCC      },
    { "HOTINE OBLIQUE MERCATOR",          GEO_OM       },
    { "OBLIQUE MERCATOR",                 GEO_OM       },
    { "MERCATOR",                         GEO_MER      },
    { "MILLAR CYLINDRICAL",               GEO_MC       },
    { "ORTHOGRAPHIC",                     GEO_OG       },
    { "POLAR STEREOGRAPHIC",              GEO_PS       },
    { "POLYCONIC",                        GEO_PC       },
    { "SINUSOIDAL",                       GEO_SIN      },
    { "STATE PLANE",                      GEO_SPCS     },
    { "STEREOGRAPHIC",                    GEO_SG       },
    { "TRANSVERSE MERCATOR",              GEO_TM       },
    { "VAN DER GRINTEN",                  GEO_VDG      },
    { NULL, 0 }
};

static key_value_pair ellipsoid_list[] =
{
    { "AIRY",                              9 },
    { "MODIFIED AIRY",                    11 },
    { "AUSTRIAN NATIONAL",                14 },
    { "BESSEL",                            2 },
    { "CLARKE 1886",                       0 },
    { "CLARKE 1880",                       1 },
    { "CLARKE",                            0 },
    { "EVEREST 1830",                      6 },
    { "EVEREST 1948",                     10 },
    { "EVEREST",                           6 },
    { "FISCHER 1960",                     17 },
    { "FISCHER 1968",                     18 },
    { "FISCHER",                          17 },
    { "MODIFIED FISCHER",                 13 },
    { "GRS",                               8 },
    { "HOUGH",                            16 },
    { "INTERNATIONAL",                     4 },
    { "KRASSOVSKY",                       15 },
    { "NEW INTERNATIONAL",                 3 },
    { "SPHERE",                           19 },
    { "NORMAL SPHERE",                    19 },
    { "SOUTH AMERICAN",                   14 },
    { "WGS 66",                            7 },
    { "WGS 72",                            5 },
    { "WGS 84",                           12 },
    { "WGS",                              12 },
    { NULL, 0 }
};

void GetEmbeddedLatLongData( FILE *fp, CeosSARVolume_t *volume, ProjInfo_t *proj );

void GetCeosProjectionData( FILE *fp, CeosSARVolume_t *volume, ProjInfo_t *proj )
{
    CeosRecord_t *proj_rec;
    unsigned char proj_code[] = MAP_PROJ_RECORD_TYPECODE;

    char ellipsoid_name[ 33 ];
    char projection_name[ 33 ];
    int earthmodel;
    int projection;
    int i;
    
    char proj_str[ 17 ];
    char utm_zone[ 5 ];

    double dval;
    double top, left, right, bottom;

    ProjInfo_t long_proj, copy_proj;

    /* Initialize the ProjInfo_t structure */
    InitProjInfo( proj, 1 );
    proj->IFields = 1;


    /* Look for a map projection data record */
    proj_rec = FindCeosRecord( volume->RecordList, *(CeosTypeCode_t *) &proj_code, -1, -1, -1 );
    if ( proj_rec == NULL )
    {
	/* No map projection record found.  Use embedded (in-line) lat/long data instead */
	GetEmbeddedLatLongData( fp, volume, proj );
	return;
    }

    /* Get the projection name */
    GetCeosField( proj_rec, 413, "A32", projection_name );
    if ( projection_name[ 0 ] == ' ' )
    {
	/* Field at 413 is blank, try field at 29 */
	GetCeosField( proj_rec, 29, "A32", projection_name );
	if ( projection_name[ 0 ] == ' ' )
	{
	    /* No projection name given: default to UTM */
	    strcpy( projection_name, "UTM" );
	}
    }
    
    /* Decode the projection name */
    projection = -1;
    for ( i = 0; projection_list[ i ].key != NULL; i++ )
    {
	if ( strncasecmp( projection_name, projection_list[ i ].key,
			  strlen( projection_list[ i ].key ) ) == 0 )
	{
	    projection = projection_list[ i ].value;
	    break;
	}
    }    

    /* Check for unsupported or badly formatted projection string */
    if ( projection == -1 )
    {
	/* Default to obtaining embedded lat/long data */
	GetEmbeddedLatLongData( fp, volume, proj );
	return;
    }	

    /* Get the ellipsoid name */
    GetCeosField( proj_rec, 237, "A32", ellipsoid_name );
    
    /* Decode the ellipsoid name */
    earthmodel = -1;
    for ( i = 0; ellipsoid_list[ i ].key != NULL; i++ )
    {
	if ( strncasecmp( ellipsoid_name, ellipsoid_list[ i ].key,
			  strlen( ellipsoid_list[ i ].key ) ) == 0 )
	{
	    earthmodel = ellipsoid_list[ i ].value;
	    break;
	}
    }

    /* Get projection parameters based on projection type */
    switch ( projection )
    {
    case GEO_UTM_ZONE:
	GetCeosField( proj_rec, 477, "A4", utm_zone );
	sprintf( proj_str, "UTM %s", utm_zone );
	break;

    case GEO_UPS:
	strcpy( proj_str, "UPS" );
	dval = 1000.0;
	GetCeosField( proj_rec, 641, "F16.7", &dval );
	if ( dval != 1000.0 )
	{
	    if ( dval >= 0.0 )
		strcat( proj_str, " A" );
	    else
		strcat( proj_str, " Z" );
	}
	break;

    case GEO_ACEA:
	strcpy( proj_str, "ACEA" );
	GetCeosField( proj_rec, 705, "F16.7", &(proj->FalseEasting) );
	GetCeosField( proj_rec, 721, "F16.7", &(proj->FalseNorthing) );
	GetCeosField( proj_rec, 737, "F16.7", &(proj->RefLong) );
	GetCeosField( proj_rec, 753, "F16.7", &(proj->RefLat) );
	GetCeosField( proj_rec, 769, "F16.7", &(proj->StdParallel1) );
	GetCeosField( proj_rec, 785, "F16.7", &(proj->StdParallel2) );
	break;

    case GEO_AE:
	strcpy( proj_str, "AE" );
	GetCeosField( proj_rec, 705, "F16.7", &(proj->FalseEasting) );
	GetCeosField( proj_rec, 721, "F16.7", &(proj->FalseNorthing) );
	GetCeosField( proj_rec, 737, "F16.7", &(proj->RefLong) );
	GetCeosField( proj_rec, 753, "F16.7", &(proj->RefLat) );
	break;

    case GEO_EC:
	strcpy( proj_str, "EC" );
	GetCeosField( proj_rec, 705, "F16.7", &(proj->FalseEasting) );
	GetCeosField( proj_rec, 721, "F16.7", &(proj->FalseNorthing) );
	GetCeosField( proj_rec, 737, "F16.7", &(proj->RefLong) );
	GetCeosField( proj_rec, 753, "F16.7", &(proj->RefLat) );
	GetCeosField( proj_rec, 769, "F16.7", &(proj->StdParallel1) );
	GetCeosField( proj_rec, 785, "F16.7", &(proj->StdParallel2) );
	if ( proj->StdParallel2 < -90.0 || proj->StdParallel2 > 90.0 )
	{
	    proj->StdParallel2 = 0.0;
	}
	break;

    case GEO_ER:
	strcpy( proj_str, "ER" );
	GetCeosField( proj_rec, 705, "F16.7", &(proj->FalseEasting) );
	GetCeosField( proj_rec, 721, "F16.7", &(proj->FalseNorthing) );
	GetCeosField( proj_rec, 737, "F16.7", &(proj->RefLong) );
	GetCeosField( proj_rec, 769, "F16.7", &(proj->RefLat) );
	break;

    case GEO_GNO:
	strcpy( proj_str, "GNO" );
	GetCeosField( proj_rec, 705, "F16.7", &(proj->FalseEasting) );
	GetCeosField( proj_rec, 721, "F16.7", &(proj->FalseNorthing) );
	GetCeosField( proj_rec, 737, "F16.7", &(proj->RefLong) );
	GetCeosField( proj_rec, 753, "F16.7", &(proj->RefLat) );
	break;

    case GEO_GVNP:
	strcpy( proj_str, "GVNP" );
	GetCeosField( proj_rec, 705, "F16.7", &(proj->FalseEasting) );
	GetCeosField( proj_rec, 721, "F16.7", &(proj->FalseNorthing) );
	GetCeosField( proj_rec, 737, "F16.7", &(proj->RefLong) );
	GetCeosField( proj_rec, 753, "F16.7", &(proj->RefLat) );
	GetCeosField( proj_rec, 881, "F16.7", &(proj->Height) );
	break;

    case GEO_LAEA:
	strcpy( proj_str, "LAEA" );
	GetCeosField( proj_rec, 705, "F16.7", &(proj->FalseEasting) );
	GetCeosField( proj_rec, 721, "F16.7", &(proj->FalseNorthing) );
	GetCeosField( proj_rec, 737, "F16.7", &(proj->RefLong) );
	GetCeosField( proj_rec, 753, "F16.7", &(proj->RefLat) );
	break;

    case GEO_LCC:
	strcpy( proj_str, "LCC" );
	GetCeosField( proj_rec, 705, "F16.7", &(proj->FalseEasting) );
	GetCeosField( proj_rec, 721, "F16.7", &(proj->FalseNorthing) );
	GetCeosField( proj_rec, 737, "F16.7", &(proj->RefLong) );
	GetCeosField( proj_rec, 753, "F16.7", &(proj->RefLat) );
	GetCeosField( proj_rec, 769, "F16.7", &(proj->StdParallel1) );
	GetCeosField( proj_rec, 785, "F16.7", &(proj->StdParallel2) );
	break;

    case GEO_MC:
	strcpy( proj_str, "MC" );
	GetCeosField( proj_rec, 705, "F16.7", &(proj->FalseEasting) );
	GetCeosField( proj_rec, 721, "F16.7", &(proj->FalseNorthing) );
	GetCeosField( proj_rec, 737, "F16.7", &(proj->RefLong) );
	break;

    case GEO_MER:
	strcpy( proj_str, "MER" );
	GetCeosField( proj_rec, 705, "F16.7", &(proj->FalseEasting) );
	GetCeosField( proj_rec, 721, "F16.7", &(proj->FalseNorthing) );
	GetCeosField( proj_rec, 737, "F16.7", &(proj->RefLong) );
	GetCeosField( proj_rec, 769, "F16.7", &(proj->RefLat) );
	break;

    case GEO_OG:
	strcpy( proj_str, "OG" );
	GetCeosField( proj_rec, 705, "F16.7", &(proj->FalseEasting) );
	GetCeosField( proj_rec, 721, "F16.7", &(proj->FalseNorthing) );
	GetCeosField( proj_rec, 737, "F16.7", &(proj->RefLong) );
	GetCeosField( proj_rec, 753, "F16.7", &(proj->RefLat) );
	break;

    case GEO_OM:
	strcpy( proj_str, "OM" );
	GetCeosField( proj_rec, 705, "F16.7", &(proj->FalseEasting) );
	GetCeosField( proj_rec, 721, "F16.7", &(proj->FalseNorthing) );
	GetCeosField( proj_rec, 753, "F16.7", &(proj->RefLat) );
	GetCeosField( proj_rec, 769, "F16.7", &(proj->Lat1) );
	GetCeosField( proj_rec, 785, "F16.7", &(proj->Lat2) );
	GetCeosField( proj_rec, 833, "F16.7", &(proj->RefLong ) );
	GetCeosField( proj_rec, 833, "F16.7", &(proj->Long1) );
	GetCeosField( proj_rec, 849, "F16.7", &(proj->Long2) );
	GetCeosField( proj_rec, 881, "F16.7", &(proj->Scale) );
	GetCeosField( proj_rec, 897, "F16.7", &(proj->Azimuth) );
	if ( proj->RefLong < -180.0 || proj->RefLong > 180.0 )
	{
	    proj->RefLong = 0.0;
	}
	if ( proj->Long1 < -180.0 || proj->Long1 > 180.0 )
	{
	    proj->Long1 = 0.0;
	}
	if ( proj->Long2 < -180.0 || proj->Long2 > 180.0 )
	{
	    proj->Long2 = 0.0;
	}
	if ( proj->Lat1 < -90.0 || proj->Lat1 > 90.0 )
	{
	    proj->Lat1 = 0.0;
	}
	if ( proj->Lat2 < -90.0 || proj->Lat2 > 90.0 )
	{
	    proj->Lat2 = 0.0;
	}
	break;

    case GEO_PC:
	strcpy( proj_str, "PC" );
	GetCeosField( proj_rec, 705, "F16.7", &(proj->FalseEasting) );
	GetCeosField( proj_rec, 721, "F16.7", &(proj->FalseNorthing) );
	GetCeosField( proj_rec, 737, "F16.7", &(proj->RefLong) );
	GetCeosField( proj_rec, 753, "F16.7", &(proj->RefLat) );
	break;

    case GEO_PS:
	strcpy( proj_str, "PS" );
	GetCeosField( proj_rec, 705, "F16.7", &(proj->FalseEasting) );
	GetCeosField( proj_rec, 721, "F16.7", &(proj->FalseNorthing) );
	GetCeosField( proj_rec, 737, "F16.7", &(proj->RefLong) );
	GetCeosField( proj_rec, 769, "F16.7", &(proj->RefLat) );
	break;

    case GEO_SG:
	strcpy( proj_str, "SG" );
	GetCeosField( proj_rec, 705, "F16.7", &(proj->FalseEasting) );
	GetCeosField( proj_rec, 721, "F16.7", &(proj->FalseNorthing) );
	GetCeosField( proj_rec, 737, "F16.7", &(proj->RefLong) );
	GetCeosField( proj_rec, 753, "F16.7", &(proj->RefLat) );
	break;

    case GEO_SIN:
	strcpy( proj_str, "SIN" );
	GetCeosField( proj_rec, 705, "F16.7", &(proj->FalseEasting) );
	GetCeosField( proj_rec, 721, "F16.7", &(proj->FalseNorthing) );
	GetCeosField( proj_rec, 737, "F16.7", &(proj->RefLong) );
	break;

    case GEO_SPCS:
	GetCeosField( proj_rec, 881, "F16.7", &dval );
	sprintf( proj_str, "SPCS %d", (int) dval );
	/* Check earth model */
	if ( earthmodel != 0 && earthmodel != 8 )
	{
	    earthmodel = -1;
	}
	break;

    case GEO_TM:
	strcpy( proj_str, "TM" );
	GetCeosField( proj_rec, 705, "F16.7", &(proj->FalseEasting) );
	GetCeosField( proj_rec, 721, "F16.7", &(proj->FalseNorthing) );
	GetCeosField( proj_rec, 737, "F16.7", &(proj->RefLong) );
	GetCeosField( proj_rec, 753, "F16.7", &(proj->RefLat) );
	GetCeosField( proj_rec, 881, "F16.7", &(proj->Scale) );
	break;

    case GEO_VDG:
	strcpy( proj_str, "VDG" );
	GetCeosField( proj_rec, 705, "F16.7", &(proj->FalseEasting) );
	GetCeosField( proj_rec, 721, "F16.7", &(proj->FalseNorthing) );
	GetCeosField( proj_rec, 737, "F16.7", &(proj->RefLong) );
	break;

    default:
	strcpy( proj_str, "LONG" );
	break;
    }

    /* Add the earth model to form the geosys string */
    if ( earthmodel == -1 )
    {
	strcpy( proj->Units, proj_str );
    }
    else
    {
	sprintf( proj->Units, "%s E%03d", proj_str, earthmodel );
    }   
    DecodeGeosys( proj->Units, proj->Units );
    
    /* Read the corner points */
    if ( projection == GEO_UTM || projection == GEO_UTM_ZONE ||
	 projection == GEO_UPS )
    {
	/* Use northing and easting data */
	GetCeosField( proj_rec, 945, "F16.7", &top );
	GetCeosField( proj_rec, 961, "F16.7", &left );
	GetCeosField( proj_rec, 993, "F16.7", &right );
	GetCeosField( proj_rec, 1041, "F16.7", &bottom );
    }
    else
    {
	/* Use lat/long data */
	GetCeosField( proj_rec, 1073, "F16.7", &top );
	GetCeosField( proj_rec, 1089, "F16.7", &left );
	GetCeosField( proj_rec, 1121, "F16.7", &right );
	GetCeosField( proj_rec, 1169, "F16.7", &bottom );	
	
	/* Set up for GCTP transforms */
	copy_proj = *proj;
	Geosys2ProjInfo( &long_proj, "LONG E0", 0.0, 0.0, 1.0, 1.0 );
	PCI2GCTP( &copy_proj );
	PCI2GCTP( &long_proj );

	/* Transform lat/long coordinates to projection coordinates */
	GCTPTransform( &long_proj, right, top,
		       &copy_proj, &right, &dval );
	GCTPTransform( &long_proj, left, bottom,
		       &copy_proj, &dval, &bottom );
	GCTPTransform( &long_proj, left, top,
		       &copy_proj, &left, &top );
    }

    /* Calculate offset and pixel size (projection units) */
    proj->XOff = left;
    proj->YOff = top;
    proj->XSize = ( right  - proj->XOff ) / volume->ImageDesc.PixelsPerLine;
    proj->YSize = ( bottom - proj->YOff ) / volume->ImageDesc.Lines;
}

void GetEmbeddedLatLongData( FILE *fp, CeosSARVolume_t *volume, ProjInfo_t *proj )
{
    int32 top, left, right, bottom;
    CeosRecord_t record;
    int start;
    uchar buffer[ PROCESSED_DATA_RECORD_PREFIX_LENGTH ];

    /* Set projection type to lat/long */
    Geosys2ProjInfo( proj, "LONG E0", 0.0, 0.0, 1.0, 1.0 );

    /* Init temporary record */
    record.Length = PROCESSED_DATA_RECORD_PREFIX_LENGTH;
    record.Buffer = buffer;

    /* Read the first processed data record (prefix only) */
    CalcCeosSARImageFilePosition( volume, 1, 1, NULL, &start );

    if ( DKRead( fp, record.Buffer, start, record.Length ) != record.Length )
    {
	return;
    }

    /* Get top-left corner point */
    GetCeosField( &record, 133, "B4", &top );
    GetCeosField( &record, 145, "B4", &left );

    /* Read the last processed data record (prefix only) */
    CalcCeosSARImageFilePosition( volume, 1, volume->ImageDesc.Lines, NULL, &start );

    if ( DKRead( fp, record.Buffer, start, record.Length ) != record.Length )
    {
	return;
    }

    /* Get bottom-right corner point */
    GetCeosField( &record, 141, "B4", &bottom );
    GetCeosField( &record, 153, "B4", &right );

    /* Check for all zeros (no data) */
    if ( top == 0 && left == 0 && bottom == 0 && right == 0 )
    {
	return;
    }

    /* Calculate offset and scale */
    /* N.B. lat/long values are in millionths of a degree */
    proj->XOff = (double)left * 1.0e-6;
    proj->YOff = (double)top * 1.0e-6;
    proj->XSize = ( (double)right * 1.0e-6 - proj->XOff ) / volume->ImageDesc.PixelsPerLine;
    proj->YSize = ( (double)bottom * 1.0e-6 - proj->YOff ) / volume->ImageDesc.Lines;
}
