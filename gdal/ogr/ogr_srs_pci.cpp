/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  OGRSpatialReference translation to/from PCI georeferencing
 *           information.
 * Author:   Andrey Kiselev, dron@ak4719.spb.edu
 *
 ******************************************************************************
 * Copyright (c) 2003, Andrey Kiselev <dron@ak4719.spb.edu>
 * Copyright (c) 2009-2011, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "cpl_port.h"
#include "ogr_srs_api.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "cpl_conv.h"
#include "cpl_csv.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "ogr_core.h"
#include "ogr_p.h"
#include "ogr_spatialref.h"

CPL_CVSID("$Id$")

// PCI uses a 16-character string for coordinate system and datum/ellipsoid.
static const int knProjSize = 16;

typedef struct
{
    const char *pszPCIDatum;
    int        nEPSGCode;
} PCIDatums;

static const PCIDatums asDatums[] =
{
    { "D-01", 4267 },   // NAD27 (USA, NADCON)
    { "D-03", 4267 },   // NAD27 (Canada, NTv1)
    { "D-02", 4269 },   // NAD83 (USA, NADCON)
    { "D-04", 4269 },   // NAD83 (Canada, NTv1)
    { "D000", 4326 },   // WGS 1984
    { "D001", 4322 },   // WGS 1972
    { "D008", 4296 },   // Sudan
    { "D013", 4601 },   // Antigua Island Astro 1943
    { "D029", 4202 },   // Australian Geodetic 1966
    { "D030", 4203 },   // Australian Geodetic 1984
    { "D033", 4216 },   // Bermuda 1957
    { "D034", 4165 },   // Bissau
    { "D036", 4219 },   // Bukit Rimpah
    { "D038", 4221 },   // Campo Inchauspe
    { "D040", 4222 },   // Cape
    { "D042", 4223 },   // Carthage
    { "D044", 4224 },   // Chua Astro
    { "D045", 4225 },   // Corrego Alegre
    { "D046", 4155 },   // Dabola (Guinea)
    { "D066", 4272 },   // Geodetic Datum 1949 (New Zealand)
    { "D071", 4255 },   // Herat North (Afghanistan)
    { "D077", 4239 },   // Indian 1954 (Thailand, Vietnam)
    { "D078", 4240 },   // Indian 1975 (Thailand)
    { "D083", 4244 },   // Kandawala (Sri Lanka)
    { "D085", 4245 },   // Kertau 1948 (West Malaysia & Singapore)
    { "D088", 4250 },   // Leigon (Ghana)
    { "D089", 4251 },   // Liberia 1964 (Liberia)
    { "D092", 4256 },   // Mahe 1971 (Mahe Island)
    { "D093", 4262 },   // Massawa (Ethiopia (Eritrea))
    { "D094", 4261 },   // Merchich (Morocco)
    { "D098", 4604 },   // Montserrat Island Astro 1958 (Montserrat (Leeward Islands))
    { "D110", 4267 },   // NAD27 / Alaska
    { "D139", 4282 },   // Pointe Noire 1948 (Congo)
    { "D140", 4615 },   // Porto Santo 1936 (Porto Santo, Madeira Islands)
    { "D151", 4139 },   // Puerto Rico (Puerto Rico, Virgin Islands)
    { "D153", 4287 },   // Qornoq (Greenland (South))
    { "D158", 4292 },   // Sapper Hill 1943 (East Falkland Island)
    { "D159", 4293 },   // Schwarzeck (Namibia)
    { "D160", 4616 },   // Selvagem Grande 1938 (Salvage Islands)
    { "D176", 4297 },   // Tananarive Observatory 1925 (Madagascar)
    { "D177", 4298 },   // Timbalai 1948 (Brunei, East Malaysia (Sabah, Sarawak))
    { "D187", 4309 },   // Yacare (Uruguay)
    { "D188", 4311 },   // Zanderij (Suriname)
    { "D401", 4124 },   // RT90 (Sweden)
    { "D501", 4312 },   // MGI (Hermannskogel, Austria)
    { NULL, 0 }
};

static const PCIDatums asEllips[] =
{
    { "E000", 7008 },     // Clarke, 1866 (NAD1927)
    { "E001", 7034 },     // Clarke, 1880
    { "E002", 7004 },     // Bessel, 1841
    { "E004", 7022 },     // International, 1924 (Hayford, 1909)
    { "E005", 7043 },     // WGS, 1972
    { "E006", 7042 },     // Everest, 1830
    { "E008", 7019 },     // GRS, 1980 (NAD1983)
    { "E009", 7001 },     // Airy, 1830
    { "E010", 7018 },     // Modified Everest
    { "E011", 7002 },     // Modified Airy
    { "E012", 7030 },     // WGS, 1984 (GPS)
    { "E014", 7003 },     // Australian National, 1965
    { "E015", 7024 },     // Krassovsky, 1940
    { "E016", 7053 },     // Hough
    { "E019", 7052 },     // normal sphere
    { "E333", 7046 },     // Bessel 1841 (Japan By Law)
    { "E900", 7006 },     // Bessel, 1841 (Namibia)
    { "E901", 7044 },     // Everest, 1956
    { "E902", 7056 },     // Everest, 1969
    { "E903", 7016 },     // Everest (Sabah & Sarawak)
    { "E904", 7020 },     // Helmert, 1906
    { "E907", 7036 },     // South American, 1969
    { "E910", 7041 },     // ATS77
    { NULL, 0 }
};

/************************************************************************/
/*                         OSRImportFromPCI()                           */
/************************************************************************/

/**
 * \brief Import coordinate system from PCI projection definition.
 *
 * This function is the same as OGRSpatialReference::importFromPCI().
 */

OGRErr OSRImportFromPCI( OGRSpatialReferenceH hSRS, const char *pszProj,
                         const char *pszUnits, double *padfPrjParams )

{
    VALIDATE_POINTER1( hSRS, "OSRImportFromPCI", OGRERR_FAILURE );

    return ((OGRSpatialReference *) hSRS)->importFromPCI( pszProj,
                                                          pszUnits,
                                                          padfPrjParams );
}

/************************************************************************/
/*                          importFromPCI()                             */
/************************************************************************/

/**
 * \brief Import coordinate system from PCI projection definition.
 *
 * PCI software uses 16-character string to specify coordinate system
 * and datum/ellipsoid. You should supply at least this string to the
 * importFromPCI() function.
 *
 * This function is the equivalent of the C function OSRImportFromPCI().
 *
 * @param pszProj NULL terminated string containing the definition. Looks
 * like "pppppppppppp Ennn" or "pppppppppppp Dnnn", where "pppppppppppp" is
 * a projection code, "Ennn" is an ellipsoid code, "Dnnn" --- a datum code.
 *
 * @param pszUnits Grid units code ("DEGREE" or "METRE"). If NULL "METRE" will
 * be used.
 *
 * @param padfPrjParams Array of 17 coordinate system parameters:
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
 * zero. If NULL is supplied instead of an array pointer, default values will be
 * used (i.e., zeroes).
 *
 * @return OGRERR_NONE on success or an error code in case of failure.
 */

OGRErr OGRSpatialReference::importFromPCI( const char *pszProj,
                                           const char *pszUnits,
                                           double *padfPrjParams )

{
    Clear();

    if( pszProj == NULL ||
        CPLStrnlen(pszProj, knProjSize) < static_cast<size_t>(knProjSize) )
    {
        return OGRERR_CORRUPT_DATA;
    }

    CPLDebug( "OSR_PCI", "Trying to import projection \"%s\"", pszProj );

/* -------------------------------------------------------------------- */
/*      Use safe defaults if projection parameters are not supplied.    */
/* -------------------------------------------------------------------- */
    bool bProjAllocated = false;

    if( padfPrjParams == NULL )
    {
        padfPrjParams = static_cast<double *>(CPLMalloc( 17 * sizeof(double) ));
        if( !padfPrjParams )
            return OGRERR_NOT_ENOUGH_MEMORY;
        for( int i = 0; i < 17; i++ )
            padfPrjParams[i] = 0.0;
        bProjAllocated = true;
    }

/* -------------------------------------------------------------------- */
/*      Extract and "normalize" the earthmodel to look like E001,       */
/*      D-02 or D109.                                                   */
/* -------------------------------------------------------------------- */
    char szEarthModel[5] = {};

    strcpy( szEarthModel, "" );
    const char *pszEM = pszProj + strlen(pszProj) - 1;
    while( pszEM != pszProj )
    {
        if( *pszEM == 'e' || *pszEM == 'E' || *pszEM == 'd' || *pszEM == 'D' )
        {
            int nCode = atoi(pszEM+1);

            if( nCode >= -99 && nCode <= 999 )
                snprintf( szEarthModel, sizeof(szEarthModel),
                          "%c%03d", toupper(*pszEM), nCode );

            break;
        }

        pszEM--;
    }

    const bool bIsNAD27 =
        EQUAL(pszEM, "E000")
        || EQUAL(pszEM, "D-01")
        || EQUAL(pszEM, "D-03")
        || EQUAL(pszEM, "D-07")
        || EQUAL(pszEM, "D-09")
        || EQUAL(pszEM, "D-11")
        || EQUAL(pszEM, "D-13")
        || EQUAL(pszEM, "D-17");


/* -------------------------------------------------------------------- */
/*      Operate on the basis of the projection name.                    */
/* -------------------------------------------------------------------- */
    if( STARTS_WITH_CI(pszProj, "LONG/LAT") )
    {
        // TODO(schwehr): A NOP is okay?
    }
    else if( STARTS_WITH_CI(pszProj, "METER")
             || STARTS_WITH_CI(pszProj, "METRE") )
    {
        SetLocalCS( "METER" );
        SetLinearUnits( "METER", 1.0 );
    }
    else if( STARTS_WITH_CI(pszProj, "FEET")
             || STARTS_WITH_CI(pszProj, "FOOT") )
    {
        SetLocalCS( "FEET" );
        SetLinearUnits( "FEET", CPLAtof(SRS_UL_FOOT_CONV) );
    }
    else if( STARTS_WITH_CI(pszProj, "ACEA") )
    {
        SetACEA( padfPrjParams[4], padfPrjParams[5],
                 padfPrjParams[3], padfPrjParams[2],
                 padfPrjParams[6], padfPrjParams[7] );
    }
    else if( STARTS_WITH_CI(pszProj, "AE") )
    {
        SetAE( padfPrjParams[3], padfPrjParams[2],
               padfPrjParams[6], padfPrjParams[7] );
    }
    else if( STARTS_WITH_CI(pszProj, "CASS ") )
    {
        SetCS( padfPrjParams[3], padfPrjParams[2],
               padfPrjParams[6], padfPrjParams[7] );
    }
    else if( STARTS_WITH_CI(pszProj, "EC") )
    {
        SetEC( padfPrjParams[4], padfPrjParams[5],
               padfPrjParams[3], padfPrjParams[2],
               padfPrjParams[6], padfPrjParams[7] );
    }
    else if( STARTS_WITH_CI(pszProj, "ER") )
    {
        // PCI and GCTP don't support natural origin lat.
        SetEquirectangular2( 0.0, padfPrjParams[2],
                             padfPrjParams[3],
                             padfPrjParams[6], padfPrjParams[7] );
    }
    else if( STARTS_WITH_CI(pszProj, "GNO") )
    {
        SetGnomonic( padfPrjParams[3], padfPrjParams[2],
                     padfPrjParams[6], padfPrjParams[7] );
    }
    // FIXME: GVNP --- General Vertical Near- Side Perspective skipped.
    // FIXME: GOOD -- Our Goode's is not the interrupted version from PCI.
    else if( STARTS_WITH_CI(pszProj, "LAEA") )
    {
        SetLAEA( padfPrjParams[3], padfPrjParams[2],
                 padfPrjParams[6], padfPrjParams[7] );
    }
    else if( STARTS_WITH_CI(pszProj, "LCC ") )
    {
        SetLCC( padfPrjParams[4], padfPrjParams[5],
                padfPrjParams[3], padfPrjParams[2],
                padfPrjParams[6], padfPrjParams[7] );
    }
    else if( STARTS_WITH_CI(pszProj, "LCC_1SP ") )
    {
        SetLCC1SP( padfPrjParams[3], padfPrjParams[2],
                   padfPrjParams[8],
                   padfPrjParams[6], padfPrjParams[7] );
    }
    else if( STARTS_WITH_CI(pszProj, "MC") )
    {
        SetMC( padfPrjParams[3], padfPrjParams[2],
               padfPrjParams[6], padfPrjParams[7] );
    }
    else if( STARTS_WITH_CI(pszProj, "MER") )
    {
        SetMercator( padfPrjParams[3], padfPrjParams[2],
                     (padfPrjParams[8] != 0.0) ? padfPrjParams[8] : 1.0,
                     padfPrjParams[6], padfPrjParams[7] );
    }
    else if( STARTS_WITH_CI(pszProj, "OG") )
    {
        SetOrthographic( padfPrjParams[3], padfPrjParams[2],
                         padfPrjParams[6], padfPrjParams[7] );
    }
    else if( STARTS_WITH_CI(pszProj, "OM ") )
    {
        if( padfPrjParams[10] == 0.0
            && padfPrjParams[11] == 0.0
            && padfPrjParams[12] == 0.0
            && padfPrjParams[13] == 0.0 )
        {
            SetHOM( padfPrjParams[3], padfPrjParams[2],
                    padfPrjParams[14],
                    padfPrjParams[14], // Use azimuth for grid angle.
                    padfPrjParams[8],
                    padfPrjParams[6], padfPrjParams[7] );
        }
        else
        {
            SetHOM2PNO( padfPrjParams[3],
                        padfPrjParams[11], padfPrjParams[10],
                        padfPrjParams[13], padfPrjParams[12],
                        padfPrjParams[8],
                        padfPrjParams[6], padfPrjParams[7] );
        }
    }
    else if( STARTS_WITH_CI(pszProj, "PC") )
    {
        SetPolyconic( padfPrjParams[3], padfPrjParams[2],
                      padfPrjParams[6], padfPrjParams[7] );
    }
    else if( STARTS_WITH_CI(pszProj, "PS") )
    {
        SetPS( padfPrjParams[3], padfPrjParams[2],
               (padfPrjParams[8] != 0.0) ? padfPrjParams[8] : 1.0,
               padfPrjParams[6], padfPrjParams[7] );
    }
    else if( STARTS_WITH_CI(pszProj, "ROB") )
    {
        SetRobinson( padfPrjParams[2],
                     padfPrjParams[6], padfPrjParams[7] );
    }

    else if( STARTS_WITH_CI(pszProj, "SGDO") )
    {
        SetOS( padfPrjParams[3], padfPrjParams[2],
               (padfPrjParams[8] != 0.0) ? padfPrjParams[8] : 1.0,
               padfPrjParams[6], padfPrjParams[7] );
    }
    else if( STARTS_WITH_CI(pszProj, "SG") )
    {
        SetStereographic( padfPrjParams[3], padfPrjParams[2],
                          (padfPrjParams[8] != 0.0) ? padfPrjParams[8] : 1.0,
                          padfPrjParams[6], padfPrjParams[7] );
    }
    else if( STARTS_WITH_CI(pszProj, "SIN") )
    {
        SetSinusoidal( padfPrjParams[2],
                       padfPrjParams[6], padfPrjParams[7] );
    }
    // FIXME: SOM --- Space Oblique Mercator skipped.
    else if( STARTS_WITH_CI(pszProj, "SPCS") )
    {
        const int iZone =
            static_cast<int>(CPLScanLong( const_cast<char *>(pszProj) + 5, 4 ));

        SetStatePlane( iZone, !bIsNAD27 );
        SetLinearUnitsAndUpdateParameters( SRS_UL_METER, 1.0 );
    }
    else if( STARTS_WITH_CI(pszProj, "SPIF") )
    {
        const int iZone =
            static_cast<int>(CPLScanLong( const_cast<char *>(pszProj) + 5, 4 ));

        SetStatePlane( iZone, !bIsNAD27 );
        SetLinearUnitsAndUpdateParameters( SRS_UL_FOOT,
                                           CPLAtof(SRS_UL_FOOT_CONV) );
    }
    else if( STARTS_WITH_CI(pszProj, "SPAF") )
    {
        const int iZone =
            static_cast<int>(CPLScanLong( const_cast<char *>(pszProj) + 5, 4 ));

        SetStatePlane( iZone, !bIsNAD27 );
        SetLinearUnitsAndUpdateParameters( SRS_UL_US_FOOT,
                                           CPLAtof(SRS_UL_US_FOOT_CONV) );
    }
    else if( STARTS_WITH_CI(pszProj, "TM") )
    {
        SetTM( padfPrjParams[3], padfPrjParams[2],
               (padfPrjParams[8] != 0.0) ? padfPrjParams[8] : 1.0,
               padfPrjParams[6], padfPrjParams[7] );
    }

    else if( STARTS_WITH_CI(pszProj, "UTM") )
    {
        bool bNorth = true;

        int iZone =
            static_cast<int>(CPLScanLong( const_cast<char *>(pszProj) + 4, 5 ));
        if( iZone < 0 )
        {
            iZone = -iZone;
            bNorth = false;
        }

        // Check for a zone letter. PCI uses, accidentally, MGRS
        // type row lettering in its UTM projection.
        char byZoneID = 0;

        if( strlen(pszProj) > 10 && pszProj[10] != ' ' )
            byZoneID = pszProj[10];

        // Determine if the MGRS zone falls above or below the equator.
        if( byZoneID != 0 )
        {
            CPLDebug("OSR_PCI", "Found MGRS zone in UTM projection string: %c",
                     byZoneID);

            if( byZoneID >= 'N' && byZoneID <= 'X' )
            {
                bNorth = true;
            }
            else if( byZoneID >= 'C' && byZoneID <= 'M' )
            {
                bNorth = false;
            }
            else
            {
                // Yikes.  Most likely we got something that was not really
                // an MGRS zone code so we ignore it.
            }
        }

        SetUTM( iZone, bNorth );
    }
    else if( STARTS_WITH_CI(pszProj, "VDG") )
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
/*      We have an earthmodel string, look it up in the datum list.     */
/* -------------------------------------------------------------------- */
    if( strlen(szEarthModel) > 0
        && (poRoot == NULL || IsProjected() || IsGeographic()) )
    {
        const PCIDatums *pasDatum = asDatums;

        // Search for matching datum.
        while( pasDatum->pszPCIDatum )
        {
            if( EQUALN( szEarthModel, pasDatum->pszPCIDatum, 4 ) )
            {
                OGRSpatialReference oGCS;
                oGCS.importFromEPSG( pasDatum->nEPSGCode );
                CopyGeogCSFrom( &oGCS );
                break;
            }
            pasDatum++;
        }

/* -------------------------------------------------------------------- */
/*      If we did not find a datum definition in our in-code EPSG        */
/*      lookup table, then try fetching from the pci_datum.txt          */
/*      file.                                                           */
/* -------------------------------------------------------------------- */
        char **papszDatumDefn = NULL;

        if( !pasDatum->pszPCIDatum && szEarthModel[0] == 'D' )
        {
            const char *pszDatumCSV = CSVFilename( "pci_datum.txt" );
            VSILFILE *fp = pszDatumCSV ? VSIFOpenL( pszDatumCSV, "r" ) : NULL;

            if( fp != NULL )
            {
                char **papszLineItems = NULL;

                while( (papszLineItems = CSVReadParseLineL( fp )) != NULL )
                {
                    if( CSLCount(papszLineItems) > 3
                        && EQUALN(papszLineItems[0], szEarthModel,
                                  sizeof(szEarthModel)-1) )
                    {
                        papszDatumDefn = papszLineItems;
                        strncpy( szEarthModel, papszLineItems[2],
                                 sizeof(szEarthModel)-1 );
                        break;
                    }
                    CSLDestroy( papszLineItems );
                }

                VSIFCloseL( fp );
            }
        }

/* -------------------------------------------------------------------- */
/*      If not, look in the ellipsoid/EPSG matching list.               */
/* -------------------------------------------------------------------- */
        if( !pasDatum->pszPCIDatum )  // No matching; search for ellipsoids.
        {
            char *pszName = NULL;
            double dfSemiMajor = 0.0;
            double dfInvFlattening = 0.0;
            int nEPSGCode = 0;

            pasDatum = asEllips;

            while( pasDatum->pszPCIDatum )
            {
                if( EQUALN( szEarthModel, pasDatum->pszPCIDatum, 4 ) )
                {
                    nEPSGCode = pasDatum->nEPSGCode;
                    CPL_IGNORE_RET_VAL(
                        OSRGetEllipsoidInfo( pasDatum->nEPSGCode, &pszName,
                                             &dfSemiMajor, &dfInvFlattening ));
                    break;
                }
                pasDatum++;
            }

/* -------------------------------------------------------------------- */
/*      If we don't find it in that list, do a lookup in the            */
/*      pci_ellips.txt file.                                            */
/* -------------------------------------------------------------------- */
            if( !pasDatum->pszPCIDatum && szEarthModel[0] == 'E' )
            {
                const char *pszCSV = CSVFilename( "pci_ellips.txt" );
                VSILFILE *fp = pszCSV ? VSIFOpenL( pszCSV, "r" ) : NULL;

                if( fp != NULL )
                {
                    char **papszLineItems = NULL;

                    while( (papszLineItems = CSVReadParseLineL( fp )) != NULL )
                    {
                        if( CSLCount(papszLineItems) > 3
                            && EQUALN(papszLineItems[0], szEarthModel, 4) )
                        {
                            dfSemiMajor = CPLAtof( papszLineItems[2] );
                            const double dfSemiMinor =
                                CPLAtof( papszLineItems[3] );
                            dfInvFlattening =
                                OSRCalcInvFlattening(dfSemiMajor, dfSemiMinor);
                            break;
                        }
                        CSLDestroy( papszLineItems );
                    }
                    CSLDestroy( papszLineItems );

                    VSIFCloseL( fp );
                }
            }

/* -------------------------------------------------------------------- */
/*      Custom spheroid?                                                */
/* -------------------------------------------------------------------- */
            if( dfSemiMajor == 0.0 && STARTS_WITH_CI(szEarthModel, "E999")
                && padfPrjParams[0] != 0.0 )
            {
                dfSemiMajor = padfPrjParams[0];
                double dfSemiMinor = padfPrjParams[1];
                dfInvFlattening =
                    OSRCalcInvFlattening(dfSemiMajor, dfSemiMinor);
            }

/* -------------------------------------------------------------------- */
/*      If nothing else, fall back to WGS84 parameters.                 */
/* -------------------------------------------------------------------- */
            if( dfSemiMajor == 0.0 )
            {
                dfSemiMajor = SRS_WGS84_SEMIMAJOR;
                dfInvFlattening = SRS_WGS84_INVFLATTENING;
            }

/* -------------------------------------------------------------------- */
/*      Now try to put this all together into a GEOGCS definition.      */
/* -------------------------------------------------------------------- */

            CPLString osEllipseName;
            if( pszName )
                osEllipseName = pszName;
            else
                osEllipseName.Printf( "Unknown - PCI %s", szEarthModel );
            CPLFree( pszName );

            CPLString osDatumName;
            if( papszDatumDefn )
                osDatumName = papszDatumDefn[1];
            else
                osDatumName.Printf( "Unknown - PCI %s", szEarthModel );

            const CPLString osGCSName = osDatumName;

            SetGeogCS( osGCSName, osDatumName, osEllipseName,
                       dfSemiMajor, dfInvFlattening );

            // Do we have an ellipsoid EPSG code?
            if( nEPSGCode != 0 )
                SetAuthority( "SPHEROID", "EPSG", nEPSGCode );

            // Do we have 7 datum shift parameters?
            if( papszDatumDefn != NULL &&
                CSLCount(papszDatumDefn) >= 15
                && CPLAtof(papszDatumDefn[14]) != 0.0 )
            {
                double dfScale = CPLAtof(papszDatumDefn[14]);

                // we want scale in parts per million off 1.0
                // but pci uses a mix of forms.
                if( dfScale >= 0.999 && dfScale <= 1.001 )
                    dfScale = (dfScale - 1.0) * 1000000.0;

                SetTOWGS84( CPLAtof(papszDatumDefn[3]),
                            CPLAtof(papszDatumDefn[4]),
                            CPLAtof(papszDatumDefn[5]),
                            CPLAtof(papszDatumDefn[11]),
                            CPLAtof(papszDatumDefn[12]),
                            CPLAtof(papszDatumDefn[13]),
                            dfScale );
            }

            // Do we have 7 datum shift parameters?
            else if( papszDatumDefn != NULL &&
                     CSLCount(papszDatumDefn) == 11
                     && (CPLAtof(papszDatumDefn[3]) != 0.0
                         || CPLAtof(papszDatumDefn[4]) != 0.0
                         || CPLAtof(papszDatumDefn[5]) != 0.0 ) )
            {
                SetTOWGS84( CPLAtof(papszDatumDefn[3]),
                            CPLAtof(papszDatumDefn[4]),
                            CPLAtof(papszDatumDefn[5]) );
            }
        }

        CSLDestroy(papszDatumDefn);
    }

/* -------------------------------------------------------------------- */
/*      Grid units translation                                          */
/* -------------------------------------------------------------------- */
    if( (IsLocal() || IsProjected()) && pszUnits )
    {
        if( EQUAL( pszUnits, "METRE" ) )
            SetLinearUnits( SRS_UL_METER, 1.0 );
        else if( EQUAL( pszUnits, "DEGREE" ) )
            SetAngularUnits( SRS_UA_DEGREE, CPLAtof(SRS_UA_DEGREE_CONV) );
        else
            SetLinearUnits( SRS_UL_METER, 1.0 );
    }

    FixupOrdering();

    if( bProjAllocated && padfPrjParams )
        CPLFree( padfPrjParams );

    return OGRERR_NONE;
}

/************************************************************************/
/*                          OSRExportToPCI()                            */
/************************************************************************/
/**
 * \brief Export coordinate system in PCI projection definition.
 *
 * This function is the same as OGRSpatialReference::exportToPCI().
 */
OGRErr OSRExportToPCI( OGRSpatialReferenceH hSRS,
                       char **ppszProj, char **ppszUnits,
                       double **ppadfPrjParams )

{
    VALIDATE_POINTER1( hSRS, "OSRExportToPCI", OGRERR_FAILURE );

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
 * \brief Export coordinate system in PCI projection definition.
 *
 * Converts the loaded coordinate reference system into PCI projection
 * definition to the extent possible. The strings returned in ppszProj,
 * ppszUnits and ppadfPrjParams array should be deallocated by the caller
 * with CPLFree() when no longer needed.
 *
 * LOCAL_CS coordinate systems are not translatable.  An empty string
 * will be returned along with OGRERR_NONE.
 *
 * This method is the equivalent of the C function OSRExportToPCI().
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
    const char *pszProjection = GetAttrValue("PROJECTION");

/* -------------------------------------------------------------------- */
/*      Fill all projection parameters with zero.                       */
/* -------------------------------------------------------------------- */
    *ppadfPrjParams = static_cast<double *>(CPLMalloc( 17 * sizeof(double) ));
    for( int i = 0; i < 17; i++ )
        (*ppadfPrjParams)[i] = 0.0;

/* -------------------------------------------------------------------- */
/*      Get the prime meridian info.                                    */
/* -------------------------------------------------------------------- */
#if 0
    const OGR_SRSNode *poPRIMEM = GetAttrNode( "PRIMEM" );
    double dfFromGreenwich = 0.0;

    if( poPRIMEM != NULL && poPRIMEM->GetChildCount() >= 2
        && CPLAtof(poPRIMEM->GetChild(1)->GetValue()) != 0.0 )
    {
        dfFromGreenwich = CPLAtof(poPRIMEM->GetChild(1)->GetValue());
    }
#endif

/* ==================================================================== */
/*      Handle the projection definition.                               */
/* ==================================================================== */
    char szProj[knProjSize + 1] = {};

    if( IsLocal() )
    {
        if( GetLinearUnits() > 0.30479999 && GetLinearUnits() < 0.3048010 )
            CPLPrintStringFill( szProj, "FEET", knProjSize );
        else
            CPLPrintStringFill( szProj, "METER", knProjSize );
    }
    else if( pszProjection == NULL )
    {
        CPLPrintStringFill( szProj, "LONG/LAT", knProjSize );
    }
    else if( EQUAL(pszProjection, SRS_PT_ALBERS_CONIC_EQUAL_AREA) )
    {
        CPLPrintStringFill( szProj, "ACEA", knProjSize );
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
        CPLPrintStringFill( szProj, "AE", knProjSize );
        (*ppadfPrjParams)[2] = GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 );
        (*ppadfPrjParams)[3] =
            GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 );
        (*ppadfPrjParams)[6] = GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 );
        (*ppadfPrjParams)[7] = GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 );
    }
    else if( EQUAL(pszProjection, SRS_PT_CASSINI_SOLDNER) )
    {
        CPLPrintStringFill( szProj, "CASS", knProjSize );
        (*ppadfPrjParams)[2] = GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 );
        (*ppadfPrjParams)[3] =
            GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 );
        (*ppadfPrjParams)[6] = GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 );
        (*ppadfPrjParams)[7] = GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 );
    }
    else if( EQUAL(pszProjection, SRS_PT_EQUIDISTANT_CONIC) )
    {
        CPLPrintStringFill( szProj, "EC", knProjSize );
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
        CPLPrintStringFill( szProj, "ER", knProjSize );
        (*ppadfPrjParams)[2] = GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 );
        (*ppadfPrjParams)[3] =
            GetNormProjParm( SRS_PP_STANDARD_PARALLEL_1, 0.0 );
        (*ppadfPrjParams)[6] = GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 );
        (*ppadfPrjParams)[7] = GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 );
    }
    else if( EQUAL(pszProjection, SRS_PT_GNOMONIC) )
    {
        CPLPrintStringFill( szProj, "GNO", knProjSize );
        (*ppadfPrjParams)[2] = GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 );
        (*ppadfPrjParams)[3] =
            GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 );
        (*ppadfPrjParams)[6] = GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 );
        (*ppadfPrjParams)[7] = GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 );
    }
    else if( EQUAL(pszProjection, SRS_PT_LAMBERT_AZIMUTHAL_EQUAL_AREA) )
    {
        CPLPrintStringFill( szProj, "LAEA", knProjSize );
        (*ppadfPrjParams)[2] = GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 );
        (*ppadfPrjParams)[3] =
            GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 );
        (*ppadfPrjParams)[6] = GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 );
        (*ppadfPrjParams)[7] = GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 );
    }
    else if( EQUAL(pszProjection, SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP) )
    {
        CPLPrintStringFill( szProj, "LCC", knProjSize );
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
    else if( EQUAL(pszProjection, SRS_PT_LAMBERT_CONFORMAL_CONIC_1SP) )
    {
        CPLPrintStringFill( szProj, "LCC_1SP", knProjSize );
        (*ppadfPrjParams)[2] = GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 );
        (*ppadfPrjParams)[3] =
            GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 );
        (*ppadfPrjParams)[8] = GetNormProjParm( SRS_PP_SCALE_FACTOR, 1.0 );
        (*ppadfPrjParams)[6] = GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 );
        (*ppadfPrjParams)[7] = GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 );
    }
    else if( EQUAL(pszProjection, SRS_PT_MILLER_CYLINDRICAL) )
    {
        CPLPrintStringFill( szProj, "MC", knProjSize );
        (*ppadfPrjParams)[2] = GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 );
        (*ppadfPrjParams)[3] =
            GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 );
        (*ppadfPrjParams)[6] = GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 );
        (*ppadfPrjParams)[7] = GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 );
    }
    else if( EQUAL(pszProjection, SRS_PT_MERCATOR_1SP) )
    {
        CPLPrintStringFill( szProj, "MER", knProjSize );
        (*ppadfPrjParams)[2] = GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 );
        (*ppadfPrjParams)[3] =
            GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 );
        (*ppadfPrjParams)[6] = GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 );
        (*ppadfPrjParams)[7] = GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 );
        (*ppadfPrjParams)[8] = GetNormProjParm( SRS_PP_SCALE_FACTOR, 1.0 );
    }
    else if( EQUAL(pszProjection, SRS_PT_ORTHOGRAPHIC) )
    {
        CPLPrintStringFill( szProj, "OG", knProjSize );
        (*ppadfPrjParams)[2] = GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 );
        (*ppadfPrjParams)[3] =
            GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 );
        (*ppadfPrjParams)[6] = GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 );
        (*ppadfPrjParams)[7] = GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 );
    }
    else if( EQUAL(pszProjection, SRS_PT_HOTINE_OBLIQUE_MERCATOR) )
    {
        CPLPrintStringFill( szProj, "OM", knProjSize );
        (*ppadfPrjParams)[2] =
            GetNormProjParm( SRS_PP_LONGITUDE_OF_CENTER, 0.0);
        (*ppadfPrjParams)[3] = GetNormProjParm( SRS_PP_LATITUDE_OF_CENTER, 0.0);
        (*ppadfPrjParams)[14] = GetNormProjParm( SRS_PP_AZIMUTH, 0.0);
        // Note: Ignoring rectified_grid_angle which has no PCI analog.
        (*ppadfPrjParams)[8] = GetNormProjParm( SRS_PP_SCALE_FACTOR, 0.0);
        (*ppadfPrjParams)[6] = GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 );
        (*ppadfPrjParams)[7] = GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 );
    }
    else if( EQUAL(pszProjection,
                   SRS_PT_HOTINE_OBLIQUE_MERCATOR_TWO_POINT_NATURAL_ORIGIN) )
    {
        CPLPrintStringFill( szProj, "OM", knProjSize );
        (*ppadfPrjParams)[3] = GetNormProjParm( SRS_PP_LATITUDE_OF_CENTER, 0.0);
        (*ppadfPrjParams)[11] =
            GetNormProjParm(SRS_PP_LATITUDE_OF_POINT_1, 0.0);
        (*ppadfPrjParams)[10] =
            GetNormProjParm(SRS_PP_LONGITUDE_OF_POINT_1, 0.0);
        (*ppadfPrjParams)[13] =
            GetNormProjParm(SRS_PP_LATITUDE_OF_POINT_2, 0.0);
        (*ppadfPrjParams)[12] =
            GetNormProjParm(SRS_PP_LONGITUDE_OF_POINT_2, 0.0);
        (*ppadfPrjParams)[8] = GetNormProjParm( SRS_PP_SCALE_FACTOR, 0.0);
        (*ppadfPrjParams)[6] = GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 );
        (*ppadfPrjParams)[7] = GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 );
    }
    else if( EQUAL(pszProjection, SRS_PT_POLYCONIC) )
    {
        CPLPrintStringFill( szProj, "PC", knProjSize );
        (*ppadfPrjParams)[2] = GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 );
        (*ppadfPrjParams)[3] =
            GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 );
        (*ppadfPrjParams)[6] = GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 );
        (*ppadfPrjParams)[7] = GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 );
    }
    else if( EQUAL(pszProjection, SRS_PT_POLAR_STEREOGRAPHIC) )
    {
        CPLPrintStringFill( szProj, "PS", knProjSize );
        (*ppadfPrjParams)[2] = GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 );
        (*ppadfPrjParams)[3] =
            GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 );
        (*ppadfPrjParams)[6] = GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 );
        (*ppadfPrjParams)[7] = GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 );
        (*ppadfPrjParams)[8] = GetNormProjParm( SRS_PP_SCALE_FACTOR, 1.0 );
    }
    else if( EQUAL(pszProjection, SRS_PT_ROBINSON) )
    {
        CPLPrintStringFill( szProj, "ROB", knProjSize );
        (*ppadfPrjParams)[2] = GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 );
        (*ppadfPrjParams)[6] = GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 );
        (*ppadfPrjParams)[7] = GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 );
    }
    else if( EQUAL(pszProjection, SRS_PT_OBLIQUE_STEREOGRAPHIC) )
    {
        CPLPrintStringFill( szProj, "SGDO", knProjSize );
        (*ppadfPrjParams)[2] = GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 );
        (*ppadfPrjParams)[3] =
            GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 );
        (*ppadfPrjParams)[6] = GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 );
        (*ppadfPrjParams)[7] = GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 );
        (*ppadfPrjParams)[8] = GetNormProjParm( SRS_PP_SCALE_FACTOR, 1.0 );
    }
    else if( EQUAL(pszProjection, SRS_PT_STEREOGRAPHIC) )
    {
        CPLPrintStringFill( szProj, "SG", knProjSize );
        (*ppadfPrjParams)[2] = GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 );
        (*ppadfPrjParams)[3] =
            GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 );
        (*ppadfPrjParams)[6] = GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 );
        (*ppadfPrjParams)[7] = GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 );
        (*ppadfPrjParams)[8] = GetNormProjParm( SRS_PP_SCALE_FACTOR, 1.0 );
    }
    else if( EQUAL(pszProjection, SRS_PT_SINUSOIDAL) )
    {
        CPLPrintStringFill( szProj, "SIN", knProjSize );
        (*ppadfPrjParams)[2] =
            GetNormProjParm( SRS_PP_LONGITUDE_OF_CENTER, 0.0 );
        (*ppadfPrjParams)[6] = GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 );
        (*ppadfPrjParams)[7] = GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 );
    }
    else if( EQUAL(pszProjection, SRS_PT_TRANSVERSE_MERCATOR) )
    {
        int bNorth = FALSE;
        int nZone = GetUTMZone( &bNorth );

        if( nZone != 0 )
        {
            CPLPrintStringFill( szProj, "UTM", knProjSize );
            if( bNorth )
                CPLPrintInt32( szProj + 5, nZone, 4 );
            else
                CPLPrintInt32( szProj + 5, -nZone, 4 );
        }
        else
        {
            CPLPrintStringFill( szProj, "TM", knProjSize );
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
        CPLPrintStringFill( szProj, "VDG", knProjSize );
        (*ppadfPrjParams)[2] = GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 );
        (*ppadfPrjParams)[6] = GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 );
        (*ppadfPrjParams)[7] = GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 );
    }
    // Projection unsupported by PCI.
    else
    {
        CPLDebug( "OSR_PCI",
                  "Projection \"%s\" unsupported by PCI. "
                  "PIXEL value will be used.", pszProjection );
        CPLPrintStringFill( szProj, "PIXEL", knProjSize );
    }

/* ==================================================================== */
/*      Translate the earth model.                                      */
/* ==================================================================== */

/* -------------------------------------------------------------------- */
/*      Is this a well known datum?                                     */
/* -------------------------------------------------------------------- */
    const char *pszDatum = GetAttrValue( "DATUM" );
    char szEarthModel[5] = {};

    if( pszDatum == NULL || strlen(pszDatum) == 0 )
    {
        // Do nothing.
    }
    else if( EQUAL( pszDatum, SRS_DN_NAD27 ) )
    {
        CPLPrintStringFill( szEarthModel, "D-01", 4 );
    }
    else if( EQUAL( pszDatum, SRS_DN_NAD83 ) )
    {
        CPLPrintStringFill( szEarthModel, "D-02", 4 );
    }
    else if( EQUAL( pszDatum, SRS_DN_WGS84 ) )
    {
        CPLPrintStringFill( szEarthModel, "D000", 4 );
    }

/* -------------------------------------------------------------------- */
/*      If not a very well known datum, try for an EPSG based           */
/*      translation.                                                    */
/* -------------------------------------------------------------------- */
    if( szEarthModel[0] == '\0' )
    {
        const char *pszAuthority = GetAuthorityName("GEOGCS");

        if( pszAuthority && EQUAL(pszAuthority, "EPSG") )
        {
            const int nGCS_EPSG = atoi(GetAuthorityCode("GEOGCS"));

            for( int i = 0; asDatums[i].nEPSGCode != 0; i++ )
            {
                if( asDatums[i].nEPSGCode == nGCS_EPSG )
                {
                    snprintf( szEarthModel, sizeof(szEarthModel), "%s",
                              asDatums[i].pszPCIDatum );
                    break;
                }
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      If we haven't found something yet, try translating the          */
/*      ellipsoid.                                                      */
/* -------------------------------------------------------------------- */
    if( szEarthModel[0] == '\0' )
    {
        const double dfSemiMajor = GetSemiMajor();
        const double dfInvFlattening = GetInvFlattening();

        const PCIDatums *pasDatum = asEllips;

        while( pasDatum->pszPCIDatum )
        {
            double dfSM = 0.0;
            double dfIF = 0.0;

            if( OSRGetEllipsoidInfo( pasDatum->nEPSGCode, NULL,
                                     &dfSM, &dfIF ) == OGRERR_NONE
                && CPLIsEqual( dfSemiMajor, dfSM )
                && CPLIsEqual( dfInvFlattening, dfIF ) )
            {
                CPLPrintStringFill( szEarthModel, pasDatum->pszPCIDatum, 4 );
                break;
            }

            pasDatum++;
        }

        // Try to find in pci_ellips.txt.
        if( szEarthModel[0] == '\0' )
        {
            const char *pszCSV = CSVFilename( "pci_ellips.txt" );
            const double dfSemiMinor =
                OSRCalcSemiMinorFromInvFlattening(dfSemiMajor, dfInvFlattening);

            VSILFILE *fp = pszCSV ? VSIFOpenL( pszCSV, "r" ) : NULL;

            if( fp != NULL )
            {
                char **papszLineItems = NULL;

                while( (papszLineItems = CSVReadParseLineL( fp )) != NULL )
                {
                    if( CSLCount(papszLineItems) >= 4
                        && CPLIsEqual(dfSemiMajor, CPLAtof(papszLineItems[2]))
                        && CPLIsEqual(dfSemiMinor, CPLAtof(papszLineItems[3])) )
                    {
                        snprintf( szEarthModel, sizeof(szEarthModel), "%s",
                                  papszLineItems[0] );
                        break;
                    }

                    CSLDestroy( papszLineItems );
                }

                CSLDestroy( papszLineItems );
                VSIFCloseL( fp );
            }
        }

        // Custom ellipsoid parameters.
        if( szEarthModel[0] == '\0' )
        {
            CPLPrintStringFill( szEarthModel, "E999", 4 );
            (*ppadfPrjParams)[0] = dfSemiMajor;
            (*ppadfPrjParams)[1] =
                OSRCalcSemiMinorFromInvFlattening(dfSemiMajor, dfInvFlattening);
        }
    }

/* -------------------------------------------------------------------- */
/*      If we have a non-parameteric ellipsoid, scan the                */
/*      pci_datum.txt for a match.                                      */
/* -------------------------------------------------------------------- */
    if( szEarthModel[0] == 'E'
        && !EQUAL(szEarthModel, "E999")
        && pszDatum != NULL )
    {
        const char *pszDatumCSV = CSVFilename( "pci_datum.txt" );
        double adfTOWGS84[7] = {};
        const bool bHaveTOWGS84 = GetTOWGS84(adfTOWGS84, 7) == OGRERR_NONE;

        VSILFILE *fp = pszDatumCSV ? VSIFOpenL( pszDatumCSV, "r" ) : NULL;

        if( fp != NULL )
        {
            char **papszLineItems = NULL;

            while( (papszLineItems = CSVReadParseLineL( fp )) != NULL )
            {
                // Compare based on datum name.  This is mostly for
                // PCI round-tripping.  We won't usually get exact matches
                // from other sources.
                if( CSLCount(papszLineItems) > 3
                    && EQUAL(papszLineItems[1], pszDatum)
                    && EQUAL(papszLineItems[2], szEarthModel) )
                {
                    snprintf( szEarthModel, sizeof(szEarthModel), "%s",
                              papszLineItems[0] );
                    break;
                }

                bool bTOWGS84Match = bHaveTOWGS84;

                if( CSLCount(papszLineItems) < 11 )
                    bTOWGS84Match = false;

                if( bTOWGS84Match
                    && (!CPLIsEqual(adfTOWGS84[0], CPLAtof(papszLineItems[3]))
                        || !CPLIsEqual(adfTOWGS84[1],
                                       CPLAtof(papszLineItems[4]))
                        || !CPLIsEqual(adfTOWGS84[2],
                                       CPLAtof(papszLineItems[5]))))
                    bTOWGS84Match = false;

                if( bTOWGS84Match && CSLCount(papszLineItems) >= 15
                    && (!CPLIsEqual(adfTOWGS84[3], CPLAtof(papszLineItems[11]))
                        || !CPLIsEqual(adfTOWGS84[4],
                                       CPLAtof(papszLineItems[12]))
                        || !CPLIsEqual(adfTOWGS84[5],
                                       CPLAtof(papszLineItems[13]))))
                    bTOWGS84Match = false;

                if( bTOWGS84Match && CSLCount(papszLineItems) >= 15 )
                {
                    double dfScale = CPLAtof(papszLineItems[14]);

                    // Convert to parts per million if is a 1 based scaling.
                    if( dfScale >= 0.999 && dfScale <= 1.001 )
                        dfScale = (dfScale-1.0) * 1000000.0;

                    if( !CPLIsEqual(adfTOWGS84[6], dfScale) )
                        bTOWGS84Match = false;
                }

                if( bTOWGS84Match && CSLCount(papszLineItems) < 15
                    && (!CPLIsEqual(adfTOWGS84[3], 0.0)
                        || !CPLIsEqual(adfTOWGS84[4], 0.0)
                        || !CPLIsEqual(adfTOWGS84[5], 0.0)
                        || !CPLIsEqual(adfTOWGS84[6], 0.0)) )
                    bTOWGS84Match = false;

                if( bTOWGS84Match )
                {
                    snprintf( szEarthModel, sizeof(szEarthModel), "%s",
                              papszLineItems[0] );
                    break;
                }

                CSLDestroy( papszLineItems );
            }

            CSLDestroy( papszLineItems );
            VSIFCloseL( fp );
        }
    }

    CPLPrintStringFill( szProj + 12, szEarthModel, 4 );

    CPLDebug( "OSR_PCI", "Translated as '%s'", szProj );

/* -------------------------------------------------------------------- */
/*      Translate the linear units.                                     */
/* -------------------------------------------------------------------- */
    const char *pszUnits =
        STARTS_WITH_CI(szProj, "LONG/LAT") ? "DEGREE" : "METRE";

/* -------------------------------------------------------------------- */
/*      Report results.                                                 */
/* -------------------------------------------------------------------- */
    szProj[knProjSize] = '\0';
    *ppszProj = CPLStrdup( szProj );

    *ppszUnits = CPLStrdup( pszUnits );

    return OGRERR_NONE;
}
