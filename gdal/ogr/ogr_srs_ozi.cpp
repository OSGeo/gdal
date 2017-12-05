/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  OGRSpatialReference translation from OziExplorer
 *           georeferencing information.
 * Author:   Andrey Kiselev, dron@ak4719.spb.edu
 *
 ******************************************************************************
 * Copyright (c) 2009, Andrey Kiselev <dron@ak4719.spb.edu>
 * Copyright (c) 2009-2012, Even Rouault <even dot rouault at mines-paris dot org>
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
#include "ogr_spatialref.h"

#include <cstdlib>
#include <cstring>

#include "cpl_conv.h"
#include "cpl_csv.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "ogr_core.h"
#include "ogr_srs_api.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                          OSRImportFromOzi()                          */
/************************************************************************/

/**
 * Import coordinate system from OziExplorer projection definition.
 *
 * This function will import projection definition in style, used by
 * OziExplorer software.
 *
 * Note: another version of this function with a different signature existed
 * in GDAL 1.X.
 *
 * @param hSRS spatial reference object.
 * @param papszLines Map file lines. This is an array of strings containing
 * the whole OziExplorer .MAP file. The array is terminated by a NULL pointer.
 *
 * @return OGRERR_NONE on success or an error code in case of failure.
 *
 * @since OGR 2.0
 */

OGRErr OSRImportFromOzi( OGRSpatialReferenceH hSRS,
                         const char * const* papszLines )

{
    VALIDATE_POINTER1( hSRS, "OSRImportFromOzi", OGRERR_FAILURE );

    return ((OGRSpatialReference *) hSRS)->importFromOzi( papszLines );
}

/************************************************************************/
/*                            importFromOzi()                           */
/************************************************************************/

/**
 * Import coordinate system from OziExplorer projection definition.
 *
 * This method will import projection definition in style, used by
 * OziExplorer software.
 *
 * @param papszLines Map file lines. This is an array of strings containing
 * the whole OziExplorer .MAP file. The array is terminated by a NULL pointer.
 *
 * @return OGRERR_NONE on success or an error code in case of failure.
 *
 * @since OGR 1.10
 */

OGRErr OGRSpatialReference::importFromOzi( const char * const* papszLines )
{
    const char *pszDatum;
    const char *pszProj = NULL;
    const char *pszProjParms = NULL;

    Clear();

    const int nLines = CSLCount((char**)papszLines);
    if( nLines < 5 )
        return OGRERR_NOT_ENOUGH_DATA;

    pszDatum = papszLines[4];

    for( int iLine = 5; iLine < nLines; iLine++ )
    {
        if( STARTS_WITH_CI(papszLines[iLine], "Map Projection") )
        {
            pszProj = papszLines[iLine];
        }
        else if( STARTS_WITH_CI(papszLines[iLine], "Projection Setup") )
        {
            pszProjParms = papszLines[iLine];
        }
    }

    if( !(pszDatum && pszProj && pszProjParms) )
        return OGRERR_NOT_ENOUGH_DATA;

/* -------------------------------------------------------------------- */
/*      Operate on the basis of the projection name.                    */
/* -------------------------------------------------------------------- */
    char **papszProj = CSLTokenizeStringComplex( pszProj, ",", TRUE, TRUE );
    char **papszProjParms = CSLTokenizeStringComplex( pszProjParms, ",",
                                                      TRUE, TRUE );
    char **papszDatum = NULL;

    if( CSLCount(papszProj) < 2 )
    {
        goto not_enough_data;
    }

    if( STARTS_WITH_CI(papszProj[1], "Latitude/Longitude") )
    {
        // Do nothing.
    }
    else if( STARTS_WITH_CI(papszProj[1], "Mercator") )
    {
        if( CSLCount(papszProjParms) < 6 ) goto not_enough_data;
        double dfScale = CPLAtof(papszProjParms[3]);
        // If unset, default to scale = 1.
        if( papszProjParms[3][0] == 0 ) dfScale = 1;
        SetMercator( CPLAtof(papszProjParms[1]), CPLAtof(papszProjParms[2]),
                     dfScale,
                     CPLAtof(papszProjParms[4]), CPLAtof(papszProjParms[5]) );
    }
    else if( STARTS_WITH_CI(papszProj[1], "Transverse Mercator") )
    {
        if( CSLCount(papszProjParms) < 6 ) goto not_enough_data;
        SetTM( CPLAtof(papszProjParms[1]), CPLAtof(papszProjParms[2]),
               CPLAtof(papszProjParms[3]),
               CPLAtof(papszProjParms[4]), CPLAtof(papszProjParms[5]) );
    }
    else if( STARTS_WITH_CI(papszProj[1], "Lambert Conformal Conic") )
    {
        if( CSLCount(papszProjParms) < 8 ) goto not_enough_data;
        SetLCC( CPLAtof(papszProjParms[6]), CPLAtof(papszProjParms[7]),
                CPLAtof(papszProjParms[1]), CPLAtof(papszProjParms[2]),
                CPLAtof(papszProjParms[4]), CPLAtof(papszProjParms[5]) );
    }
    else if( STARTS_WITH_CI(papszProj[1], "Sinusoidal") )
    {
        if( CSLCount(papszProjParms) < 6 ) goto not_enough_data;
        SetSinusoidal( CPLAtof(papszProjParms[2]),
                       CPLAtof(papszProjParms[4]), CPLAtof(papszProjParms[5]) );
    }
    else if( STARTS_WITH_CI(papszProj[1], "Albers Equal Area") )
    {
        if( CSLCount(papszProjParms) < 8 ) goto not_enough_data;
        SetACEA( CPLAtof(papszProjParms[6]), CPLAtof(papszProjParms[7]),
                 CPLAtof(papszProjParms[1]), CPLAtof(papszProjParms[2]),
                 CPLAtof(papszProjParms[4]), CPLAtof(papszProjParms[5]) );
    }
    else if( STARTS_WITH_CI(
                 papszProj[1], "(UTM) Universal Transverse Mercator") &&
             nLines > 5 )
    {
        // Look for the UTM zone in the calibration point data.
        int iLine = 5;  // Used after for.
        for( ; iLine < nLines; iLine++ )
        {
            if( STARTS_WITH_CI(papszLines[iLine], "Point") )
            {
                char **papszTok =
                    CSLTokenizeString2(papszLines[iLine], ",",
                                       CSLT_ALLOWEMPTYTOKENS
                                       | CSLT_STRIPLEADSPACES
                                       | CSLT_STRIPENDSPACES);
                if( CSLCount(papszTok) < 17
                    || EQUAL(papszTok[2], "")
                    || EQUAL(papszTok[13], "")
                    || EQUAL(papszTok[14], "")
                    || EQUAL(papszTok[15], "")
                    || EQUAL(papszTok[16], "") )
                {
                    CSLDestroy(papszTok);
                    continue;
                }
                SetUTM( atoi(papszTok[13]), EQUAL(papszTok[16], "N") );
                CSLDestroy(papszTok);
                break;
            }
        }
        if( iLine == nLines )  // Try to guess the UTM zone.
        {
            float fMinLongitude = 1000.0f;
            float fMaxLongitude = -1000.0f;
            float fMinLatitude = 1000.0f;
            float fMaxLatitude = -1000.0f;
            bool bFoundMMPLL = false;
            for( iLine = 5; iLine < nLines; iLine++ )
            {
                if( STARTS_WITH_CI(papszLines[iLine], "MMPLL") )
                {
                    char **papszTok =
                        CSLTokenizeString2(papszLines[iLine], ",",
                                           CSLT_ALLOWEMPTYTOKENS
                                           | CSLT_STRIPLEADSPACES
                                           | CSLT_STRIPENDSPACES);
                    if( CSLCount(papszTok) < 4 )
                    {
                        CSLDestroy(papszTok);
                        continue;
                    }
                    const float fLongitude =
                        static_cast<float>(CPLAtofM(papszTok[2]));
                    const float fLatitude =
                        static_cast<float>(CPLAtofM(papszTok[3]));
                    CSLDestroy(papszTok);

                    bFoundMMPLL = true;

                    if( fMinLongitude > fLongitude )
                        fMinLongitude = fLongitude;
                    if( fMaxLongitude < fLongitude )
                        fMaxLongitude = fLongitude;
                    if( fMinLatitude > fLatitude )
                        fMinLatitude = fLatitude;
                    if( fMaxLatitude < fLatitude )
                        fMaxLatitude = fLatitude;
                }
            }
            const float fMedianLatitude = (fMinLatitude + fMaxLatitude) / 2;
            const float fMedianLongitude = (fMinLongitude + fMaxLongitude) / 2;
            if( bFoundMMPLL && fMaxLatitude <= 90 )
            {
                int nUtmZone = 0;
                if( fMedianLatitude >= 56 && fMedianLatitude <= 64 &&
                    fMedianLongitude >= 3 && fMedianLongitude <= 12 )
                    nUtmZone = 32;  // Norway exception.
                else if( fMedianLatitude >= 72 && fMedianLatitude <= 84 &&
                         fMedianLongitude >= 0 && fMedianLongitude <= 42 )
                    // Svalbard exception.
                    nUtmZone =
                        static_cast<int>((fMedianLongitude + 3) / 12) * 2 + 31;
                else
                    nUtmZone =
                        static_cast<int>((fMedianLongitude + 180 ) / 6) + 1;
                SetUTM( nUtmZone, fMedianLatitude >= 0 );
            }
            else
            {
                CPLDebug( "OSR_Ozi", "UTM Zone not found");
            }
        }
    }
    else if( STARTS_WITH_CI(papszProj[1], "(I) France Zone I") )
    {
        SetLCC1SP( 49.5, 2.337229167, 0.99987734, 600000, 1200000 );
    }
    else if( STARTS_WITH_CI(papszProj[1], "(II) France Zone II") )
    {
        SetLCC1SP( 46.8, 2.337229167, 0.99987742, 600000, 2200000 );
    }
    else if( STARTS_WITH_CI(papszProj[1], "(III) France Zone III") )
    {
        SetLCC1SP( 44.1, 2.337229167, 0.99987750, 600000, 3200000 );
    }
    else if( STARTS_WITH_CI(papszProj[1], "(IV) France Zone IV") )
    {
        SetLCC1SP( 42.165, 2.337229167, 0.99994471, 234.358, 4185861.369 );
    }

/*
 *  Note: The following projections have not been implemented yet
 *
 */

/*
    else if( STARTS_WITH_CI(papszProj[1], "(BNG) British National Grid") )
    {
    }
    else if( STARTS_WITH_CI(papszProj[1], "(IG) Irish Grid") )
    {
    }

    else if( STARTS_WITH_CI(papszProj[1], "(NZG) New Zealand Grid") )
    {
    }
    else if( STARTS_WITH_CI(papszProj[1], "(NZTM2) New Zealand TM 2000") )
    {
    }
    else if( STARTS_WITH_CI(papszProj[1], "(SG) Swedish Grid") )
    {
    }
    else if( STARTS_WITH_CI(papszProj[1], "(SUI) Swiss Grid") )
    {
    }
    else if( STARTS_WITH_CI(papszProj[1], "(A)Lambert Azimuthual Equal Area") )
    {
    }
    else if( STARTS_WITH_CI(papszProj[1], "(EQC) Equidistant Conic") )
    {
    }
    else if( STARTS_WITH_CI(papszProj[1], "Polyconic (American)") )
    {
    }
    else if( STARTS_WITH_CI(papszProj[1], "Van Der Grinten") )
    {
    }
    else if( STARTS_WITH_CI(papszProj[1], "Vertical Near-Sided Perspective") )
    {
    }
    else if( STARTS_WITH_CI(papszProj[1], "(WIV) Wagner IV") )
    {
    }
    else if( STARTS_WITH_CI(papszProj[1], "Bonne") )
    {
    }
    else if( STARTS_WITH_CI(papszProj[1],
                            "(MT0) Montana State Plane Zone 2500") )
    {
    }
    else if( STARTS_WITH_CI(papszProj[1], "ITA1) Italy Grid Zone 1") )
    {
    }
    else if( STARTS_WITH_CI(papszProj[1], "ITA2) Italy Grid Zone 2") )
    {
    }
    else if( STARTS_WITH_CI(papszProj[1],
                            "(VICMAP-TM) Victoria Aust.(pseudo AMG)") )
    {
    }
    else if( STARTS_WITH_CI(papszProj[1], "VICGRID) Victoria Australia") )
    {
    }
    else if( STARTS_WITH_CI(papszProj[1],
                            "(VG94) VICGRID94 Victoria Australia") )
    {
    }
    else if( STARTS_WITH_CI(papszProj[1], "Gnomonic") )
    {
    }
*/
    else
    {
        CPLDebug( "OSR_Ozi", "Unsupported projection: \"%s\"", papszProj[1] );
        SetLocalCS( CPLString().Printf("\"Ozi\" projection \"%s\"",
                                       papszProj[1]) );
    }

/* -------------------------------------------------------------------- */
/*      Try to translate the datum/spheroid.                            */
/* -------------------------------------------------------------------- */
    papszDatum = CSLTokenizeString2( pszDatum, ",",
                                     CSLT_ALLOWEMPTYTOKENS
                                     | CSLT_STRIPLEADSPACES
                                     | CSLT_STRIPENDSPACES );
    if( papszDatum == NULL )
        goto not_enough_data;

    if( !IsLocal() )
    {
/* -------------------------------------------------------------------- */
/*      Verify that we can find the CSV file containing the datums      */
/* -------------------------------------------------------------------- */
        if( CSVScanFileByName( CSVFilename( "ozi_datum.csv" ),
                               "EPSG_DATUM_CODE",
                               "4326", CC_Integer ) == NULL )
        {
            CPLError(CE_Failure, CPLE_OpenFailed,
                     "Unable to open OZI support file %s.  "
                     "Try setting the GDAL_DATA environment variable to point "
                     "to the directory containing OZI csv files.",
                     CSVFilename( "ozi_datum.csv" ));
            goto other_error;
        }

/* -------------------------------------------------------------------- */
/*      Search for matching datum                                       */
/* -------------------------------------------------------------------- */
        const char *pszOziDatum = CSVFilename( "ozi_datum.csv" );
        CPLString osDName = CSVGetField( pszOziDatum, "NAME", papszDatum[0],
                                    CC_ApproxString, "NAME" );
        if( osDName.empty() )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                    "Failed to find datum %s in ozi_datum.csv.",
                    papszDatum[0] );
            goto other_error;
        }

        const int nDatumCode =
            atoi( CSVGetField( pszOziDatum, "NAME", papszDatum[0],
                               CC_ApproxString, "EPSG_DATUM_CODE" ) );

        if( nDatumCode > 0 ) // There is a matching EPSG code
        {
            OGRSpatialReference oGCS;
            oGCS.importFromEPSG( nDatumCode );
            CopyGeogCSFrom( &oGCS );
        }
        else // We use the parameters from the CSV files
        {
            CPLString osEllipseCode =
                CSVGetField( pszOziDatum, "NAME", papszDatum[0],
                             CC_ApproxString, "ELLIPSOID_CODE" );
            const double dfDeltaX =
                CPLAtof(CSVGetField( pszOziDatum, "NAME", papszDatum[0],
                                     CC_ApproxString, "DELTAX" ) );
            const double dfDeltaY =
                CPLAtof(CSVGetField( pszOziDatum, "NAME", papszDatum[0],
                                     CC_ApproxString, "DELTAY" ) );
            const double dfDeltaZ =
                CPLAtof(CSVGetField( pszOziDatum, "NAME", papszDatum[0],
                                     CC_ApproxString, "DELTAZ" ) );

    /* -------------------------------------------------------------------- */
    /*     Verify that we can find the CSV file containing the ellipsoids.  */
    /* -------------------------------------------------------------------- */
            if( CSVScanFileByName( CSVFilename( "ozi_ellips.csv" ),
                                   "ELLIPSOID_CODE",
                                   "20", CC_Integer ) == NULL )
            {
                CPLError(
                    CE_Failure, CPLE_OpenFailed,
                    "Unable to open OZI support file %s.  "
                    "Try setting the GDAL_DATA environment variable to point "
                    "to the directory containing OZI csv files.",
                    CSVFilename( "ozi_ellips.csv" ) );
                goto other_error;
            }

    /* -------------------------------------------------------------------- */
    /*      Lookup the ellipse code.                                        */
    /* -------------------------------------------------------------------- */
            const char *pszOziEllipse = CSVFilename( "ozi_ellips.csv" );

            CPLString osEName =
                CSVGetField( pszOziEllipse, "ELLIPSOID_CODE", osEllipseCode,
                             CC_ApproxString, "NAME" );
            if( osEName.empty() )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                        "Failed to find ellipsoid %s in ozi_ellips.csv.",
                        osEllipseCode.c_str() );
                goto other_error;
            }

            const double dfA =
                CPLAtof(CSVGetField( pszOziEllipse, "ELLIPSOID_CODE",
                                     osEllipseCode, CC_ApproxString, "A" ));
            const double dfInvF =
                CPLAtof(CSVGetField( pszOziEllipse, "ELLIPSOID_CODE",
                                     osEllipseCode, CC_ApproxString, "INVF" ));

    /* -------------------------------------------------------------------- */
    /*      Create geographic coordinate system.                            */
    /* -------------------------------------------------------------------- */
            SetGeogCS( osDName, osDName, osEName, dfA, dfInvF );
            SetTOWGS84( dfDeltaX, dfDeltaY, dfDeltaZ );
        }
    }

/* -------------------------------------------------------------------- */
/*      Grid units translation                                          */
/* -------------------------------------------------------------------- */
    if( IsLocal() || IsProjected() )
        SetLinearUnits( SRS_UL_METER, 1.0 );

    FixupOrdering();

    CSLDestroy(papszProj);
    CSLDestroy(papszProjParms);
    CSLDestroy(papszDatum);

    return OGRERR_NONE;

not_enough_data:

    CSLDestroy(papszProj);
    CSLDestroy(papszProjParms);
    CSLDestroy(papszDatum);

    return OGRERR_NOT_ENOUGH_DATA;

other_error:

    CSLDestroy(papszProj);
    CSLDestroy(papszProjParms);
    CSLDestroy(papszDatum);

    return OGRERR_FAILURE;
}
