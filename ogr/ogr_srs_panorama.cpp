/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  OGRSpatialReference translation to/from "Panorama" GIS
 *           georeferencing information (also know as GIS "Integration").
 * Author:   Andrey Kiselev, dron@remotesensing.org
 *
 ******************************************************************************
 * Copyright (c) 2005, Andrey Kiselev <dron@remotesensing.org>
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
 ****************************************************************************/

#include "ogr_spatialref.h"
#include "cpl_conv.h"
#include "cpl_csv.h"

CPL_CVSID("$Id$");

#define TO_DEGREES (180.0 / 3.14159265358979323846)
#define TO_RADIANS (3.14159265358979323846 / 180.0)

/************************************************************************/
/*  "Panorama" projection codes.                                        */
/************************************************************************/

#define NONE    -1L
#define TM      1L      // Gauss-Kruger (Transverse Mercator)
#define LAEA    4L      // Lambert Azimuthal Equal Area
#define STEREO  5L      // Stereographic
#define AE      6L      // Azimuthal Equidistant (Postel)
#define MERCAT  8L      // Mercator
#define POLYC   11L      // Polyconic
#define PS      13L     // Polar Stereographic
#define GNOMON  15L     // Gnomonic
#define UTM     17L     // Universal Transverse Mercator (UTM)
#define MOLL    19L     // Mollweide
#define EC      20L     // Equidistant Conic


/************************************************************************/
/*  Correspondence between "Panorama" and EPSG datum codes.             */
/************************************************************************/

static long aoDatums[] =
{
    0,
    4284,   // Pulkovo, 1942
    4326    // WGS, 1984
};

#define NUMBER_OF_DATUMS        (long)(sizeof(aoDatums)/sizeof(aoDatums[0]))

/************************************************************************/
/*  Correspondence between "Panorama" and EPSG ellipsoid codes.         */
/************************************************************************/

static long aoEllips[] =
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
    7030    // WGS, 1984 (GPS)
};

#define NUMBER_OF_ELLIPSOIDS    (long)(sizeof(aoEllips)/sizeof(aoEllips[0]))

/************************************************************************/
/*                    PanoramaGetUOMLengthInfo()                        */
/*                                                                      */
/*      Note: This function should eventually also know how to          */
/*      lookup length aliases in the UOM_LE_ALIAS table.                */
/************************************************************************/

static int 
PanoramaGetUOMLengthInfo( int nUOMLengthCode, char **ppszUOMName,
                          double * pdfInMeters )

{
    char        **papszUnitsRecord;
    char        szSearchKey[24];
    int         iNameField;

#define UOM_FILENAME CSVFilename( "unit_of_measure.csv" )

/* -------------------------------------------------------------------- */
/*      We short cut meter to save work in the most common case.        */
/* -------------------------------------------------------------------- */
    if( nUOMLengthCode == 9001 )
    {
        if( ppszUOMName != NULL )
            *ppszUOMName = CPLStrdup( "metre" );
        if( pdfInMeters != NULL )
            *pdfInMeters = 1.0;

        return TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Search the units database for this unit.  If we don't find      */
/*      it return failure.                                              */
/* -------------------------------------------------------------------- */
    sprintf( szSearchKey, "%d", nUOMLengthCode );
    papszUnitsRecord =
        CSVScanFileByName( UOM_FILENAME, "UOM_CODE", szSearchKey, CC_Integer );

    if( papszUnitsRecord == NULL )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Get the name, if requested.                                     */
/* -------------------------------------------------------------------- */
    if( ppszUOMName != NULL )
    {
        iNameField = CSVGetFileFieldId( UOM_FILENAME, "UNIT_OF_MEAS_NAME" );
        *ppszUOMName = CPLStrdup( CSLGetField(papszUnitsRecord, iNameField) );
    }
    
/* -------------------------------------------------------------------- */
/*      Get the A and B factor fields, and create the multiplicative    */
/*      factor.                                                         */
/* -------------------------------------------------------------------- */
    if( pdfInMeters != NULL )
    {
        int     iBFactorField, iCFactorField;
        
        iBFactorField = CSVGetFileFieldId( UOM_FILENAME, "FACTOR_B" );
        iCFactorField = CSVGetFileFieldId( UOM_FILENAME, "FACTOR_C" );

        if( atof(CSLGetField(papszUnitsRecord, iCFactorField)) > 0.0 )
            *pdfInMeters = atof(CSLGetField(papszUnitsRecord, iBFactorField))
                / atof(CSLGetField(papszUnitsRecord, iCFactorField));
        else
            *pdfInMeters = 0.0;
    }
    
    return( TRUE );
}

/************************************************************************/
/*                     PanoramaGetEllipsoidInfo()                       */
/*                                                                      */
/*      Fetch info about an ellipsoid.  Axes are always returned in     */
/*      meters.  SemiMajor computed based on inverse flattening         */
/*      where that is provided.                                         */
/************************************************************************/

static int 
PanoramaGetEllipsoidInfo( int nCode, char ** ppszName,
                          double * pdfSemiMajor, double * pdfInvFlattening )

{
    char        szSearchKey[24];
    double      dfSemiMajor, dfToMeters = 1.0;
    int         nUOMLength;
    
/* -------------------------------------------------------------------- */
/*      Get the semi major axis.                                        */
/* -------------------------------------------------------------------- */
    sprintf( szSearchKey, "%d", nCode );

    dfSemiMajor =
        atof(CSVGetField( CSVFilename("ellipsoid.csv" ),
                          "ELLIPSOID_CODE", szSearchKey, CC_Integer,
                          "SEMI_MAJOR_AXIS" ) );
    if( dfSemiMajor == 0.0 )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Get the translation factor into meters.                         */
/* -------------------------------------------------------------------- */
    nUOMLength = atoi(CSVGetField( CSVFilename("ellipsoid.csv" ),
                                   "ELLIPSOID_CODE", szSearchKey, CC_Integer,
                                   "UOM_CODE" ));
    PanoramaGetUOMLengthInfo( nUOMLength, NULL, &dfToMeters );

    dfSemiMajor *= dfToMeters;
    
    if( pdfSemiMajor != NULL )
        *pdfSemiMajor = dfSemiMajor;
    
/* -------------------------------------------------------------------- */
/*      Get the semi-minor if requested.  If the Semi-minor axis        */
/*      isn't available, compute it based on the inverse flattening.    */
/* -------------------------------------------------------------------- */
    if( pdfInvFlattening != NULL )
    {
        *pdfInvFlattening = 
            atof(CSVGetField( CSVFilename("ellipsoid.csv" ),
                              "ELLIPSOID_CODE", szSearchKey, CC_Integer,
                              "INV_FLATTENING" ));

        if( *pdfInvFlattening == 0.0 )
        {
            double dfSemiMinor;

            dfSemiMinor =
                atof(CSVGetField( CSVFilename("ellipsoid.csv" ),
                                  "ELLIPSOID_CODE", szSearchKey, CC_Integer,
                                  "SEMI_MINOR_AXIS" )) * dfToMeters;

            if( dfSemiMajor != 0.0 && dfSemiMajor != dfSemiMinor )
                *pdfInvFlattening = 
                    -1.0 / (dfSemiMinor/dfSemiMajor - 1.0);
            else
                *pdfInvFlattening = 0.0;
        }
    }

/* -------------------------------------------------------------------- */
/*      Get the name, if requested.                                     */
/* -------------------------------------------------------------------- */
    if( ppszName != NULL )
        *ppszName =
            CPLStrdup(CSVGetField( CSVFilename("ellipsoid.csv" ),
                                   "ELLIPSOID_CODE", szSearchKey, CC_Integer,
                                   "ELLIPSOID_NAME" ));
    
    return( TRUE );
}

/************************************************************************/
/*                        OSRImportFromPanorama()                       */
/************************************************************************/

OGRErr OSRImportFromPanorama( OGRSpatialReferenceH hSRS,
                              long iProjSys, long iDatum, long iEllips,
                              long iZone,
                              double dfStdP1, double dfStdP2,
                              double dfCenterLat, double dfCenterLong )

{
    return ((OGRSpatialReference *) hSRS)->importFromPanorama( iProjSys, iDatum,
                                                               iEllips, iZone,
                                                               dfStdP1, dfStdP2,
                                                               dfCenterLat,
                                                               dfCenterLong );
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
 * @param iProjSys Input projection system code, used in GCTP.
 *
 * @param iDatum Input coordinate system.
 *
 * @param iEllips Input spheroid.
 * 
 * @param iZone Input zone for UTM projection system.
 *
 * @param dfStdP1 Latitude of the first standard parallel (radians).
 *
 * @param dfStdP2 Latitude of the second standard parallel (radians).
 *
 * @param dfCenterLat Latitude of center of projection (radians).
 *
 * @param dfCenterLong Longitude of center of projection (radians).
 *
 * @param iDatum Output spheroid.<p>
 *
 *      <h4>Supported Datums</h4>
 * <pre>
 *       1: Pulkovo, 1942
 *       2: WGS, 1984
 * </pre>
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
 * @return OGRERR_NONE on success or an error code in case of failure. 
 */

OGRErr OGRSpatialReference::importFromPanorama( long iProjSys, long iDatum,
                                                long iEllips, long iZone,
                                                double dfStdP1, double dfStdP2,
                                                double dfCenterLat,
                                                double dfCenterLong )

{
/* -------------------------------------------------------------------- */
/*      Operate on the basis of the projection code.                    */
/* -------------------------------------------------------------------- */
    switch ( iProjSys )
    {
        case NONE:
            break;

        case UTM:
            if ( iZone >= 0 )
                SetUTM( iZone, TRUE );
            else
                SetUTM( -iZone, FALSE );
            break;

        case MERCAT:
            SetMercator( TO_DEGREES * dfCenterLat, TO_DEGREES * dfCenterLong,
                         1.0, 0.0, 0.0 );
            break;

        case PS:
            SetPS( TO_DEGREES * dfCenterLat, TO_DEGREES * dfCenterLong,
                   1.0, 0.0, 0.0 );
            break;

        case POLYC:
            SetPolyconic( TO_DEGREES * dfCenterLat, TO_DEGREES * dfCenterLong,
                          0.0, 0.0 );
            break;

        case EC:
            SetEC( TO_DEGREES * dfStdP1, TO_DEGREES * dfStdP2,
                   TO_DEGREES * dfCenterLat, TO_DEGREES * dfCenterLong,
                   0.0, 0.0 );
            break;

        case TM:
            SetTM( TO_DEGREES * dfCenterLat, TO_DEGREES * dfCenterLong,
                   1.0, 0.0, 0.0 );
            break;

        case STEREO:
            SetStereographic( TO_DEGREES * dfCenterLat,
                              TO_DEGREES * dfCenterLong,
                              1.0, 0.0, 0.0 );
            break;

        case LAEA:
            SetLAEA( TO_DEGREES * dfCenterLat, TO_DEGREES * dfCenterLong,
                     0.0, 0.0 );
            break;

        case AE:
            SetAE( TO_DEGREES * dfCenterLat, TO_DEGREES * dfCenterLong,
                   0.0, 0.0 );
            break;

        case GNOMON:
            SetGnomonic( TO_DEGREES * dfCenterLat, TO_DEGREES * dfCenterLong,
                         0.0, 0.0 );
            break;

        case MOLL:
            SetMollweide( TO_DEGREES * dfCenterLong, 0.0, 0.0 );
            break;

        default:
            CPLDebug( "OSR_Panorama", "Unsupported projection: %d", iProjSys );
            SetLocalCS( CPLSPrintf("\"Panorama\" projection number %d",
                                   iProjSys) );
            break;
            
    }

/* -------------------------------------------------------------------- */
/*      Try to translate the datum/spheroid.                            */
/* -------------------------------------------------------------------- */

    if ( !IsLocal() )
    {
        if ( iDatum > 0 && iDatum < NUMBER_OF_DATUMS && aoDatums[iDatum] )
        {
            OGRSpatialReference oGCS;
            oGCS.importFromEPSG( aoDatums[iDatum] );
            CopyGeogCSFrom( &oGCS );
        }

        else if ( iEllips > 0
                  && iEllips < NUMBER_OF_ELLIPSOIDS
                  && aoEllips[iEllips] )
        {
            char    *pszName = NULL;
            double  dfSemiMajor, dfInvFlattening;

            if( PanoramaGetEllipsoidInfo( aoEllips[iEllips],
                                          &pszName,
                                          &dfSemiMajor, &dfInvFlattening ) )
            {
                SetGeogCS( CPLSPrintf("Unknown datum based upon the %s ellipsoid",
                                      pszName ),
                           CPLSPrintf( "Not specified (based on %s spheroid)",
                                       pszName ),
                           pszName, dfSemiMajor, dfInvFlattening,
                           NULL, 0.0, NULL, 0.0 );
                SetAuthority( "SPHEROID", "EPSG", aoEllips[iEllips] );
            }
            else
            {
                CPLError( CE_Warning, CPLE_AppDefined,
                          "Failed to lookup ellipsoid code %d, likely due to"
                          " missing GDAL gcs.csv\n"
                          " file.  Falling back to use WGS84.", iEllips );
                SetWellKnownGeogCS("WGS84" );
            }

            if ( pszName )
                CPLFree( pszName );
        }
        
        else
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "Wrong datum code %d. Supported datums are 1--%d only.\n"
                      "Setting WGS84 as a fallback.",
                      iDatum, NUMBER_OF_DATUMS - 1 );
            SetWellKnownGeogCS( "WGS84" );
        }
    }

/* -------------------------------------------------------------------- */
/*      Grid units translation                                          */
/* -------------------------------------------------------------------- */
    if( IsLocal() || IsProjected() )
        SetLinearUnits( SRS_UL_METER, 1.0 );

    FixupOrdering();

    return OGRERR_NONE;
}

/************************************************************************/
/*                      OSRExportToPanorama()                           */
/************************************************************************/

OGRErr OSRExportToPanorama( OGRSpatialReferenceH hSRS,
                            long *piProjSys, long *piDatum, long *piEllips,
                            long *piZone,
                            double *pdfStdP1, double *pdfStdP2,
                            double *pdfCenterLat, double *pdfCenterLong )

{
    return ((OGRSpatialReference *) hSRS)->exportToPanorama( piProjSys,
                                                             piDatum, piEllips,
                                                             piZone,
                                                             pdfStdP1, pdfStdP2,
                                                             pdfCenterLat,
                                                             pdfCenterLong );
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
 * @param pdfStdP1 Pointer to variable, where the latitude of the first
 * standard parallel will be returned (radians).
 *
 * @param pdfStdP2 Pointer to variable, where the latitude of the second
 * standard parallel will be returned (radians).
 *
 * @param pdfCenterLat Pointer to variable, where the latitude of center
 * of projection will be returned (radians).
 *
 * @param pdfCenterLong Pointer to variable, where the longitude of center
 * of projection will be returned (radians).
 *
 * @return OGRERR_NONE on success or an error code on failure. 
 */

OGRErr OGRSpatialReference::exportToPanorama( long *piProjSys, long *piDatum,
                                              long *piEllips, long *piZone,
                                              double *pdfStdP1,
                                              double *pdfStdP2,
                                              double *pdfCenterLat,
                                              double *pdfCenterLong ) const

{
    const char  *pszProjection = GetAttrValue("PROJECTION");

/* -------------------------------------------------------------------- */
/*      Fill all projection parameters with zero.                       */
/* -------------------------------------------------------------------- */
    *piDatum = 0L;
    *piEllips = 0L;
    *piZone = 0L;
    *pdfStdP1 = *pdfStdP2 = *pdfCenterLat = *pdfCenterLong = 0.0;

/* ==================================================================== */
/*      Handle the projection definition.                               */
/* ==================================================================== */
    if( IsLocal() )
        *piProjSys = NONE;

    else if( pszProjection == NULL )
    {
#ifdef DEBUG
        CPLDebug( "OSR_Panorama",
                  "Empty projection definition, considered as Geographic" );
#endif
        *piProjSys = NONE;
    }

    else if( EQUAL(pszProjection, SRS_PT_MERCATOR_1SP) )
    {
        *piProjSys = MERCAT;
        *pdfCenterLong =
            TO_RADIANS * GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 );
        *pdfCenterLat = 
            TO_RADIANS * GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 );
    }

    else if( EQUAL(pszProjection, SRS_PT_POLAR_STEREOGRAPHIC) )
    {
        *piProjSys = PS;
        *pdfCenterLong =
            TO_RADIANS * GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 );
        *pdfCenterLat = 
            TO_RADIANS * GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 );
    }

    else if( EQUAL(pszProjection, SRS_PT_POLYCONIC) )
    {
        *piProjSys = POLYC;
        *pdfCenterLong =
            TO_RADIANS * GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 );
        *pdfCenterLat = 
            TO_RADIANS * GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 );
    }

    else if( EQUAL(pszProjection, SRS_PT_EQUIDISTANT_CONIC) )
    {
        *piProjSys = EC;
        *pdfStdP1 =
            TO_RADIANS * GetNormProjParm( SRS_PP_STANDARD_PARALLEL_1, 0.0 );
        *pdfStdP2 = 
            TO_RADIANS * GetNormProjParm( SRS_PP_STANDARD_PARALLEL_2, 0.0 );
        *pdfCenterLong =
            TO_RADIANS * GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 );
        *pdfCenterLat = 
            TO_RADIANS * GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 );
    }

    else if( EQUAL(pszProjection, SRS_PT_TRANSVERSE_MERCATOR) )
    {
        int bNorth;

        *piZone = GetUTMZone( &bNorth );

        if( *piZone != 0 )
        {
            *piProjSys = UTM;
            if( !bNorth )
                *piZone = - *piZone;
        }            
        else
        {
            *piProjSys = TM;
            *pdfCenterLong =
                TO_RADIANS * GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 );
            *pdfCenterLat = 
                TO_RADIANS * GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 );
        }
    }

    else if( EQUAL(pszProjection, SRS_PT_STEREOGRAPHIC) )
    {
        *piProjSys = STEREO;
        *pdfCenterLong =
            TO_RADIANS * GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 );
        *pdfCenterLat = 
            TO_RADIANS * GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 );
    }

    else if( EQUAL(pszProjection, SRS_PT_LAMBERT_AZIMUTHAL_EQUAL_AREA) )
    {
        *piProjSys = LAEA;
        *pdfCenterLong =
            TO_RADIANS * GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 );
        *pdfCenterLat = 
            TO_RADIANS * GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 );
    }

    else if( EQUAL(pszProjection, SRS_PT_AZIMUTHAL_EQUIDISTANT) )
    {
        *piProjSys = AE;
        *pdfCenterLong =
            TO_RADIANS * GetNormProjParm( SRS_PP_LONGITUDE_OF_CENTER, 0.0 );
        *pdfCenterLat = 
            TO_RADIANS * GetNormProjParm( SRS_PP_LATITUDE_OF_CENTER, 0.0 );
    }

    else if( EQUAL(pszProjection, SRS_PT_GNOMONIC) )
    {
        *piProjSys = GNOMON;
        *pdfCenterLong =
            TO_RADIANS * GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 );
        *pdfCenterLat = 
            TO_RADIANS * GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 );
    }

    else if( EQUAL(pszProjection, SRS_PT_MOLLWEIDE) )
    {
        *piProjSys = MOLL;
        *pdfCenterLong =
            TO_RADIANS * GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 );
    }

    // Projection unsupported by "Panorama" GIS
    else
    {
        CPLDebug( "OSR_Panorama",
                  "Projection \"%s\" unsupported by \"Panorama\" GIS. "
                  "Geographic system will be used.", pszProjection );
        *piProjSys = NONE;
    }
 
/* -------------------------------------------------------------------- */
/*      Translate the datum.                                            */
/* -------------------------------------------------------------------- */
    const char  *pszDatum = GetAttrValue( "DATUM" );

    if ( EQUAL( pszDatum, "Pulkovo_1942" ) )
        *piDatum = 1L;
    if( EQUAL( pszDatum, SRS_DN_WGS84 ) )
        *piDatum = 2L;

    // If not found well known datum, translate ellipsoid
    else
    {
        double      dfSemiMajor = GetSemiMajor();
        double      dfInvFlattening = GetInvFlattening();
        int         i;

#ifdef DEBUG
        CPLDebug( "OSR_Panorama",
                  "Datum \"%s\" unsupported by \"Panorama\" GIS. "
                  "Try to translate ellipsoid definition.", pszDatum );
#endif
        
        for ( i = 0; i < NUMBER_OF_ELLIPSOIDS && aoEllips[i]; i++ )
        {
            double  dfSM = 0.0;
            double  dfIF = 1.0;

            PanoramaGetEllipsoidInfo( aoEllips[i], NULL, &dfSM, &dfIF );
            if( CPLIsEqual(dfSemiMajor, dfSM)
                && CPLIsEqual(dfInvFlattening, dfIF) )
            {
                *piEllips = i;
                break;
            }
        }

        if ( i == NUMBER_OF_ELLIPSOIDS )    // Didn't found matches.
        {
#ifdef DEBUG
            CPLDebug( "OSR_Panorama",
                      "Ellipsoid \"%s\" unsupported by \"Panorama\" GIS.",
                      pszDatum );
#endif
            *piEllips = 0;
        }
    }

    return OGRERR_NONE;
}

