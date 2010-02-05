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
 ****************************************************************************/

#include "ogr_spatialref.h"
#include "cpl_conv.h"
#include "cpl_csv.h"

CPL_CVSID("$Id$");

typedef struct 
{
    const char  *pszPCIDatum;
    int         nEPSGCode;
    double      dfSemiMajor;
    double      dfSemiMinor;
} PCIDatums;

static const PCIDatums aoDatums[] =
{
    { "D-01", 4267, 0, 0 },   // NAD27 (USA, NADCON)
    { "D-03", 4267, 0, 0 },   // NAD27 (Canada, NTv1)
    { "D-02", 4269, 0, 0 },   // NAD83 (USA, NADCON)
    { "D-04", 4269, 0, 0 },   // NAD83 (Canada, NTv1)
    { "D000", 4326, 0, 0 },   // WGS 1984
    { "D001", 4322, 0, 0 },   // WGS 1972
    { "D008", 4296, 0, 0 },   // Sudan
    { "D013", 4601, 0, 0 },   // Antigua Island Astro 1943
    { "D029", 4202, 0, 0 },   // Australian Geodetic 1966
    { "D030", 4203, 0, 0 },   // Australian Geodetic 1984
    { "D033", 4216, 0, 0 },   // Bermuda 1957
    { "D034", 4165, 0, 0 },   // Bissau
    { "D036", 4219, 0, 0 },   // Bukit Rimpah
    { "D038", 4221, 0, 0 },   // Campo Inchauspe
    { "D040", 4222, 0, 0 },   // Cape
    { "D042", 4223, 0, 0 },   // Carthage
    { "D044", 4224, 0, 0 },   // Chua Astro
    { "D045", 4225, 0, 0 },   // Corrego Alegre
    { "D046", 4155, 0, 0 },   // Dabola (Guinea)
    { "D066", 4272, 0, 0 },   // Geodetic Datum 1949 (New Zealand)
    { "D071", 4255, 0, 0 },   // Herat North (Afghanistan)
    { "D077", 4239, 0, 0 },   // Indian 1954 (Thailand, Vietnam)
    { "D078", 4240, 0, 0 },   // Indian 1975 (Thailand)
    { "D083", 4244, 0, 0 },   // Kandawala (Sri Lanka)
    { "D085", 4245, 0, 0 },   // Kertau 1948 (West Malaysia & Singapore)
    { "D088", 4250, 0, 0 },   // Leigon (Ghana)
    { "D089", 4251, 0, 0 },   // Liberia 1964 (Liberia)
    { "D092", 4256, 0, 0 },   // Mahe 1971 (Mahe Island)
    { "D093", 4262, 0, 0 },   // Massawa (Ethiopia (Eritrea))
    { "D094", 4261, 0, 0 },   // Merchich (Morocco)
    { "D098", 4604, 0, 0 },   // Montserrat Island Astro 1958 (Montserrat (Leeward Islands))
    { "D110", 4267, 0, 0 },   // NAD27 / Alaska
    { "D139", 4282, 0, 0 },   // Pointe Noire 1948 (Congo)
    { "D140", 4615, 0, 0 },   // Porto Santo 1936 (Porto Santo, Madeira Islands)
    { "D151", 4139, 0, 0 },   // Puerto Rico (Puerto Rico, Virgin Islands)
    { "D153", 4287, 0, 0 },   // Qornoq (Greenland (South))
    { "D158", 4292, 0, 0 },   // Sapper Hill 1943 (East Falkland Island)
    { "D159", 4293, 0, 0 },   // Schwarzeck (Namibia)
    { "D160", 4616, 0, 0 },   // Selvagem Grande 1938 (Salvage Islands)
    { "D176", 4297, 0, 0 },   // Tananarive Observatory 1925 (Madagascar)
    { "D177", 4298, 0, 0 },   // Timbalai 1948 (Brunei, East Malaysia (Sabah, Sarawak))
    { "D187", 4309, 0, 0 },   // Yacare (Uruguay)
    { "D188", 4311, 0, 0 },   // Zanderij (Suriname)
    { "D401", 4124, 0, 0 },   // RT90 (Sweden)
    { "D501", 4312, 0, 0 },   // MGI (Hermannskogel, Austria)
    { NULL, 0 }
};

static const PCIDatums aoEllips[] =
{
    { "E000", 7008, 0, 0 },     // Clarke, 1866 (NAD1927)
    { "E001", 7034, 0, 0 },     // Clarke, 1880
    { "E002", 7004, 0, 0 },     // Bessel, 1841
    { "E003", 0, 6378157.5,6356772.2 },   // New International, 1967
    { "E004", 7022, 0, 0 },     // International, 1924 (Hayford, 1909)
    { "E005", 7043, 0, 0 },     // WGS, 1972
    { "E006", 7042, 0, 0 },     // Everest, 1830
    { "E007", 0, 6378145.,6356759.769356 }, // WGS, 1966
    { "E008", 7019, 0, 0 },     // GRS, 1980 (NAD1983)
    { "E009", 7001, 0, 0 },     // Airy, 1830
    { "E010", 7018, 0, 0 },     // Modified Everest 
    { "E011", 7002, 0, 0 },     // Modified Airy
    { "E012", 7030, 0, 0 },     // WGS, 1984 (GPS)
    { "E013", 0, 6378155.,6356773.3205 }, // Southeast Asia
    { "E014", 7003, 0, 0 },     // Australian National, 1965
    { "E015", 7024, 0, 0 },     // Krassovsky, 1940
    { "E016", 7053, 0, 0 },     // Hough
    { "E017", 0, 6378166.,6356784.283666 }, // Mercury, 1960
    { "E018", 0, 6378150.,6356768.337303 }, //  Modified Mercury, 1968
    { "E019", 7052, 0, 0},      // normal sphere
    { "E333", 7046, 0, 0 },     // Bessel 1841 (Japan By Law)
    { "E600", 0, 6378144.0,6356759.0 }, // D-PAF (Orbits)
    { "E900", 7006, 0, 0 },     // Bessel, 1841 (Namibia)
    { "E901", 7044, 0, 0 },     // Everest, 1956
    { "E902", 7056, 0, 0 },     // Everest, 1969
    { "E903", 7016, 0, 0 },     // Everest (Sabah & Sarawak)
    { "E904", 7020, 0, 0 },     // Helmert, 1906
    { "E905", 0, 6378136.,6356751.301569 }, // SGS 85
    { "E906", 0, 6378165.,6356783.286959 }, // WGS 60
    { "E907", 7036, 0, 0 },     // South American, 1969
    { "E910", 7041, 0, 0 },     // ATS77
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
    VALIDATE_POINTER1( hSRS, "OSRImportFromPCI", CE_Failure );

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
 * [10] Longitude of 1st point on center line
 * [11] Latitude of 1st point on center line
 * [12] Longitude of 2nd point on center line
 * [13] Latitude of 2nd point on center line
 * [14] Azimuth east of north for center line
 * [15] Landsat satellite number
 * [16] Landsat path number
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
    Clear();

    if( pszProj == NULL )
        return OGRERR_CORRUPT_DATA;

#ifdef DEBUG
    CPLDebug( "OSR_PCI", "Trying to import projection \"%s\"", pszProj );
#endif

/* -------------------------------------------------------------------- */
/*      Use safe defaults if projection parameters are not supplied.    */
/* -------------------------------------------------------------------- */
    int     bProjAllocated = FALSE;

    if( padfPrjParams == NULL )
    {
        int     i;

        padfPrjParams = (double *)CPLMalloc( 17 * sizeof(double) );
        if ( !padfPrjParams )
            return OGRERR_NOT_ENOUGH_MEMORY;
        for ( i = 0; i < 17; i++ )
            padfPrjParams[i] = 0.0;
        bProjAllocated = TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Operate on the basis of the projection name.                    */
/* -------------------------------------------------------------------- */
    if( EQUALN( pszProj, "LONG/LAT", 8 ) )
    {
    }
    
    else if( EQUALN( pszProj, "METER", 5 ) 
             || EQUALN( pszProj, "METRE", 5 ) )
    {
        SetLocalCS( "METER" );
        SetLinearUnits( "METER", 1.0 );
    }

    else if( EQUALN( pszProj, "FEET", 4 ) 
             || EQUALN( pszProj, "FOOT", 4 ) )
    {
        SetLocalCS( "FEET" );
        SetLinearUnits( "FEET", atof(SRS_UL_FOOT_CONV) );
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
        // PCI and GCTP don't support natural origin lat. 
        SetEquirectangular2( 0.0, padfPrjParams[2],
                             padfPrjParams[3], 
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

    // FIXME: Add SPIF and SPAF?  (state plane international or american feet)

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
        
        // Check for a zone letter. PCI uses, accidentally, MGRS
        // type row lettering in its UTM projection
        char byZoneID = 0;

        if( strlen(pszProj) > 10 && pszProj[10] != ' ' )
            byZoneID = pszProj[10];

        // Determine if the MGRS zone falls above or below the equator
        if (byZoneID != 0 )
        {
#ifdef DEBUG
            CPLDebug("OSR_PCI", "Found MGRS zone in UTM projection string: %c",
                byZoneID);
#endif
            if (byZoneID >= 'N' && byZoneID <= 'X')
            {
                bNorth = TRUE;
            }
            else
            {
                bNorth = FALSE;
            }
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
        CPLDebug( "OSR_PCI", "Unsupported projection: %s", pszProj );
        SetLocalCS( pszProj );
    }

/* ==================================================================== */
/*      Translate the datum/spheroid.                                   */
/* ==================================================================== */

/* -------------------------------------------------------------------- */
/*      Extract and "normalize" the earthmodel to look like E001,       */
/*      D-02 or D109.                                                   */
/* -------------------------------------------------------------------- */
    char szEarthModel[5];
    const char *pszEM;

    strcpy( szEarthModel, "" );
    pszEM = pszProj + strlen(pszProj) - 1;
    while( pszEM != pszProj )
    {
        if( *pszEM == 'e' || *pszEM == 'E' || *pszEM == 'd' || *pszEM == 'D' )
        {
            int nCode = atoi(pszEM+1);

            if( nCode >= -99 && nCode <= 999 )
                sprintf( szEarthModel, "%c%03d", toupper(*pszEM), nCode );

            break;
        }

        pszEM--;
    }
    
/* -------------------------------------------------------------------- */
/*      We have an earthmodel string, look it up in the datum list.     */
/* -------------------------------------------------------------------- */
    if( strlen(szEarthModel) > 0 
        && (poRoot == NULL || IsProjected() || IsGeographic()) )
    {
        const PCIDatums   *paoDatum = aoDatums;

        // Search for matching datum
        while ( paoDatum->pszPCIDatum )
        {
            if( EQUALN( szEarthModel, paoDatum->pszPCIDatum, 4 ) )
            {
                OGRSpatialReference oGCS;
                oGCS.importFromEPSG( paoDatum->nEPSGCode );
                CopyGeogCSFrom( &oGCS );
                break;
            }
            paoDatum++;
        }

/* -------------------------------------------------------------------- */
/*      If not, look in the ellipsoid list.                             */
/* -------------------------------------------------------------------- */
        if ( !paoDatum->pszPCIDatum )  // No matching; search for ellipsoids
        {
            paoDatum = aoEllips;

#ifdef DEBUG
            CPLDebug( "OSR_PCI",
                      "Cannot found matching datum definition, "
                      "search for ellipsoids." );
#endif

            while ( paoDatum->pszPCIDatum )
            {
                if( EQUALN( szEarthModel, paoDatum->pszPCIDatum, 4 ) 
                    && paoDatum->nEPSGCode != 0 )
                {
                    char    *pszName = NULL;
                    double  dfSemiMajor;
                    double  dfInvFlattening;
                    
                    PCIGetEllipsoidInfo( paoDatum->nEPSGCode, &pszName,
                                         &dfSemiMajor, &dfInvFlattening );
                    SetGeogCS( CPLString().Printf(
                                   "Unknown datum based upon the %s ellipsoid",
                                   pszName ),
                               CPLString().Printf(
                                   "Not specified (based on %s spheroid)",
                                   pszName ),
                               pszName, dfSemiMajor, dfInvFlattening,
                               NULL, 0.0, NULL, 0.0 );
                    SetAuthority( "SPHEROID", "EPSG", paoDatum->nEPSGCode );

                    if ( pszName )
                        CPLFree( pszName );

                    break;
                }
                else if( EQUALN( szEarthModel, paoDatum->pszPCIDatum, 4 ) )
                {
                    double      dfInvFlattening;

                    if( ABS(paoDatum->dfSemiMajor - paoDatum->dfSemiMinor) 
                        < 0.01 )
                        dfInvFlattening = 0.0;
                    else
                        dfInvFlattening = paoDatum->dfSemiMajor 
                            / (paoDatum->dfSemiMajor-paoDatum->dfSemiMinor);

                    SetGeogCS( "Unknown datum based upon the custom spheroid",
                               "Not specified (based on custom spheroid)",
                               CPLString().Printf( "PCI Ellipse %s", 
                                                   szEarthModel),
                               paoDatum->dfSemiMajor, dfInvFlattening,
                               NULL, 0, NULL, 0 );
                    break;
                }
                paoDatum++;
            }
        }

/* -------------------------------------------------------------------- */
/*      Custom spheroid.                                                */
/* -------------------------------------------------------------------- */
        if ( !paoDatum->pszPCIDatum )      // Didn't found matches
        {
#ifdef DEBUG
            CPLDebug( "OSR_PCI",
                      "Cannot found matching ellipsoid definition." );
#endif

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
#ifdef DEBUG
                CPLDebug( "OSR_PCI", "Setting WGS84 as a fallback." );
#endif

            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Grid units translation                                          */
/* -------------------------------------------------------------------- */
    if( (IsLocal() || IsProjected()) && pszUnits )
    {
        if( EQUAL( pszUnits, "METRE" ) )
            SetLinearUnits( SRS_UL_METER, 1.0 );
        else if( EQUAL( pszUnits, "DEGREE" ) )
            SetAngularUnits( SRS_UA_DEGREE, atof(SRS_UA_DEGREE_CONV) );
        else
            SetLinearUnits( SRS_UL_METER, 1.0 );
    }

    FixupOrdering();

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
    VALIDATE_POINTER1( hSRS, "OSRExportToPCI", CE_Failure );

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
 * 17 projection parameters will be assigned. See importFromPCI() for the list
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

    *ppadfPrjParams = (double *)CPLMalloc( 17 * sizeof(double) );
    for ( i = 0; i < 17; i++ )
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
        if( GetLinearUnits() > 0.30479999 && GetLinearUnits() < 0.3048010 )
            CPLPrintStringFill( szProj, "FEET", 17 );
        else
            CPLPrintStringFill( szProj, "METER", 17 );
    }

    else if( pszProjection == NULL )
    {
#ifdef DEBUG
        CPLDebug( "OSR_PCI",
                  "Empty projection definition, considered as LONG/LAT" );
#endif
        CPLPrintStringFill( szProj, "LONG/LAT", 17 );
    }

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
            GetNormProjParm( SRS_PP_STANDARD_PARALLEL_1, 0.0 );
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
    {
        CPLDebug( "OSR_PCI",
                  "Projection \"%s\" unsupported by PCI. "
                  "PIXEL value will be used.", pszProjection );
        CPLPrintStringFill( szProj, "PIXEL", 16 );
    }
    
/* -------------------------------------------------------------------- */
/*      Translate the datum.                                            */
/* -------------------------------------------------------------------- */
    const char  *pszDatum = GetAttrValue( "DATUM" );

    if( pszDatum == NULL || strlen(pszDatum) == 0 )
        /* do nothing */;
    else if( EQUAL( pszDatum, SRS_DN_NAD27 ) )
        CPLPrintStringFill( szProj + 12, "D-01", 4 );

    else if( EQUAL( pszDatum, SRS_DN_NAD83 ) )
        CPLPrintStringFill( szProj + 12, "D-02", 4 );

    else if( EQUAL( pszDatum, SRS_DN_WGS84 ) )
        CPLPrintStringFill( szProj + 12, "D000", 4 );

    // If not found well known datum, translate ellipsoid
    else
    {
        double      dfSemiMajor = GetSemiMajor();
        double      dfInvFlattening = GetInvFlattening();

        const PCIDatums   *paoDatum = aoEllips;

#ifdef DEBUG
        CPLDebug( "OSR_PCI",
                  "Datum \"%s\" unsupported by PCI. "
                  "Try to translate ellipsoid definition.", pszDatum );
#endif
        
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
#ifdef DEBUG
            CPLDebug( "OSR_PCI",
                      "Ellipsoid \"%s\" unsupported by PCI. "
                      "Custom PCI ellipsoid will be used.", pszDatum );
#endif
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
    const char  *pszUnits;
        
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

