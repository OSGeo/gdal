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
 * Revision 1.2  1999/11/18 19:02:19  warmerda
 * expanded tabs
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
        /* must be geographic? */
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

    else if( EQUAL(pszProjection,SRS_PT_MILLER_CYLINDRICAL) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=mill +lat_0=%.9f +lon_0=%.9f +x_0=%.3f +y_0=%.3f ",
                 GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0),
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
    sprintf( szProj4+strlen(szProj4), "+a=%.3f +b=%.3f ",
             GetSemiMajor(), GetSemiMinor() );

/* -------------------------------------------------------------------- */
/*      Handle linear units.                                            */
/* -------------------------------------------------------------------- */
    if( GetLinearUnits() != 1.0 )
        sprintf( szProj4+strlen(szProj4), "+to_meter=+%.10f ",
                 GetLinearUnits() );

    *ppszProj4 = CPLStrdup( szProj4 );

    return OGRERR_NONE;
}

