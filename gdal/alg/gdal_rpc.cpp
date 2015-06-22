/******************************************************************************
 * $Id$
 *
 * Project:  Image Warper
 * Purpose:  Implements a rational polynomail (RPC) based transformer. 
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2009-2013, Even Rouault <even dot rouault at mines-paris dot org>
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
#include "gdal_mdreader.h"

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

char ** RPCInfoToMD( GDALRPCInfo *psRPCInfo )

{
    char **papszMD = NULL;
    CPLString osField, osMultiField;
    int i;

    osField.Printf( "%.15g", psRPCInfo->dfLINE_OFF );
    papszMD = CSLSetNameValue( papszMD, RPC_LINE_OFF, osField );

    osField.Printf( "%.15g", psRPCInfo->dfSAMP_OFF );
    papszMD = CSLSetNameValue( papszMD, RPC_SAMP_OFF, osField );

    osField.Printf( "%.15g", psRPCInfo->dfLAT_OFF );
    papszMD = CSLSetNameValue( papszMD, RPC_LAT_OFF, osField );

    osField.Printf( "%.15g", psRPCInfo->dfLONG_OFF );
    papszMD = CSLSetNameValue( papszMD, RPC_LONG_OFF, osField );

    osField.Printf( "%.15g", psRPCInfo->dfHEIGHT_OFF );
    papszMD = CSLSetNameValue( papszMD, RPC_HEIGHT_OFF, osField );

    osField.Printf( "%.15g", psRPCInfo->dfLINE_SCALE );
    papszMD = CSLSetNameValue( papszMD, RPC_LINE_SCALE, osField );

    osField.Printf( "%.15g", psRPCInfo->dfSAMP_SCALE );
    papszMD = CSLSetNameValue( papszMD, RPC_SAMP_SCALE, osField );

    osField.Printf( "%.15g", psRPCInfo->dfLAT_SCALE );
    papszMD = CSLSetNameValue( papszMD, RPC_LAT_SCALE, osField );

    osField.Printf( "%.15g", psRPCInfo->dfLONG_SCALE );
    papszMD = CSLSetNameValue( papszMD, RPC_LONG_SCALE, osField );

    osField.Printf( "%.15g", psRPCInfo->dfHEIGHT_SCALE );
    papszMD = CSLSetNameValue( papszMD, RPC_HEIGHT_SCALE, osField );

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
   
    // RPCs are using the center of upper left pixel = 0,0 convention
    // convert to top left corner = 0,0 convention used in GDAL
     *pdfPixel = dfResultX * psRPC->dfSAMP_SCALE + psRPC->dfSAMP_OFF + 0.5;
    *pdfLine = dfResultY * psRPC->dfLINE_SCALE + psRPC->dfLINE_OFF + 0.5;
}

/************************************************************************/
/* ==================================================================== */
/*			     GDALRPCTransformer                         */
/* ==================================================================== */
/************************************************************************/

/*! DEM Resampling Algorithm */
typedef enum {
  /*! Nearest neighbour (select on one input pixel) */ DRA_NearestNeighbour=0,
  /*! Bilinear (2x2 kernel) */                         DRA_Bilinear=1,
  /*! Cubic Convolution Approximation (4x4 kernel) */  DRA_Cubic=2
} DEMResampleAlg;

typedef struct {

    GDALTransformerInfo sTI;

    GDALRPCInfo sRPC;

    double      adfPLToLatLongGeoTransform[6];

    int         bReversed;

    double      dfPixErrThreshold;

    double      dfHeightOffset;

    double      dfHeightScale;

    char        *pszDEMPath;

    DEMResampleAlg eResampleAlg;
    
    int         bHasDEMMissingValue;
    double      dfDEMMissingValue;

    int         bHasTriedOpeningDS;
    GDALDataset *poDS;

    OGRCoordinateTransformation *poCT;

    double      adfDEMGeoTransform[6];
    double      adfDEMReverseGeoTransform[6];
} GDALRPCTransformInfo;

/************************************************************************/
/*                     GDALSerializeRPCDEMResample()                    */
/************************************************************************/

static const char* GDALSerializeRPCDEMResample(DEMResampleAlg eResampleAlg)
{
    switch(eResampleAlg)
    {
        case  DRA_NearestNeighbour:
            return "near";
        case DRA_Cubic:
            return "cubic";
        default:
        case DRA_Bilinear:
            return "bilinear";
    }
}

/************************************************************************/
/*                   GDALCreateSimilarRPCTransformer()                  */
/************************************************************************/

static
void* GDALCreateSimilarRPCTransformer( void *hTransformArg, double dfRatioX, double dfRatioY )
{
    VALIDATE_POINTER1( hTransformArg, "GDALCreateSimilarRPCTransformer", NULL );

    GDALRPCTransformInfo *psInfo = (GDALRPCTransformInfo *) hTransformArg;
    
    GDALRPCInfo sRPC;
    memcpy(&sRPC, &(psInfo->sRPC), sizeof(GDALRPCInfo));
    
    if( dfRatioX != 1.0 || dfRatioY != 1.0 )
    {
        sRPC.dfLINE_OFF /= dfRatioY;
        sRPC.dfLINE_SCALE /= dfRatioY;
        sRPC.dfSAMP_OFF /= dfRatioX;
        sRPC.dfSAMP_SCALE /= dfRatioX;
    }

    char** papszOptions = NULL;
    papszOptions = CSLSetNameValue(papszOptions, "RPC_HEIGHT",
                                   CPLSPrintf("%.18g", psInfo->dfHeightOffset));
    papszOptions = CSLSetNameValue(papszOptions, "RPC_HEIGHT_SCALE",
                                   CPLSPrintf("%.18g", psInfo->dfHeightScale));
    if( psInfo->pszDEMPath != NULL )
    {
        papszOptions = CSLSetNameValue(papszOptions, "RPC_DEM", psInfo->pszDEMPath);
        papszOptions = CSLSetNameValue(papszOptions, "RPC_DEMINTERPOLATION",
                                       GDALSerializeRPCDEMResample(psInfo->eResampleAlg));
        if( psInfo->bHasDEMMissingValue )
            papszOptions = CSLSetNameValue(papszOptions, "RPC_DEM_MISSING_VALUE",
                                           CPLSPrintf("%.18g", psInfo->dfDEMMissingValue)) ;
    }
    psInfo = (GDALRPCTransformInfo*) GDALCreateRPCTransformer( &sRPC,
           psInfo->bReversed, psInfo->dfPixErrThreshold, papszOptions );
    CSLDestroy(papszOptions);

    return psInfo;
}

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
 * and the GeoTIFF RPC document http://geotiff.maptools.org/rpc_prop.html .  
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
 * Useful when elevation offsets of the DEM are not expressed in meters. (GDAL >= 1.8.0)
 *
 * <li> RPC_DEM: the name of a GDAL dataset (a DEM file typically) used to
 * extract elevation offsets from. In this situation the Z passed into the
 * transformation function is assumed to be height above ground. This option
 * should be used in replacement of RPC_HEIGHT to provide a way of defining
 * a non uniform ground for the target scene (GDAL >= 1.8.0)
 *
 * <li> RPC_DEMINTERPOLATION: the DEM interpolation (near, bilinear or cubic)
 *
 * <li> RPC_DEM_MISSING_VALUE: value of DEM height that must be used in case
 * the DEM has nodata value at the sampling point, or if its extent does not
 * cover the requested coordinate. When not specified, missing values will cause
 * a failed transform. (GDAL >= 1.11.2)
 *
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

    memcpy( psTransform->sTI.abySignature, GDAL_GTI2_SIGNATURE, strlen(GDAL_GTI2_SIGNATURE) );
    psTransform->sTI.pszClassName = "GDALRPCTransformer";
    psTransform->sTI.pfnTransform = GDALRPCTransform;
    psTransform->sTI.pfnCleanup = GDALDestroyRPCTransformer;
    psTransform->sTI.pfnSerialize = GDALSerializeRPCTransformer;
    psTransform->sTI.pfnCreateSimilar = GDALCreateSimilarRPCTransformer;
   
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
/*                      The DEM interpolation                           */
/* -------------------------------------------------------------------- */
    const char *pszDEMInterpolation = CSLFetchNameValueDef( papszOptions, "RPC_DEMINTERPOLATION", "bilinear" );
    if(EQUAL(pszDEMInterpolation, "near" ))
        psTransform->eResampleAlg = DRA_NearestNeighbour;
    else if(EQUAL(pszDEMInterpolation, "bilinear" ))
        psTransform->eResampleAlg = DRA_Bilinear;
    else if(EQUAL(pszDEMInterpolation, "cubic" ))
        psTransform->eResampleAlg = DRA_Cubic;
    else
    {
        CPLDebug("RPC", "Unknown interpolation %s. Defaulting to bilinear", pszDEMInterpolation); 
        psTransform->eResampleAlg = DRA_Bilinear;
    }

/* -------------------------------------------------------------------- */
/*                       The DEM missing value                          */
/* -------------------------------------------------------------------- */
    const char *pszDEMMissingValue = CSLFetchNameValue( papszOptions, "RPC_DEM_MISSING_VALUE" );
    if( pszDEMMissingValue != NULL )
    {
        psTransform->bHasDEMMissingValue = TRUE;
        psTransform->dfDEMMissingValue = CPLAtof(pszDEMMissingValue);
    }

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
    adfGTFromLL[4] = (dfRefLineDelta - dfRefLine) / dfLLDelta;
    
    RPCTransformPoint( psRPCInfo, dfRefLong, dfRefLat+dfLLDelta, 0.0, 
                       &dfRefPixelDelta, &dfRefLineDelta );
    adfGTFromLL[2] = (dfRefPixelDelta - dfRefPixel) / dfLLDelta;
    adfGTFromLL[5] = (dfRefLineDelta - dfRefLine) / dfLLDelta;

    adfGTFromLL[0] = dfRefPixel
        - adfGTFromLL[1] * dfRefLong - adfGTFromLL[2] * dfRefLat;
    adfGTFromLL[3] = dfRefLine
        - adfGTFromLL[4] * dfRefLong - adfGTFromLL[5] * dfRefLat;
    
    if( !GDALInvGeoTransform( adfGTFromLL, psTransform->adfPLToLatLongGeoTransform) )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot invert geotransform");
        GDALDestroyRPCTransformer(psTransform);
        return NULL;
    }
    return psTransform;
}

/************************************************************************/
/*                 GDALDestroyReprojectionTransformer()                 */
/************************************************************************/

void GDALDestroyRPCTransformer( void *pTransformAlg )

{
    if( pTransformAlg == NULL )
        return;

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
    double dfPixelDeltaX=0.0, dfPixelDeltaY=0.0;

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
    {
#ifdef notdef
        CPLDebug( "RPC", "Failed Iterations %d: Got: %g,%g  Offset=%g,%g", 
                  iIter, 
                  dfResultX, dfResultY,
                  dfPixelDeltaX, dfPixelDeltaY );
#endif
    }
    
    *pdfLong = dfResultX;
    *pdfLat = dfResultY;
}


static
double BiCubicKernel(double dfVal)
{
	if ( dfVal > 2.0 )
		return 0.0;
	
	double a, b, c, d;
	double xm1 = dfVal - 1.0;
	double xp1 = dfVal + 1.0;
	double xp2 = dfVal + 2.0;
	
	a = ( xp2 <= 0.0 ) ? 0.0 : xp2 * xp2 * xp2;
	b = ( xp1 <= 0.0 ) ? 0.0 : xp1 * xp1 * xp1;
	c = ( dfVal   <= 0.0 ) ? 0.0 : dfVal * dfVal * dfVal;
	d = ( xm1 <= 0.0 ) ? 0.0 : xm1 * xm1 * xm1;
	
	return ( 0.16666666666666666667 * ( a - ( 4.0 * b ) + ( 6.0 * c ) - ( 4.0 * d ) ) );
}

/************************************************************************/
/*                        GDALRPCGetDEMHeight()                         */
/************************************************************************/

static
int GDALRPCGetDEMHeight( GDALRPCTransformInfo *psTransform,
                      double dfX, double dfY, double* pdfDEMH )
{
    
    int bGotNoDataValue = FALSE;
    double dfNoDataValue = 0;
    int nRasterXSize = psTransform->poDS->GetRasterXSize();
    int nRasterYSize = psTransform->poDS->GetRasterYSize();
    dfNoDataValue = psTransform->poDS->GetRasterBand(1)->GetNoDataValue( &bGotNoDataValue );

    int bands[1] = {1};

    double dfDEMH(0);

    if(psTransform->eResampleAlg == DRA_Cubic)
    {
        // convert from upper left corner of pixel coordinates to center of pixel coordinates:
        dfX -= 0.5;
        dfY -= 0.5;
        int dX = int(dfX);
        int dY = int(dfY);
        double dfDeltaX = dfX - dX;
        double dfDeltaY = dfY - dY;

        int dXNew = dX - 1;
        int dYNew = dY - 1;
        if (!(dXNew >= 0 && dYNew >= 0 && dXNew + 4 <= nRasterXSize && dYNew + 4 <= nRasterYSize))
        {
            return FALSE;
        }
        //cubic interpolation
        double adfElevData[16] = {0};
        CPLErr eErr = psTransform->poDS->RasterIO(GF_Read, dXNew, dYNew, 4, 4,
                                                    &adfElevData, 4, 4,
                                                    GDT_Float64, 1, bands, 0, 0, 0,
                                                    NULL);
        if(eErr != CE_None)
        {
            return FALSE;
        }

        double dfSumH(0), dfSumWeight(0);
        for ( int k_i = 0; k_i < 4; k_i++ )
        {
            // Loop across the X axis
            for ( int k_j = 0; k_j < 4; k_j++ )
            {
                // Calculate the weight for the specified pixel according
                // to the bicubic b-spline kernel we're using for
                // interpolation
                int dKernIndX = k_j - 1;
                int dKernIndY = k_i - 1;
                double dfPixelWeight = BiCubicKernel(dKernIndX - dfDeltaX) * BiCubicKernel(dKernIndY - dfDeltaY);

                // Create a sum of all values
                // adjusted for the pixel's calculated weight
                double dfElev = adfElevData[k_j + k_i * 4];
                if( bGotNoDataValue && ARE_REAL_EQUAL(dfNoDataValue, dfElev) )
                    continue;

                dfSumH += dfElev * dfPixelWeight;
                dfSumWeight += dfPixelWeight;
            }
        }
        if( dfSumWeight == 0.0 )
        {
            return FALSE;
        }
        dfDEMH = dfSumH / dfSumWeight;
    }
    else if(psTransform->eResampleAlg == DRA_Bilinear)
    {
        // convert from upper left corner of pixel coordinates to center of pixel coordinates:
        dfX -= 0.5;
        dfY -= 0.5;
        int dX = int(dfX);
        int dY = int(dfY);
        double dfDeltaX = dfX - dX;
        double dfDeltaY = dfY - dY;

        if (!(dX >= 0 && dY >= 0 && dX + 2 <= nRasterXSize && dY + 2 <= nRasterYSize))
        {
            return FALSE;
        }
        //bilinear interpolation
        double adfElevData[4] = {0,0,0,0};
        CPLErr eErr = psTransform->poDS->RasterIO(GF_Read, dX, dY, 2, 2,
                                                    &adfElevData, 2, 2,
                                                    GDT_Float64, 1, bands, 0, 0, 0,
                                                  NULL);
        if(eErr != CE_None)
        {
            return FALSE;
        }
        if( bGotNoDataValue )
        {
            // TODO: we could perhaps use a valid sample if there's one
            int bFoundNoDataElev = FALSE;
            for(int k_i=0;k_i<4;k_i++)
            {
                if( ARE_REAL_EQUAL(dfNoDataValue, adfElevData[k_i]) )
                    bFoundNoDataElev = TRUE;
            }
            if( bFoundNoDataElev )
            {
                return FALSE;
            }
        }
        double dfDeltaX1 = 1.0 - dfDeltaX;                
        double dfDeltaY1 = 1.0 - dfDeltaY;

        double dfXZ1 = adfElevData[0] * dfDeltaX1 + adfElevData[1] * dfDeltaX;
        double dfXZ2 = adfElevData[2] * dfDeltaX1 + adfElevData[3] * dfDeltaX;
        double dfYZ = dfXZ1 * dfDeltaY1 + dfXZ2 * dfDeltaY;
        dfDEMH = dfYZ;
    }
    else
    {
        int dX = (int) (dfX);
        int dY = (int) (dfY);
        if (!(dX >= 0 && dY >= 0 && dX < nRasterXSize && dY < nRasterYSize))
        {
            return FALSE;
        }
        CPLErr eErr = psTransform->poDS->RasterIO(GF_Read, dX, dY, 1, 1,
                                                    &dfDEMH, 1, 1,
                                                    GDT_Float64, 1, bands, 0, 0, 0,
                                                  NULL);
        if(eErr != CE_None ||
            (bGotNoDataValue && ARE_REAL_EQUAL(dfNoDataValue, dfDEMH)) )
        {
            return FALSE;
        }
    }
    
    *pdfDEMH = dfDEMH;

    return TRUE;
}

/************************************************************************/
/*                    GDALRPCTransformWholeLineWithDEM()                */
/************************************************************************/

static int GDALRPCTransformWholeLineWithDEM( GDALRPCTransformInfo *psTransform, 
                                             int nPointCount, 
                                             double *padfX, double *padfY, double *padfZ,
                                             int *panSuccess,
                                             int nXLeft, int nXWidth,
                                             int nYTop, int nYHeight )
{
    int i;
    GDALRPCInfo *psRPC = &(psTransform->sRPC);

    double* padfDEMBuffer = (double*) VSIMalloc2(sizeof(double), nXWidth * nYHeight);
    if( padfDEMBuffer == NULL )
    {
        for( i = 0; i < nPointCount; i++ )
            panSuccess[i] = FALSE;
        return FALSE;
    }
    CPLErr eErr = psTransform->poDS->GetRasterBand(1)->
            RasterIO(GF_Read, nXLeft, nYTop, nXWidth, nYHeight,
                        padfDEMBuffer, nXWidth, nYHeight,
                        GDT_Float64, 0, 0, NULL);
    if( eErr != CE_None )
    {
        for( i = 0; i < nPointCount; i++ )
            panSuccess[i] = FALSE;
        VSIFree(padfDEMBuffer);
        return FALSE;
    }


    int bGotNoDataValue = FALSE;
    double dfNoDataValue = 0;
    dfNoDataValue = psTransform->poDS->GetRasterBand(1)->GetNoDataValue( &bGotNoDataValue );

    // dfY in pixel center convention
    double dfY = psTransform->adfDEMReverseGeoTransform[3] +
                        padfY[0] * psTransform->adfDEMReverseGeoTransform[5] - 0.5;
    int nY = int(dfY);
    double dfDeltaY = dfY - nY;

    for( i = 0; i < nPointCount; i++ )
    {
        double dfDEMH(0);

        if(psTransform->eResampleAlg == DRA_Cubic)
        {
            // dfX in pixel center convention
            double dfX = psTransform->adfDEMReverseGeoTransform[0] +
                            padfX[i] * psTransform->adfDEMReverseGeoTransform[1] - 0.5;
            int nX = int(dfX);
            double dfDeltaX = dfX - nX;

            int nXNew = nX - 1;

            double dfSumH(0), dfSumWeight(0);
            for ( int k_i = 0; k_i < 4; k_i++ )
            {
                // Loop across the X axis
                for ( int k_j = 0; k_j < 4; k_j++ )
                {
                    // Calculate the weight for the specified pixel according
                    // to the bicubic b-spline kernel we're using for
                    // interpolation
                    int dKernIndX = k_j - 1;
                    int dKernIndY = k_i - 1;
                    double dfPixelWeight = BiCubicKernel(dKernIndX - dfDeltaX) * BiCubicKernel(dKernIndY - dfDeltaY);

                    // Create a sum of all values
                    // adjusted for the pixel's calculated weight
                    double dfElev = padfDEMBuffer[k_i * nXWidth + nXNew - nXLeft + k_j];
                    if( bGotNoDataValue && ARE_REAL_EQUAL(dfNoDataValue, dfElev) )
                        continue;

                    dfSumH += dfElev * dfPixelWeight;
                    dfSumWeight += dfPixelWeight;
                }
            }
            if( dfSumWeight == 0.0 )
            {
                if( psTransform->bHasDEMMissingValue )
                    dfDEMH = psTransform->dfDEMMissingValue;
                else
                {
                    panSuccess[i] = FALSE;
                    continue;
                }
            }
            else
                dfDEMH = dfSumH / dfSumWeight;
        }
        else if(psTransform->eResampleAlg == DRA_Bilinear)
        {
            // dfX in pixel center convention
            double dfX = psTransform->adfDEMReverseGeoTransform[0] +
                            padfX[i] * psTransform->adfDEMReverseGeoTransform[1] - 0.5;
            int nX = int(dfX);
            double dfDeltaX = dfX - nX;

            //bilinear interpolation
            double adfElevData[4];
            memcpy(adfElevData, padfDEMBuffer + nX - nXLeft, 2 * sizeof(double));
            memcpy(adfElevData + 2, padfDEMBuffer + nXWidth + nX - nXLeft, 2 * sizeof(double));

            int bFoundNoDataElev = FALSE;
            if( bGotNoDataValue )
            {
                int k_valid_sample = -1;
                for(int k_i=0;k_i<4;k_i++)
                {
                    if( ARE_REAL_EQUAL(dfNoDataValue, adfElevData[k_i]) )
                    {
                        bFoundNoDataElev = TRUE;
                    }
                    else if( k_valid_sample < 0 )
                        k_valid_sample = k_i;
                }
                if( bFoundNoDataElev )
                {
                    if( k_valid_sample >= 0 )
                    {
                        dfDEMH = adfElevData[k_valid_sample];
                        RPCTransformPoint( psRPC, padfX[i], padfY[i], 
                            padfZ[i] + (psTransform->dfHeightOffset + dfDEMH) *
                                        psTransform->dfHeightScale, 
                            padfX + i, padfY + i );

                        panSuccess[i] = TRUE;
                        continue;
                    }
                    else if( psTransform->bHasDEMMissingValue )
                    {
                        dfDEMH = psTransform->dfDEMMissingValue;
                        RPCTransformPoint( psRPC, padfX[i], padfY[i], 
                            padfZ[i] + (psTransform->dfHeightOffset + dfDEMH) *
                                        psTransform->dfHeightScale, 
                            padfX + i, padfY + i );

                        panSuccess[i] = TRUE;
                        continue;
                    }
                    else
                    {
                        panSuccess[i] = FALSE;
                        continue;
                    }
                }
            }
            double dfDeltaX1 = 1.0 - dfDeltaX;                
            double dfDeltaY1 = 1.0 - dfDeltaY;

            double dfXZ1 = adfElevData[0] * dfDeltaX1 + adfElevData[1] * dfDeltaX;
            double dfXZ2 = adfElevData[2] * dfDeltaX1 + adfElevData[3] * dfDeltaX;
            double dfYZ = dfXZ1 * dfDeltaY1 + dfXZ2 * dfDeltaY;
            dfDEMH = dfYZ;
        }
        else
        {
            double dfX = psTransform->adfDEMReverseGeoTransform[0] +
                            padfX[i] * psTransform->adfDEMReverseGeoTransform[1];
            int nX = int(dfX);

            dfDEMH = padfDEMBuffer[nX - nXLeft];
            if( bGotNoDataValue && ARE_REAL_EQUAL(dfNoDataValue, dfDEMH) )
            {
                if( psTransform->bHasDEMMissingValue )
                    dfDEMH = psTransform->dfDEMMissingValue;
                else
                {
                    panSuccess[i] = FALSE;
                    continue;
                }
            }
        }

        RPCTransformPoint( psRPC, padfX[i], padfY[i], 
                            padfZ[i] + (psTransform->dfHeightOffset + dfDEMH) *
                                        psTransform->dfHeightScale, 
                            padfX + i, padfY + i );

        panSuccess[i] = TRUE;
    }

    VSIFree(padfDEMBuffer);
    
    return TRUE;
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
                                psTransform->adfDEMGeoTransform) == CE_None &&
                GDALInvGeoTransform( psTransform->adfDEMGeoTransform,
                                     psTransform->adfDEMReverseGeoTransform ))
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

/* -------------------------------------------------------------------- */
/*      The simple case is transforming from lat/long to pixel/line.    */
/*      Just apply the equations directly.                              */
/* -------------------------------------------------------------------- */
    if( bDstToSrc )
    {
        /* Optimization to avoid doing too many picking in DEM in the particular */
        /* case where each point to transform is on a single line of the DEM */
        /* To make it simple and fast we check that all input latitudes are */
        /* identical, that the DEM is in WGS84 geodetic and that it has no rotation. */
        /* Such case is for example triggered when doing gdalwarp with a target SRS */
        /* of EPSG:4326 or EPSG:3857 */
        if( nPointCount >= 10 && psTransform->poDS != NULL &&
            psTransform->poCT == NULL && padfY[0] == padfY[nPointCount-1] &&
            padfY[0] == padfY[nPointCount/ 2] && 
            psTransform->adfDEMReverseGeoTransform[1] > 0.0 &&
            psTransform->adfDEMReverseGeoTransform[2] == 0.0 && 
            psTransform->adfDEMReverseGeoTransform[4] == 0.0 &&
            CSLTestBoolean(CPLGetConfigOption("GDAL_RPC_DEM_OPTIM", "YES")) )
        {
            int bUseOptimized = TRUE;
            double dfMinX = padfX[0], dfMaxX = padfX[0];
            for(i = 1; i < nPointCount; i++)
            {
                if( padfY[i] != padfY[0] )
                {
                    bUseOptimized = FALSE;
                    break;
                }
                if( padfX[i] < dfMinX ) dfMinX = padfX[i];
                if( padfX[i] > dfMaxX ) dfMaxX = padfX[i];
            }
            if( bUseOptimized )
            {
                double dfX1, dfY1, dfX2, dfY2;
                GDALApplyGeoTransform( psTransform->adfDEMReverseGeoTransform,
                                    dfMinX, padfY[0], &dfX1, &dfY1 );
                GDALApplyGeoTransform( psTransform->adfDEMReverseGeoTransform,
                                    dfMaxX, padfY[0], &dfX2, &dfY2 );

                // convert to center of pixel convention for reading the image data
                if( psTransform->eResampleAlg != DRA_NearestNeighbour )
                {
                    dfX1 -= 0.5;
                    dfY1 -= 0.5;
                    dfX2 -= 0.5;
                    dfY2 -= 0.5;
                }
                int nXLeft = int(floor(dfX1));
                int nXRight = int(floor(dfX2));
                int nXWidth = nXRight - nXLeft + 1;
                int nYTop = int(floor(dfY1));
                int nYHeight;
                if( psTransform->eResampleAlg == DRA_Cubic )
                {
                    nXLeft --;
                    nXWidth += 3;
                    nYTop --;
                    nYHeight = 4;
                }
                else if( psTransform->eResampleAlg == DRA_Bilinear )
                {
                    nXWidth ++;
                    nYHeight = 2;
                }
                else
                {
                    nYHeight = 1;
                }
                if( nXLeft >= 0 && nXLeft + nXWidth <= psTransform->poDS->GetRasterXSize() &&
                    nYTop >= 0 && nYTop + nYHeight <= psTransform->poDS->GetRasterYSize() )
                {
                    static int bOnce = FALSE;
                    if( !bOnce )
                    {
                        bOnce = TRUE;
                        CPLDebug("RPC", "Using GDALRPCTransformWholeLineWithDEM");
                    }
                    return GDALRPCTransformWholeLineWithDEM( psTransform, nPointCount, 
                                                             padfX, padfY, padfZ,
                                                             panSuccess,
                                                             nXLeft, nXWidth,
                                                             nYTop, nYHeight );
                }
            }
        }
        
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
                    GDALApplyGeoTransform( psTransform->adfDEMReverseGeoTransform,
                                           dfXOrig, dfYOrig, &dfX, &dfY );
                }
                else
                    GDALApplyGeoTransform( psTransform->adfDEMReverseGeoTransform,
                                           padfX[i], padfY[i], &dfX, &dfY );

                double dfDEMH(0);
                if( !GDALRPCGetDEMHeight( psTransform, dfX, dfY, &dfDEMH) )
                {
                    if( psTransform->bHasDEMMissingValue )
                        dfDEMH = psTransform->dfDEMMissingValue;
                    else
                    {
                        panSuccess[i] = FALSE;
                        continue;
                    }
                }

                RPCTransformPoint( psRPC, padfX[i], padfY[i], 
                                   padfZ[i] + (psTransform->dfHeightOffset + dfDEMH) *
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

            GDALApplyGeoTransform( psTransform->adfDEMReverseGeoTransform,
                                    dfResultX, dfResultY, &dfX, &dfY );

            double dfDEMH(0);
            if( !GDALRPCGetDEMHeight( psTransform, dfX, dfY, &dfDEMH) )
            {
                if( psTransform->bHasDEMMissingValue )
                    dfDEMH = psTransform->dfDEMMissingValue;
                else
                {
                    panSuccess[i] = FALSE;
                    continue;
                }
            }

            RPCInverseTransformPoint( psTransform, padfX[i], padfY[i], 
                                      padfZ[i] + (psTransform->dfHeightOffset + dfDEMH) *
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
    {
        CPLCreateXMLElementAndValue( 
            psTree, "DEMPath", 
            CPLString().Printf( "%s", psInfo->pszDEMPath ) );

/* -------------------------------------------------------------------- */
/*      Serialize DEM interpolation                                     */
/* -------------------------------------------------------------------- */
        CPLCreateXMLElementAndValue( 
            psTree, "DEMInterpolation", GDALSerializeRPCDEMResample(psInfo->eResampleAlg) );

        if( psInfo->bHasDEMMissingValue )
        {
            CPLCreateXMLElementAndValue( 
                psTree, "DEMMissingValue", CPLSPrintf("%.18g", psInfo->dfDEMMissingValue) );
        }
    }

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

    if( psMetadata == NULL
        || psMetadata->eType != CXT_Element
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

    const char* pszDEMInterpolation = CPLGetXMLValue(psTree,"DEMInterpolation", "bilinear");
    if (pszDEMInterpolation != NULL)
        papszOptions = CSLSetNameValue( papszOptions, "RPC_DEMINTERPOLATION",
                                        pszDEMInterpolation);

    const char* pszDEMMissingValue = CPLGetXMLValue(psTree,"DEMMissingValue", NULL);
    if (pszDEMMissingValue != NULL)
        papszOptions = CSLSetNameValue( papszOptions, "RPC_DEM_MISSING_VALUE",
                                        pszDEMMissingValue);

/* -------------------------------------------------------------------- */
/*      Generate transformation.                                        */
/* -------------------------------------------------------------------- */
    pResult = GDALCreateRPCTransformer( &sRPC, bReversed, dfPixErrThreshold,
                                        papszOptions );
    
    CSLDestroy( papszOptions );

    return pResult;
}
