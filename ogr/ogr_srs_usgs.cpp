/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  OGRSpatialReference translation to/from USGS georeferencing
 *           information (used in GCTP package).
 * Author:   Andrey Kiselev, dron@remotesensing.org
 *
 ******************************************************************************
 * Copyright (c) 2004, Andrey Kiselev <dron@remotesensing.org>
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
 * Revision 1.1  2004/02/01 14:22:51  dron
 * Added OSRImportFromUSGS().
 *
 */

#include "ogr_spatialref.h"
#include "cpl_conv.h"
#include "cpl_csv.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*  GCTP projection codes.                                              */
/************************************************************************/

#define GEO     0L
#define UTM     1L
#define SPCS    2L
#define ALBERS  3L
#define LAMCC   4L
#define MERCAT  5L
#define PS      6L
#define POLYC   7L
#define EQUIDC  8L
#define TM      9L
#define STEREO  10L
#define LAMAZ   11L
#define AZMEQD  12L
#define GNOMON  13L
#define ORTHO   14L
#define GVNSP   15L
#define SNSOID  16L
#define EQRECT  17L
#define MILLER  18L
#define VGRINT  19L
#define HOM     20L
#define ROBIN   21L
#define SOM     22L
#define ALASKA  23L
#define GOODE   24L
#define MOLL    25L
#define IMOLL   26L
#define HAMMER  27L
#define WAGIV   28L
#define WAGVII  29L
#define OBEQA   30L
#define ISINUS  99L

/************************************************************************/
/*  Correspondence between GCTP and EPSG ellipsoid codes.               */
/************************************************************************/

static long aoEllips[] =
{
    7008,   // Clarke, 1866 (NAD1927)
    7034,   // Clarke, 1880
    7004,   // Bessel, 1841
    0,// FIXME: New International, 1967 --- skipped
    7022,   // International, 1924 (Hayford, 1909) XXX?
    7043,   // WGS, 1972
    7042,   // Everest, 1830
    7025,   // FIXME: WGS, 1966
    7019,   // GRS, 1980 (NAD1983)
    7001,   // Airy, 1830
    7018,   // Modified Everest --- skipped
    7002,   // Modified Airy
    7030,   // WGS, 1984 (GPS)
    0,// FIXME: Southeast Asia --- skipped
    7003,   // Australian National, 1965
    7024,   // Krassovsky, 1940
    0,// FIXME: Hough --- skipped
    0,// FIXME: Mercury, 1960 --- skipped
    0,// FIXME: Modified Mercury, 1968 --- skipped
    7047,   // Sphere, rad 6370997 m (normal sphere)
    7006,   // Bessel, 1841 (Namibia)
    7016,   // Everest (Sabah & Sarawak)
    7044,   // Everest, 1956
    0,// FIXME: Everest, Malaysia 1969 --- skipped
    7018,   // Everest, Malay & Singapr 1948
    0,// FIXME: Everest, Pakistan --- skipped
    7022,   // Hayford (International 1924) XXX?
    7020,   // Helmert 1906
    7021,   // Indonesian, 1974
    7036,   // South American, 1969
    0// FIXME: WGS 60 --- skipped
};

#define NUMBER_OF_ELLIPSOIDS    31

/************************************************************************/
/*                        USGSGetUOMLengthInfo()                        */
/*                                                                      */
/*      Note: This function should eventually also know how to          */
/*      lookup length aliases in the UOM_LE_ALIAS table.                */
/************************************************************************/

static int 
USGSGetUOMLengthInfo( int nUOMLengthCode, char **ppszUOMName,
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
/*                        USGSGetEllipsoidInfo()                        */
/*                                                                      */
/*      Fetch info about an ellipsoid.  Axes are always returned in     */
/*      meters.  SemiMajor computed based on inverse flattening         */
/*      where that is provided.                                         */
/************************************************************************/

static int 
USGSGetEllipsoidInfo( int nCode, char ** ppszName,
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
    USGSGetUOMLengthInfo( nUOMLength, NULL, &dfToMeters );

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
/*                         OSRImportFromUSGSI()                         */
/************************************************************************/

OGRErr OSRImportFromUSGS( OGRSpatialReferenceH hSRS, long iProjsys,
                          long iZone, double *padfPrjParams, long iDatum )

{
    return ((OGRSpatialReference *) hSRS)->importFromUSGS( iProjsys, iZone,
                                                           padfPrjParams,
                                                           iDatum );
}

/************************************************************************/
/*                          importFromUSGS()                            */
/************************************************************************/

/**
 * Import coordinate system from USGS projection definition.
 *
 * This method will import projection definition in style, used by USGS GCTP
 * software. GCTP operates on angles in packed DMS format (see
 * CPLPackedDMSToDec() function for details), so all angle values specified in
 * the padfPrjParams array should be in the packed DMS format.
 *
 * This function is the equivalent of the C function OSRImportFromUSGSI().
 *
 * @param iProjSys Output projection system code, used in GCTP.
 *
 * @param iZone Output zone for UTM and Stat Plane projection systems.
 * For Southern Hemisphere UTM use a negative zone code. iZone ignored
 * for all other projections.
 *
 * @param padfPrjParams Array of 15 coordinate system parameters. Refer to
 * GCTP documatation for parameters meaning.
 *
 * @param iDatum Output spheroid. 
 *
 * @return OGRERR_NONE on success or an error code in case of failure. 
 */

OGRErr OGRSpatialReference::importFromUSGS( long iProjSys, long iZone,
                                            double *padfPrjParams,
                                            long iDatum )

{
    if( !padfPrjParams )
        return OGRERR_CORRUPT_DATA;

/* -------------------------------------------------------------------- */
/*      Operate on the basis of the projection code.                    */
/* -------------------------------------------------------------------- */
    switch ( iProjSys )
    {
        case GEO:
            break;

        case UTM:
            {
                int bNorth = TRUE;
                if ( padfPrjParams[2] != 0.0 )
                    iZone = (long) padfPrjParams[2];
                else if ( padfPrjParams[0] != 0.0 && padfPrjParams[1] != 0.0 )
                {
                        ;
                }

                if ( iZone < 0 )
                {
                    iZone = -iZone;
                    bNorth = FALSE;
                }
                SetUTM( iZone, bNorth );
            }
            break;

        case SPCS:
            {
                int bNAD83 = TRUE;

                if ( iDatum == 0 )
                    bNAD83 = FALSE;
                else if ( iDatum != 8 )
                    CPLError( CE_Warning, CPLE_AppDefined,
                              "Wrong datum for State Plane projection %d. "
                              "Should be 0 or 8.", iDatum );
                
                SetStatePlane( iZone, bNAD83 );
            }
            break;

        case ALBERS:
            SetACEA( CPLPackedDMSToDec(padfPrjParams[2]),
                     CPLPackedDMSToDec(padfPrjParams[3]),
                     CPLPackedDMSToDec(padfPrjParams[5]),
                     CPLPackedDMSToDec(padfPrjParams[4]),
                     padfPrjParams[6], padfPrjParams[7] );
            break;

        case LAMCC:
            SetLCC( CPLPackedDMSToDec(padfPrjParams[2]),
                    CPLPackedDMSToDec(padfPrjParams[3]),
                    CPLPackedDMSToDec(padfPrjParams[5]),
                    CPLPackedDMSToDec(padfPrjParams[4]),
                    padfPrjParams[6], padfPrjParams[7] );
            break;

        case MERCAT:
            SetMercator( CPLPackedDMSToDec(padfPrjParams[5]),
                         CPLPackedDMSToDec(padfPrjParams[4]),
                         1.0,
                         padfPrjParams[6], padfPrjParams[7] );
            break;

        case PS:
            SetPS( CPLPackedDMSToDec(padfPrjParams[5]),
                   CPLPackedDMSToDec(padfPrjParams[4]),
                   1.0,
                   padfPrjParams[6], padfPrjParams[7] );

            break;

        case POLYC:
            SetPolyconic( CPLPackedDMSToDec(padfPrjParams[5]),
                          CPLPackedDMSToDec(padfPrjParams[4]),
                          padfPrjParams[6], padfPrjParams[7] );
            break;

        case EQUIDC:
            SetEC( CPLPackedDMSToDec(padfPrjParams[2]),
                   CPLPackedDMSToDec(padfPrjParams[3]),
                   CPLPackedDMSToDec(padfPrjParams[5]),
                   CPLPackedDMSToDec(padfPrjParams[4]),
                   padfPrjParams[6], padfPrjParams[7] );
            break;

        case TM:
            SetTM( CPLPackedDMSToDec(padfPrjParams[5]),
                   CPLPackedDMSToDec(padfPrjParams[4]),
                   padfPrjParams[2],
                   padfPrjParams[6], padfPrjParams[7] );
            break;

        case STEREO:
            SetStereographic( CPLPackedDMSToDec(padfPrjParams[5]),
                              CPLPackedDMSToDec(padfPrjParams[4]),
                              1.0,
                              padfPrjParams[6], padfPrjParams[7] );
            break;

        case LAMAZ:
            SetLAEA( CPLPackedDMSToDec(padfPrjParams[5]),
                     CPLPackedDMSToDec(padfPrjParams[4]),
                     padfPrjParams[6], padfPrjParams[7] );
            break;

        case AZMEQD:
            SetAE( CPLPackedDMSToDec(padfPrjParams[5]),
                   CPLPackedDMSToDec(padfPrjParams[4]),
                   padfPrjParams[6], padfPrjParams[7] );
            break;

        case GNOMON:
            SetGnomonic( CPLPackedDMSToDec(padfPrjParams[5]),
                         CPLPackedDMSToDec(padfPrjParams[4]),
                         padfPrjParams[6], padfPrjParams[7] );
            break;

        case ORTHO:
            SetOrthographic( CPLPackedDMSToDec(padfPrjParams[5]),
                             CPLPackedDMSToDec(padfPrjParams[4]),
                             padfPrjParams[6], padfPrjParams[7] );
            break;

        // FIXME: GVNSP --- General Vertical Near-Side Perspective skipped

        case SNSOID:
            SetSinusoidal( CPLPackedDMSToDec(padfPrjParams[4]),
                           padfPrjParams[6], padfPrjParams[7] );
            break;

        case EQRECT:
            SetEquirectangular( CPLPackedDMSToDec(padfPrjParams[5]),
                                CPLPackedDMSToDec(padfPrjParams[4]),
                                padfPrjParams[6], padfPrjParams[7] );
            break;

        case MILLER:
            SetMC( CPLPackedDMSToDec(padfPrjParams[5]),
                   CPLPackedDMSToDec(padfPrjParams[4]),
                   padfPrjParams[6], padfPrjParams[7] );
            break;

        case VGRINT:
            SetVDG( CPLPackedDMSToDec(padfPrjParams[4]),
                    padfPrjParams[6], padfPrjParams[7] );
            break;

        /*case HOM:
            // FIXME
            SetHOM( padfPrjParams[5], padfPrjParams[4],
                    padfPrjParams[3], padfPrjParams[0],
                    padfPrjParams[2],
                    padfPrjParams[6],  padfPrjParams[7] );
            break;*/

        case ROBIN:
            SetRobinson( CPLPackedDMSToDec(padfPrjParams[4]),
                         padfPrjParams[6], padfPrjParams[7] );
            break;

        // FIXME: SOM --- Space Oblique Mercator skipped

        // FIXME: ALASKA --- Alaska Conformal skipped

        // FIXME: GOODE --- Interrupted Goode skipped

        case MOLL:
            SetMollweide( CPLPackedDMSToDec(padfPrjParams[4]),
                          padfPrjParams[6], padfPrjParams[7] );
            break;

        // FIXME: IMOLL --- Interrupt Mollweide skipped

        // FIXME: HAMMER --- Hammer skipped

        // FIXME: WAGIV --- Wagner IV skipped

        // FIXME: WAGVII --- Wagner VII skipped

        // FIXME: OBEQA --- Oblated Equal Area skipped

        // FIXME: ISINUS --- Integrized Sinusoidal skipped

        default:
            CPLDebug( "OSR_USGS", "Unsupported projection: %d", iProjSys );
            SetLocalCS( CPLSPrintf("GCTP projection number %d", iProjSys) );
            break;
            
    }

/* -------------------------------------------------------------------- */
/*      Try to translate the datum/spheroid.                            */
/* -------------------------------------------------------------------- */

    if ( !IsLocal() )
    {
        if ( iDatum < 0 && padfPrjParams[0] > 0.0 ) // Use specified
        {                                           // ellipsoid parameters
            double      dfInvFlattening;

            if ( padfPrjParams[1] > 1.0 )
            {
                if( ABS(padfPrjParams[0] - padfPrjParams[1]) < 0.01 )
                {
                    dfInvFlattening = 0.0;
                }
                else
                {
                    dfInvFlattening =
                        padfPrjParams[0]/(padfPrjParams[0]-padfPrjParams[1]);
                }
            }
            else if ( padfPrjParams[1] > 0.0 )
            {
                dfInvFlattening = 1.0 / (1.0 - sqrt(1.0 - padfPrjParams[1]));
            }
            else
                dfInvFlattening = 0.0;

            SetGeogCS( "Unknown datum based upon the custom spheroid",
                       "Not specified (based on custom spheroid)",
                       "Custom spheroid", padfPrjParams[0], dfInvFlattening,
                       NULL, 0, NULL, 0 );
        }
        else if ( iDatum < NUMBER_OF_ELLIPSOIDS && aoEllips[iDatum] )
        {
                char    *pszName = NULL;
                double  dfSemiMajor;
                double  dfInvFlattening;
                
                USGSGetEllipsoidInfo( aoEllips[iDatum], &pszName,
                                     &dfSemiMajor, &dfInvFlattening );
                SetGeogCS( CPLSPrintf(
                               "Unknown datum based upon the %s ellipsoid",
                               pszName ),
                           CPLSPrintf( "Not specified (based on %s spheroid)",
                                       pszName ),
                           pszName, dfSemiMajor, dfInvFlattening,
                           NULL, 0.0, NULL, 0.0 );
                SetAuthority( "SPHEROID", "EPSG", aoEllips[iDatum] );

                if ( pszName )
                    CPLFree( pszName );

        }
        else
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "Wrong datum code %d. Supported datums 0--%d only.\n"
                      "Setting WGS84 as a fallback.",
                      iDatum, NUMBER_OF_ELLIPSOIDS );
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

