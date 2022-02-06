/******************************************************************************
 * $Id$
 *
 * Project:  libgeotiff
 * Purpose:  Code to normalize PCS and other composite codes in a GeoTIFF file.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 * Copyright (c) 2018, Even Rouault <even.rouault at spatialys.com>
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
 *****************************************************************************/

#include <assert.h>

#include "cpl_serv.h"
#include "geo_tiffp.h"
#include "geovalues.h"
#include "geo_normalize.h"
#include "geo_keyp.h"

#include "proj.h"

#ifndef KvUserDefined
#  define KvUserDefined 32767
#endif

#ifndef M_PI
#  define M_PI 3.14159265358979323846
#endif

/* EPSG Codes for projection parameters.  Unfortunately, these bear no
   relationship to the GeoTIFF codes even though the names are so similar. */

#define EPSGNatOriginLat         8801
#define EPSGNatOriginLong        8802
#define EPSGNatOriginScaleFactor 8805
#define EPSGFalseEasting         8806
#define EPSGFalseNorthing        8807
#define EPSGProjCenterLat        8811
#define EPSGProjCenterLong       8812
#define EPSGAzimuth              8813
#define EPSGAngleRectifiedToSkewedGrid 8814
#define EPSGInitialLineScaleFactor 8815
#define EPSGProjCenterEasting    8816
#define EPSGProjCenterNorthing   8817
#define EPSGPseudoStdParallelLat 8818
#define EPSGPseudoStdParallelScaleFactor 8819
#define EPSGFalseOriginLat       8821
#define EPSGFalseOriginLong      8822
#define EPSGStdParallel1Lat      8823
#define EPSGStdParallel2Lat      8824
#define EPSGFalseOriginEasting   8826
#define EPSGFalseOriginNorthing  8827
#define EPSGSphericalOriginLat   8828
#define EPSGSphericalOriginLong  8829
#define EPSGInitialLongitude     8830
#define EPSGZoneWidth            8831
#define EPSGLatOfStdParallel     8832
#define EPSGOriginLong           8833
#define EPSGTopocentricOriginLat 8834
#define EPSGTopocentricOriginLong 8835
#define EPSGTopocentricOriginHeight 8836

#define CT_Ext_Mercator_2SP     -CT_Mercator

#ifndef CPL_INLINE
#  if (defined(__GNUC__) && !defined(__NO_INLINE__)) || defined(_MSC_VER)
#    define HAS_CPL_INLINE  1
#    define CPL_INLINE __inline
#  elif defined(__SUNPRO_CC)
#    define HAS_CPL_INLINE  1
#    define CPL_INLINE inline
#  else
#    define CPL_INLINE
#  endif
#endif

#ifndef CPL_UNUSED
#if defined(__GNUC__) && __GNUC__ >= 4
#  define CPL_UNUSED __attribute((__unused__))
#else
/* TODO: add cases for other compilers */
#  define CPL_UNUSED
#endif
#endif

CPL_INLINE static void CPL_IGNORE_RET_VAL_INT(CPL_UNUSED int unused) {}

/************************************************************************/
/*                         GTIFKeyGetSSHORT()                           */
/************************************************************************/

// Geotiff SHORT keys are supposed to be unsigned, but geo_normalize interface
// uses signed short...
static int GTIFKeyGetSSHORT( GTIF *gtif, geokey_t key, short* pnVal )
{
    unsigned short sVal;
    if( GTIFKeyGetSHORT(gtif, key, &sVal, 0, 1) == 1 )
    {
        memcpy(pnVal, &sVal, 2);
        return 1;
    }
    return 0;
}

/************************************************************************/
/*                           GTIFGetPCSInfo()                           */
/************************************************************************/

int GTIFGetPCSInfoEx( void* ctxIn,
                      int nPCSCode, char **ppszEPSGName,
                      short *pnProjOp, short *pnUOMLengthCode,
                      short *pnGeogCS )

{
    PJ_CONTEXT* ctx = (PJ_CONTEXT*)ctxIn;
    int         nDatum;
    int         nZone;

    /* Deal with a few well known CRS */
    int Proj = GTIFPCSToMapSys( nPCSCode, &nDatum, &nZone );
    if ((Proj == MapSys_UTM_North || Proj == MapSys_UTM_South) &&
        nDatum != KvUserDefined)
    {
        const char* pszDatumName = NULL;
        switch (nDatum)
        {
            case GCS_NAD27: pszDatumName = "NAD27"; break;
            case GCS_NAD83: pszDatumName = "NAD83"; break;
            case GCS_WGS_72: pszDatumName = "WGS 72"; break;
            case GCS_WGS_72BE: pszDatumName = "WGS 72BE"; break;
            case GCS_WGS_84: pszDatumName = "WGS 84"; break;
            default: break;
        }

        if (pszDatumName)
        {
            if (ppszEPSGName)
            {
                char szEPSGName[64];
                sprintf(szEPSGName, "%s / UTM zone %d%c",
                        pszDatumName, nZone, (Proj == MapSys_UTM_North) ? 'N' : 'S');
                *ppszEPSGName = CPLStrdup(szEPSGName);
            }

            if (pnProjOp)
                *pnProjOp = (short) (((Proj == MapSys_UTM_North) ? Proj_UTM_zone_1N - 1 : Proj_UTM_zone_1S - 1) + nZone);

            if (pnUOMLengthCode)
                *pnUOMLengthCode = 9001; /* Linear_Meter */

            if (pnGeogCS)
                *pnGeogCS = (short) nDatum;

            return TRUE;
        }
    }

    if( nPCSCode == KvUserDefined )
        return FALSE;

    {
        char szCode[12];
        PJ* proj_crs;

        sprintf(szCode, "%d", nPCSCode);
        proj_crs = proj_create_from_database(
            ctx, "EPSG", szCode, PJ_CATEGORY_CRS, 0, NULL);
        if( !proj_crs )
        {
            return FALSE;
        }

        if( proj_get_type(proj_crs) != PJ_TYPE_PROJECTED_CRS )
        {
            proj_destroy(proj_crs);
            return FALSE;
        }

        if( ppszEPSGName )
        {
            const char* pszName = proj_get_name(proj_crs);
            if( !pszName )
            {
                // shouldn't happen
                proj_destroy(proj_crs);
                return FALSE;
            }
            *ppszEPSGName = CPLStrdup(pszName);
        }

        if( pnProjOp )
        {
            PJ* conversion = proj_crs_get_coordoperation(
                ctx, proj_crs);
            if( !conversion )
            {
                // shouldn't happen except out of memory
                proj_destroy(proj_crs);
                return FALSE;
            }

            {
                const char* pszConvCode = proj_get_id_code(conversion, 0);
                assert( pszConvCode );
                *pnProjOp = (short) atoi(pszConvCode);
            }

            proj_destroy(conversion);
        }

        if( pnUOMLengthCode )
        {
            PJ* coordSys = proj_crs_get_coordinate_system(
                ctx, proj_crs);
            if( !coordSys )
            {
                // shouldn't happen except out of memory
                proj_destroy(proj_crs);
                return FALSE;
            }

            {
                const char* pszUnitCode = NULL;
                if( !proj_cs_get_axis_info(
                    ctx, coordSys, 0,
                    NULL, /* name */
                    NULL, /* abbreviation*/
                    NULL, /* direction */
                    NULL, /* conversion factor */
                    NULL, /* unit name */
                    NULL, /* unit auth name (should be EPSG) */
                    &pszUnitCode) || pszUnitCode == NULL )
                {
                    proj_destroy(coordSys);
                    return FALSE;
                }
                *pnUOMLengthCode = (short) atoi(pszUnitCode);
                proj_destroy(coordSys);
            }
        }

        if( pnGeogCS )
        {
            PJ* geod_crs = proj_crs_get_geodetic_crs(ctx, proj_crs);
            if( !geod_crs )
            {
                // shouldn't happen except out of memory
                proj_destroy(proj_crs);
                return FALSE;
            }

            {
                const char* pszGeodCode = proj_get_id_code(geod_crs, 0);
                assert( pszGeodCode );
                *pnGeogCS = (short) atoi(pszGeodCode);
            }

            proj_destroy(geod_crs);
        }


        proj_destroy(proj_crs);
        return TRUE;
    }
}


int GTIFGetPCSInfo( int nPCSCode, char **ppszEPSGName,
                    short *pnProjOp, short *pnUOMLengthCode,
                    short *pnGeogCS )

{
    PJ_CONTEXT* ctx = proj_context_create();
    int ret = GTIFGetPCSInfoEx(ctx, nPCSCode, ppszEPSGName, pnProjOp,
                               pnUOMLengthCode, pnGeogCS);
    proj_context_destroy(ctx);
    return ret;
}

/************************************************************************/
/*                           GTIFAngleToDD()                            */
/*                                                                      */
/*      Convert a numeric angle to decimal degrees.                     */
/************************************************************************/

double GTIFAngleToDD( double dfAngle, int nUOMAngle )

{
    if( nUOMAngle == 9110 )		/* DDD.MMSSsss */
    {
        if( dfAngle > -999.9 && dfAngle < 999.9 )
        {
            char	szAngleString[32];

            sprintf( szAngleString, "%12.7f", dfAngle );
            dfAngle = GTIFAngleStringToDD( szAngleString, nUOMAngle );
        }
    }
    else if ( nUOMAngle != KvUserDefined )
    {
        double		dfInDegrees = 1.0;

        GTIFGetUOMAngleInfo( nUOMAngle, NULL, &dfInDegrees );
        dfAngle = dfAngle * dfInDegrees;
    }

    return dfAngle;
}

/************************************************************************/
/*                        GTIFAngleStringToDD()                         */
/*                                                                      */
/*      Convert an angle in the specified units to decimal degrees.     */
/************************************************************************/

double GTIFAngleStringToDD( const char * pszAngle, int nUOMAngle )

{
    double	dfAngle;

    if( nUOMAngle == 9110 )		/* DDD.MMSSsss */
    {
        char	*pszDecimal;

        dfAngle = ABS(atoi(pszAngle));
        pszDecimal = strchr(pszAngle,'.');
        if( pszDecimal != NULL && strlen(pszDecimal) > 1 )
        {
            char	szMinutes[3];
            char	szSeconds[64];

            szMinutes[0] = pszDecimal[1];
            if( pszDecimal[2] >= '0' && pszDecimal[2] <= '9' )
                szMinutes[1] = pszDecimal[2];
            else
                szMinutes[1] = '0';

            szMinutes[2] = '\0';
            dfAngle += atoi(szMinutes) / 60.0;

            if( strlen(pszDecimal) > 3 )
            {
                szSeconds[0] = pszDecimal[3];
                if( pszDecimal[4] >= '0' && pszDecimal[4] <= '9' )
                {
                    szSeconds[1] = pszDecimal[4];
                    szSeconds[2] = '.';
                    strncpy( szSeconds+3, pszDecimal + 5, sizeof(szSeconds) - 3 );
                    szSeconds[sizeof(szSeconds) - 1] = 0;
                }
                else
                {
                    szSeconds[1] = '0';
                    szSeconds[2] = '\0';
                }
                dfAngle += GTIFAtof(szSeconds) / 3600.0;
            }
        }

        if( pszAngle[0] == '-' )
            dfAngle *= -1;
    }
    else if( nUOMAngle == 9105 || nUOMAngle == 9106 )	/* grad */
    {
        dfAngle = 180 * (GTIFAtof(pszAngle ) / 200);
    }
    else if( nUOMAngle == 9101 )			/* radians */
    {
        dfAngle = 180 * (GTIFAtof(pszAngle ) / M_PI);
    }
    else if( nUOMAngle == 9103 )			/* arc-minute */
    {
        dfAngle = GTIFAtof(pszAngle) / 60;
    }
    else if( nUOMAngle == 9104 )			/* arc-second */
    {
        dfAngle = GTIFAtof(pszAngle) / 3600;
    }
    else /* decimal degrees ... some cases missing but seemingly never used */
    {
        CPLAssert( nUOMAngle == 9102 || nUOMAngle == KvUserDefined
                   || nUOMAngle == 0 );

        dfAngle = GTIFAtof(pszAngle );
    }

    return dfAngle;
}

/************************************************************************/
/*                           GTIFGetGCSInfo()                           */
/*                                                                      */
/*      Fetch the datum, and prime meridian related to a particular     */
/*      GCS.                                                            */
/************************************************************************/

int GTIFGetGCSInfoEx( void* ctxIn,
                      int nGCSCode, char ** ppszName,
                      short * pnDatum, short * pnPM, short *pnUOMAngle )

{
    PJ_CONTEXT* ctx = (PJ_CONTEXT*)ctxIn;
    int		nDatum=0, nPM, nUOMAngle;

/* -------------------------------------------------------------------- */
/*      Handle some "well known" GCS codes directly                     */
/* -------------------------------------------------------------------- */
    const char * pszName = NULL;
    nPM = PM_Greenwich;
    nUOMAngle = Angular_DMS_Hemisphere;
    if( nGCSCode == GCS_NAD27 )
    {
        nDatum = Datum_North_American_Datum_1927;
        pszName = "NAD27";
    }
    else if( nGCSCode == GCS_NAD83 )
    {
        nDatum = Datum_North_American_Datum_1983;
        pszName = "NAD83";
    }
    else if( nGCSCode == GCS_WGS_84 )
    {
        nDatum = Datum_WGS84;
        pszName = "WGS 84";
    }
    else if( nGCSCode == GCS_WGS_72 )
    {
        nDatum = Datum_WGS72;
        pszName = "WGS 72";
    }
    else if ( nGCSCode == KvUserDefined )
    {
        return FALSE;
    }

    if (pszName != NULL)
    {
        if( ppszName != NULL )
            *ppszName = CPLStrdup( pszName );
        if( pnDatum != NULL )
            *pnDatum = (short) nDatum;
        if( pnPM != NULL )
            *pnPM = (short) nPM;
        if( pnUOMAngle != NULL )
            *pnUOMAngle = (short) nUOMAngle;

        return TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Search the database.                                            */
/* -------------------------------------------------------------------- */

    {
        char szCode[12];
        PJ* geod_crs;

        sprintf(szCode, "%d", nGCSCode);
        geod_crs = proj_create_from_database(
            ctx, "EPSG", szCode, PJ_CATEGORY_CRS, 0, NULL);
        if( !geod_crs )
        {
            return FALSE;
        }

        {
            int objType = proj_get_type(geod_crs);
            if( objType != PJ_TYPE_GEODETIC_CRS &&
                objType != PJ_TYPE_GEOCENTRIC_CRS &&
                objType != PJ_TYPE_GEOGRAPHIC_2D_CRS &&
                objType != PJ_TYPE_GEOGRAPHIC_3D_CRS )
            {
                proj_destroy(geod_crs);
                return FALSE;
            }
        }

        if( ppszName )
        {
            pszName = proj_get_name(geod_crs);
            if( !pszName )
            {
                // shouldn't happen
                proj_destroy(geod_crs);
                return FALSE;
            }
            *ppszName = CPLStrdup(pszName);
        }

        if( pnDatum )
        {
#if PROJ_VERSION_MAJOR >= 8
            PJ* datum = proj_crs_get_datum_forced(ctx, geod_crs);
#else
            PJ* datum = proj_crs_get_datum(ctx, geod_crs);
#endif
            if( !datum )
            {
                proj_destroy(geod_crs);
                return FALSE;
            }

            {
                const char* pszDatumCode = proj_get_id_code(datum, 0);
                assert( pszDatumCode );
                *pnDatum = (short) atoi(pszDatumCode);
            }

            proj_destroy(datum);
        }

        if( pnPM )
        {
            PJ* pm = proj_get_prime_meridian(ctx, geod_crs);
            if( !pm )
            {
                proj_destroy(geod_crs);
                return FALSE;
            }

            {
                const char* pszPMCode = proj_get_id_code(pm, 0);
                assert( pszPMCode );
                *pnPM = (short) atoi(pszPMCode);
            }

            proj_destroy(pm);
        }

        if( pnUOMAngle )
        {
            PJ* coordSys = proj_crs_get_coordinate_system(
                ctx, geod_crs);
            if( !coordSys )
            {
                // shouldn't happen except out of memory
                proj_destroy(geod_crs);
                return FALSE;
            }

            {
                const char* pszUnitCode = NULL;
                if( !proj_cs_get_axis_info(
                    ctx, coordSys, 0,
                    NULL, /* name */
                    NULL, /* abbreviation*/
                    NULL, /* direction */
                    NULL, /* conversion factor */
                    NULL, /* unit name */
                    NULL, /* unit auth name (should be EPSG) */
                    &pszUnitCode) || pszUnitCode == NULL )
                {
                    proj_destroy(coordSys);
                    return FALSE;
                }
                *pnUOMAngle = (short) atoi(pszUnitCode);
                proj_destroy(coordSys);
            }
        }

        proj_destroy(geod_crs);
        return TRUE;
    }
}

int GTIFGetGCSInfo( int nGCSCode, char ** ppszName,
                    short * pnDatum, short * pnPM, short *pnUOMAngle )

{
    PJ_CONTEXT* ctx = proj_context_create();
    int ret = GTIFGetGCSInfoEx(ctx, nGCSCode, ppszName, pnDatum,
                               pnPM, pnUOMAngle);
    proj_context_destroy(ctx);
    return ret;
}

/************************************************************************/
/*                        GTIFGetEllipsoidInfo()                        */
/*                                                                      */
/*      Fetch info about an ellipsoid.  Axes are always returned in     */
/*      meters.  SemiMajor computed based on inverse flattening         */
/*      where that is provided.                                         */
/************************************************************************/

int GTIFGetEllipsoidInfoEx( void* ctxIn,
                            int nEllipseCode, char ** ppszName,
                            double * pdfSemiMajor, double * pdfSemiMinor )

{
    PJ_CONTEXT* ctx = (PJ_CONTEXT*)ctxIn;
/* -------------------------------------------------------------------- */
/*      Try some well known ellipsoids.                                 */
/* -------------------------------------------------------------------- */
    double	dfSemiMajor=0.0;
    double     dfInvFlattening=0.0, dfSemiMinor=0.0;
    const char *pszName = NULL;

    if( nEllipseCode == Ellipse_Clarke_1866 )
    {
        pszName = "Clarke 1866";
        dfSemiMajor = 6378206.4;
        dfSemiMinor = 6356583.8;
        dfInvFlattening = 0.0;
    }
    else if( nEllipseCode == Ellipse_GRS_1980 )
    {
        pszName = "GRS 1980";
        dfSemiMajor = 6378137.0;
        dfSemiMinor = 0.0;
        dfInvFlattening = 298.257222101;
    }
    else if( nEllipseCode == Ellipse_WGS_84 )
    {
        pszName = "WGS 84";
        dfSemiMajor = 6378137.0;
        dfSemiMinor = 0.0;
        dfInvFlattening = 298.257223563;
    }
    else if( nEllipseCode == 7043 )
    {
        pszName = "WGS 72";
        dfSemiMajor = 6378135.0;
        dfSemiMinor = 0.0;
        dfInvFlattening = 298.26;
    }

    if (pszName != NULL)
    {
        if( dfSemiMinor == 0.0 )
            dfSemiMinor = dfSemiMajor * (1 - 1.0/dfInvFlattening);

        if( pdfSemiMinor != NULL )
            *pdfSemiMinor = dfSemiMinor;
        if( pdfSemiMajor != NULL )
            *pdfSemiMajor = dfSemiMajor;
        if( ppszName != NULL )
            *ppszName = CPLStrdup( pszName );

        return TRUE;
    }

    if( nEllipseCode == KvUserDefined )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Search the database.                                            */
/* -------------------------------------------------------------------- */
    {
        char szCode[12];
        PJ* ellipsoid;

        sprintf(szCode, "%d", nEllipseCode);
        ellipsoid = proj_create_from_database(
            ctx, "EPSG", szCode, PJ_CATEGORY_ELLIPSOID, 0, NULL);
        if( !ellipsoid )
        {
            return FALSE;
        }

        if( ppszName )
        {
            pszName = proj_get_name(ellipsoid);
            if( !pszName )
            {
                // shouldn't happen
                proj_destroy(ellipsoid);
                return FALSE;
            }
            *ppszName = CPLStrdup(pszName);
        }

        proj_ellipsoid_get_parameters(
            ctx, ellipsoid, pdfSemiMajor, pdfSemiMinor, NULL, NULL);

        proj_destroy(ellipsoid);

        return TRUE;
    }
}

int GTIFGetEllipsoidInfo( int nEllipseCode, char ** ppszName,
                          double * pdfSemiMajor, double * pdfSemiMinor )

{
    PJ_CONTEXT* ctx = proj_context_create();
    int ret = GTIFGetEllipsoidInfoEx(ctx, nEllipseCode, ppszName, pdfSemiMajor,
                                     pdfSemiMinor);
    proj_context_destroy(ctx);
    return ret;
}

/************************************************************************/
/*                           GTIFGetPMInfo()                            */
/*                                                                      */
/*      Get the offset between a given prime meridian and Greenwich     */
/*      in degrees.                                                     */
/************************************************************************/

int GTIFGetPMInfoEx( void* ctxIn,
                     int nPMCode, char ** ppszName, double *pdfOffset )

{
    PJ_CONTEXT* ctx = (PJ_CONTEXT*)ctxIn;

/* -------------------------------------------------------------------- */
/*      Use a special short cut for Greenwich, since it is so common.   */
/* -------------------------------------------------------------------- */
    if( nPMCode == PM_Greenwich )
    {
        if( pdfOffset != NULL )
            *pdfOffset = 0.0;
        if( ppszName != NULL )
            *ppszName = CPLStrdup( "Greenwich" );
        return TRUE;
    }


    if( nPMCode == KvUserDefined )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Search the database.                                            */
/* -------------------------------------------------------------------- */
    {
        char szCode[12];
        PJ* pm;

        sprintf(szCode, "%d", nPMCode);
        pm = proj_create_from_database(
            ctx, "EPSG", szCode, PJ_CATEGORY_PRIME_MERIDIAN, 0, NULL);
        if( !pm )
        {
            return FALSE;
        }

        if( ppszName )
        {
            const char* pszName = proj_get_name(pm);
            if( !pszName )
            {
                // shouldn't happen
                proj_destroy(pm);
                return FALSE;
            }
            *ppszName = CPLStrdup(pszName);
        }

        if( pdfOffset )
        {
            double conv_factor = 0;
            proj_prime_meridian_get_parameters(
                ctx, pm, pdfOffset, &conv_factor, NULL);
            *pdfOffset *= conv_factor * 180.0 / M_PI;
        }

        proj_destroy(pm);

        return TRUE;
    }
}

int GTIFGetPMInfo( int nPMCode, char ** ppszName, double *pdfOffset )

{
    PJ_CONTEXT* ctx = proj_context_create();
    int ret = GTIFGetPMInfoEx(ctx, nPMCode, ppszName, pdfOffset);
    proj_context_destroy(ctx);
    return ret;
}

/************************************************************************/
/*                          GTIFGetDatumInfo()                          */
/*                                                                      */
/*      Fetch the ellipsoid, and name for a datum.                      */
/************************************************************************/

int GTIFGetDatumInfoEx( void* ctxIn,
                        int nDatumCode, char ** ppszName, short * pnEllipsoid )

{
    PJ_CONTEXT* ctx = (PJ_CONTEXT*)ctxIn;
    const char* pszName = NULL;
    int		nEllipsoid = 0;

/* -------------------------------------------------------------------- */
/*      Handle a few built-in datums.                                   */
/* -------------------------------------------------------------------- */
    if( nDatumCode == Datum_North_American_Datum_1927 )
    {
        nEllipsoid = Ellipse_Clarke_1866;
        pszName = "North American Datum 1927";
    }
    else if( nDatumCode == Datum_North_American_Datum_1983 )
    {
        nEllipsoid = Ellipse_GRS_1980;
        pszName = "North American Datum 1983";
    }
    else if( nDatumCode == Datum_WGS84 )
    {
        nEllipsoid = Ellipse_WGS_84;
        pszName = "World Geodetic System 1984";
    }
    else if( nDatumCode == Datum_WGS72 )
    {
        nEllipsoid = 7043; /* WGS72 */
        pszName = "World Geodetic System 1972";
    }

    if (pszName != NULL)
    {
        if( pnEllipsoid != NULL )
            *pnEllipsoid = (short) nEllipsoid;

        if( ppszName != NULL )
            *ppszName = CPLStrdup( pszName );

        return TRUE;
    }

    if( nDatumCode == KvUserDefined )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Search the database.                                            */
/* -------------------------------------------------------------------- */
    {
        char szCode[12];
        PJ* datum;

        sprintf(szCode, "%d", nDatumCode);
        datum = proj_create_from_database(
            ctx, "EPSG", szCode, PJ_CATEGORY_DATUM, 0, NULL);
        if( !datum )
        {
            return FALSE;
        }

        if( proj_get_type(datum) != PJ_TYPE_GEODETIC_REFERENCE_FRAME )
        {
            proj_destroy(datum);
            return FALSE;
        }

        if( ppszName )
        {
            pszName = proj_get_name(datum);
            if( !pszName )
            {
                // shouldn't happen
                proj_destroy(datum);
                return FALSE;
            }
            *ppszName = CPLStrdup(pszName);
        }

        if( pnEllipsoid )
        {
            PJ* ellipsoid = proj_get_ellipsoid(ctx, datum);
            if( !ellipsoid )
            {
                proj_destroy(datum);
                return FALSE;
            }

            {
                const char* pszEllipsoidCode = proj_get_id_code(
                    ellipsoid, 0);
                assert( pszEllipsoidCode );
                *pnEllipsoid = (short) atoi(pszEllipsoidCode);
            }

            proj_destroy(ellipsoid);
        }

        proj_destroy(datum);

        return TRUE;
    }
}

int GTIFGetDatumInfo( int nDatumCode, char ** ppszName, short * pnEllipsoid )

{
    PJ_CONTEXT* ctx = proj_context_create();
    int ret = GTIFGetDatumInfoEx(ctx, nDatumCode, ppszName, pnEllipsoid);
    proj_context_destroy(ctx);
    return ret;
}

/************************************************************************/
/*                        GTIFGetUOMLengthInfo()                        */
/*                                                                      */
/*      Note: This function should eventually also know how to          */
/*      lookup length aliases in the UOM_LE_ALIAS table.                */
/************************************************************************/

int GTIFGetUOMLengthInfoEx( void* ctxIn,
                            int nUOMLengthCode,
                            char **ppszUOMName,
                            double * pdfInMeters )

{
    PJ_CONTEXT* ctx = (PJ_CONTEXT*)ctxIn;
/* -------------------------------------------------------------------- */
/*      We short cut meter to save work and avoid failure for missing   */
/*      in the most common cases.       				*/
/* -------------------------------------------------------------------- */
    if( nUOMLengthCode == 9001 )
    {
        if( ppszUOMName != NULL )
            *ppszUOMName = CPLStrdup( "metre" );
        if( pdfInMeters != NULL )
            *pdfInMeters = 1.0;

        return TRUE;
    }

    if( nUOMLengthCode == 9002 )
    {
        if( ppszUOMName != NULL )
            *ppszUOMName = CPLStrdup( "foot" );
        if( pdfInMeters != NULL )
            *pdfInMeters = 0.3048;

        return TRUE;
    }

    if( nUOMLengthCode == 9003 )
    {
        if( ppszUOMName != NULL )
            *ppszUOMName = CPLStrdup( "US survey foot" );
        if( pdfInMeters != NULL )
            *pdfInMeters = 12.0 / 39.37;

        return TRUE;
    }

    if( nUOMLengthCode == KvUserDefined )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Search the units database for this unit.  If we don't find      */
/*      it return failure.                                              */
/* -------------------------------------------------------------------- */
    {
        char szCode[12];
        const char* pszName = NULL;

        sprintf(szCode, "%d", nUOMLengthCode);
        if( !proj_uom_get_info_from_database(
            ctx, "EPSG", szCode, &pszName, pdfInMeters,  NULL) )
        {
            return FALSE;
        }
        if( ppszUOMName )
        {
            *ppszUOMName = CPLStrdup(pszName);
        }
        return TRUE;
    }
}

int GTIFGetUOMLengthInfo( int nUOMLengthCode,
                          char **ppszUOMName,
                          double * pdfInMeters )

{
    PJ_CONTEXT* ctx = proj_context_create();
    int ret = GTIFGetUOMLengthInfoEx(
        ctx, nUOMLengthCode, ppszUOMName, pdfInMeters);
    proj_context_destroy(ctx);
    return ret;
}

/************************************************************************/
/*                        GTIFGetUOMAngleInfo()                         */
/************************************************************************/

int GTIFGetUOMAngleInfoEx( void* ctxIn,
                           int nUOMAngleCode,
                           char **ppszUOMName,
                           double * pdfInDegrees )

{
    PJ_CONTEXT* ctx = (PJ_CONTEXT*)ctxIn;
    const char	*pszUOMName = NULL;
    double	dfInDegrees = 1.0;

    switch( nUOMAngleCode )
    {
      case 9101:
        pszUOMName = "radian";
        dfInDegrees = 180.0 / M_PI;
        break;

      case 9102:
      case 9107:
      case 9108:
      case 9110:
      case 9122:
        pszUOMName = "degree";
        dfInDegrees = 1.0;
        break;

      case 9103:
        pszUOMName = "arc-minute";
        dfInDegrees = 1 / 60.0;
        break;

      case 9104:
        pszUOMName = "arc-second";
        dfInDegrees = 1 / 3600.0;
        break;

      case 9105:
        pszUOMName = "grad";
        dfInDegrees = 180.0 / 200.0;
        break;

      case 9106:
        pszUOMName = "gon";
        dfInDegrees = 180.0 / 200.0;
        break;

      case 9109:
        pszUOMName = "microradian";
        dfInDegrees = 180.0 / (M_PI * 1000000.0);
        break;

      default:
        break;
    }

    if (pszUOMName)
    {
        if( ppszUOMName != NULL )
        {
            *ppszUOMName = CPLStrdup( pszUOMName );
        }

        if( pdfInDegrees != NULL )
            *pdfInDegrees = dfInDegrees;

        return TRUE;
    }

    if( nUOMAngleCode == KvUserDefined )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Search the units database for this unit.  If we don't find      */
/*      it return failure.                                              */
/* -------------------------------------------------------------------- */
    {
        char szCode[12];
        const char* pszName = NULL;
        double dfConvFactorToRadians = 0;

        sprintf(szCode, "%d", nUOMAngleCode);
        if( !proj_uom_get_info_from_database(
            ctx, "EPSG", szCode, &pszName, &dfConvFactorToRadians, NULL) )
        {
            return FALSE;
        }
        if( ppszUOMName )
        {
            *ppszUOMName = CPLStrdup(pszName);
        }
        if( pdfInDegrees )
        {
            *pdfInDegrees = dfConvFactorToRadians * 180.0 / M_PI;
        }
        return TRUE;
    }
}

int GTIFGetUOMAngleInfo( int nUOMAngleCode,
                         char **ppszUOMName,
                         double * pdfInDegrees )

{
    PJ_CONTEXT* ctx = proj_context_create();
    int ret = GTIFGetUOMAngleInfoEx(
        ctx, nUOMAngleCode, ppszUOMName, pdfInDegrees);
    proj_context_destroy(ctx);
    return ret;
}

/************************************************************************/
/*                    EPSGProjMethodToCTProjMethod()                    */
/*                                                                      */
/*      Convert between the EPSG enumeration for projection methods,    */
/*      and the GeoTIFF CT codes.                                       */
/************************************************************************/

static int EPSGProjMethodToCTProjMethod( int nEPSG, int bReturnExtendedCTCode )

{
    switch( nEPSG )
    {
      case 9801:
        return CT_LambertConfConic_1SP;

      case 9802:
        return CT_LambertConfConic_2SP;

      case 9803:
        return CT_LambertConfConic_2SP; /* Belgian variant not supported */

      case 9804:
        return CT_Mercator;  /* 1SP and 2SP not differentiated */

      case 9805:
        if( bReturnExtendedCTCode )
            return CT_Ext_Mercator_2SP;
        else
            return CT_Mercator;  /* 1SP and 2SP not differentiated */

      /* Mercator 1SP (Spherical) For EPSG:3785 */
      case 9841:
        return CT_Mercator;  /* 1SP and 2SP not differentiated */

      /* Google Mercator For EPSG:3857 */
      case 1024:
        return CT_Mercator;  /* 1SP and 2SP not differentiated */

      case 9806:
        return CT_CassiniSoldner;

      case 9807:
        return CT_TransverseMercator;

      case 9808:
        return CT_TransvMercator_SouthOriented;

      case 9809:
        return CT_ObliqueStereographic;

      case 9810:
      case 9829: /* variant B not quite the same - not sure how to handle */
        return CT_PolarStereographic;

      case 9811:
        return CT_NewZealandMapGrid;

      case 9812:
        return CT_ObliqueMercator; /* is hotine actually different? */

      case 9813:
        return CT_ObliqueMercator_Laborde;

      case 9814:
        return CT_ObliqueMercator_Rosenmund; /* swiss  */

      case 9815:
        return CT_HotineObliqueMercatorAzimuthCenter;

      case 9816: /* tunesia mining grid has no counterpart */
        return KvUserDefined;

      case 9818:
        return CT_Polyconic;

      case 9820:
      case 1027:
        return CT_LambertAzimEqualArea;

      case 9822:
        return CT_AlbersEqualArea;

      case 9834:
        return CT_CylindricalEqualArea;

      case 1028:
      case 1029:
      case 9823: /* spherical */
      case 9842: /* elliptical */
        return CT_Equirectangular;

      default: /* use the EPSG code for other methods */
        return nEPSG;
    }
}

/************************************************************************/
/*                           SetGTParamIds()                            */
/*                                                                      */
/*      This is hardcoded logic to set the GeoTIFF parameter            */
/*      identifiers for all the EPSG supported projections.  As new     */
/*      projection methods are added, this code will need to be updated */
/************************************************************************/

static int SetGTParamIds( int nCTProjection,
                          int nEPSGProjMethod,
                          int *panProjParamId,
                          int *panEPSGCodes )

{
    int anWorkingDummy[7];

    if( panEPSGCodes == NULL )
        panEPSGCodes = anWorkingDummy;
    if( panProjParamId == NULL )
        panProjParamId = anWorkingDummy;

    memset( panEPSGCodes, 0, sizeof(int) * 7 );

    /* psDefn->nParms = 7; */

    switch( nCTProjection )
    {
      case CT_CassiniSoldner:
      case CT_NewZealandMapGrid:
      case CT_Polyconic:
        panProjParamId[0] = ProjNatOriginLatGeoKey;
        panProjParamId[1] = ProjNatOriginLongGeoKey;
        panProjParamId[5] = ProjFalseEastingGeoKey;
        panProjParamId[6] = ProjFalseNorthingGeoKey;

        panEPSGCodes[0] = EPSGNatOriginLat;
        panEPSGCodes[1] = EPSGNatOriginLong;
        panEPSGCodes[5] = EPSGFalseEasting;
        panEPSGCodes[6] = EPSGFalseNorthing;
        return TRUE;

      case CT_ObliqueMercator:
      case CT_HotineObliqueMercatorAzimuthCenter:
        panProjParamId[0] = ProjCenterLatGeoKey;
        panProjParamId[1] = ProjCenterLongGeoKey;
        panProjParamId[2] = ProjAzimuthAngleGeoKey;
        panProjParamId[3] = ProjRectifiedGridAngleGeoKey;
        panProjParamId[4] = ProjScaleAtCenterGeoKey;
        panProjParamId[5] = ProjFalseEastingGeoKey;
        panProjParamId[6] = ProjFalseNorthingGeoKey;

        panEPSGCodes[0] = EPSGProjCenterLat;
        panEPSGCodes[1] = EPSGProjCenterLong;
        panEPSGCodes[2] = EPSGAzimuth;
        panEPSGCodes[3] = EPSGAngleRectifiedToSkewedGrid;
        panEPSGCodes[4] = EPSGInitialLineScaleFactor;
        panEPSGCodes[5] = EPSGProjCenterEasting; /* EPSG proj method 9812 uses EPSGFalseEasting, but 9815 uses EPSGProjCenterEasting */
        panEPSGCodes[6] = EPSGProjCenterNorthing; /* EPSG proj method 9812 uses EPSGFalseNorthing, but 9815 uses EPSGProjCenterNorthing */
        return TRUE;

      case CT_ObliqueMercator_Laborde:
        panProjParamId[0] = ProjCenterLatGeoKey;
        panProjParamId[1] = ProjCenterLongGeoKey;
        panProjParamId[2] = ProjAzimuthAngleGeoKey;
        panProjParamId[4] = ProjScaleAtCenterGeoKey;
        panProjParamId[5] = ProjFalseEastingGeoKey;
        panProjParamId[6] = ProjFalseNorthingGeoKey;

        panEPSGCodes[0] = EPSGProjCenterLat;
        panEPSGCodes[1] = EPSGProjCenterLong;
        panEPSGCodes[2] = EPSGAzimuth;
        panEPSGCodes[4] = EPSGInitialLineScaleFactor;
        panEPSGCodes[5] = EPSGFalseEasting;
        panEPSGCodes[6] = EPSGFalseNorthing;
        return TRUE;

      case CT_LambertConfConic_1SP:
      case CT_Mercator:
      case CT_ObliqueStereographic:
      case CT_PolarStereographic:
      case CT_TransverseMercator:
      case CT_TransvMercator_SouthOriented:
        panProjParamId[0] = ProjNatOriginLatGeoKey;
        if( nCTProjection == CT_PolarStereographic )
        {
            panProjParamId[1] = ProjStraightVertPoleLongGeoKey;
        }
        else
        {
            panProjParamId[1] = ProjNatOriginLongGeoKey;
        }
        if( nEPSGProjMethod == 9805 ) /* Mercator_2SP */
        {
            panProjParamId[2] = ProjStdParallel1GeoKey;
        }
        panProjParamId[4] = ProjScaleAtNatOriginGeoKey;
        panProjParamId[5] = ProjFalseEastingGeoKey;
        panProjParamId[6] = ProjFalseNorthingGeoKey;

        panEPSGCodes[0] = EPSGNatOriginLat;
        panEPSGCodes[1] = EPSGNatOriginLong;
        if( nEPSGProjMethod == 9805 ) /* Mercator_2SP */
        {
            panEPSGCodes[2] = EPSGStdParallel1Lat;
        }
        panEPSGCodes[4] = EPSGNatOriginScaleFactor;
        panEPSGCodes[5] = EPSGFalseEasting;
        panEPSGCodes[6] = EPSGFalseNorthing;
        return TRUE;

      case CT_LambertConfConic_2SP:
        panProjParamId[0] = ProjFalseOriginLatGeoKey;
        panProjParamId[1] = ProjFalseOriginLongGeoKey;
        panProjParamId[2] = ProjStdParallel1GeoKey;
        panProjParamId[3] = ProjStdParallel2GeoKey;
        panProjParamId[5] = ProjFalseEastingGeoKey;
        panProjParamId[6] = ProjFalseNorthingGeoKey;

        panEPSGCodes[0] = EPSGFalseOriginLat;
        panEPSGCodes[1] = EPSGFalseOriginLong;
        panEPSGCodes[2] = EPSGStdParallel1Lat;
        panEPSGCodes[3] = EPSGStdParallel2Lat;
        panEPSGCodes[5] = EPSGFalseOriginEasting;
        panEPSGCodes[6] = EPSGFalseOriginNorthing;
        return TRUE;

      case CT_AlbersEqualArea:
        panProjParamId[0] = ProjStdParallel1GeoKey;
        panProjParamId[1] = ProjStdParallel2GeoKey;
        panProjParamId[2] = ProjNatOriginLatGeoKey;
        panProjParamId[3] = ProjNatOriginLongGeoKey;
        panProjParamId[5] = ProjFalseEastingGeoKey;
        panProjParamId[6] = ProjFalseNorthingGeoKey;

        panEPSGCodes[0] = EPSGStdParallel1Lat;
        panEPSGCodes[1] = EPSGStdParallel2Lat;
        panEPSGCodes[2] = EPSGFalseOriginLat;
        panEPSGCodes[3] = EPSGFalseOriginLong;
        panEPSGCodes[5] = EPSGFalseOriginEasting;
        panEPSGCodes[6] = EPSGFalseOriginNorthing;
        return TRUE;

      case CT_SwissObliqueCylindrical:
        panProjParamId[0] = ProjCenterLatGeoKey;
        panProjParamId[1] = ProjCenterLongGeoKey;
        panProjParamId[5] = ProjFalseEastingGeoKey;
        panProjParamId[6] = ProjFalseNorthingGeoKey;

        /* EPSG codes? */
        return TRUE;

      case CT_LambertAzimEqualArea:
        panProjParamId[0] = ProjCenterLatGeoKey;
        panProjParamId[1] = ProjCenterLongGeoKey;
        panProjParamId[5] = ProjFalseEastingGeoKey;
        panProjParamId[6] = ProjFalseNorthingGeoKey;

        panEPSGCodes[0] = EPSGNatOriginLat;
        panEPSGCodes[1] = EPSGNatOriginLong;
        panEPSGCodes[5] = EPSGFalseEasting;
        panEPSGCodes[6] = EPSGFalseNorthing;
        return TRUE;

      case CT_CylindricalEqualArea:
        panProjParamId[0] = ProjStdParallel1GeoKey;
        panProjParamId[1] = ProjNatOriginLongGeoKey;
        panProjParamId[5] = ProjFalseEastingGeoKey;
        panProjParamId[6] = ProjFalseNorthingGeoKey;

        panEPSGCodes[0] = EPSGStdParallel1Lat;
        panEPSGCodes[1] = EPSGFalseOriginLong;
        panEPSGCodes[5] = EPSGFalseOriginEasting;
        panEPSGCodes[6] = EPSGFalseOriginNorthing;
        return TRUE;

      case CT_Equirectangular:
        panProjParamId[0] = ProjCenterLatGeoKey;
        panProjParamId[1] = ProjCenterLongGeoKey;
        panProjParamId[2] = ProjStdParallel1GeoKey;
        panProjParamId[5] = ProjFalseEastingGeoKey;
        panProjParamId[6] = ProjFalseNorthingGeoKey;

        panEPSGCodes[0] = EPSGNatOriginLat;
        panEPSGCodes[1] = EPSGNatOriginLong;
        panEPSGCodes[2] = EPSGStdParallel1Lat;
        panEPSGCodes[5] = EPSGFalseEasting;
        panEPSGCodes[6] = EPSGFalseNorthing;
        return TRUE;

      case CT_Ext_Mercator_2SP:
        panProjParamId[0] = ProjNatOriginLatGeoKey;
        panProjParamId[1] = ProjNatOriginLongGeoKey;
        panProjParamId[2] = ProjStdParallel1GeoKey;
        panProjParamId[5] = ProjFalseEastingGeoKey;
        panProjParamId[6] = ProjFalseNorthingGeoKey;

        panEPSGCodes[0] = EPSGNatOriginLat;
        panEPSGCodes[1] = EPSGNatOriginLong;
        panEPSGCodes[2] = EPSGStdParallel1Lat;
        panEPSGCodes[5] = EPSGFalseEasting;
        panEPSGCodes[6] = EPSGFalseNorthing;
        return TRUE;

      default:
        return FALSE;
    }
}

/************************************************************************/
/*                         GTIFGetProjTRFInfo()                         */
/*                                                                      */
/*      Transform a PROJECTION_TRF_CODE into a projection method,       */
/*      and a set of parameters.  The parameters identify will          */
/*      depend on the returned method, but they will all have been      */
/*      normalized into degrees and meters.                             */
/************************************************************************/

int GTIFGetProjTRFInfoEx( void* ctxIn,
                          int nProjTRFCode,
                          char **ppszProjTRFName,
                          short * pnProjMethod,
                          double * padfProjParams )

{
    PJ_CONTEXT* ctx = (PJ_CONTEXT*)ctxIn;

    if ((nProjTRFCode >= Proj_UTM_zone_1N && nProjTRFCode <= Proj_UTM_zone_60N) ||
        (nProjTRFCode >= Proj_UTM_zone_1S && nProjTRFCode <= Proj_UTM_zone_60S))
    {
        int bNorth;
        int nZone;
        if (nProjTRFCode <= Proj_UTM_zone_60N)
        {
            bNorth = TRUE;
            nZone = nProjTRFCode - Proj_UTM_zone_1N + 1;
        }
        else
        {
            bNorth = FALSE;
            nZone = nProjTRFCode - Proj_UTM_zone_1S + 1;
        }

        if (ppszProjTRFName)
        {
            char szProjTRFName[64];
            sprintf(szProjTRFName, "UTM zone %d%c",
                    nZone, (bNorth) ? 'N' : 'S');
            *ppszProjTRFName = CPLStrdup(szProjTRFName);
        }

        if (pnProjMethod)
            *pnProjMethod = 9807;

        if (padfProjParams)
        {
            padfProjParams[0] = 0;
            padfProjParams[1] = -183 + 6 * nZone;
            padfProjParams[2] = 0;
            padfProjParams[3] = 0;
            padfProjParams[4] = 0.9996;
            padfProjParams[5] = 500000;
            padfProjParams[6] = (bNorth) ? 0 : 10000000;
        }

        return TRUE;
    }

    if( nProjTRFCode == KvUserDefined )
        return FALSE;

    {
        int     nProjMethod, i, anEPSGCodes[7];
        double  adfProjParams[7];
        char    szCode[12];
        const char* pszMethodCode = NULL;
        int     nCTProjMethod;
        PJ *transf;

        sprintf(szCode, "%d", nProjTRFCode);
        transf = proj_create_from_database(
            ctx, "EPSG", szCode, PJ_CATEGORY_COORDINATE_OPERATION, 0, NULL);
        if( !transf )
        {
            return FALSE;
        }

        if( proj_get_type(transf) != PJ_TYPE_CONVERSION )
        {
            proj_destroy(transf);
            return FALSE;
        }

        /* Get the projection method code */
        proj_coordoperation_get_method_info(ctx, transf,
                                            NULL, /* method name */
                                            NULL, /* method auth name (should be EPSG) */
                                            &pszMethodCode);
        assert( pszMethodCode );
        nProjMethod = atoi(pszMethodCode);

/* -------------------------------------------------------------------- */
/*      Initialize a definition of what EPSG codes need to be loaded    */
/*      into what fields in adfProjParams.                               */
/* -------------------------------------------------------------------- */
        nCTProjMethod = EPSGProjMethodToCTProjMethod( nProjMethod, TRUE );
        SetGTParamIds( nCTProjMethod, nProjMethod, NULL, anEPSGCodes );

/* -------------------------------------------------------------------- */
/*      Get the parameters for this projection.                         */
/* -------------------------------------------------------------------- */

        for( i = 0; i < 7; i++ )
        {
            double  dfValue = 0.0;
            double  dfUnitConvFactor = 0.0;
            const char *pszUOMCategory = NULL;
            int     nEPSGCode = anEPSGCodes[i];
            int     iEPSG;
            int     nParamCount;

            /* Establish default */
            if( nEPSGCode == EPSGAngleRectifiedToSkewedGrid )
                adfProjParams[i] = 90.0;
            else if( nEPSGCode == EPSGNatOriginScaleFactor
                    || nEPSGCode == EPSGInitialLineScaleFactor
                    || nEPSGCode == EPSGPseudoStdParallelScaleFactor )
                adfProjParams[i] = 1.0;
            else
                adfProjParams[i] = 0.0;

            /* If there is no parameter, skip */
            if( nEPSGCode == 0 )
                continue;

            nParamCount = proj_coordoperation_get_param_count(ctx, transf);

            /* Find the matching parameter */
            for( iEPSG = 0; iEPSG < nParamCount; iEPSG++ )
            {
                const char* pszParamCode = NULL;
                proj_coordoperation_get_param(
                    ctx, transf, iEPSG,
                    NULL, /* name */
                    NULL, /* auth name */
                    &pszParamCode,
                    &dfValue,
                    NULL, /* value (string) */
                    &dfUnitConvFactor, /* unit conv factor */
                    NULL, /* unit name */
                    NULL, /* unit auth name */
                    NULL, /* unit code */
                    &pszUOMCategory /* unit category */);
                assert(pszParamCode);
                if( atoi(pszParamCode) == nEPSGCode )
                {
                    break;
                }
            }

            /* not found, accept the default */
            if( iEPSG == nParamCount )
            {
                /* for CT_ObliqueMercator try alternate parameter codes first */
                /* because EPSG proj method 9812 uses EPSGFalseXXXXX, but 9815 uses EPSGProjCenterXXXXX */
                if ( nCTProjMethod == CT_ObliqueMercator && nEPSGCode == EPSGProjCenterEasting )
                    nEPSGCode = EPSGFalseEasting;
                else if ( nCTProjMethod == CT_ObliqueMercator && nEPSGCode == EPSGProjCenterNorthing )
                    nEPSGCode = EPSGFalseNorthing;
                /* for CT_PolarStereographic try alternate parameter codes first */
                /* because EPSG proj method 9829 uses EPSGLatOfStdParallel instead of EPSGNatOriginLat */
                /* and EPSGOriginLong instead of EPSGNatOriginLong */
                else if( nCTProjMethod == CT_PolarStereographic && nEPSGCode == EPSGNatOriginLat )
                    nEPSGCode = EPSGLatOfStdParallel;
                else if( nCTProjMethod == CT_PolarStereographic && nEPSGCode == EPSGNatOriginLong )
                    nEPSGCode = EPSGOriginLong;
                else
                    continue;

                for( iEPSG = 0; iEPSG < nParamCount; iEPSG++ )
                {
                    const char* pszParamCode = NULL;
                    proj_coordoperation_get_param(
                        ctx, transf, iEPSG,
                        NULL, /* name */
                        NULL, /* auth name */
                        &pszParamCode,
                        &dfValue,
                        NULL, /* value (string) */
                        &dfUnitConvFactor, /* unit conv factor */
                        NULL, /* unit name */
                        NULL, /* unit auth name */
                        NULL, /* unit code */
                        &pszUOMCategory /* unit category */);
                    assert(pszParamCode);
                    if( atoi(pszParamCode) == nEPSGCode )
                    {
                        break;
                    }
                }

                if( iEPSG == nParamCount )
                    continue;
            }

            assert(pszUOMCategory);

            adfProjParams[i] = dfValue * dfUnitConvFactor;
            if( strcmp(pszUOMCategory, "angular") == 0.0 )
            {
                /* Convert from radians to degrees */
                adfProjParams[i] *= 180 / M_PI;
            }
        }

/* -------------------------------------------------------------------- */
/*      Get the name, if requested.                                     */
/* -------------------------------------------------------------------- */
        if( ppszProjTRFName != NULL )
        {
            const char* pszName = proj_get_name(transf);
            if( !pszName )
            {
                // shouldn't happen
                proj_destroy(transf);
                return FALSE;
            }
            *ppszProjTRFName = CPLStrdup(pszName);
        }

/* -------------------------------------------------------------------- */
/*      Transfer requested data into passed variables.                  */
/* -------------------------------------------------------------------- */
        if( pnProjMethod != NULL )
            *pnProjMethod = (short) nProjMethod;

        if( padfProjParams != NULL )
        {
            for( i = 0; i < 7; i++ )
                padfProjParams[i] = adfProjParams[i];
        }

        proj_destroy(transf);

        return TRUE;
    }
}

int GTIFGetProjTRFInfo( /* Conversion code */
                        int nProjTRFCode,
                        char **ppszProjTRFName,
                        short * pnProjMethod,
                        double * padfProjParams )
{
    PJ_CONTEXT* ctx = proj_context_create();
    int ret = GTIFGetProjTRFInfoEx(
        ctx, nProjTRFCode, ppszProjTRFName, pnProjMethod, padfProjParams);
    proj_context_destroy(ctx);
    return ret;
}

/************************************************************************/
/*                         GTIFFetchProjParms()                         */
/*                                                                      */
/*      Fetch the projection parameters for a particular projection     */
/*      from a GeoTIFF file, and fill the GTIFDefn structure out        */
/*      with them.                                                      */
/************************************************************************/

static void GTIFFetchProjParms( GTIF * psGTIF, GTIFDefn * psDefn )

{
    double dfNatOriginLong = 0.0, dfNatOriginLat = 0.0, dfRectGridAngle = 0.0;
    double dfFalseEasting = 0.0, dfFalseNorthing = 0.0, dfNatOriginScale = 1.0;
    double dfStdParallel1 = 0.0, dfStdParallel2 = 0.0, dfAzimuth = 0.0;
    int iParam;
    int bHaveSP1, bHaveNOS;

/* -------------------------------------------------------------------- */
/*      Get the false easting, and northing if available.               */
/* -------------------------------------------------------------------- */
    if( !GTIFKeyGetDOUBLE(psGTIF, ProjFalseEastingGeoKey, &dfFalseEasting, 0, 1)
        && !GTIFKeyGetDOUBLE(psGTIF, ProjCenterEastingGeoKey,
                       &dfFalseEasting, 0, 1)
        && !GTIFKeyGetDOUBLE(psGTIF, ProjFalseOriginEastingGeoKey,
                       &dfFalseEasting, 0, 1) )
        dfFalseEasting = 0.0;

    if( !GTIFKeyGetDOUBLE(psGTIF, ProjFalseNorthingGeoKey, &dfFalseNorthing,0,1)
        && !GTIFKeyGetDOUBLE(psGTIF, ProjCenterNorthingGeoKey,
                       &dfFalseNorthing, 0, 1)
        && !GTIFKeyGetDOUBLE(psGTIF, ProjFalseOriginNorthingGeoKey,
                       &dfFalseNorthing, 0, 1) )
        dfFalseNorthing = 0.0;

    switch( psDefn->CTProjection )
    {
/* -------------------------------------------------------------------- */
      case CT_Stereographic:
/* -------------------------------------------------------------------- */
        if( GTIFKeyGetDOUBLE(psGTIF, ProjNatOriginLongGeoKey,
                       &dfNatOriginLong, 0, 1 ) == 0
            && GTIFKeyGetDOUBLE(psGTIF, ProjFalseOriginLongGeoKey,
                          &dfNatOriginLong, 0, 1 ) == 0
            && GTIFKeyGetDOUBLE(psGTIF, ProjCenterLongGeoKey,
                          &dfNatOriginLong, 0, 1 ) == 0 )
            dfNatOriginLong = 0.0;

        if( GTIFKeyGetDOUBLE(psGTIF, ProjNatOriginLatGeoKey,
                       &dfNatOriginLat, 0, 1 ) == 0
            && GTIFKeyGetDOUBLE(psGTIF, ProjFalseOriginLatGeoKey,
                          &dfNatOriginLat, 0, 1 ) == 0
            && GTIFKeyGetDOUBLE(psGTIF, ProjCenterLatGeoKey,
                          &dfNatOriginLat, 0, 1 ) == 0 )
            dfNatOriginLat = 0.0;

        if( GTIFKeyGetDOUBLE(psGTIF, ProjScaleAtNatOriginGeoKey,
                       &dfNatOriginScale, 0, 1 ) == 0 )
            dfNatOriginScale = 1.0;

        /* notdef: should transform to decimal degrees at this point */

        psDefn->ProjParm[0] = dfNatOriginLat;
        psDefn->ProjParmId[0] = ProjCenterLatGeoKey;
        psDefn->ProjParm[1] = dfNatOriginLong;
        psDefn->ProjParmId[1] = ProjCenterLongGeoKey;
        psDefn->ProjParm[4] = dfNatOriginScale;
        psDefn->ProjParmId[4] = ProjScaleAtNatOriginGeoKey;
        psDefn->ProjParm[5] = dfFalseEasting;
        psDefn->ProjParmId[5] = ProjFalseEastingGeoKey;
        psDefn->ProjParm[6] = dfFalseNorthing;
        psDefn->ProjParmId[6] = ProjFalseNorthingGeoKey;

        psDefn->nParms = 7;
        break;

/* -------------------------------------------------------------------- */
      case CT_Mercator:
/* -------------------------------------------------------------------- */
        if( GTIFKeyGetDOUBLE(psGTIF, ProjNatOriginLongGeoKey,
                       &dfNatOriginLong, 0, 1 ) == 0
            && GTIFKeyGetDOUBLE(psGTIF, ProjFalseOriginLongGeoKey,
                          &dfNatOriginLong, 0, 1 ) == 0
            && GTIFKeyGetDOUBLE(psGTIF, ProjCenterLongGeoKey,
                          &dfNatOriginLong, 0, 1 ) == 0 )
            dfNatOriginLong = 0.0;

        if( GTIFKeyGetDOUBLE(psGTIF, ProjNatOriginLatGeoKey,
                       &dfNatOriginLat, 0, 1 ) == 0
            && GTIFKeyGetDOUBLE(psGTIF, ProjFalseOriginLatGeoKey,
                          &dfNatOriginLat, 0, 1 ) == 0
            && GTIFKeyGetDOUBLE(psGTIF, ProjCenterLatGeoKey,
                          &dfNatOriginLat, 0, 1 ) == 0 )
            dfNatOriginLat = 0.0;


        bHaveSP1 = GTIFKeyGetDOUBLE(psGTIF, ProjStdParallel1GeoKey,
                              &dfStdParallel1, 0, 1 );

        bHaveNOS = GTIFKeyGetDOUBLE(psGTIF, ProjScaleAtNatOriginGeoKey,
                              &dfNatOriginScale, 0, 1 );

        /* Default scale only if dfStdParallel1 isn't defined either */
        if( !bHaveNOS && !bHaveSP1)
        {
            bHaveNOS = TRUE;
            dfNatOriginScale = 1.0;
        }

        /* notdef: should transform to decimal degrees at this point */

        psDefn->ProjParm[0] = dfNatOriginLat;
        psDefn->ProjParmId[0] = ProjNatOriginLatGeoKey;
        psDefn->ProjParm[1] = dfNatOriginLong;
        psDefn->ProjParmId[1] = ProjNatOriginLongGeoKey;
        if( bHaveSP1 )
        {
            psDefn->ProjParm[2] = dfStdParallel1;
            psDefn->ProjParmId[2] = ProjStdParallel1GeoKey;
        }
        if( bHaveNOS )
        {
            psDefn->ProjParm[4] = dfNatOriginScale;
            psDefn->ProjParmId[4] = ProjScaleAtNatOriginGeoKey;
        }
        psDefn->ProjParm[5] = dfFalseEasting;
        psDefn->ProjParmId[5] = ProjFalseEastingGeoKey;
        psDefn->ProjParm[6] = dfFalseNorthing;
        psDefn->ProjParmId[6] = ProjFalseNorthingGeoKey;

        psDefn->nParms = 7;
        break;

/* -------------------------------------------------------------------- */
      case CT_LambertConfConic_1SP:
      case CT_ObliqueStereographic:
      case CT_TransverseMercator:
      case CT_TransvMercator_SouthOriented:
/* -------------------------------------------------------------------- */
        if( GTIFKeyGetDOUBLE(psGTIF, ProjNatOriginLongGeoKey,
                       &dfNatOriginLong, 0, 1 ) == 0
            && GTIFKeyGetDOUBLE(psGTIF, ProjFalseOriginLongGeoKey,
                          &dfNatOriginLong, 0, 1 ) == 0
            && GTIFKeyGetDOUBLE(psGTIF, ProjCenterLongGeoKey,
                          &dfNatOriginLong, 0, 1 ) == 0 )
            dfNatOriginLong = 0.0;

        if( GTIFKeyGetDOUBLE(psGTIF, ProjNatOriginLatGeoKey,
                       &dfNatOriginLat, 0, 1 ) == 0
            && GTIFKeyGetDOUBLE(psGTIF, ProjFalseOriginLatGeoKey,
                          &dfNatOriginLat, 0, 1 ) == 0
            && GTIFKeyGetDOUBLE(psGTIF, ProjCenterLatGeoKey,
                          &dfNatOriginLat, 0, 1 ) == 0 )
            dfNatOriginLat = 0.0;

        if( GTIFKeyGetDOUBLE(psGTIF, ProjScaleAtNatOriginGeoKey,
                       &dfNatOriginScale, 0, 1 ) == 0
            /* See https://github.com/OSGeo/gdal/files/1665718/lasinfo.txt */
            && GTIFKeyGetDOUBLE(psGTIF, ProjScaleAtCenterGeoKey,
                       &dfNatOriginScale, 0, 1 ) == 0 )
            dfNatOriginScale = 1.0;

        /* notdef: should transform to decimal degrees at this point */

        psDefn->ProjParm[0] = dfNatOriginLat;
        psDefn->ProjParmId[0] = ProjNatOriginLatGeoKey;
        psDefn->ProjParm[1] = dfNatOriginLong;
        psDefn->ProjParmId[1] = ProjNatOriginLongGeoKey;
        psDefn->ProjParm[4] = dfNatOriginScale;
        psDefn->ProjParmId[4] = ProjScaleAtNatOriginGeoKey;
        psDefn->ProjParm[5] = dfFalseEasting;
        psDefn->ProjParmId[5] = ProjFalseEastingGeoKey;
        psDefn->ProjParm[6] = dfFalseNorthing;
        psDefn->ProjParmId[6] = ProjFalseNorthingGeoKey;

        psDefn->nParms = 7;
        break;

/* -------------------------------------------------------------------- */
      case CT_ObliqueMercator: /* hotine */
      case CT_HotineObliqueMercatorAzimuthCenter:
/* -------------------------------------------------------------------- */
        if( GTIFKeyGetDOUBLE(psGTIF, ProjNatOriginLongGeoKey,
                       &dfNatOriginLong, 0, 1 ) == 0
            && GTIFKeyGetDOUBLE(psGTIF, ProjFalseOriginLongGeoKey,
                          &dfNatOriginLong, 0, 1 ) == 0
            && GTIFKeyGetDOUBLE(psGTIF, ProjCenterLongGeoKey,
                          &dfNatOriginLong, 0, 1 ) == 0 )
            dfNatOriginLong = 0.0;

        if( GTIFKeyGetDOUBLE(psGTIF, ProjNatOriginLatGeoKey,
                       &dfNatOriginLat, 0, 1 ) == 0
            && GTIFKeyGetDOUBLE(psGTIF, ProjFalseOriginLatGeoKey,
                          &dfNatOriginLat, 0, 1 ) == 0
            && GTIFKeyGetDOUBLE(psGTIF, ProjCenterLatGeoKey,
                          &dfNatOriginLat, 0, 1 ) == 0 )
            dfNatOriginLat = 0.0;

        if( GTIFKeyGetDOUBLE(psGTIF, ProjAzimuthAngleGeoKey,
                       &dfAzimuth, 0, 1 ) == 0 )
            dfAzimuth = 0.0;

        if( GTIFKeyGetDOUBLE(psGTIF, ProjRectifiedGridAngleGeoKey,
                       &dfRectGridAngle, 0, 1 ) == 0 )
            dfRectGridAngle = 90.0;

        if( GTIFKeyGetDOUBLE(psGTIF, ProjScaleAtNatOriginGeoKey,
                       &dfNatOriginScale, 0, 1 ) == 0
            && GTIFKeyGetDOUBLE(psGTIF, ProjScaleAtCenterGeoKey,
                          &dfNatOriginScale, 0, 1 ) == 0 )
            dfNatOriginScale = 1.0;

        /* notdef: should transform to decimal degrees at this point */

        psDefn->ProjParm[0] = dfNatOriginLat;
        psDefn->ProjParmId[0] = ProjCenterLatGeoKey;
        psDefn->ProjParm[1] = dfNatOriginLong;
        psDefn->ProjParmId[1] = ProjCenterLongGeoKey;
        psDefn->ProjParm[2] = dfAzimuth;
        psDefn->ProjParmId[2] = ProjAzimuthAngleGeoKey;
        psDefn->ProjParm[3] = dfRectGridAngle;
        psDefn->ProjParmId[3] = ProjRectifiedGridAngleGeoKey;
        psDefn->ProjParm[4] = dfNatOriginScale;
        psDefn->ProjParmId[4] = ProjScaleAtCenterGeoKey;
        psDefn->ProjParm[5] = dfFalseEasting;
        psDefn->ProjParmId[5] = ProjFalseEastingGeoKey;
        psDefn->ProjParm[6] = dfFalseNorthing;
        psDefn->ProjParmId[6] = ProjFalseNorthingGeoKey;

        psDefn->nParms = 7;
        break;

/* -------------------------------------------------------------------- */
      case CT_ObliqueMercator_Laborde:
/* -------------------------------------------------------------------- */
        if( GTIFKeyGetDOUBLE(psGTIF, ProjNatOriginLongGeoKey,
                       &dfNatOriginLong, 0, 1 ) == 0
            && GTIFKeyGetDOUBLE(psGTIF, ProjFalseOriginLongGeoKey,
                          &dfNatOriginLong, 0, 1 ) == 0
            && GTIFKeyGetDOUBLE(psGTIF, ProjCenterLongGeoKey,
                          &dfNatOriginLong, 0, 1 ) == 0 )
            dfNatOriginLong = 0.0;

        if( GTIFKeyGetDOUBLE(psGTIF, ProjNatOriginLatGeoKey,
                       &dfNatOriginLat, 0, 1 ) == 0
            && GTIFKeyGetDOUBLE(psGTIF, ProjFalseOriginLatGeoKey,
                          &dfNatOriginLat, 0, 1 ) == 0
            && GTIFKeyGetDOUBLE(psGTIF, ProjCenterLatGeoKey,
                          &dfNatOriginLat, 0, 1 ) == 0 )
            dfNatOriginLat = 0.0;

        if( GTIFKeyGetDOUBLE(psGTIF, ProjAzimuthAngleGeoKey,
                       &dfAzimuth, 0, 1 ) == 0 )
            dfAzimuth = 0.0;

        if( GTIFKeyGetDOUBLE(psGTIF, ProjScaleAtNatOriginGeoKey,
                       &dfNatOriginScale, 0, 1 ) == 0
            && GTIFKeyGetDOUBLE(psGTIF, ProjScaleAtCenterGeoKey,
                          &dfNatOriginScale, 0, 1 ) == 0 )
            dfNatOriginScale = 1.0;

        /* notdef: should transform to decimal degrees at this point */

        psDefn->ProjParm[0] = dfNatOriginLat;
        psDefn->ProjParmId[0] = ProjCenterLatGeoKey;
        psDefn->ProjParm[1] = dfNatOriginLong;
        psDefn->ProjParmId[1] = ProjCenterLongGeoKey;
        psDefn->ProjParm[2] = dfAzimuth;
        psDefn->ProjParmId[2] = ProjAzimuthAngleGeoKey;
        psDefn->ProjParm[4] = dfNatOriginScale;
        psDefn->ProjParmId[4] = ProjScaleAtCenterGeoKey;
        psDefn->ProjParm[5] = dfFalseEasting;
        psDefn->ProjParmId[5] = ProjFalseEastingGeoKey;
        psDefn->ProjParm[6] = dfFalseNorthing;
        psDefn->ProjParmId[6] = ProjFalseNorthingGeoKey;

        psDefn->nParms = 7;
        break;

/* -------------------------------------------------------------------- */
      case CT_CassiniSoldner:
      case CT_Polyconic:
/* -------------------------------------------------------------------- */
        if( GTIFKeyGetDOUBLE(psGTIF, ProjNatOriginLongGeoKey,
                       &dfNatOriginLong, 0, 1 ) == 0
            && GTIFKeyGetDOUBLE(psGTIF, ProjFalseOriginLongGeoKey,
                          &dfNatOriginLong, 0, 1 ) == 0
            && GTIFKeyGetDOUBLE(psGTIF, ProjCenterLongGeoKey,
                          &dfNatOriginLong, 0, 1 ) == 0 )
            dfNatOriginLong = 0.0;

        if( GTIFKeyGetDOUBLE(psGTIF, ProjNatOriginLatGeoKey,
                       &dfNatOriginLat, 0, 1 ) == 0
            && GTIFKeyGetDOUBLE(psGTIF, ProjFalseOriginLatGeoKey,
                          &dfNatOriginLat, 0, 1 ) == 0
            && GTIFKeyGetDOUBLE(psGTIF, ProjCenterLatGeoKey,
                          &dfNatOriginLat, 0, 1 ) == 0 )
            dfNatOriginLat = 0.0;

        if( GTIFKeyGetDOUBLE(psGTIF, ProjScaleAtNatOriginGeoKey,
                       &dfNatOriginScale, 0, 1 ) == 0
            && GTIFKeyGetDOUBLE(psGTIF, ProjScaleAtCenterGeoKey,
                          &dfNatOriginScale, 0, 1 ) == 0 )
            dfNatOriginScale = 1.0;

        /* notdef: should transform to decimal degrees at this point */

        psDefn->ProjParm[0] = dfNatOriginLat;
        psDefn->ProjParmId[0] = ProjNatOriginLatGeoKey;
        psDefn->ProjParm[1] = dfNatOriginLong;
        psDefn->ProjParmId[1] = ProjNatOriginLongGeoKey;
        psDefn->ProjParm[4] = dfNatOriginScale;
        psDefn->ProjParmId[4] = ProjScaleAtNatOriginGeoKey;
        psDefn->ProjParm[5] = dfFalseEasting;
        psDefn->ProjParmId[5] = ProjFalseEastingGeoKey;
        psDefn->ProjParm[6] = dfFalseNorthing;
        psDefn->ProjParmId[6] = ProjFalseNorthingGeoKey;

        psDefn->nParms = 7;
        break;

/* -------------------------------------------------------------------- */
      case CT_AzimuthalEquidistant:
      case CT_MillerCylindrical:
      case CT_Gnomonic:
      case CT_LambertAzimEqualArea:
      case CT_Orthographic:
      case CT_NewZealandMapGrid:
/* -------------------------------------------------------------------- */
        if( GTIFKeyGetDOUBLE(psGTIF, ProjNatOriginLongGeoKey,
                       &dfNatOriginLong, 0, 1 ) == 0
            && GTIFKeyGetDOUBLE(psGTIF, ProjFalseOriginLongGeoKey,
                          &dfNatOriginLong, 0, 1 ) == 0
            && GTIFKeyGetDOUBLE(psGTIF, ProjCenterLongGeoKey,
                          &dfNatOriginLong, 0, 1 ) == 0 )
            dfNatOriginLong = 0.0;

        if( GTIFKeyGetDOUBLE(psGTIF, ProjNatOriginLatGeoKey,
                       &dfNatOriginLat, 0, 1 ) == 0
            && GTIFKeyGetDOUBLE(psGTIF, ProjFalseOriginLatGeoKey,
                          &dfNatOriginLat, 0, 1 ) == 0
            && GTIFKeyGetDOUBLE(psGTIF, ProjCenterLatGeoKey,
                          &dfNatOriginLat, 0, 1 ) == 0 )
            dfNatOriginLat = 0.0;

        /* notdef: should transform to decimal degrees at this point */

        psDefn->ProjParm[0] = dfNatOriginLat;
        psDefn->ProjParmId[0] = ProjCenterLatGeoKey;
        psDefn->ProjParm[1] = dfNatOriginLong;
        psDefn->ProjParmId[1] = ProjCenterLongGeoKey;
        psDefn->ProjParm[5] = dfFalseEasting;
        psDefn->ProjParmId[5] = ProjFalseEastingGeoKey;
        psDefn->ProjParm[6] = dfFalseNorthing;
        psDefn->ProjParmId[6] = ProjFalseNorthingGeoKey;

        psDefn->nParms = 7;
        break;

/* -------------------------------------------------------------------- */
      case CT_Equirectangular:
/* -------------------------------------------------------------------- */
        if( GTIFKeyGetDOUBLE(psGTIF, ProjNatOriginLongGeoKey,
                       &dfNatOriginLong, 0, 1 ) == 0
            && GTIFKeyGetDOUBLE(psGTIF, ProjFalseOriginLongGeoKey,
                          &dfNatOriginLong, 0, 1 ) == 0
            && GTIFKeyGetDOUBLE(psGTIF, ProjCenterLongGeoKey,
                          &dfNatOriginLong, 0, 1 ) == 0 )
            dfNatOriginLong = 0.0;

        if( GTIFKeyGetDOUBLE(psGTIF, ProjNatOriginLatGeoKey,
                       &dfNatOriginLat, 0, 1 ) == 0
            && GTIFKeyGetDOUBLE(psGTIF, ProjFalseOriginLatGeoKey,
                          &dfNatOriginLat, 0, 1 ) == 0
            && GTIFKeyGetDOUBLE(psGTIF, ProjCenterLatGeoKey,
                          &dfNatOriginLat, 0, 1 ) == 0 )
            dfNatOriginLat = 0.0;

        if( GTIFKeyGetDOUBLE(psGTIF, ProjStdParallel1GeoKey,
                       &dfStdParallel1, 0, 1 ) == 0 )
            dfStdParallel1 = 0.0;

        /* notdef: should transform to decimal degrees at this point */

        psDefn->ProjParm[0] = dfNatOriginLat;
        psDefn->ProjParmId[0] = ProjCenterLatGeoKey;
        psDefn->ProjParm[1] = dfNatOriginLong;
        psDefn->ProjParmId[1] = ProjCenterLongGeoKey;
        psDefn->ProjParm[2] = dfStdParallel1;
        psDefn->ProjParmId[2] = ProjStdParallel1GeoKey;
        psDefn->ProjParm[5] = dfFalseEasting;
        psDefn->ProjParmId[5] = ProjFalseEastingGeoKey;
        psDefn->ProjParm[6] = dfFalseNorthing;
        psDefn->ProjParmId[6] = ProjFalseNorthingGeoKey;

        psDefn->nParms = 7;
        break;

/* -------------------------------------------------------------------- */
      case CT_Robinson:
      case CT_Sinusoidal:
      case CT_VanDerGrinten:
/* -------------------------------------------------------------------- */
        if( GTIFKeyGetDOUBLE(psGTIF, ProjNatOriginLongGeoKey,
                       &dfNatOriginLong, 0, 1 ) == 0
            && GTIFKeyGetDOUBLE(psGTIF, ProjFalseOriginLongGeoKey,
                          &dfNatOriginLong, 0, 1 ) == 0
            && GTIFKeyGetDOUBLE(psGTIF, ProjCenterLongGeoKey,
                          &dfNatOriginLong, 0, 1 ) == 0 )
            dfNatOriginLong = 0.0;

        /* notdef: should transform to decimal degrees at this point */

        psDefn->ProjParm[1] = dfNatOriginLong;
        psDefn->ProjParmId[1] = ProjCenterLongGeoKey;
        psDefn->ProjParm[5] = dfFalseEasting;
        psDefn->ProjParmId[5] = ProjFalseEastingGeoKey;
        psDefn->ProjParm[6] = dfFalseNorthing;
        psDefn->ProjParmId[6] = ProjFalseNorthingGeoKey;

        psDefn->nParms = 7;
        break;

/* -------------------------------------------------------------------- */
      case CT_PolarStereographic:
/* -------------------------------------------------------------------- */
        if( GTIFKeyGetDOUBLE(psGTIF, ProjStraightVertPoleLongGeoKey,
                       &dfNatOriginLong, 0, 1 ) == 0
            && GTIFKeyGetDOUBLE(psGTIF, ProjNatOriginLongGeoKey,
                          &dfNatOriginLong, 0, 1 ) == 0
            && GTIFKeyGetDOUBLE(psGTIF, ProjFalseOriginLongGeoKey,
                          &dfNatOriginLong, 0, 1 ) == 0
            && GTIFKeyGetDOUBLE(psGTIF, ProjCenterLongGeoKey,
                          &dfNatOriginLong, 0, 1 ) == 0 )
            dfNatOriginLong = 0.0;

        if( GTIFKeyGetDOUBLE(psGTIF, ProjNatOriginLatGeoKey,
                       &dfNatOriginLat, 0, 1 ) == 0
            && GTIFKeyGetDOUBLE(psGTIF, ProjFalseOriginLatGeoKey,
                          &dfNatOriginLat, 0, 1 ) == 0
            && GTIFKeyGetDOUBLE(psGTIF, ProjCenterLatGeoKey,
                          &dfNatOriginLat, 0, 1 ) == 0 )
            dfNatOriginLat = 0.0;

        if( GTIFKeyGetDOUBLE(psGTIF, ProjScaleAtNatOriginGeoKey,
                       &dfNatOriginScale, 0, 1 ) == 0
            && GTIFKeyGetDOUBLE(psGTIF, ProjScaleAtCenterGeoKey,
                          &dfNatOriginScale, 0, 1 ) == 0 )
            dfNatOriginScale = 1.0;

        /* notdef: should transform to decimal degrees at this point */

        psDefn->ProjParm[0] = dfNatOriginLat;
        psDefn->ProjParmId[0] = ProjNatOriginLatGeoKey;;
        psDefn->ProjParm[1] = dfNatOriginLong;
        psDefn->ProjParmId[1] = ProjStraightVertPoleLongGeoKey;
        psDefn->ProjParm[4] = dfNatOriginScale;
        psDefn->ProjParmId[4] = ProjScaleAtNatOriginGeoKey;
        psDefn->ProjParm[5] = dfFalseEasting;
        psDefn->ProjParmId[5] = ProjFalseEastingGeoKey;
        psDefn->ProjParm[6] = dfFalseNorthing;
        psDefn->ProjParmId[6] = ProjFalseNorthingGeoKey;

        psDefn->nParms = 7;
        break;

/* -------------------------------------------------------------------- */
      case CT_LambertConfConic_2SP:
/* -------------------------------------------------------------------- */
        if( GTIFKeyGetDOUBLE(psGTIF, ProjStdParallel1GeoKey,
                       &dfStdParallel1, 0, 1 ) == 0 )
            dfStdParallel1 = 0.0;

        if( GTIFKeyGetDOUBLE(psGTIF, ProjStdParallel2GeoKey,
                       &dfStdParallel2, 0, 1 ) == 0 )
            dfStdParallel1 = 0.0;

        if( GTIFKeyGetDOUBLE(psGTIF, ProjNatOriginLongGeoKey,
                       &dfNatOriginLong, 0, 1 ) == 0
            && GTIFKeyGetDOUBLE(psGTIF, ProjFalseOriginLongGeoKey,
                          &dfNatOriginLong, 0, 1 ) == 0
            && GTIFKeyGetDOUBLE(psGTIF, ProjCenterLongGeoKey,
                          &dfNatOriginLong, 0, 1 ) == 0 )
            dfNatOriginLong = 0.0;

        if( GTIFKeyGetDOUBLE(psGTIF, ProjNatOriginLatGeoKey,
                       &dfNatOriginLat, 0, 1 ) == 0
            && GTIFKeyGetDOUBLE(psGTIF, ProjFalseOriginLatGeoKey,
                          &dfNatOriginLat, 0, 1 ) == 0
            && GTIFKeyGetDOUBLE(psGTIF, ProjCenterLatGeoKey,
                          &dfNatOriginLat, 0, 1 ) == 0 )
            dfNatOriginLat = 0.0;

        /* notdef: should transform to decimal degrees at this point */

        psDefn->ProjParm[0] = dfNatOriginLat;
        psDefn->ProjParmId[0] = ProjFalseOriginLatGeoKey;
        psDefn->ProjParm[1] = dfNatOriginLong;
        psDefn->ProjParmId[1] = ProjFalseOriginLongGeoKey;
        psDefn->ProjParm[2] = dfStdParallel1;
        psDefn->ProjParmId[2] = ProjStdParallel1GeoKey;
        psDefn->ProjParm[3] = dfStdParallel2;
        psDefn->ProjParmId[3] = ProjStdParallel2GeoKey;
        psDefn->ProjParm[5] = dfFalseEasting;
        psDefn->ProjParmId[5] = ProjFalseEastingGeoKey;
        psDefn->ProjParm[6] = dfFalseNorthing;
        psDefn->ProjParmId[6] = ProjFalseNorthingGeoKey;

        psDefn->nParms = 7;
        break;

/* -------------------------------------------------------------------- */
      case CT_AlbersEqualArea:
      case CT_EquidistantConic:
/* -------------------------------------------------------------------- */
        if( GTIFKeyGetDOUBLE(psGTIF, ProjStdParallel1GeoKey,
                       &dfStdParallel1, 0, 1 ) == 0 )
            dfStdParallel1 = 0.0;

        if( GTIFKeyGetDOUBLE(psGTIF, ProjStdParallel2GeoKey,
                       &dfStdParallel2, 0, 1 ) == 0 )
            dfStdParallel2 = 0.0;

        if( GTIFKeyGetDOUBLE(psGTIF, ProjNatOriginLongGeoKey,
                       &dfNatOriginLong, 0, 1 ) == 0
            && GTIFKeyGetDOUBLE(psGTIF, ProjFalseOriginLongGeoKey,
                          &dfNatOriginLong, 0, 1 ) == 0
            && GTIFKeyGetDOUBLE(psGTIF, ProjCenterLongGeoKey,
                          &dfNatOriginLong, 0, 1 ) == 0 )
            dfNatOriginLong = 0.0;

        if( GTIFKeyGetDOUBLE(psGTIF, ProjNatOriginLatGeoKey,
                       &dfNatOriginLat, 0, 1 ) == 0
            && GTIFKeyGetDOUBLE(psGTIF, ProjFalseOriginLatGeoKey,
                          &dfNatOriginLat, 0, 1 ) == 0
            && GTIFKeyGetDOUBLE(psGTIF, ProjCenterLatGeoKey,
                          &dfNatOriginLat, 0, 1 ) == 0 )
            dfNatOriginLat = 0.0;

        /* notdef: should transform to decimal degrees at this point */

        psDefn->ProjParm[0] = dfStdParallel1;
        psDefn->ProjParmId[0] = ProjStdParallel1GeoKey;
        psDefn->ProjParm[1] = dfStdParallel2;
        psDefn->ProjParmId[1] = ProjStdParallel2GeoKey;
        psDefn->ProjParm[2] = dfNatOriginLat;
        psDefn->ProjParmId[2] = ProjNatOriginLatGeoKey;
        psDefn->ProjParm[3] = dfNatOriginLong;
        psDefn->ProjParmId[3] = ProjNatOriginLongGeoKey;
        psDefn->ProjParm[5] = dfFalseEasting;
        psDefn->ProjParmId[5] = ProjFalseEastingGeoKey;
        psDefn->ProjParm[6] = dfFalseNorthing;
        psDefn->ProjParmId[6] = ProjFalseNorthingGeoKey;

        psDefn->nParms = 7;
        break;

/* -------------------------------------------------------------------- */
      case CT_CylindricalEqualArea:
/* -------------------------------------------------------------------- */
        if( GTIFKeyGetDOUBLE(psGTIF, ProjStdParallel1GeoKey,
                       &dfStdParallel1, 0, 1 ) == 0 )
            dfStdParallel1 = 0.0;

        if( GTIFKeyGetDOUBLE(psGTIF, ProjNatOriginLongGeoKey,
                       &dfNatOriginLong, 0, 1 ) == 0
            && GTIFKeyGetDOUBLE(psGTIF, ProjFalseOriginLongGeoKey,
                          &dfNatOriginLong, 0, 1 ) == 0
            && GTIFKeyGetDOUBLE(psGTIF, ProjCenterLongGeoKey,
                          &dfNatOriginLong, 0, 1 ) == 0 )
            dfNatOriginLong = 0.0;

        /* notdef: should transform to decimal degrees at this point */

        psDefn->ProjParm[0] = dfStdParallel1;
        psDefn->ProjParmId[0] = ProjStdParallel1GeoKey;
        psDefn->ProjParm[1] = dfNatOriginLong;
        psDefn->ProjParmId[1] = ProjNatOriginLongGeoKey;
        psDefn->ProjParm[5] = dfFalseEasting;
        psDefn->ProjParmId[5] = ProjFalseEastingGeoKey;
        psDefn->ProjParm[6] = dfFalseNorthing;
        psDefn->ProjParmId[6] = ProjFalseNorthingGeoKey;

        psDefn->nParms = 7;
        break;
    }

/* -------------------------------------------------------------------- */
/*      Normalize any linear parameters into meters.  In GeoTIFF        */
/*      the linear projection parameter tags are normally in the        */
/*      units of the coordinate system described.                       */
/* -------------------------------------------------------------------- */
    for( iParam = 0; iParam < psDefn->nParms; iParam++ )
    {
        switch( psDefn->ProjParmId[iParam] )
        {
          case ProjFalseEastingGeoKey:
          case ProjFalseNorthingGeoKey:
          case ProjFalseOriginEastingGeoKey:
          case ProjFalseOriginNorthingGeoKey:
          case ProjCenterEastingGeoKey:
          case ProjCenterNorthingGeoKey:
            if( psDefn->UOMLengthInMeters != 0
                && psDefn->UOMLengthInMeters != 1.0 )
            {
                psDefn->ProjParm[iParam] *= psDefn->UOMLengthInMeters;
            }
            break;

          default:
            break;
        }
    }
}

/************************************************************************/
/*                            GTIFGetDefn()                             */
/************************************************************************/

/**
@param psGTIF GeoTIFF information handle as returned by GTIFNew.
@param psDefn Pointer to an existing GTIFDefn structure allocated by GTIFAllocDefn().

@return TRUE if the function has been successful, otherwise FALSE.

This function reads the coordinate system definition from a GeoTIFF file,
and <i>normalizes</i> it into a set of component information using
definitions from the EPSG database as provided by the PROJ library.
This function is intended to simplify correct support for
reading files with defined PCS (Projected Coordinate System) codes that
wouldn't otherwise be directly known by application software by reducing
it to the underlying projection method, parameters, datum, ellipsoid,
prime meridian and units.<p>

The application should pass a pointer to an existing uninitialized
GTIFDefn structure, and GTIFGetDefn() will fill it in.  The function
currently always returns TRUE but in the future will return FALSE if
the database is not found.  In any event, all geokeys actually found in the
file will be copied into the GTIFDefn.  However, if the database isn't
found, codes implied by other codes will not be set properly.<p>

GTIFGetDefn() will not generally work if the EPSG derived database cannot
be found.<p>

The normalization methodology operates by fetching tags from the GeoTIFF
file, and then setting all other tags implied by them in the structure.  The
implied relationships are worked out by reading definitions from the
various EPSG derived database tables.<p>

For instance, if a PCS (ProjectedCSTypeGeoKey) is found in the GeoTIFF file
this code is used to lookup a record in the database.
For example given the PCS 26746 we can find the name
(NAD27 / California zone VI), the GCS 4257 (NAD27), and the ProjectionCode
10406 (California CS27 zone VI).  The GCS, and ProjectionCode can in turn
be looked up in other tables until all the details of units, ellipsoid,
prime meridian, datum, projection (LambertConfConic_2SP) and projection
parameters are established.  A full listgeo dump of a file
for this result might look like the following, all based on a single PCS
value:<p>

<pre>
% listgeo -norm ~/data/geotiff/pci_eg/spaf27.tif
Geotiff_Information:
   Version: 1
   Key_Revision: 1.0
   Tagged_Information:
      ModelTiepointTag (2,3):
         0                0                0
         1577139.71       634349.176       0
      ModelPixelScaleTag (1,3):
         195.509321       198.32184        0
      End_Of_Tags.
   Keyed_Information:
      GTModelTypeGeoKey (Short,1): ModelTypeProjected
      GTRasterTypeGeoKey (Short,1): RasterPixelIsArea
      ProjectedCSTypeGeoKey (Short,1): PCS_NAD27_California_VI
      End_Of_Keys.
   End_Of_Geotiff.

PCS = 26746 (NAD27 / California zone VI)
Projection = 10406 (California CS27 zone VI)
Projection Method: CT_LambertConfConic_2SP
   ProjStdParallel1GeoKey: 33.883333
   ProjStdParallel2GeoKey: 32.766667
   ProjFalseOriginLatGeoKey: 32.166667
   ProjFalseOriginLongGeoKey: -116.233333
   ProjFalseEastingGeoKey: 609601.219202
   ProjFalseNorthingGeoKey: 0.000000
GCS: 4267/NAD27
Datum: 6267/North American Datum 1927
Ellipsoid: 7008/Clarke 1866 (6378206.40,6356583.80)
Prime Meridian: 8901/Greenwich (0.000000)
Projection Linear Units: 9003/US survey foot (0.304801m)
</pre>

Note that GTIFGetDefn() does not inspect or return the tiepoints and scale.
This must be handled separately as it normally would.  It is intended to
simplify capture and normalization of the coordinate system definition.
Note that GTIFGetDefn() also does the following things:

<ol>
<li> Convert all angular values to decimal degrees.
<li> Convert all linear values to meters.
<li> Return the linear units and conversion to meters for the tiepoints and
scale (though the tiepoints and scale remain in their native units).
<li> When reading projection parameters a variety of differences between
different GeoTIFF generators are handled, and a normalized set of parameters
for each projection are always returned.
</ol>

Code fields in the GTIFDefn are filled with KvUserDefined if there is not
value to assign.  The parameter lists for each of the underlying projection
transform methods can be found at the
<a href="http://www.remotesensing.org/geotiff/proj_list">Projections</a>
page.  Note that nParms will be set based on the maximum parameter used.
Some of the parameters may not be used in which case the
GTIFDefn::ProjParmId[] will
be zero.  This is done to retain correspondence to the EPSG parameter
numbering scheme.<p>

The
<a href="http://www.remotesensing.org/cgi-bin/cvsweb.cgi/~checkout~/osrs/geotiff/libgeotiff/geotiff_proj4.c">geotiff_proj4.c</a> module distributed with libgeotiff can
be used as an example of code that converts a GTIFDefn into another projection
system.<p>

@see GTIFKeySet()

*/

int GTIFGetDefn( GTIF * psGTIF, GTIFDefn * psDefn )

{
    int		i;
    short	nGeogUOMLinear;
    double	dfInvFlattening;

    if( !GTIFGetPROJContext(psGTIF, TRUE, NULL) )
    {
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Initially we default all the information we can.                */
/* -------------------------------------------------------------------- */
    psDefn->DefnSet = 1;
    psDefn->Model = KvUserDefined;
    psDefn->PCS = KvUserDefined;
    psDefn->GCS = KvUserDefined;
    psDefn->UOMLength = KvUserDefined;
    psDefn->UOMLengthInMeters = 1.0;
    psDefn->UOMAngle = KvUserDefined;
    psDefn->UOMAngleInDegrees = 1.0;
    psDefn->Datum = KvUserDefined;
    psDefn->Ellipsoid = KvUserDefined;
    psDefn->SemiMajor = 0.0;
    psDefn->SemiMinor = 0.0;
    psDefn->PM = KvUserDefined;
    psDefn->PMLongToGreenwich = 0.0;
#if !defined(GEO_NORMALIZE_DISABLE_TOWGS84)
    psDefn->TOWGS84Count = 0;
    memset( psDefn->TOWGS84, 0, sizeof(psDefn->TOWGS84) );
#endif

    psDefn->ProjCode = KvUserDefined;
    psDefn->Projection = KvUserDefined;
    psDefn->CTProjection = KvUserDefined;

    psDefn->nParms = 0;
    for( i = 0; i < MAX_GTIF_PROJPARMS; i++ )
    {
        psDefn->ProjParm[i] = 0.0;
        psDefn->ProjParmId[i] = 0;
    }

    psDefn->MapSys = KvUserDefined;
    psDefn->Zone = 0;

/* -------------------------------------------------------------------- */
/*      Do we have any geokeys?                                         */
/* -------------------------------------------------------------------- */
    {
        int     nKeyCount = 0;
        int     anVersion[3];
        GTIFDirectoryInfo( psGTIF, anVersion, &nKeyCount );

        if( nKeyCount == 0 )
        {
            psDefn->DefnSet = 0;
            return FALSE;
        }
    }

/* -------------------------------------------------------------------- */
/*	Try to get the overall model type.				*/
/* -------------------------------------------------------------------- */
    GTIFKeyGetSSHORT(psGTIF,GTModelTypeGeoKey,&(psDefn->Model));

/* -------------------------------------------------------------------- */
/*	Extract the Geog units.  					*/
/* -------------------------------------------------------------------- */
    nGeogUOMLinear = 9001; /* Linear_Meter */
    if( GTIFKeyGetSSHORT(psGTIF, GeogLinearUnitsGeoKey, &nGeogUOMLinear) == 1 )
    {
        psDefn->UOMLength = nGeogUOMLinear;
    }

/* -------------------------------------------------------------------- */
/*      Try to get a PCS.                                               */
/* -------------------------------------------------------------------- */
    if( GTIFKeyGetSSHORT(psGTIF,ProjectedCSTypeGeoKey, &(psDefn->PCS)) == 1
        && psDefn->PCS != KvUserDefined )
    {
        /*
         * Translate this into useful information.
         */
        GTIFGetPCSInfoEx( psGTIF->pj_context,
                          psDefn->PCS, NULL, &(psDefn->ProjCode),
                          &(psDefn->UOMLength), &(psDefn->GCS) );
    }

/* -------------------------------------------------------------------- */
/*       If we have the PCS code, but didn't find it in the database    */
/*      (likely because we can't find them) we will try some ``jiffy    */
/*      rules'' for UTM and state plane.                                */
/* -------------------------------------------------------------------- */
    if( psDefn->PCS != KvUserDefined && psDefn->ProjCode == KvUserDefined )
    {
        int	nMapSys, nZone;
        int	nGCS = psDefn->GCS;

        nMapSys = GTIFPCSToMapSys( psDefn->PCS, &nGCS, &nZone );
        if( nMapSys != KvUserDefined )
        {
            psDefn->ProjCode = (short) GTIFMapSysToProj( nMapSys, nZone );
            psDefn->GCS = (short) nGCS;
        }
    }

/* -------------------------------------------------------------------- */
/*      If the Proj_ code is specified directly, use that.              */
/* -------------------------------------------------------------------- */
    if( psDefn->ProjCode == KvUserDefined )
        GTIFKeyGetSSHORT(psGTIF, ProjectionGeoKey, &(psDefn->ProjCode));

    if( psDefn->ProjCode != KvUserDefined )
    {
        /*
         * We have an underlying projection transformation value.  Look
         * this up.  For a PCS of ``WGS 84 / UTM 11'' the transformation
         * would be Transverse Mercator, with a particular set of options.
         * The nProjTRFCode itself would correspond to the name
         * ``UTM zone 11N'', and doesn't include datum info.
         */
        GTIFGetProjTRFInfoEx( psGTIF->pj_context,
                              psDefn->ProjCode, NULL, &(psDefn->Projection),
                              psDefn->ProjParm );

        /*
         * Set the GeoTIFF identity of the parameters.
         */
        psDefn->CTProjection = (short)
            EPSGProjMethodToCTProjMethod( psDefn->Projection, FALSE );

        SetGTParamIds( EPSGProjMethodToCTProjMethod(psDefn->Projection, TRUE),
                      psDefn->Projection,
                      psDefn->ProjParmId, NULL);
        psDefn->nParms = 7;
    }

/* -------------------------------------------------------------------- */
/*      Try to get a GCS.  If found, it will override any implied by    */
/*      the PCS.                                                        */
/* -------------------------------------------------------------------- */
    GTIFKeyGetSSHORT(psGTIF, GeographicTypeGeoKey, &(psDefn->GCS));
    if( psDefn->GCS < 1 || psDefn->GCS >= KvUserDefined )
        psDefn->GCS = KvUserDefined;

/* -------------------------------------------------------------------- */
/*      Derive the datum, and prime meridian from the GCS.              */
/* -------------------------------------------------------------------- */
    if( psDefn->GCS != KvUserDefined )
    {
        GTIFGetGCSInfoEx( psGTIF->pj_context,
                          psDefn->GCS, NULL, &(psDefn->Datum), &(psDefn->PM),
                          &(psDefn->UOMAngle) );
    }

/* -------------------------------------------------------------------- */
/*      Handle the GCS angular units.  GeogAngularUnitsGeoKey           */
/*      overrides the GCS or PCS setting.                               */
/* -------------------------------------------------------------------- */
    GTIFKeyGetSSHORT(psGTIF, GeogAngularUnitsGeoKey, &(psDefn->UOMAngle));
    if( psDefn->UOMAngle != KvUserDefined )
    {
        GTIFGetUOMAngleInfoEx( psGTIF->pj_context,
                               psDefn->UOMAngle, NULL,
                               &(psDefn->UOMAngleInDegrees) );
    }

/* -------------------------------------------------------------------- */
/*      Check for a datum setting, and then use the datum to derive     */
/*      an ellipsoid.                                                   */
/* -------------------------------------------------------------------- */
    GTIFKeyGetSSHORT(psGTIF, GeogGeodeticDatumGeoKey, &(psDefn->Datum));

    if( psDefn->Datum != KvUserDefined )
    {
        GTIFGetDatumInfoEx( psGTIF->pj_context,
                            psDefn->Datum, NULL, &(psDefn->Ellipsoid) );
    }

/* -------------------------------------------------------------------- */
/*      Check for an explicit ellipsoid.  Use the ellipsoid to          */
/*      derive the ellipsoid characteristics, if possible.              */
/* -------------------------------------------------------------------- */
    GTIFKeyGetSSHORT(psGTIF, GeogEllipsoidGeoKey, &(psDefn->Ellipsoid));

    if( psDefn->Ellipsoid != KvUserDefined )
    {
        GTIFGetEllipsoidInfoEx( psGTIF->pj_context,
                                psDefn->Ellipsoid, NULL,
                                &(psDefn->SemiMajor), &(psDefn->SemiMinor) );
    }

/* -------------------------------------------------------------------- */
/*      Check for overridden ellipsoid parameters.  It would be nice    */
/*      to warn if they conflict with provided information, but for     */
/*      now we just override.                                           */
/* -------------------------------------------------------------------- */
    CPL_IGNORE_RET_VAL_INT(GTIFKeyGetDOUBLE(psGTIF, GeogSemiMajorAxisGeoKey, &(psDefn->SemiMajor), 0, 1 ));
    CPL_IGNORE_RET_VAL_INT(GTIFKeyGetDOUBLE(psGTIF, GeogSemiMinorAxisGeoKey, &(psDefn->SemiMinor), 0, 1 ));

    if( GTIFKeyGetDOUBLE(psGTIF, GeogInvFlatteningGeoKey, &dfInvFlattening,
                   0, 1 ) == 1 )
    {
        if( dfInvFlattening != 0.0 )
            psDefn->SemiMinor =
                psDefn->SemiMajor * (1 - 1.0/dfInvFlattening);
        else
            psDefn->SemiMinor = psDefn->SemiMajor;
    }

/* -------------------------------------------------------------------- */
/*      Get the prime meridian info.                                    */
/* -------------------------------------------------------------------- */
    GTIFKeyGetSSHORT(psGTIF, GeogPrimeMeridianGeoKey, &(psDefn->PM));

    if( psDefn->PM != KvUserDefined )
    {
        GTIFGetPMInfoEx( psGTIF->pj_context,
                         psDefn->PM, NULL, &(psDefn->PMLongToGreenwich) );
    }
    else
    {
        CPL_IGNORE_RET_VAL_INT(GTIFKeyGetDOUBLE(psGTIF, GeogPrimeMeridianLongGeoKey,
                   &(psDefn->PMLongToGreenwich), 0, 1 ));

        psDefn->PMLongToGreenwich =
            GTIFAngleToDD( psDefn->PMLongToGreenwich,
                           psDefn->UOMAngle );
    }

/* -------------------------------------------------------------------- */
/*      Get the TOWGS84 parameters.                                     */
/* -------------------------------------------------------------------- */
#if !defined(GEO_NORMALIZE_DISABLE_TOWGS84)
    psDefn->TOWGS84Count =
        (short)GTIFKeyGetDOUBLE(psGTIF, GeogTOWGS84GeoKey, psDefn->TOWGS84, 0, 7 );
#endif

/* -------------------------------------------------------------------- */
/*      Have the projection units of measure been overridden?  We       */
/*      should likely be doing something about angular units too,       */
/*      but these are very rarely not decimal degrees for actual        */
/*      file coordinates.                                               */
/* -------------------------------------------------------------------- */
    GTIFKeyGetSSHORT(psGTIF,ProjLinearUnitsGeoKey,&(psDefn->UOMLength));

    if( psDefn->UOMLength != KvUserDefined )
    {
        GTIFGetUOMLengthInfoEx( psGTIF->pj_context,
                                psDefn->UOMLength, NULL,
                                &(psDefn->UOMLengthInMeters) );
    }
    else
    {
        CPL_IGNORE_RET_VAL_INT(GTIFKeyGetDOUBLE(psGTIF,ProjLinearUnitSizeGeoKey,&(psDefn->UOMLengthInMeters),0,1));
    }

/* -------------------------------------------------------------------- */
/*      Handle a variety of user defined transform types.               */
/* -------------------------------------------------------------------- */
    if( GTIFKeyGetSSHORT(psGTIF,ProjCoordTransGeoKey,
                   &(psDefn->CTProjection)) == 1)
    {
        GTIFFetchProjParms( psGTIF, psDefn );
    }

/* -------------------------------------------------------------------- */
/*      Try to set the zoned map system information.                    */
/* -------------------------------------------------------------------- */
    psDefn->MapSys = GTIFProjToMapSys( psDefn->ProjCode, &(psDefn->Zone) );

/* -------------------------------------------------------------------- */
/*      If this is UTM, and we were unable to extract the projection    */
/*      parameters from the database just set them directly now,        */
/*      since it's pretty easy, and a common case.                      */
/* -------------------------------------------------------------------- */
    if( (psDefn->MapSys == MapSys_UTM_North
         || psDefn->MapSys == MapSys_UTM_South)
        && psDefn->CTProjection == KvUserDefined )
    {
        psDefn->CTProjection = CT_TransverseMercator;
        psDefn->nParms = 7;
        psDefn->ProjParmId[0] = ProjNatOriginLatGeoKey;
        psDefn->ProjParm[0] = 0.0;

        psDefn->ProjParmId[1] = ProjNatOriginLongGeoKey;
        psDefn->ProjParm[1] = psDefn->Zone*6 - 183.0;

        psDefn->ProjParmId[4] = ProjScaleAtNatOriginGeoKey;
        psDefn->ProjParm[4] = 0.9996;

        psDefn->ProjParmId[5] = ProjFalseEastingGeoKey;
        psDefn->ProjParm[5] = 500000.0;

        psDefn->ProjParmId[6] = ProjFalseNorthingGeoKey;

        if( psDefn->MapSys == MapSys_UTM_North )
            psDefn->ProjParm[6] = 0.0;
        else
            psDefn->ProjParm[6] = 10000000.0;
    }

    return TRUE;
}

/************************************************************************/
/*                            GTIFDecToDMS()                            */
/*                                                                      */
/*      Convenient function to translate decimal degrees to DMS         */
/*      format for reporting to a user.                                 */
/************************************************************************/

const char *GTIFDecToDMS( double dfAngle, const char * pszAxis,
                          int nPrecision )

{
    int		nDegrees, nMinutes;
    double	dfSeconds;
    char	szFormat[30];
    static char szBuffer[50];
    const char	*pszHemisphere = NULL;
    double	dfRound;
    int		i;

    if( !(dfAngle >= -360 && dfAngle <= 360) )
        return "";

    dfRound = 0.5/60;
    for( i = 0; i < nPrecision; i++ )
        dfRound = dfRound * 0.1;

    nDegrees = (int) ABS(dfAngle);
    nMinutes = (int) ((ABS(dfAngle) - nDegrees) * 60 + dfRound);
    if( nMinutes == 60 )
    {
        nDegrees ++;
        nMinutes = 0;
    }
    dfSeconds = ABS((ABS(dfAngle) * 3600 - nDegrees*3600 - nMinutes*60));

    if( EQUAL(pszAxis,"Long") && dfAngle < 0.0 )
        pszHemisphere = "W";
    else if( EQUAL(pszAxis,"Long") )
        pszHemisphere = "E";
    else if( dfAngle < 0.0 )
        pszHemisphere = "S";
    else
        pszHemisphere = "N";

    sprintf( szFormat, "%%3dd%%2d\'%%%d.%df\"%s",
             nPrecision+3, nPrecision, pszHemisphere );
    sprintf( szBuffer, szFormat, nDegrees, nMinutes, dfSeconds );

    return szBuffer;
}

/************************************************************************/
/*                           GTIFPrintDefn()                            */
/*                                                                      */
/*      Report the contents of a GTIFDefn structure ... mostly for      */
/*      debugging.                                                      */
/************************************************************************/

void GTIFPrintDefnEx( GTIF *psGTIF, GTIFDefn * psDefn, FILE * fp )

{
    GTIFGetPROJContext(psGTIF, TRUE, NULL);

/* -------------------------------------------------------------------- */
/*      Do we have anything to report?                                  */
/* -------------------------------------------------------------------- */
    if( !psDefn->DefnSet )
    {
        fprintf( fp, "No GeoKeys found.\n" );
        return;
    }

/* -------------------------------------------------------------------- */
/*      Get the PCS name if possible.                                   */
/* -------------------------------------------------------------------- */
    if( psDefn->PCS != KvUserDefined )
    {
        char	*pszPCSName = NULL;

        if( psGTIF->pj_context )
        {
            GTIFGetPCSInfoEx( psGTIF->pj_context,
                              psDefn->PCS, &pszPCSName, NULL, NULL, NULL );
        }
        if( pszPCSName == NULL )
            pszPCSName = CPLStrdup("name unknown");

        fprintf( fp, "PCS = %d (%s)\n", psDefn->PCS, pszPCSName );
        CPLFree( pszPCSName );
    }

/* -------------------------------------------------------------------- */
/*	Dump the projection code if possible.				*/
/* -------------------------------------------------------------------- */
    if( psDefn->ProjCode != KvUserDefined )
    {
        char	*pszTRFName = NULL;

        if( psGTIF->pj_context )
        {
            GTIFGetProjTRFInfoEx( psGTIF->pj_context,
                                  psDefn->ProjCode, &pszTRFName, NULL, NULL );
        }
        if( pszTRFName == NULL )
            pszTRFName = CPLStrdup("");

        fprintf( fp, "Projection = %d (%s)\n",
                 psDefn->ProjCode, pszTRFName );

        CPLFree( pszTRFName );
    }

/* -------------------------------------------------------------------- */
/*      Try to dump the projection method name, and parameters if possible.*/
/* -------------------------------------------------------------------- */
    if( psDefn->CTProjection != KvUserDefined )
    {
        const char *pszProjectionMethodName =
            GTIFValueNameEx(psGTIF,
                            ProjCoordTransGeoKey,
                            psDefn->CTProjection);
        int     i;

        if( pszProjectionMethodName == NULL )
            pszProjectionMethodName = "(unknown)";

        fprintf( fp, "Projection Method: %s\n", pszProjectionMethodName );

        for( i = 0; i < psDefn->nParms; i++ )
        {
            char* pszName;
            if( psDefn->ProjParmId[i] == 0 )
                continue;

            pszName = GTIFKeyName((geokey_t) psDefn->ProjParmId[i]);
            if( pszName == NULL )
                pszName = "(unknown)";

            if( i < 4 )
            {
                char	*pszAxisName;

                if( strstr(pszName,"Long") != NULL )
                    pszAxisName = "Long";
                else if( strstr(pszName,"Lat") != NULL )
                    pszAxisName = "Lat";
                else
                    pszAxisName = "?";

                fprintf( fp, "   %s: %f (%s)\n",
                         pszName, psDefn->ProjParm[i],
                         GTIFDecToDMS( psDefn->ProjParm[i], pszAxisName, 2 ) );
            }
            else if( i == 4 )
                fprintf( fp, "   %s: %f\n", pszName, psDefn->ProjParm[i] );
            else
                fprintf( fp, "   %s: %f m\n", pszName, psDefn->ProjParm[i] );
        }
    }

/* -------------------------------------------------------------------- */
/*      Report the GCS name, and number.                                */
/* -------------------------------------------------------------------- */
    if( psDefn->GCS != KvUserDefined )
    {
        char	*pszName = NULL;

        if( psGTIF->pj_context )
        {
            GTIFGetGCSInfoEx( psGTIF->pj_context,
                              psDefn->GCS, &pszName, NULL, NULL, NULL );
        }
        if( pszName == NULL )
            pszName = CPLStrdup("(unknown)");

        fprintf( fp, "GCS: %d/%s\n", psDefn->GCS, pszName );
        CPLFree( pszName );
    }

/* -------------------------------------------------------------------- */
/*      Report the datum name.                                          */
/* -------------------------------------------------------------------- */
    if( psDefn->Datum != KvUserDefined )
    {
        char	*pszName = NULL;

        if( psGTIF->pj_context )
        {
            GTIFGetDatumInfoEx( psGTIF->pj_context,
                                psDefn->Datum, &pszName, NULL );
        }
        if( pszName == NULL )
            pszName = CPLStrdup("(unknown)");

        fprintf( fp, "Datum: %d/%s\n", psDefn->Datum, pszName );
        CPLFree( pszName );
    }

/* -------------------------------------------------------------------- */
/*      Report the ellipsoid.                                           */
/* -------------------------------------------------------------------- */
    if( psDefn->Ellipsoid != KvUserDefined )
    {
        char	*pszName = NULL;

        if( psGTIF->pj_context )
        {
            GTIFGetEllipsoidInfoEx( psGTIF->pj_context,
                                    psDefn->Ellipsoid, &pszName, NULL, NULL );
        }
        if( pszName == NULL )
            pszName = CPLStrdup("(unknown)");

        fprintf( fp, "Ellipsoid: %d/%s (%.2f,%.2f)\n",
                 psDefn->Ellipsoid, pszName,
                 psDefn->SemiMajor, psDefn->SemiMinor );
        CPLFree( pszName );
    }

/* -------------------------------------------------------------------- */
/*      Report the prime meridian.                                      */
/* -------------------------------------------------------------------- */
    if( psDefn->PM != KvUserDefined )
    {
        char	*pszName = NULL;

        if( psGTIF->pj_context )
        {
            GTIFGetPMInfoEx( psGTIF->pj_context,
                             psDefn->PM, &pszName, NULL );
        }

        if( pszName == NULL )
            pszName = CPLStrdup("(unknown)");

        fprintf( fp, "Prime Meridian: %d/%s (%f/%s)\n",
                 psDefn->PM, pszName,
                 psDefn->PMLongToGreenwich,
                 GTIFDecToDMS( psDefn->PMLongToGreenwich, "Long", 2 ) );
        CPLFree( pszName );
    }

/* -------------------------------------------------------------------- */
/*      Report TOWGS84 parameters.                                      */
/* -------------------------------------------------------------------- */
#if !defined(GEO_NORMALIZE_DISABLE_TOWGS84)
    if( psDefn->TOWGS84Count > 0 )
    {
        int i;

        fprintf( fp, "TOWGS84: " );

        for( i = 0; i < psDefn->TOWGS84Count; i++ )
        {
            if( i > 0 )
                fprintf( fp, "," );
            fprintf( fp, "%g", psDefn->TOWGS84[i] );
        }

        fprintf( fp, "\n" );
    }
#endif

/* -------------------------------------------------------------------- */
/*      Report the projection units of measure (currently just          */
/*      linear).                                                        */
/* -------------------------------------------------------------------- */
    if( psDefn->UOMLength != KvUserDefined )
    {
        char	*pszName = NULL;

        if( psGTIF->pj_context )
        {
            GTIFGetUOMLengthInfoEx(
                psGTIF->pj_context, psDefn->UOMLength, &pszName, NULL );
        }
        if( pszName == NULL )
            pszName = CPLStrdup( "(unknown)" );

        fprintf( fp, "Projection Linear Units: %d/%s (%fm)\n",
                 psDefn->UOMLength, pszName, psDefn->UOMLengthInMeters );
        CPLFree( pszName );
    }
    else
    {
        fprintf( fp, "Projection Linear Units: User-Defined (%fm)\n",
                 psDefn->UOMLengthInMeters );
    }
}

void GTIFPrintDefn( GTIFDefn * psDefn, FILE * fp )
{
    GTIF *psGTIF = GTIFNew(NULL);
    if( psGTIF )
    {
        GTIFPrintDefnEx(psGTIF, psDefn, fp);
        GTIFFree(psGTIF);
    }
}

/************************************************************************/
/*                           GTIFFreeMemory()                           */
/*                                                                      */
/*      Externally visible function to free memory allocated within     */
/*      geo_normalize.c.                                                */
/************************************************************************/

void GTIFFreeMemory( char * pMemory )

{
    if( pMemory != NULL )
        VSIFree( pMemory );
}

/************************************************************************/
/*                           GTIFAllocDefn()                            */
/*                                                                      */
/*      This allocates a GTIF structure in such a way that the          */
/*      calling application doesn't need to know the size and           */
/*      initializes it appropriately.                                   */
/************************************************************************/

GTIFDefn *GTIFAllocDefn()
{
    return (GTIFDefn *) CPLCalloc(sizeof(GTIFDefn),1);
}

/************************************************************************/
/*                            GTIFFreeDefn()                            */
/*                                                                      */
/*      Free a GTIF structure allocated by GTIFAllocDefn().             */
/************************************************************************/

void GTIFFreeDefn( GTIFDefn *defn )
{
    VSIFree( defn );
}

/************************************************************************/
/*                       GTIFAttachPROJContext()                        */
/*                                                                      */
/*      Attach an existing PROJ context to the GTIF handle, but         */
/*      ownership of the context remains to the caller.                 */
/************************************************************************/

void GTIFAttachPROJContext( GTIF *psGTIF, void* pjContext )
{
    if( psGTIF->own_pj_context )
    {
        proj_context_destroy(psGTIF->pj_context);
    }
    psGTIF->own_pj_context = FALSE;
    psGTIF->pj_context = (PJ_CONTEXT*) pjContext;
}

/************************************************************************/
/*                         GTIFGetPROJContext()                         */
/*                                                                      */
/*      Return the PROJ context attached to the GTIF handle.            */
/*      If it has not yet been instantiated and instantiateIfNeeded=TRUE*/
/*      then, it will be instantiated (and owned by GTIF handle).       */
/************************************************************************/

void *GTIFGetPROJContext( GTIF *psGTIF, int instantiateIfNeeded,
                          int* out_gtif_own_pj_context )
{
    if( psGTIF->pj_context || !instantiateIfNeeded )
    {
        if( out_gtif_own_pj_context )
        {
            *out_gtif_own_pj_context = psGTIF->own_pj_context;
        }
        return psGTIF->pj_context;
    }
    psGTIF->pj_context = proj_context_create();
    psGTIF->own_pj_context = psGTIF->pj_context != NULL;
    if( out_gtif_own_pj_context )
    {
        *out_gtif_own_pj_context = psGTIF->own_pj_context;
    }
    return psGTIF->pj_context;
}


void GTIFDeaccessCSV( void )
{
    /* No operation */
}

#ifndef GDAL_COMPILATION
void SetCSVFilenameHook( const char *(*CSVFileOverride)(const char *) )
{
    (void)CSVFileOverride;
    /* No operation */
}
#endif
