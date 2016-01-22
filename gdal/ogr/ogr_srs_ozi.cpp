/******************************************************************************
 * $Id$
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

#include "ogr_spatialref.h"
#include "cpl_conv.h"
#include "cpl_csv.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                          OSRImportFromOzi()                          */
/************************************************************************/

OGRErr OSRImportFromOzi( OGRSpatialReferenceH hSRS,
                         const char *pszDatum, const char *pszProj,
                         const char *pszProjParms )

{
    VALIDATE_POINTER1( hSRS, "OSRImportFromOzi", CE_Failure );

    return ((OGRSpatialReference *) hSRS)->importFromOzi( pszDatum, pszProj,
                                                          pszProjParms );
}

/************************************************************************/
/*                            importFromOzi()                           */
/************************************************************************/

/**
 * Note : This method is obsolete, but has been kept to avoid breaking the API.
 *        It can be removed in GDAL 2.0
 */

/**
 * Import coordinate system from OziExplorer projection definition.
 *
 * This method will import projection definition in style, used by
 * OziExplorer software.
 *
 * This function is the equivalent of the C function OSRImportFromOzi().
 *
 * @param pszDatum Datum string. This is a fifth string in the
 * OziExplorer .MAP file.
 *
 * @param pszProj Projection string. Search for line starting with
 * "Map Projection" name in the OziExplorer .MAP file and supply it as a
 * whole in this parameter.
 *
 * @param pszProjParms String containing projection parameters. Search for
 * "Projection Setup" name in the OziExplorer .MAP file and supply it as a
 * whole in this parameter.
 * 
 * @return OGRERR_NONE on success or an error code in case of failure. 
 *
 * @deprecated Use importFromOzi( const char * const* papszLines ) instead
 */

OGRErr OGRSpatialReference::importFromOzi( const char *pszDatum,
                                           const char *pszProj,
                                           const char *pszProjParms )

{
    const char* papszLines[8];

    // Fake
    papszLines[0] = "";
    papszLines[1] = "";
    papszLines[2] = "";
    papszLines[3] = "";
    papszLines[4] = pszDatum; /* Must be in that position */
    papszLines[5] = pszProj; /* Must be after 5th line */
    papszLines[6] = pszProjParms; /* Must be after 5th line */
    papszLines[7] = NULL;

    return importFromOzi(papszLines);
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
    int iLine;
    const char *pszDatum, *pszProj = NULL, *pszProjParms = NULL;

    Clear();

    int nLines = CSLCount((char**)papszLines);
    if( nLines < 5 )
        return OGRERR_NOT_ENOUGH_DATA;

    pszDatum = papszLines[4];

    for ( iLine = 5; iLine < nLines; iLine++ )
    {
        if ( EQUALN(papszLines[iLine], "Map Projection", 14) )
        {
            pszProj = papszLines[iLine];
        }
        else if ( EQUALN(papszLines[iLine], "Projection Setup", 16) )
        {
            pszProjParms = papszLines[iLine];
        }
    }

    if ( ! ( pszDatum && pszProj && pszProjParms ) )
        return OGRERR_NOT_ENOUGH_DATA;

/* -------------------------------------------------------------------- */
/*      Operate on the basis of the projection name.                    */
/* -------------------------------------------------------------------- */
    char    **papszProj = CSLTokenizeStringComplex( pszProj, ",", TRUE, TRUE );
    char    **papszProjParms = CSLTokenizeStringComplex( pszProjParms, ",", 
                                                         TRUE, TRUE );
    char    **papszDatum = NULL;
                                                         
    if (CSLCount(papszProj) < 2)
    {
        goto not_enough_data;
    }

    if ( EQUALN(papszProj[1], "Latitude/Longitude", 18) )
    {
    }

    else if ( EQUALN(papszProj[1], "Mercator", 8) )
    {
        if (CSLCount(papszProjParms) < 6) goto not_enough_data;
        double dfScale = CPLAtof(papszProjParms[3]);
        if (papszProjParms[3][0] == 0) dfScale = 1; /* if unset, default to scale = 1 */
        SetMercator( CPLAtof(papszProjParms[1]), CPLAtof(papszProjParms[2]),
                     dfScale,
                     CPLAtof(papszProjParms[4]), CPLAtof(papszProjParms[5]) );
    }

    else if ( EQUALN(papszProj[1], "Transverse Mercator", 19) )
    {
        if (CSLCount(papszProjParms) < 6) goto not_enough_data;
        SetTM( CPLAtof(papszProjParms[1]), CPLAtof(papszProjParms[2]),
               CPLAtof(papszProjParms[3]),
               CPLAtof(papszProjParms[4]), CPLAtof(papszProjParms[5]) );
    }

    else if ( EQUALN(papszProj[1], "Lambert Conformal Conic", 23) )
    {
        if (CSLCount(papszProjParms) < 8) goto not_enough_data;
        SetLCC( CPLAtof(papszProjParms[6]), CPLAtof(papszProjParms[7]),
                CPLAtof(papszProjParms[1]), CPLAtof(papszProjParms[2]),
                CPLAtof(papszProjParms[4]), CPLAtof(papszProjParms[5]) );
    }

    else if ( EQUALN(papszProj[1], "Sinusoidal", 10) )
    {
        if (CSLCount(papszProjParms) < 6) goto not_enough_data;
        SetSinusoidal( CPLAtof(papszProjParms[2]),
                       CPLAtof(papszProjParms[4]), CPLAtof(papszProjParms[5]) );
    }

    else if ( EQUALN(papszProj[1], "Albers Equal Area", 17) )
    {
        if (CSLCount(papszProjParms) < 8) goto not_enough_data;
        SetACEA( CPLAtof(papszProjParms[6]), CPLAtof(papszProjParms[7]),
                 CPLAtof(papszProjParms[1]), CPLAtof(papszProjParms[2]),
                 CPLAtof(papszProjParms[4]), CPLAtof(papszProjParms[5]) );
    }

    else if ( EQUALN(papszProj[1], "(UTM) Universal Transverse Mercator", 35) && nLines > 5 )
    {
        /* Look for the UTM zone in the calibration point data */
        for ( iLine = 5; iLine < nLines; iLine++ )
        {
            if ( EQUALN(papszLines[iLine], "Point", 5) )
            {
                char    **papszTok = NULL;
                papszTok = CSLTokenizeString2( papszLines[iLine], ",",
                                               CSLT_ALLOWEMPTYTOKENS
                                               | CSLT_STRIPLEADSPACES
                                               | CSLT_STRIPENDSPACES );
                if ( CSLCount(papszTok) < 17
                     || EQUAL(papszTok[2], "")
                     || EQUAL(papszTok[13], "")
                     || EQUAL(papszTok[14], "")
                     || EQUAL(papszTok[15], "")
                     || EQUAL(papszTok[16], "") )
                {
                    CSLDestroy(papszTok);
                    continue;
                }
                SetUTM( CPLAtofM(papszTok[13]), EQUAL(papszTok[16], "N") );
                CSLDestroy(papszTok);
                break;
            }
        }
        if ( iLine == nLines )    /* Try to guess the UTM zone */
        {
            float fMinLongitude = INT_MAX;
            float fMaxLongitude = INT_MIN;
            float fMinLatitude = INT_MAX;
            float fMaxLatitude = INT_MIN;
            int bFoundMMPLL = FALSE;
            for ( iLine = 5; iLine < nLines; iLine++ )
            {
                if ( EQUALN(papszLines[iLine], "MMPLL", 5) )
                {
                    char    **papszTok = NULL;
                    papszTok = CSLTokenizeString2( papszLines[iLine], ",",
                                                   CSLT_ALLOWEMPTYTOKENS
                                                   | CSLT_STRIPLEADSPACES
                                                   | CSLT_STRIPENDSPACES );
                    if ( CSLCount(papszTok) < 4 )
                    {
                        CSLDestroy(papszTok);
                        continue;
                    }
                    float fLongitude = CPLAtofM(papszTok[2]);
                    float fLatitude = CPLAtofM(papszTok[3]);
                    CSLDestroy(papszTok);

                    bFoundMMPLL = TRUE;

                    if ( fMinLongitude > fLongitude )
                        fMinLongitude = fLongitude;
                    if ( fMaxLongitude < fLongitude )
                        fMaxLongitude = fLongitude;
                    if ( fMinLatitude > fLatitude )
                        fMinLatitude = fLatitude;
                    if ( fMaxLatitude < fLatitude )
                        fMaxLatitude = fLatitude;
                }
            }
            float fMedianLatitude = ( fMinLatitude + fMaxLatitude ) / 2;
            float fMedianLongitude = ( fMinLongitude + fMaxLongitude ) / 2;
            if ( bFoundMMPLL && fMaxLatitude <= 90 )
            {
                int nUtmZone;
                if ( fMedianLatitude >= 56 && fMedianLatitude <= 64 && 
                     fMedianLongitude >= 3 && fMedianLongitude <= 12 )
                    nUtmZone = 32;                                             /* Norway exception */
                else if ( fMedianLatitude >= 72 && fMedianLatitude <= 84 && 
                         fMedianLongitude >= 0 && fMedianLongitude <= 42 )
                    nUtmZone = (int) ((fMedianLongitude + 3 ) / 12) * 2 + 31;  /* Svalbard exception */
                else
                    nUtmZone = (int) ((fMedianLongitude + 180 ) / 6) + 1;
                SetUTM( nUtmZone, fMedianLatitude >= 0 );
            }
            else
                CPLDebug( "OSR_Ozi", "UTM Zone not found");
        }
    }

    else if ( EQUALN(papszProj[1], "(I) France Zone I", 17) )
    {
        SetLCC1SP( 49.5, 2.337229167, 0.99987734, 600000, 1200000 );
    }

    else if ( EQUALN(papszProj[1], "(II) France Zone II", 19) )
    {
        SetLCC1SP( 46.8, 2.337229167, 0.99987742, 600000, 2200000 );
    }

    else if ( EQUALN(papszProj[1], "(III) France Zone III", 21) )
    {
        SetLCC1SP( 44.1, 2.337229167, 0.99987750, 600000, 3200000 );
    }

    else if ( EQUALN(papszProj[1], "(IV) France Zone IV", 19) )
    {
        SetLCC1SP( 42.165, 2.337229167, 0.99994471, 234.358, 4185861.369 );
    }

/*
 *  Note : The following projections have not been implemented yet
 *
 */

/*
    else if ( EQUALN(papszProj[1], "(BNG) British National Grid", 27) )
    {
    }

    else if ( EQUALN(papszProj[1], "(IG) Irish Grid", 15) )
    {
    }

    else if ( EQUALN(papszProj[1], "(NZG) New Zealand Grid", 22) )
    {
    }

    else if ( EQUALN(papszProj[1], "(NZTM2) New Zealand TM 2000", 27) )
    {
    }

    else if ( EQUALN(papszProj[1], "(SG) Swedish Grid", 27) )
    {
    }

    else if ( EQUALN(papszProj[1], "(SUI) Swiss Grid", 26) )
    {
    }

    else if ( EQUALN(papszProj[1], "(A)Lambert Azimuthual Equal Area", 32) )
    {
    }

    else if ( EQUALN(papszProj[1], "(EQC) Equidistant Conic", 23) )
    {
    }

    else if ( EQUALN(papszProj[1], "Polyconic (American)", 20) )
    {
    }

    else if ( EQUALN(papszProj[1], "Van Der Grinten", 15) )
    {
    }

    else if ( EQUALN(papszProj[1], "Vertical Near-Sided Perspective", 31) )
    {
    }

    else if ( EQUALN(papszProj[1], "(WIV) Wagner IV", 15) )
    {
    }

    else if ( EQUALN(papszProj[1], "Bonne", 5) )
    {
    }

    else if ( EQUALN(papszProj[1], "(MT0) Montana State Plane Zone 2500", 35) )
    {
    }

    else if ( EQUALN(papszProj[1], "ITA1) Italy Grid Zone 1", 23) )
    {
    }

    else if ( EQUALN(papszProj[1], "ITA2) Italy Grid Zone 2", 23) )
    {
    }

    else if ( EQUALN(papszProj[1], "(VICMAP-TM) Victoria Aust.(pseudo AMG)", 38) )
    {
    }

    else if ( EQUALN(papszProj[1], "VICGRID) Victoria Australia", 27) )
    {
    }

    else if ( EQUALN(papszProj[1], "(VG94) VICGRID94 Victoria Australia", 35) )
    {
    }

    else if ( EQUALN(papszProj[1], "Gnomonic", 8) )
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
    if ( papszDatum == NULL)
        goto not_enough_data;
        
    if ( !IsLocal() )
    {

/* -------------------------------------------------------------------- */
/*      Verify that we can find the CSV file containing the datums      */
/* -------------------------------------------------------------------- */
        if( CSVScanFileByName( CSVFilename( "ozi_datum.csv" ),
                            "EPSG_DATUM_CODE",
                            "4326", CC_Integer ) == NULL )
        {
            CPLError( CE_Failure, CPLE_OpenFailed,
                    "Unable to open OZI support file %s.\n"
                    "Try setting the GDAL_DATA environment variable to point\n"
                    "to the directory containing OZI csv files.",
                    CSVFilename( "ozi_datum.csv" ) );
            goto other_error;
        }

/* -------------------------------------------------------------------- */
/*      Search for matching datum                                       */
/* -------------------------------------------------------------------- */
        const char *pszOziDatum = CSVFilename( "ozi_datum.csv" );
        CPLString osDName = CSVGetField( pszOziDatum, "NAME", papszDatum[0],
                                    CC_ApproxString, "NAME" );
        if( strlen(osDName) == 0 )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                    "Failed to find datum %s in ozi_datum.csv.",
                    papszDatum[0] );
            goto other_error;
        }

        int nDatumCode = atoi( CSVGetField( pszOziDatum, "NAME", papszDatum[0],
                                            CC_ApproxString, "EPSG_DATUM_CODE" ) );

        if ( nDatumCode > 0 ) // There is a matching EPSG code
        {
            OGRSpatialReference oGCS;
            oGCS.importFromEPSG( nDatumCode );
            CopyGeogCSFrom( &oGCS );
        }
        else // We use the parameters from the CSV files
        {
            CPLString osEllipseCode = CSVGetField( pszOziDatum, "NAME", papszDatum[0],
                                                CC_ApproxString, "ELLIPSOID_CODE" );
            double dfDeltaX = CPLAtof(CSVGetField( pszOziDatum, "NAME", papszDatum[0],
                                                CC_ApproxString, "DELTAX" ) );
            double dfDeltaY = CPLAtof(CSVGetField( pszOziDatum, "NAME", papszDatum[0],
                                                CC_ApproxString, "DELTAY" ) );
            double dfDeltaZ = CPLAtof(CSVGetField( pszOziDatum, "NAME", papszDatum[0],
                                                CC_ApproxString, "DELTAZ" ) );


    /* -------------------------------------------------------------------- */
    /*      Verify that we can find the CSV file containing the ellipsoids  */
    /* -------------------------------------------------------------------- */
            if( CSVScanFileByName( CSVFilename( "ozi_ellips.csv" ),
                                "ELLIPSOID_CODE",
                                "20", CC_Integer ) == NULL )
            {
                CPLError( CE_Failure, CPLE_OpenFailed,
                    "Unable to open OZI support file %s.\n"
                    "Try setting the GDAL_DATA environment variable to point\n"
                    "to the directory containing OZI csv files.",
                    CSVFilename( "ozi_ellips.csv" ) );
                goto other_error;
            }

    /* -------------------------------------------------------------------- */
    /*      Lookup the ellipse code.                                        */
    /* -------------------------------------------------------------------- */
            const char *pszOziEllipse = CSVFilename( "ozi_ellips.csv" );

            CPLString osEName = CSVGetField( pszOziEllipse, "ELLIPSOID_CODE", osEllipseCode,
                                        CC_ApproxString, "NAME" );
            if( strlen(osEName) == 0 )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                        "Failed to find ellipsoid %s in ozi_ellips.csv.",
                        osEllipseCode.c_str() );
                goto other_error;
            }

            double dfA = CPLAtof(CSVGetField( pszOziEllipse, "ELLIPSOID_CODE", osEllipseCode,
                                        CC_ApproxString, "A" ));
            double dfInvF = CPLAtof(CSVGetField( pszOziEllipse, "ELLIPSOID_CODE", osEllipseCode,
                                            CC_ApproxString, "INVF" ));

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

