/******************************************************************************
 * $Id$
 *
 * Project:  libgeotiff
 * Purpose:  Code to normalize PCS and other composite codes in a GeoTIFF file.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
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
 * Revision 1.1  1999/02/22 18:51:08  warmerda
 * New
 *
 */
 
#include "cpl_csv.h"
#include "geotiff.h"
#include "geo_normalize.h"

#ifndef KvUserDefined
#  define KvUserDefined 32767
#endif

/************************************************************************/
/*                            CSVFilename()                             */
/*                                                                      */
/*      Return the full path to a particular CSV file.  This will       */
/*      eventually be something the application can override.           */
/************************************************************************/

static const char * CSVFilename( const char *pszBasename )

{
    static char		szPath[512];

    sprintf( szPath, "/home/warmerda/gdal/frmts/gtiff/newcsv/%s", pszBasename );

    return( szPath );
}

/************************************************************************/
/*                           GTIFGetPCSInfo()                           */
/************************************************************************/

int GTIFGetPCSInfo( int nPCSCode, char **ppszEPSGName,
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
            *pnUOMLengthCode = KvUserDefined;
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
            *pnUOMAngleCode = KvUserDefined;
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
            *pnGeogCS = KvUserDefined;
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
            *pnTRFCode = KvUserDefined;
    }

    return TRUE;
}

/************************************************************************/
/*                           GTIFAngleToDD()                            */
/*                                                                      */
/*      Convert an angle in the specified units to decimal degrees.     */
/************************************************************************/

double GTIFAngleStringToDD( const char * pszAngle, int nUOMAngle )

{
    double	dfAngle;
    
    if( nUOMAngle == 9110 )		/* DDD.MMSSsss */
    {
        char	*pszDecimal;
        
        dfAngle = ABS(atoi(pszAngle));
        pszDecimal = strchr(pszAngle,'.');
        if( pszDecimal != NULL )
        {
            dfAngle += ((int) (atof(pszDecimal)*100)) / 60.0;
            if( strlen(pszDecimal) > 3 )
                dfAngle += atof(pszDecimal+3) / 3600.0;
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
        CPLAssert( nUOMAngle == 9102 || nUOMAngle == KvUserDefined
                   || nUOMAngle == 0 );
        
        dfAngle = atof(pszAngle );
    }

    return( dfAngle );
}

/************************************************************************/
/*                           GTIFGetGCSInfo()                           */
/*                                                                      */
/*      Fetch the datum, and prime meridian related to a particular     */
/*      GCS.                                                            */
/************************************************************************/

int GTIFGetGCSInfo( int nGCSCode, char ** ppszName, int * pnDatum, int * pnPM )

{
    char	szSearchKey[24];
    int		nDatum, nPM;

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
/*                        GTIFGetEllipsoidInfo()                        */
/*                                                                      */
/*      Fetch info about an ellipsoid.  Axes are always returned in     */
/*      meters.  SemiMajor computed based on inverse flattening         */
/*      where that is provided.                                         */
/************************************************************************/

int GTIFGetEllipsoidInfo( int nGCSCode, char ** ppszName,
                          double * pdfSemiMajor, double * pdfSemiMinor )

{
    char	szSearchKey[24];
    double	dfSemiMajor, dfToMeters = 1.0;
    int		nUOMLength;
    
/* -------------------------------------------------------------------- */
/*      Get the semi major axis.                                        */
/* -------------------------------------------------------------------- */
    sprintf( szSearchKey, "%d", nGCSCode );

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
    GTIFGetUOMLengthInfo( nUOMLength, NULL, &dfToMeters );

    dfSemiMajor *= dfToMeters;
    
    if( pdfSemiMajor != NULL )
        *pdfSemiMajor = dfSemiMajor;
    
/* -------------------------------------------------------------------- */
/*      Get the semi-minor if requested.  If the Semi-minor axis        */
/*      isn't available, compute it based on the inverse flattening.    */
/* -------------------------------------------------------------------- */
    if( pdfSemiMinor != NULL )
    {
        *pdfSemiMinor =
            atof(CSVGetField( CSVFilename("ellipsoid.csv" ),
                              "ELLIPSOID_CODE", szSearchKey, CC_Integer,
                              "SEMI_MINOR_AXIS" )) * dfToMeters;

        if( *pdfSemiMinor == 0.0 )
        {
            double	dfInvFlattening;
            
            dfInvFlattening = 
                atof(CSVGetField( CSVFilename("ellipsoid.csv" ),
                                  "ELLIPSOID_CODE", szSearchKey, CC_Integer,
                                  "INV_FLATTENING" ));
            *pdfSemiMinor = dfSemiMajor * (1 - 1.0/dfInvFlattening);
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
/*                           GTIFGetPMInfo()                            */
/*                                                                      */
/*      Get the offset between a given prime meridian and Greenwich     */
/*      in degrees.                                                     */
/************************************************************************/

int GTIFGetPMInfo( int nPMCode, char ** ppszName, double *pdfOffset )

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
            GTIFAngleStringToDD(
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
/*                          GTIFGetDatumInfo()                          */
/*                                                                      */
/*      Fetch the ellipsoid, and name for a datum.                      */
/************************************************************************/

int GTIFGetDatumInfo( int nDatumCode, char ** ppszName, int * pnEllipsoid )

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
/*                        GTIFGetUOMLengthInfo()                        */
/*                                                                      */
/*      Note: This function should eventually also know how to          */
/*      lookup length aliases in the UOM_LE_ALIAS table.                */
/************************************************************************/

int GTIFGetUOMLengthInfo( int nUOMLengthCode,
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
            *ppszUOMName = CPLStrdup( "meter" );
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
/*                         GTIFGetProjTRFInfo()                         */
/*                                                                      */
/*      Transform a PROJECTION_TRF_CODE into a projection method,       */
/*      and a set of parameters.  The parameters identify will          */
/*      depend on the returned method, but they will all have been      */
/*      normalized into degrees and meters.                             */
/************************************************************************/

int GTIFGetProjTRFInfo( int nProjTRFCode, int * pnProjMethod,
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

    if( !GTIFGetUOMLengthInfo( nUOMLinear, NULL, &dfInMeters ) )
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
            adfProjParms[i] = GTIFAngleStringToDD( pszValue, nUOMAngle );
        else if( i > 4 )
            adfProjParms[i] = atof(pszValue) * dfInMeters;
        else
            adfProjParms[i] = atof( pszValue );
            
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
/*                            GTIFGetDefn()                             */
/*                                                                      */
/*      Try and read, and build a fully normalized set of               */
/*      information about the files projection.                         */
/************************************************************************/

int GTIFGetDefn( GTIF * psGTIF, GTIFDefn * psDefn )

{
    int		i;
    
/* -------------------------------------------------------------------- */
/*      Initially we default all the information we can.                */
/* -------------------------------------------------------------------- */
    psDefn->Model = KvUserDefined;
    psDefn->PCS = KvUserDefined;
    psDefn->GCS = KvUserDefined;
    psDefn->UOMLength = KvUserDefined;
    psDefn->UOMLengthInMeters = 1.0;
    psDefn->Datum = KvUserDefined;
    psDefn->Ellipsoid = KvUserDefined;
    psDefn->SemiMajor = 0.0;
    psDefn->SemiMinor = 0.0;
    psDefn->PM = KvUserDefined;
    psDefn->PMLongToGreenwich = 0.0;

    psDefn->Projection = KvUserDefined;

    for( i = 0; i < sizeof(psDefn->ProjParms)/sizeof(double); i++ )
        psDefn->ProjParms[i] = 0.0;

/* -------------------------------------------------------------------- */
/*	Try to get the overall model type.				*/
/* -------------------------------------------------------------------- */
    GTIFKeyGet(psGTIF,ProjectedCSTypeGeoKey,&(psDefn->Model),0,1);

/* -------------------------------------------------------------------- */
/*      Try to get a PCS.                                               */
/* -------------------------------------------------------------------- */
    if( GTIFKeyGet(psGTIF,ProjectedCSTypeGeoKey,&(psDefn->PCS),0,1) == 1
        && psDefn->PCS != KvUserDefined )
    {
        int		nProjTRFCode, nUOMAngle;
        
        /*
         * Translate this into useful information.
         */
        if( GTIFGetPCSInfo( psDefn->PCS, NULL,
                            &(psDefn->UOMLength), &(nUOMAngle),
                            &(psDefn->GCS), &nProjTRFCode ) )
        {
            /*
             * We have an underlying projection transformation value.  Look
             * this up.  For a PCS of ``WGS 84 / UTM 11'' the transformation
             * would be Transverse Mercator, with a particular set of options.
             * The nProjTRFCode itself would correspond to the name
             * ``UTM zone 11N'', and doesn't include datum info.
             */
            GTIFGetProjTRFInfo( nProjTRFCode,
                                &(psDefn->Projection),
                                psDefn->ProjParms );
        }
    }

/* -------------------------------------------------------------------- */
/*      Try to get a GCS.  If found, it will override any implied by    */
/*      the PCS.                                                        */
/* -------------------------------------------------------------------- */
    GTIFKeyGet(psGTIF, GeographicTypeGeoKey, &(psDefn->GCS), 0, 1 );

/* -------------------------------------------------------------------- */
/*      Derive the datum, and prime meridian from the GCS.              */
/* -------------------------------------------------------------------- */
    if( psDefn->GCS != KvUserDefined )
    {
        GTIFGetGCSInfo( psDefn->GCS, NULL, &(psDefn->Datum), &(psDefn->PM) );
    }
    
/* -------------------------------------------------------------------- */
/*      Check for a datum setting, and then use the datum to derive     */
/*      an ellipsoid.                                                   */
/* -------------------------------------------------------------------- */
    GTIFKeyGet(psGTIF, GeogGeodeticDatumGeoKey, &(psDefn->Datum), 0, 1 );

    if( psDefn->Datum != KvUserDefined )
    {
        GTIFGetDatumInfo( psDefn->Datum, NULL, &(psDefn->Ellipsoid) );
    }

/* -------------------------------------------------------------------- */
/*      Check for an explicit ellipsoid.  Use the ellipsoid to          */
/*      derive the ellipsoid characteristics, if possible.              */
/* -------------------------------------------------------------------- */
    GTIFKeyGet(psGTIF, GeogEllipsoidGeoKey, &(psDefn->Ellipsoid), 0, 1 );

    if( psDefn->Ellipsoid != KvUserDefined )
    {
        GTIFGetEllipsoidInfo( psDefn->Ellipsoid, NULL,
                              &(psDefn->SemiMajor), &(psDefn->SemiMinor) );
    }
    
/* -------------------------------------------------------------------- */
/*      Get the prime meridian info.                                    */
/* -------------------------------------------------------------------- */
    GTIFKeyGet(psGTIF, GeogPrimeMeridianGeoKey, &(psDefn->PM), 0, 1 );

    if( psDefn->PM != KvUserDefined )
    {
        GTIFGetPMInfo( psDefn->PM, NULL, &(psDefn->PMLongToGreenwich) );
    }

    return TRUE;
}

/************************************************************************/
/*                           GTIFPrintDefn()                            */
/*                                                                      */
/*      Report the contents of a GTIFDefn structure ... mostly for      */
/*      debugging.                                                      */
/************************************************************************/

void GTIFPrintDefn( GTIFDefn * psDefn, FILE * fp )

{
/* -------------------------------------------------------------------- */
/*      Get the PCS name if possible.                                   */
/* -------------------------------------------------------------------- */
    if( psDefn->PCS != KvUserDefined )
    {
        char	*pszPCSName = "name unknown";
    
        GTIFGetPCSInfo( psDefn->PCS, &pszPCSName, NULL, NULL, NULL, NULL );
        fprintf( fp, "PCS = %d (%s)\n", psDefn->PCS, pszPCSName );
    }

/* -------------------------------------------------------------------- */
/*      Try to dump the projection method name, and parameters if possible.*/
/* -------------------------------------------------------------------- */
    if( psDefn->Projection != KvUserDefined )
    {
        char	szKey[16];
        int     i;

        sprintf( szKey, "%d", psDefn->Projection );
        printf( "Projection Method: %s\n",
                CSVGetField( CSVFilename( "trf_method.csv" ),
                             "COORD_TRF_METHOD_CODE", szKey, CC_Integer,
                             "CTRF_METHOD_EPSG_NAME" ) );

        for( i = 0; i < 7; i++ )
        {
            char	szFieldName[32];
            const char *pszValue;

            sprintf( szFieldName, "PARAM_%d_NAME", i+1 );

            pszValue =
                CSVGetField( CSVFilename( "trf_method.csv" ),
                             "COORD_TRF_METHOD_CODE", szKey, CC_Integer,
                             szFieldName );

            if( strlen(pszValue) == 0 )
                continue;
            
            printf( "   %s: %f\n", pszValue, psDefn->ProjParms[i] );
        }
    }

/* -------------------------------------------------------------------- */
/*      Report the GCS name, and number.                                */
/* -------------------------------------------------------------------- */
    if( psDefn->GCS != KvUserDefined )
    {
        char	*pszName = NULL;

        GTIFGetGCSInfo( psDefn->GCS, &pszName, NULL, NULL );
        printf( "GCS: %d/%s\n", psDefn->GCS, pszName );
    }

/* -------------------------------------------------------------------- */
/*      Report the datum name.                                          */
/* -------------------------------------------------------------------- */
    if( psDefn->Datum != KvUserDefined )
    {
        char	*pszName = NULL;

        GTIFGetDatumInfo( psDefn->Datum, &pszName, NULL );
        printf( "Datum: %d/%s\n", psDefn->Datum, pszName );
    }

/* -------------------------------------------------------------------- */
/*      Report the ellipsoid.                                           */
/* -------------------------------------------------------------------- */
    if( psDefn->Ellipsoid != KvUserDefined )
    {
        char	*pszName = NULL;

        GTIFGetEllipsoidInfo( psDefn->Ellipsoid, &pszName, NULL, NULL );
        printf( "Ellipsoid: %d/%s (%.2f,%.2f)\n",
                psDefn->Ellipsoid, pszName,
                psDefn->SemiMajor, psDefn->SemiMinor );
    }
    
/* -------------------------------------------------------------------- */
/*      Report the prime meridian.                                      */
/* -------------------------------------------------------------------- */
    if( psDefn->PM != KvUserDefined )
    {
        char	*pszName = NULL;

        GTIFGetPMInfo( psDefn->PM, &pszName, NULL );
        printf( "Prime Meridian: %d/%s (%f)\n",
                psDefn->Ellipsoid, pszName, psDefn->PMLongToGreenwich );
    }
}

