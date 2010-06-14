/******************************************************************************
 * $Id$
 *
 * Project:  Image Warper
 * Purpose:  Implements a rational polynomail (RPC) based transformer. 
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
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

#include "gdal_priv.h"
#include "gdal_alg.h"
#include "ogr_spatialref.h"
#include "cpl_minixml.h"

CPL_CVSID("$Id$");

CPL_C_START
CPLXMLNode *GDALSerializeRPCTransformer( void *pTransformArg );
void *GDALDeserializeRPCTransformer( CPLXMLNode *psTree );
CPL_C_END

/************************************************************************/
/*                            RPCInfoToMD()                             */
/*                                                                      */
/*      Turn an RPCInfo structure back into it's metadata format.       */
/************************************************************************/

static char ** RPCInfoToMD( GDALRPCInfo *psRPCInfo )

{
    char **papszMD = NULL;
    CPLString osField, osMultiField;
    int i;

    osField.Printf( "%.15g", psRPCInfo->dfLINE_OFF );
    papszMD = CSLSetNameValue( papszMD, "LINE_OFF", osField );

    osField.Printf( "%.15g", psRPCInfo->dfSAMP_OFF );
    papszMD = CSLSetNameValue( papszMD, "SAMP_OFF", osField );

    osField.Printf( "%.15g", psRPCInfo->dfLAT_OFF );
    papszMD = CSLSetNameValue( papszMD, "LAT_OFF", osField );

    osField.Printf( "%.15g", psRPCInfo->dfLONG_OFF );
    papszMD = CSLSetNameValue( papszMD, "LONG_OFF", osField );

    osField.Printf( "%.15g", psRPCInfo->dfHEIGHT_OFF );
    papszMD = CSLSetNameValue( papszMD, "HEIGHT_OFF", osField );

    osField.Printf( "%.15g", psRPCInfo->dfLINE_SCALE );
    papszMD = CSLSetNameValue( papszMD, "LINE_SCALE", osField );

    osField.Printf( "%.15g", psRPCInfo->dfSAMP_SCALE );
    papszMD = CSLSetNameValue( papszMD, "SAMP_SCALE", osField );

    osField.Printf( "%.15g", psRPCInfo->dfLAT_SCALE );
    papszMD = CSLSetNameValue( papszMD, "LAT_SCALE", osField );

    osField.Printf( "%.15g", psRPCInfo->dfLONG_SCALE );
    papszMD = CSLSetNameValue( papszMD, "LONG_SCALE", osField );

    osField.Printf( "%.15g", psRPCInfo->dfHEIGHT_SCALE );
    papszMD = CSLSetNameValue( papszMD, "HEIGHT_SCALE", osField );

    osField.Printf( "%.15g", psRPCInfo->dfMIN_LONG );
    papszMD = CSLSetNameValue( papszMD, "MIN_LONG", osField );

    osField.Printf( "%.15g", psRPCInfo->dfMIN_LAT );
    papszMD = CSLSetNameValue( papszMD, "MIN_LAT", osField );

    osField.Printf( "%.15g", psRPCInfo->dfMAX_LONG );
    papszMD = CSLSetNameValue( papszMD, "MAX_LONG", osField );

    osField.Printf( "%.15g", psRPCInfo->dfMAX_LAT );
    papszMD = CSLSetNameValue( papszMD, "MAX_LAT", osField );

    for( i = 0; i < 20; i++ )
    {
        osField.Printf( "%.15g", psRPCInfo->adfLINE_NUM_COEFF[i] );
        if( i > 0 )
            osMultiField += " ";
        else
            osMultiField = "";
        osMultiField += osField;
    }
    papszMD = CSLSetNameValue( papszMD, "LINE_NUM_COEFF", osMultiField );

    for( i = 0; i < 20; i++ )
    {
        osField.Printf( "%.15g", psRPCInfo->adfLINE_DEN_COEFF[i] );
        if( i > 0 )
            osMultiField += " ";
        else
            osMultiField = "";
        osMultiField += osField;
    }
    papszMD = CSLSetNameValue( papszMD, "LINE_DEN_COEFF", osMultiField );

    for( i = 0; i < 20; i++ )
    {
        osField.Printf( "%.15g", psRPCInfo->adfSAMP_NUM_COEFF[i] );
        if( i > 0 )
            osMultiField += " ";
        else
            osMultiField = "";
        osMultiField += osField;
    }
    papszMD = CSLSetNameValue( papszMD, "SAMP_NUM_COEFF", osMultiField );

    for( i = 0; i < 20; i++ )
    {
        osField.Printf( "%.15g", psRPCInfo->adfSAMP_DEN_COEFF[i] );
        if( i > 0 )
            osMultiField += " ";
        else
            osMultiField = "";
        osMultiField += osField;
    }
    papszMD = CSLSetNameValue( papszMD, "SAMP_DEN_COEFF", osMultiField );

    return papszMD;
}

/************************************************************************/
/*                          RPCComputeTerms()                           */
/************************************************************************/

static void RPCComputeTerms( double dfLong, double dfLat, double dfHeight,
                             double *padfTerms )

{
    padfTerms[0] = 1.0;
    padfTerms[1] = dfLong;
    padfTerms[2] = dfLat;
    padfTerms[3] = dfHeight;
    padfTerms[4] = dfLong * dfLat;
    padfTerms[5] = dfLong * dfHeight;
    padfTerms[6] = dfLat * dfHeight;
    padfTerms[7] = dfLong * dfLong;
    padfTerms[8] = dfLat * dfLat;
    padfTerms[9] = dfHeight * dfHeight;

    padfTerms[10] = dfLong * dfLat * dfHeight;
    padfTerms[11] = dfLong * dfLong * dfLong;
    padfTerms[12] = dfLong * dfLat * dfLat;
    padfTerms[13] = dfLong * dfHeight * dfHeight;
    padfTerms[14] = dfLong * dfLong * dfLat;
    padfTerms[15] = dfLat * dfLat * dfLat;
    padfTerms[16] = dfLat * dfHeight * dfHeight;
    padfTerms[17] = dfLong * dfLong * dfHeight;
    padfTerms[18] = dfLat * dfLat * dfHeight;
    padfTerms[19] = dfHeight * dfHeight * dfHeight;
}

/************************************************************************/
/*                            RPCEvaluate()                             */
/************************************************************************/

static double RPCEvaluate( double *padfTerms, double *padfCoefs )

{
    double dfSum = 0.0;
    int i;

    for( i = 0; i < 20; i++ )
        dfSum += padfTerms[i] * padfCoefs[i];

    return dfSum;
}

/************************************************************************/
/*                         RPCTransformPoint()                          */
/************************************************************************/

static void RPCTransformPoint( GDALRPCInfo *psRPC, 
                               double dfLong, double dfLat, double dfHeight, 
                               double *pdfPixel, double *pdfLine )

{
    double dfResultX, dfResultY;
    double adfTerms[20];
   
    RPCComputeTerms( 
        (dfLong   - psRPC->dfLONG_OFF) / psRPC->dfLONG_SCALE, 
        (dfLat    - psRPC->dfLAT_OFF) / psRPC->dfLAT_SCALE, 
        (dfHeight - psRPC->dfHEIGHT_OFF) / psRPC->dfHEIGHT_SCALE,
        adfTerms );
    
    dfResultX = RPCEvaluate( adfTerms, psRPC->adfSAMP_NUM_COEFF )
        / RPCEvaluate( adfTerms, psRPC->adfSAMP_DEN_COEFF );
    
    dfResultY = RPCEvaluate( adfTerms, psRPC->adfLINE_NUM_COEFF )
        / RPCEvaluate( adfTerms, psRPC->adfLINE_DEN_COEFF );
    
    *pdfPixel = dfResultX * psRPC->dfSAMP_SCALE + psRPC->dfSAMP_OFF;
    *pdfLine = dfResultY * psRPC->dfLINE_SCALE + psRPC->dfLINE_OFF;
}

/************************************************************************/
/* ==================================================================== */
/*			     GDALRPCTransformer                         */
/* ==================================================================== */
/************************************************************************/

typedef struct {

    GDALTransformerInfo sTI;

    GDALRPCInfo sRPC;

    double      adfPLToLatLongGeoTransform[6];

    int         bReversed;

    double      dfPixErrThreshold;

    double      dfHeightOffset;

    double      dfHeightScale;

    char        *pszDEMPath;

    int         bHasTriedOpeningDS;
    GDALDataset *poDS;

    OGRCoordinateTransformation *poCT;

    double      adfGeoTransform[6];
    double      adfReverseGeoTransform[6];
} GDALRPCTransformInfo;

/************************************************************************/
/*                      GDALCreateRPCTransformer()                      */
/************************************************************************/

/**
 * Create an RPC based transformer. 
 *
 * The geometric sensor model describing the physical relationship between 
 * image coordinates and ground coordinate is known as a Rigorous Projection 
 * Model. A Rigorous Projection Model expresses the mapping of the image space 
 * coordinates of rows and columns (r,c) onto the object space reference 
 * surface geodetic coordinates (long, lat, height).
 * 
 * RPC supports a generic description of the Rigorous Projection Models. The 
 * approximation used by GDAL (RPC00) is a set of rational polynomials exp 
 * ressing the normalized row and column values, (rn , cn), as a function of
 *  normalized geodetic latitude, longitude, and height, (P, L, H), given a 
 * set of normalized polynomial coefficients (LINE_NUM_COEF_n, LINE_DEN_COEF_n,
 *  SAMP_NUM_COEF_n, SAMP_DEN_COEF_n). Normalized values, rather than actual 
 * values are used in order to minimize introduction of errors during the 
 * calculations. The transformation between row and column values (r,c), and 
 * normalized row and column values (rn, cn), and between the geodetic 
 * latitude, longitude, and height and normalized geodetic latitude, 
 * longitude, and height (P, L, H), is defined by a set of normalizing 
 * translations (offsets) and scales that ensure all values are contained i 
 * the range -1 to +1.
 *
 * This function creates a GDALTransformFunc compatible transformer 
 * for going between image pixel/line and long/lat/height coordinates 
 * using RPCs.  The RPCs are provided in a GDALRPCInfo structure which is
 * normally read from metadata using GDALExtractRPCInfo().  
 *
 * GDAL RPC Metadata has the following entries (also described in GDAL RFC 22
 * and the GeoTIFF RPC document http://geotiff.maptools.org/rpc_prop.html.  
 *
 * <ul>
 * <li>ERR_BIAS: Error - Bias. The RMS bias error in meters per horizontal axis of all points in the image (-1.0 if unknown)
 * <li>ERR_RAND: Error - Random. RMS random error in meters per horizontal axis of each point in the image (-1.0 if unknown)
 * <li>LINE_OFF: Line Offset
 * <li>SAMP_OFF: Sample Offset
 * <li>LAT_OFF: Geodetic Latitude Offset
 * <li>LONG_OFF: Geodetic Longitude Offset
 * <li>HEIGHT_OFF: Geodetic Height Offset
 * <li>LINE_SCALE: Line Scale
 * <li>SAMP_SCALE: Sample Scale
 * <li>LAT_SCALE: Geodetic Latitude Scale
 * <li>LONG_SCALE: Geodetic Longitude Scale
 * <li>HEIGHT_SCALE: Geodetic Height Scale
 * <li>LINE_NUM_COEFF (1-20): Line Numerator Coefficients. Twenty coefficients for the polynomial in the Numerator of the rn equation. (space separated)
 * <li>LINE_DEN_COEFF (1-20): Line Denominator Coefficients. Twenty coefficients for the polynomial in the Denominator of the rn equation. (space separated)
 * <li>SAMP_NUM_COEFF (1-20): Sample Numerator Coefficients. Twenty coefficients for the polynomial in the Numerator of the cn equation. (space separated)
 * <li>SAMP_DEN_COEFF (1-20): Sample Denominator Coefficients. Twenty coefficients for the polynomial in the Denominator of the cn equation. (space separated)
 * </ul>
 *
 * The transformer normally maps from pixel/line/height to long/lat/height space
 * as a forward transformation though in RPC terms that would be considered
 * an inverse transformation (and is solved by iterative approximation using
 * long/lat/height to pixel/line transformations).  The default direction can
 * be reversed by passing bReversed=TRUE.  
 * 
 * The iterative solution of pixel/line
 * to lat/long/height is currently run for up to 10 iterations or until 
 * the apparent error is less than dfPixErrThreshold pixels.  Passing zero
 * will not avoid all error, but will cause the operation to run for the maximum
 * number of iterations. 
 *
 * Additional options to the transformer can be supplied in papszOptions.
 *
 * Options:
 * 
 * <ul>
 * <li> RPC_HEIGHT: a fixed height offset to be applied to all points passed
 * in.  In this situation the Z passed into the transformation function is
 * assumed to be height above ground, and the RPC_HEIGHT is assumed to be
 * an average height above sea level for ground in the target scene. 
 *
 * <li> RPC_HEIGHT_SCALE: a factor used to multiply heights above ground.
 * Usefull when elevation offsets of the DEM are not expressed in meters. (GDAL >= 1.8.0)
 *
 * <li> RPC_DEM: the name of a GDAL dataset (a DEM file typically) used to
 * extract elevation offsets from. In this situation the Z passed into the
 * transformation function is assumed to be height above ground. This option
 * should be used in replacement of RPC_HEIGHT to provide a way of defining
 * a non uniform ground for the target scene (GDAL >= 1.8.0)
 * </ul>
 *
 * @param psRPCInfo Definition of the RPC parameters.
 *
 * @param bReversed If true "forward" transformation will be lat/long to pixel/line instead of the normal pixel/line to lat/long.
 *
 * @param dfPixErrThreshold the error (measured in pixels) allowed in the 
 * iterative solution of pixel/line to lat/long computations (the other way
 * is always exact given the equations). 
 *
 * @param papszOptions Other transformer options (ie. RPC_HEIGHT=<z>). 
 *
 * @return transformer callback data (deallocate with GDALDestroyTransformer()).
 */

void *GDALCreateRPCTransformer( GDALRPCInfo *psRPCInfo, int bReversed, 
                                double dfPixErrThreshold,
                                char **papszOptions )

{
    GDALRPCTransformInfo *psTransform;

/* -------------------------------------------------------------------- */
/*      Initialize core info.                                           */
/* -------------------------------------------------------------------- */
    psTransform = (GDALRPCTransformInfo *) 
        CPLCalloc(sizeof(GDALRPCTransformInfo),1);

    memcpy( &(psTransform->sRPC), psRPCInfo, sizeof(GDALRPCInfo) );
    psTransform->bReversed = bReversed;
    psTransform->dfPixErrThreshold = dfPixErrThreshold;
    psTransform->dfHeightOffset = 0.0;
    psTransform->dfHeightScale = 1.0;

    strcpy( psTransform->sTI.szSignature, "GTI" );
    psTransform->sTI.pszClassName = "GDALRPCTransformer";
    psTransform->sTI.pfnTransform = GDALRPCTransform;
    psTransform->sTI.pfnCleanup = GDALDestroyRPCTransformer;
    psTransform->sTI.pfnSerialize = GDALSerializeRPCTransformer;

/* -------------------------------------------------------------------- */
/*      Do we have a "average height" that we want to consider all      */
/*      elevations to be relative to?                                   */
/* -------------------------------------------------------------------- */
    const char *pszHeight = CSLFetchNameValue( papszOptions, "RPC_HEIGHT" );
    if( pszHeight != NULL )
        psTransform->dfHeightOffset = CPLAtof(pszHeight);

/* -------------------------------------------------------------------- */
/*                       The "height scale"                             */
/* -------------------------------------------------------------------- */
    const char *pszHeightScale = CSLFetchNameValue( papszOptions, "RPC_HEIGHT_SCALE" );
    if( pszHeightScale != NULL )
        psTransform->dfHeightScale = CPLAtof(pszHeightScale);

/* -------------------------------------------------------------------- */
/*                       The DEM file name                              */
/* -------------------------------------------------------------------- */
    const char *pszDEMPath = CSLFetchNameValue( papszOptions, "RPC_DEM" );
    if( pszDEMPath != NULL )
        psTransform->pszDEMPath = CPLStrdup(pszDEMPath);
        
/* -------------------------------------------------------------------- */
/*      Establish a reference point for calcualating an affine          */
/*      geotransform approximate transformation.                        */
/* -------------------------------------------------------------------- */
    double adfGTFromLL[6], dfRefPixel = -1.0, dfRefLine = -1.0;
    double dfRefLong = 0.0, dfRefLat = 0.0;

    if( psRPCInfo->dfMIN_LONG != -180 || psRPCInfo->dfMAX_LONG != 180 )
    {
        dfRefLong = (psRPCInfo->dfMIN_LONG + psRPCInfo->dfMAX_LONG) * 0.5;
        dfRefLat  = (psRPCInfo->dfMIN_LAT  + psRPCInfo->dfMAX_LAT ) * 0.5;

        RPCTransformPoint( psRPCInfo, dfRefLong, dfRefLat, 0.0, 
                           &dfRefPixel, &dfRefLine );
    }

    // Try with scale and offset if we don't can't use bounds or
    // the results seem daft. 
    if( dfRefPixel < 0.0 || dfRefLine < 0.0
        || dfRefPixel > 100000 || dfRefLine > 100000 )
    {
        dfRefLong = psRPCInfo->dfLONG_OFF;
        dfRefLat  = psRPCInfo->dfLAT_OFF;

        RPCTransformPoint( psRPCInfo, dfRefLong, dfRefLat, 0.0, 
                           &dfRefPixel, &dfRefLine );
    }

/* -------------------------------------------------------------------- */
/*      Transform nearby locations to establish affine direction        */
/*      vectors.                                                        */
/* -------------------------------------------------------------------- */
    double dfRefPixelDelta, dfRefLineDelta, dfLLDelta = 0.0001;
    
    RPCTransformPoint( psRPCInfo, dfRefLong+dfLLDelta, dfRefLat, 0.0, 
                       &dfRefPixelDelta, &dfRefLineDelta );
    adfGTFromLL[1] = (dfRefPixelDelta - dfRefPixel) / dfLLDelta;
    adfGTFromLL[2] = (dfRefLineDelta - dfRefLine) / dfLLDelta;
    
    RPCTransformPoint( psRPCInfo, dfRefLong, dfRefLat+dfLLDelta, 0.0, 
                       &dfRefPixelDelta, &dfRefLineDelta );
    adfGTFromLL[4] = (dfRefPixelDelta - dfRefPixel) / dfLLDelta;
    adfGTFromLL[5] = (dfRefLineDelta - dfRefLine) / dfLLDelta;

    adfGTFromLL[0] = dfRefPixel 
        - adfGTFromLL[1] * dfRefLong - adfGTFromLL[2] * dfRefLat;
    adfGTFromLL[3] = dfRefLine 
        - adfGTFromLL[4] * dfRefLong - adfGTFromLL[5] * dfRefLat;

    GDALInvGeoTransform( adfGTFromLL, psTransform->adfPLToLatLongGeoTransform);
    
    return psTransform;
}

/************************************************************************/
/*                 GDALDestroyReprojectionTransformer()                 */
/************************************************************************/

void GDALDestroyRPCTransformer( void *pTransformAlg )

{
    GDALRPCTransformInfo *psTransform = (GDALRPCTransformInfo *) pTransformAlg;

    CPLFree( psTransform->pszDEMPath );

    if(psTransform->poDS)
        GDALClose(psTransform->poDS);
    if(psTransform->poCT)
        OCTDestroyCoordinateTransformation((OGRCoordinateTransformationH)psTransform->poCT);

    CPLFree( pTransformAlg );
}

/************************************************************************/
/*                      RPCInverseTransformPoint()                      */
/************************************************************************/

static void 
RPCInverseTransformPoint( GDALRPCTransformInfo *psTransform,
                          double dfPixel, double dfLine, double dfHeight, 
                          double *pdfLong, double *pdfLat )

{
    double dfResultX, dfResultY;
    int    iIter;
    GDALRPCInfo *psRPC = &(psTransform->sRPC);

/* -------------------------------------------------------------------- */
/*      Compute an initial approximation based on linear                */
/*      interpolation from our reference point.                         */
/* -------------------------------------------------------------------- */
    dfResultX = psTransform->adfPLToLatLongGeoTransform[0]
        + psTransform->adfPLToLatLongGeoTransform[1] * dfPixel
        + psTransform->adfPLToLatLongGeoTransform[2] * dfLine;

    dfResultY = psTransform->adfPLToLatLongGeoTransform[3]
        + psTransform->adfPLToLatLongGeoTransform[4] * dfPixel
        + psTransform->adfPLToLatLongGeoTransform[5] * dfLine;

/* -------------------------------------------------------------------- */
/*      Now iterate, trying to find a closer LL location that will      */
/*      back transform to the indicated pixel and line.                 */
/* -------------------------------------------------------------------- */
    double dfPixelDeltaX, dfPixelDeltaY;

    for( iIter = 0; iIter < 10; iIter++ )
    {
        double dfBackPixel, dfBackLine;

        RPCTransformPoint( psRPC, dfResultX, dfResultY, dfHeight, 
                           &dfBackPixel, &dfBackLine );

        dfPixelDeltaX = dfBackPixel - dfPixel;
        dfPixelDeltaY = dfBackLine - dfLine;

        dfResultX = dfResultX 
            - dfPixelDeltaX * psTransform->adfPLToLatLongGeoTransform[1]
            - dfPixelDeltaY * psTransform->adfPLToLatLongGeoTransform[2];
        dfResultY = dfResultY 
            - dfPixelDeltaX * psTransform->adfPLToLatLongGeoTransform[4]
            - dfPixelDeltaY * psTransform->adfPLToLatLongGeoTransform[5];

        if( ABS(dfPixelDeltaX) < psTransform->dfPixErrThreshold
            && ABS(dfPixelDeltaY) < psTransform->dfPixErrThreshold )
        {
            iIter = -1;
            //CPLDebug( "RPC", "Converged!" );
            break;
        }

    }

    if( iIter != -1 )
        CPLDebug( "RPC", "Iterations %d: Got: %g,%g  Offset=%g,%g", 
                  iIter, 
                  dfResultX, dfResultY,
                  dfPixelDeltaX, dfPixelDeltaY );
    
    *pdfLong = dfResultX;
    *pdfLat = dfResultY;
}

/************************************************************************/
/*                          GDALRPCTransform()                          */
/************************************************************************/

int GDALRPCTransform( void *pTransformArg, int bDstToSrc, 
                      int nPointCount, 
                      double *padfX, double *padfY, double *padfZ,
                      int *panSuccess )

{
    VALIDATE_POINTER1( pTransformArg, "GDALRPCTransform", 0 );

    GDALRPCTransformInfo *psTransform = (GDALRPCTransformInfo *) pTransformArg;
    GDALRPCInfo *psRPC = &(psTransform->sRPC);
    int i;

    if( psTransform->bReversed )
        bDstToSrc = !bDstToSrc;

    int bands[1] = {1};
    int nRasterXSize = 0, nRasterYSize = 0;

/* -------------------------------------------------------------------- */
/*      Lazy opening of the optionnal DEM file.                         */
/* -------------------------------------------------------------------- */
    if(psTransform->pszDEMPath != NULL &&
       psTransform->bHasTriedOpeningDS == FALSE)
    {
        int bIsValid = FALSE;
        psTransform->bHasTriedOpeningDS = TRUE;
        psTransform->poDS = (GDALDataset *)
                                GDALOpen( psTransform->pszDEMPath, GA_ReadOnly );
        if(psTransform->poDS != NULL && psTransform->poDS->GetRasterCount() >= 1)
        {
            const char* pszSpatialRef = psTransform->poDS->GetProjectionRef();
            if (pszSpatialRef != NULL && pszSpatialRef[0] != '\0')
            {
                OGRSpatialReference* poWGSSpaRef =
                        new OGRSpatialReference(SRS_WKT_WGS84);
                OGRSpatialReference* poDSSpaRef =
                        new OGRSpatialReference(pszSpatialRef);
                if(!poWGSSpaRef->IsSame(poDSSpaRef))
                    psTransform->poCT =OGRCreateCoordinateTransformation(
                                                    poWGSSpaRef, poDSSpaRef );
                delete poWGSSpaRef;
                delete poDSSpaRef;
            }

            if (psTransform->poDS->GetGeoTransform(
                                psTransform->adfGeoTransform) == CE_None &&
                GDALInvGeoTransform( psTransform->adfGeoTransform,
                                     psTransform->adfReverseGeoTransform ))
            {
                bIsValid = TRUE;
            }
        }

        if (!bIsValid && psTransform->poDS != NULL)
        {
            GDALClose(psTransform->poDS);
            psTransform->poDS = NULL;
        }
    }
    if (psTransform->poDS)
    {
        nRasterXSize = psTransform->poDS->GetRasterXSize();
        nRasterYSize = psTransform->poDS->GetRasterYSize();
    }

/* -------------------------------------------------------------------- */
/*      The simple case is transforming from lat/long to pixel/line.    */
/*      Just apply the equations directly.                              */
/* -------------------------------------------------------------------- */
    if( bDstToSrc )
    {
        for( i = 0; i < nPointCount; i++ )
        {
            if(psTransform->poDS)
            {
                double dfX, dfY;
                //check if dem is not in WGS84 and transform points padfX[i], padfY[i]
                if(psTransform->poCT)
                {
                    double dfXOrig = padfX[i];
                    double dfYOrig = padfY[i];
                    double dfZOrig = padfZ[i];
                    if (!psTransform->poCT->Transform(
                                                1, &dfXOrig, &dfYOrig, &dfZOrig))
                    {
                        panSuccess[i] = FALSE;
                        continue;
                    }
                    GDALApplyGeoTransform( psTransform->adfReverseGeoTransform,
                                           dfXOrig, dfYOrig, &dfX, &dfY );
                }
                else
                    GDALApplyGeoTransform( psTransform->adfReverseGeoTransform,
                                           padfX[i], padfY[i], &dfX, &dfY );
                int dX = int(dfX);
                int dY = int(dfY);

                if (!(dX >= 0 && dY >= 0 &&
                      dX+2 <= nRasterXSize && dY+2 <= nRasterYSize))
                {
                    panSuccess[i] = FALSE;
                    continue;
                }
                int adElevData[4] = {0,0,0,0};
                CPLErr eErr = psTransform->poDS->RasterIO(GF_Read, dX, dY, 2, 2,
                                                          &adElevData, 2, 2,
                                                          GDT_Int32, 1, bands, 0, 0, 0);
                if(eErr != CE_None)
                {
                    panSuccess[i] = FALSE;
                    continue;
                }
                //bilinear interpolation
                double dfDeltaX = dfX - dX;
                double dfDeltaX1 = 1.0 - dfDeltaX;
                double dfDeltaY = dfY - dY;
                double dfDeltaY1 = 1.0 - dfDeltaY;

                double dfXZ1 = adElevData[0] * dfDeltaX1 + adElevData[1] * dfDeltaX;
                double dfXZ2 = adElevData[2] * dfDeltaX1 + adElevData[3] * dfDeltaX;
                double dfYZ = dfXZ1 * dfDeltaY1 + dfXZ2 * dfDeltaY;

                RPCTransformPoint( psRPC, padfX[i], padfY[i], 
                                   padfZ[i] + (psTransform->dfHeightOffset + dfYZ) *
                                                psTransform->dfHeightScale, 
                                   padfX + i, padfY + i );
            }
            else
                RPCTransformPoint( psRPC, padfX[i], padfY[i], 
                                   padfZ[i] + psTransform->dfHeightOffset *
                                              psTransform->dfHeightScale, 
                                   padfX + i, padfY + i );
            panSuccess[i] = TRUE;
        }

        return TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Compute the inverse (pixel/line/height to lat/long).  This      */
/*      function uses an iterative method from an initial linear        */
/*      approximation.                                                  */
/* -------------------------------------------------------------------- */
    for( i = 0; i < nPointCount; i++ )
    {
        double dfResultX, dfResultY;

        if(psTransform->poDS)
        {
            RPCInverseTransformPoint( psTransform, padfX[i], padfY[i], 
                      padfZ[i] + psTransform->dfHeightOffset *
                                 psTransform->dfHeightScale,
                      &dfResultX, &dfResultY );

            double dfX, dfY;
            //check if dem is not in WGS84 and transform points padfX[i], padfY[i]
            if(psTransform->poCT)
            {
                double dfZ = 0;
                if (!psTransform->poCT->Transform(1, &dfResultX, &dfResultY, &dfZ))
                {
                    panSuccess[i] = FALSE;
                    continue;
                }
            }

            GDALApplyGeoTransform( psTransform->adfReverseGeoTransform,
                                    dfResultX, dfResultY, &dfX, &dfY );
            int dX = int(dfX);
            int dY = int(dfY);

            if (!(dX >= 0 && dY >= 0 && dX+2 <= nRasterXSize && dY+2 <= nRasterYSize))
            {
                panSuccess[i] = FALSE;
                continue;
            }
            int adElevData[4] = {0,0,0,0};
            CPLErr eErr = psTransform->poDS->RasterIO(GF_Read, dX, dY, 2, 2,
                                                      &adElevData, 2, 2,
                                                      GDT_Int32, 1, bands, 0, 0, 0);
            if(eErr != CE_None)
            {
                panSuccess[i] = FALSE;
                continue;
            }
            //bilinear interpolation
            double dfDeltaX = dfX - dX;
            double dfDeltaX1 = 1.0 - dfDeltaX;
            double dfDeltaY = dfY - dY;
            double dfDeltaY1 = 1.0 - dfDeltaY;

            double dfXZ1 = adElevData[0] * dfDeltaX1 + adElevData[1] * dfDeltaX;
            double dfXZ2 = adElevData[2] * dfDeltaX1 + adElevData[3] * dfDeltaX;
            double dfYZ = dfXZ1 * dfDeltaY1 + dfXZ2 * dfDeltaY;

            RPCInverseTransformPoint( psTransform, padfX[i], padfY[i], 
                                      padfZ[i] + (psTransform->dfHeightOffset + dfYZ) *
                                                  psTransform->dfHeightScale,
                                      &dfResultX, &dfResultY );
        }
        else
        {
            RPCInverseTransformPoint( psTransform, padfX[i], padfY[i], 
                                      padfZ[i] + psTransform->dfHeightOffset *
                                                 psTransform->dfHeightScale,
                                      &dfResultX, &dfResultY );

        }
        padfX[i] = dfResultX;
        padfY[i] = dfResultY;

        panSuccess[i] = TRUE;
    }

    return TRUE;
}

/************************************************************************/
/*                    GDALSerializeRPCTransformer()                     */
/************************************************************************/

CPLXMLNode *GDALSerializeRPCTransformer( void *pTransformArg )

{
    VALIDATE_POINTER1( pTransformArg, "GDALSerializeRPCTransformer", NULL );

    CPLXMLNode *psTree;
    GDALRPCTransformInfo *psInfo = 
        (GDALRPCTransformInfo *)(pTransformArg);

    psTree = CPLCreateXMLNode( NULL, CXT_Element, "RPCTransformer" );

/* -------------------------------------------------------------------- */
/*      Serialize bReversed.                                            */
/* -------------------------------------------------------------------- */
    CPLCreateXMLElementAndValue( 
        psTree, "Reversed", 
        CPLString().Printf( "%d", psInfo->bReversed ) );

/* -------------------------------------------------------------------- */
/*      Serialize Height Offset.                                        */
/* -------------------------------------------------------------------- */
    CPLCreateXMLElementAndValue( 
        psTree, "HeightOffset", 
        CPLString().Printf( "%.15g", psInfo->dfHeightOffset ) );

/* -------------------------------------------------------------------- */
/*      Serialize Height Scale.                                         */
/* -------------------------------------------------------------------- */
    if (psInfo->dfHeightScale != 1.0)
        CPLCreateXMLElementAndValue( 
            psTree, "HeightScale", 
            CPLString().Printf( "%.15g", psInfo->dfHeightScale ) );

/* -------------------------------------------------------------------- */
/*      Serialize DEM path.                                             */
/* -------------------------------------------------------------------- */
    if (psInfo->pszDEMPath != NULL)
        CPLCreateXMLElementAndValue( 
            psTree, "DEMPath", 
            CPLString().Printf( "%s", psInfo->pszDEMPath ) );

/* -------------------------------------------------------------------- */
/*      Serialize pixel error threshold.                                */
/* -------------------------------------------------------------------- */
    CPLCreateXMLElementAndValue( 
        psTree, "PixErrThreshold", 
        CPLString().Printf( "%.15g", psInfo->dfPixErrThreshold ) );

/* -------------------------------------------------------------------- */
/*      RPC metadata.                                                   */
/* -------------------------------------------------------------------- */
    char **papszMD = RPCInfoToMD( &(psInfo->sRPC) );
    CPLXMLNode *psMD= CPLCreateXMLNode( psTree, CXT_Element, 
                                        "Metadata" );

    for( int i = 0; papszMD != NULL && papszMD[i] != NULL; i++ )
    {
        const char *pszRawValue;
        char *pszKey;
        CPLXMLNode *psMDI;
                
        pszRawValue = CPLParseNameValue( papszMD[i], &pszKey );
                
        psMDI = CPLCreateXMLNode( psMD, CXT_Element, "MDI" );
        CPLSetXMLValue( psMDI, "#key", pszKey );
        CPLCreateXMLNode( psMDI, CXT_Text, pszRawValue );
                
        CPLFree( pszKey );
    }

    CSLDestroy( papszMD );

    return psTree;
}

/************************************************************************/
/*                   GDALDeserializeRPCTransformer()                    */
/************************************************************************/

void *GDALDeserializeRPCTransformer( CPLXMLNode *psTree )

{
    void *pResult;
    char **papszOptions = NULL;

/* -------------------------------------------------------------------- */
/*      Collect metadata.                                               */
/* -------------------------------------------------------------------- */
    char **papszMD = NULL;
    CPLXMLNode *psMDI, *psMetadata;
    GDALRPCInfo sRPC;

    psMetadata = CPLGetXMLNode( psTree, "Metadata" );

    if( psMetadata->eType != CXT_Element
        || !EQUAL(psMetadata->pszValue,"Metadata") )
        return NULL;
    
    for( psMDI = psMetadata->psChild; psMDI != NULL; 
         psMDI = psMDI->psNext )
    {
        if( !EQUAL(psMDI->pszValue,"MDI") 
            || psMDI->eType != CXT_Element 
            || psMDI->psChild == NULL 
            || psMDI->psChild->psNext == NULL 
            || psMDI->psChild->eType != CXT_Attribute
            || psMDI->psChild->psChild == NULL )
            continue;
        
        papszMD = 
            CSLSetNameValue( papszMD, 
                             psMDI->psChild->psChild->pszValue, 
                             psMDI->psChild->psNext->pszValue );
    }

    if( !GDALExtractRPCInfo( papszMD, &sRPC ) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Failed to reconstitute RPC transformer." );
        CSLDestroy( papszMD );
        return NULL;
    }

    CSLDestroy( papszMD );

/* -------------------------------------------------------------------- */
/*      Get other flags.                                                */
/* -------------------------------------------------------------------- */
    double dfPixErrThreshold;
    int bReversed;

    bReversed = atoi(CPLGetXMLValue(psTree,"Reversed","0"));

    dfPixErrThreshold = 
        CPLAtof(CPLGetXMLValue(psTree,"PixErrThreshold","0.25"));

    papszOptions = CSLSetNameValue( papszOptions, "RPC_HEIGHT",
                                    CPLGetXMLValue(psTree,"HeightOffset","0"));
    papszOptions = CSLSetNameValue( papszOptions, "RPC_HEIGHT_SCALE",
                                    CPLGetXMLValue(psTree,"HeightScale","1"));
    const char* pszDEMPath = CPLGetXMLValue(psTree,"DEMPath",NULL);
    if (pszDEMPath != NULL)
        papszOptions = CSLSetNameValue( papszOptions, "RPC_DEM",
                                        pszDEMPath);

/* -------------------------------------------------------------------- */
/*      Generate transformation.                                        */
/* -------------------------------------------------------------------- */
    pResult = GDALCreateRPCTransformer( &sRPC, bReversed, dfPixErrThreshold,
                                        papszOptions );
    
    CSLDestroy( papszOptions );

    return pResult;
}
