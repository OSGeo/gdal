/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Generate an OGRSpatialReference object based on an EPSG
 *           PROJCS, or GEOGCS code.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
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
 ****************************************************************************/

#include "ogr_spatialref.h"
#include "cpl_csv.h"

CPL_CVSID("$Id$");

#ifndef PI
#  define PI 3.14159265358979323846
#endif

static const char *papszDatumEquiv[] =
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
/*                      OGREPSGDatumNameMassage()                       */
/*                                                                      */
/*      Massage an EPSG datum name into WMT format.  Also transform     */
/*      specific exception cases into WKT versions.                     */
/************************************************************************/

void OGREPSGDatumNameMassage( char ** ppszDatum )

{
    int         i, j;
    char        *pszDatum = *ppszDatum;

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
    double      dfAngle;
    
    if( nUOMAngle == 9110 )             /* DDD.MMSSsss */
    {
        char    *pszDecimal;
        
        dfAngle = ABS(atoi(pszAngle));
        pszDecimal = (char *) strchr(pszAngle,'.');
        if( pszDecimal != NULL && strlen(pszDecimal) > 1 )
        {
            char        szMinutes[3];
            char        szSeconds[64];

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
    else if( nUOMAngle == 9105 || nUOMAngle == 9106 )   /* grad */
    {
        dfAngle = 180 * (atof(pszAngle ) / 200);
    }
    else if( nUOMAngle == 9101 )                        /* radians */
    {
        dfAngle = 180 * (atof(pszAngle ) / PI);
    }
    else if( nUOMAngle == 9103 )                        /* arc-minute */
    {
        dfAngle = atof(pszAngle) / 60;
    }
    else if( nUOMAngle == 9104 )                        /* arc-second */
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
/*                        EPSGGetUOMAngleInfo()                         */
/************************************************************************/

int EPSGGetUOMAngleInfo( int nUOMAngleCode,
                         char **ppszUOMName,
                         double * pdfInDegrees )

{
    const char  *pszUOMName = NULL;
    double      dfInDegrees = 1.0;
    const char *pszFilename = CSVFilename( "unit_of_measure.csv" );
    char        szSearchKey[24];

    sprintf( szSearchKey, "%d", nUOMAngleCode );
    pszUOMName = CSVGetField( pszFilename,
                              "UOM_CODE", szSearchKey, CC_Integer,
                              "UNIT_OF_MEAS_NAME" );

/* -------------------------------------------------------------------- */
/*      If the file is found, read from there.  Note that FactorC is    */
/*      an empty field for any of the DMS style formats, and in this    */
/*      case we really want to return the default InDegrees value       */
/*      (1.0) from above.                                               */
/* -------------------------------------------------------------------- */
    if( pszUOMName != NULL )
    {
        double dfFactorB, dfFactorC;
        
        dfFactorB = 
            atof(CSVGetField( pszFilename,
                              "UOM_CODE", szSearchKey, CC_Integer,
                              "FACTOR_B" ));
        
        dfFactorC = 
            atof(CSVGetField( pszFilename,
                              "UOM_CODE", szSearchKey, CC_Integer,
                              "FACTOR_C" ));

        if( dfFactorC != 0.0 )
            dfInDegrees = (dfFactorB / dfFactorC) * (180.0 / PI);

        /* We do a special override of some of the DMS formats name */
        if( nUOMAngleCode == 9102 || nUOMAngleCode == 9107
            || nUOMAngleCode == 9108 || nUOMAngleCode == 9110 
            || nUOMAngleCode == 9122 )
            pszUOMName = "degree";

        // For some reason, (FactorB) is not very precise in EPSG, use
        // a more exact form for grads.
        if( nUOMAngleCode == 9105 )
            dfInDegrees = 180.0 / 200.0;
    }

/* -------------------------------------------------------------------- */
/*      Otherwise handle a few well known units directly.               */
/* -------------------------------------------------------------------- */
    else
    {
        switch( nUOMAngleCode )
        {
          case 9101:
            pszUOMName = "radian";
            dfInDegrees = 180.0 / PI;
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
            dfInDegrees = 180.0 / (3.14159265358979 * 1000000.0);
            break;

          default:
            return FALSE;
        }
    }

/* -------------------------------------------------------------------- */
/*      Return to caller.                                               */
/* -------------------------------------------------------------------- */
    if( ppszUOMName != NULL )
    {
        if( pszUOMName != NULL )
            *ppszUOMName = CPLStrdup( pszUOMName );
        else
            *ppszUOMName = NULL;
    }

    if( pdfInDegrees != NULL )
        *pdfInDegrees = dfInDegrees;

    return( TRUE );
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
    int         nMethodCode, iDXField, iField;
    char        szCode[32];
    const char *pszFilename = CSVFilename("gcs.csv");

/* -------------------------------------------------------------------- */
/*      Fetch the line from the GCS table.                              */
/* -------------------------------------------------------------------- */
    char **papszLine;

    sprintf( szCode, "%d", nGeogCS );
    papszLine = CSVScanFileByName( pszFilename,
                                   "COORD_REF_SYS_CODE", 
                                   szCode, CC_Integer );
    if( papszLine == NULL )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Verify that the method code is one of our accepted ones.        */
/* -------------------------------------------------------------------- */
    nMethodCode = 
        atoi(CSLGetField( papszLine,
                          CSVGetFileFieldId(pszFilename,
                                            "COORD_OP_METHOD_CODE")));
    if( nMethodCode != 9603 && nMethodCode != 9607 && nMethodCode != 9606 )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Fetch the transformation parameters.                            */
/* -------------------------------------------------------------------- */
    iDXField = CSVGetFileFieldId(pszFilename, "DX");

    for( iField = 0; iField < 7; iField++ )
        padfTransform[iField] = atof(papszLine[iDXField+iField]);

/* -------------------------------------------------------------------- */
/*      9607 - coordinate frame rotation has reverse signs on the       */
/*      rotational coefficients.  Fix up now since we internal          */
/*      operate according to method 9606 (position vector 7-parameter). */
/* -------------------------------------------------------------------- */
    if( nMethodCode == 9607 )
    {
        padfTransform[3] *= -1;
        padfTransform[4] *= -1;
        padfTransform[5] *= -1;
    }
        
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
    char        szSearchKey[24];
    int         nUOMAngle;

#define PM_FILENAME CSVFilename("prime_meridian.csv")

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
        atoi(CSVGetField( PM_FILENAME,
                          "PRIME_MERIDIAN_CODE", szSearchKey, CC_Integer,
                          "UOM_CODE" ) );
    if( nUOMAngle < 1 )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Get the PM offset.                                              */
/* -------------------------------------------------------------------- */
    if( pdfOffset != NULL )
    {
        *pdfOffset =
            EPSGAngleStringToDD(
                CSVGetField( PM_FILENAME,
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
                CSVGetField( PM_FILENAME, 
                             "PRIME_MERIDIAN_CODE", szSearchKey, CC_Integer,
                             "PRIME_MERIDIAN_NAME" ));
    
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
                int * pnDatum, char **ppszDatumName,
                int * pnPM, int *pnEllipsoid, int *pnUOMAngle )

{
    char        szSearchKey[24];
    int         nDatum, nPM, nUOMAngle, nEllipsoid;
    const char  *pszFilename = CSVFilename("gcs.csv");


/* -------------------------------------------------------------------- */
/*      Search the database for the corresponding datum code.           */
/* -------------------------------------------------------------------- */
    sprintf( szSearchKey, "%d", nGCSCode );

    nDatum = atoi(CSVGetField( pszFilename, "COORD_REF_SYS_CODE", 
                               szSearchKey, CC_Integer,
                               "DATUM_CODE" ) );

    if( nDatum < 1 )
        return FALSE;

    if( pnDatum != NULL )
        *pnDatum = nDatum;
    
/* -------------------------------------------------------------------- */
/*      Get the PM.                                                     */
/* -------------------------------------------------------------------- */
    nPM = atoi(CSVGetField( pszFilename, "COORD_REF_SYS_CODE", 
                            szSearchKey, CC_Integer,
                            "PRIME_MERIDIAN_CODE" ) );

    if( nPM < 1 )
        return FALSE;

    if( pnPM != NULL )
        *pnPM = nPM;

/* -------------------------------------------------------------------- */
/*      Get the Ellipsoid.                                              */
/* -------------------------------------------------------------------- */
    nEllipsoid = atoi(CSVGetField( pszFilename, "COORD_REF_SYS_CODE", 
                                   szSearchKey, CC_Integer,
                                   "ELLIPSOID_CODE" ) );

    if( nEllipsoid < 1 )
        return FALSE;

    if( pnEllipsoid != NULL )
        *pnEllipsoid = nEllipsoid;

/* -------------------------------------------------------------------- */
/*      Get the angular units.                                          */
/* -------------------------------------------------------------------- */
    nUOMAngle = atoi(CSVGetField( pszFilename, "COORD_REF_SYS_CODE", 
                                  szSearchKey, CC_Integer,
                                  "UOM_CODE" ) );

    if( nUOMAngle < 1 )
        return FALSE;

    if( pnUOMAngle != NULL )
        *pnUOMAngle = nUOMAngle;

/* -------------------------------------------------------------------- */
/*      Get the name, if requested.                                     */
/* -------------------------------------------------------------------- */
    if( ppszName != NULL )
        *ppszName =
            CPLStrdup(CSVGetField( pszFilename, "COORD_REF_SYS_CODE", 
                                   szSearchKey, CC_Integer,
                                   "COORD_REF_SYS_NAME" ));
    
/* -------------------------------------------------------------------- */
/*      Get the datum name, if requested.                               */
/* -------------------------------------------------------------------- */
    if( ppszDatumName != NULL )
        *ppszDatumName =
            CPLStrdup(CSVGetField( pszFilename, "COORD_REF_SYS_CODE", 
                                   szSearchKey, CC_Integer,
                                   "DATUM_NAME" ));
    
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
                                   "ELLIPSOID_NAME" ));
    
    return( TRUE );
}

#define NatOriginLat         8801
#define NatOriginLong        8802
#define NatOriginScaleFactor 8805
#define FalseEasting         8806
#define FalseNorthing        8807
#define ProjCenterLat        8811
#define ProjCenterLong       8812
#define Azimuth              8813
#define AngleRectifiedToSkewedGrid 8814
#define InitialLineScaleFactor 8815
#define ProjCenterEasting    8816
#define ProjCenterNorthing   8817
#define PseudoStdParallelLat 8818
#define PseudoStdParallelScaleFactor 8819
#define FalseOriginLat       8821
#define FalseOriginLong      8822
#define StdParallel1Lat      8823
#define StdParallel2Lat      8824
#define FalseOriginEasting   8826
#define FalseOriginNorthing  8827
#define SphericalOriginLat   8828
#define SphericalOriginLong  8829
#define InitialLongitude     8830
#define ZoneWidth            8831
#define PolarLatStdParallel  8832
#define PolarLongOrigin      8833

/************************************************************************/
/*                         EPSGGetProjTRFInfo()                         */
/*                                                                      */
/*      Transform a PROJECTION_TRF_CODE into a projection method,       */
/*      and a set of parameters.  The parameters identify will          */
/*      depend on the returned method, but they will all have been      */
/*      normalized into degrees and meters.                             */
/************************************************************************/

static int
EPSGGetProjTRFInfo( int nPCS, int * pnProjMethod,
                    int *panParmIds, double * padfProjParms )

{
    int         nProjMethod, i;
    double      adfProjParms[7];
    char        szTRFCode[16];
    char       *pszFilename = CPLStrdup(CSVFilename( "pcs.csv" ));

/* -------------------------------------------------------------------- */
/*      Get the proj method.  If this fails to return a meaningful      */
/*      number, then the whole function fails.                          */
/* -------------------------------------------------------------------- */
    sprintf( szTRFCode, "%d", nPCS );
    nProjMethod =
        atoi( CSVGetField( pszFilename,
                           "COORD_REF_SYS_CODE", szTRFCode, CC_Integer,
                           "COORD_OP_METHOD_CODE" ) );
    if( nProjMethod == 0 )
    {
        CPLFree( pszFilename );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Get the parameters for this projection.                         */
/* -------------------------------------------------------------------- */

    for( i = 0; i < 7; i++ )
    {
        char    szParamUOMID[32], szParamValueID[32], szParamCodeID[32];
        char    *pszValue;
        int     nUOM;
        
        sprintf( szParamCodeID, "PARAMETER_CODE_%d", i+1 );
        sprintf( szParamUOMID, "PARAMETER_UOM_%d", i+1 );
        sprintf( szParamValueID, "PARAMETER_VALUE_%d", i+1 );

        if( panParmIds != NULL )
            panParmIds[i] = 
                atoi(CSVGetField( pszFilename, "COORD_REF_SYS_CODE", szTRFCode,
                                  CC_Integer, szParamCodeID ));

        nUOM = atoi(CSVGetField( pszFilename, "COORD_REF_SYS_CODE", szTRFCode,
                                 CC_Integer, szParamUOMID ));
        pszValue = CPLStrdup(
            CSVGetField( pszFilename, "COORD_REF_SYS_CODE", szTRFCode,
                         CC_Integer, szParamValueID ));

        // there is a bug in the EPSG 6.2.2 database for PCS 2935 and 2936
        // such that they have foot units for the scale factor.  Avoid this.
        if( (panParmIds[i] == NatOriginScaleFactor 
             || panParmIds[i] == InitialLineScaleFactor
             || panParmIds[i] == PseudoStdParallelScaleFactor) 
            && nUOM < 9200 )
            nUOM = 9201;

        if( nUOM >= 9100 && nUOM < 9200 )
            adfProjParms[i] = EPSGAngleStringToDD( pszValue, nUOM );
        else if( nUOM > 9000 && nUOM < 9100 )
        {
            double dfInMeters;

            if( !EPSGGetUOMLengthInfo( nUOM, NULL, &dfInMeters ) )
                dfInMeters = 1.0;
            adfProjParms[i] = atof(pszValue) * dfInMeters;
        }
        else if( EQUAL(pszValue,"") ) /* null field */
        {
            adfProjParms[i] = 0.0;
        }
        else /* really we should consider looking up other scaling factors */
        {
            if( nUOM != 9201 )
                CPLDebug( "OGR", "Non-unity scale factor units!" );
            adfProjParms[i] = atof(pszValue);
        }

        CPLFree( pszValue );
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

    CPLFree( pszFilename );

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
    char        **papszRecord;
    char        szSearchKey[24];
    const char  *pszFilename = CSVFilename( "pcs.csv" );
    
/* -------------------------------------------------------------------- */
/*      Search the units database for this unit.  If we don't find      */
/*      it return failure.                                              */
/* -------------------------------------------------------------------- */
    sprintf( szSearchKey, "%d", nPCSCode );
    papszRecord = CSVScanFileByName( pszFilename, "COORD_REF_SYS_CODE",
                                     szSearchKey, CC_Integer );

    if( papszRecord == NULL )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Get the name, if requested.                                     */
/* -------------------------------------------------------------------- */
    if( ppszEPSGName != NULL )
    {
        CPLString osPCSName = 
            CSLGetField( papszRecord,
                         CSVGetFileFieldId(pszFilename,
                                           "COORD_REF_SYS_NAME"));
            
        const char *pszDeprecated = 
            CSLGetField( papszRecord,
                         CSVGetFileFieldId(pszFilename,
                                           "DEPRECATED") );

        if( pszDeprecated != NULL && *pszDeprecated == '1' )
            osPCSName += " (deprecated)";

        *ppszEPSGName = CPLStrdup(osPCSName);
    }

/* -------------------------------------------------------------------- */
/*      Get the UOM Length code, if requested.                          */
/* -------------------------------------------------------------------- */
    if( pnUOMLengthCode != NULL )
    {
        const char      *pszValue;

        pszValue =
            CSLGetField( papszRecord,
                         CSVGetFileFieldId(pszFilename,"UOM_CODE"));
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
        const char      *pszValue;
        
        pszValue =
            CSLGetField( papszRecord,
                         CSVGetFileFieldId(pszFilename,"UOM_ANGLE_CODE") );
        
        if( atoi(pszValue) > 0 )
            *pnUOMAngleCode = atoi(pszValue);
        else
            *pnUOMAngleCode = 0;
    }

/* -------------------------------------------------------------------- */
/*      Get the GeogCS (Datum with PM) code, if requested.              */
/* -------------------------------------------------------------------- */
    if( pnGeogCS != NULL )
    {
        const char      *pszValue;

        pszValue =
            CSLGetField( papszRecord,
                         CSVGetFileFieldId(pszFilename,"SOURCE_GEOGCRS_CODE"));
        if( atoi(pszValue) > 0 )
            *pnGeogCS = atoi(pszValue);
        else
            *pnGeogCS = 0;
    }

/* -------------------------------------------------------------------- */
/*      Get the GeogCS (Datum with PM) code, if requested.              */
/* -------------------------------------------------------------------- */
    if( pnTRFCode != NULL )
    {
        const char      *pszValue;

        pszValue =
            CSLGetField( papszRecord,
                         CSVGetFileFieldId(pszFilename,"COORD_OP_CODE"));
                         
        
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
    char *pszPMName = NULL, *pszAngleName = NULL;
    double dfPMOffset, dfSemiMajor, dfInvFlattening, adfBursaTransform[7];
    double dfAngleInDegrees, dfAngleInRadians;

    if( !EPSGGetGCSInfo( nGeogCS, &pszGeogCSName,
                         &nDatumCode, &pszDatumName, 
                         &nPMCode, &nEllipsoidCode, &nUOMAngle ) )
        return OGRERR_UNSUPPORTED_SRS;

    if( !EPSGGetPMInfo( nPMCode, &pszPMName, &dfPMOffset ) )
        return OGRERR_UNSUPPORTED_SRS;

    OGREPSGDatumNameMassage( &pszDatumName );

    if( !EPSGGetEllipsoidInfo( nEllipsoidCode, &pszEllipsoidName, 
                               &dfSemiMajor, &dfInvFlattening ) )
        return OGRERR_UNSUPPORTED_SRS;

    if( !EPSGGetUOMAngleInfo( nUOMAngle, &pszAngleName, &dfAngleInDegrees ) )
    {
        pszAngleName = CPLStrdup("degree");
        dfAngleInDegrees = 1.0;
        nUOMAngle = -1;
    }

    if( dfAngleInDegrees == 1.0 )
        dfAngleInRadians = atof(SRS_UA_DEGREE_CONV);
    else
        dfAngleInRadians = atof(SRS_UA_DEGREE_CONV) * dfAngleInDegrees;

    poSRS->SetGeogCS( pszGeogCSName, pszDatumName, 
                      pszEllipsoidName, dfSemiMajor, dfInvFlattening,
                      pszPMName, dfPMOffset,
                      pszAngleName, dfAngleInRadians );

    if( EPSGGetWGS84Transform( nGeogCS, adfBursaTransform ) )
    {
        OGR_SRSNode     *poWGS84;
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

    if( nUOMAngle > 0 )
        poSRS->SetAuthority( "GEOGCS|UNIT", "EPSG", nUOMAngle );

    CPLFree( pszAngleName );
    CPLFree( pszDatumName );
    CPLFree( pszEllipsoidName );
    CPLFree( pszGeogCSName );
    CPLFree( pszPMName );

    return OGRERR_NONE;
}

/************************************************************************/
/*                           OGR_FetchParm()                            */
/*                                                                      */
/*      Fetch a parameter from the parm list, based on it's EPSG        */
/*      parameter code.                                                 */
/************************************************************************/

static double OGR_FetchParm( double *padfProjParms, int *panParmIds, 
                             int nTargetId, double dfFromGreenwich )

{
    int i;
    double dfResult;

/* -------------------------------------------------------------------- */
/*      Set default in meters/degrees.                                  */
/* -------------------------------------------------------------------- */
    switch( nTargetId )
    {
      case NatOriginScaleFactor:
      case InitialLineScaleFactor:
      case PseudoStdParallelScaleFactor:
        dfResult = 1.0;
        break;
        
      case AngleRectifiedToSkewedGrid:
        dfResult = 90.0;
        break;

      default:
        dfResult = 0.0;
    }

/* -------------------------------------------------------------------- */
/*      Try to find actual value in parameter list.                     */
/* -------------------------------------------------------------------- */
    for( i = 0; i < 7; i++ )
    {
        if( panParmIds[i] == nTargetId )
        {
            dfResult = padfProjParms[i];
            break;
        }
    }

/* -------------------------------------------------------------------- */
/*      EPSG longitudes are relative to greenwich.  The follow code     */
/*      could be used to make them relative to the prime meridian of    */
/*      the associated GCS if that was appropriate.  However, the       */
/*      SetNormProjParm() method expects longitudes relative to         */
/*      greenwich, so there is nothing for us to do.                    */
/* -------------------------------------------------------------------- */
#ifdef notdef
    switch( nTargetId )
    {
      case NatOriginLong:
      case ProjCenterLong:
      case FalseOriginLong:
      case SphericalOriginLong:
      case InitialLongitude:
        // Note that the EPSG values are already relative to greenwich.
        // This shift is really making it relative to the provided prime
        // meridian, so that when SetTM() and company the correction back
        // ends up back relative to greenwich.
        dfResult = dfResult + dfFromGreenwich;
        break;

      default:
        ;
    }
#endif

    return dfResult;
}

#define OGR_FP(x) OGR_FetchParm( adfProjParms, anParmIds, (x), \
                                 dfFromGreenwich )

/************************************************************************/
/*                           SetEPSGProjCS()                            */
/************************************************************************/

static OGRErr SetEPSGProjCS( OGRSpatialReference * poSRS, int nPCSCode )

{
    int         nGCSCode, nUOMAngleCode, nUOMLength, nTRFCode, nProjMethod;
    int         anParmIds[7];
    char        *pszPCSName = NULL, *pszUOMLengthName = NULL;
    double      adfProjParms[7], dfInMeters, dfFromGreenwich;
    OGRErr      nErr;
    OGR_SRSNode *poNode;

    if( !EPSGGetPCSInfo( nPCSCode, &pszPCSName, &nUOMLength, &nUOMAngleCode,
                         &nGCSCode, &nTRFCode ) )
        return OGRERR_UNSUPPORTED_SRS;

    poSRS->SetNode( "PROJCS", pszPCSName );
    
/* -------------------------------------------------------------------- */
/*      Set GEOGCS.                                                     */
/* -------------------------------------------------------------------- */
    nErr = SetEPSGGeogCS( poSRS, nGCSCode );
    if( nErr != OGRERR_NONE )
        return nErr;

    dfFromGreenwich = poSRS->GetPrimeMeridian();

/* -------------------------------------------------------------------- */
/*      Set linear units.                                               */
/* -------------------------------------------------------------------- */
    if( !EPSGGetUOMLengthInfo( nUOMLength, &pszUOMLengthName, &dfInMeters ) )
        return OGRERR_UNSUPPORTED_SRS;

    poSRS->SetLinearUnits( pszUOMLengthName, dfInMeters );
    poSRS->SetAuthority( "PROJCS|UNIT", "EPSG", nUOMLength );

    CPLFree( pszUOMLengthName );
    CPLFree( pszPCSName );

/* -------------------------------------------------------------------- */
/*      Set projection and parameters.                                  */
/* -------------------------------------------------------------------- */
    if( !EPSGGetProjTRFInfo( nPCSCode, &nProjMethod, anParmIds, adfProjParms ))
        return OGRERR_UNSUPPORTED_SRS;

    switch( nProjMethod )
    {
      case 9801:
      case 9817: /* really LCC near conformal */
        poSRS->SetLCC1SP( OGR_FP( NatOriginLat ), OGR_FP( NatOriginLong ),
                          OGR_FP( NatOriginScaleFactor ), 
                          OGR_FP( FalseEasting ), OGR_FP( FalseNorthing ) );
        break;

      case 9802:
        poSRS->SetLCC( OGR_FP( StdParallel1Lat ), OGR_FP( StdParallel2Lat ),
                       OGR_FP( FalseOriginLat ), OGR_FP( FalseOriginLong ),
                       OGR_FP( FalseOriginEasting ), 
                       OGR_FP( FalseOriginNorthing ));
        break;

      case 9803:
        poSRS->SetLCCB( OGR_FP( StdParallel1Lat ), OGR_FP( StdParallel2Lat ),
                        OGR_FP( FalseOriginLat ), OGR_FP( FalseOriginLong ),
                        OGR_FP( FalseOriginEasting ), 
                        OGR_FP( FalseOriginNorthing ));
        break;

      case 9804:
      case 9805: /* NOTE: treats 1SP and 2SP cases the same */
        poSRS->SetMercator( OGR_FP( NatOriginLat ), OGR_FP( NatOriginLong ),
                            OGR_FP( NatOriginScaleFactor ), 
                            OGR_FP( FalseEasting ), OGR_FP( FalseNorthing ) );
        break;

      case 9806:
        poSRS->SetCS( OGR_FP( NatOriginLat ), OGR_FP( NatOriginLong ),
                            OGR_FP( FalseEasting ), OGR_FP( FalseNorthing ) );
        break;

      case 9807:
        poSRS->SetTM( OGR_FP( NatOriginLat ), OGR_FP( NatOriginLong ),
                      OGR_FP( NatOriginScaleFactor ), 
                      OGR_FP( FalseEasting ), OGR_FP( FalseNorthing ) );
        break;
        
      case 9808:
        poSRS->SetTMSO( OGR_FP( NatOriginLat ), OGR_FP( NatOriginLong ),
                        OGR_FP( NatOriginScaleFactor ), 
                        OGR_FP( FalseEasting ), OGR_FP( FalseNorthing ) );
        break;
        
      case 9809:
        poSRS->SetOS( OGR_FP( NatOriginLat ), OGR_FP( NatOriginLong ),
                      OGR_FP( NatOriginScaleFactor ), 
                      OGR_FP( FalseEasting ), OGR_FP( FalseNorthing ) );
        break;

      case 9810:
        poSRS->SetPS( OGR_FP( NatOriginLat ), OGR_FP( NatOriginLong ),
                      OGR_FP( NatOriginScaleFactor ), 
                      OGR_FP( FalseEasting ), OGR_FP( FalseNorthing ) );
        break;

      case 9811:
        poSRS->SetNZMG( OGR_FP( NatOriginLat ), OGR_FP( NatOriginLong ),
                        OGR_FP( FalseEasting ), OGR_FP( FalseNorthing ) );
        break;

      case 9812:
      case 9813:
        poSRS->SetHOM( OGR_FP( ProjCenterLat ), OGR_FP( ProjCenterLong ),
                       OGR_FP( Azimuth ), 
                       OGR_FP( AngleRectifiedToSkewedGrid ),
                       OGR_FP( InitialLineScaleFactor ),
                       OGR_FP( FalseEasting ), OGR_FP( FalseNorthing ) );

        poNode = poSRS->GetAttrNode( "PROJECTION" )->GetChild( 0 );
        if( nProjMethod == 9813 )
            poNode->SetValue( SRS_PT_LABORDE_OBLIQUE_MERCATOR );
        break;

      case 9814:
        /* NOTE: This is no longer used!  Swiss Oblique Mercator gets
        ** implemented using 9815 instead.  
        */
        poSRS->SetSOC( OGR_FP( ProjCenterLat ), OGR_FP( ProjCenterLong ),
                       OGR_FP( FalseEasting ), OGR_FP( FalseNorthing ) );
        break;

      case 9815:
        poSRS->SetHOM( OGR_FP( ProjCenterLat ), OGR_FP( ProjCenterLong ),
                       OGR_FP( Azimuth ), 
                       OGR_FP( AngleRectifiedToSkewedGrid ),
                       OGR_FP( InitialLineScaleFactor ),
                       OGR_FP( ProjCenterEasting ), 
                       OGR_FP( ProjCenterNorthing ) );
        break;

      case 9816:
        poSRS->SetTMG( OGR_FP( FalseOriginLat ), OGR_FP( FalseOriginLong ),
                       OGR_FP( FalseOriginEasting ), 
                       OGR_FP( FalseOriginNorthing ) );
        break;

      case 9818:
        poSRS->SetPolyconic( OGR_FP( NatOriginLat ), OGR_FP( NatOriginLong ),
                             OGR_FP( FalseEasting ), OGR_FP( FalseNorthing ) );
        break;

      case 9819:
        poSRS->SetKrovak( OGR_FP( ProjCenterLat ), OGR_FP( ProjCenterLong ),
                          OGR_FP( Azimuth ), 
                          OGR_FP( PseudoStdParallelLat ),
                          OGR_FP( PseudoStdParallelScaleFactor ),
                          OGR_FP( ProjCenterEasting ), 
                          OGR_FP( ProjCenterNorthing ) );
        break;

      case 9820:
        poSRS->SetLAEA( OGR_FP( NatOriginLat ), OGR_FP( NatOriginLong ),
                        OGR_FP( FalseEasting ), OGR_FP( FalseNorthing ) );
        break;

      case 9821: /* this is the spherical form, and really needs different
                    equations which give different results but PROJ.4 doesn't
                    seem to support the spherical form. */
        poSRS->SetLAEA( OGR_FP( SphericalOriginLat ), 
                        OGR_FP( SphericalOriginLong ),
                        OGR_FP( FalseEasting ), OGR_FP( FalseNorthing ) );
        break;

      case 9822: /* Albers (Conic) Equal Area */
        poSRS->SetACEA( OGR_FP( StdParallel1Lat ), 
                        OGR_FP( StdParallel2Lat ), 
                        OGR_FP( FalseOriginLat ),
                        OGR_FP( FalseOriginLong ),
                        OGR_FP( FalseOriginEasting ),
                        OGR_FP( FalseOriginNorthing ) );
        break;

      case 9823: /* Equidistant Cylindrical / Plate Carre / Equirectangular */
        poSRS->SetEquirectangular( OGR_FP( NatOriginLat ),
                                   OGR_FP( NatOriginLong ), 
                                   0.0, 0.0 );
        break;

      case 9829: /* Polar Stereographic (Variant B) */
          poSRS->SetPS( OGR_FP( PolarLatStdParallel ), OGR_FP(PolarLongOrigin),
                        1.0,
                        OGR_FP( FalseEasting ), OGR_FP( FalseNorthing ) );
          break;

      default:
        CPLDebug( "EPSG", "No WKT support for projection method %d.",
                  nProjMethod );
        return OGRERR_UNSUPPORTED_SRS;
    }

/* -------------------------------------------------------------------- */
/*      Set overall PCS authority code.                                 */
/* -------------------------------------------------------------------- */
    poSRS->SetAuthority( "PROJCS", "EPSG", nPCSCode );

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
 * This method is the same as the C function OSRImportFromEPSG().
 *
 * @param nCode a GCS or PCS code from the horizontal coordinate system table.
 * 
 * @return OGRERR_NONE on success, or an error code on failure.
 */

OGRErr OGRSpatialReference::importFromEPSG( int nCode )

{
    OGRErr  eErr;

    bNormInfoSet = FALSE;

/* -------------------------------------------------------------------- */
/*      Clear any existing definition.                                  */
/* -------------------------------------------------------------------- */
    if( GetRoot() != NULL )
    {
        delete poRoot;
        poRoot = NULL;
    }

/* -------------------------------------------------------------------- */
/*      Verify that we can find the required filename(s).               */
/* -------------------------------------------------------------------- */
    if( CSVScanFileByName( CSVFilename( "gcs.csv" ),
                           "COORD_REF_SYS_CODE", 
                           "4269", CC_Integer ) == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Unable to open EPSG support file %s.\n"
                  "Try setting the GDAL_DATA environment variable to point to the\n"
                  "directory containing EPSG csv files.", 
                  CSVFilename( "gcs.csv" ) );
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Is this a GeogCS code?   this is inadequate as a criteria       */
/* -------------------------------------------------------------------- */
    if( EPSGGetGCSInfo( nCode, NULL, NULL, NULL, NULL, NULL, NULL ) )
        eErr = SetEPSGGeogCS( this, nCode );
    else
        eErr = SetEPSGProjCS( this, nCode );

/* -------------------------------------------------------------------- */
/*      If we get it as an unsupported code, try looking it up in       */
/*      the epsg.wkt coordinate system dictionary.                      */
/* -------------------------------------------------------------------- */
    if( eErr == OGRERR_UNSUPPORTED_SRS )
    {
        char szCode[32];
        sprintf( szCode, "%d", nCode );
        eErr = importFromDict( "epsg.wkt", szCode );
    }

/* -------------------------------------------------------------------- */
/*      If we get it as an unsupported code, try looking it up in       */
/*      the PROJ.4 support file(s).                                     */
/* -------------------------------------------------------------------- */
    static int bLoopingToProj4 = FALSE;

    if( eErr == OGRERR_UNSUPPORTED_SRS && !bLoopingToProj4 )
    {
        char szWrkDefn[100];

        sprintf( szWrkDefn, "+init=epsg:%d", nCode );
        bLoopingToProj4 = TRUE;
        CPLPushErrorHandler( CPLQuietErrorHandler );
        eErr = SetFromUserInput( szWrkDefn );
        CPLPopErrorHandler();
        bLoopingToProj4 = FALSE;

        if( eErr != OGRERR_NONE )
            eErr = OGRERR_UNSUPPORTED_SRS;
        else
        {
            if( IsProjected() )
                SetAuthority( "PROJCS", "EPSG", nCode );
            else if( IsGeographic() )
                SetAuthority( "GEOGCS", "EPSG", nCode );
        }
    }

    if( eErr == OGRERR_UNSUPPORTED_SRS )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "EPSG PCS/GCS code %d not found in EPSG support files.  Is this a valid\nEPSG coordinate system?", 
                  nCode );
    }

/* -------------------------------------------------------------------- */
/*      Make sure any peculiarities in the ordering are fixed up.       */
/* -------------------------------------------------------------------- */
    if( eErr == OGRERR_NONE )
        eErr = FixupOrdering();

    return eErr;
}

/************************************************************************/
/*                         OSRImportFromEPSG()                          */
/************************************************************************/

OGRErr CPL_STDCALL OSRImportFromEPSG( OGRSpatialReferenceH hSRS, int nCode )

{
    return ((OGRSpatialReference *) hSRS)->importFromEPSG( nCode );
}

/************************************************************************/
/*                           SetStatePlane()                            */
/************************************************************************/

/**
 * Set State Plane projection definition.
 *
 * This will attempt to generate a complete definition of a state plane
 * zone based on generating the entire SRS from the EPSG tables.  If the
 * EPSG tables are unavailable, it will produce a stubbed LOCAL_CS definition
 * and return OGRERR_FAILURE.
 *
 * This method is the same as the C function OSRSetStatePlaneWithUnits().
 *
 * @param nZone State plane zone number, in the USGS numbering scheme (as
 * dinstinct from the Arc/Info and Erdas numbering scheme. 
 *
 * @param bNAD83 TRUE if the NAD83 zone definition should be used or FALSE
 * if the NAD27 zone definition should be used.  
 *
 * @param pszOverrideUnitName Linear unit name to apply overriding the 
 * legal definition for this zone.
 *
 * @param dfOverrideUnit Linear unit conversion factor to apply overriding
 * the legal definition for this zone. 
 * 
 * @return OGRERR_NONE on success, or OGRERR_FAILURE on failure, mostly likely
 * due to the EPSG tables not being accessable. 
 */

OGRErr OGRSpatialReference::SetStatePlane( int nZone, int bNAD83,
                                           const char *pszOverrideUnitName,
                                           double dfOverrideUnit )

{
    int         nAdjustedId;
    int         nPCSCode;
    char        szID[32];

/* -------------------------------------------------------------------- */
/*      Get the index id from stateplane.csv.                           */
/* -------------------------------------------------------------------- */
    if( bNAD83 )
        nAdjustedId = nZone;
    else
        nAdjustedId = nZone + 10000;

/* -------------------------------------------------------------------- */
/*      Turn this into a PCS code.  We assume there will only be one    */
/*      PCS corresponding to each Proj_ code since the proj code        */
/*      already effectively indicates NAD27 or NAD83.                   */
/* -------------------------------------------------------------------- */
    sprintf( szID, "%d", nAdjustedId );
    nPCSCode =
        atoi( CSVGetField( CSVFilename( "stateplane.csv" ),
                           "ID", szID, CC_Integer,
                           "EPSG_PCS_CODE" ) );
    if( nPCSCode < 1 )
    {
        char    szName[128];
        static int bFailureReported = FALSE;

        if( !bFailureReported )
        {
            bFailureReported = TRUE;
            CPLError( CE_Warning, CPLE_OpenFailed, 
                      "Unable to find state plane zone in stateplane.csv,\n"
                      "likely because the GDAL data files cannot be found.  Using\n"
                      "incomplete definition of state plane zone.\n" );
        }

        Clear();
        if( bNAD83 )
        {
            sprintf( szName, "State Plane Zone %d / NAD83", nZone );
            SetLocalCS( szName );
            SetLinearUnits( SRS_UL_METER, 1.0 );
        }
        else
        {
            sprintf( szName, "State Plane Zone %d / NAD27", nZone );
            SetLocalCS( szName );
            SetLinearUnits( SRS_UL_US_FOOT, atof(SRS_UL_US_FOOT_CONV) );
        }

        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Define based on a full EPSG definition of the zone.             */
/* -------------------------------------------------------------------- */
    OGRErr eErr = importFromEPSG( nPCSCode );

    if( eErr != OGRERR_NONE )
        return eErr;

/* -------------------------------------------------------------------- */
/*      Apply units override if required.                               */
/*                                                                      */
/*      We will need to adjust the linear projection parameter to       */
/*      match the provided units, and clear the authority code.         */
/* -------------------------------------------------------------------- */
    if( dfOverrideUnit != 0.0 
        && fabs(dfOverrideUnit - GetLinearUnits()) > 0.0000000001 )
    {
        double dfFalseEasting = GetNormProjParm( SRS_PP_FALSE_EASTING );
        double dfFalseNorthing= GetNormProjParm( SRS_PP_FALSE_NORTHING);
        OGR_SRSNode *poPROJCS;

        SetLinearUnits( pszOverrideUnitName, dfOverrideUnit );
        
        SetNormProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
        SetNormProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

        poPROJCS = GetAttrNode( "PROJCS" );
        if( poPROJCS != NULL && poPROJCS->FindChild( "AUTHORITY" ) != -1 )
        {
            poPROJCS->DestroyChild( poPROJCS->FindChild( "AUTHORITY" ) );
        }
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                          OSRSetStatePlane()                          */
/************************************************************************/

OGRErr OSRSetStatePlane( OGRSpatialReferenceH hSRS, int nZone, int bNAD83 )

{
    return ((OGRSpatialReference *) hSRS)->SetStatePlane( nZone, bNAD83 );
}

/************************************************************************/
/*                     OSRSetStatePlaneWithUnits()                      */
/************************************************************************/

OGRErr OSRSetStatePlaneWithUnits( OGRSpatialReferenceH hSRS, 
                                  int nZone, int bNAD83,
                                  const char *pszOverrideUnitName,
                                  double dfOverrideUnit )

{
    return ((OGRSpatialReference *) hSRS)->SetStatePlane( nZone, bNAD83,
                                                          pszOverrideUnitName,
                                                          dfOverrideUnit );
}

/************************************************************************/
/*                          AutoIdentifyEPSG()                          */
/************************************************************************/

/**
 * Set EPSG authority info if possible.
 *
 * This method inspects a WKT definition, and adds EPSG authority nodes
 * where an aspect of the coordinate system can be easily and safely 
 * corresponded with an EPSG identifier.  In practice, this method will 
 * evolve over time.  In theory it can add authority nodes for any object
 * (ie. spheroid, datum, GEOGCS, units, and PROJCS) that could have an 
 * authority node.  Mostly this is useful to inserting appropriate 
 * PROJCS codes for common formulations (like UTM n WGS84). 
 *
 * If it success the OGRSpatialReference is updated in place, and the 
 * method return OGRERR_NONE.  If the method fails to identify the 
 * general coordinate system OGRERR_UNSUPPORTED_SRS is returned but no 
 * error message is posted via CPLError(). 
 *
 * This method is the same as the C function OSRAutoIdentifyEPSG().
 *
 * @return OGRERR_NONE or OGRERR_UNSUPPORTED_SRS.
 */

OGRErr OGRSpatialReference::AutoIdentifyEPSG()

{
/* -------------------------------------------------------------------- */
/*      Is this a UTM coordinate system with a common GEOGCS?           */
/* -------------------------------------------------------------------- */
    int nZone, bNorth;
    if( (nZone = GetUTMZone( &bNorth )) != 0 
        && GetAuthorityCode( "PROJCS") == NULL )
    {
        const char *pszAuthName, *pszAuthCode;

        pszAuthName = GetAuthorityName( "PROJCS|GEOGCS" );
        pszAuthCode = GetAuthorityCode( "PROJCS|GEOGCS" );

        if( pszAuthName == NULL ||  pszAuthCode == NULL )
        {
            /* don't exactly recognise datum */
        }
        else if( EQUAL(pszAuthName,"EPSG") && atoi(pszAuthCode) == 4326 )
        { // WGS84
            if( bNorth ) 
                SetAuthority( "PROJCS", "EPSG", 32600 + nZone );
            else
                SetAuthority( "PROJCS", "EPSG", 32700 + nZone );
        }
        else if( EQUAL(pszAuthName,"EPSG") && atoi(pszAuthCode) == 4267 
                 && nZone >= 3 && nZone <= 22 && bNorth )
            SetAuthority( "PROJCS", "EPSG", 26700 + nZone ); // NAD27
        else if( EQUAL(pszAuthName,"EPSG") && atoi(pszAuthCode) == 4269
                 && nZone >= 3 && nZone <= 23 && bNorth )
            SetAuthority( "PROJCS", "EPSG", 26900 + nZone ); // NAD83
        else if( EQUAL(pszAuthName,"EPSG") && atoi(pszAuthCode) == 4322 )
        { // WGS72
            if( bNorth ) 
                SetAuthority( "PROJCS", "EPSG", 32200 + nZone );
            else
                SetAuthority( "PROJCS", "EPSG", 32300 + nZone );
        }
    }

/* -------------------------------------------------------------------- */
/*      Return.                                                         */
/* -------------------------------------------------------------------- */
    if( IsProjected() && GetAuthorityCode("PROJCS") != NULL )
        return OGRERR_NONE;
    else if( IsGeographic() && GetAuthorityCode("GEOGCS") != NULL )
        return OGRERR_NONE;
    else
        return OGRERR_UNSUPPORTED_SRS;
}

/************************************************************************/
/*                        OSRAutoIdentifyEPSG()                         */
/************************************************************************/

OGRErr OSRAutoIdentifyEPSG( OGRSpatialReferenceH hSRS )

{
    return ((OGRSpatialReference *) hSRS)->AutoIdentifyEPSG();
}
