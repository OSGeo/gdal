/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Generate an OGRSpatialReference object based on an EPSG
 *           PROJCS, or GEOGCS code.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
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
 * Revision 1.2  2000/03/20 14:59:05  warmerda
 * fixed semi major axis check
 *
 * Revision 1.1  2000/03/16 19:04:14  warmerda
 * New
 *
 */

#include "ogr_spatialref.h"
#include "cpl_csv.h"

#ifndef PI
#  define PI 3.14159265358979323846
#endif

static char *papszDatumEquiv[] =
{
    "Militar_Geographische_Institut",
    "Militar_Geographische_Institute",
    "World_Geodetic_System_1984",
    "WGS_1984",
    "WGS_72_Transit_Broadcast_Ephemeris",
    "WGS_1972_Transit_Broadcast_Ephemeris",
    "World_Geodetic_System_1972",
    "WGS_1972",
    "European_Terrestrial_Reference_System_89",
    "European_Reference_System_1989",
    NULL
};

/************************************************************************/
/*                          WKTMassageDatum()                           */
/*                                                                      */
/*      Massage an EPSG datum name into WMT format.  Also transform     */
/*      specific exception cases into WKT versions.                     */
/************************************************************************/

static void WKTMassageDatum( char ** ppszDatum )

{
    int		i, j;
    char	*pszDatum = *ppszDatum;

/* -------------------------------------------------------------------- */
/*      Translate non-alphanumeric values to underscores.               */
/* -------------------------------------------------------------------- */
    for( i = 0; pszDatum[i] != '\0'; i++ )
    {
        if( !(pszDatum[i] >= 'A' && pszDatum[i] <= 'Z')
            && !(pszDatum[i] >= 'a' && pszDatum[i] <= 'z')
            && !(pszDatum[i] >= '0' && pszDatum[i] <= '9') )
        {
            pszDatum[i] = '_';
        }
    }

/* -------------------------------------------------------------------- */
/*      Remove repeated and trailing underscores.                       */
/* -------------------------------------------------------------------- */
    for( i = 1, j = 0; pszDatum[i] != '\0'; i++ )
    {
        if( pszDatum[j] == '_' && pszDatum[i] == '_' )
            continue;

        pszDatum[++j] = pszDatum[i];
    }
    if( pszDatum[j] == '_' )
        pszDatum[j] = '\0';
    else
        pszDatum[j+1] = '\0';
    
/* -------------------------------------------------------------------- */
/*      Search for datum equivelences.  Specific massaged names get     */
/*      mapped to OpenGIS specified names.                              */
/* -------------------------------------------------------------------- */
    for( i = 0; papszDatumEquiv[i] != NULL; i += 2 )
    {
        if( EQUAL(*ppszDatum,papszDatumEquiv[i]) )
        {
            CPLFree( *ppszDatum );
            *ppszDatum = CPLStrdup( papszDatumEquiv[i+1] );
            break;
        }
    }
}

/************************************************************************/
/*                        EPSGAngleStringToDD()                         */
/*                                                                      */
/*      Convert an angle in the specified units to decimal degrees.     */
/************************************************************************/

static double
EPSGAngleStringToDD( const char * pszAngle, int nUOMAngle )

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
                    strcpy( szSeconds+3, pszDecimal + 5 );
                }
                else
                {
                    szSeconds[1] = '0';
                    szSeconds[2] = '\0';
                }
                dfAngle += atof(szSeconds) / 3600.0;
            }
        }

        if( pszAngle[0] == '-' )
            dfAngle *= -1;
    }
    else if( nUOMAngle == 9105 || nUOMAngle == 9106 )	/* grad */
    {
        dfAngle = 180 * (atof(pszAngle ) / 200);
    }
    else if( nUOMAngle == 9101 )			/* radians */
    {
        dfAngle = 180 * (atof(pszAngle ) / PI);
    }
    else if( nUOMAngle == 9103 )			/* arc-minute */
    {
        dfAngle = atof(pszAngle) / 60;
    }
    else if( nUOMAngle == 9104 )			/* arc-second */
    {
        dfAngle = atof(pszAngle) / 3600;
    }
    else /* decimal degrees ... some cases missing but seeminly never used */
    {
        CPLAssert( nUOMAngle == 9102 || nUOMAngle == 0 );
        
        dfAngle = atof(pszAngle );
    }

    return( dfAngle );
}

/************************************************************************/
/*                        EPSGGetUOMLengthInfo()                        */
/*                                                                      */
/*      Note: This function should eventually also know how to          */
/*      lookup length aliases in the UOM_LE_ALIAS table.                */
/************************************************************************/

static int 
EPSGGetUOMLengthInfo( int nUOMLengthCode,
                      char **ppszUOMName,
                      double * pdfInMeters )

{
    char	**papszUnitsRecord;
    char	szSearchKey[24];
    int		iNameField;

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
        CSVScanFileByName( CSVFilename( "uom_length.csv" ),
                           "UOM_LENGTH_CODE", szSearchKey, CC_Integer );

    if( papszUnitsRecord == NULL )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Get the name, if requested.                                     */
/* -------------------------------------------------------------------- */
    if( ppszUOMName != NULL )
    {
        iNameField = CSVGetFileFieldId( CSVFilename( "uom_length.csv" ),
                                        "UNIT_OF_MEAS_EPSG_NAME" );
        *ppszUOMName = CPLStrdup( CSLGetField(papszUnitsRecord, iNameField) );
    }
    
/* -------------------------------------------------------------------- */
/*      Get the A and B factor fields, and create the multiplicative    */
/*      factor.                                                         */
/* -------------------------------------------------------------------- */
    if( pdfInMeters != NULL )
    {
        int	iBFactorField, iCFactorField;
        
        iBFactorField = CSVGetFileFieldId( CSVFilename( "uom_length.csv" ),
                                           "FACTOR_B" );
        iCFactorField = CSVGetFileFieldId( CSVFilename( "uom_length.csv" ),
                                           "FACTOR_C" );

        if( atof(CSLGetField(papszUnitsRecord, iCFactorField)) > 0.0 )
            *pdfInMeters = atof(CSLGetField(papszUnitsRecord, iBFactorField))
                / atof(CSLGetField(papszUnitsRecord, iCFactorField));
        else
            *pdfInMeters = 0.0;
    }
    
    return( TRUE );
}

/************************************************************************/
/*                       EPSGGetWGS84Transform()                        */
/*                                                                      */
/*      The following code attempts to find a bursa-wolf                */
/*      transformation from this GeogCS to WGS84 (4326).                */
/*                                                                      */
/*      Faults:                                                         */
/*       o I think there are codes other than 9603 and 9607 that        */
/*         return compatible, or easily transformed parameters.         */
/*       o Only the first path from the given GeogCS is checked due     */
/*         to limitations in the CSV API.                               */
/************************************************************************/

int EPSGGetWGS84Transform( int nGeogCS, double *padfTransform )

{
    int		nTRFCode;
    char	szCode[32];

/* -------------------------------------------------------------------- */
/*      Search the trf_path table for a transformation mapping this     */
/*      GeogCS into WGS84.                                              */
/* -------------------------------------------------------------------- */
    char **papszLine;

    sprintf( szCode, "%d", nGeogCS );
    papszLine = CSVScanFileByName( CSVFilename("trf_path.csv"), 
                                   "SOURCE_HORIZCS_CODE", 
                                   szCode, CC_Integer );
    if( papszLine == NULL )
        return FALSE;

    if( CSLCount(papszLine) < 6 || atoi(papszLine[3]) != 4326 )
        return FALSE;
    
    nTRFCode = atoi(papszLine[5]);
    if( nTRFCode == 0 )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Find the translation, and verify it is a bursa-wolf style       */
/*      transformation.                                                 */
/* -------------------------------------------------------------------- */
    sprintf( szCode, "%d", nTRFCode );
    papszLine = CSVScanFileByName( CSVFilename("trf_nonpolynomial.csv"), 
                                   "COORD_TRF_CODE", 
                                   szCode, CC_Integer );

    if( CSLCount(papszLine) < 12 )
        return FALSE;

    if( atoi(papszLine[6]) != 9603 && atoi(papszLine[6]) != 9607 )
        return FALSE;

    for( int iField = 0; iField < 7; iField++ )
        padfTransform[iField] = atof(papszLine[iField+7]);
        
    return TRUE;
}

/************************************************************************/
/*                           EPSGGetPMInfo()                            */
/*                                                                      */
/*      Get the offset between a given prime meridian and Greenwich     */
/*      in degrees.                                                     */
/************************************************************************/

static int 
EPSGGetPMInfo( int nPMCode, char ** ppszName, double *pdfOffset )

{
    char	szSearchKey[24];
    int		nUOMAngle;

/* -------------------------------------------------------------------- */
/*      Use a special short cut for Greenwich, since it is so common.   */
/* -------------------------------------------------------------------- */
    if( nPMCode == 7022 /* PM_Greenwich */ )
    {
        if( pdfOffset != NULL )
            *pdfOffset = 0.0;
        if( ppszName != NULL )
            *ppszName = CPLStrdup( "Greenwich" );
        return TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Search the database for the corresponding datum code.           */
/* -------------------------------------------------------------------- */
    sprintf( szSearchKey, "%d", nPMCode );

    nUOMAngle =
        atoi(CSVGetField( CSVFilename("p_meridian.csv" ),
                          "PRIME_MERIDIAN_CODE", szSearchKey, CC_Integer,
                          "UOM_ANGLE_CODE" ) );
    if( nUOMAngle < 1 )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Get the PM offset.                                              */
/* -------------------------------------------------------------------- */
    if( pdfOffset != NULL )
    {
        *pdfOffset =
            EPSGAngleStringToDD(
                CSVGetField( CSVFilename("p_meridian.csv" ),
                             "PRIME_MERIDIAN_CODE", szSearchKey, CC_Integer,
                             "GREENWICH_LONGITUDE" ),
                nUOMAngle );
    }
    
/* -------------------------------------------------------------------- */
/*      Get the name, if requested.                                     */
/* -------------------------------------------------------------------- */
    if( ppszName != NULL )
        *ppszName =
            CPLStrdup(
                CSVGetField( CSVFilename("p_meridian.csv" ),
                             "PRIME_MERIDIAN_CODE", szSearchKey, CC_Integer,
                             "PRIME_MERID_EPSG_NAME" ));
    
    return( TRUE );
}

/************************************************************************/
/*                           EPSGGetGCSInfo()                           */
/*                                                                      */
/*      Fetch the datum, and prime meridian related to a particular     */
/*      GCS.                                                            */
/************************************************************************/

static int
EPSGGetGCSInfo( int nGCSCode, char ** ppszName,
                int * pnDatum, int * pnPM, int *pnUOMAngle )

{
    char	szSearchKey[24];
    int		nDatum, nPM, nUOMAngle;

/* -------------------------------------------------------------------- */
/*      Search the database for the corresponding datum code.           */
/* -------------------------------------------------------------------- */
    sprintf( szSearchKey, "%d", nGCSCode );

    nDatum = atoi(CSVGetField( CSVFilename("horiz_cs.csv" ),
                               "HORIZCS_CODE", szSearchKey, CC_Integer,
                               "GEOD_DATUM_CODE" ) );

    if( nDatum < 1 )
        return FALSE;

    if( pnDatum != NULL )
        *pnDatum = nDatum;
    
/* -------------------------------------------------------------------- */
/*      Get the PM.                                                     */
/* -------------------------------------------------------------------- */
    nPM = atoi(CSVGetField( CSVFilename("horiz_cs.csv" ),
                            "HORIZCS_CODE", szSearchKey, CC_Integer,
                            "PRIME_MERIDIAN_CODE" ) );

    if( nPM < 1 )
        return FALSE;

    if( pnPM != NULL )
        *pnPM = nPM;

/* -------------------------------------------------------------------- */
/*      Get the angular units.                                          */
/* -------------------------------------------------------------------- */
    nUOMAngle = atoi(CSVGetField( CSVFilename("horiz_cs.csv" ),
                                  "HORIZCS_CODE", szSearchKey, CC_Integer,
                                  "UOM_ANGLE_CODE" ) );

    if( nUOMAngle < 1 )
        return FALSE;

    if( pnUOMAngle != NULL )
        *pnUOMAngle = nUOMAngle;

/* -------------------------------------------------------------------- */
/*      Get the name, if requested.                                     */
/* -------------------------------------------------------------------- */
    if( ppszName != NULL )
        *ppszName =
            CPLStrdup(CSVGetField( CSVFilename("horiz_cs.csv" ),
                                   "HORIZCS_CODE", szSearchKey, CC_Integer,
                                   "HORIZCS_EPSG_NAME" ));
    
    return( TRUE );
}

/************************************************************************/
/*                          EPSGGetDatumInfo()                          */
/*                                                                      */
/*      Fetch the ellipsoid, and name for a datum.                      */
/************************************************************************/

static int
EPSGGetDatumInfo( int nDatumCode, char ** ppszName, int * pnEllipsoid )

{
    char	szSearchKey[24];
    int		nEllipsoid;

/* -------------------------------------------------------------------- */
/*      Search the database for the corresponding datum code.           */
/* -------------------------------------------------------------------- */
    sprintf( szSearchKey, "%d", nDatumCode );

    nEllipsoid = atoi(CSVGetField( CSVFilename("geod_datum.csv" ),
                                   "GEOD_DATUM_CODE", szSearchKey, CC_Integer,
                                   "ELLIPSOID_CODE" ) );

    if( nEllipsoid < 1 )
        return FALSE;

    if( pnEllipsoid != NULL )
        *pnEllipsoid = nEllipsoid;
    
/* -------------------------------------------------------------------- */
/*      Get the name, if requested.                                     */
/* -------------------------------------------------------------------- */
    if( ppszName != NULL )
        *ppszName =
            CPLStrdup(CSVGetField( CSVFilename("geod_datum.csv" ),
                                   "GEOD_DATUM_CODE", szSearchKey, CC_Integer,
                                   "GEOD_DAT_EPSG_NAME" ));
    
    return( TRUE );
}

/************************************************************************/
/*                        EPSGGetEllipsoidInfo()                        */
/*                                                                      */
/*      Fetch info about an ellipsoid.  Axes are always returned in     */
/*      meters.  SemiMajor computed based on inverse flattening         */
/*      where that is provided.                                         */
/************************************************************************/

static int 
EPSGGetEllipsoidInfo( int nCode, char ** ppszName,
                      double * pdfSemiMajor, double * pdfInvFlattening )

{
    char	szSearchKey[24];
    double	dfSemiMajor, dfToMeters = 1.0;
    int		nUOMLength;
    
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
/*	Get the translation factor into meters.				*/
/* -------------------------------------------------------------------- */
    nUOMLength = atoi(CSVGetField( CSVFilename("ellipsoid.csv" ),
                                   "ELLIPSOID_CODE", szSearchKey, CC_Integer,
                                   "UOM_LENGTH_CODE" ));
    EPSGGetUOMLengthInfo( nUOMLength, NULL, &dfToMeters );

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
                                   "ELLIPSOID_EPSG_NAME" ));
    
    return( TRUE );
}

/************************************************************************/
/*                         EPSGGetProjTRFInfo()                         */
/*                                                                      */
/*      Transform a PROJECTION_TRF_CODE into a projection method,       */
/*      and a set of parameters.  The parameters identify will          */
/*      depend on the returned method, but they will all have been      */
/*      normalized into degrees and meters.                             */
/************************************************************************/

static int
EPSGGetProjTRFInfo( int nProjTRFCode,
                    char **ppszProjTRFName,
                    int * pnProjMethod,
                    double * padfProjParms )

{
    int		nProjMethod, nUOMLinear, nUOMAngle, i;
    double	adfProjParms[7], dfInMeters;
    char	szTRFCode[16];

/* -------------------------------------------------------------------- */
/*      Get the proj method.  If this fails to return a meaningful      */
/*      number, then the whole function fails.                          */
/* -------------------------------------------------------------------- */
    sprintf( szTRFCode, "%d", nProjTRFCode );
    nProjMethod =
        atoi( CSVGetField( CSVFilename( "trf_nonpolynomial.csv" ),
                           "COORD_TRF_CODE", szTRFCode, CC_Integer,
                           "COORD_TRF_METHOD_CODE" ) );
    if( nProjMethod == 0 )
        return FALSE;

/* -------------------------------------------------------------------- */
/*	Get the linear, and angular (geog) units for the projection	*/
/*	parameters.							*/    
/* -------------------------------------------------------------------- */
    nUOMLinear = 
        atoi( CSVGetField( CSVFilename( "trf_nonpolynomial.csv" ),
                           "COORD_TRF_CODE", szTRFCode, CC_Integer,
                           "UOM_LENGTH_CODE" ) );

    if( !EPSGGetUOMLengthInfo( nUOMLinear, NULL, &dfInMeters ) )
        dfInMeters = 1.0;
    
    nUOMAngle = 
        atoi( CSVGetField( CSVFilename( "trf_nonpolynomial.csv" ),
                           "COORD_TRF_CODE", szTRFCode, CC_Integer,
                           "UOM_ANGLE_CODE" ) );
    
/* -------------------------------------------------------------------- */
/*      Get the parameters for this projection.  For the time being     */
/*      I am assuming the first four parameters are angles, the         */
/*      fifth is unitless (normally scale), and the remainder are       */
/*      linear measures.  This works fine for the existing              */
/*      projections, but is a pretty fragile approach.                  */
/* -------------------------------------------------------------------- */

    for( i = 0; i < 7; i++ )
    {
        char	szID[16];
        const char *pszValue;
        
        sprintf( szID, "PARAMETER_%d", i+1 );

        pszValue = CSVGetField( CSVFilename( "trf_nonpolynomial.csv" ),
                                "COORD_TRF_CODE", szTRFCode,CC_Integer, szID );
        if( i < 4 )
            adfProjParms[i] = EPSGAngleStringToDD( pszValue, nUOMAngle );
        else if( i > 4 )
            adfProjParms[i] = atof(pszValue) * dfInMeters;
        else
            adfProjParms[i] = atof( pszValue );
    }

/* -------------------------------------------------------------------- */
/*      Get the name, if requested.                                     */
/* -------------------------------------------------------------------- */
    if( ppszProjTRFName != NULL )
    {
        *ppszProjTRFName =
            CPLStrdup(CSVGetField( CSVFilename("trf_nonpolynomial.csv" ),
                                   "COORD_TRF_CODE", szTRFCode, CC_Integer,
                                   "COORD_TRF_EPSG_NAME" ));
    }
    
/* -------------------------------------------------------------------- */
/*      Transfer requested data into passed variables.                  */
/* -------------------------------------------------------------------- */
    if( pnProjMethod != NULL )
        *pnProjMethod = nProjMethod;

    if( padfProjParms != NULL )
    {
        for( i = 0; i < 7; i++ )
            padfProjParms[i] = adfProjParms[i];
    }

    return TRUE;
}


/************************************************************************/
/*                           EPSGGetPCSInfo()                           */
/************************************************************************/

static int 
EPSGGetPCSInfo( int nPCSCode, char **ppszEPSGName,
                int *pnUOMLengthCode, int *pnUOMAngleCode,
                int *pnGeogCS, int *pnTRFCode )

{
    char	**papszRecord;
    char	szSearchKey[24];
    const char	*pszFilename = CSVFilename( "horiz_cs.csv" );
    
/* -------------------------------------------------------------------- */
/*      Search the units database for this unit.  If we don't find      */
/*      it return failure.                                              */
/* -------------------------------------------------------------------- */
    sprintf( szSearchKey, "%d", nPCSCode );
    papszRecord = CSVScanFileByName( pszFilename, "HORIZCS_CODE",
                                     szSearchKey, CC_Integer );

    if( papszRecord == NULL )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Get the name, if requested.                                     */
/* -------------------------------------------------------------------- */
    if( ppszEPSGName != NULL )
    {
        *ppszEPSGName =
            CPLStrdup( CSLGetField( papszRecord,
                                    CSVGetFileFieldId(pszFilename,
                                                      "HORIZCS_EPSG_NAME") ));
    }

/* -------------------------------------------------------------------- */
/*      Get the UOM Length code, if requested.                          */
/* -------------------------------------------------------------------- */
    if( pnUOMLengthCode != NULL )
    {
        const char	*pszValue;

        pszValue =
            CSLGetField( papszRecord,
                         CSVGetFileFieldId(pszFilename,"UOM_LENGTH_CODE"));
        if( atoi(pszValue) > 0 )
            *pnUOMLengthCode = atoi(pszValue);
        else
            *pnUOMLengthCode = 0;
    }

/* -------------------------------------------------------------------- */
/*      Get the UOM Angle code, if requested.                           */
/* -------------------------------------------------------------------- */
    if( pnUOMAngleCode != NULL )
    {
        const char	*pszValue;
        
        pszValue =
            CSLGetField( papszRecord,
                         CSVGetFileFieldId(pszFilename,"UOM_ANGLE_CODE") );
        
        if( atoi(pszValue) > 0 )
            *pnUOMAngleCode = atoi(pszValue);
        else
            *pnUOMAngleCode = 0;
    }

/* -------------------------------------------------------------------- */
/*      Get the GeogCS (Datum with PM) code, if requested.		*/
/* -------------------------------------------------------------------- */
    if( pnGeogCS != NULL )
    {
        const char	*pszValue;

        pszValue =
            CSLGetField( papszRecord,
                         CSVGetFileFieldId(pszFilename,"SOURCE_GEOGCS_CODE") );
        if( atoi(pszValue) > 0 )
            *pnGeogCS = atoi(pszValue);
        else
            *pnGeogCS = 0;
    }

/* -------------------------------------------------------------------- */
/*      Get the GeogCS (Datum with PM) code, if requested.		*/
/* -------------------------------------------------------------------- */
    if( pnTRFCode != NULL )
    {
        const char	*pszValue;

        pszValue =
            CSLGetField( papszRecord,
                         CSVGetFileFieldId(pszFilename,"PROJECTION_TRF_CODE"));
                         
        
        if( atoi(pszValue) > 0 )
            *pnTRFCode = atoi(pszValue);
        else
            *pnTRFCode = 0;
    }

    return TRUE;
}

/************************************************************************/
/*                           SetEPSGGeogCS()                            */
/*                                                                      */
/*      FLAWS:                                                          */
/*       o Units are all hardcoded.                                     */
/*       o Axis hardcoded.                                              */
/************************************************************************/

static OGRErr SetEPSGGeogCS( OGRSpatialReference * poSRS, int nGeogCS )

{
    int  nDatumCode, nPMCode, nUOMAngle, nEllipsoidCode;
    char *pszGeogCSName = NULL, *pszDatumName = NULL, *pszEllipsoidName = NULL;
    char *pszPMName = NULL;
    double dfPMOffset, dfSemiMajor, dfInvFlattening, adfBursaTransform[7];

    if( !EPSGGetGCSInfo( nGeogCS, &pszGeogCSName,
                         &nDatumCode,&nPMCode, &nUOMAngle ) )
        return OGRERR_UNSUPPORTED_SRS;

    if( !EPSGGetPMInfo( nPMCode, &pszPMName, &dfPMOffset ) )
        return OGRERR_UNSUPPORTED_SRS;

    if( !EPSGGetDatumInfo( nDatumCode, &pszDatumName, &nEllipsoidCode ) )
        return OGRERR_UNSUPPORTED_SRS;

    WKTMassageDatum( &pszDatumName );

    if( !EPSGGetEllipsoidInfo( nEllipsoidCode, &pszEllipsoidName, 
                               &dfSemiMajor, &dfInvFlattening ) )
        return OGRERR_UNSUPPORTED_SRS;

    poSRS->SetGeogCS( pszGeogCSName, pszDatumName, 
                      pszEllipsoidName, dfSemiMajor, dfInvFlattening,
                      pszPMName, dfPMOffset );

    if( EPSGGetWGS84Transform( nGeogCS, adfBursaTransform ) )
    {
        OGR_SRSNode	*poWGS84;
        char            szValue[48];

        poWGS84 = new OGR_SRSNode( "TOWGS84" );

        for( int iCoeff = 0; iCoeff < 7; iCoeff++ )
        {
            sprintf( szValue, "%g", adfBursaTransform[iCoeff] );
            poWGS84->AddChild( new OGR_SRSNode( szValue ) );
        }

        poSRS->GetAttrNode( "DATUM" )->AddChild( poWGS84 );
    }

    poSRS->SetAuthority( "GEOGCS", "EPSG", nGeogCS );
    poSRS->SetAuthority( "DATUM", "EPSG", nDatumCode );
    poSRS->SetAuthority( "SPHEROID", "EPSG", nEllipsoidCode );
    poSRS->SetAuthority( "PRIMEM", "EPSG", nPMCode );

    CPLFree( pszDatumName );
    CPLFree( pszEllipsoidName );
    CPLFree( pszGeogCSName );
    CPLFree( pszPMName );

    return OGRERR_NONE;
}

/************************************************************************/
/*                           SetEPSGProjCS()                            */
/************************************************************************/

static OGRErr SetEPSGProjCS( OGRSpatialReference * poSRS, int nPCSCode )

{
    int		nGCSCode, nUOMAngleCode, nUOMLength, nTRFCode, nProjMethod;
    char        *pszPCSName = NULL, *pszUOMLengthName = NULL;
    double      adfProjParms[7], dfInMeters;
    OGRErr      nErr;
    OGR_SRSNode *poNode;

    if( !EPSGGetPCSInfo( nPCSCode, &pszPCSName, &nUOMLength, &nUOMAngleCode,
                         &nGCSCode, &nTRFCode ) )
        return OGRERR_UNSUPPORTED_SRS;

    poSRS->SetNode( "PROJCS", pszPCSName );
    
    nErr = SetEPSGGeogCS( poSRS, nGCSCode );
    if( nErr != OGRERR_NONE )
        return nErr;

    if( !EPSGGetProjTRFInfo( nTRFCode, NULL, &nProjMethod, adfProjParms ) )
        return OGRERR_UNSUPPORTED_SRS;

    switch( nProjMethod )
    {
      case 9801:
        poSRS->SetLCC1SP( adfProjParms[0], adfProjParms[1], adfProjParms[4], 
                          adfProjParms[5], adfProjParms[6] );
        break;

      case 9802:
        poSRS->SetLCC( adfProjParms[0], adfProjParms[1], 
                       adfProjParms[2], adfProjParms[3],
                       adfProjParms[5], adfProjParms[6] );
        break;

      case 9803:
        poSRS->SetLCCB( adfProjParms[0], adfProjParms[1], 
                        adfProjParms[2], adfProjParms[3],
                        adfProjParms[5], adfProjParms[6] );
        break;

      case 9804:
      case 9805: /* NOTE: treats 1SP and 2SP cases the same */
        poSRS->SetMercator( adfProjParms[0], adfProjParms[1], 
                            adfProjParms[4], 
                            adfProjParms[5], adfProjParms[6] );
        break;

      case 9806:
        poSRS->SetCS( adfProjParms[0], adfProjParms[1], 
                      adfProjParms[5], adfProjParms[6] );
        break;

      case 9807:
        poSRS->SetTM( adfProjParms[0], adfProjParms[1], adfProjParms[4], 
                      adfProjParms[5], adfProjParms[6] );
        break;
        
      case 9808:
        poSRS->SetTMSO( adfProjParms[0], adfProjParms[1], adfProjParms[4], 
                        adfProjParms[5], adfProjParms[6] );
        break;
        
      case 9809:
        poSRS->SetOS( adfProjParms[0], adfProjParms[1], 
                      adfProjParms[4],
                      adfProjParms[5], adfProjParms[6] );
        break;

      case 9810:
        poSRS->SetPS( adfProjParms[0], adfProjParms[1], 
                      adfProjParms[4],
                      adfProjParms[5], adfProjParms[6] );
        break;

      case 9811:
        poSRS->SetNZMG( adfProjParms[0], adfProjParms[1], 
                        adfProjParms[5], adfProjParms[6] );
        break;

      case 9812:
      case 9813:
      case 9814:
      case 9815:
        poSRS->SetHOM( adfProjParms[0], adfProjParms[1], 
                       adfProjParms[2], 90.0, 
                       adfProjParms[4],
                       adfProjParms[5], adfProjParms[6] );
        poNode = poSRS->GetAttrNode( "PROJECTION" )->GetChild( 0 );
        if( nProjMethod == 9813 )
            poNode->SetValue( SRS_PT_LABORDE_OBLIQUE_MERCATOR );
        break;

      case 9816:
        poSRS->SetTMG( adfProjParms[0], adfProjParms[1],
                       adfProjParms[5], adfProjParms[6] );
        break;

      default:
        return OGRERR_UNSUPPORTED_SRS;
    }

    if( !EPSGGetUOMLengthInfo( nUOMLength, &pszUOMLengthName, &dfInMeters ) )
        return OGRERR_UNSUPPORTED_SRS;

    poSRS->SetLinearUnits( pszUOMLengthName, dfInMeters );
    poSRS->SetAuthority( "PROJCS|UNIT", "EPSG", nUOMLength );

    poSRS->SetAuthority( "PROJCS", "EPSG", nPCSCode );

    CPLFree( pszUOMLengthName );
    CPLFree( pszPCSName );

    return OGRERR_NONE;
}

/************************************************************************/
/*                           importFromEPSG()                           */
/************************************************************************/

/**
 * Initialize SRS based on EPSG GCS or PCS code.
 *
 * This code uses the GeoTIFF cpl_csv services to access the EPSG CSV 
 * data.  If frmts/gtiff/libgeotiff isn't linked in, linking will fail. 
 * If EPSG tables can't be found at runtime, the method will fail.
 *
 * @param nCode a GCS or PCS code from the horizontal coordinate system table.
 * 
 * @return OGRERR_NONE on success, or an error code on failure.
 */

OGRErr OGRSpatialReference::importFromEPSG( int nCode )

{
/* -------------------------------------------------------------------- */
/*      Clear any existing definition.                                  */
/* -------------------------------------------------------------------- */
    if( GetRoot() != NULL )
    {
        delete poRoot;
        poRoot = NULL;
    }

/* -------------------------------------------------------------------- */
/*      Is this a GeogCS code?   this is inadequate as a criteria       */
/* -------------------------------------------------------------------- */
    if( EPSGGetGCSInfo( nCode, NULL, NULL, NULL, NULL ) )
        return SetEPSGGeogCS( this, nCode );
    else
        return SetEPSGProjCS( this, nCode );
        
    return OGRERR_NONE;
}
