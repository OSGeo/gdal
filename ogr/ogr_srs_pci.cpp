/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  OGRSpatialReference translation to/from PCI georeferencing
 *           information.
 * Author:   Andrey Kiselev, dron@remotesensing.org
 *
 ******************************************************************************
 * Copyright (c) 2003, Andrey Kiselev <dron@remotesensing.org>
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
 * Revision 1.1  2003/08/31 14:49:37  dron
 * New.
 *
 *
 */

#include "ogr_spatialref.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                         OSRImportFromPCI()                           */
/************************************************************************/

OGRErr OSRImportFromPCI( OGRSpatialReferenceH hSRS, const char *pszProj,
                         const char *pszUnits, double *padfPrjParams )

{
    return ((OGRSpatialReference *) hSRS)->importFromPCI( pszProj,
                                                          pszUnits,
                                                          padfPrjParams );
}

/************************************************************************/
/*                          importFromPCI()                             */
/************************************************************************/

/**
 * Import coordinate system from PCI projection definition.
 *
 * PCI software uses 16-character string to specify coordinate system
 * and datum/ellipsoid. You should supply at least this string to the
 * importFromPCI() function.
 *
 * This function is the equilvelent of the C function OSRImportFromPCI().
 *
 * @param pszProj NULL terminated string containing the definition. Looks
 * like "pppppppppppp Ennn" or "pppppppppppp Dnnn", where "pppppppppppp" is
 * a projection code, "Ennn" is an ellipsoid code, "Dnnn" --- a datum code.
 *
 * @param pszUnits Grid units code ("DEGREE" or "METRE"). If NULL "METRE" will
 * be used.
 *
 * @param padfPrjParams Array of 16 coordinate system parameters:
 *
 * [0]  Spheroid semi major axis
 * [1]  Spheroid semi minor axis
 * [2]  Reference Longitude
 * [3]  Reference Latitude
 * [4]  First Standard Parallel
 * [5]  Second Standard Parallel
 * [6]  False Easting
 * [7]  False Northing
 * [8]  Scale Factor
 * [9]  Height above sphere surface
 * [9]  Longitude of 1st point on center line
 * [10] Latitude of 1st point on center line
 * [11] Longitude of 2nd point on center line
 * [12] Latitude of 2nd point on center line
 * [13] Azimuth east of north for center line
 * [14] Landsat satellite number
 * [15] Landsat path number
 *
 * Particular projection uses different parameters, unused ones may be set to
 * zero. If NULL suppliet instead of array pointer default values will be
 * used (i.e., zeroes).
 *
 * @return OGRERR_NONE on success or an error code in case of failure. 
 */

OGRErr OGRSpatialReference::importFromPCI( const char *pszProj,
                                           const char *pszUnits,
                                           double *padfPrjParams )

{
    if( pszProj == NULL || strlen( pszProj ) < 16 )
        return OGRERR_CORRUPT_DATA;

/* -------------------------------------------------------------------- */
/*      Use safe defaults if projection parameters are not supplied.    */
/* -------------------------------------------------------------------- */
    int     bProjAllocated = FALSE;

    if( padfPrjParams == NULL )
    {
        int     i;
        padfPrjParams = (double *)CPLMalloc( 16 * sizeof(double) );
        if ( !padfPrjParams )
            return OGRERR_NOT_ENOUGH_MEMORY;
        for ( i = 0; i < 16; i++ )
            padfPrjParams[i] = 0.0;
        bProjAllocated = TRUE;
    }

    if( pszUnits == NULL )
        pszUnits = "METRE";

/* -------------------------------------------------------------------- */
/*      Operate on the basis of the projection name.                    */
/* -------------------------------------------------------------------- */
    if( EQUALN( pszProj, "LONG/LAT", 8 ) )
    {
    }
    
    else if( EQUALN( pszProj, "ACEA", 4 ) )
    {
        SetACEA( padfPrjParams[4], padfPrjParams[5],
                 padfPrjParams[3], padfPrjParams[2],
                 padfPrjParams[6], padfPrjParams[7] );
    }

    else if( EQUALN( pszProj, "AE", 2 ) )
    {
        SetAE( padfPrjParams[3], padfPrjParams[2],
               padfPrjParams[6], padfPrjParams[7] );
    }

    else if( EQUALN( pszProj, "EC", 2 ) )
    {
        SetEC( padfPrjParams[4], padfPrjParams[5],
               padfPrjParams[3], padfPrjParams[2],
               padfPrjParams[6], padfPrjParams[7] );
    }

    else if( EQUALN( pszProj, "ER", 2 ) )
    {
        SetEquirectangular( padfPrjParams[3], padfPrjParams[2],
                            padfPrjParams[6], padfPrjParams[7] );
    }

    else if( EQUALN( pszProj, "GNO", 3 ) )
    {
        SetGnomonic( padfPrjParams[3], padfPrjParams[2],
                     padfPrjParams[6], padfPrjParams[7] );
    }

    // FIXME: GVNP --- General Vertical Near- Side Perspective skipped

    else if( EQUALN( pszProj, "LAEA", 4 ) )
    {
        SetLAEA( padfPrjParams[3], padfPrjParams[2],
                 padfPrjParams[6], padfPrjParams[7] );
    }

    else if( EQUALN( pszProj, "LCC", 3 ) )
    {
        SetLCC( padfPrjParams[4], padfPrjParams[5],
                padfPrjParams[3], padfPrjParams[2],
                padfPrjParams[6], padfPrjParams[7] );
    }

    else if( EQUALN( pszProj, "MC", 2 ) )
    {
        SetMC( padfPrjParams[3], padfPrjParams[2],
               padfPrjParams[6], padfPrjParams[7] );
    }

    else if( EQUALN( pszProj, "MER", 3 ) )
    {
        SetMercator( padfPrjParams[3], padfPrjParams[2],
                     padfPrjParams[8],
                     padfPrjParams[6], padfPrjParams[7] );
    }

    else if( EQUALN( pszProj, "OG", 2 ) )
    {
        SetOrthographic( padfPrjParams[3], padfPrjParams[2],
                         padfPrjParams[6], padfPrjParams[7] );
    }

    // FIXME: OM --- Oblique Mercator skipped

    else if( EQUALN( pszProj, "PC", 2 ) )
    {
        SetPolyconic( padfPrjParams[3], padfPrjParams[2],
                      padfPrjParams[6], padfPrjParams[7] );
    }

    else if( EQUALN( pszProj, "PS", 2 ) )
    {
        SetPS( padfPrjParams[3], padfPrjParams[2],
               padfPrjParams[8],
               padfPrjParams[6], padfPrjParams[7] );
    }

    else if( EQUALN( pszProj, "ROB", 3 ) )
    {
        SetRobinson( padfPrjParams[2],
                     padfPrjParams[6], padfPrjParams[7] );
    }

    else if( EQUALN( pszProj, "SG", 2 ) )
    {
        SetStereographic( padfPrjParams[3], padfPrjParams[2],
                          padfPrjParams[8],
                          padfPrjParams[6], padfPrjParams[7] );
    }

    else if( EQUALN( pszProj, "SIN", 3 ) )
    {
        SetSinusoidal( padfPrjParams[2],
                       padfPrjParams[6], padfPrjParams[7] );
    }

    // FIXME: SOM --- Space Oblique Mercator skipped

    else if( EQUALN( pszProj, "SPCS", 4 ) )
    {
        int     iZone, bNAD83 = TRUE;

        iZone = CPLScanLong( (char *)pszProj + 5, 4 );

        if ( !EQUALN( pszProj + 12, "E008", 4 ) )
            bNAD83 = FALSE;
        
        SetStatePlane( iZone, bNAD83 );
    }

    else if( EQUALN( pszProj, "TM", 2 ) )
    {
        SetTM( padfPrjParams[3], padfPrjParams[2],
               padfPrjParams[8],
               padfPrjParams[6], padfPrjParams[7] );
    }

    else if( EQUALN( pszProj, "UTM", 3 ) )
    {
        int     iZone, bNorth = TRUE;

        iZone = CPLScanLong( (char *)pszProj + 5, 4 );;
        if ( iZone < 0 )
        {
            iZone = -iZone;
            bNorth = FALSE;
        }
        
        SetUTM( iZone, bNorth );
    }

    else if( EQUALN( pszProj, "VDG", 3 ) )
    {
        SetVDG( padfPrjParams[2],
                padfPrjParams[6], padfPrjParams[7] );
    }

    else
    {
        CPLDebug( "OGR_ESRI", "Unsupported projection: %s", pszProj );
        SetLocalCS( pszProj );
    }

/* -------------------------------------------------------------------- */
/*      Try to translate the datum/spheroid.                            */
/* -------------------------------------------------------------------- */

    if ( !IsLocal() )
    {

        // Clarke, 1866 (NAD1927)
        if( EQUALN( pszProj + 12, "E000", 4 ) )
        {
            OGRSpatialReference oGCS;
            oGCS.importFromEPSG( 4267 );
            CopyGeogCSFrom( &oGCS );
        }

        // Clarke, 1880
        else if( EQUALN( pszProj + 12, "E001", 4 ) )
        {
            OGRSpatialReference oGCS;
            oGCS.importFromEPSG( 4034 );
            CopyGeogCSFrom( &oGCS );
        }

        // Bessel, 1841
        else if( EQUALN( pszProj + 12, "E002", 4 ) )
        {
            OGRSpatialReference oGCS;
            oGCS.importFromEPSG( 4004 );
            CopyGeogCSFrom( &oGCS );
        }

        // FIXME: New International, 1967 --- skipped

        // International, 1924 (Hayford, 1909)
        else if( EQUALN( pszProj + 12, "E004", 4 ) )
        {
            OGRSpatialReference oGCS;
            oGCS.importFromEPSG( 4022 );
            CopyGeogCSFrom( &oGCS );
        }

        // WGS, 1972
        else if( EQUALN( pszProj + 12, "E005", 4 ) )
        {
            OGRSpatialReference oGCS;
            oGCS.importFromEPSG( 4322 );
            CopyGeogCSFrom( &oGCS );
        }

        // FIXME: Everest, 1830 --- skipped

        // FIXME: WGS, 1966 --- skipped

        // GRS, 1980 (NAD1983)
        else if( EQUALN( pszProj + 12, "E008", 4 ) )
        {
            OGRSpatialReference oGCS;
            oGCS.importFromEPSG( 4269 );
            CopyGeogCSFrom( &oGCS );
        }

        // Airy, 1830
        else if( EQUALN( pszProj + 12, "E009", 4 ) )
        {
            OGRSpatialReference oGCS;
            oGCS.importFromEPSG( 4001 );
            CopyGeogCSFrom( &oGCS );
        }

        // FIXME: Modified Everest --- skipped

        // Modified Airy
        else if( EQUALN( pszProj + 12, "E011", 4 ) )
        {
            OGRSpatialReference oGCS;
            oGCS.importFromEPSG( 4002 );
            CopyGeogCSFrom( &oGCS );
        }

        // WGS, 1984 (GPS)
        else if( EQUALN( pszProj + 12, "E012", 4 ) )
        {
            OGRSpatialReference oGCS;
            oGCS.importFromEPSG( 4326 );
            CopyGeogCSFrom( &oGCS );
        }

        // FIXME: Southeast Asia --- skipped

        // Australian National, 1965
        else if( EQUALN( pszProj + 12, "E014", 4 ) )
        {
            OGRSpatialReference oGCS;
            oGCS.importFromEPSG( 4003 );
            CopyGeogCSFrom( &oGCS );
        }

        // Krassovsky, 1940
        else if( EQUALN( pszProj + 12, "E015", 4 ) )
        {
            OGRSpatialReference oGCS;
            oGCS.importFromEPSG( 4024 );
            CopyGeogCSFrom( &oGCS );
        }

        // FIXME: Hough --- skipped

        // FIXME: Mercury, 1960 --- skipped

        // FIXME: Modified Mercury, 1968 --- skipped

        // FIXME: Sphere, rad 6370997 m (normal sphere) --- skipped
        
        // FIXME: Bessel, 1841 (Japan by Law) --- skipped

        // FIXME: D-PAF (Orbits) --- skipped

        // Bessel, 1841 (Namibia)
        else if( EQUALN( pszProj + 12, "E900", 4 ) )
        {
            OGRSpatialReference oGCS;
            oGCS.importFromEPSG( 4006 );
            CopyGeogCSFrom( &oGCS );
        }

        // FIXME: Everest, 1956 --- skipped

        // FIXME: Everest, 1969 --- skipped

        // FIXME: Everest (Sabah & Sarawak) --- skipped

        // Helmert, 1906
        else if( EQUALN( pszProj + 12, "E904", 4 ) )
        {
            OGRSpatialReference oGCS;
            oGCS.importFromEPSG( 4020 );
            CopyGeogCSFrom( &oGCS );
        }

        // FIXME: SGS 85 --- skipped

        // FIXME: WGS 60 --- skipped

        // South American, 1969
        else if( EQUALN( pszProj + 12, "E907", 4 ) )
        {
            OGRSpatialReference oGCS;
            oGCS.importFromEPSG( 4291 );
            CopyGeogCSFrom( &oGCS );
        }

        // ATS77
        else if( EQUALN( pszProj + 12, "E910", 4 ) )
        {
            OGRSpatialReference oGCS;
            oGCS.importFromEPSG( 4122 );
            CopyGeogCSFrom( &oGCS );
        }

        else if( EQUALN( pszProj + 12, "E999", 4 ) )
        {
            double      dfInvFlattening;
            OGRSpatialReference oGCS;

            if( padfPrjParams[0] == padfPrjParams[1] )
            {
                dfInvFlattening = 0.0;
            }
            else
            {
                dfInvFlattening =
                    padfPrjParams[0] / (padfPrjParams[0] - padfPrjParams[1]);
            }

            oGCS.SetGeogCS( "Unknown datum based upon the custom spheroid",
                            "Not specified (based on custom spheroid)",
                            "Custom spheroid",
                            padfPrjParams[0], dfInvFlattening,
                            NULL, 0, NULL, 0 );
            CopyGeogCSFrom( &oGCS );
        }

        else
        {
            // If we don't know, default to WGS84 so there is something there.
            SetWellKnownGeogCS( "WGS84" );
        }
    }

/* -------------------------------------------------------------------- */
/*      Grid units translation                                          */
/* -------------------------------------------------------------------- */
    if( IsLocal() || IsProjected() )
    {
        if( EQUAL( pszUnits, "METRE" ) )
            SetLinearUnits( SRS_UL_METER, 1.0 );
        else if( EQUAL( pszUnits, "DEGREE" ) )
            SetAngularUnits( SRS_UA_DEGREE, atof(SRS_UA_DEGREE_CONV) );
        else
            SetLinearUnits( SRS_UL_METER, 1.0 );
    }

    if ( bProjAllocated && padfPrjParams )
        CPLFree( padfPrjParams );

    return OGRERR_NONE;
}

