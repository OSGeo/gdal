/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  OGRSpatialReference interface to PROJ.4.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 1999,  Les Technologies SoftMap Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ******************************************************************************
 *
 * $Log$
 * Revision 1.6  1999/12/22 15:39:43  warmerda
 * fix to differentiate WGS variants
 *
 * Revision 1.5  1999/12/13 16:29:59  warmerda
 * Added improved units, and ellipse support.
 *
 * Revision 1.4  1999/12/08 16:34:05  warmerda
 * added five or six more projections
 *
 * Revision 1.1  1999/07/29 17:29:15  warmerda
 * New
 *
 */

#include "ogr_spatialref.h"
#include "ogr_p.h"

/************************************************************************/
/*                           exportToProj4()                            */
/************************************************************************/

OGRErr OGRSpatialReference::exportToProj4( char ** ppszProj4 )

{
    char        szProj4[512];
    const char *pszProjection = GetAttrValue("PROJECTION");

    szProj4[0] = '\0';

/* ==================================================================== */
/*      Handle the projection definition.                               */
/* ==================================================================== */

    if( pszProjection == NULL )
    {
        sprintf( szProj4+strlen(szProj4), "+proj=longlat " );
    }
    else if( EQUAL(pszProjection,SRS_PT_CYLINDRICAL_EQUAL_AREA) )
    {
        sprintf( szProj4+strlen(szProj4),
           "+proj=cea +lon_0=%.9f +lat_ts=%.9f +x_0=%.3f +y_0=%.3f ",
                 GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetProjParm(SRS_PP_STANDARD_PARALLEL_1,0.0),
                 GetProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_TRANSVERSE_MERCATOR) )
    {
        sprintf( szProj4+strlen(szProj4),
           "+proj=tmerc +lat_0=%.9f +lon_0=%.9f +k=%f +x_0=%.3f +y_0=%.3f ",
                 GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0),
                 GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetProjParm(SRS_PP_SCALE_FACTOR,1.0),
                 GetProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_MERCATOR_1SP) )
    {
        sprintf( szProj4+strlen(szProj4),
           "+proj=merc +lat_ts=%.9f +lon_0=%.9f +k=%f +x_0=%.3f +y_0=%.3f ",
                 GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0),
                 GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetProjParm(SRS_PP_SCALE_FACTOR,1.0),
                 GetProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_OBLIQUE_STEREOGRAPHIC) )
    {
        sprintf( szProj4+strlen(szProj4),
           "+proj=stere +lat_0=%.9f +lon_0=%.9f +k=%f +x_0=%.3f +y_0=%.3f ",
                 GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0),
                 GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetProjParm(SRS_PP_SCALE_FACTOR,1.0),
                 GetProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_STEREOGRAPHIC) )
    {
        sprintf( szProj4+strlen(szProj4),
           "+proj=stere +lat_0=%.9f +lon_0=%.9f +x_0=%.3f +y_0=%.3f ",
                 GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0),
                 GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_POLAR_STEREOGRAPHIC) )
    {
        /* note we are ignore the scale factory handled by SetPS() */
        
        sprintf( szProj4+strlen(szProj4),
                 "+proj=stere +lat_0=%.9f +lon_0=%.9f +x_0=%.3f +y_0=%.3f ",
                 GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0),
                 GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_EQUIRECTANGULAR) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=eqc +lat_ts=%.9f +lon_0=%.9f +x_0=%.3f +y_0=%.3f ",
                 GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0),
                 GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_GNOMONIC) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=gnom +lat_0=%.9f +lon_0=%.9f +x_0=%.3f +y_0=%.3f ",
                 GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0),
                 GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_ORTHOGRAPHIC) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=ortho +lat_0=%.9f +lon_0=%.9f +x_0=%.3f +y_0=%.3f ",
                 GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0),
                 GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_LAMBERT_AZIMUTHAL_EQUAL_AREA) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=laea +lat_0=%.9f +lon_0=%.9f +x_0=%.3f +y_0=%.3f ",
                 GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0),
                 GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_AZIMUTHAL_EQUIDISTANT) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=aeqd +lat_0=%.9f +lon_0=%.9f +x_0=%.3f +y_0=%.3f ",
                 GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0),
                 GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_EQUIDISTANT_CONIC) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=eqdc +lat_0=%.9f +lon_0=%.9f +lat_1=%.9f +lat_2=%.9f"
                 " +x_0=%.3f +y_0=%.3f ",
                 GetProjParm(SRS_PP_LATITUDE_OF_CENTER,0.0),
                 GetProjParm(SRS_PP_LONGITUDE_OF_CENTER,0.0),
                 GetProjParm(SRS_PP_STANDARD_PARALLEL_1,0.0),
                 GetProjParm(SRS_PP_STANDARD_PARALLEL_2,0.0),
                 GetProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_MILLER_CYLINDRICAL) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=mill +lat_0=%.9f +lon_0=%.9f +x_0=%.3f +y_0=%.3f ",
                 GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0),
                 GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_MOLLWEIDE) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=moll +lon_0=%.9f +x_0=%.3f +y_0=%.3f ",
                 GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_ECKERT_IV) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=eck4 +lon_0=%.9f +x_0=%.3f +y_0=%.3f ",
                 GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_ECKERT_VI) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=eck6 +lon_0=%.9f +x_0=%.3f +y_0=%.3f ",
                 GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_POLYCONIC) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=poly +lat_0=%.9f +lon_0=%.9f +x_0=%.3f +y_0=%.3f ",
                 GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0),
                 GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_ALBERS_CONIC_EQUAL_AREA) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=aea +lat_1=%.9f +lat_2=%.9f +lat_0=%.9f +lon_0=%.9f"
                 " +x_0=%.3f +y_0=%.3f ",
                 GetProjParm(SRS_PP_STANDARD_PARALLEL_1,0.0),
                 GetProjParm(SRS_PP_STANDARD_PARALLEL_2,0.0),
                 GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0),
                 GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_ROBINSON) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=robin +lon_0=%.9f +x_0=%.3f +y_0=%.3f ",
                 GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_VANDERGRINTEN) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=vandg +lon_0=%.9f +x_0=%.3f +y_0=%.3f ",
                 GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_SINUSOIDAL) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=sinu +lon_0=%.9f +x_0=%.3f +y_0=%.3f ",
                 GetProjParm(SRS_PP_LONGITUDE_OF_CENTER,0.0),
                 GetProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_GALL_STEREOGRAPHIC) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=gall +lon_0=%.9f +x_0=%.3f +y_0=%.3f ",
                 GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=lcc +lat_1=%.9f +lat_2=%.9f +lat_0=%.9f +lon_0=%.9f"
                 " +x_0=%.3f +y_0=%.3f ",
                 GetProjParm(SRS_PP_STANDARD_PARALLEL_1,0.0),
                 GetProjParm(SRS_PP_STANDARD_PARALLEL_2,0.0),
                 GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0),
                 GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }
    
    else if( EQUAL(pszProjection,SRS_PT_HOTINE_OBLIQUE_MERCATOR) )
    {
        /* not clear how ProjParm[3] - angle from rectified to skewed grid -
           should be applied ... see the +not_rot flag for PROJ.4.
           Just ignoring for now. */

        sprintf( szProj4+strlen(szProj4),
                 "+proj=omerc +lat_0=%.9f +lonc=%.9f +alpha=%.9f"
                 " +k=%.9f +x_0=%.3f +y_0=%.3f ",
                 GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0),
                 GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetProjParm(SRS_PP_AZIMUTH,0.0),
                 GetProjParm(SRS_PP_SCALE_FACTOR,1.0),
                 GetProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

/* -------------------------------------------------------------------- */
/*      Handle earth model.  For now we just always emit the user       */
/*      defined ellipsoid parameters.                                   */
/* -------------------------------------------------------------------- */
    double	dfSemiMajor = GetSemiMajor();
    double	dfInvFlattening = GetInvFlattening();
    const char 	*pszPROJ4Ellipse = NULL;
    const char  *pszDatum = GetAttrValue("DATUM");

    if( ABS(dfSemiMajor-6378249.145) < 0.01
        && ABS(dfInvFlattening-293.465) < 0.0001 )
    {
        pszPROJ4Ellipse = "clrk80";	/* Clark 1880 */
    }
    else if( ABS(dfSemiMajor-6378245.0) < 0.01
             && ABS(dfInvFlattening-298.3) < 0.0001 )
    {
        pszPROJ4Ellipse = "krass";	/* Krassovsky */
    }
    else if( ABS(dfSemiMajor-6378388.0) < 0.01
             && ABS(dfInvFlattening-297.0) < 0.0001 )
    {
        pszPROJ4Ellipse = "intl"; 	/* International 1924 */
    }
    else if( ABS(dfSemiMajor-6378160.0) < 0.01
             && ABS(dfInvFlattening-298.25) < 0.0001 )
    {
        pszPROJ4Ellipse = "aust_SA"; 	/* Australian */
    }
    else if( ABS(dfSemiMajor-6377397.155) < 0.01
             && ABS(dfInvFlattening-299.1528128) < 0.0001 )
    {
        pszPROJ4Ellipse = "bessel";	/* Bessel 1841 */
    }
    else if( ABS(dfSemiMajor-6377483.865) < 0.01
             && ABS(dfInvFlattening-299.1528128) < 0.0001 )
    {
        pszPROJ4Ellipse = "bess_nam";	/* Bessel 1841 (Namibia / Schwarzeck)*/
    }
    else if( ABS(dfSemiMajor-6378160.0) < 0.01
             && ABS(dfInvFlattening-298.247167427) < 0.0001 )
    {
        pszPROJ4Ellipse = "GRS67";	/* GRS 1967 */
    }
    else if( ABS(dfSemiMajor-6378137) < 0.01
             && ABS(dfInvFlattening-298.257222101) < 0.000001 )
    {
        pszPROJ4Ellipse = "GRS80";	/* GRS 1980 */
    }
    else if( ABS(dfSemiMajor-6378206.4) < 0.01
             && ABS(dfInvFlattening-294.9786982) < 0.0001 )
    {
        pszPROJ4Ellipse = "clrk66";	/* Clarke 1866 */
    }
    else if( ABS(dfSemiMajor-6378206.4) < 0.01
             && ABS(dfInvFlattening-294.9786982) < 0.0001 )
    {
        pszPROJ4Ellipse = "mod_airy";	/* Modified Airy */
    }
    else if( ABS(dfSemiMajor-6377563.396) < 0.01
             && ABS(dfInvFlattening-299.3249646) < 0.0001 )
    {
        pszPROJ4Ellipse = "airy";	/* Modified Airy */
    }
    else if( ABS(dfSemiMajor-6378200) < 0.01
             && ABS(dfInvFlattening-298.3) < 0.0001 )
    {
        pszPROJ4Ellipse = "helmert";	/* Helmert 1906 */
    }
    else if( ABS(dfSemiMajor-6378155) < 0.01
             && ABS(dfInvFlattening-298.3) < 0.0001 )
    {
        pszPROJ4Ellipse = "fschr60m";	/* Modified Fischer 1960 */
    }
    else if( ABS(dfSemiMajor-6377298.556) < 0.01
             && ABS(dfInvFlattening-300.8017) < 0.0001 )
    {
        pszPROJ4Ellipse = "evrstSS";	/* Everest (Sabah & Sarawak) */
    }
    else if( ABS(dfSemiMajor-6378165.0) < 0.01
             && ABS(dfInvFlattening-298.3) < 0.0001 )
    {
        pszPROJ4Ellipse = "WGS60";	
    }
    else if( ABS(dfSemiMajor-6378145.0) < 0.01
             && ABS(dfInvFlattening-298.25) < 0.0001 )
    {
        pszPROJ4Ellipse = "WGS66";	
    }
    else if( ABS(dfSemiMajor-6378135.0) < 0.01
             && ABS(dfInvFlattening-298.26) < 0.0001 )
    {
        pszPROJ4Ellipse = "WGS72";	
    }
    else if( ABS(dfSemiMajor-6378137.0) < 0.01
             && ABS(dfInvFlattening-298.257223563) < 0.000001 )
    {
        pszPROJ4Ellipse = "WGS84";
    }
    else if( EQUAL(pszDatum,"North_Americian_Datum_1927") )
    {
        pszPROJ4Ellipse = "clrk66:+datum=nad27"; /* NAD 27 */
    }
    else if( EQUAL(pszDatum,"North_Americian_Datum_1983") )
    {
        pszPROJ4Ellipse = "GRS80:+datum=nad83";	/* NAD 83 */
    }
    
    if( pszPROJ4Ellipse == NULL )
        sprintf( szProj4+strlen(szProj4), "+a=%.3f +b=%.3f ",
                 GetSemiMajor(), GetSemiMinor() );
    else
        sprintf( szProj4+strlen(szProj4), "+ellps=%s ",
                 pszPROJ4Ellipse );
    
/* -------------------------------------------------------------------- */
/*      Handle linear units.                                            */
/* -------------------------------------------------------------------- */
    const char	*pszPROJ4Units;
    char  	*pszLinearUnits = NULL;
    double	dfLinearConv;

    dfLinearConv = GetLinearUnits( &pszLinearUnits );
        
    if( strstr(szProj4,"longlat") != NULL )
        pszPROJ4Units = NULL;
    
    else if( dfLinearConv == 1.0 )
        pszPROJ4Units = "m";

    else if( dfLinearConv == 1000.0 )
        pszPROJ4Units = "km";
    
    else if( dfLinearConv == 0.0254 )
        pszPROJ4Units = "in";
    
    else if( EQUAL(pszLinearUnits,SRS_UL_FOOT) )
        pszPROJ4Units = "ft";
    
    else if( EQUAL(pszLinearUnits,"IYARD") || dfLinearConv == 0.9144 )
        pszPROJ4Units = "yd";
    
    else if( dfLinearConv == 0.001 )
        pszPROJ4Units = "mm";
    
    else if( dfLinearConv == 0.01 )
        pszPROJ4Units = "cm";

    else if( EQUAL(pszLinearUnits,SRS_UL_US_FOOT) )
        pszPROJ4Units = "us-ft";

    else if( EQUAL(pszLinearUnits,SRS_UL_NAUTICAL_MILE) )
        pszPROJ4Units = "kmi";

    else if( EQUAL(pszLinearUnits,"Mile") 
             || EQUAL(pszLinearUnits,"IMILE") )
        pszPROJ4Units = "mi";

    else
    {
        sprintf( szProj4+strlen(szProj4), "+to_meter=+%.10f ",
                 dfLinearConv );
    }

    if( pszPROJ4Units != NULL )
        sprintf( szProj4+strlen(szProj4), "+units=%s ",
                 pszPROJ4Units );

    *ppszProj4 = CPLStrdup( szProj4 );

    return OGRERR_NONE;
}

