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
 * Revision 1.3  1999/03/02 21:10:57  warmerda
 * added lots of projections
 *
 * Revision 1.2  1999/02/24 16:24:15  warmerda
 * Continuing to evolve
 *
 * Revision 1.1  1999/02/22 18:51:08  warmerda
 * New
 *
 */
 
#include "cpl_csv.h"
#include "geotiff.h"
#include "xtiffio.h"
#include "geovalues.h"
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
/*      Convert a numeric angle to decimal degress.                     */
/************************************************************************/

double GTIFAngleToDD( double dfAngle, int nUOMAngle )

{
    if( nUOMAngle == 9110 )		/* DDD.MMSSsss */
    {
        char	szAngleString[32];

        sprintf( szAngleString, "%12.7f", dfAngle );
        dfAngle = GTIFAngleStringToDD( szAngleString, nUOMAngle );
    }
    else if( nUOMAngle == 9105 || nUOMAngle == 9106 )	/* grad */
    {
        dfAngle = 180 * (dfAngle / 200);
    }
    else if( nUOMAngle == 9101 )			/* radians */
    {
        dfAngle = 180 * (dfAngle / PI);
    }
    else if( nUOMAngle == 9103 )			/* arc-minute */
    {
        dfAngle = dfAngle / 60;
    }
    else if( nUOMAngle == 9104 )			/* arc-second */
    {
        dfAngle = dfAngle / 3600;
    }
    else /* decimal degrees ... some cases missing but seeminly never used */
    {
        CPLAssert( nUOMAngle == 9102 || nUOMAngle == KvUserDefined
                   || nUOMAngle == 0 );
    }

    return( dfAngle );
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
/*                    EPSGProjMethodToCTProjMethod()                    */
/*                                                                      */
/*      Convert between the EPSG enumeration for projection methods,    */
/*      and the GeoTIFF CT codes.                                       */
/************************************************************************/

static int EPSGProjMethodToCTProjMethod( int nEPSG )

{
    /* see trf_method.csv for list of EPSG codes */
    
    switch( nEPSG )
    {
      case 9801:
        return( CT_LambertConfConic_1SP );

      case 9802:
        return( CT_LambertConfConic_2SP );

      case 9803:
        return( CT_LambertConfConic_2SP ); /* Belgian variant not supported */

      case 9804:
        return( CT_Mercator );  /* 1SP and 2SP not differentiated */

      case 9805:
        return( CT_Mercator );  /* 1SP and 2SP not differentiated */

      case 9806:
        return( CT_CassiniSoldner );

      case 9807:
        return( CT_TransverseMercator );

      case 9808:
        return( CT_TransvMercator_Modified_Alaska );

      case 9809:
        return( CT_ObliqueStereographic );

      case 9810:
        return( CT_PolarStereographic );

      case 9811:
        return( CT_NewZealandMapGrid );

      case 9812:
        return( CT_ObliqueMercator_Hotine );

      case 9813:
        return( CT_ObliqueMercator_Laborde );

      case 9814:
        return( CT_ObliqueMercator_Rosenmund ); /* swiss  */

      case 9815:
        return( CT_ObliqueMercator_Hotine ); /* what's the difference? */

      case 9816: /* tunesia mining grid has no counterpart */
        return( KvUserDefined );
    }

    return( KvUserDefined );
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
/*                            SetGTParmIds()                            */
/*                                                                      */
/*      This is hardcoded logic to set the GeoTIFF parmaeter            */
/*      identifiers for all the EPSG supported projections.  As the     */
/*      trf_method.csv table grows with new projections, this code      */
/*      will need to be updated.                                        */
/************************************************************************/

static int SetGTParmIds( GTIFDefn * psDefn )

{
    psDefn->nParms = 7;
    
    switch( psDefn->CTProjection )
    {
      case CT_CassiniSoldner:
      case CT_NewZealandMapGrid:
        psDefn->ProjParmId[0] = ProjNatOriginLatGeoKey;
        psDefn->ProjParmId[1] = ProjNatOriginLongGeoKey;
        psDefn->ProjParmId[5] = ProjFalseEastingGeoKey;
        psDefn->ProjParmId[6] = ProjFalseNorthingGeoKey;
        return TRUE;

      case CT_ObliqueMercator:
        psDefn->ProjParmId[0] = ProjCenterLatGeoKey;
        psDefn->ProjParmId[1] = ProjCenterLongGeoKey;
        psDefn->ProjParmId[2] = ProjAzimuthAngleGeoKey;
        /*psDefn->ProjParmId[3] = angled from rectified to skew grid not sup.*/
        psDefn->ProjParmId[4] = ProjScaleAtCenterGeoKey;
        psDefn->ProjParmId[5] = ProjFalseEastingGeoKey;
        psDefn->ProjParmId[6] = ProjFalseNorthingGeoKey;
        return TRUE;

      case CT_ObliqueMercator_Laborde:
        psDefn->ProjParmId[0] = ProjCenterLatGeoKey;
        psDefn->ProjParmId[1] = ProjCenterLongGeoKey;
        psDefn->ProjParmId[2] = ProjAzimuthAngleGeoKey;
        psDefn->ProjParmId[4] = ProjScaleAtCenterGeoKey;
        psDefn->ProjParmId[5] = ProjFalseEastingGeoKey;
        psDefn->ProjParmId[6] = ProjFalseNorthingGeoKey;
        return TRUE;
        
      case CT_LambertConfConic_1SP:
      case CT_Mercator:
      case CT_ObliqueStereographic:
      case CT_PolarStereographic:
      case CT_TransverseMercator:
      case CT_TransvMercator_SouthOriented:
        psDefn->ProjParmId[0] = ProjNatOriginLatGeoKey;
        psDefn->ProjParmId[1] = ProjNatOriginLongGeoKey;
        psDefn->ProjParmId[4] = ProjScaleAtNatOriginGeoKey;
        psDefn->ProjParmId[5] = ProjFalseEastingGeoKey;
        psDefn->ProjParmId[6] = ProjFalseNorthingGeoKey;
        return TRUE;

      case CT_LambertConfConic_2SP:
        psDefn->ProjParmId[0] = ProjStdParallel1GeoKey;
        psDefn->ProjParmId[1] = ProjStdParallel2GeoKey;
        psDefn->ProjParmId[2] = ProjFalseOriginLatGeoKey;
        psDefn->ProjParmId[3] = ProjFalseOriginLongGeoKey;
        psDefn->ProjParmId[5] = ProjFalseEastingGeoKey;
        psDefn->ProjParmId[6] = ProjFalseNorthingGeoKey;
        return TRUE;

      case CT_SwissObliqueCylindrical:
        psDefn->ProjParmId[0] = ProjCenterLatGeoKey;
        psDefn->ProjParmId[1] = ProjCenterLongGeoKey;
        psDefn->ProjParmId[5] = ProjFalseEastingGeoKey;
        psDefn->ProjParmId[6] = ProjFalseNorthingGeoKey;
        return TRUE;

      default:
        return( FALSE );
    }
}

/************************************************************************/
/*                         GTIFFetchProjParms()                         */
/*                                                                      */
/*      Fetch the projection parameters for a particular projection     */
/*      from a GeoTIFF file, and fill the GTIFDefn structure out        */
/*      with them.                                                      */
/************************************************************************/

static void GTIFFetchProjParms( GTIF * psGTIF, GTIFDefn * psDefn )

{
    double	dfNatOriginLong, dfNatOriginLat;
    double	dfFalseEasting, dfFalseNorthing, dfNatOriginScale;
    double	dfStdParallel1, dfStdParallel2, dfAzimuth;

/* -------------------------------------------------------------------- */
/*      Get the false easting, and northing if available.               */
/* -------------------------------------------------------------------- */
    if( !GTIFKeyGet(psGTIF, ProjFalseEastingGeoKey, &dfFalseEasting, 0, 1)
        && !GTIFKeyGet(psGTIF, ProjCenterEastingGeoKey,
                       &dfFalseEasting, 0, 1) )
        dfFalseEasting = 0.0;
        
    if( !GTIFKeyGet(psGTIF, ProjFalseNorthingGeoKey, &dfFalseNorthing,0,1)
        && !GTIFKeyGet(psGTIF, ProjCenterNorthingGeoKey,
                       &dfFalseEasting, 0, 1) )
        dfFalseNorthing = 0.0;
        
    switch( psDefn->CTProjection )
    {
/* -------------------------------------------------------------------- */
      case CT_LambertConfConic_1SP:
      case CT_Mercator:
      case CT_ObliqueStereographic:
      case CT_TransverseMercator:
      case CT_TransvMercator_SouthOriented:
/* -------------------------------------------------------------------- */
        if( GTIFKeyGet(psGTIF, ProjNatOriginLongGeoKey, 
                       &dfNatOriginLong, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjFalseOriginLongGeoKey, 
                          &dfNatOriginLong, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjCenterLongGeoKey, 
                          &dfNatOriginLong, 0, 1 ) == 0 )
            dfNatOriginLong = 0.0;

        if( GTIFKeyGet(psGTIF, ProjNatOriginLatGeoKey, 
                       &dfNatOriginLat, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjFalseOriginLatGeoKey, 
                          &dfNatOriginLat, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjCenterLatGeoKey, 
                          &dfNatOriginLat, 0, 1 ) == 0 )
            dfNatOriginLat = 0.0;

        if( GTIFKeyGet(psGTIF, ProjScaleAtNatOriginGeoKey,
                       &dfNatOriginScale, 0, 1 ) == 0 )
            dfNatOriginScale = 1.0;
            
        /* notdef: should transform to decimal degrees at this point */

        psDefn->ProjParm[0] = dfNatOriginLat;
        psDefn->ProjParmId[0] = ProjNatOriginLatGeoKey;
        psDefn->ProjParm[1] = dfNatOriginLong;
        psDefn->ProjParmId[1] = ProjNatOriginLongGeoKey;
        psDefn->ProjParm[4] = dfNatOriginScale;
        psDefn->ProjParmId[4] = ProjScaleAtNatOriginGeoKey;
        psDefn->ProjParm[5] = dfFalseEasting;
        psDefn->ProjParmId[5] = ProjFalseEastingGeoKey;
        psDefn->ProjParm[6] = dfFalseNorthing;
        psDefn->ProjParmId[6] = ProjFalseNorthingGeoKey;

        psDefn->nParms = 7;
        break;

/* -------------------------------------------------------------------- */
      case CT_ObliqueMercator: /* hotine */
/* -------------------------------------------------------------------- */
        if( GTIFKeyGet(psGTIF, ProjNatOriginLongGeoKey, 
                       &dfNatOriginLong, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjFalseOriginLongGeoKey, 
                          &dfNatOriginLong, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjCenterLongGeoKey, 
                          &dfNatOriginLong, 0, 1 ) == 0 )
            dfNatOriginLong = 0.0;

        if( GTIFKeyGet(psGTIF, ProjNatOriginLatGeoKey, 
                       &dfNatOriginLat, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjFalseOriginLatGeoKey, 
                          &dfNatOriginLat, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjCenterLatGeoKey, 
                          &dfNatOriginLat, 0, 1 ) == 0 )
            dfNatOriginLat = 0.0;

        if( GTIFKeyGet(psGTIF, ProjAzimuthAngleGeoKey, 
                       &dfAzimuth, 0, 1 ) == 0 )
            dfAzimuth = 0.0;

        if( GTIFKeyGet(psGTIF, ProjScaleAtNatOriginGeoKey,
                       &dfNatOriginScale, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjScaleAtCenterGeoKey,
                          &dfNatOriginScale, 0, 1 ) == 0 )
            dfNatOriginScale = 1.0;
            
        /* notdef: should transform to decimal degrees at this point */

        psDefn->ProjParm[0] = dfNatOriginLat;
        psDefn->ProjParmId[0] = ProjCenterLatGeoKey;
        psDefn->ProjParm[1] = dfNatOriginLong;
        psDefn->ProjParmId[1] = ProjCenterLongGeoKey;
        psDefn->ProjParm[2] = dfAzimuth;
        psDefn->ProjParmId[2] = ProjAzimuthAngleGeoKey;
        psDefn->ProjParm[4] = dfNatOriginScale;
        psDefn->ProjParmId[4] = ProjScaleAtCenterGeoKey;
        psDefn->ProjParm[5] = dfFalseEasting;
        psDefn->ProjParmId[5] = ProjFalseEastingGeoKey;
        psDefn->ProjParm[6] = dfFalseNorthing;
        psDefn->ProjParmId[6] = ProjFalseNorthingGeoKey;

        psDefn->nParms = 7;
        break;

/* -------------------------------------------------------------------- */
      case CT_CassiniSoldner:
      case CT_Polyconic:
/* -------------------------------------------------------------------- */
        if( GTIFKeyGet(psGTIF, ProjNatOriginLongGeoKey, 
                       &dfNatOriginLong, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjFalseOriginLongGeoKey, 
                          &dfNatOriginLong, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjCenterLongGeoKey, 
                          &dfNatOriginLong, 0, 1 ) == 0 )
            dfNatOriginLong = 0.0;

        if( GTIFKeyGet(psGTIF, ProjNatOriginLatGeoKey, 
                       &dfNatOriginLat, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjFalseOriginLatGeoKey, 
                          &dfNatOriginLat, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjCenterLatGeoKey, 
                          &dfNatOriginLat, 0, 1 ) == 0 )
            dfNatOriginLat = 0.0;

        /* notdef: should transform to decimal degrees at this point */

        psDefn->ProjParm[0] = dfNatOriginLat;
        psDefn->ProjParmId[0] = ProjNatOriginLatGeoKey;
        psDefn->ProjParm[1] = dfNatOriginLong;
        psDefn->ProjParmId[1] = ProjNatOriginLongGeoKey;
        psDefn->ProjParm[5] = dfFalseEasting;
        psDefn->ProjParmId[5] = ProjFalseEastingGeoKey;
        psDefn->ProjParm[6] = dfFalseNorthing;
        psDefn->ProjParmId[6] = ProjFalseNorthingGeoKey;

        psDefn->nParms = 7;
        break;

/* -------------------------------------------------------------------- */
      case CT_Stereographic:
      case CT_AzimuthalEquidistant:
      case CT_MillerCylindrical:
      case CT_Equirectangular:
      case CT_Gnomonic:
      case CT_LambertAzimEqualArea:
      case CT_Orthographic:
/* -------------------------------------------------------------------- */
        if( GTIFKeyGet(psGTIF, ProjNatOriginLongGeoKey, 
                       &dfNatOriginLong, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjFalseOriginLongGeoKey, 
                          &dfNatOriginLong, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjCenterLongGeoKey, 
                          &dfNatOriginLong, 0, 1 ) == 0 )
            dfNatOriginLong = 0.0;

        if( GTIFKeyGet(psGTIF, ProjNatOriginLatGeoKey, 
                       &dfNatOriginLat, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjFalseOriginLatGeoKey, 
                          &dfNatOriginLat, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjCenterLatGeoKey, 
                          &dfNatOriginLat, 0, 1 ) == 0 )
            dfNatOriginLat = 0.0;

        /* notdef: should transform to decimal degrees at this point */

        psDefn->ProjParm[0] = dfNatOriginLat;
        psDefn->ProjParmId[0] = ProjCenterLatGeoKey;
        psDefn->ProjParm[1] = dfNatOriginLong;
        psDefn->ProjParmId[1] = ProjCenterLongGeoKey;
        psDefn->ProjParm[5] = dfFalseEasting;
        psDefn->ProjParmId[5] = ProjFalseEastingGeoKey;
        psDefn->ProjParm[6] = dfFalseNorthing;
        psDefn->ProjParmId[6] = ProjFalseNorthingGeoKey;

        psDefn->nParms = 7;
        break;

/* -------------------------------------------------------------------- */
      case CT_Robinson:
      case CT_Sinusoidal:
      case CT_VanDerGrinten:
/* -------------------------------------------------------------------- */
        if( GTIFKeyGet(psGTIF, ProjNatOriginLongGeoKey, 
                       &dfNatOriginLong, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjFalseOriginLongGeoKey, 
                          &dfNatOriginLong, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjCenterLongGeoKey, 
                          &dfNatOriginLong, 0, 1 ) == 0 )
            dfNatOriginLong = 0.0;

        /* notdef: should transform to decimal degrees at this point */

        psDefn->ProjParm[1] = dfNatOriginLong;
        psDefn->ProjParmId[1] = ProjCenterLongGeoKey;
        psDefn->ProjParm[5] = dfFalseEasting;
        psDefn->ProjParmId[5] = ProjFalseEastingGeoKey;
        psDefn->ProjParm[6] = dfFalseNorthing;
        psDefn->ProjParmId[6] = ProjFalseNorthingGeoKey;

        psDefn->nParms = 7;
        break;

/* -------------------------------------------------------------------- */
      case CT_PolarStereographic:
/* -------------------------------------------------------------------- */
        if( GTIFKeyGet(psGTIF, ProjStraightVertPoleLongGeoKey, 
                       &dfNatOriginLong, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjNatOriginLongGeoKey, 
                          &dfNatOriginLong, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjFalseOriginLongGeoKey, 
                          &dfNatOriginLong, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjCenterLongGeoKey, 
                          &dfNatOriginLong, 0, 1 ) == 0 )
            dfNatOriginLong = 0.0;

        if( GTIFKeyGet(psGTIF, ProjNatOriginLatGeoKey, 
                       &dfNatOriginLat, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjFalseOriginLatGeoKey, 
                          &dfNatOriginLat, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjCenterLatGeoKey, 
                          &dfNatOriginLat, 0, 1 ) == 0 )
            dfNatOriginLat = 0.0;

        if( GTIFKeyGet(psGTIF, ProjScaleAtNatOriginGeoKey,
                       &dfNatOriginScale, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjScaleAtCenterGeoKey,
                          &dfNatOriginScale, 0, 1 ) == 0 )
            dfNatOriginScale = 1.0;
            
        /* notdef: should transform to decimal degrees at this point */

        psDefn->ProjParm[0] = dfNatOriginLat;
        psDefn->ProjParmId[0] = ProjNatOriginLatGeoKey;;
        psDefn->ProjParm[1] = dfNatOriginLong;
        psDefn->ProjParmId[1] = ProjStraightVertPoleLongGeoKey;
        psDefn->ProjParm[4] = dfNatOriginScale;
        psDefn->ProjParmId[4] = ProjScaleAtCenterGeoKey;
        psDefn->ProjParm[5] = dfFalseEasting;
        psDefn->ProjParmId[5] = ProjFalseEastingGeoKey;
        psDefn->ProjParm[6] = dfFalseNorthing;
        psDefn->ProjParmId[6] = ProjFalseNorthingGeoKey;

        psDefn->nParms = 7;
        break;

/* -------------------------------------------------------------------- */
      case CT_LambertConfConic_2SP:
/* -------------------------------------------------------------------- */
        if( GTIFKeyGet(psGTIF, ProjStdParallel1GeoKey, 
                       &dfStdParallel1, 0, 1 ) == 0 )
            dfStdParallel1 = 0.0;

        if( GTIFKeyGet(psGTIF, ProjStdParallel2GeoKey, 
                       &dfStdParallel2, 0, 1 ) == 0 )
            dfStdParallel1 = 0.0;

        if( GTIFKeyGet(psGTIF, ProjNatOriginLongGeoKey, 
                       &dfNatOriginLong, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjFalseOriginLongGeoKey, 
                          &dfNatOriginLong, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjCenterLongGeoKey, 
                          &dfNatOriginLong, 0, 1 ) == 0 )
            dfNatOriginLong = 0.0;

        if( GTIFKeyGet(psGTIF, ProjNatOriginLatGeoKey, 
                       &dfNatOriginLat, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjFalseOriginLatGeoKey, 
                          &dfNatOriginLat, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjCenterLatGeoKey, 
                          &dfNatOriginLat, 0, 1 ) == 0 )
            dfNatOriginLat = 0.0;

        /* notdef: should transform to decimal degrees at this point */

        psDefn->ProjParm[0] = dfStdParallel1;
        psDefn->ProjParmId[0] = ProjStdParallel1GeoKey;
        psDefn->ProjParm[1] = dfStdParallel2;
        psDefn->ProjParmId[1] = ProjStdParallel1GeoKey;
        psDefn->ProjParm[2] = dfNatOriginLat;
        psDefn->ProjParmId[2] = ProjFalseOriginLatGeoKey;
        psDefn->ProjParm[3] = dfNatOriginLong;
        psDefn->ProjParmId[3] = ProjFalseOriginLongGeoKey;
        psDefn->ProjParm[5] = dfFalseEasting;
        psDefn->ProjParmId[5] = ProjFalseEastingGeoKey;
        psDefn->ProjParm[6] = dfFalseNorthing;
        psDefn->ProjParmId[6] = ProjFalseNorthingGeoKey;

        psDefn->nParms = 7;
        break;

/* -------------------------------------------------------------------- */
      case CT_AlbersEqualArea:
/* -------------------------------------------------------------------- */
        if( GTIFKeyGet(psGTIF, ProjStdParallel1GeoKey, 
                       &dfStdParallel1, 0, 1 ) == 0 )
            dfStdParallel1 = 0.0;

        if( GTIFKeyGet(psGTIF, ProjStdParallel2GeoKey, 
                       &dfStdParallel2, 0, 1 ) == 0 )
            dfStdParallel1 = 0.0;

        if( GTIFKeyGet(psGTIF, ProjNatOriginLongGeoKey, 
                       &dfNatOriginLong, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjFalseOriginLongGeoKey, 
                          &dfNatOriginLong, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjCenterLongGeoKey, 
                          &dfNatOriginLong, 0, 1 ) == 0 )
            dfNatOriginLong = 0.0;

        if( GTIFKeyGet(psGTIF, ProjNatOriginLatGeoKey, 
                       &dfNatOriginLat, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjFalseOriginLatGeoKey, 
                          &dfNatOriginLat, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjCenterLatGeoKey, 
                          &dfNatOriginLat, 0, 1 ) == 0 )
            dfNatOriginLat = 0.0;

        /* notdef: should transform to decimal degrees at this point */

        psDefn->ProjParm[0] = dfStdParallel1;
        psDefn->ProjParmId[0] = ProjStdParallel1GeoKey;
        psDefn->ProjParm[1] = dfStdParallel2;
        psDefn->ProjParmId[1] = ProjStdParallel1GeoKey;
        psDefn->ProjParm[2] = dfNatOriginLat;
        psDefn->ProjParmId[2] = ProjNatOriginLatGeoKey;
        psDefn->ProjParm[3] = dfNatOriginLong;
        psDefn->ProjParmId[3] = ProjNatOriginLongGeoKey;
        psDefn->ProjParm[5] = dfFalseEasting;
        psDefn->ProjParmId[5] = ProjFalseEastingGeoKey;
        psDefn->ProjParm[6] = dfFalseNorthing;
        psDefn->ProjParmId[6] = ProjFalseNorthingGeoKey;

        psDefn->nParms = 7;
        break;
    }
}

/************************************************************************/
/*                            GTIFGetDefn()                             */
/*                                                                      */
/*      Try and read, and build a fully normalized set of               */
/*      information about the files projection.                         */
/************************************************************************/

int GTIFGetDefn( GTIF * psGTIF, GTIFDefn * psDefn )

{
    int		i, nGeogUOMAngle, nGeogUOMLinear;
    
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

    psDefn->ProjCode = KvUserDefined;
    psDefn->Projection = KvUserDefined;
    psDefn->CTProjection = KvUserDefined;

    psDefn->nParms = 0;
    for( i = 0; i < MAX_GTIF_PROJPARMS; i++ )
    {
        psDefn->ProjParm[i] = 0.0;
        psDefn->ProjParmId[i] = 0;
    }

/* -------------------------------------------------------------------- */
/*	Try to get the overall model type.				*/
/* -------------------------------------------------------------------- */
    GTIFKeyGet(psGTIF,ProjectedCSTypeGeoKey,&(psDefn->Model),0,1);

/* -------------------------------------------------------------------- */
/*	Extract the Geog units.  					*/
/* -------------------------------------------------------------------- */
    nGeogUOMAngle = 9102; /* Angluar_Degrees */
    GTIFKeyGet(psGTIF, GeogAngularUnitsGeoKey, &nGeogUOMAngle, 0, 1 );

    nGeogUOMLinear = 9001; /* Linear_Meter */
    GTIFKeyGet(psGTIF, GeogLinearUnitsGeoKey, &nGeogUOMLinear, 0, 1 );

/* -------------------------------------------------------------------- */
/*      Try to get a PCS.                                               */
/* -------------------------------------------------------------------- */
    if( GTIFKeyGet(psGTIF,ProjectedCSTypeGeoKey, &(psDefn->PCS),0,1) == 1
        && psDefn->PCS != KvUserDefined )
    {
        int		nUOMAngle;
        
        /*
         * Translate this into useful information.
         */
        GTIFGetPCSInfo( psDefn->PCS, NULL,
                            &(psDefn->UOMLength), &(nUOMAngle),
                            &(psDefn->GCS), &(psDefn->ProjCode) );
    }

/* -------------------------------------------------------------------- */
/*      If the Proj_ code is specified directly, use that.              */
/* -------------------------------------------------------------------- */
    if( psDefn->ProjCode == KvUserDefined )
        GTIFKeyGet(psGTIF, ProjectionGeoKey, &(psDefn->ProjCode), 0, 1 );
    
    if( psDefn->ProjCode != KvUserDefined )
    {
        /*
         * We have an underlying projection transformation value.  Look
         * this up.  For a PCS of ``WGS 84 / UTM 11'' the transformation
         * would be Transverse Mercator, with a particular set of options.
         * The nProjTRFCode itself would correspond to the name
         * ``UTM zone 11N'', and doesn't include datum info.
         */
        GTIFGetProjTRFInfo( psDefn->ProjCode,
                            &(psDefn->Projection),
                            psDefn->ProjParm );
        psDefn->CTProjection =
            EPSGProjMethodToCTProjMethod( psDefn->Projection );
        
        /*
         * Set the GeoTIFF identity of the parameters.
         */
        SetGTParmIds( psDefn );
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
    else
    {
        GTIFKeyGet(psGTIF, GeogPrimeMeridianLongGeoKey,
                   &(psDefn->PMLongToGreenwich), 0, 1 );

        psDefn->PMLongToGreenwich =
            GTIFAngleToDD( psDefn->PMLongToGreenwich, nGeogUOMAngle );
    }

/* -------------------------------------------------------------------- */
/*      Have the projection units of measure been overridden?  We       */
/*      should likely be doing something about angular units too,       */
/*      but these are very rarely not decimal degrees for actual        */
/*      file coordinates.                                               */
/* -------------------------------------------------------------------- */
    GTIFKeyGet(psGTIF,ProjLinearUnitsGeoKey,&(psDefn->UOMLength),0,1);

    if( psDefn->UOMLength != KvUserDefined )
    {
        GTIFGetUOMLengthInfo( psDefn->UOMLength, NULL,
                              &(psDefn->UOMLengthInMeters) );
    }

/* -------------------------------------------------------------------- */
/*      Handle a variety of user defined transform types.               */
/* -------------------------------------------------------------------- */
    if( GTIFKeyGet(psGTIF,ProjCoordTransGeoKey,
                   &(psDefn->CTProjection),0,1) == 1)
    {
        GTIFFetchProjParms( psGTIF, psDefn );
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
/*	Dump the projection code if possible.				*/
/* -------------------------------------------------------------------- */
    if( psDefn->ProjCode != KvUserDefined )
    {
        fprintf( fp, "Projection = %d (%s)\n",
                 psDefn->ProjCode,
                 GTIFValueName(ProjectionGeoKey, psDefn->ProjCode) );

    }

/* -------------------------------------------------------------------- */
/*      Try to dump the projection method name, and parameters if possible.*/
/* -------------------------------------------------------------------- */
    if( psDefn->CTProjection != KvUserDefined )
    {
        char	*pszName = GTIFValueName(ProjCoordTransGeoKey,
                                         psDefn->CTProjection);
        int     i;

        if( pszName == NULL )
            pszName = "(unknown)";
            
        printf( "Projection Method: %s\n", pszName );
                
        for( i = 0; i < psDefn->nParms; i++ )
        {
            if( psDefn->ProjParmId[i] == 0 )
                continue;

            pszName = GTIFKeyName(psDefn->ProjParmId[i]);
            if( pszName == NULL )
                pszName = "(unknown)";
            
            printf( "   %s: %f\n", pszName, psDefn->ProjParm[i] );
        }
    }

/* -------------------------------------------------------------------- */
/*      Report the GCS name, and number.                                */
/* -------------------------------------------------------------------- */
    if( psDefn->GCS != KvUserDefined )
    {
        char	*pszName = "(unknown)";

        GTIFGetGCSInfo( psDefn->GCS, &pszName, NULL, NULL );
        printf( "GCS: %d/%s\n", psDefn->GCS, pszName );
    }

/* -------------------------------------------------------------------- */
/*      Report the datum name.                                          */
/* -------------------------------------------------------------------- */
    if( psDefn->Datum != KvUserDefined )
    {
        char	*pszName = "(unknown)";

        GTIFGetDatumInfo( psDefn->Datum, &pszName, NULL );
        printf( "Datum: %d/%s\n", psDefn->Datum, pszName );
    }

/* -------------------------------------------------------------------- */
/*      Report the ellipsoid.                                           */
/* -------------------------------------------------------------------- */
    if( psDefn->Ellipsoid != KvUserDefined )
    {
        char	*pszName = "(unknown)";

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

