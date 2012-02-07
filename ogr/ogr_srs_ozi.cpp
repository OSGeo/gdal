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
    return ((OGRSpatialReference *) hSRS)->importFromOzi( pszDatum, pszProj,
                                                          pszProjParms );
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
 */

OGRErr OGRSpatialReference::importFromOzi( const char *pszDatum,
                                           const char *pszProj,
                                           const char *pszProjParms )

{
    Clear();

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
