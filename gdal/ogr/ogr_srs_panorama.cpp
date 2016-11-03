/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  OGRSpatialReference translation to/from "Panorama" GIS
 *           georeferencing information (also know as GIS "Integration").
 * Author:   Andrey Kiselev, dron@ak4719.spb.edu
 *
 ******************************************************************************
 * Copyright (c) 2005, Andrey Kiselev <dron@ak4719.spb.edu>
 * Copyright (c) 2008-2012, Even Rouault <even dot rouault at mines-paris dot org>
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
 ****************************************************************************/

#include "ogr_spatialref.h"
#include "cpl_conv.h"
#include "cpl_csv.h"
#include "ogr_p.h"

CPL_CVSID("$Id$");

static const double TO_DEGREES = 57.2957795130823208766;
static const double TO_RADIANS = 0.017453292519943295769;

// XXX: this macro computes zone number from the central meridian parameter.
// Note, that "Panorama" parameters are set in radians.
// In degrees it means formula:
//
//              zone = (central_meridian + 3) / 6
//
static int TO_ZONE( double x )
{
  return
      static_cast<int>((x + 0.05235987755982989) / 0.1047197551196597 + 0.5);
}

/************************************************************************/
/*  "Panorama" projection codes.                                        */
/************************************************************************/

static const long PAN_PROJ_NONE   = -1L;
static const long PAN_PROJ_TM     = 1L;   // Gauss-Kruger (Transverse Mercator)
static const long PAN_PROJ_LCC    = 2L;   // Lambert Conformal Conic 2SP
static const long PAN_PROJ_STEREO = 5L;   // Stereographic
static const long PAN_PROJ_AE     = 6L;   // Azimuthal Equidistant (Postel)
static const long PAN_PROJ_MERCAT = 8L;   // Mercator
static const long PAN_PROJ_POLYC  = 10L;  // Polyconic
static const long PAN_PROJ_PS     = 13L;  // Polar Stereographic
static const long PAN_PROJ_GNOMON = 15L;  // Gnomonic
static const long PAN_PROJ_UTM    = 17L;  // Universal Transverse Mercator (UTM)
static const long PAN_PROJ_WAG1   = 18L;  // Wagner I (Kavraisky VI)
static const long PAN_PROJ_MOLL   = 19L;  // Mollweide
static const long PAN_PROJ_EC     = 20L;  // Equidistant Conic
static const long PAN_PROJ_LAEA   = 24L;  // Lambert Azimuthal Equal Area
static const long PAN_PROJ_EQC    = 27L;  // Equirectangular
static const long PAN_PROJ_CEA    = 28L;  // Cylindrical Equal Area (Lambert)
static const long PAN_PROJ_IMWP   = 29L;  // International Map of the World Polyconic
static const long PAN_PROJ_MILLER = 34L;  // Miller
/************************************************************************/
/*  "Panorama" datum codes.                                             */
/************************************************************************/

static const long PAN_DATUM_NONE      = -1L;
static const long PAN_DATUM_PULKOVO42 = 1L;  // Pulkovo 1942
static const long PAN_DATUM_WGS84     = 2L;  // WGS84

/************************************************************************/
/*  "Panorama" ellipsoid codes.                                         */
/************************************************************************/

static const long PAN_ELLIPSOID_NONE        = -1L;
static const long PAN_ELLIPSOID_KRASSOVSKY  = 1L;  // Krassovsky, 1940
// static const long PAN_ELLIPSOID_WGS72       = 2L;  // WGS, 1972
// static const long PAN_ELLIPSOID_INT1924     = 3L;  // International, 1924 (Hayford, 1909)
// static const long PAN_ELLIPSOID_CLARCKE1880 = 4L;  // Clarke, 1880
// static const long PAN_ELLIPSOID_CLARCKE1866 = 5L;  // Clarke, 1866 (NAD1927)
// static const long PAN_ELLIPSOID_EVEREST1830 = 6L;  // Everest, 1830
// static const long PAN_ELLIPSOID_BESSEL1841  = 7L;  // Bessel, 1841
// static const long PAN_ELLIPSOID_AIRY1830    = 8L;  // Airy, 1830
static const long PAN_ELLIPSOID_WGS84       = 9L;  // WGS, 1984 (GPS)

/************************************************************************/
/*  Correspondence between "Panorama" and EPSG datum codes.             */
/************************************************************************/

static const int aoDatums[] =
{
    0,
    4284,   // Pulkovo, 1942
    4326,   // WGS, 1984,
    4277,   // OSGB 1936 (British National Grid)
    0,
    0,
    0,
    0,
    0,
    4200    // Pulkovo, 1995
};

#define NUMBER_OF_DATUMS        (long)(sizeof(aoDatums)/sizeof(aoDatums[0]))

/************************************************************************/
/*  Correspondence between "Panorama" and EPSG ellipsoid codes.         */
/************************************************************************/

static const int aoEllips[] =
{
    0,
    7024,   // Krassovsky, 1940
    7043,   // WGS, 1972
    7022,   // International, 1924 (Hayford, 1909)
    7034,   // Clarke, 1880
    7008,   // Clarke, 1866 (NAD1927)
    7015,   // Everest, 1830
    7004,   // Bessel, 1841
    7001,   // Airy, 1830
    7030,   // WGS, 1984 (GPS)
    0,      // FIXME: PZ90.02
    7019,   // GRS, 1980 (NAD1983)
    7022,   // International, 1924 (Hayford, 1909) XXX?
    7036,   // South American, 1969
    7021,   // Indonesian, 1974
    7020,   // Helmert 1906
    0,      // FIXME: Fisher 1960
    0,      // FIXME: Fisher 1968
    0,      // FIXME: Haff 1960
    7042,   // Everest, 1830
    7003   // Australian National, 1965
};

static const int NUMBER_OF_ELLIPSOIDS =
    static_cast<int>(sizeof(aoEllips)/sizeof(aoEllips[0]));

/************************************************************************/
/*                        OSRImportFromPanorama()                       */
/************************************************************************/

/** Import coordinate system from "Panorama" GIS projection definition.
 *
 * See OGRSpatialReference::importFromPanorama()
 */

OGRErr OSRImportFromPanorama( OGRSpatialReferenceH hSRS,
                              long iProjSys, long iDatum, long iEllips,
                              double *padfPrjParams )

{
    VALIDATE_POINTER1( hSRS, "OSRImportFromPanorama", OGRERR_FAILURE );

    return ((OGRSpatialReference *) hSRS)->importFromPanorama( iProjSys,
                                                               iDatum, iEllips,
                                                               padfPrjParams );
}

/************************************************************************/
/*                          importFromPanorama()                        */
/************************************************************************/

/**
 * Import coordinate system from "Panorama" GIS projection definition.
 *
 * This method will import projection definition in style, used by
 * "Panorama" GIS.
 *
 * This function is the equivalent of the C function OSRImportFromPanorama().
 *
 * @param iProjSys Input projection system code, used in GIS "Panorama".
 *
 *      <h4>Supported Projections</h4>
 * <pre>
 *      1:  Gauss-Kruger (Transverse Mercator)
 *      2:  Lambert Conformal Conic 2SP
 *      5:  Stereographic
 *      6:  Azimuthal Equidistant (Postel)
 *      8:  Mercator
 *      10: Polyconic
 *      13: Polar Stereographic
 *      15: Gnomonic
 *      17: Universal Transverse Mercator (UTM)
 *      18: Wagner I (Kavraisky VI)
 *      19: Mollweide
 *      20: Equidistant Conic
 *      24: Lambert Azimuthal Equal Area
 *      27: Equirectangular
 *      28: Cylindrical Equal Area (Lambert)
 *      29: International Map of the World Polyconic
 * </pre>
 *
 * @param iDatum Input coordinate system.
 *
 *      <h4>Supported Datums</h4>
 * <pre>
 *       1: Pulkovo, 1942
 *       2: WGS, 1984
 *       3: OSGB 1936 (British National Grid)
 *       9: Pulkovo, 1995
 * </pre>
 *
 * @param iEllips Input spheroid.
 *
 *      <h4>Supported Spheroids</h4>
 * <pre>
 *       1: Krassovsky, 1940
 *       2: WGS, 1972
 *       3: International, 1924 (Hayford, 1909)
 *       4: Clarke, 1880
 *       5: Clarke, 1866 (NAD1927)
 *       6: Everest, 1830
 *       7: Bessel, 1841
 *       8: Airy, 1830
 *       9: WGS, 1984 (GPS)
 * </pre>
 *
 * @param padfPrjParams Array of 8 coordinate system parameters:
 *
 * <pre>
 *      [0]  Latitude of the first standard parallel (radians)
 *      [1]  Latitude of the second standard parallel (radians)
 *      [2]  Latitude of center of projection (radians)
 *      [3]  Longitude of center of projection (radians)
 *      [4]  Scaling factor
 *      [5]  False Easting
 *      [6]  False Northing
 *      [7]  Zone number
 * </pre>
 *
 * Particular projection uses different parameters, unused ones may be set to
 * zero. If NULL supplied instead of array pointer default values will be used
 * (i.e., zeroes).
 *
 * @return OGRERR_NONE on success or an error code in case of failure.
 */

OGRErr OGRSpatialReference::importFromPanorama( long iProjSys, long iDatum,
                                                long iEllips,
                                                double *padfPrjParams )

{
    Clear();

/* -------------------------------------------------------------------- */
/*      Use safe defaults if projection parameters are not supplied.    */
/* -------------------------------------------------------------------- */
    int bProjAllocated = false;

    if( padfPrjParams == NULL )
    {
        padfPrjParams = (double *)CPLMalloc( 8 * sizeof(double) );
        if( !padfPrjParams )
            return OGRERR_NOT_ENOUGH_MEMORY;
        for( int i = 0; i < 7; i++ )
            padfPrjParams[i] = 0.0;
        bProjAllocated = true;
    }

/* -------------------------------------------------------------------- */
/*      Operate on the basis of the projection code.                    */
/* -------------------------------------------------------------------- */
    switch( iProjSys )
    {
        case PAN_PROJ_NONE:
            break;

        case PAN_PROJ_UTM:
            {
                int nZone;

                if( padfPrjParams[7] == 0.0 )
                    nZone = TO_ZONE(padfPrjParams[3]);
                else
                    nZone = (int) padfPrjParams[7];

                // XXX: no way to determine south hemisphere. Always assume
                // northern hemisphere.
                SetUTM( nZone, TRUE );
            }
            break;

        case PAN_PROJ_WAG1:
            SetWagner( 1, 0.0,
                       padfPrjParams[5], padfPrjParams[6] );
            break;

        case PAN_PROJ_MERCAT:
            SetMercator( TO_DEGREES * padfPrjParams[0],
                         TO_DEGREES * padfPrjParams[3],
                         padfPrjParams[4],
                         padfPrjParams[5], padfPrjParams[6] );
            break;

        case PAN_PROJ_PS:
            SetPS( TO_DEGREES * padfPrjParams[2],
                   TO_DEGREES * padfPrjParams[3],
                   padfPrjParams[4],
                   padfPrjParams[5], padfPrjParams[6] );
            break;

        case PAN_PROJ_POLYC:
            SetPolyconic( TO_DEGREES * padfPrjParams[2],
                          TO_DEGREES * padfPrjParams[3],
                          padfPrjParams[5], padfPrjParams[6] );
            break;

        case PAN_PROJ_EC:
            SetEC( TO_DEGREES * padfPrjParams[0],
                   TO_DEGREES * padfPrjParams[1],
                   TO_DEGREES * padfPrjParams[2],
                   TO_DEGREES * padfPrjParams[3],
                   padfPrjParams[5], padfPrjParams[6] );
            break;

        case PAN_PROJ_LCC:
            SetLCC( TO_DEGREES * padfPrjParams[0],
                    TO_DEGREES * padfPrjParams[1],
                    TO_DEGREES * padfPrjParams[2],
                    TO_DEGREES * padfPrjParams[3],
                    padfPrjParams[5], padfPrjParams[6] );
            break;

        case PAN_PROJ_TM:
            {
                // XXX: we need zone number to compute false easting
                // parameter, because usually it is not contained in the
                // "Panorama" projection definition.
                // FIXME: what to do with negative values?
                int nZone = 0;
                double dfCenterLong = 0.0;

                if( padfPrjParams[7] == 0.0 )
                {
                    nZone = TO_ZONE(padfPrjParams[3]);
                    dfCenterLong = TO_DEGREES * padfPrjParams[3];
                }
                else
                {
                    nZone = static_cast<int>(padfPrjParams[7]);
                    dfCenterLong = 6.0 * nZone - 3.0;
                }

                padfPrjParams[5] = nZone * 1000000.0 + 500000.0;
                padfPrjParams[4] = 1.0;
                SetTM( TO_DEGREES * padfPrjParams[2],
                       dfCenterLong,
                       padfPrjParams[4],
                       padfPrjParams[5], padfPrjParams[6] );
            }
            break;

        case PAN_PROJ_STEREO:
            SetStereographic( TO_DEGREES * padfPrjParams[2],
                              TO_DEGREES * padfPrjParams[3],
                              padfPrjParams[4],
                              padfPrjParams[5], padfPrjParams[6] );
            break;

        case PAN_PROJ_AE:
            SetAE( TO_DEGREES * padfPrjParams[0],
                   TO_DEGREES * padfPrjParams[3],
                   padfPrjParams[5], padfPrjParams[6] );
            break;

        case PAN_PROJ_GNOMON:
            SetGnomonic( TO_DEGREES * padfPrjParams[2],
                         TO_DEGREES * padfPrjParams[3],
                         padfPrjParams[5], padfPrjParams[6] );
            break;

        case PAN_PROJ_MOLL:
            SetMollweide( TO_DEGREES * padfPrjParams[3],
                          padfPrjParams[5], padfPrjParams[6] );
            break;

        case PAN_PROJ_LAEA:
            SetLAEA( TO_DEGREES * padfPrjParams[0],
                     TO_DEGREES * padfPrjParams[3],
                     padfPrjParams[5], padfPrjParams[6] );
            break;

        case PAN_PROJ_EQC:
            SetEquirectangular( TO_DEGREES * padfPrjParams[0],
                                TO_DEGREES * padfPrjParams[3],
                                padfPrjParams[5], padfPrjParams[6] );
            break;

        case PAN_PROJ_CEA:
            SetCEA( TO_DEGREES * padfPrjParams[0],
                    TO_DEGREES * padfPrjParams[3],
                    padfPrjParams[5], padfPrjParams[6] );
            break;

        case PAN_PROJ_IMWP:
            SetIWMPolyconic( TO_DEGREES * padfPrjParams[0],
                             TO_DEGREES * padfPrjParams[1],
                             TO_DEGREES * padfPrjParams[3],
                             padfPrjParams[5], padfPrjParams[6] );
            break;

        case PAN_PROJ_MILLER:
            SetMC(TO_DEGREES * padfPrjParams[5],
                TO_DEGREES * padfPrjParams[4],
                padfPrjParams[6], padfPrjParams[7]);
            break;

        default:
            CPLDebug( "OSR_Panorama", "Unsupported projection: %ld", iProjSys );
            SetLocalCS( CPLString().Printf("\"Panorama\" projection number %ld",
                                   iProjSys) );
            break;
    }

/* -------------------------------------------------------------------- */
/*      Try to translate the datum/spheroid.                            */
/* -------------------------------------------------------------------- */

    if( !IsLocal() )
    {
        if( iDatum > 0 && iDatum < NUMBER_OF_DATUMS && aoDatums[iDatum] )
        {
            OGRSpatialReference oGCS;
            oGCS.importFromEPSG( aoDatums[iDatum] );
            CopyGeogCSFrom( &oGCS );
        }
        else if( iEllips > 0
                 && iEllips < NUMBER_OF_ELLIPSOIDS
                 && aoEllips[iEllips] )
        {
            char *pszName = NULL;
            double dfSemiMajor = 0.0;
            double dfInvFlattening = 0.0;

            if( OSRGetEllipsoidInfo( aoEllips[iEllips], &pszName,
                                     &dfSemiMajor,
                                     &dfInvFlattening ) == OGRERR_NONE )
            {
                SetGeogCS(
                   CPLString().Printf(
                       "Unknown datum based upon the %s ellipsoid",
                       pszName ),
                   CPLString().Printf(
                       "Not specified (based on %s spheroid)", pszName ),
                   pszName, dfSemiMajor, dfInvFlattening,
                   NULL, 0.0, NULL, 0.0 );
                SetAuthority( "SPHEROID", "EPSG", aoEllips[iEllips] );
            }
            else
            {
                CPLError( CE_Warning, CPLE_AppDefined,
                          "Failed to lookup ellipsoid code %ld, likely due to "
                          "missing GDAL gcs.csv "
                          "file.  Falling back to use Pulkovo 42.", iEllips );
                SetWellKnownGeogCS( "EPSG:4284" );
            }

            if( pszName )
                CPLFree( pszName );
        }
        else
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "Wrong datum code %ld. Supported datums are 1--%ld "
                      "only.  Falling back to use Pulkovo 42.",
                      iDatum, NUMBER_OF_DATUMS - 1 );
            SetWellKnownGeogCS( "EPSG:4284" );
        }
    }

/* -------------------------------------------------------------------- */
/*      Grid units translation                                          */
/* -------------------------------------------------------------------- */
    if( IsLocal() || IsProjected() )
        SetLinearUnits( SRS_UL_METER, 1.0 );

    FixupOrdering();

    if( bProjAllocated && padfPrjParams )
        CPLFree( padfPrjParams );

    return OGRERR_NONE;
}

/************************************************************************/
/*                      OSRExportToPanorama()                           */
/************************************************************************/

/** Export coordinate system in "Panorama" GIS projection definition.
 *
 * See OGRSpatialReference::exportToPanorama()
 */

OGRErr OSRExportToPanorama( OGRSpatialReferenceH hSRS,
                            long *piProjSys, long *piDatum, long *piEllips,
                            long *piZone, double *padfPrjParams )

{
    VALIDATE_POINTER1( hSRS, "OSRExportToPanorama", OGRERR_FAILURE );
    VALIDATE_POINTER1( piProjSys, "OSRExportToPanorama", OGRERR_FAILURE );
    VALIDATE_POINTER1( piDatum, "OSRExportToPanorama", OGRERR_FAILURE );
    VALIDATE_POINTER1( piEllips, "OSRExportToPanorama", OGRERR_FAILURE );
    VALIDATE_POINTER1( padfPrjParams, "OSRExportToPanorama", OGRERR_FAILURE );

    return ((OGRSpatialReference *) hSRS)->exportToPanorama( piProjSys,
                                                             piDatum, piEllips,
                                                             piZone,
                                                             padfPrjParams );
}

/************************************************************************/
/*                           exportToPanorama()                         */
/************************************************************************/

/**
 * Export coordinate system in "Panorama" GIS projection definition.
 *
 * This method is the equivalent of the C function OSRExportToPanorama().
 *
 * @param piProjSys Pointer to variable, where the projection system code will
 * be returned.
 *
 * @param piDatum Pointer to variable, where the coordinate system code will
 * be returned.
 *
 * @param piEllips Pointer to variable, where the spheroid code will be
 * returned.
 *
 * @param piZone Pointer to variable, where the zone for UTM projection
 * system will be returned.
 *
 * @param padfPrjParams an existing 7 double buffer into which the
 * projection parameters will be placed. See importFromPanorama()
 * for the list of parameters.
 *
 * @return OGRERR_NONE on success or an error code on failure.
 */

OGRErr OGRSpatialReference::exportToPanorama( long *piProjSys, long *piDatum,
                                              long *piEllips, long *piZone,
                                              double *padfPrjParams ) const

{
    CPLAssert( padfPrjParams );

    const char  *pszProjection = GetAttrValue("PROJECTION");

/* -------------------------------------------------------------------- */
/*      Fill all projection parameters with zero.                       */
/* -------------------------------------------------------------------- */
    *piDatum = 0L;
    *piEllips = 0L;
    *piZone = 0L;
    for( int i = 0; i < 7; i++ )
        padfPrjParams[i] = 0.0;

/* ==================================================================== */
/*      Handle the projection definition.                               */
/* ==================================================================== */
    if( IsLocal() )
    {
        *piProjSys = PAN_PROJ_NONE;
    }
    else if( pszProjection == NULL )
    {
#ifdef DEBUG
        CPLDebug( "OSR_Panorama",
                  "Empty projection definition, considered as Geographic" );
#endif
        *piProjSys = PAN_PROJ_NONE;
    }
    else if( EQUAL(pszProjection, SRS_PT_MERCATOR_1SP) )
    {
        *piProjSys = PAN_PROJ_MERCAT;
        padfPrjParams[3] =
            TO_RADIANS * GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 );
        padfPrjParams[0] =
            TO_RADIANS * GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 );
        padfPrjParams[4] = GetNormProjParm( SRS_PP_SCALE_FACTOR, 1.0 );
        padfPrjParams[5] = GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 );
        padfPrjParams[6] = GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 );
    }
    else if( EQUAL(pszProjection, SRS_PT_POLAR_STEREOGRAPHIC) )
    {
        *piProjSys = PAN_PROJ_PS;
        padfPrjParams[3] =
            TO_RADIANS * GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 );
        padfPrjParams[2] =
            TO_RADIANS * GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 );
        padfPrjParams[4] = GetNormProjParm( SRS_PP_SCALE_FACTOR, 1.0 );
        padfPrjParams[5] = GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 );
        padfPrjParams[6] = GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 );
    }
    else if( EQUAL(pszProjection, SRS_PT_POLYCONIC) )
    {
        *piProjSys = PAN_PROJ_POLYC;
        padfPrjParams[3] =
            TO_RADIANS * GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 );
        padfPrjParams[2] =
            TO_RADIANS * GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 );
        padfPrjParams[5] = GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 );
        padfPrjParams[6] = GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 );
    }
    else if( EQUAL(pszProjection, SRS_PT_EQUIDISTANT_CONIC) )
    {
        *piProjSys = PAN_PROJ_EC;
        padfPrjParams[0] =
            TO_RADIANS * GetNormProjParm( SRS_PP_STANDARD_PARALLEL_1, 0.0 );
        padfPrjParams[1] =
            TO_RADIANS * GetNormProjParm( SRS_PP_STANDARD_PARALLEL_2, 0.0 );
        padfPrjParams[3] =
            TO_RADIANS * GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 );
        padfPrjParams[2] =
            TO_RADIANS * GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 );
        padfPrjParams[5] = GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 );
        padfPrjParams[6] = GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 );
    }
    else if( EQUAL(pszProjection, SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP) )
    {
        *piProjSys = PAN_PROJ_LCC;
        padfPrjParams[0] =
            TO_RADIANS * GetNormProjParm( SRS_PP_STANDARD_PARALLEL_1, 0.0 );
        padfPrjParams[1] =
            TO_RADIANS * GetNormProjParm( SRS_PP_STANDARD_PARALLEL_2, 0.0 );
        padfPrjParams[3] =
            TO_RADIANS * GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 );
        padfPrjParams[2] =
            TO_RADIANS * GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 );
        padfPrjParams[5] = GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 );
        padfPrjParams[6] = GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 );
    }
    else if( EQUAL(pszProjection, SRS_PT_TRANSVERSE_MERCATOR) )
    {
        int bNorth = FALSE;

        *piZone = GetUTMZone( &bNorth );

        if( *piZone != 0 )
        {
            *piProjSys = PAN_PROJ_UTM;
            if( !bNorth )
                *piZone = - *piZone;
        }
        else
        {
            *piProjSys = PAN_PROJ_TM;
            padfPrjParams[3] =
                TO_RADIANS * GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 );
            padfPrjParams[2] =
                TO_RADIANS * GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 );
            padfPrjParams[4] =
                GetNormProjParm( SRS_PP_SCALE_FACTOR, 1.0 );
            padfPrjParams[5] =
                GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 );
            padfPrjParams[6] =
                GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 );
        }
    }
    else if( EQUAL(pszProjection, SRS_PT_WAGNER_I) )
    {
        *piProjSys = PAN_PROJ_WAG1;
        padfPrjParams[5] = GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 );
        padfPrjParams[6] = GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 );
    }
    else if( EQUAL(pszProjection, SRS_PT_STEREOGRAPHIC) )
    {
        *piProjSys = PAN_PROJ_STEREO;
        padfPrjParams[3] =
            TO_RADIANS * GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 );
        padfPrjParams[2] =
            TO_RADIANS * GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 );
        padfPrjParams[4] = GetNormProjParm( SRS_PP_SCALE_FACTOR, 1.0 );
        padfPrjParams[5] = GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 );
        padfPrjParams[6] = GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 );
    }
    else if( EQUAL(pszProjection, SRS_PT_AZIMUTHAL_EQUIDISTANT) )
    {
        *piProjSys = PAN_PROJ_AE;
        padfPrjParams[3] =
            TO_RADIANS * GetNormProjParm( SRS_PP_LONGITUDE_OF_CENTER, 0.0 );
        padfPrjParams[0] =
            TO_RADIANS * GetNormProjParm( SRS_PP_LATITUDE_OF_CENTER, 0.0 );
        padfPrjParams[5] = GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 );
        padfPrjParams[6] = GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 );
    }
    else if( EQUAL(pszProjection, SRS_PT_GNOMONIC) )
    {
        *piProjSys = PAN_PROJ_GNOMON;
        padfPrjParams[3] =
            TO_RADIANS * GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 );
        padfPrjParams[2] =
            TO_RADIANS * GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 );
        padfPrjParams[5] = GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 );
        padfPrjParams[6] = GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 );
    }
    else if( EQUAL(pszProjection, SRS_PT_MOLLWEIDE) )
    {
        *piProjSys = PAN_PROJ_MOLL;
        padfPrjParams[3] =
            TO_RADIANS * GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 );
        padfPrjParams[5] = GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 );
        padfPrjParams[6] = GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 );
    }
    else if( EQUAL(pszProjection, SRS_PT_LAMBERT_AZIMUTHAL_EQUAL_AREA) )
    {
        *piProjSys = PAN_PROJ_LAEA;
        padfPrjParams[3] =
            TO_RADIANS * GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 );
        padfPrjParams[0] =
            TO_RADIANS * GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 );
        padfPrjParams[5] = GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 );
        padfPrjParams[6] = GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 );
    }
    else if( EQUAL(pszProjection, SRS_PT_EQUIRECTANGULAR) )
    {
        *piProjSys = PAN_PROJ_EQC;
        padfPrjParams[3] =
            TO_RADIANS * GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 );
        padfPrjParams[0] =
            TO_RADIANS * GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 );
        padfPrjParams[5] = GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 );
        padfPrjParams[6] = GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 );
    }
    else if( EQUAL(pszProjection, SRS_PT_CYLINDRICAL_EQUAL_AREA) )
    {
        *piProjSys = PAN_PROJ_CEA;
        padfPrjParams[3] =
            TO_RADIANS * GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 );
        padfPrjParams[2] =
            TO_RADIANS * GetNormProjParm( SRS_PP_STANDARD_PARALLEL_1, 0.0 );
        padfPrjParams[5] = GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 );
        padfPrjParams[6] = GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 );
    }
    else if( EQUAL(pszProjection, SRS_PT_IMW_POLYCONIC) )
    {
        *piProjSys = PAN_PROJ_IMWP;
        padfPrjParams[3] =
            TO_RADIANS * GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 );
        padfPrjParams[0] =
            TO_RADIANS * GetNormProjParm( SRS_PP_LATITUDE_OF_1ST_POINT, 0.0 );
        padfPrjParams[1] =
            TO_RADIANS * GetNormProjParm( SRS_PP_LATITUDE_OF_2ND_POINT, 0.0 );
        padfPrjParams[5] = GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 );
        padfPrjParams[6] = GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 );
    }
    // Projection unsupported by "Panorama" GIS
    else
    {
        CPLDebug( "OSR_Panorama",
                  "Projection \"%s\" unsupported by \"Panorama\" GIS. "
                  "Geographic system will be used.", pszProjection );
        *piProjSys = PAN_PROJ_NONE;
    }

/* -------------------------------------------------------------------- */
/*      Translate the datum.                                            */
/* -------------------------------------------------------------------- */
    const char  *pszDatum = GetAttrValue( "DATUM" );

    if( pszDatum == NULL )
    {
        *piDatum = PAN_DATUM_NONE;
        *piEllips = PAN_ELLIPSOID_NONE;
    }
    else if( EQUAL( pszDatum, "Pulkovo_1942" ) )
    {
        *piDatum = PAN_DATUM_PULKOVO42;
        *piEllips = PAN_ELLIPSOID_KRASSOVSKY;
    }
    else if( EQUAL( pszDatum, SRS_DN_WGS84 ) )
    {
        *piDatum = PAN_DATUM_WGS84;
        *piEllips = PAN_ELLIPSOID_WGS84;
    }

    // If not found well known datum, translate ellipsoid.
    else
    {
        const double dfSemiMajor = GetSemiMajor();
        const double dfInvFlattening = GetInvFlattening();

#ifdef DEBUG
        CPLDebug( "OSR_Panorama",
                  "Datum \"%s\" unsupported by \"Panorama\" GIS. "
                  "Trying to translate an ellipsoid definition.", pszDatum );
#endif

        int i = 0;  // Used after for.
        for( ; i < NUMBER_OF_ELLIPSOIDS; i++ )
        {
            if( aoEllips[i] )
            {
                double dfSM = 0.0;
                double dfIF = 1.0;

                if( OSRGetEllipsoidInfo( aoEllips[i], NULL,
                                         &dfSM, &dfIF ) == OGRERR_NONE
                    && CPLIsEqual(dfSemiMajor, dfSM)
                    && CPLIsEqual(dfInvFlattening, dfIF) )
                {
                    *piEllips = i;
                    break;
                }
            }
        }

        if( i == NUMBER_OF_ELLIPSOIDS )  // Didn't found matches.
        {
#ifdef DEBUG
            CPLDebug( "OSR_Panorama",
                      "Ellipsoid \"%s\" unsupported by \"Panorama\" GIS.",
                      pszDatum );
#endif
            *piDatum = PAN_DATUM_NONE;
            *piEllips = PAN_ELLIPSOID_NONE;
        }
    }

    return OGRERR_NONE;
}
