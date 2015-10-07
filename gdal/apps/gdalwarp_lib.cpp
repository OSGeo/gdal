/******************************************************************************
 * $Id$
 *
 * Project:  High Performance Image Reprojector
 * Purpose:  Test program for high performance warper API.
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
 *
 ******************************************************************************
 * Copyright (c) 2002, i3 - information integration and imaging 
 *                          Fort Collin, CO
 * Copyright (c) 2007-2015, Even Rouault <even dot rouault at mines-paris dot org>
 * Copyright (c) 2015, Faza Mahamood
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

#include "gdalwarper.h"
#include "cpl_string.h"
#include "cpl_error.h"
#include "ogr_spatialref.h"
#include "ogr_api.h"
#include "commonutils.h"
#include "gdal_priv.h"
#include <vector>
#include "gdal_utils_priv.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                        GDALWarpAppOptions                            */
/************************************************************************/

/** Options for use with GDALWarp(). GDALWarpAppOptions* must be allocated and
 * freed with GDALWarpAppOptionsNew() and GDALWarpAppOptionsFree() respectively.
 */
struct GDALWarpAppOptions
{
    /*! set georeferenced extents of output file to be created (in target SRS by default,
        or in the SRS specified with pszTE_SRS) */
    double dfMinX;
    double dfMinY;
    double dfMaxX;
    double dfMaxY;

    /*! the SRS in which to interpret the coordinates given in GDALWarpAppOptions::dfMinX,
        GDALWarpAppOptions::dfMinY, GDALWarpAppOptions::dfMaxX and GDALWarpAppOptions::dfMaxY.
        The SRS may be any of the usual GDAL/OGR forms,
        complete WKT, PROJ.4, EPSG:n or a file containing the WKT. It is a
        conveniency e.g. when knowing the output coordinates in a
        geodetic long/lat SRS, but still wanting a result in a projected
        coordinate system. */
    char *pszTE_SRS;

    /*! set output file resolution (in target georeferenced units) */
    double dfXRes;
    double dfYRes;

    /*! align the coordinates of the extent of the output file to the values of the
        GDALWarpAppOptions::dfXRes and GDALWarpAppOptions::dfYRes, such that the
        aligned extent includes the minimum extent. */
    int bTargetAlignedPixels;
    
    /*! set output file size in pixels and lines. If GDALWarpAppOptions::nForcePixels
        or GDALWarpAppOptions::nForceLines is set to 0, the other dimension will be
        guessed from the computed resolution. Note that GDALWarpAppOptions::nForcePixels and
        GDALWarpAppOptions::nForceLines cannot be used with GDALWarpAppOptions::dfXRes and
        GDALWarpAppOptions::dfYRes. */
    int nForcePixels;
    int nForceLines;
    
    /*! allow or suppress progress monitor and other non-error output */
    int bQuiet;

    /*! the progress function to use */
    GDALProgressFunc pfnProgress;
    
    /*! pointer to the progress data variable */
    void *pProgressData;

    /*! creates an output alpha band to identify nodata (unset/transparent) pixels
        when set to TRUE */
    int bEnableDstAlpha;

    int bEnableSrcAlpha;

    /*! output format. The default is GeoTIFF (GTiff). Use the short format name. */
    char *pszFormat;

    int bCreateOutput;

    /*! list of warp options. ("NAME1=VALUE1","NAME2=VALUE2",...). The
        GDALWarpOptions::papszWarpOptions docs show all options. */
    char **papszWarpOptions;

    double dfErrorThreshold;

    /*! the amount of memory (in megabytes) that the warp API is allowed
        to use for caching. */
    double dfWarpMemoryLimit;

    /*! list of create options for the output format driver. See format
        specific documentation for legal creation options for each format. */
    char **papszCreateOptions;

    /*! the data type of the output bands */
    GDALDataType eOutputType;

    /*! working pixel data type. The data type of pixels in the source
        image and destination image buffers. */
    GDALDataType eWorkingType;

    /*! the resampling method. Available methods are: near, bilinear,
        cubic, cubicspline, lanczos, average, mode, max, min, med,
        q1, q3 */
    GDALResampleAlg eResampleAlg;
    
    /*! nodata masking values for input bands (different values can be supplied
        for each band). ("value1 value2 ..."). Masked values will not be used
        in interpolation. Use a value of "None" to ignore intrinsic nodata
        settings on the source dataset. */
    char *pszSrcNodata;

    /*! nodata values for output bands (different values can be supplied for
        each band). ("value1 value2 ..."). New files will be initialized to
        this value and if possible the nodata value will be recorded in the
        output file. Use a value of "None" to ensure that nodata is not defined.
        If this argument is not used then nodata values will be copied from
        the source dataset. */
    char *pszDstNodata;

    /*! use multithreaded warping implementation. Multiple threads will be used
        to process chunks of image and perform input/output operation simultaneously. */
    int bMulti;

    /*! list of transformer options suitable to pass to GDALCreateGenImgProjTransformer2().
        ("NAME1=VALUE1","NAME2=VALUE2",...) */
    char **papszTO;

    /*! enable use of a blend cutline from the name OGR support pszCutlineDSName */
    char *pszCutlineDSName;

    /*! the named layer to be selected from the cutline datasource */
    char *pszCLayer;

    /*! restrict desired cutline features based on attribute query */
    char *pszCWHERE;

    /*! SQL query to select the cutline features instead of from a layer
        with pszCLayer */
    char *pszCSQL;

    /*! crop the extent of the target dataset to the extent of the cutline */
    int bCropToCutline;

    /*! copy dataset and band metadata will be copied from the first source dataset. Items that differ between
        source datasets will be set "*" (see GDALWarpAppOptions::pszMDConflictValue) */
    int bCopyMetadata;

    /*! copy band information from the first source dataset */
    int bCopyBandInfo;

    /*! value to set metadata items that conflict between source datasets (default is "*").
        Use "" to remove conflicting items. */
    char *pszMDConflictValue;

    /*! set the color interpretation of the bands of the target dataset from the source dataset */
    int bSetColorInterpretation;

    /*! overview level of source files to be used */
    int nOvLevel;
};

static CPLErr
LoadCutline( const char *pszCutlineDSName, const char *pszCLayer, 
             const char *pszCWHERE, const char *pszCSQL, 
             void **phCutlineRet );
static CPLErr
TransformCutlineToSource( GDALDatasetH hSrcDS, void *hCutline,
                          char ***ppapszWarpOptions, char **papszTO );

static GDALDatasetH 
GDALWarpCreateOutput( int nSrcCount, GDALDatasetH *pahSrcDS, const char *pszFilename, 
                      const char *pszFormat, char **papszTO,
                      char ***ppapszCreateOptions, GDALDataType eDT,
                      void ** phTransformArg,
                      int bSetColorInterpretation,
                      GDALWarpAppOptions *psOptions);

static void 
RemoveConflictingMetadata( GDALMajorObjectH hObj, char **papszMetadata,
                           const char *pszValueConflict );

#ifdef OGR_ENABLED

static double GetAverageSegmentLength(OGRGeometryH hGeom)
{
    if( hGeom == NULL )
        return 0;
    switch(wkbFlatten(OGR_G_GetGeometryType(hGeom)))
    {
        case wkbLineString:
        {
            if( OGR_G_GetPointCount(hGeom) == 0 )
                return 0;
            double dfSum = 0;
            for(int i=0;i<OGR_G_GetPointCount(hGeom)-1;i++)
            {
                double dfX1 = OGR_G_GetX(hGeom, i);
                double dfY1 = OGR_G_GetY(hGeom, i);
                double dfX2 = OGR_G_GetX(hGeom, i+1);
                double dfY2 = OGR_G_GetY(hGeom, i+1);
                double dfDX = dfX2 - dfX1;
                double dfDY = dfY2 - dfY1;
                dfSum += sqrt(dfDX * dfDX + dfDY * dfDY);
            }
            return dfSum / OGR_G_GetPointCount(hGeom);
        }

        case wkbPolygon:
        case wkbMultiPolygon:
        case wkbMultiLineString:
        case wkbGeometryCollection:
        {
            if( OGR_G_GetGeometryCount(hGeom) == 0 )
                return 0;
            double dfSum = 0;
            for(int i=0; i< OGR_G_GetGeometryCount(hGeom); i++)
            {
                dfSum += GetAverageSegmentLength(OGR_G_GetGeometryRef(hGeom, i));
            }
            return dfSum / OGR_G_GetGeometryCount(hGeom);
        }
        
        default:
            return 0;
    }
}

/************************************************************************/
/*                           CropToCutline()                            */
/************************************************************************/

static CPLErr CropToCutline( void* hCutline, char** papszTO, int nSrcCount, GDALDatasetH *pahSrcDS,
                           double& dfMinX, double& dfMinY, double& dfMaxX, double &dfMaxY )
{
    OGRGeometryH hCutlineGeom = OGR_G_Clone( (OGRGeometryH) hCutline );
    OGRSpatialReferenceH hCutlineSRS = OGR_G_GetSpatialReference( hCutlineGeom );
    const char *pszThisTargetSRS = CSLFetchNameValue( papszTO, "DST_SRS" );
    const char *pszThisSourceSRS = CSLFetchNameValue(papszTO, "SRC_SRS");
    OGRCoordinateTransformationH hCTCutlineToSrc = NULL;
    OGRCoordinateTransformationH hCTSrcToDst = NULL;
    OGRSpatialReferenceH hSrcSRS = NULL, hDstSRS = NULL;

    if( pszThisSourceSRS != NULL )
    {
        hSrcSRS = OSRNewSpatialReference(NULL);
        if( OSRImportFromWkt( hSrcSRS, (char **)&pszThisSourceSRS ) != OGRERR_NONE )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot compute bounding box of cutline.");
            OSRDestroySpatialReference(hSrcSRS);
            OGR_G_DestroyGeometry(hCutlineGeom);
            return CE_Failure;
        }
    }
    else if( nSrcCount == 0 || pahSrcDS[0] == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot compute bounding box of cutline.");
        OGR_G_DestroyGeometry(hCutlineGeom);
        return CE_Failure;
    }
    else
    {
        const char *pszProjection = NULL;

        if( GDALGetProjectionRef( pahSrcDS[0] ) != NULL
            && strlen(GDALGetProjectionRef( pahSrcDS[0] )) > 0 )
            pszProjection = GDALGetProjectionRef( pahSrcDS[0] );
        else if( GDALGetGCPProjection( pahSrcDS[0] ) != NULL )
            pszProjection = GDALGetGCPProjection( pahSrcDS[0] );

        if( pszProjection == NULL || pszProjection[0] == '\0' )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot compute bounding box of cutline.");
            OGR_G_DestroyGeometry(hCutlineGeom);
            return CE_Failure;
        }

        hSrcSRS = OSRNewSpatialReference(NULL);
        if( OSRImportFromWkt( hSrcSRS, (char **)&pszProjection ) != OGRERR_NONE )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot compute bounding box of cutline.");
            OSRDestroySpatialReference(hSrcSRS);
            OGR_G_DestroyGeometry(hCutlineGeom);
            return CE_Failure;
        }

    }

    if ( pszThisTargetSRS != NULL )
    {
        hDstSRS = OSRNewSpatialReference(NULL);
        if( OSRImportFromWkt( hDstSRS, (char **)&pszThisTargetSRS ) != OGRERR_NONE )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot compute bounding box of cutline.");
            OSRDestroySpatialReference(hSrcSRS);
            OSRDestroySpatialReference(hDstSRS);
            OGR_G_DestroyGeometry(hCutlineGeom);
            return CE_Failure;
        }
    }
    else
        hDstSRS = OSRClone(hSrcSRS);

    OGRSpatialReferenceH hCutlineOrTargetSRS = hCutlineSRS ? hCutlineSRS : hDstSRS;

    if( !OSRIsSame(hCutlineOrTargetSRS, hSrcSRS) )
        hCTCutlineToSrc = OCTNewCoordinateTransformation(hCutlineOrTargetSRS, hSrcSRS);
    if( !OSRIsSame(hSrcSRS, hDstSRS) )
        hCTSrcToDst = OCTNewCoordinateTransformation(hSrcSRS, hDstSRS);

    OSRDestroySpatialReference(hSrcSRS);
    hSrcSRS = NULL;

    OSRDestroySpatialReference(hDstSRS);
    hDstSRS = NULL;

    // Reproject cutline to target SRS, by doing intermediate vertex densifications
    // in source SRS.
    if( hCTSrcToDst != NULL || hCTCutlineToSrc != NULL )
    {
        OGREnvelope sLastEnvelope, sCurEnvelope;
        OGRGeometryH hTransformedGeom = NULL;
        OGRGeometryH hGeomInSrcSRS = OGR_G_Clone(hCutlineGeom);
        if( hCTCutlineToSrc != NULL )
            OGR_G_Transform( hGeomInSrcSRS, hCTCutlineToSrc );

        for(int nIter=0;nIter<10;nIter++)
        {
            if( hTransformedGeom != NULL )
                OGR_G_DestroyGeometry(hTransformedGeom);
            hTransformedGeom = OGR_G_Clone(hGeomInSrcSRS);
            if( hCTSrcToDst != NULL )
                OGR_G_Transform( hTransformedGeom, hCTSrcToDst );
            OGR_G_GetEnvelope(hTransformedGeom, &sCurEnvelope);
            if( nIter > 0 || hCTSrcToDst == NULL )
            {
                if( sCurEnvelope.MinX == sLastEnvelope.MinX &&
                    sCurEnvelope.MinY == sLastEnvelope.MinY &&
                    sCurEnvelope.MaxX == sLastEnvelope.MaxX &&
                    sCurEnvelope.MaxY == sLastEnvelope.MaxY )
                {
                    break;
                }
            }
            double dfAverageSegmentLength = GetAverageSegmentLength(hGeomInSrcSRS);
            OGR_G_Segmentize(hGeomInSrcSRS, dfAverageSegmentLength/4);

            memcpy(&sLastEnvelope, &sCurEnvelope, sizeof(OGREnvelope));
        }
        
        OGR_G_DestroyGeometry(hGeomInSrcSRS);

        OGR_G_DestroyGeometry(hCutlineGeom);
        hCutlineGeom = hTransformedGeom;
    }

    if( hCTCutlineToSrc) 
        OCTDestroyCoordinateTransformation(hCTCutlineToSrc);
    if( hCTSrcToDst )
        OCTDestroyCoordinateTransformation(hCTSrcToDst);

    OGREnvelope sEnvelope;
    OGR_G_GetEnvelope(hCutlineGeom, &sEnvelope);

    dfMinX = sEnvelope.MinX;
    dfMinY = sEnvelope.MinY;
    dfMaxX = sEnvelope.MaxX;
    dfMaxY = sEnvelope.MaxY;
    
    OGR_G_DestroyGeometry(hCutlineGeom);

    return CE_None;
}
#endif /* OGR_ENABLED */



/************************************************************************/
/*                          GDALWarpAppOptionsClone()                   */
/************************************************************************/

static
GDALWarpAppOptions* GDALWarpAppOptionsClone(const GDALWarpAppOptions *psOptionsIn)
{
    GDALWarpAppOptions* psOptions = (GDALWarpAppOptions*) CPLMalloc(sizeof(GDALWarpAppOptions));
    memcpy(psOptions, psOptionsIn, sizeof(GDALWarpAppOptions));
    psOptions->pszFormat = CPLStrdup(psOptionsIn->pszFormat);
    psOptions->papszCreateOptions = CSLDuplicate(psOptionsIn->papszCreateOptions);
    psOptions->papszWarpOptions = CSLDuplicate(psOptionsIn->papszWarpOptions);
    if( psOptionsIn->pszSrcNodata ) psOptions->pszSrcNodata = CPLStrdup(psOptionsIn->pszSrcNodata);
    if( psOptionsIn->pszDstNodata ) psOptions->pszDstNodata = CPLStrdup(psOptionsIn->pszDstNodata);
    psOptions->papszTO = CSLDuplicate(psOptionsIn->papszTO);
    if( psOptionsIn->pszCutlineDSName ) psOptions->pszCutlineDSName = CPLStrdup(psOptionsIn->pszCutlineDSName);
    if( psOptionsIn->pszCLayer ) psOptions->pszCLayer = CPLStrdup(psOptionsIn->pszCLayer);
    if( psOptionsIn->pszCWHERE ) psOptions->pszCWHERE = CPLStrdup(psOptionsIn->pszCWHERE);
    if( psOptionsIn->pszCSQL ) psOptions->pszCSQL = CPLStrdup(psOptionsIn->pszCSQL);
    if( psOptionsIn->pszMDConflictValue ) psOptions->pszMDConflictValue = CPLStrdup(psOptionsIn->pszMDConflictValue);
    if( psOptionsIn->pszTE_SRS ) psOptions->pszTE_SRS = CPLStrdup(psOptionsIn->pszTE_SRS);
    return psOptions;
}

/************************************************************************/
/*                             GDALWarp()                               */
/************************************************************************/

/**
 * Image reprojection and warping function.
 *
 * This is the equivalent of the <a href="gdal_warp.html">gdalwarp</a> utility.
 *
 * GDALWarpAppOptions* must be allocated and freed with GDALWarpAppOptionsNew()
 * and GDALWarpAppOptionsFree() respectively.
 * pszDest and hDstDS cannot be used at the same time.
 *
 * @param pszDest the destination dataset path or NULL.
 * @param hDstDS the destination dataset or NULL.
 * @param nSrcCount the number of input datasets.
 * @param pahSrcDS the list of input datasets.
 * @param psOptions the options struct returned by GDALWarpAppOptionsNew() or NULL.
 * @param pbUsageError the pointer to int variable to determine any usage error has occured
 * @return the output dataset (new dataset that must be closed using GDALClose(), or hDstDS is not NULL) or NULL in case of error
 *
 * @since GDAL 2.1
 */

GDALDatasetH GDALWarp( const char *pszDest, GDALDatasetH hDstDS, int nSrcCount,
                       GDALDatasetH *pahSrcDS, const GDALWarpAppOptions *psOptionsIn, int *pbUsageError )
{
    if( pszDest == NULL && hDstDS == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "pszDest == NULL && hDstDS == NULL");

        if(pbUsageError)
            *pbUsageError = TRUE;
        return NULL;
    }
    if( pszDest == NULL )
        pszDest = GDALGetDescription(hDstDS);

    int bMustCloseDstDSInCaseOfError = (hDstDS == NULL);
    GDALTransformerFunc pfnTransformer = NULL;
    void *hTransformArg = NULL;
    int bHasGotErr = FALSE;
    int i;
    int bVRT = FALSE;
    void *hCutline = NULL;

    GDALWarpAppOptions* psOptions =
        (psOptionsIn) ? GDALWarpAppOptionsClone(psOptionsIn) :
                        GDALWarpAppOptionsNew(NULL, NULL);

    if( EQUAL(psOptions->pszFormat,"VRT") )
    {
        if( hDstDS != NULL )
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                     "VRT output not compatible with existing dataset.");
            GDALWarpAppOptionsFree(psOptions);
            return NULL;
        }

        bVRT = TRUE;
    
        if( nSrcCount > 1 )
        {
            CPLError(CE_Warning, CPLE_AppDefined, "gdalwarp -of VRT just takes into account "
                            "the first source dataset.\nIf all source datasets "
                            "are in the same projection, try making a mosaic of\n"
                            "them with gdalbuildvrt, and use the resulting "
                            "VRT file as the input of\ngdalwarp -of VRT.");
        }
    }

/* -------------------------------------------------------------------- */
/*      Check that incompatible options are not used                    */
/* -------------------------------------------------------------------- */

    if ((psOptions->nForcePixels != 0 || psOptions->nForceLines != 0) && 
        (psOptions->dfXRes != 0 && psOptions->dfYRes != 0))
    {
        CPLError(CE_Failure, CPLE_IllegalArg, "-tr and -ts options cannot be used at the same time.");
        if(pbUsageError)
            *pbUsageError = TRUE;
        GDALWarpAppOptionsFree(psOptions);
        return NULL;
    }
    
    if (psOptions->bTargetAlignedPixels && psOptions->dfXRes == 0 && psOptions->dfYRes == 0)
    {
        CPLError(CE_Failure, CPLE_IllegalArg, "-tap option cannot be used without using -tr.");
        if(pbUsageError)
            *pbUsageError = TRUE;
        GDALWarpAppOptionsFree(psOptions);
        return NULL;
    }
    
    if( !psOptions->bQuiet && !(psOptions->dfMinX == 0.0 && psOptions->dfMinY == 0.0 && psOptions->dfMaxX == 0.0 && psOptions->dfMaxY == 0.0)  )
    {
        if( psOptions->dfMinX >= psOptions->dfMaxX )
            CPLError(CE_Warning, CPLE_AppDefined, "-ts values have minx >= maxx. This will result in a horizontally flipped image.");
        if( psOptions->dfMinY >= psOptions->dfMaxY )
            CPLError(CE_Warning, CPLE_AppDefined, "-ts values have miny >= maxy. This will result in a vertically flipped image.");
    }

    if( psOptions->dfErrorThreshold < 0 )
    {
        // By default, use approximate transformer unless RPC_DEM is specified
        if( CSLFetchNameValue(psOptions->papszWarpOptions, "RPC_DEM") != NULL )
            psOptions->dfErrorThreshold = 0.0;
        else
            psOptions->dfErrorThreshold = 0.125;
    }

/* -------------------------------------------------------------------- */
/*      -te_srs option                                                  */
/* -------------------------------------------------------------------- */
    if( psOptions->pszTE_SRS != NULL )
    {
        if( psOptions->dfMinX == 0.0 && psOptions->dfMinY == 0.0 && psOptions->dfMaxX == 0.0 && psOptions->dfMaxY == 0.0 )
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "-te_srs ignored since -te is not specified.");
        }
        else
        {
            OGRSpatialReference oSRSIn;
            oSRSIn.SetFromUserInput(psOptions->pszTE_SRS);
            OGRSpatialReference oSRSDS;
            int bOK = FALSE;
            if( CSLFetchNameValue( psOptions->papszTO, "DST_SRS" ) != NULL )
            {
                oSRSDS.SetFromUserInput( CSLFetchNameValue( psOptions->papszTO, "DST_SRS" ) );
                bOK = TRUE;
            }
            else if( CSLFetchNameValue( psOptions->papszTO, "SRC_SRS" ) != NULL )
            {
                oSRSDS.SetFromUserInput( CSLFetchNameValue( psOptions->papszTO, "SRC_SRS" ) );
                bOK = TRUE;
            }
            else
            {
                if( nSrcCount && pahSrcDS[0] && GDALGetProjectionRef(pahSrcDS[0]) && GDALGetProjectionRef(pahSrcDS[0])[0] )
                {
                    oSRSDS.SetFromUserInput( GDALGetProjectionRef(pahSrcDS[0]) );
                    bOK = TRUE;
                }
            }
            if( !bOK )
            {
                CPLError( CE_Failure, CPLE_AppDefined, "-te_srs ignored since none of -t_srs, -s_srs is specified or the input dataset has no projection.");
                GDALWarpAppOptionsFree(psOptions);
                return NULL;
            }
            if( !oSRSIn.IsSame(&oSRSDS) )
            {
                OGRCoordinateTransformation* poCT = OGRCreateCoordinateTransformation(&oSRSIn, &oSRSDS);
                if( !(poCT &&
                    poCT->Transform(1, &psOptions->dfMinX, &psOptions->dfMinY) &&
                    poCT->Transform(1, &psOptions->dfMaxX, &psOptions->dfMaxY)) )
                {
                    OGRCoordinateTransformation::DestroyCT(poCT);

                    CPLError( CE_Failure, CPLE_AppDefined, "-te_srs ignored since coordinate transformation failed.");
                    GDALWarpAppOptionsFree(psOptions);
                    return NULL;
                }
                delete poCT;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      If we have a cutline datasource read it and attach it in the    */
/*      warp options.                                                   */
/* -------------------------------------------------------------------- */
    if( psOptions->pszCutlineDSName != NULL )
    {
        CPLErr eError;
        eError = LoadCutline( psOptions->pszCutlineDSName, psOptions->pszCLayer, psOptions->pszCWHERE, psOptions->pszCSQL,
                     &hCutline );
        if(eError == CE_Failure)
        {
            GDALWarpAppOptionsFree(psOptions);
            return NULL;
        }
    }

#ifdef OGR_ENABLED
    if ( psOptions->bCropToCutline && hCutline != NULL )
    {
        CPLErr eError;
        eError = CropToCutline( hCutline, psOptions->papszTO, nSrcCount, pahSrcDS,
                       psOptions->dfMinX, psOptions->dfMinY, psOptions->dfMaxX, psOptions->dfMaxY );
        if(eError == CE_Failure)
        {
            GDALWarpAppOptionsFree(psOptions);
            OGR_G_DestroyGeometry( (OGRGeometryH) hCutline );
            return NULL;
        }
    }
#endif
    
/* -------------------------------------------------------------------- */
/*      If not, we need to create it.                                   */
/* -------------------------------------------------------------------- */
    void* hUniqueTransformArg = NULL;
    int bInitDestSetByUser = ( CSLFetchNameValue( psOptions->papszWarpOptions, "INIT_DEST" ) != NULL );

    const char* pszWarpThreads = CSLFetchNameValue(psOptions->papszWarpOptions, "NUM_THREADS");
    if( pszWarpThreads != NULL )
    {
        /* Used by TPS transformer to parallelize direct and inverse matrix computation */
        psOptions->papszTO = CSLSetNameValue(psOptions->papszTO, "NUM_THREADS", pszWarpThreads);
    }

    if( hDstDS == NULL )
    {
        hDstDS = GDALWarpCreateOutput( nSrcCount, pahSrcDS, pszDest,psOptions->pszFormat,
                                       psOptions->papszTO, &psOptions->papszCreateOptions, 
                                       psOptions->eOutputType, &hUniqueTransformArg,
                                       psOptions->bSetColorInterpretation,
                                       psOptions);
        if(hDstDS == NULL)
        {
            if( hUniqueTransformArg )
                GDALDestroyTransformer( hUniqueTransformArg );
            GDALWarpAppOptionsFree(psOptions);
#ifdef OGR_ENABLED
            if( hCutline != NULL )
                OGR_G_DestroyGeometry( (OGRGeometryH) hCutline );
#endif
            return NULL;
        }

        psOptions->bCreateOutput = TRUE;

        if( !bInitDestSetByUser )
        {
            if ( psOptions->pszDstNodata == NULL )
            {
                psOptions->papszWarpOptions = CSLSetNameValue(psOptions->papszWarpOptions,
                                                "INIT_DEST", "0");
            }
            else
            {
                psOptions->papszWarpOptions = CSLSetNameValue(psOptions->papszWarpOptions,
                                                "INIT_DEST", "NO_DATA" );
            }
        }

        CSLDestroy( psOptions->papszCreateOptions );
        psOptions->papszCreateOptions = NULL;
    }
 
    if( hDstDS == NULL )
    {
        GDALWarpAppOptionsFree(psOptions);
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Loop over all source files, processing each in turn.            */
/* -------------------------------------------------------------------- */
    int iSrc;

    for( iSrc = 0; iSrc < nSrcCount; iSrc++ )
    {
        GDALDatasetH hSrcDS;
       
/* -------------------------------------------------------------------- */
/*      Open this file.                                                 */
/* -------------------------------------------------------------------- */
        hSrcDS = pahSrcDS[iSrc];
        if( hSrcDS == NULL )
        {
            GDALWarpAppOptionsFree(psOptions);
#ifdef OGR_ENABLED
            if( hCutline != NULL )
                OGR_G_DestroyGeometry( (OGRGeometryH) hCutline );
#endif
            if( bMustCloseDstDSInCaseOfError )
                GDALClose(hDstDS);
            return NULL;
        }

/* -------------------------------------------------------------------- */
/*      Check that there's at least one raster band                     */
/* -------------------------------------------------------------------- */
        if ( GDALGetRasterCount(hSrcDS) == 0 )
        {     
            CPLError(CE_Failure, CPLE_AppDefined, "Input file %s has no raster bands.", GDALGetDescription(hSrcDS) );
            GDALWarpAppOptionsFree(psOptions);
#ifdef OGR_ENABLED
            if( hCutline != NULL )
                OGR_G_DestroyGeometry( (OGRGeometryH) hCutline );
#endif
            if( bMustCloseDstDSInCaseOfError )
                GDALClose(hDstDS);
            return NULL;
        }

        if( !psOptions->bQuiet )
            printf( "Processing input file %s.\n", GDALGetDescription(hSrcDS) );

/* -------------------------------------------------------------------- */
/*      Get the metadata of the first source DS and copy it to the      */
/*      destination DS. Copy Band-level metadata and other info, only   */
/*      if source and destination band count are equal. Any values that */
/*      conflict between source datasets are set to pszMDConflictValue. */
/* -------------------------------------------------------------------- */
        if ( psOptions->bCopyMetadata )
        {
            char **papszMetadata = NULL;
            const char *pszSrcInfo = NULL;
            const char *pszDstInfo = NULL;
            GDALRasterBandH hSrcBand = NULL;
            GDALRasterBandH hDstBand = NULL;

            /* copy metadata from first dataset */
            if ( iSrc == 0 )
            {
                CPLDebug("WARP", "Copying metadata from first source to destination dataset");
                /* copy dataset-level metadata */
                papszMetadata = GDALGetMetadata( hSrcDS, NULL );

                char** papszMetadataNew = NULL;
                for( int i = 0; papszMetadata != NULL && papszMetadata[i] != NULL; i++ )
                {
                    // Do not preserve NODATA_VALUES when the output includes an alpha band
                    if( psOptions->bEnableDstAlpha &&
                        EQUALN(papszMetadata[i], "NODATA_VALUES=", strlen("NODATA_VALUES=")) )
                    {
                        continue;
                    }

                    papszMetadataNew = CSLAddString(papszMetadataNew, papszMetadata[i]);
                }

                if ( CSLCount(papszMetadataNew) > 0 ) {
                    if ( GDALSetMetadata( hDstDS, papszMetadataNew, NULL ) != CE_None )
                         CPLError( CE_Warning, CPLE_AppDefined, 
                                  "error copying metadata to destination dataset." );
                }

                CSLDestroy(papszMetadataNew);

                /* copy band-level metadata and other info */
                if ( GDALGetRasterCount( hSrcDS ) == GDALGetRasterCount( hDstDS ) )              
                {
                    for ( int iBand = 0; iBand < GDALGetRasterCount( hSrcDS ); iBand++ )
                    {
                        hSrcBand = GDALGetRasterBand( hSrcDS, iBand + 1 );
                        hDstBand = GDALGetRasterBand( hDstDS, iBand + 1 );
                        /* copy metadata, except stats (#5319) */
                        papszMetadata = GDALGetMetadata( hSrcBand, NULL);              
                        if ( CSLCount(papszMetadata) > 0 )
                        {
                            //GDALSetMetadata( hDstBand, papszMetadata, NULL );       
                            char** papszMetadataNew = NULL;
                            for( int i = 0; papszMetadata != NULL && papszMetadata[i] != NULL; i++ )
                            {
                                if (strncmp(papszMetadata[i], "STATISTICS_", 11) != 0)
                                    papszMetadataNew = CSLAddString(papszMetadataNew, papszMetadata[i]);
                            }
                            GDALSetMetadata( hDstBand, papszMetadataNew, NULL );
                            CSLDestroy(papszMetadataNew);
                        }
                        /* copy other info (Description, Unit Type) - what else? */
                        if ( psOptions->bCopyBandInfo ) {
                            pszSrcInfo = GDALGetDescription( hSrcBand );
                            if(  pszSrcInfo != NULL && strlen(pszSrcInfo) > 0 )
                                GDALSetDescription( hDstBand, pszSrcInfo );  
                            pszSrcInfo = GDALGetRasterUnitType( hSrcBand );
                            if(  pszSrcInfo != NULL && strlen(pszSrcInfo) > 0 )
                                GDALSetRasterUnitType( hDstBand, pszSrcInfo );  
                        }
                    }
                }
            }
            /* remove metadata that conflicts between datasets */
            else 
            {
                CPLDebug("WARP", 
                         "Removing conflicting metadata from destination dataset (source #%d)", iSrc );
                /* remove conflicting dataset-level metadata */
                RemoveConflictingMetadata( hDstDS, GDALGetMetadata( hSrcDS, NULL ), psOptions->pszMDConflictValue );
                
                /* remove conflicting copy band-level metadata and other info */
                if ( GDALGetRasterCount( hSrcDS ) == GDALGetRasterCount( hDstDS ) )              
                {
                    for ( int iBand = 0; iBand < GDALGetRasterCount( hSrcDS ); iBand++ )
                    {
                        hSrcBand = GDALGetRasterBand( hSrcDS, iBand + 1 );
                        hDstBand = GDALGetRasterBand( hDstDS, iBand + 1 );
                        /* remove conflicting metadata */
                        RemoveConflictingMetadata( hDstBand, GDALGetMetadata( hSrcBand, NULL ), psOptions->pszMDConflictValue );
                        /* remove conflicting info */
                        if ( psOptions->bCopyBandInfo ) {
                            pszSrcInfo = GDALGetDescription( hSrcBand );
                            pszDstInfo = GDALGetDescription( hDstBand );
                            if( ! ( pszSrcInfo != NULL && strlen(pszSrcInfo) > 0  &&
                                    pszDstInfo != NULL && strlen(pszDstInfo) > 0  &&
                                    EQUAL( pszSrcInfo, pszDstInfo ) ) )
                                GDALSetDescription( hDstBand, "" );  
                            pszSrcInfo = GDALGetRasterUnitType( hSrcBand );
                            pszDstInfo = GDALGetRasterUnitType( hDstBand );
                            if( ! ( pszSrcInfo != NULL && strlen(pszSrcInfo) > 0  &&
                                    pszDstInfo != NULL && strlen(pszDstInfo) > 0  &&
                                    EQUAL( pszSrcInfo, pszDstInfo ) ) )
                                GDALSetRasterUnitType( hDstBand, "" );  
                        }
                    }
                }
            }          
        }

/* -------------------------------------------------------------------- */
/*      Warns if the file has a color table and something more          */
/*      complicated than nearest neighbour resampling is asked          */
/* -------------------------------------------------------------------- */
 
        if ( psOptions->eResampleAlg != GRA_NearestNeighbour &&
             psOptions->eResampleAlg != GRA_Mode &&
             GDALGetRasterColorTable(GDALGetRasterBand(hSrcDS, 1)) != NULL)
        {
            if( !psOptions->bQuiet )
                CPLError( CE_Warning, CPLE_AppDefined, "Input file %s has a color table, which will likely lead to "
                        "bad results when using a resampling method other than "
                        "nearest neighbour or mode. Converting the dataset prior to 24/32 bit "
                        "is advised.", GDALGetDescription(hSrcDS) );
        }

/* -------------------------------------------------------------------- */
/*      Do we have a source alpha band?                                 */
/* -------------------------------------------------------------------- */
        if( GDALGetRasterColorInterpretation( 
                GDALGetRasterBand(hSrcDS,GDALGetRasterCount(hSrcDS)) ) 
            == GCI_AlphaBand 
            && !psOptions->bEnableSrcAlpha )
        {
            psOptions->bEnableSrcAlpha = TRUE;
            if( !psOptions->bQuiet )
                printf( "Using band %d of source image as alpha.\n", 
                        GDALGetRasterCount(hSrcDS) );
        }

/* -------------------------------------------------------------------- */
/*      Create a transformation object from the source to               */
/*      destination coordinate system.                                  */
/* -------------------------------------------------------------------- */
        if (hUniqueTransformArg)
            hTransformArg = hUniqueTransformArg;
        else
            hTransformArg =
                GDALCreateGenImgProjTransformer2( hSrcDS, hDstDS, psOptions->papszTO );
        
        if( hTransformArg == NULL )
        {
            GDALWarpAppOptionsFree(psOptions);
#ifdef OGR_ENABLED
            if( hCutline != NULL )
                OGR_G_DestroyGeometry( (OGRGeometryH) hCutline );
#endif
            if( bMustCloseDstDSInCaseOfError )
                GDALClose(hDstDS);
            return NULL;
        }

        pfnTransformer = GDALGenImgProjTransform;

/* -------------------------------------------------------------------- */
/*      Determine if we must work with the full-resolution source       */
/*      dataset, or one of its overview level.                          */
/* -------------------------------------------------------------------- */
        GDALDataset* poSrcDS = (GDALDataset*) hSrcDS;
        GDALDataset* poSrcOvrDS = NULL;
        int nOvCount = poSrcDS->GetRasterBand(1)->GetOverviewCount();
        if( psOptions->nOvLevel <= -2 && nOvCount > 0 )
        {
            double adfSuggestedGeoTransform[6];
            double adfExtent[4];
            int    nPixels, nLines;
            /* Compute what the "natural" output resolution (in pixels) would be for this */
            /* input dataset */
            if( GDALSuggestedWarpOutput2(hSrcDS, pfnTransformer, hTransformArg,
                                         adfSuggestedGeoTransform, &nPixels, &nLines,
                                         adfExtent, 0) == CE_None)
            {
                double dfTargetRatio = 1.0 / adfSuggestedGeoTransform[1];
                if( dfTargetRatio > 1.0 )
                {
                    int iOvr;
                    for( iOvr = -1; iOvr < nOvCount-1; iOvr++ )
                    {
                        double dfOvrRatio = (iOvr < 0) ? 1.0 : (double)poSrcDS->GetRasterXSize() /
                            poSrcDS->GetRasterBand(1)->GetOverview(iOvr)->GetXSize();
                        double dfNextOvrRatio = (double)poSrcDS->GetRasterXSize() /
                            poSrcDS->GetRasterBand(1)->GetOverview(iOvr+1)->GetXSize();
                        if( dfOvrRatio < dfTargetRatio && dfNextOvrRatio > dfTargetRatio )
                            break;
                        if( fabs(dfOvrRatio - dfTargetRatio) < 1e-1 )
                            break;
                    }
                    iOvr += (psOptions->nOvLevel+2);
                    if( iOvr >= 0 )
                    {
                        CPLDebug("WARP", "Selecting overview level %d for %s",
                                 iOvr, GDALGetDescription(hSrcDS));
                        poSrcOvrDS = GDALCreateOverviewDataset( poSrcDS, iOvr, FALSE, FALSE );
                    }
                }
            }
        }
        else if( psOptions->nOvLevel >= 0 )
        {
            poSrcOvrDS = GDALCreateOverviewDataset( poSrcDS, psOptions->nOvLevel, TRUE, FALSE );
            if( poSrcOvrDS == NULL )
            {
                if( !psOptions->bQuiet )
                {
                    CPLError(CE_Warning, CPLE_AppDefined, "cannot get overview level %d for "
                            "dataset %s. Defaulting to level %d",
                            psOptions->nOvLevel, GDALGetDescription(hSrcDS), nOvCount - 1);
                }
                if( nOvCount > 0 )
                    poSrcOvrDS = GDALCreateOverviewDataset( poSrcDS, nOvCount - 1, FALSE, FALSE );
            }
            else
            {
                CPLDebug("WARP", "Selecting overview level %d for %s",
                         psOptions->nOvLevel, GDALGetDescription(hSrcDS));
            }
        }

        GDALDatasetH hWrkSrcDS = (poSrcOvrDS) ? (GDALDatasetH)poSrcOvrDS : hSrcDS;

        /* We need to recreate the transform when operating on an overview */
        if( poSrcOvrDS != NULL )
        {
            GDALDestroyGenImgProjTransformer( hTransformArg );
            hTransformArg =
                GDALCreateGenImgProjTransformer2( hWrkSrcDS, hDstDS, psOptions->papszTO );
        }

/* -------------------------------------------------------------------- */
/*      Warp the transformer with a linear approximator unless the      */
/*      acceptable error is zero.                                       */
/* -------------------------------------------------------------------- */
        if( psOptions->dfErrorThreshold != 0.0 )
        {
            hTransformArg =
                GDALCreateApproxTransformer( GDALGenImgProjTransform, 
                                             hTransformArg, psOptions->dfErrorThreshold);
            pfnTransformer = GDALApproxTransform;
            GDALApproxTransformerOwnsSubtransformer(hTransformArg, TRUE);
        }

/* -------------------------------------------------------------------- */
/*      Clear temporary INIT_DEST settings after the first image.       */
/* -------------------------------------------------------------------- */
        if( psOptions->bCreateOutput && iSrc == 1 )
            psOptions->papszWarpOptions = CSLSetNameValue( psOptions->papszWarpOptions, 
                                                "INIT_DEST", NULL );

/* -------------------------------------------------------------------- */
/*      Setup warp options.                                             */
/* -------------------------------------------------------------------- */
        GDALWarpOptions *psWO = GDALCreateWarpOptions();

        psWO->papszWarpOptions = CSLDuplicate(psOptions->papszWarpOptions);
        psWO->eWorkingDataType = psOptions->eWorkingType;
        psWO->eResampleAlg = psOptions->eResampleAlg;

        psWO->hSrcDS = hWrkSrcDS;
        psWO->hDstDS = hDstDS;

        psWO->pfnTransformer = pfnTransformer;
        psWO->pTransformerArg = hTransformArg;

        psWO->pfnProgress = psOptions->pfnProgress;
        psWO->pProgressArg = psOptions->pProgressData;

        if( psOptions->dfWarpMemoryLimit != 0.0 )
            psWO->dfWarpMemoryLimit = psOptions->dfWarpMemoryLimit;

/* -------------------------------------------------------------------- */
/*      Setup band mapping.                                             */
/* -------------------------------------------------------------------- */
        if( psOptions->bEnableSrcAlpha )
            psWO->nBandCount = GDALGetRasterCount(hWrkSrcDS) - 1;
        else
            psWO->nBandCount = GDALGetRasterCount(hWrkSrcDS);

        psWO->panSrcBands = (int *) CPLMalloc(psWO->nBandCount*sizeof(int));
        psWO->panDstBands = (int *) CPLMalloc(psWO->nBandCount*sizeof(int));

        for( i = 0; i < psWO->nBandCount; i++ )
        {
            psWO->panSrcBands[i] = i+1;
            psWO->panDstBands[i] = i+1;
        }

/* -------------------------------------------------------------------- */
/*      Setup alpha bands used if any.                                  */
/* -------------------------------------------------------------------- */
        if( psOptions->bEnableSrcAlpha )
            psWO->nSrcAlphaBand = GDALGetRasterCount(hWrkSrcDS);

        if( !psOptions->bEnableDstAlpha 
            && GDALGetRasterCount(hDstDS) == psWO->nBandCount+1 
            && GDALGetRasterColorInterpretation( 
                GDALGetRasterBand(hDstDS,GDALGetRasterCount(hDstDS))) 
            == GCI_AlphaBand )
        {
            if( !psOptions->bQuiet )
                printf( "Using band %d of destination image as alpha.\n", 
                        GDALGetRasterCount(hDstDS) );
                
            psOptions->bEnableDstAlpha = TRUE;
        }

        if( psOptions->bEnableDstAlpha )
            psWO->nDstAlphaBand = GDALGetRasterCount(hDstDS);

/* -------------------------------------------------------------------- */
/*      Setup NODATA options.                                           */
/* -------------------------------------------------------------------- */
        if( psOptions->pszSrcNodata != NULL && !EQUAL(psOptions->pszSrcNodata,"none") )
        {
            char **papszTokens = CSLTokenizeString( psOptions->pszSrcNodata );
            int  nTokenCount = CSLCount(papszTokens);

            psWO->padfSrcNoDataReal = (double *) 
                CPLMalloc(psWO->nBandCount*sizeof(double));
            psWO->padfSrcNoDataImag = (double *) 
                CPLMalloc(psWO->nBandCount*sizeof(double));

            for( i = 0; i < psWO->nBandCount; i++ )
            {
                if( i < nTokenCount )
                {
                    CPLStringToComplex( papszTokens[i], 
                                        psWO->padfSrcNoDataReal + i,
                                        psWO->padfSrcNoDataImag + i );
                }
                else
                {
                    psWO->padfSrcNoDataReal[i] = psWO->padfSrcNoDataReal[i-1];
                    psWO->padfSrcNoDataImag[i] = psWO->padfSrcNoDataImag[i-1];
                }
            }

            CSLDestroy( papszTokens );

            psWO->papszWarpOptions = CSLSetNameValue(psWO->papszWarpOptions,
                                               "UNIFIED_SRC_NODATA", "YES" );
        }

/* -------------------------------------------------------------------- */
/*      If -srcnodata was not specified, but the data has nodata        */
/*      values, use them.                                               */
/* -------------------------------------------------------------------- */
        if( psOptions->pszSrcNodata == NULL )
        {
            int bHaveNodata = FALSE;
            double dfReal = 0.0;

            for( i = 0; !bHaveNodata && i < psWO->nBandCount; i++ )
            {
                GDALRasterBandH hBand = GDALGetRasterBand( hWrkSrcDS, i+1 );
                dfReal = GDALGetRasterNoDataValue( hBand, &bHaveNodata );
            }

            if( bHaveNodata )
            {
                if( !psOptions->bQuiet )
                {
                    if (CPLIsNan(dfReal))
                        printf( "Using internal nodata values (e.g. nan) for image %s.\n",
                                GDALGetDescription(hSrcDS) );
                    else
                        printf( "Using internal nodata values (e.g. %g) for image %s.\n",
                                dfReal, GDALGetDescription(hSrcDS) );
                }
                psWO->padfSrcNoDataReal = (double *) 
                    CPLMalloc(psWO->nBandCount*sizeof(double));
                psWO->padfSrcNoDataImag = (double *) 
                    CPLMalloc(psWO->nBandCount*sizeof(double));
                
                for( i = 0; i < psWO->nBandCount; i++ )
                {
                    GDALRasterBandH hBand = GDALGetRasterBand( hWrkSrcDS, i+1 );

                    dfReal = GDALGetRasterNoDataValue( hBand, &bHaveNodata );

                    if( bHaveNodata )
                    {
                        psWO->padfSrcNoDataReal[i] = dfReal;
                        psWO->padfSrcNoDataImag[i] = 0.0;
                    }
                    else
                    {
                        psWO->padfSrcNoDataReal[i] = -123456.789;
                        psWO->padfSrcNoDataImag[i] = 0.0;
                    }
                }
            }
        }

/* -------------------------------------------------------------------- */
/*      If the output dataset was created, and we have a destination    */
/*      nodata value, go through marking the bands with the information.*/
/* -------------------------------------------------------------------- */
        if( psOptions->pszDstNodata != NULL && !EQUAL(psOptions->pszDstNodata,"none") )
        {
            char **papszTokens = CSLTokenizeString( psOptions->pszDstNodata );
            int  nTokenCount = CSLCount(papszTokens);
            int bDstNoDataNone = TRUE;

            psWO->padfDstNoDataReal = (double *) 
                CPLMalloc(psWO->nBandCount*sizeof(double));
            psWO->padfDstNoDataImag = (double *) 
                CPLMalloc(psWO->nBandCount*sizeof(double));

            for( i = 0; i < psWO->nBandCount; i++ )
            {
                psWO->padfDstNoDataReal[i] = -1.1e20;
                psWO->padfDstNoDataImag[i] = 0.0;

                if( i < nTokenCount )
                {
                    if ( papszTokens[i] != NULL && EQUAL(papszTokens[i],"none") )
                    {
                        CPLDebug( "WARP", "dstnodata of band %d not set", i );
                        bDstNoDataNone = TRUE;
                        continue;
                    }
                    else if ( papszTokens[i] == NULL ) // this shouldn't happen, but just in case
                    {
                        CPLError( CE_Failure, CPLE_AppDefined, "Error parsing dstnodata arg #%d", i );
                        bDstNoDataNone = TRUE;
                        continue;
                    }
                    CPLStringToComplex( papszTokens[i], 
                                        psWO->padfDstNoDataReal + i,
                                        psWO->padfDstNoDataImag + i );
                    bDstNoDataNone = FALSE;
                    CPLDebug( "WARP", "dstnodata of band %d set to %f", i, psWO->padfDstNoDataReal[i] );
                }
                else
                {
                    if ( ! bDstNoDataNone )
                    {                    
                        psWO->padfDstNoDataReal[i] = psWO->padfDstNoDataReal[i-1];
                        psWO->padfDstNoDataImag[i] = psWO->padfDstNoDataImag[i-1];
                        CPLDebug( "WARP", "dstnodata of band %d set from previous band", i );
                    }
                    else
                    {
                        CPLDebug( "WARP", "dstnodata value of band %d not set", i );
                        continue;
                    }
                }
                
                GDALRasterBandH hBand = GDALGetRasterBand( hDstDS, i+1 );
                int bClamped = FALSE, bRounded = FALSE;

#define CLAMP(val,type,minval,maxval) \
    do { if (val < minval) { bClamped = TRUE; val = minval; } \
    else if (val > maxval) { bClamped = TRUE; val = maxval; } \
    else if (val != (type)val) { bRounded = TRUE; val = (type)(val + 0.5); } } \
    while(0)

                switch(GDALGetRasterDataType(hBand))
                {
                    case GDT_Byte:
                        CLAMP(psWO->padfDstNoDataReal[i], GByte,
                              0.0, 255.0);
                        break;
                    case GDT_Int16:
                        CLAMP(psWO->padfDstNoDataReal[i], GInt16,
                              -32768.0, 32767.0);
                        break;
                    case GDT_UInt16:
                        CLAMP(psWO->padfDstNoDataReal[i], GUInt16,
                              0.0, 65535.0);
                        break;
                    case GDT_Int32:
                        CLAMP(psWO->padfDstNoDataReal[i], GInt32,
                              -2147483648.0, 2147483647.0);
                        break;
                    case GDT_UInt32:
                        CLAMP(psWO->padfDstNoDataReal[i], GUInt32,
                              0.0, 4294967295.0);
                        break;
                    default:
                        break;
                }
                    
                if (bClamped)
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "for band %d, destination nodata value has been clamped "
                           "to %.0f, the original value being out of range.",
                           i + 1, psWO->padfDstNoDataReal[i]);
                }
                else if(bRounded)
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "for band %d, destination nodata value has been rounded "
                           "to %.0f, %s being an integer datatype.",
                           i + 1, psWO->padfDstNoDataReal[i],
                           GDALGetDataTypeName(GDALGetRasterDataType(hBand)));
                }

                if( psOptions->bCreateOutput )
                {
                    GDALSetRasterNoDataValue( 
                        GDALGetRasterBand( hDstDS, psWO->panDstBands[i] ), 
                        psWO->padfDstNoDataReal[i] );
                }
            }

            CSLDestroy( papszTokens );
        }

        /* else try to fill dstNoData from source bands */
        if ( psOptions->pszDstNodata == NULL && psWO->padfSrcNoDataReal != NULL )
        {
            psWO->padfDstNoDataReal = (double *) 
                CPLMalloc(psWO->nBandCount*sizeof(double));
            psWO->padfDstNoDataImag = (double *) 
                CPLMalloc(psWO->nBandCount*sizeof(double));

            if( !psOptions->bQuiet )
                printf( "Copying nodata values from source %s to destination %s.\n",
                        GDALGetDescription(hSrcDS), pszDest );

            for( i = 0; i < psWO->nBandCount; i++ )
            {
                int bHaveNodata = FALSE;
                
                GDALRasterBandH hBand = GDALGetRasterBand( hWrkSrcDS, i+1 );
                GDALGetRasterNoDataValue( hBand, &bHaveNodata );

                CPLDebug("WARP", "band=%d bHaveNodata=%d", i, bHaveNodata);
                if( bHaveNodata )
                {
                    psWO->padfDstNoDataReal[i] = psWO->padfSrcNoDataReal[i];
                    psWO->padfDstNoDataImag[i] = psWO->padfSrcNoDataImag[i];
                    CPLDebug("WARP", "srcNoData=%f dstNoData=%f", 
                             psWO->padfSrcNoDataReal[i], psWO->padfDstNoDataReal[i] );
                }

                if( psOptions->bCreateOutput )
                {
                    CPLDebug("WARP", "calling GDALSetRasterNoDataValue() for band#%d", i );
                    GDALSetRasterNoDataValue( 
                        GDALGetRasterBand( hDstDS, psWO->panDstBands[i] ), 
                        psWO->padfDstNoDataReal[i] );
                }
            }

            if( psOptions->bCreateOutput && !bInitDestSetByUser && iSrc == 0 )
            {
                /* As we didn't know at the beginning if there was source nodata */
                /* we have initialized INIT_DEST=0. Override this with NO_DATA now */
                psWO->papszWarpOptions = CSLSetNameValue(psWO->papszWarpOptions,
                                                   "INIT_DEST", "NO_DATA" );
            }
        }

/* -------------------------------------------------------------------- */
/*      If we have a cutline, transform it into the source              */
/*      pixel/line coordinate system and insert into warp options.      */
/* -------------------------------------------------------------------- */
        if( hCutline != NULL )
        {
            CPLErr eError;
            eError = TransformCutlineToSource( hWrkSrcDS, hCutline, 
                                      &(psWO->papszWarpOptions), 
                                      psOptions->papszTO );
            if(eError == CE_Failure)
            {
                GDALWarpAppOptionsFree(psOptions);
#ifdef OGR_ENABLED
                if( hCutline != NULL )
                    OGR_G_DestroyGeometry( (OGRGeometryH) hCutline );
#endif
                if( bMustCloseDstDSInCaseOfError )
                    GDALClose(hDstDS);
                return NULL;
            }
        }

/* -------------------------------------------------------------------- */
/*      If we are producing VRT output, then just initialize it with    */
/*      the warp options and write out now rather than proceeding       */
/*      with the operations.                                            */
/* -------------------------------------------------------------------- */
        if( bVRT )
        {
            GDALSetMetadataItem(hDstDS, "SrcOvrLevel", CPLSPrintf("%d", psOptions->nOvLevel), NULL);
            if( GDALInitializeWarpedVRT( hDstDS, psWO ) != CE_None )
            {
                GDALWarpAppOptionsFree(psOptions);
#ifdef OGR_ENABLED
                if( hCutline != NULL )
                    OGR_G_DestroyGeometry( (OGRGeometryH) hCutline );
#endif
                if( bMustCloseDstDSInCaseOfError )
                    GDALClose(hDstDS);
                return NULL;
            }

            // We need to close it before destroying poSrcOvrDS and warping options
            CPLString osDstFilename(GDALGetDescription(hDstDS));
            GDALClose(hDstDS);

            if( poSrcOvrDS )
                delete poSrcOvrDS;

            GDALDestroyWarpOptions( psWO );
        
            GDALWarpAppOptionsFree(psOptions);
            return GDALOpen(osDstFilename, GA_Update);
        }

/* -------------------------------------------------------------------- */
/*      Initialize and execute the warp.                                */
/* -------------------------------------------------------------------- */
        GDALWarpOperation oWO;

        if( oWO.Initialize( psWO ) == CE_None )
        {
            CPLErr eErr;
            if( psOptions->bMulti )
                eErr = oWO.ChunkAndWarpMulti( 0, 0, 
                                       GDALGetRasterXSize( hDstDS ),
                                       GDALGetRasterYSize( hDstDS ) );
            else
                eErr = oWO.ChunkAndWarpImage( 0, 0, 
                                       GDALGetRasterXSize( hDstDS ),
                                       GDALGetRasterYSize( hDstDS ) );
            if (eErr != CE_None)
                bHasGotErr = TRUE;
        }

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
        if( hTransformArg != NULL )
            GDALDestroyTransformer( hTransformArg );

        GDALDestroyWarpOptions( psWO );

        if( poSrcOvrDS )
            delete poSrcOvrDS;
    }

/* -------------------------------------------------------------------- */
/*      Final Cleanup.                                                  */
/* -------------------------------------------------------------------- */
    CPLErrorReset();
    GDALFlushCache( hDstDS );
    if( CPLGetLastErrorType() == CE_Failure )
        bHasGotErr = TRUE;

#ifdef OGR_ENABLED
    if( hCutline != NULL )
        OGR_G_DestroyGeometry( (OGRGeometryH) hCutline );
#endif

    GDALWarpAppOptionsFree(psOptions);
    return (bHasGotErr) ? NULL : hDstDS;
}

/************************************************************************/
/*                            LoadCutline()                             */
/*                                                                      */
/*      Load blend cutline from OGR datasource.                         */
/************************************************************************/

static CPLErr
LoadCutline( const char *pszCutlineDSName, const char *pszCLayer, 
             const char *pszCWHERE, const char *pszCSQL, 
             void **phCutlineRet )

{
#ifndef OGR_ENABLED
    CPLError( CE_Failure, CPLE_AppDefined, 
              "Request to load a cutline failed, this build does not support OGR features." );
    return CE_Failure;
#else // def OGR_ENABLED
    OGRRegisterAll();

/* -------------------------------------------------------------------- */
/*      Open source vector dataset.                                     */
/* -------------------------------------------------------------------- */
    OGRDataSourceH hSrcDS;

    hSrcDS = OGROpen( pszCutlineDSName, FALSE, NULL );
    if( hSrcDS == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "Cannot open %s.", pszCutlineDSName);
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Get the source layer                                            */
/* -------------------------------------------------------------------- */
    OGRLayerH hLayer = NULL;

    if( pszCSQL != NULL )
        hLayer = OGR_DS_ExecuteSQL( hSrcDS, pszCSQL, NULL, NULL ); 
    else if( pszCLayer != NULL )
        hLayer = OGR_DS_GetLayerByName( hSrcDS, pszCLayer );
    else
        hLayer = OGR_DS_GetLayer( hSrcDS, 0 );

    if( hLayer == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "Failed to identify source layer from datasource." );
        OGR_DS_Destroy( hSrcDS );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Apply WHERE clause if there is one.                             */
/* -------------------------------------------------------------------- */
    if( pszCWHERE != NULL )
        OGR_L_SetAttributeFilter( hLayer, pszCWHERE );

/* -------------------------------------------------------------------- */
/*      Collect the geometries from this layer, and build list of       */
/*      burn values.                                                    */
/* -------------------------------------------------------------------- */
    OGRFeatureH hFeat;
    OGRGeometryH hMultiPolygon = OGR_G_CreateGeometry( wkbMultiPolygon );

    OGR_L_ResetReading( hLayer );
    
    while( (hFeat = OGR_L_GetNextFeature( hLayer )) != NULL )
    {
        OGRGeometryH hGeom = OGR_F_GetGeometryRef(hFeat);

        if( hGeom == NULL )
        {
            CPLError( CE_Failure, CPLE_AppDefined, "Cutline feature without a geometry." );
            OGR_F_Destroy( hFeat );
            goto error;
        }

        OGRwkbGeometryType eType = wkbFlatten(OGR_G_GetGeometryType( hGeom ));

        if( eType == wkbPolygon )
            OGR_G_AddGeometry( hMultiPolygon, hGeom );
        else if( eType == wkbMultiPolygon )
        {
            int iGeom;

            for( iGeom = 0; iGeom < OGR_G_GetGeometryCount( hGeom ); iGeom++ )
            {
                OGR_G_AddGeometry( hMultiPolygon, 
                                   OGR_G_GetGeometryRef(hGeom,iGeom) );
            }
        }
        else
        {
            CPLError( CE_Failure, CPLE_AppDefined, "Cutline not of polygon type." );
            OGR_F_Destroy( hFeat );
            goto error;
        }

        OGR_F_Destroy( hFeat );
    }

    if( OGR_G_GetGeometryCount( hMultiPolygon ) == 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "Did not get any cutline features." );
        goto error;
    }

/* -------------------------------------------------------------------- */
/*      Ensure the coordinate system gets set on the geometry.          */
/* -------------------------------------------------------------------- */
    OGR_G_AssignSpatialReference(
        hMultiPolygon, OGR_L_GetSpatialRef(hLayer) );

    *phCutlineRet = (void *) hMultiPolygon;

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    if( pszCSQL != NULL )
        OGR_DS_ReleaseResultSet( hSrcDS, hLayer );

    OGR_DS_Destroy( hSrcDS );

    return CE_None;

error:
    OGR_G_DestroyGeometry(hMultiPolygon);
    if( pszCSQL != NULL )
        OGR_DS_ReleaseResultSet( hSrcDS, hLayer );
    OGR_DS_Destroy( hSrcDS );
    return CE_Failure;
#endif
}

/************************************************************************/
/*                        GDALWarpCreateOutput()                        */
/*                                                                      */
/*      Create the output file based on various commandline options,    */
/*      and the input file.                                             */
/*      If there's just one source file, then *phTransformArg will be   */
/*      set in order them to be reused by main function. This saves     */
/*      transform recomputation, which can be expensive in the -tps case*/
/************************************************************************/

static GDALDatasetH 
GDALWarpCreateOutput( int nSrcCount, GDALDatasetH *pahSrcDS, const char *pszFilename, 
                      const char *pszFormat, char **papszTO, 
                      char ***ppapszCreateOptions, GDALDataType eDT,
                      void ** phTransformArg,
                      int bSetColorInterpretation,
                      GDALWarpAppOptions *psOptions)


{
    GDALDriverH hDriver;
    GDALDatasetH hDstDS;
    void *hTransformArg;
    GDALColorTableH hCT = NULL;
    double dfWrkMinX=0, dfWrkMaxX=0, dfWrkMinY=0, dfWrkMaxY=0;
    double dfWrkResX=0, dfWrkResY=0;
    int nDstBandCount = 0;
    std::vector<GDALColorInterp> apeColorInterpretations;
    int bVRT = FALSE;

    if( EQUAL(pszFormat,"VRT") )
        bVRT = TRUE;

    /* If (-ts and -te) or (-tr and -te) are specified, we don't need to compute the suggested output extent */
    int    bNeedsSuggestedWarpOutput = 
                  !( ((psOptions->nForcePixels != 0 && psOptions->nForceLines != 0) || (psOptions->dfXRes != 0 && psOptions->dfYRes != 0)) &&
                     !(psOptions->dfMinX == 0.0 && psOptions->dfMinY == 0.0 && psOptions->dfMaxX == 0.0 && psOptions->dfMaxY == 0.0) );

    *phTransformArg = NULL;

/* -------------------------------------------------------------------- */
/*      Find the output driver.                                         */
/* -------------------------------------------------------------------- */
    hDriver = GDALGetDriverByName( pszFormat );
    if( hDriver == NULL 
        || GDALGetMetadataItem( hDriver, GDAL_DCAP_CREATE, NULL ) == NULL )
    {
        int iDr;
        
        printf( "Output driver `%s' not recognised or does not support\n", 
                pszFormat );
        printf( "direct output file creation.  The following format drivers are configured\n"
                "and support direct output:\n" );

        for( iDr = 0; iDr < GDALGetDriverCount(); iDr++ )
        {
            GDALDriverH hDriver = GDALGetDriver(iDr);

            if( GDALGetMetadataItem( hDriver, GDAL_DCAP_RASTER, NULL) != NULL &&
                GDALGetMetadataItem( hDriver, GDAL_DCAP_CREATE, NULL) != NULL )
            {
                printf( "  %s: %s\n",
                        GDALGetDriverShortName( hDriver  ),
                        GDALGetDriverLongName( hDriver ) );
            }
        }
        printf( "\n" );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      For virtual output files, we have to set a special subclass     */
/*      of dataset to create.                                           */
/* -------------------------------------------------------------------- */
    if( bVRT )
        *ppapszCreateOptions = 
            CSLSetNameValue( *ppapszCreateOptions, "SUBCLASS", 
                             "VRTWarpedDataset" );

/* -------------------------------------------------------------------- */
/*      Loop over all input files to collect extents.                   */
/* -------------------------------------------------------------------- */
    int     iSrc;
    CPLString osThisTargetSRS;
    {
        const char *pszThisTargetSRS = CSLFetchNameValue( papszTO, "DST_SRS" );
        if( pszThisTargetSRS != NULL )
            osThisTargetSRS = pszThisTargetSRS;
    }

    for( iSrc = 0; iSrc < nSrcCount; iSrc++ )
    {
        const char *pszThisSourceSRS = CSLFetchNameValue(papszTO,"SRC_SRS");

        if( pahSrcDS[iSrc] == NULL )
        {
            if( hCT != NULL )
                GDALDestroyColorTable( hCT );
            return NULL;
        }

/* -------------------------------------------------------------------- */
/*      Check that there's at least one raster band                     */
/* -------------------------------------------------------------------- */
        if ( GDALGetRasterCount(pahSrcDS[iSrc]) == 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Input file %s has no raster bands.", GDALGetDescription(pahSrcDS[iSrc]) );
            if( hCT != NULL )
                GDALDestroyColorTable( hCT );
            return NULL;
        }

        if( eDT == GDT_Unknown )
            eDT = GDALGetRasterDataType(GDALGetRasterBand(pahSrcDS[iSrc],1));

/* -------------------------------------------------------------------- */
/*      If we are processing the first file, and it has a color         */
/*      table, then we will copy it to the destination file.            */
/* -------------------------------------------------------------------- */
        if( iSrc == 0 )
        {
            nDstBandCount = GDALGetRasterCount(pahSrcDS[iSrc]);
            hCT = GDALGetRasterColorTable( GDALGetRasterBand(pahSrcDS[iSrc],1) );
            if( hCT != NULL )
            {
                hCT = GDALCloneColorTable( hCT );
                if( !psOptions->bQuiet )
                    printf( "Copying color table from %s to new file.\n", 
                            GDALGetDescription(pahSrcDS[iSrc]) );
            }

            for(int iBand = 0; iBand < nDstBandCount; iBand++)
            {
                apeColorInterpretations.push_back(
                    GDALGetRasterColorInterpretation(GDALGetRasterBand(pahSrcDS[iSrc],iBand+1)) );
            }
        }

/* -------------------------------------------------------------------- */
/*      Get the sourcesrs from the dataset, if not set already.         */
/* -------------------------------------------------------------------- */
        if( pszThisSourceSRS == NULL )
        {
            const char *pszMethod = CSLFetchNameValue( papszTO, "METHOD" );

            if( GDALGetProjectionRef( pahSrcDS[iSrc] ) != NULL 
                && strlen(GDALGetProjectionRef( pahSrcDS[iSrc] )) > 0
                && (pszMethod == NULL || EQUAL(pszMethod,"GEOTRANSFORM")) )
                pszThisSourceSRS = GDALGetProjectionRef( pahSrcDS[iSrc] );
            
            else if( GDALGetGCPProjection( pahSrcDS[iSrc] ) != NULL
                     && strlen(GDALGetGCPProjection(pahSrcDS[iSrc])) > 0 
                     && GDALGetGCPCount( pahSrcDS[iSrc] ) > 1 
                     && (pszMethod == NULL || EQUALN(pszMethod,"GCP_",4)) )
                pszThisSourceSRS = GDALGetGCPProjection( pahSrcDS[iSrc] );
            else if( pszMethod != NULL && EQUAL(pszMethod,"RPC") )
                pszThisSourceSRS = SRS_WKT_WGS84;
            else
                pszThisSourceSRS = "";
        }

        if( osThisTargetSRS.size() == 0 )
            osThisTargetSRS = pszThisSourceSRS;
        
/* -------------------------------------------------------------------- */
/*      Create a transformation object from the source to               */
/*      destination coordinate system.                                  */
/* -------------------------------------------------------------------- */
        hTransformArg = 
            GDALCreateGenImgProjTransformer2( pahSrcDS[iSrc], NULL, papszTO );
        
        if( hTransformArg == NULL )
        {
            if( hCT != NULL )
                GDALDestroyColorTable( hCT );
            return NULL;
        }
        
        GDALTransformerInfo* psInfo = (GDALTransformerInfo*)hTransformArg;

/* -------------------------------------------------------------------- */
/*      Get approximate output definition.                              */
/* -------------------------------------------------------------------- */
        if( bNeedsSuggestedWarpOutput )
        {
            double adfThisGeoTransform[6];
            double adfExtent[4];
            int    nThisPixels, nThisLines;

            if ( GDALSuggestedWarpOutput2( pahSrcDS[iSrc], 
                                        psInfo->pfnTransform, hTransformArg, 
                                        adfThisGeoTransform, 
                                        &nThisPixels, &nThisLines, 
                                        adfExtent, 0 ) != CE_None )
            {
                if( hCT != NULL )
                    GDALDestroyColorTable( hCT );
                GDALDestroyGenImgProjTransformer( hTransformArg );
                return NULL;
            }
            
            if ( CPLGetConfigOption( "CHECK_WITH_INVERT_PROJ", NULL ) == NULL )
            {
                double MinX = adfExtent[0];
                double MaxX = adfExtent[2];
                double MaxY = adfExtent[3];
                double MinY = adfExtent[1];
                int bSuccess = TRUE;
                
                /* Check that the the edges of the target image are in the validity area */
                /* of the target projection */
    #define N_STEPS 20
                int i,j;
                for(i=0;i<=N_STEPS && bSuccess;i++)
                {
                    for(j=0;j<=N_STEPS && bSuccess;j++)
                    {
                        double dfRatioI = i * 1.0 / N_STEPS;
                        double dfRatioJ = j * 1.0 / N_STEPS;
                        double expected_x = (1 - dfRatioI) * MinX + dfRatioI * MaxX;
                        double expected_y = (1 - dfRatioJ) * MinY + dfRatioJ * MaxY;
                        double x = expected_x;
                        double y = expected_y;
                        double z = 0;
                        /* Target SRS coordinates to source image pixel coordinates */
                        if (!psInfo->pfnTransform(hTransformArg, TRUE, 1, &x, &y, &z, &bSuccess) || !bSuccess)
                            bSuccess = FALSE;
                        /* Source image pixel coordinates to target SRS coordinates */
                        if (!psInfo->pfnTransform(hTransformArg, FALSE, 1, &x, &y, &z, &bSuccess) || !bSuccess)
                            bSuccess = FALSE;
                        if (fabs(x - expected_x) > (MaxX - MinX) / nThisPixels ||
                            fabs(y - expected_y) > (MaxY - MinY) / nThisLines)
                            bSuccess = FALSE;
                    }
                }
                
                /* If not, retry with CHECK_WITH_INVERT_PROJ=TRUE that forces ogrct.cpp */
                /* to check the consistency of each requested projection result with the */
                /* invert projection */
                if (!bSuccess)
                {
                    CPLSetThreadLocalConfigOption( "CHECK_WITH_INVERT_PROJ", "TRUE" );
                    CPLDebug("WARP", "Recompute out extent with CHECK_WITH_INVERT_PROJ=TRUE");

                    CPLErr eErr = GDALSuggestedWarpOutput2( pahSrcDS[iSrc], 
                                        psInfo->pfnTransform, hTransformArg, 
                                        adfThisGeoTransform, 
                                        &nThisPixels, &nThisLines, 
                                        adfExtent, 0 );
                    CPLSetThreadLocalConfigOption( "CHECK_WITH_INVERT_PROJ", NULL );
                    if( eErr != CE_None )
                    {
                        if( hCT != NULL )
                            GDALDestroyColorTable( hCT );
                        GDALDestroyGenImgProjTransformer( hTransformArg );
                        return NULL;
                    }
                }
            }

    /* -------------------------------------------------------------------- */
    /*      Expand the working bounds to include this region, ensure the    */
    /*      working resolution is no more than this resolution.             */
    /* -------------------------------------------------------------------- */
            if( dfWrkMaxX == 0.0 && dfWrkMinX == 0.0 )
            {
                dfWrkMinX = adfExtent[0];
                dfWrkMaxX = adfExtent[2];
                dfWrkMaxY = adfExtent[3];
                dfWrkMinY = adfExtent[1];
                dfWrkResX = adfThisGeoTransform[1];
                dfWrkResY = ABS(adfThisGeoTransform[5]);
            }
            else
            {
                dfWrkMinX = MIN(dfWrkMinX,adfExtent[0]);
                dfWrkMaxX = MAX(dfWrkMaxX,adfExtent[2]);
                dfWrkMaxY = MAX(dfWrkMaxY,adfExtent[3]);
                dfWrkMinY = MIN(dfWrkMinY,adfExtent[1]);
                dfWrkResX = MIN(dfWrkResX,adfThisGeoTransform[1]);
                dfWrkResY = MIN(dfWrkResY,ABS(adfThisGeoTransform[5]));
            }
        }

        if (nSrcCount == 1)
        {
            *phTransformArg = hTransformArg;
        }
        else
        {
            GDALDestroyGenImgProjTransformer( hTransformArg );
        }
    }

/* -------------------------------------------------------------------- */
/*      Did we have any usable sources?                                 */
/* -------------------------------------------------------------------- */
    if( nDstBandCount == 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "No usable source images." );
        if( hCT != NULL )
            GDALDestroyColorTable( hCT );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Turn the suggested region into a geotransform and suggested     */
/*      number of pixels and lines.                                     */
/* -------------------------------------------------------------------- */
    double adfDstGeoTransform[6] = { 0, 0, 0, 0, 0, 0 };
    int nPixels = 0, nLines = 0;
    
    if( bNeedsSuggestedWarpOutput )
    {
        adfDstGeoTransform[0] = dfWrkMinX;
        adfDstGeoTransform[1] = dfWrkResX;
        adfDstGeoTransform[2] = 0.0;
        adfDstGeoTransform[3] = dfWrkMaxY;
        adfDstGeoTransform[4] = 0.0;
        adfDstGeoTransform[5] = -1 * dfWrkResY;

        nPixels = (int) ((dfWrkMaxX - dfWrkMinX) / dfWrkResX + 0.5);
        nLines = (int) ((dfWrkMaxY - dfWrkMinY) / dfWrkResY + 0.5);
    }

/* -------------------------------------------------------------------- */
/*      Did the user override some parameters?                          */
/* -------------------------------------------------------------------- */
    if( psOptions->dfXRes != 0.0 && psOptions->dfYRes != 0.0 )
    {
        if( psOptions->dfMinX == 0.0 && psOptions->dfMinY == 0.0 && psOptions->dfMaxX == 0.0 && psOptions->dfMaxY == 0.0 )
        {
            psOptions->dfMinX = adfDstGeoTransform[0];
            psOptions->dfMaxX = adfDstGeoTransform[0] + adfDstGeoTransform[1] * nPixels;
            psOptions->dfMaxY = adfDstGeoTransform[3];
            psOptions->dfMinY = adfDstGeoTransform[3] + adfDstGeoTransform[5] * nLines;
        }
        
        if ( psOptions->bTargetAlignedPixels )
        {
            psOptions->dfMinX = floor(psOptions->dfMinX / psOptions->dfXRes) * psOptions->dfXRes;
            psOptions->dfMaxX = ceil(psOptions->dfMaxX / psOptions->dfXRes) * psOptions->dfXRes;
            psOptions->dfMinY = floor(psOptions->dfMinY / psOptions->dfYRes) * psOptions->dfYRes;
            psOptions->dfMaxY = ceil(psOptions->dfMaxY / psOptions->dfYRes) * psOptions->dfYRes;
        }

        nPixels = (int) ((psOptions->dfMaxX - psOptions->dfMinX + (psOptions->dfXRes/2.0)) / psOptions->dfXRes);
        nLines = (int) ((psOptions->dfMaxY - psOptions->dfMinY + (psOptions->dfYRes/2.0)) / psOptions->dfYRes);
        adfDstGeoTransform[0] = psOptions->dfMinX;
        adfDstGeoTransform[3] = psOptions->dfMaxY;
        adfDstGeoTransform[1] = psOptions->dfXRes;
        adfDstGeoTransform[5] = -psOptions->dfYRes;
    }

    else if( psOptions->nForcePixels != 0 && psOptions->nForceLines != 0 )
    {
        if( psOptions->dfMinX == 0.0 && psOptions->dfMinY == 0.0 && psOptions->dfMaxX == 0.0 && psOptions->dfMaxY == 0.0 )
        {
            psOptions->dfMinX = dfWrkMinX;
            psOptions->dfMaxX = dfWrkMaxX;
            psOptions->dfMaxY = dfWrkMaxY;
            psOptions->dfMinY = dfWrkMinY;
        }

        psOptions->dfXRes = (psOptions->dfMaxX - psOptions->dfMinX) / psOptions->nForcePixels;
        psOptions->dfYRes = (psOptions->dfMaxY - psOptions->dfMinY) / psOptions->nForceLines;

        adfDstGeoTransform[0] = psOptions->dfMinX;
        adfDstGeoTransform[3] = psOptions->dfMaxY;
        adfDstGeoTransform[1] = psOptions->dfXRes;
        adfDstGeoTransform[5] = -psOptions->dfYRes;

        nPixels = psOptions->nForcePixels;
        nLines = psOptions->nForceLines;
    }

    else if( psOptions->nForcePixels != 0 )
    {
        if( psOptions->dfMinX == 0.0 && psOptions->dfMinY == 0.0 && psOptions->dfMaxX == 0.0 && psOptions->dfMaxY == 0.0 )
        {
            psOptions->dfMinX = dfWrkMinX;
            psOptions->dfMaxX = dfWrkMaxX;
            psOptions->dfMaxY = dfWrkMaxY;
            psOptions->dfMinY = dfWrkMinY;
        }

        psOptions->dfXRes = (psOptions->dfMaxX - psOptions->dfMinX) / psOptions->nForcePixels;
        psOptions->dfYRes = psOptions->dfXRes;

        adfDstGeoTransform[0] = psOptions->dfMinX;
        adfDstGeoTransform[3] = psOptions->dfMaxY;
        adfDstGeoTransform[1] = psOptions->dfXRes;
        adfDstGeoTransform[5] = -psOptions->dfYRes;

        nPixels = psOptions->nForcePixels;
        nLines = (int) ((psOptions->dfMaxY - psOptions->dfMinY + (psOptions->dfYRes/2.0)) / psOptions->dfYRes);
    }

    else if( psOptions->nForceLines != 0 )
    {
        if( psOptions->dfMinX == 0.0 && psOptions->dfMinY == 0.0 && psOptions->dfMaxX == 0.0 && psOptions->dfMaxY == 0.0 )
        {
            psOptions->dfMinX = dfWrkMinX;
            psOptions->dfMaxX = dfWrkMaxX;
            psOptions->dfMaxY = dfWrkMaxY;
            psOptions->dfMinY = dfWrkMinY;
        }

        psOptions->dfYRes = (psOptions->dfMaxY - psOptions->dfMinY) / psOptions->nForceLines;
        psOptions->dfXRes = psOptions->dfYRes;

        adfDstGeoTransform[0] = psOptions->dfMinX;
        adfDstGeoTransform[3] = psOptions->dfMaxY;
        adfDstGeoTransform[1] = psOptions->dfXRes;
        adfDstGeoTransform[5] = -psOptions->dfYRes;

        nPixels = (int) ((psOptions->dfMaxX - psOptions->dfMinX + (psOptions->dfXRes/2.0)) / psOptions->dfXRes);
        nLines = psOptions->nForceLines;
    }

    else if( psOptions->dfMinX != 0.0 || psOptions->dfMinY != 0.0 || psOptions->dfMaxX != 0.0 || psOptions->dfMaxY != 0.0 )
    {
        psOptions->dfXRes = adfDstGeoTransform[1];
        psOptions->dfYRes = fabs(adfDstGeoTransform[5]);

        nPixels = (int) ((psOptions->dfMaxX - psOptions->dfMinX + (psOptions->dfXRes/2.0)) / psOptions->dfXRes);
        nLines = (int) ((psOptions->dfMaxY - psOptions->dfMinY + (psOptions->dfYRes/2.0)) / psOptions->dfYRes);

        psOptions->dfXRes = (psOptions->dfMaxX - psOptions->dfMinX) / nPixels;
        psOptions->dfYRes = (psOptions->dfMaxY - psOptions->dfMinY) / nLines;

        adfDstGeoTransform[0] = psOptions->dfMinX;
        adfDstGeoTransform[3] = psOptions->dfMaxY;
        adfDstGeoTransform[1] = psOptions->dfXRes;
        adfDstGeoTransform[5] = -psOptions->dfYRes;
    }

/* -------------------------------------------------------------------- */
/*      Do we want to generate an alpha band in the output file?        */
/* -------------------------------------------------------------------- */
    if( psOptions->bEnableSrcAlpha )
        nDstBandCount--;

    if( psOptions->bEnableDstAlpha )
        nDstBandCount++;

/* -------------------------------------------------------------------- */
/*      Create the output file.                                         */
/* -------------------------------------------------------------------- */
    if( !psOptions->bQuiet )
        printf( "Creating output file that is %dP x %dL.\n", nPixels, nLines );

    hDstDS = GDALCreate( hDriver, pszFilename, nPixels, nLines, 
                         nDstBandCount, eDT, *ppapszCreateOptions );
    
    if( hDstDS == NULL )
    {
        if( hCT != NULL )
            GDALDestroyColorTable( hCT );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Write out the projection definition.                            */
/* -------------------------------------------------------------------- */
    const char *pszDstMethod = CSLFetchNameValue(papszTO,"DST_METHOD");
    if( pszDstMethod == NULL || !EQUAL(pszDstMethod, "NO_GEOTRANSFORM") )
    {
        if( GDALSetProjection( hDstDS, osThisTargetSRS.c_str() ) == CE_Failure ||
            GDALSetGeoTransform( hDstDS, adfDstGeoTransform ) == CE_Failure )
        {
            if( hCT != NULL )
                GDALDestroyColorTable( hCT );
            return NULL;
        }
    }
    else
    {
        adfDstGeoTransform[0] = 0.0;
        adfDstGeoTransform[3] = 0.0;
        adfDstGeoTransform[5] = fabs(adfDstGeoTransform[5]);
    }

    if (*phTransformArg != NULL)
        GDALSetGenImgProjTransformerDstGeoTransform( *phTransformArg, adfDstGeoTransform);

/* -------------------------------------------------------------------- */
/*      Try to set color interpretation of source bands to target       */
/*      dataset.                                                        */
/*      FIXME? We should likely do that for other drivers than VRT      */
/*      but it might create spurious .aux.xml files (at least with HFA, */
/*      and netCDF)                                                     */
/* -------------------------------------------------------------------- */
    if( bVRT || bSetColorInterpretation )
    {
        int nBandsToCopy = (int)apeColorInterpretations.size();
        if ( psOptions->bEnableSrcAlpha )
            nBandsToCopy --;
        for(int iBand = 0; iBand < nBandsToCopy; iBand++)
        {
            GDALSetRasterColorInterpretation(
                GDALGetRasterBand( hDstDS, iBand + 1 ),
                apeColorInterpretations[iBand] );
        }
    }
    
/* -------------------------------------------------------------------- */
/*      Try to set color interpretation of output file alpha band.      */
/* -------------------------------------------------------------------- */
    if( psOptions->bEnableDstAlpha )
    {
        GDALSetRasterColorInterpretation( 
            GDALGetRasterBand( hDstDS, nDstBandCount ), 
            GCI_AlphaBand );
    }

/* -------------------------------------------------------------------- */
/*      Copy the color table, if required.                              */
/* -------------------------------------------------------------------- */
    if( hCT != NULL )
    {
        GDALSetRasterColorTable( GDALGetRasterBand(hDstDS,1), hCT );
        GDALDestroyColorTable( hCT );
    }

    return hDstDS;
}

/************************************************************************/
/*                      GeoTransform_Transformer()                      */
/*                                                                      */
/*      Convert points from georef coordinates to pixel/line based      */
/*      on a geotransform.                                              */
/************************************************************************/

class CutlineTransformer : public OGRCoordinateTransformation
{
public:

    void         *hSrcImageTransformer;

    virtual OGRSpatialReference *GetSourceCS() { return NULL; }
    virtual OGRSpatialReference *GetTargetCS() { return NULL; }

    virtual int Transform( int nCount, 
                           double *x, double *y, double *z = NULL ) {
        int nResult;

        int *pabSuccess = (int *) CPLCalloc(sizeof(int),nCount);
        nResult = TransformEx( nCount, x, y, z, pabSuccess );
        CPLFree( pabSuccess );

        return nResult;
    }

    virtual int TransformEx( int nCount, 
                             double *x, double *y, double *z = NULL,
                             int *pabSuccess = NULL ) {
        return GDALGenImgProjTransform( hSrcImageTransformer, TRUE, 
                                        nCount, x, y, z, pabSuccess );
    }
};

/************************************************************************/
/*                      TransformCutlineToSource()                      */
/*                                                                      */
/*      Transform cutline from its SRS to source pixel/line coordinates.*/
/************************************************************************/
static CPLErr
TransformCutlineToSource( GDALDatasetH hSrcDS, void *hCutline,
                          char ***ppapszWarpOptions, char **papszTO_In )

{
#ifdef OGR_ENABLED
    OGRGeometryH hMultiPolygon = OGR_G_Clone( (OGRGeometryH) hCutline );
    char **papszTO = CSLDuplicate( papszTO_In );

/* -------------------------------------------------------------------- */
/*      Checkout that SRS are the same.                                 */
/* -------------------------------------------------------------------- */
    OGRSpatialReferenceH  hRasterSRS = NULL;
    const char *pszProjection = NULL;

    if( GDALGetProjectionRef( hSrcDS ) != NULL 
        && strlen(GDALGetProjectionRef( hSrcDS )) > 0 )
        pszProjection = GDALGetProjectionRef( hSrcDS );
    else if( GDALGetGCPProjection( hSrcDS ) != NULL )
        pszProjection = GDALGetGCPProjection( hSrcDS );

    if( pszProjection == NULL || EQUAL( pszProjection, "" ) )
        pszProjection = CSLFetchNameValue( papszTO, "SRC_SRS" );

    if( pszProjection != NULL )
    {
        hRasterSRS = OSRNewSpatialReference(NULL);
        if( OSRImportFromWkt( hRasterSRS, (char **)&pszProjection ) != OGRERR_NONE )
        {
            OSRDestroySpatialReference(hRasterSRS);
            hRasterSRS = NULL;
        }
    }

    OGRSpatialReferenceH hCutlineSRS = OGR_G_GetSpatialReference( hMultiPolygon );
    if( hRasterSRS != NULL && hCutlineSRS != NULL )
    {
        /* ok, we will reproject */
    }
    else if( hRasterSRS != NULL && hCutlineSRS == NULL )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                "the source raster dataset has a SRS, but the cutline features\n"
                "not.  We assume that the cutline coordinates are expressed in the destination SRS.\n"
                "If not, cutline results may be incorrect.");
    }
    else if( hRasterSRS == NULL && hCutlineSRS != NULL )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                "the input vector layer has a SRS, but the source raster dataset does not.\n"
                "Cutline results may be incorrect.");
    }

    if( hRasterSRS != NULL )
        OSRDestroySpatialReference(hRasterSRS);

/* -------------------------------------------------------------------- */
/*      Extract the cutline SRS WKT.                                    */
/* -------------------------------------------------------------------- */
    if( hCutlineSRS != NULL )
    {
        char *pszCutlineSRS_WKT = NULL;

        OSRExportToWkt( hCutlineSRS, &pszCutlineSRS_WKT );
        papszTO = CSLSetNameValue( papszTO, "DST_SRS", pszCutlineSRS_WKT );
        CPLFree( pszCutlineSRS_WKT );
    }

/* -------------------------------------------------------------------- */
/*      It may be unwise to let the mask geometry be re-wrapped by      */
/*      the CENTER_LONG machinery as this can easily screw up world     */
/*      spanning masks and invert the mask topology.                    */
/* -------------------------------------------------------------------- */
    papszTO = CSLSetNameValue( papszTO, "INSERT_CENTER_LONG", "FALSE" );

/* -------------------------------------------------------------------- */
/*      Transform the geometry to pixel/line coordinates.               */
/* -------------------------------------------------------------------- */
    CutlineTransformer oTransformer;

    /* The cutline transformer will *invert* the hSrcImageTransformer */
    /* so it will convert from the cutline SRS to the source pixel/line */
    /* coordinates */
    oTransformer.hSrcImageTransformer = 
        GDALCreateGenImgProjTransformer2( hSrcDS, NULL, papszTO );

    CSLDestroy( papszTO );

    if( oTransformer.hSrcImageTransformer == NULL )
        return CE_Failure;

    OGR_G_Transform( hMultiPolygon, 
                     (OGRCoordinateTransformationH) &oTransformer );

    GDALDestroyGenImgProjTransformer( oTransformer.hSrcImageTransformer );

/* -------------------------------------------------------------------- */
/*      Convert aggregate geometry into WKT.                            */
/* -------------------------------------------------------------------- */
    char *pszWKT = NULL;

    OGR_G_ExportToWkt( hMultiPolygon, &pszWKT );
    OGR_G_DestroyGeometry( hMultiPolygon );

    *ppapszWarpOptions = CSLSetNameValue( *ppapszWarpOptions, 
                                          "CUTLINE", pszWKT );
    CPLFree( pszWKT );
#endif
    return CE_None;
}

static void 
RemoveConflictingMetadata( GDALMajorObjectH hObj, char **papszMetadata, 
                           const char *pszValueConflict )
{
    if ( hObj == NULL ) return;

    char *pszKey = NULL; 
    const char *pszValueRef; 
    const char *pszValueComp; 
    char ** papszMetadataRef = CSLDuplicate( papszMetadata );
    int nCount = CSLCount( papszMetadataRef ); 

    for( int i = 0; i < nCount; i++ ) 
    {
        pszKey = NULL;
        pszValueRef = CPLParseNameValue( papszMetadataRef[i], &pszKey ); 
        if( pszKey != NULL )
        {
            pszValueComp = GDALGetMetadataItem( hObj, pszKey, NULL );
            if ( ( pszValueRef == NULL || pszValueComp == NULL ||
                ! EQUAL( pszValueRef, pszValueComp ) ) &&
                ( pszValueComp == NULL ||
                ! EQUAL( pszValueComp, pszValueConflict ) ) ) {
                GDALSetMetadataItem( hObj, pszKey, pszValueConflict, NULL ); 
            }
            CPLFree( pszKey );
        }
    } 

    CSLDestroy( papszMetadataRef );
}

/************************************************************************/
/*                             SanitizeSRS                              */
/************************************************************************/

char *SanitizeSRS( const char *pszUserInput )

{
    OGRSpatialReferenceH hSRS;
    char *pszResult = NULL;

    CPLErrorReset();
    
    hSRS = OSRNewSpatialReference( NULL );
    if( OSRSetFromUserInput( hSRS, pszUserInput ) == OGRERR_NONE )
        OSRExportToWkt( hSRS, &pszResult );
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Translating source or target SRS failed:\n%s",
                  pszUserInput );
    }
    
    OSRDestroySpatialReference( hSRS );

    return pszResult;
}

/************************************************************************/
/*                             GDALWarpAppOptionsNew()                  */
/************************************************************************/

/**
 * Allocates a GDALWarpAppOptions struct.
 *
 * @param papszArgv NULL terminated list of options (potentially including filename and open options too), or NULL.
 *                  The accepted options are the ones of the <a href="gdal_warp.html">gdalwarp</a> utility.
 * @param psOptionsForBinary (output) may be NULL (and should generally be NULL),
 *                           otherwise (gdal_translate_bin.cpp use case) must be allocated with
 *                           GDALWarpAppOptionsForBinaryNew() prior to this function. Will be
 *                           filled with potentially present filename, open options,...
 * @return pointer to the allocated GDALWarpAppOptions struct. Must be freed with GDALWarpAppOptionsFree().
 *
 * @since GDAL 2.1
 */

GDALWarpAppOptions *GDALWarpAppOptionsNew(char** papszArgv,
                                          GDALWarpAppOptionsForBinary* psOptionsForBinary)
{
    GDALWarpAppOptions *psOptions = (GDALWarpAppOptions *)CPLCalloc(1, sizeof(GDALWarpAppOptions));

    psOptions->dfMinX = 0.0;
    psOptions->dfMinY = 0.0;
    psOptions->dfMaxX = 0.0;
    psOptions->dfMaxY = 0.0;
    psOptions->dfXRes = 0.0;
    psOptions->dfYRes = 0.0;
    psOptions->bTargetAlignedPixels = FALSE;
    psOptions->nForcePixels = 0;
    psOptions->nForceLines = 0;
    psOptions->bQuiet = TRUE;
    psOptions->pfnProgress = GDALDummyProgress;
    psOptions->pProgressData = NULL;
    psOptions->bEnableDstAlpha = FALSE;
    psOptions->bEnableSrcAlpha = FALSE;
    psOptions->pszFormat = CPLStrdup("GTiff");
    psOptions->bCreateOutput = FALSE;
    psOptions->papszWarpOptions = NULL;
    psOptions->dfErrorThreshold = -1;
    psOptions->dfWarpMemoryLimit = 0.0;
    psOptions->papszCreateOptions = NULL;
    psOptions->eOutputType = GDT_Unknown;
    psOptions->eWorkingType = GDT_Unknown;
    psOptions->eResampleAlg = GRA_NearestNeighbour;
    psOptions->pszSrcNodata = NULL;
    psOptions->pszDstNodata = NULL;
    psOptions->bMulti = FALSE;
    psOptions->papszTO = NULL;
    psOptions->pszCutlineDSName = NULL;
    psOptions->pszCLayer = NULL;
    psOptions->pszCWHERE = NULL;
    psOptions->pszCSQL = NULL;
    psOptions->bCropToCutline = FALSE;
    psOptions->bCopyMetadata = TRUE;
    psOptions->bCopyBandInfo = TRUE;
    psOptions->pszMDConflictValue = CPLStrdup("*");
    psOptions->bSetColorInterpretation = FALSE;
    psOptions->nOvLevel = -2;

/* -------------------------------------------------------------------- */
/*      Parse arguments.                                                */
/* -------------------------------------------------------------------- */
    int argc = CSLCount(papszArgv);
    for( int i = 0; i < argc; i++ )
    {
        if( EQUAL(papszArgv[i],"-tps") || EQUAL(papszArgv[i],"-rpc") || EQUAL(papszArgv[i],"-geoloc")  )
        {
            const char* pszMethod = CSLFetchNameValue(psOptions->papszTO, "METHOD");
            if (pszMethod)
                CPLError(CE_Warning, CPLE_IllegalArg,
                         "Warning: only one METHOD can be used. Method %s is already defined.",
                        pszMethod);
            const char* pszMAX_GCP_ORDER = CSLFetchNameValue(psOptions->papszTO, "MAX_GCP_ORDER");
            if (pszMAX_GCP_ORDER)
                CPLError(CE_Warning, CPLE_IllegalArg,
                         "Warning: only one METHOD can be used. -order %s option was specified, so it is likely that GCP_POLYNOMIAL was implied.",
                        pszMAX_GCP_ORDER);
        } /* do not add 'else' in front of the next line */

        if( EQUAL(papszArgv[i],"-co") && i+1 < argc )
        {
            psOptions->papszCreateOptions = CSLAddString( psOptions->papszCreateOptions, papszArgv[++i] );
            psOptions->bCreateOutput = TRUE;
        }   
        else if( EQUAL(papszArgv[i],"-wo") && i+1 < argc )
        {
            psOptions->papszWarpOptions = CSLAddString( psOptions->papszWarpOptions, papszArgv[++i] );
        }   
        else if( EQUAL(papszArgv[i],"-multi") )
        {
            psOptions->bMulti = TRUE;
        }   
        else if( EQUAL(papszArgv[i],"-q") || EQUAL(papszArgv[i],"-quiet"))
        {
            if( psOptionsForBinary )
                psOptionsForBinary->bQuiet = TRUE;
        }   
        else if( EQUAL(papszArgv[i],"-dstalpha") )
        {
            psOptions->bEnableDstAlpha = TRUE;
        }
        else if( EQUAL(papszArgv[i],"-srcalpha") )
        {
            psOptions->bEnableSrcAlpha = TRUE;
        }
        else if( EQUAL(papszArgv[i],"-of") && i+1 < argc )
        {
            CPLFree(psOptions->pszFormat);
            psOptions->pszFormat = CPLStrdup(papszArgv[++i]);
            psOptions->bCreateOutput = TRUE;
            if( psOptionsForBinary )
            {
                psOptionsForBinary->bFormatExplicitlySet = TRUE;
            }
        }
        else if( EQUAL(papszArgv[i],"-t_srs") && i+1 < argc )
        {
            char *pszSRS = SanitizeSRS(papszArgv[++i]);
            if(pszSRS == NULL)
            {
                GDALWarpAppOptionsFree(psOptions);
                return NULL;
            }
            psOptions->papszTO = CSLSetNameValue( psOptions->papszTO, "DST_SRS", pszSRS );
            CPLFree( pszSRS );
        }
        else if( EQUAL(papszArgv[i],"-s_srs") && i+1 < argc )
        {
            char *pszSRS = SanitizeSRS(papszArgv[++i]);
            if(pszSRS == NULL)
            {
                GDALWarpAppOptionsFree(psOptions);
                return NULL;
            }
            psOptions->papszTO = CSLSetNameValue( psOptions->papszTO, "SRC_SRS", pszSRS );
            CPLFree( pszSRS );
        }
        else if( EQUAL(papszArgv[i],"-order") && i+1 < argc )
        {
            const char* pszMethod = CSLFetchNameValue(psOptions->papszTO, "METHOD");
            if (pszMethod)
                CPLError(CE_Warning, CPLE_IllegalArg,
                         "Warning: only one METHOD can be used. Method %s is already defined",
                        pszMethod);
            psOptions->papszTO = CSLSetNameValue( psOptions->papszTO, "MAX_GCP_ORDER", papszArgv[++i] );
        }
        else if( EQUAL(papszArgv[i],"-refine_gcps") && i+1 < argc )
        {
            psOptions->papszTO = CSLSetNameValue( psOptions->papszTO, "REFINE_TOLERANCE", papszArgv[++i] );
            if(CPLAtof(papszArgv[i]) < 0)
            {
                CPLError(CE_Failure, CPLE_IllegalArg, "The tolerance for -refine_gcps may not be negative.");
                GDALWarpAppOptionsFree(psOptions);
                return NULL;
            }
            if (i < argc-1 && atoi(papszArgv[i+1]) >= 0 && isdigit(papszArgv[i+1][0]))
            {
                psOptions->papszTO = CSLSetNameValue( psOptions->papszTO, "REFINE_MINIMUM_GCPS", papszArgv[++i] );
            }
            else
            {
                psOptions->papszTO = CSLSetNameValue( psOptions->papszTO, "REFINE_MINIMUM_GCPS", "-1" );
            }
        }
        else if( EQUAL(papszArgv[i],"-tps") )
        {
            psOptions->papszTO = CSLSetNameValue( psOptions->papszTO, "METHOD", "GCP_TPS" );
        }
        else if( EQUAL(papszArgv[i],"-rpc") )
        {
            psOptions->papszTO = CSLSetNameValue( psOptions->papszTO, "METHOD", "RPC" );
        }
        else if( EQUAL(papszArgv[i],"-geoloc") )
        {
            psOptions->papszTO = CSLSetNameValue( psOptions->papszTO, "METHOD", "GEOLOC_ARRAY" );
        }
        else if( EQUAL(papszArgv[i],"-to") && i+1 < argc )
        {
            psOptions->papszTO = CSLAddString( psOptions->papszTO, papszArgv[++i] );
        }
        else if( EQUAL(papszArgv[i],"-et") && i+1 < argc )
        {
            psOptions->dfErrorThreshold = CPLAtofM(papszArgv[++i]);
            psOptions->papszWarpOptions = CSLAddString( psOptions->papszWarpOptions, CPLSPrintf("ERROR_THRESHOLD=%.16g", psOptions->dfErrorThreshold) );
        }
        else if( EQUAL(papszArgv[i],"-wm") && i+1 < argc )
        {
            if( CPLAtofM(papszArgv[i+1]) < 10000 )
                psOptions->dfWarpMemoryLimit = CPLAtofM(papszArgv[i+1]) * 1024 * 1024;
            else
                psOptions->dfWarpMemoryLimit = CPLAtofM(papszArgv[i+1]);
            i++;
        }
        else if( EQUAL(papszArgv[i],"-srcnodata") && i+1 < argc )
        {
            CPLFree(psOptions->pszSrcNodata);
            psOptions->pszSrcNodata = CPLStrdup(papszArgv[++i]);
        }
        else if( EQUAL(papszArgv[i],"-dstnodata") && i+1 < argc )
        {
            CPLFree(psOptions->pszDstNodata);
            psOptions->pszDstNodata = CPLStrdup(papszArgv[++i]);
        }
        else if( EQUAL(papszArgv[i],"-tr") && i+2 < argc )
        {
            psOptions->dfXRes = CPLAtofM(papszArgv[++i]);
            psOptions->dfYRes = fabs(CPLAtofM(papszArgv[++i]));
            if( psOptions->dfXRes == 0 || psOptions->dfYRes == 0 )
            {
                CPLError(CE_Failure, CPLE_IllegalArg, "Wrong value for -tr parameters.");
                GDALWarpAppOptionsFree(psOptions);
                return NULL;
            }
            psOptions->bCreateOutput = TRUE;
        }
        else if( EQUAL(papszArgv[i],"-tap") )
        {
            psOptions->bTargetAlignedPixels = TRUE;
        }
        else if( EQUAL(papszArgv[i],"-ot") && i+1 < argc )
        {
            int iType;
            
            for( iType = 1; iType < GDT_TypeCount; iType++ )
            {
                if( GDALGetDataTypeName((GDALDataType)iType) != NULL
                    && EQUAL(GDALGetDataTypeName((GDALDataType)iType),
                             papszArgv[i+1]) )
                {
                    psOptions->eOutputType = (GDALDataType) iType;
                }
            }

            if( psOptions->eOutputType == GDT_Unknown )
            {
                CPLError(CE_Failure, CPLE_IllegalArg, "Unknown output pixel type: %s.", papszArgv[i+1]);
                GDALWarpAppOptionsFree(psOptions);
                return NULL;
            }
            i++;
            psOptions->bCreateOutput = TRUE;
        }
        else if( EQUAL(papszArgv[i],"-wt") && i+1 < argc )
        {
            int iType;
            
            for( iType = 1; iType < GDT_TypeCount; iType++ )
            {
                if( GDALGetDataTypeName((GDALDataType)iType) != NULL
                    && EQUAL(GDALGetDataTypeName((GDALDataType)iType),
                             papszArgv[i+1]) )
                {
                    psOptions->eWorkingType = (GDALDataType) iType;
                }
            }

            if( psOptions->eWorkingType == GDT_Unknown )
            {
                CPLError(CE_Failure, CPLE_IllegalArg, "Unknown working pixel type: %s.", papszArgv[i+1]);
                GDALWarpAppOptionsFree(psOptions);
                return NULL;
            }
            i++;
        }
        else if( EQUAL(papszArgv[i],"-ts") && i+2 < argc )
        {
            psOptions->nForcePixels = atoi(papszArgv[++i]);
            psOptions->nForceLines = atoi(papszArgv[++i]);
            psOptions->bCreateOutput = TRUE;
        }
        else if( EQUAL(papszArgv[i],"-te") && i+4 < argc )
        {
            psOptions->dfMinX = CPLAtofM(papszArgv[++i]);
            psOptions->dfMinY = CPLAtofM(papszArgv[++i]);
            psOptions->dfMaxX = CPLAtofM(papszArgv[++i]);
            psOptions->dfMaxY = CPLAtofM(papszArgv[++i]);
            psOptions->bCreateOutput = TRUE;
        }
        else if( EQUAL(papszArgv[i],"-te_srs") && i+1 < argc )
        {
            char *pszSRS = SanitizeSRS(papszArgv[++i]);
            if(pszSRS == NULL)
            {
                GDALWarpAppOptionsFree(psOptions);
                return NULL;
            }
            CPLFree(psOptions->pszTE_SRS);
            psOptions->pszTE_SRS = CPLStrdup(pszSRS);
            CPLFree(pszSRS);
            psOptions->bCreateOutput = TRUE;
        }
        else if( EQUAL(papszArgv[i],"-rn") )
            psOptions->eResampleAlg = GRA_NearestNeighbour;

        else if( EQUAL(papszArgv[i],"-rb") )
            psOptions->eResampleAlg = GRA_Bilinear;

        else if( EQUAL(papszArgv[i],"-rc") )
            psOptions->eResampleAlg = GRA_Cubic;

        else if( EQUAL(papszArgv[i],"-rcs") )
            psOptions->eResampleAlg = GRA_CubicSpline;

        else if( EQUAL(papszArgv[i],"-rl") )
            psOptions->eResampleAlg = GRA_Lanczos;

        else if( EQUAL(papszArgv[i],"-ra") )
            psOptions->eResampleAlg = GRA_Average;

        else if( EQUAL(papszArgv[i],"-rm") )
            psOptions->eResampleAlg = GRA_Mode;

        else if( EQUAL(papszArgv[i],"-r") && i+1 < argc )
        {
            if ( EQUAL(papszArgv[++i], "near") )
                psOptions->eResampleAlg = GRA_NearestNeighbour;
            else if ( EQUAL(papszArgv[i], "bilinear") )
                psOptions->eResampleAlg = GRA_Bilinear;
            else if ( EQUAL(papszArgv[i], "cubic") )
                psOptions->eResampleAlg = GRA_Cubic;
            else if ( EQUAL(papszArgv[i], "cubicspline") )
                psOptions->eResampleAlg = GRA_CubicSpline;
            else if ( EQUAL(papszArgv[i], "lanczos") )
                psOptions->eResampleAlg = GRA_Lanczos;
            else if ( EQUAL(papszArgv[i], "average") )
                psOptions->eResampleAlg = GRA_Average;
            else if ( EQUAL(papszArgv[i], "mode") )
                psOptions->eResampleAlg = GRA_Mode;
            else if ( EQUAL(papszArgv[i], "max") )
                psOptions->eResampleAlg = GRA_Max;
            else if ( EQUAL(papszArgv[i], "min") )
                psOptions->eResampleAlg = GRA_Min;
            else if ( EQUAL(papszArgv[i], "med") )
                psOptions->eResampleAlg = GRA_Med;
            else if ( EQUAL(papszArgv[i], "q1") )
                psOptions->eResampleAlg = GRA_Q1;
            else if ( EQUAL(papszArgv[i], "q3") )
                psOptions->eResampleAlg = GRA_Q3;
            else
            {
                CPLError(CE_Failure, CPLE_IllegalArg, "Unknown resampling method: %s.", papszArgv[i]);
                GDALWarpAppOptionsFree(psOptions);
                return NULL;
            }
        }

        else if( EQUAL(papszArgv[i],"-cutline") && i+1 < argc )
        {
            CPLFree(psOptions->pszCutlineDSName);
            psOptions->pszCutlineDSName = CPLStrdup(papszArgv[++i]);
        }
        else if( EQUAL(papszArgv[i],"-cwhere") && i+1 < argc )
        {
            CPLFree(psOptions->pszCWHERE);
            psOptions->pszCWHERE = CPLStrdup(papszArgv[++i]);
        }
        else if( EQUAL(papszArgv[i],"-cl") && i+1 < argc )
        {
            CPLFree(psOptions->pszCLayer);
            psOptions->pszCLayer = CPLStrdup(papszArgv[++i]);
        }
        else if( EQUAL(papszArgv[i],"-csql") && i+1 < argc )
        {
            CPLFree(psOptions->pszCSQL);
            psOptions->pszCSQL = CPLStrdup(papszArgv[++i]);
        }
        else if( EQUAL(papszArgv[i],"-cblend") && i+1 < argc )
        {
            psOptions->papszWarpOptions = 
                CSLSetNameValue( psOptions->papszWarpOptions, 
                                 "CUTLINE_BLEND_DIST", papszArgv[++i] );
        }
        else if( EQUAL(papszArgv[i],"-crop_to_cutline")  )
        {
            psOptions->bCropToCutline = TRUE;
            psOptions->bCreateOutput = TRUE;
        }
        else if( EQUAL(papszArgv[i],"-overwrite") )
        {
            if( psOptionsForBinary )
                psOptionsForBinary->bOverwrite = TRUE;
        }
        else if( EQUAL(papszArgv[i],"-nomd") )
        {
            psOptions->bCopyMetadata = FALSE;
            psOptions->bCopyBandInfo = FALSE;
        }   
        else if( EQUAL(papszArgv[i],"-cvmd") && i+1 < argc )
        {
            CPLFree(psOptions->pszMDConflictValue);
            psOptions->pszMDConflictValue = CPLStrdup(papszArgv[++i]);
        }
        else if( EQUAL(papszArgv[i],"-setci") )
            psOptions->bSetColorInterpretation = TRUE;
        else if( EQUAL(papszArgv[i], "-oo") && i+1 < argc )
        {
            if( psOptionsForBinary )
                psOptionsForBinary->papszOpenOptions = CSLAddString(
                                                psOptionsForBinary->papszOpenOptions,
                                                papszArgv[++i] );
        }
        else if( EQUAL(papszArgv[i], "-doo") && i+1 < argc )
        {
            if( psOptionsForBinary )
                psOptionsForBinary->papszDestOpenOptions = CSLAddString(
                                                psOptionsForBinary->papszDestOpenOptions,
                                                papszArgv[++i] );
        }
        else if( EQUAL(papszArgv[i], "-ovr") && i+1 < argc )
        {
            const char* pszOvLevel = papszArgv[++i];
            if( EQUAL(pszOvLevel, "AUTO") )
                psOptions->nOvLevel = -2;
            else if( EQUALN(pszOvLevel,"AUTO-",5) )
                psOptions->nOvLevel = -2-atoi(pszOvLevel + 5);
            else if( EQUAL(pszOvLevel, "NONE") )
                psOptions->nOvLevel = -1;
            else if( CPLGetValueType(pszOvLevel) == CPL_VALUE_INTEGER )
                psOptions->nOvLevel = atoi(pszOvLevel); 
            else
            {
                CPLError(CE_Failure, CPLE_IllegalArg, "Invalid value '%s' for -ov option", pszOvLevel);
                GDALWarpAppOptionsFree(psOptions);
                return NULL;
            }
        }
        else if( papszArgv[i][0] == '-' )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Unknown option name '%s'", papszArgv[i]);
            GDALWarpAppOptionsFree(psOptions);
            return NULL;
        }

        else 
        {
            if( psOptionsForBinary )
            {
                psOptionsForBinary->papszSrcFiles = CSLAddString( psOptionsForBinary->papszSrcFiles, papszArgv[i] );
            }
        }
    }
    
    if( psOptionsForBinary )
        psOptionsForBinary->bCreateOutput = psOptions->bCreateOutput;

    if( psOptionsForBinary )
        psOptionsForBinary->pszFormat = CPLStrdup(psOptions->pszFormat);

/* -------------------------------------------------------------------- */
/*      The last filename in the file list is really our destination    */
/*      file.                                                           */
/* -------------------------------------------------------------------- */
    if( psOptionsForBinary && CSLCount(psOptionsForBinary->papszSrcFiles) > 1 )
    {
        psOptionsForBinary->pszDstFilename = psOptionsForBinary->papszSrcFiles[CSLCount(psOptionsForBinary->papszSrcFiles)-1];
        psOptionsForBinary->papszSrcFiles[CSLCount(psOptionsForBinary->papszSrcFiles)-1] = NULL;
    }

    return psOptions;
}

/************************************************************************/
/*                        GDALWarpAppOptionsFree()                    */
/************************************************************************/

/**
 * Frees the GDALWarpAppOptions struct.
 *
 * @param psOptions the options struct for GDALWarp().
 *
 * @since GDAL 2.1
 */

void GDALWarpAppOptionsFree( GDALWarpAppOptions *psOptions )
{
    if( psOptions )
    {
        CPLFree(psOptions->pszFormat);
        CSLDestroy(psOptions->papszWarpOptions);
        CSLDestroy(psOptions->papszCreateOptions);
        CPLFree(psOptions->pszSrcNodata);
        CPLFree(psOptions->pszDstNodata);
        CSLDestroy(psOptions->papszTO);
        CPLFree(psOptions->pszCutlineDSName);
        CPLFree(psOptions->pszCLayer);
        CPLFree(psOptions->pszCWHERE);
        CPLFree(psOptions->pszCSQL);
        CPLFree(psOptions->pszMDConflictValue);
        CPLFree(psOptions->pszTE_SRS);
    }

    CPLFree(psOptions);
}

/************************************************************************/
/*                 GDALWarpAppOptionsSetProgress()                    */
/************************************************************************/

/**
 * Set a progress function.
 *
 * @param psOptions the options struct for GDALWarpApp().
 * @param pfnProgress the progress callback.
 * @param pProgressData the user data for the progress callback.
 *
 * @since GDAL 2.1
 */

void GDALWarpAppOptionsSetProgress( GDALWarpAppOptions *psOptions,
                                      GDALProgressFunc pfnProgress, void *pProgressData )
{
    psOptions->pfnProgress = pfnProgress ? pfnProgress : GDALDummyProgress;
    psOptions->pProgressData = pProgressData;
    if( pfnProgress == GDALTermProgress )
        psOptions->bQuiet = FALSE;
}

/************************************************************************/
/*                 GDALWarpAppOptionsSetWarpOption()                    */
/************************************************************************/

/**
 * Set a warp option
 *
 * @param psOptions the options struct for GDALWarpApp().
 * @param pszKey key.
 * @param pszValue value.
 *
 * @since GDAL 2.1
 */

void GDALWarpAppOptionsSetWarpOption( GDALWarpAppOptions *psOptions,
                                      const char* pszKey,
                                      const char* pszValue )
{
    psOptions->papszWarpOptions = CSLSetNameValue(psOptions->papszWarpOptions, pszKey, pszValue);
}
