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
 * Revision 1.2  2003/09/09 07:49:18  dron
 * Added exportToPCI() method.
 *
 * Revision 1.1  2003/08/31 14:49:37  dron
 * New.
 */

#include "ogr_spatialref.h"
#include "cpl_conv.h"
#include "cpl_csv.h"

CPL_CVSID("$Id$");

typedef struct PCIDatums
{
    const char  *pszPCIDatum;
    const int   nEPSGCode;
};

static PCIDatums aoDatums[] =
{
    { "D000", 6326 },   // WGS 1984
    { "D001", 6322 },   // WGS 1972
    { NULL, 0 }
};

static PCIDatums aoEllips[] =
{
    { "E000", 7008 },   // Clarke, 1866 (NAD1927)
    { "E001", 7034 },   // Clarke, 1880
    { "E002", 7004 },   // Bessel, 1841
    // FIXME: New International, 1967 --- skipped
    { "E004", 7022 },   // International, 1924 (Hayford, 1909)
    { "E005", 7043 },   // WGS, 1972
    { "E006", 7042 },   // Everest, 1830 --- skipped
    // FIXME: WGS, 1966 --- skipped
    { "E008", 7019 },   // GRS, 1980 (NAD1983)
    { "E009", 7001 },   // Airy, 1830
    { "E010", 7018 },   // Modified Everest --- skipped
    { "E011", 7002 },   // Modified Airy
    { "E012", 7030 },   // WGS, 1984 (GPS)
    // FIXME: Southeast Asia --- skipped
    { "E014", 7003 },   // Australian National, 1965
    { "E015", 7024 },   // Krassovsky, 1940
    // FIXME: Hough --- skipped
    // FIXME: Mercury, 1960 --- skipped
    // FIXME: Modified Mercury, 1968 --- skipped
    // FIXME: Sphere, rad 6370997 m (normal sphere) --- skipped
    // FIXME: Bessel, 1841 (Japan by Law) --- skipped
    // FIXME: D-PAF (Orbits) --- skipped
    { "E900", 7006 },   // Bessel, 1841 (Namibia)
    // FIXME: Everest, 1956 --- skipped
    // Everest, 1969 --- skipped
    // Everest (Sabah & Sarawak) --- skipped
    { "E904", 7020 },   // Helmert, 1906
    // FIXME: SGS 85 --- skipped
    // FIXME: WGS 60 --- skipped
    { "E907", 7036 },   // South American, 1969
    { "E910", 7041 },   // ATS77
    { NULL, 0 }
};

/************************************************************************/
/*                        PCIGetUOMLengthInfo()                         */
/*                                                                      */
/*      Note: This function should eventually also know how to          */
/*      lookup length aliases in the UOM_LE_ALIAS table.                */
/************************************************************************/

static int 
PCIGetUOMLengthInfo( int nUOMLengthCode, char **ppszUOMName,
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
/*                        PCIGetEllipsoidInfo()                         */
/*                                                                      */
/*      Fetch info about an ellipsoid.  Axes are always returned in     */
/*      meters.  SemiMajor computed based on inverse flattening         */
/*      where that is provided.                                         */
/************************************************************************/

static int 
PCIGetEllipsoidInfo( int nCode, char ** ppszName,
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
    PCIGetUOMLengthInfo( nUOMLength, NULL, &dfToMeters );

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

        iZone = CPLScanLong( (char *)pszProj + 4, 5 );;
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
        PCIDatums   *paoDatum = aoDatums;

        // Search for matching datum
        while ( paoDatum->pszPCIDatum )
        {
            if( EQUALN( pszProj + 12, paoDatum->pszPCIDatum, 4 ) )
            {
                importFromEPSG( paoDatum->nEPSGCode );
                break;
            }
            paoDatum++;
        }

        if ( !paoDatum->pszPCIDatum )  // No matching; search for ellipsoids
        {
            paoDatum = aoEllips;

            while ( paoDatum->pszPCIDatum )
            {
                if( EQUALN( pszProj + 12, paoDatum->pszPCIDatum, 4 ) )
                {
                    char    *pszName = NULL;
                    double  dfSemiMajor;
                    double  dfInvFlattening;
                    
                    PCIGetEllipsoidInfo( paoDatum->nEPSGCode, &pszName,
                                         &dfSemiMajor, &dfInvFlattening );
                    SetGeogCS( CPLSPrintf(
                                   "Unknown datum based upon the %s ellipsoid",
                                   pszName ),
                               CPLSPrintf(
                                   "Not specified (based on %s spheroid)",
                                   pszName ),
                               pszName, dfSemiMajor, dfInvFlattening,
                               NULL, 0, NULL, 0 );
                    SetAuthority( "SPHEROID", "EPSG", paoDatum->nEPSGCode );

                    if ( pszName )
                        CPLFree( pszName );

                    break;
                }
                paoDatum++;
            }
        }

        if ( !paoDatum->pszPCIDatum )      // Didn't found matches
        {
            if( EQUALN( pszProj + 12, "E999", 4 ) )
            {
                double      dfInvFlattening;

                if( ABS(padfPrjParams[0] - padfPrjParams[1]) < 0.01 )
                {
                    dfInvFlattening = 0.0;
                }
                else
                {
                    dfInvFlattening =
                        padfPrjParams[0]/(padfPrjParams[0]-padfPrjParams[1]);
                }

                SetGeogCS( "Unknown datum based upon the custom spheroid",
                           "Not specified (based on custom spheroid)",
                           "Custom spheroid",
                           padfPrjParams[0], dfInvFlattening,
                           NULL, 0, NULL, 0 );
            }
            else
            {
                // If we don't know, default to WGS84
                // so there is something there.
                SetWellKnownGeogCS( "WGS84" );
            }
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

/************************************************************************/
/*                          OSRExportToPCI()                            */
/************************************************************************/

OGRErr OSRExportToPCI( OGRSpatialReferenceH hSRS,
                       char **ppszProj, char **ppszUnits,
                       double **ppadfPrjParams )

{
    *ppszProj = NULL;
    *ppszUnits = NULL;
    *ppadfPrjParams = NULL;

    return ((OGRSpatialReference *) hSRS)->exportToPCI( ppszProj, ppszUnits,
                                                        ppadfPrjParams );
}

/************************************************************************/
/*                           exportToPCI()                              */
/************************************************************************/

/**
 * Export coordinate system in PCI projection definition.
 *
 * Converts the loaded coordinate reference system into PCI projection
 * definition to the extent possible. The strings returned in ppszProj,
 * ppszUnits and ppadfPrjParams array should be deallocated by the caller
 * with CPLFree() when no longer needed.
 *
 * LOCAL_CS coordinate systems are not translatable.  An empty string
 * will be returned along with OGRERR_NONE.  
 *
 * This method is the equivelent of the C function OSRExportToPCI().
 *
 * @param ppszProj pointer to which dynamically allocated PCI projection
 * definition will be assigned.
 * 
 * @param ppszUnits pointer to which dynamically allocated units definition 
 * will be assigned.
 *
 * @param ppadfPrjParams pointer to which dynamically allocated array of
 * 16 projection parameters will be assigned. See importFromPCI() for the list
 * of parameters.
 * 
 * @return OGRERR_NONE on success or an error code on failure. 
 */

OGRErr OGRSpatialReference::exportToPCI( char **ppszProj, char **ppszUnits,
                                         double **ppadfPrjParams ) const

{
    const char  *pszProjection = GetAttrValue("PROJECTION");

/* -------------------------------------------------------------------- */
/*      Fill all projection parameters with zero.                       */
/* -------------------------------------------------------------------- */
    int         i;

    *ppadfPrjParams = (double *)CPLMalloc( 16 * sizeof(double) );
    for ( i = 0; i < 16; i++ )
        (*ppadfPrjParams)[i] = 0.0;
   
/* -------------------------------------------------------------------- */
/*      Get the prime meridian info.                                    */
/* -------------------------------------------------------------------- */
    const OGR_SRSNode *poPRIMEM = GetAttrNode( "PRIMEM" );
    double dfFromGreenwich = 0.0;

    if( poPRIMEM != NULL && poPRIMEM->GetChildCount() >= 2 
        && atof(poPRIMEM->GetChild(1)->GetValue()) != 0.0 )
    {
        dfFromGreenwich = atof(poPRIMEM->GetChild(1)->GetValue());
    }

/* ==================================================================== */
/*      Handle the projection definition.                               */
/* ==================================================================== */
    char        szProj[17];

    if( IsLocal() )
    {
        CPLPrintStringFill( szProj, "PIXEL", 16 );
        return OGRERR_NONE;
    }

    else if( pszProjection == NULL )
        CPLPrintStringFill( szProj, "LONG/LAT", 16 );

    else if( EQUAL(pszProjection, SRS_PT_ALBERS_CONIC_EQUAL_AREA) )
    {
        CPLPrintStringFill( szProj, "ACEA", 16 );
        (*ppadfPrjParams)[2] = GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 );
        (*ppadfPrjParams)[3] =
            GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 );
        (*ppadfPrjParams)[4] =
            GetNormProjParm( SRS_PP_STANDARD_PARALLEL_1, 0.0 );
        (*ppadfPrjParams)[5] =
            GetNormProjParm( SRS_PP_STANDARD_PARALLEL_2, 0.0 );
        (*ppadfPrjParams)[6] = GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 );
        (*ppadfPrjParams)[7] = GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 );
    }

    else if( EQUAL(pszProjection, SRS_PT_AZIMUTHAL_EQUIDISTANT) )
    {
        CPLPrintStringFill( szProj, "AE", 16 );
        (*ppadfPrjParams)[2] = GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 );
        (*ppadfPrjParams)[3] =
            GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 );
        (*ppadfPrjParams)[6] = GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 );
        (*ppadfPrjParams)[7] = GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 );
    }

    else if( EQUAL(pszProjection, SRS_PT_EQUIDISTANT_CONIC) )
    {
        CPLPrintStringFill( szProj, "EC", 16 );
        (*ppadfPrjParams)[2] =
            GetNormProjParm( SRS_PP_LONGITUDE_OF_CENTER, 0.0 );
        (*ppadfPrjParams)[3] =
            GetNormProjParm( SRS_PP_LATITUDE_OF_CENTER, 0.0 );
        (*ppadfPrjParams)[4] =
            GetNormProjParm( SRS_PP_STANDARD_PARALLEL_1, 0.0 );
        (*ppadfPrjParams)[5] =
            GetNormProjParm( SRS_PP_STANDARD_PARALLEL_2, 0.0 );
        (*ppadfPrjParams)[6] = GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 );
        (*ppadfPrjParams)[7] = GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 );
    }

    else if( EQUAL(pszProjection, SRS_PT_EQUIRECTANGULAR) )
    {
        CPLPrintStringFill( szProj, "ER", 16 );
        (*ppadfPrjParams)[2] = GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 );
        (*ppadfPrjParams)[3] =
            GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 );
        (*ppadfPrjParams)[6] = GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 );
        (*ppadfPrjParams)[7] = GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 );
    }

    else if( EQUAL(pszProjection, SRS_PT_GNOMONIC) )
    {
        CPLPrintStringFill( szProj, "GNO", 16 );
        (*ppadfPrjParams)[2] = GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 );
        (*ppadfPrjParams)[3] =
            GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 );
        (*ppadfPrjParams)[6] = GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 );
        (*ppadfPrjParams)[7] = GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 );
    }

    else if( EQUAL(pszProjection, SRS_PT_LAMBERT_AZIMUTHAL_EQUAL_AREA) )
    {
        CPLPrintStringFill( szProj, "LAEA", 16 );
        (*ppadfPrjParams)[2] = GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 );
        (*ppadfPrjParams)[3] =
            GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 );
        (*ppadfPrjParams)[6] = GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 );
        (*ppadfPrjParams)[7] = GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 );
    }

    else if( EQUAL(pszProjection, SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP) )
    {
        CPLPrintStringFill( szProj, "LCC", 16 );
        (*ppadfPrjParams)[2] = GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 );
        (*ppadfPrjParams)[3] =
            GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 );
        (*ppadfPrjParams)[4] =
            GetNormProjParm( SRS_PP_STANDARD_PARALLEL_1, 0.0 );
        (*ppadfPrjParams)[5] =
            GetNormProjParm( SRS_PP_STANDARD_PARALLEL_2, 0.0 );
        (*ppadfPrjParams)[6] = GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 );
        (*ppadfPrjParams)[7] = GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 );
    }

    else if( EQUAL(pszProjection, SRS_PT_MILLER_CYLINDRICAL) )
    {
        CPLPrintStringFill( szProj, "MC", 16 );
        (*ppadfPrjParams)[2] = GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 );
        (*ppadfPrjParams)[3] =
            GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 );
        (*ppadfPrjParams)[6] = GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 );
        (*ppadfPrjParams)[7] = GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 );
    }

    else if( EQUAL(pszProjection, SRS_PT_MERCATOR_1SP) )
    {
        CPLPrintStringFill( szProj, "MER", 16 );
        (*ppadfPrjParams)[2] = GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 );
        (*ppadfPrjParams)[3] =
            GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 );
        (*ppadfPrjParams)[6] = GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 );
        (*ppadfPrjParams)[7] = GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 );
        (*ppadfPrjParams)[8] = GetNormProjParm( SRS_PP_SCALE_FACTOR, 1.0 );
    }

    else if( EQUAL(pszProjection, SRS_PT_ORTHOGRAPHIC) )
    {
        CPLPrintStringFill( szProj, "OG", 16 );
        (*ppadfPrjParams)[2] = GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 );
        (*ppadfPrjParams)[3] =
            GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 );
        (*ppadfPrjParams)[6] = GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 );
        (*ppadfPrjParams)[7] = GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 );
    }

    else if( EQUAL(pszProjection, SRS_PT_POLYCONIC) )
    {
        CPLPrintStringFill( szProj, "PC", 16 );
        (*ppadfPrjParams)[2] = GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 );
        (*ppadfPrjParams)[3] =
            GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 );
        (*ppadfPrjParams)[6] = GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 );
        (*ppadfPrjParams)[7] = GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 );
    }

    else if( EQUAL(pszProjection, SRS_PT_POLAR_STEREOGRAPHIC) )
    {
        CPLPrintStringFill( szProj, "PS", 16 );
        (*ppadfPrjParams)[2] = GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 );
        (*ppadfPrjParams)[3] =
            GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 );
        (*ppadfPrjParams)[6] = GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 );
        (*ppadfPrjParams)[7] = GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 );
        (*ppadfPrjParams)[8] = GetNormProjParm( SRS_PP_SCALE_FACTOR, 1.0 );
    }

    else if( EQUAL(pszProjection, SRS_PT_ROBINSON) )
    {
        CPLPrintStringFill( szProj, "ROB", 16 );
        (*ppadfPrjParams)[2] = GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 );
        (*ppadfPrjParams)[6] = GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 );
        (*ppadfPrjParams)[7] = GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 );
    }

    else if( EQUAL(pszProjection, SRS_PT_STEREOGRAPHIC) )
    {
        CPLPrintStringFill( szProj, "SG", 16 );
        (*ppadfPrjParams)[2] = GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 );
        (*ppadfPrjParams)[3] =
            GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 );
        (*ppadfPrjParams)[6] = GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 );
        (*ppadfPrjParams)[7] = GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 );
        (*ppadfPrjParams)[8] = GetNormProjParm( SRS_PP_SCALE_FACTOR, 1.0 );
    }

    else if( EQUAL(pszProjection, SRS_PT_SINUSOIDAL) )
    {
        CPLPrintStringFill( szProj, "SIN", 16 );
        (*ppadfPrjParams)[2] =
            GetNormProjParm( SRS_PP_LONGITUDE_OF_CENTER, 0.0 );
        (*ppadfPrjParams)[6] = GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 );
        (*ppadfPrjParams)[7] = GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 );
    }

    else if( EQUAL(pszProjection, SRS_PT_TRANSVERSE_MERCATOR) )
    {
        int bNorth;
        int nZone = GetUTMZone( &bNorth );

        if( nZone != 0 )
        {
            CPLPrintStringFill( szProj, "UTM", 16 );
            if( bNorth )
                CPLPrintInt32( szProj + 5, nZone, 4 );
            else
                CPLPrintInt32( szProj + 5, -nZone, 4 );
        }            
        else
        {
            CPLPrintStringFill( szProj, "TM", 16 );
            (*ppadfPrjParams)[2] =
                GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 );
            (*ppadfPrjParams)[3] =
                GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 );
            (*ppadfPrjParams)[6] = GetNormProjParm(SRS_PP_FALSE_EASTING, 0.0);
            (*ppadfPrjParams)[7] = GetNormProjParm(SRS_PP_FALSE_NORTHING, 0.0);
            (*ppadfPrjParams)[8] = GetNormProjParm(SRS_PP_SCALE_FACTOR, 1.0);
        }
    }

    else if( EQUAL(pszProjection, SRS_PT_VANDERGRINTEN) )
    {
        CPLPrintStringFill( szProj, "VDG", 16 );
        (*ppadfPrjParams)[2] = GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 );
        (*ppadfPrjParams)[6] = GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 );
        (*ppadfPrjParams)[7] = GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 );
    }

    // Projection unsupported by PCI
    else
        CPLPrintStringFill( szProj, "PIXEL", 16 );
    
/* -------------------------------------------------------------------- */
/*      Translate the datum.                                            */
/* -------------------------------------------------------------------- */
    const char  *pszDatum = GetAttrValue( "DATUM" );

    /*if( EQUAL( pszDatum, SRS_DN_NAD27 ) )
        CPLPrintStringFill( szProj + 12, "D000", 4 );

    else if( EQUAL( pszDatum, SRS_DN_NAD83 ) )
        CPLPrintStringFill( szProj + 12, "D000", 4 );

    else*/ if( EQUAL( pszDatum, SRS_DN_WGS84 ) )
        CPLPrintStringFill( szProj + 12, "D000", 4 );

    // If not found well known datum, translate ellipsoid
    else
    {
        double      dfSemiMajor = GetSemiMajor();
        double      dfInvFlattening = GetInvFlattening();

        PCIDatums   *paoDatum = aoEllips;
        
        while ( paoDatum->pszPCIDatum )
        {
            double  dfSM;
            double  dfIF;

            PCIGetEllipsoidInfo( paoDatum->nEPSGCode, NULL, &dfSM, &dfIF );
            if( ABS( dfSemiMajor - dfSM ) < 0.01
                && ABS( dfInvFlattening - dfIF ) < 0.0001 )
            {
                CPLPrintStringFill( szProj + 12, paoDatum->pszPCIDatum, 4 );
                break;
            }

            paoDatum++;
        }

        if ( !paoDatum->pszPCIDatum )       // Didn't found matches; set
        {                                   // custom ellipsoid parameters
            CPLPrintStringFill( szProj + 12, "E999", 4 );
            (*ppadfPrjParams)[0] = dfSemiMajor;
            if ( ABS( dfInvFlattening ) < 0.000000000001 )
            {
                (*ppadfPrjParams)[1] = dfSemiMajor;
            }
            else
            {
                (*ppadfPrjParams)[1] =
                    dfSemiMajor * (1.0 - 1.0/dfInvFlattening);
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Translate the linear units.                                     */
/* -------------------------------------------------------------------- */
    char        *pszUnits;
        
    if( EQUALN( szProj, "LONG/LAT", 8 ) )
        pszUnits = "DEGREE";
    else
        pszUnits = "METRE";

/* -------------------------------------------------------------------- */
/*      Report results.                                                 */
/* -------------------------------------------------------------------- */
    szProj[16] = '\0';
    *ppszProj = CPLStrdup( szProj );

    *ppszUnits = CPLStrdup( pszUnits );

    return OGRERR_NONE;
}

