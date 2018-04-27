/******************************************************************************
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

#include "cpl_port.h"
#include "gdal_utils.h"
#include "gdal_utils_priv.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <limits>
#include <vector>

#include "commonutils.h"
#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_progress.h"
#include "cpl_string.h"
#include "gdal.h"
#include "gdal_alg.h"
#include "gdal_priv.h"
#include "gdalwarper.h"
#include "ogr_api.h"
#include "ogr_core.h"
#include "ogr_geometry.h"
#include "ogr_spatialref.h"
#include "ogr_srs_api.h"
#include "vrtdataset.h"

CPL_CVSID("$Id$")

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
        convenience e.g. when knowing the output coordinates in a
        geodetic long/lat SRS, but still wanting a result in a projected
        coordinate system. */
    char *pszTE_SRS;

    /*! set output file resolution (in target georeferenced units) */
    double dfXRes;
    double dfYRes;

    /*! align the coordinates of the extent of the output file to the values of the
        GDALWarpAppOptions::dfXRes and GDALWarpAppOptions::dfYRes, such that the
        aligned extent includes the minimum extent. */
    bool bTargetAlignedPixels;

    /*! set output file size in pixels and lines. If GDALWarpAppOptions::nForcePixels
        or GDALWarpAppOptions::nForceLines is set to 0, the other dimension will be
        guessed from the computed resolution. Note that GDALWarpAppOptions::nForcePixels and
        GDALWarpAppOptions::nForceLines cannot be used with GDALWarpAppOptions::dfXRes and
        GDALWarpAppOptions::dfYRes. */
    int nForcePixels;
    int nForceLines;

    /*! allow or suppress progress monitor and other non-error output */
    bool bQuiet;

    /*! the progress function to use */
    GDALProgressFunc pfnProgress;

    /*! pointer to the progress data variable */
    void *pProgressData;

    /*! creates an output alpha band to identify nodata (unset/transparent) pixels
        when set to true */
    bool bEnableDstAlpha;

    /*! forces the last band of an input file to be considered as alpha band. */
    bool bEnableSrcAlpha;

    /*! Prevent a source alpha band from being considered as such */
    bool bDisableSrcAlpha;

    /*! output format. Use the short format name. */
    char *pszFormat;

    bool bCreateOutput;

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
    bool bMulti;

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
    bool bCropToCutline;

    /*! copy dataset and band metadata will be copied from the first source dataset. Items that differ between
        source datasets will be set "*" (see GDALWarpAppOptions::pszMDConflictValue) */
    bool bCopyMetadata;

    /*! copy band information from the first source dataset */
    bool bCopyBandInfo;

    /*! value to set metadata items that conflict between source datasets (default is "*").
        Use "" to remove conflicting items. */
    char *pszMDConflictValue;

    /*! set the color interpretation of the bands of the target dataset from the source dataset */
    bool bSetColorInterpretation;

    /*! overview level of source files to be used */
    int nOvLevel;

    /*! Whether to disable vertical grid shift adjustment */
    bool bNoVShiftGrid;
};

static CPLErr
LoadCutline( const char *pszCutlineDSName, const char *pszCLayer,
             const char *pszCWHERE, const char *pszCSQL,
             OGRGeometryH *phCutlineRet );
static CPLErr
TransformCutlineToSource( GDALDatasetH hSrcDS, OGRGeometryH hCutline,
                          char ***ppapszWarpOptions, char **papszTO );

static GDALDatasetH
GDALWarpCreateOutput( int nSrcCount, GDALDatasetH *pahSrcDS, const char *pszFilename,
                      const char *pszFormat, char **papszTO,
                      char ***ppapszCreateOptions, GDALDataType eDT,
                      void ** phTransformArg,
                      bool bSetColorInterpretation,
                      GDALWarpAppOptions *psOptions);

static void
RemoveConflictingMetadata( GDALMajorObjectH hObj, char **papszMetadata,
                           const char *pszValueConflict );

static double GetAverageSegmentLength(OGRGeometryH hGeom)
{
    if( hGeom == nullptr )
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
/*                          GetSrcDSProjection()                        */
/*                                                                      */
/* Takes into account SRC_SRS transformer option in priority, and then  */
/* dataset characteristics as well as the METHOD transformer            */
/* option to determine the source SRS.                                  */
/************************************************************************/

static const char* GetSrcDSProjection( GDALDatasetH hDS,
                                       char** papszTO )
{
    const char *pszProjection = CSLFetchNameValue( papszTO, "SRC_SRS" );
    if( pszProjection != nullptr || hDS == nullptr )
    {
        return pszProjection;
    }

    const char *pszMethod = CSLFetchNameValue( papszTO, "METHOD" );
    char** papszMD = nullptr;
    if( GDALGetProjectionRef( hDS ) != nullptr
        && strlen(GDALGetProjectionRef( hDS )) > 0
        && (pszMethod == nullptr || EQUAL(pszMethod,"GEOTRANSFORM")) )
    {
        pszProjection = GDALGetProjectionRef( hDS );
    }
    else if( GDALGetGCPProjection( hDS ) != nullptr
             && strlen(GDALGetGCPProjection( hDS )) > 0
             && GDALGetGCPCount( hDS ) > 1
             && (pszMethod == nullptr || STARTS_WITH_CI(pszMethod, "GCP_")) )
    {
        pszProjection = GDALGetGCPProjection( hDS );
    }
    else if( GDALGetMetadata( hDS, "RPC" ) != nullptr &&
             (pszMethod == nullptr || EQUAL(pszMethod,"RPC") ) )
    {
        pszProjection = SRS_WKT_WGS84;
    }
    else if( (papszMD = GDALGetMetadata( hDS, "GEOLOCATION" )) != nullptr &&
             (pszMethod == nullptr || EQUAL(pszMethod,"GEOLOC_ARRAY") ) )
    {
        pszProjection = CSLFetchNameValue( papszMD, "SRS" );
    }
    return pszProjection;
}

/************************************************************************/
/*                           CropToCutline()                            */
/************************************************************************/

static CPLErr CropToCutline( OGRGeometryH hCutline, char** papszTO,
                             int nSrcCount, GDALDatasetH *pahSrcDS,
                             double& dfMinX, double& dfMinY,
                             double& dfMaxX, double &dfMaxY )
{
    // We could possibly directly reproject from cutline SRS to target SRS,
    // but when applying the cutline, it is reprojected to source raster image
    // space using the source SRS. To be consistent, we reproject
    // the cutline from cutline SRS to source SRS and then from source SRS to
    // target SRS.
    OGRSpatialReferenceH hCutlineSRS = OGR_G_GetSpatialReference( hCutline );
    const char *pszThisTargetSRS = CSLFetchNameValue( papszTO, "DST_SRS" );
    OGRSpatialReferenceH hSrcSRS = nullptr;
    OGRSpatialReferenceH hDstSRS = nullptr;

    const char *pszThisSourceSRS =
        GetSrcDSProjection(
            nSrcCount > 0 && pahSrcDS[0] != nullptr ? pahSrcDS[0] : nullptr,
            papszTO);
    if( pszThisSourceSRS != nullptr && pszThisSourceSRS[0] != '\0' )
    {
        hSrcSRS = OSRNewSpatialReference(nullptr);
        if( OSRImportFromWkt( hSrcSRS, const_cast<char **>(&pszThisSourceSRS) ) != OGRERR_NONE )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot compute bounding box of cutline.");
            OSRDestroySpatialReference(hSrcSRS);
            return CE_Failure;
        }
    }
    else if( pszThisTargetSRS == nullptr && hCutlineSRS == nullptr )
    {
        OGREnvelope sEnvelope;
        OGR_G_GetEnvelope(hCutline, &sEnvelope);

        dfMinX = sEnvelope.MinX;
        dfMinY = sEnvelope.MinY;
        dfMaxX = sEnvelope.MaxX;
        dfMaxY = sEnvelope.MaxY;

        return CE_None;
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                    "Cannot compute bounding box of cutline. Cannot find "
                    "source SRS");
        return CE_Failure;
    }

    if ( pszThisTargetSRS != nullptr )
    {
        hDstSRS = OSRNewSpatialReference(nullptr);
        if( OSRImportFromWkt( hDstSRS, const_cast<char **>(&pszThisTargetSRS) ) != OGRERR_NONE )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot compute bounding box of cutline.");
            OSRDestroySpatialReference(hSrcSRS);
            OSRDestroySpatialReference(hDstSRS);
            return CE_Failure;
        }
    }
    else
        hDstSRS = OSRClone(hSrcSRS);

    OGRGeometryH hCutlineGeom = OGR_G_Clone( hCutline );
    OGRSpatialReferenceH hCutlineOrTargetSRS = hCutlineSRS ? hCutlineSRS : hDstSRS;
    OGRCoordinateTransformationH hCTCutlineToSrc = nullptr;
    OGRCoordinateTransformationH hCTSrcToDst = nullptr;

    if( !OSRIsSame(hCutlineOrTargetSRS, hSrcSRS) )
        hCTCutlineToSrc = OCTNewCoordinateTransformation(hCutlineOrTargetSRS, hSrcSRS);
    if( !OSRIsSame(hSrcSRS, hDstSRS) )
        hCTSrcToDst = OCTNewCoordinateTransformation(hSrcSRS, hDstSRS);

    OSRDestroySpatialReference(hSrcSRS);
    hSrcSRS = nullptr;

    OSRDestroySpatialReference(hDstSRS);
    hDstSRS = nullptr;

    // Reproject cutline to target SRS, by doing intermediate vertex densification
    // in source SRS.
    if( hCTSrcToDst != nullptr || hCTCutlineToSrc != nullptr )
    {
        OGREnvelope sLastEnvelope, sCurEnvelope;
        OGRGeometryH hTransformedGeom = nullptr;
        OGRGeometryH hGeomInSrcSRS = OGR_G_Clone(hCutlineGeom);
        if( hCTCutlineToSrc != nullptr )
            OGR_G_Transform( hGeomInSrcSRS, hCTCutlineToSrc );

        const double epsilon = std::numeric_limits<double>::epsilon();
        for(int nIter=0;nIter<10;nIter++)
        {
            OGR_G_DestroyGeometry(hTransformedGeom);
            hTransformedGeom = OGR_G_Clone(hGeomInSrcSRS);
            if( hCTSrcToDst != nullptr )
                OGR_G_Transform( hTransformedGeom, hCTSrcToDst );
            OGR_G_GetEnvelope(hTransformedGeom, &sCurEnvelope);
            if( nIter > 0 || hCTSrcToDst == nullptr )
            {
                if (std::abs(sCurEnvelope.MinX - sLastEnvelope.MinX) <=
                        epsilon * std::abs(sCurEnvelope.MinX + sLastEnvelope.MinX) &&
                    std::abs(sCurEnvelope.MinY - sLastEnvelope.MinY) <=
                        epsilon * std::abs(sCurEnvelope.MinY + sLastEnvelope.MinY) &&
                    std::abs(sCurEnvelope.MaxX - sLastEnvelope.MaxX) <=
                        epsilon * std::abs(sCurEnvelope.MaxX + sLastEnvelope.MaxX) &&
                    std::abs(sCurEnvelope.MaxY - sLastEnvelope.MaxY) <=
                        epsilon * std::abs(sCurEnvelope.MaxY + sLastEnvelope.MaxY))
                {
                    break;
                }
            }
            double dfAverageSegmentLength = GetAverageSegmentLength(hGeomInSrcSRS);
            OGR_G_Segmentize(hGeomInSrcSRS, dfAverageSegmentLength/4);

            sLastEnvelope = sCurEnvelope;
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

/************************************************************************/
/*                          GDALWarpAppOptionsClone()                   */
/************************************************************************/

static
GDALWarpAppOptions* GDALWarpAppOptionsClone(const GDALWarpAppOptions *psOptionsIn)
{
    GDALWarpAppOptions* psOptions = static_cast<GDALWarpAppOptions *>(
        CPLMalloc(sizeof(GDALWarpAppOptions)));
    memcpy(psOptions, psOptionsIn, sizeof(GDALWarpAppOptions));
    if( psOptionsIn->pszFormat) psOptions->pszFormat = CPLStrdup(psOptionsIn->pszFormat);
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
/*                           Is3DGeogcs()                               */
/************************************************************************/

static bool Is3DGeogcs( const OGRSpatialReference& oSRS )
{
    const char* pszName = oSRS.GetAuthorityName(nullptr);
    const char* pszCode = oSRS.GetAuthorityCode(nullptr);
    // Yep, we only support that EPSG:4979, ie WGS 84 3D
    return pszName != nullptr && EQUAL(pszName, "EPSG") &&
           pszCode != nullptr && EQUAL(pszCode, "4979");
}

/************************************************************************/
/*                      ApplyVerticalShiftGrid()                        */
/************************************************************************/

static GDALDatasetH ApplyVerticalShiftGrid( GDALDatasetH hWrkSrcDS,
                                            const GDALWarpAppOptions* psOptions,
                                            GDALDatasetH hVRTDS,
                                            bool& bErrorOccurredOut )
{
    bErrorOccurredOut = false;
    // Check if we must do vertical shift grid transform
    const char* pszSrcWKT = CSLFetchNameValueDef(
                                    psOptions->papszTO, "SRC_SRS",
                                    GDALGetProjectionRef(hWrkSrcDS) );
    const char* pszDstWKT = CSLFetchNameValue( psOptions->papszTO, "DST_SRS" );
    double adfGT[6] = {};
    if( GDALGetRasterCount(hWrkSrcDS) == 1 &&
        GDALGetGeoTransform(hWrkSrcDS, adfGT) == CE_None &&
        pszSrcWKT != nullptr && pszSrcWKT[0] != '\0' &&
        pszDstWKT != nullptr )
    {
        OGRSpatialReference oSRSSrc;
        OGRSpatialReference oSRSDst;
        if( oSRSSrc.SetFromUserInput( pszSrcWKT ) == OGRERR_NONE &&
            oSRSDst.SetFromUserInput( pszDstWKT ) == OGRERR_NONE &&
            (oSRSSrc.IsCompound() || Is3DGeogcs(oSRSSrc) ||
             oSRSDst.IsCompound() || Is3DGeogcs(oSRSDst)) )
        {
            const char *pszSrcProj4Geoids =
                oSRSSrc.GetExtension( "VERT_DATUM", "PROJ4_GRIDS" );
            const char *pszDstProj4Geoids =
                oSRSDst.GetExtension( "VERT_DATUM", "PROJ4_GRIDS" );

            if( oSRSSrc.IsCompound() && pszSrcProj4Geoids == nullptr )
            {
                CPLDebug("GDALWARP", "Source SRS is a compound CRS but lacks "
                         "+geoidgrids");
            }

            if( oSRSDst.IsCompound() && pszDstProj4Geoids == nullptr )
            {
                CPLDebug("GDALWARP", "Target SRS is a compound CRS but lacks "
                         "+geoidgrids");
            }

            if( pszSrcProj4Geoids != nullptr && pszDstProj4Geoids != nullptr &&
                EQUAL(pszSrcProj4Geoids, pszDstProj4Geoids) )
            {
                pszSrcProj4Geoids = nullptr;
                pszDstProj4Geoids = nullptr;
            }

            // Select how to go from input dataset units to meters
            const char* pszUnit =
                GDALGetRasterUnitType( GDALGetRasterBand(hWrkSrcDS, 1) );
            double dfToMeterSrc = 1.0;
            if( pszUnit && (EQUAL(pszUnit, "m") ||
                            EQUAL(pszUnit, "meter")||
                            EQUAL(pszUnit, "metre")) )
            {
            }
            else if( pszUnit && (EQUAL(pszUnit, "ft") ||
                                 EQUAL(pszUnit, "foot")) )
            {
                dfToMeterSrc = CPLAtof(SRS_UL_FOOT_CONV);
            }
            else
            {
                if( pszUnit && !EQUAL(pszUnit, "") )
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Unknown units=%s", pszUnit);
                }
                if( oSRSSrc.IsCompound() )
                {
                    dfToMeterSrc =
                        oSRSSrc.GetTargetLinearUnits("VERT_CS");
                }
                else if( oSRSSrc.IsProjected() )
                {
                    dfToMeterSrc = oSRSSrc.GetLinearUnits();
                }
            }

            double dfToMeterDst = 1.0;
            if( oSRSDst.IsCompound() )
            {
                dfToMeterDst =
                    oSRSDst.GetTargetLinearUnits("VERT_CS");
            }
            else if( oSRSDst.IsProjected() )
            {
                dfToMeterDst = oSRSDst.GetLinearUnits();
            }

            char** papszOptions = nullptr;
            if( psOptions->eOutputType != GDT_Unknown )
            {
                papszOptions = CSLSetNameValue(papszOptions,
                        "DATATYPE",
                        GDALGetDataTypeName(psOptions->eOutputType));
            }
            papszOptions = CSLSetNameValue(papszOptions,
                "ERROR_ON_MISSING_VERT_SHIFT",
                CSLFetchNameValue(psOptions->papszTO,
                                  "ERROR_ON_MISSING_VERT_SHIFT"));
            papszOptions = CSLSetNameValue(papszOptions, "SRC_SRS",
                                           pszSrcWKT);

            if( pszSrcProj4Geoids != nullptr )
            {
                int bError = FALSE;
                GDALDatasetH hGridDataset =
                    GDALOpenVerticalShiftGrid(pszSrcProj4Geoids, &bError);
                if( bError && hGridDataset == nullptr )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Cannot open %s.", pszSrcProj4Geoids);
                    bErrorOccurredOut = true;
                    CSLDestroy(papszOptions);
                    return hWrkSrcDS;
                }
                else if( hGridDataset != nullptr )
                {
                    // Transform from source vertical datum to WGS84
                    GDALDatasetH hTmpDS = GDALApplyVerticalShiftGrid(
                        hWrkSrcDS, hGridDataset, FALSE,
                        dfToMeterSrc, 1.0, papszOptions );
                    GDALReleaseDataset(hGridDataset);
                    if( hTmpDS == nullptr )
                    {
                        bErrorOccurredOut = true;
                        CSLDestroy(papszOptions);
                        return hWrkSrcDS;
                    }
                    else
                    {
                        if( hVRTDS )
                        {
                            VRTWarpedDataset* poVRTDS =
                                reinterpret_cast<VRTWarpedDataset*>(hVRTDS);
                            poVRTDS->SetApplyVerticalShiftGrid(
                                pszSrcProj4Geoids,
                                false,
                                dfToMeterSrc, 1.0, papszOptions );
                        }
                        CPLDebug("GDALWARP", "Adjusting source dataset "
                                 "with source vertical datum using %s",
                                 pszSrcProj4Geoids);
                        GDALReleaseDataset(hWrkSrcDS);
                        hWrkSrcDS = hTmpDS;
                        dfToMeterSrc = 1.0;
                    }
                }
            }

            if( pszDstProj4Geoids != nullptr )
            {
                int bError = FALSE;
                GDALDatasetH hGridDataset =
                    GDALOpenVerticalShiftGrid(pszDstProj4Geoids, &bError);
                if( bError && hGridDataset == nullptr )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Cannot open %s.", pszDstProj4Geoids);
                    bErrorOccurredOut = true;
                    CSLDestroy(papszOptions);
                    return hWrkSrcDS;
                }
                else if( hGridDataset != nullptr )
                {
                    // Transform from WGS84 to target vertical datum
                    GDALDatasetH hTmpDS = GDALApplyVerticalShiftGrid(
                        hWrkSrcDS, hGridDataset, TRUE,
                        dfToMeterSrc, dfToMeterDst, papszOptions );
                    GDALReleaseDataset(hGridDataset);
                    if( hTmpDS == nullptr )
                    {
                        bErrorOccurredOut = true;
                        CSLDestroy(papszOptions);
                        return hWrkSrcDS;
                    }
                    else
                    {
                        if( hVRTDS )
                        {
                            VRTWarpedDataset* poVRTDS =
                                reinterpret_cast<VRTWarpedDataset*>(hVRTDS);
                            poVRTDS->SetApplyVerticalShiftGrid(
                                pszDstProj4Geoids,
                                true,
                                dfToMeterSrc, dfToMeterDst, papszOptions );
                        }
                        CPLDebug("GDALWARP", "Adjusting source dataset "
                                 "with target vertical datum using %s",
                                 pszDstProj4Geoids);
                        GDALReleaseDataset(hWrkSrcDS);
                        hWrkSrcDS = hTmpDS;
                    }
                }
            }

            CSLDestroy(papszOptions);
        }
    }
    return hWrkSrcDS;
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
 * @param psOptionsIn the options struct returned by GDALWarpAppOptionsNew() or NULL.
 * @param pbUsageError the pointer to int variable to determine any usage error has occurred.
 * @return the output dataset (new dataset that must be closed using GDALClose(), or hDstDS if not NULL) or NULL in case of error.
 *
 * @since GDAL 2.1
 */

GDALDatasetH GDALWarp( const char *pszDest, GDALDatasetH hDstDS, int nSrcCount,
                       GDALDatasetH *pahSrcDS, const GDALWarpAppOptions *psOptionsIn, int *pbUsageError )
{
    CPLErrorReset();
    if( pszDest == nullptr && hDstDS == nullptr )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "pszDest == NULL && hDstDS == NULL");

        if(pbUsageError)
            *pbUsageError = TRUE;
        return nullptr;
    }
    if( pszDest == nullptr )
        pszDest = GDALGetDescription(hDstDS);

#ifdef DEBUG
    GDALDataset* poDstDS = reinterpret_cast<GDALDataset*>(hDstDS);
    const int nExpectedRefCountAtEnd = ( poDstDS != nullptr ) ? poDstDS->GetRefCount() : 1;
#endif
    const bool bDropDstDSRef = (hDstDS != nullptr);
    if( hDstDS != nullptr )
        GDALReferenceDataset(hDstDS);
    GDALTransformerFunc pfnTransformer = nullptr;
    void *hTransformArg = nullptr;
    bool bHasGotErr = false;
    bool bVRT = false;
    OGRGeometryH hCutline = nullptr;

    GDALWarpAppOptions* psOptions =
        (psOptionsIn) ? GDALWarpAppOptionsClone(psOptionsIn) :
                        GDALWarpAppOptionsNew(nullptr, nullptr);

    psOptions->papszTO = CSLSetNameValue(psOptions->papszTO,
                                         "STRIP_VERT_CS", "YES");

    if( hDstDS )
    {
        if( psOptions->bCreateOutput == true )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "All options related to creation ignored in update mode");
            psOptions->bCreateOutput = false;
        }
    }

    if( (psOptions->pszFormat == nullptr && EQUAL(CPLGetExtension(pszDest), "VRT")) ||
        (psOptions->pszFormat != nullptr && EQUAL(psOptions->pszFormat,"VRT")) )
    {
        if( hDstDS != nullptr )
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                     "VRT output not compatible with existing dataset.");
            GDALWarpAppOptionsFree(psOptions);
            return nullptr;
        }

        bVRT = true;

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
        return nullptr;
    }

    if (psOptions->bTargetAlignedPixels && psOptions->dfXRes == 0 && psOptions->dfYRes == 0)
    {
        CPLError(CE_Failure, CPLE_IllegalArg, "-tap option cannot be used without using -tr.");
        if(pbUsageError)
            *pbUsageError = TRUE;
        GDALWarpAppOptionsFree(psOptions);
        return nullptr;
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
        if( CSLFetchNameValue(psOptions->papszTO, "RPC_DEM") != nullptr )
            psOptions->dfErrorThreshold = 0.0;
        else
            psOptions->dfErrorThreshold = 0.125;
    }

/* -------------------------------------------------------------------- */
/*      -te_srs option                                                  */
/* -------------------------------------------------------------------- */
    if( psOptions->pszTE_SRS != nullptr )
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
            bool bOK = false;
            if( CSLFetchNameValue( psOptions->papszTO, "DST_SRS" ) != nullptr )
            {
                oSRSDS.SetFromUserInput( CSLFetchNameValue( psOptions->papszTO, "DST_SRS" ) );
                bOK = true;
            }
            else if( CSLFetchNameValue( psOptions->papszTO, "SRC_SRS" ) != nullptr )
            {
                oSRSDS.SetFromUserInput( CSLFetchNameValue( psOptions->papszTO, "SRC_SRS" ) );
                bOK = true;
            }
            else
            {
                if( nSrcCount && pahSrcDS[0] && GDALGetProjectionRef(pahSrcDS[0]) && GDALGetProjectionRef(pahSrcDS[0])[0] )
                {
                    oSRSDS.SetFromUserInput( GDALGetProjectionRef(pahSrcDS[0]) );
                    bOK = true;
                }
            }
            if( !bOK )
            {
                CPLError( CE_Failure, CPLE_AppDefined, "-te_srs ignored since none of -t_srs, -s_srs is specified or the input dataset has no projection.");
                GDALWarpAppOptionsFree(psOptions);
                return nullptr;
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
                    return nullptr;
                }
                delete poCT;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      If we have a cutline datasource read it and attach it in the    */
/*      warp options.                                                   */
/* -------------------------------------------------------------------- */
    if( psOptions->pszCutlineDSName != nullptr )
    {
        CPLErr eError;
        eError = LoadCutline( psOptions->pszCutlineDSName,
                              psOptions->pszCLayer,
                              psOptions->pszCWHERE, psOptions->pszCSQL,
                              &hCutline );
        if(eError == CE_Failure)
        {
            GDALWarpAppOptionsFree(psOptions);
            return nullptr;
        }
    }

    if ( psOptions->bCropToCutline && hCutline != nullptr )
    {
        CPLErr eError;
        eError = CropToCutline( hCutline, psOptions->papszTO, nSrcCount, pahSrcDS,
                       psOptions->dfMinX, psOptions->dfMinY, psOptions->dfMaxX, psOptions->dfMaxY );
        if(eError == CE_Failure)
        {
            GDALWarpAppOptionsFree(psOptions);
            OGR_G_DestroyGeometry( hCutline );
            return nullptr;
        }
    }

/* -------------------------------------------------------------------- */
/*      If not, we need to create it.                                   */
/* -------------------------------------------------------------------- */
    void* hUniqueTransformArg = nullptr;
    const bool bInitDestSetByUser = ( CSLFetchNameValue( psOptions->papszWarpOptions, "INIT_DEST" ) != nullptr );

    const char* pszWarpThreads = CSLFetchNameValue(psOptions->papszWarpOptions, "NUM_THREADS");
    if( pszWarpThreads != nullptr )
    {
        /* Used by TPS transformer to parallelize direct and inverse matrix computation */
        psOptions->papszTO = CSLSetNameValue(psOptions->papszTO, "NUM_THREADS", pszWarpThreads);
    }

    if( hDstDS == nullptr )
    {
        if( psOptions->pszFormat == nullptr )
        {
            CPLString osFormat = GetOutputDriverForRaster(pszDest);
            if( osFormat.empty() )
            {
                GDALDestroyTransformer( hUniqueTransformArg );
                GDALWarpAppOptionsFree(psOptions);
                OGR_G_DestroyGeometry( hCutline );
                return nullptr;
            }
            psOptions->pszFormat = CPLStrdup(osFormat);
        }

        if( nSrcCount == 1 && pahSrcDS[0] != nullptr && !psOptions->bDisableSrcAlpha )
        {
            if( GDALGetRasterCount(pahSrcDS[0]) > 0 &&
                GDALGetRasterColorInterpretation(
                    GDALGetRasterBand(pahSrcDS[0],GDALGetRasterCount(pahSrcDS[0])) )
                == GCI_AlphaBand )
            {
                psOptions->bEnableSrcAlpha = true;
                psOptions->bEnableDstAlpha = true;
                if( !psOptions->bQuiet )
                    printf( "Using band %d of source image as alpha.\n",
                            GDALGetRasterCount(pahSrcDS[0]) );
            }
        }

        hDstDS = GDALWarpCreateOutput( nSrcCount, pahSrcDS, pszDest,psOptions->pszFormat,
                                       psOptions->papszTO, &psOptions->papszCreateOptions,
                                       psOptions->eOutputType, &hUniqueTransformArg,
                                       psOptions->bSetColorInterpretation,
                                       psOptions);
        if(hDstDS == nullptr)
        {
            GDALDestroyTransformer( hUniqueTransformArg );
            GDALWarpAppOptionsFree(psOptions);
            OGR_G_DestroyGeometry( hCutline );
            return nullptr;
        }
#ifdef DEBUG
        poDstDS = reinterpret_cast<GDALDataset*>(hDstDS);
#endif
        psOptions->bCreateOutput = true;

        if( !bInitDestSetByUser )
        {
            if ( psOptions->pszDstNodata == nullptr )
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
        psOptions->papszCreateOptions = nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Detect if output has alpha channel.                             */
/* -------------------------------------------------------------------- */
    bool bEnableDstAlpha = psOptions->bEnableDstAlpha;
    if( !bEnableDstAlpha
        && GDALGetRasterCount(hDstDS)
        && GDALGetRasterColorInterpretation(
            GDALGetRasterBand(hDstDS,GDALGetRasterCount(hDstDS)))
        == GCI_AlphaBand && !psOptions->bDisableSrcAlpha )
    {
        if( !psOptions->bQuiet )
            printf( "Using band %d of destination image as alpha.\n",
                    GDALGetRasterCount(hDstDS) );

        bEnableDstAlpha = true;
    }

/* -------------------------------------------------------------------- */
/*      Create global progress function.                                */
/* -------------------------------------------------------------------- */
    struct Progress
    {
        GDALProgressFunc pfnExternalProgress;
        void* pExternalProgressData;
        int iSrc;
        int nSrcCount;
        GDALDatasetH *pahSrcDS;

        int Do(double dfComplete)
        {
            CPLString osMsg;
            osMsg.Printf("Processing %s [%d/%d]",
                         GDALGetDescription(pahSrcDS[iSrc]),
                         iSrc + 1,
                         nSrcCount);
            return pfnExternalProgress( (iSrc + dfComplete ) / nSrcCount,
                                        osMsg.c_str(),
                                        pExternalProgressData );
        }

        static int CPL_STDCALL ProgressFunc(double dfComplete,
                                            const char *, void* pThis)
        {
            return static_cast<Progress*>(pThis)->Do(dfComplete);
        }
    };

    Progress oProgress;
    oProgress.pfnExternalProgress = psOptions->pfnProgress;
    oProgress.pExternalProgressData = psOptions->pProgressData;
    oProgress.nSrcCount = nSrcCount;
    oProgress.pahSrcDS = pahSrcDS;

/* -------------------------------------------------------------------- */
/*      Loop over all source files, processing each in turn.            */
/* -------------------------------------------------------------------- */
    for( int iSrc = 0; iSrc < nSrcCount; iSrc++ )
    {
        GDALDatasetH hSrcDS;

/* -------------------------------------------------------------------- */
/*      Open this file.                                                 */
/* -------------------------------------------------------------------- */
        hSrcDS = pahSrcDS[iSrc];
        if( hSrcDS == nullptr )
        {
            GDALWarpAppOptionsFree(psOptions);
            OGR_G_DestroyGeometry( hCutline );
            GDALReleaseDataset(hDstDS);
            return nullptr;
        }

        oProgress.iSrc = iSrc;
        oProgress.Do(0);

/* -------------------------------------------------------------------- */
/*      Check that there's at least one raster band                     */
/* -------------------------------------------------------------------- */
        if ( GDALGetRasterCount(hSrcDS) == 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Input file %s has no raster bands.", GDALGetDescription(hSrcDS) );
            GDALWarpAppOptionsFree(psOptions);
            OGR_G_DestroyGeometry( hCutline );
            GDALReleaseDataset(hDstDS);
            return nullptr;
        }

/* -------------------------------------------------------------------- */
/*      Do we have a source alpha band?                                 */
/* -------------------------------------------------------------------- */
        bool bEnableSrcAlpha = psOptions->bEnableSrcAlpha;
        if( GDALGetRasterColorInterpretation(
                GDALGetRasterBand(hSrcDS,GDALGetRasterCount(hSrcDS)) )
            == GCI_AlphaBand
            && !bEnableSrcAlpha && !psOptions->bDisableSrcAlpha )
        {
            bEnableSrcAlpha = true;
            if( !psOptions->bQuiet )
                printf( "Using band %d of source image as alpha.\n",
                        GDALGetRasterCount(hSrcDS) );
        }

/* -------------------------------------------------------------------- */
/*      Get the metadata of the first source DS and copy it to the      */
/*      destination DS. Copy Band-level metadata and other info, only   */
/*      if source and destination band count are equal. Any values that */
/*      conflict between source datasets are set to pszMDConflictValue. */
/* -------------------------------------------------------------------- */
        if ( psOptions->bCopyMetadata )
        {
            const char *pszSrcInfo = nullptr;
            GDALRasterBandH hSrcBand = nullptr;
            GDALRasterBandH hDstBand = nullptr;

            /* copy metadata from first dataset */
            if ( iSrc == 0 )
            {
                CPLDebug("WARP", "Copying metadata from first source to destination dataset");
                /* copy dataset-level metadata */
                char **papszMetadata = GDALGetMetadata(hSrcDS, nullptr);

                char** papszMetadataNew = nullptr;
                for( int i = 0; papszMetadata != nullptr && papszMetadata[i] != nullptr; i++ )
                {
                    // Do not preserve NODATA_VALUES when the output includes an alpha band
                    if( bEnableDstAlpha &&
                        STARTS_WITH_CI(papszMetadata[i], "NODATA_VALUES=") )
                    {
                        continue;
                    }

                    papszMetadataNew = CSLAddString(papszMetadataNew, papszMetadata[i]);
                }

                if ( CSLCount(papszMetadataNew) > 0 ) {
                    if ( GDALSetMetadata( hDstDS, papszMetadataNew, nullptr ) != CE_None )
                         CPLError( CE_Warning, CPLE_AppDefined,
                                  "error copying metadata to destination dataset." );
                }

                CSLDestroy(papszMetadataNew);

                /* ISIS3 -> ISIS3 special case */
                if( psOptions->pszFormat != nullptr &&
                    EQUAL(psOptions->pszFormat, "ISIS3") )
                {
                    char** papszMD_ISIS3 = GDALGetMetadata( hSrcDS, "json:ISIS3");
                    if( papszMD_ISIS3 != nullptr)
                        GDALSetMetadata(hDstDS, papszMD_ISIS3, "json:ISIS3");
                }
                else if( psOptions->pszFormat != nullptr &&
                         EQUAL(psOptions->pszFormat, "PDS4") )
                {
                    char** papszMD_PDS4 = GDALGetMetadata( hSrcDS, "xml:PDS4");
                    if( papszMD_PDS4 != nullptr)
                        GDALSetMetadata(hDstDS, papszMD_PDS4, "xml:PDS4");
                }

                /* copy band-level metadata and other info */
                if ( GDALGetRasterCount( hSrcDS ) == GDALGetRasterCount( hDstDS ) )
                {
                    for ( int iBand = 0; iBand < GDALGetRasterCount( hSrcDS ); iBand++ )
                    {
                        hSrcBand = GDALGetRasterBand( hSrcDS, iBand + 1 );
                        hDstBand = GDALGetRasterBand( hDstDS, iBand + 1 );
                        /* copy metadata, except stats (#5319) */
                        papszMetadata = GDALGetMetadata( hSrcBand, nullptr);
                        if ( CSLCount(papszMetadata) > 0 )
                        {
                            //GDALSetMetadata( hDstBand, papszMetadata, NULL );
                            papszMetadataNew = nullptr;
                            for( int i = 0; papszMetadata != nullptr && papszMetadata[i] != nullptr; i++ )
                            {
                                if (!STARTS_WITH(papszMetadata[i], "STATISTICS_"))
                                    papszMetadataNew = CSLAddString(papszMetadataNew, papszMetadata[i]);
                            }
                            GDALSetMetadata( hDstBand, papszMetadataNew, nullptr );
                            CSLDestroy(papszMetadataNew);
                        }
                        /* copy other info (Description, Unit Type) - what else? */
                        if ( psOptions->bCopyBandInfo ) {
                            pszSrcInfo = GDALGetDescription( hSrcBand );
                            if(  pszSrcInfo != nullptr && strlen(pszSrcInfo) > 0 )
                                GDALSetDescription( hDstBand, pszSrcInfo );
                            pszSrcInfo = GDALGetRasterUnitType( hSrcBand );
                            if(  pszSrcInfo != nullptr && strlen(pszSrcInfo) > 0 )
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
                RemoveConflictingMetadata( hDstDS, GDALGetMetadata( hSrcDS, nullptr ), psOptions->pszMDConflictValue );

                /* remove conflicting copy band-level metadata and other info */
                if ( GDALGetRasterCount( hSrcDS ) == GDALGetRasterCount( hDstDS ) )
                {
                    for ( int iBand = 0; iBand < GDALGetRasterCount( hSrcDS ); iBand++ )
                    {
                        hSrcBand = GDALGetRasterBand( hSrcDS, iBand + 1 );
                        hDstBand = GDALGetRasterBand( hDstDS, iBand + 1 );
                        /* remove conflicting metadata */
                        RemoveConflictingMetadata( hDstBand, GDALGetMetadata( hSrcBand, nullptr ), psOptions->pszMDConflictValue );
                        /* remove conflicting info */
                        if ( psOptions->bCopyBandInfo ) {
                            pszSrcInfo = GDALGetDescription( hSrcBand );
                            const char *pszDstInfo =
                                GDALGetDescription(hDstBand);
                            if( ! ( pszSrcInfo != nullptr && strlen(pszSrcInfo) > 0  &&
                                    pszDstInfo != nullptr && strlen(pszDstInfo) > 0  &&
                                    EQUAL( pszSrcInfo, pszDstInfo ) ) )
                                GDALSetDescription( hDstBand, "" );
                            pszSrcInfo = GDALGetRasterUnitType( hSrcBand );
                            pszDstInfo = GDALGetRasterUnitType( hDstBand );
                            if( ! ( pszSrcInfo != nullptr && strlen(pszSrcInfo) > 0  &&
                                    pszDstInfo != nullptr && strlen(pszDstInfo) > 0  &&
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
             GDALGetRasterColorTable(GDALGetRasterBand(hSrcDS, 1)) != nullptr)
        {
            if( !psOptions->bQuiet )
                CPLError( CE_Warning, CPLE_AppDefined, "Input file %s has a color table, which will likely lead to "
                        "bad results when using a resampling method other than "
                        "nearest neighbour or mode. Converting the dataset prior to 24/32 bit "
                        "is advised.", GDALGetDescription(hSrcDS) );
        }

/* -------------------------------------------------------------------- */
/*      For RPC warping add a few extra source pixels by default        */
/*      (probably mostly needed in the RPC DEM case)                    */
/* -------------------------------------------------------------------- */

        const char *pszMethod = CSLFetchNameValue(
            psOptions->papszTO, "METHOD" );
        if( iSrc == 0 &&
            CSLFetchNameValue(psOptions->papszWarpOptions,
                              "SOURCE_EXTRA") == nullptr &&
            ( GDALGetMetadata( hSrcDS, "RPC" ) != nullptr &&
             (pszMethod == nullptr || EQUAL(pszMethod,"RPC") ) ) )
        {
            psOptions->papszWarpOptions = CSLSetNameValue(
                psOptions->papszWarpOptions, "SOURCE_EXTRA", "5");
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

        if( hTransformArg == nullptr )
        {
            GDALWarpAppOptionsFree(psOptions);
            OGR_G_DestroyGeometry( hCutline );
            GDALReleaseDataset(hDstDS);
            return nullptr;
        }

        pfnTransformer = GDALGenImgProjTransform;

/* -------------------------------------------------------------------- */
/*      Determine if we must work with the full-resolution source       */
/*      dataset, or one of its overview level.                          */
/* -------------------------------------------------------------------- */
        GDALDataset* poSrcDS = static_cast<GDALDataset*>(hSrcDS);
        GDALDataset* poSrcOvrDS = nullptr;
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
                    int iOvr = -1;
                    for( ; iOvr < nOvCount-1; iOvr++ )
                    {
                        const double dfOvrRatio =
                            iOvr < 0
                            ? 1.0
                            : static_cast<double>(poSrcDS->GetRasterXSize()) /
                              poSrcDS->GetRasterBand(1)->GetOverview(iOvr)->
                                  GetXSize();
                        const double dfNextOvrRatio =
                            static_cast<double>(poSrcDS->GetRasterXSize()) /
                            poSrcDS->GetRasterBand(1)->GetOverview(iOvr+1)->
                                GetXSize();
                        if( dfOvrRatio < dfTargetRatio &&
                            dfNextOvrRatio > dfTargetRatio )
                            break;
                        if( fabs(dfOvrRatio - dfTargetRatio) < 1e-1 )
                            break;
                    }
                    iOvr += (psOptions->nOvLevel+2);
                    if( iOvr >= 0 )
                    {
                        CPLDebug("WARP", "Selecting overview level %d for %s",
                                 iOvr, GDALGetDescription(hSrcDS));
                        poSrcOvrDS = GDALCreateOverviewDataset( poSrcDS, iOvr, FALSE );
                    }
                }
            }
        }
        else if( psOptions->nOvLevel >= 0 )
        {
            poSrcOvrDS = GDALCreateOverviewDataset( poSrcDS, psOptions->nOvLevel, TRUE );
            if( poSrcOvrDS == nullptr )
            {
                if( !psOptions->bQuiet )
                {
                    CPLError(CE_Warning, CPLE_AppDefined, "cannot get overview level %d for "
                            "dataset %s. Defaulting to level %d",
                            psOptions->nOvLevel, GDALGetDescription(hSrcDS), nOvCount - 1);
                }
                if( nOvCount > 0 )
                    poSrcOvrDS = GDALCreateOverviewDataset( poSrcDS, nOvCount - 1, FALSE );
            }
            else
            {
                CPLDebug("WARP", "Selecting overview level %d for %s",
                         psOptions->nOvLevel, GDALGetDescription(hSrcDS));
            }
        }

        if( poSrcOvrDS == nullptr )
            GDALReferenceDataset(hSrcDS);

        GDALDatasetH hWrkSrcDS =
            poSrcOvrDS ? static_cast<GDALDatasetH>(poSrcOvrDS) : hSrcDS;

        if( !psOptions->bNoVShiftGrid )
        {
            bool bErrorOccurred = false;
            hWrkSrcDS = ApplyVerticalShiftGrid( hWrkSrcDS,
                                                psOptions,
                                                bVRT ? hDstDS : nullptr,
                                                bErrorOccurred );
            if( bErrorOccurred )
            {
                GDALDestroyTransformer( hTransformArg );
                GDALWarpAppOptionsFree(psOptions);
                OGR_G_DestroyGeometry( hCutline );
                GDALReleaseDataset(hWrkSrcDS);
                GDALReleaseDataset(hDstDS);
                return nullptr;
            }
        }

        /* We need to recreate the transform when operating on an overview */
        if( poSrcOvrDS != nullptr )
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
                                                "INIT_DEST", nullptr );

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

        if( !bVRT )
        {
            psWO->pfnProgress = Progress::ProgressFunc;
            psWO->pProgressArg = &oProgress;
        }

        if( psOptions->dfWarpMemoryLimit != 0.0 )
            psWO->dfWarpMemoryLimit = psOptions->dfWarpMemoryLimit;

/* -------------------------------------------------------------------- */
/*      Setup band mapping.                                             */
/* -------------------------------------------------------------------- */
        if( bEnableSrcAlpha )
            psWO->nBandCount = GDALGetRasterCount(hWrkSrcDS) - 1;
        else
            psWO->nBandCount = GDALGetRasterCount(hWrkSrcDS);

        psWO->panSrcBands =
            static_cast<int *>(CPLMalloc(psWO->nBandCount*sizeof(int)));
        psWO->panDstBands =
            static_cast<int *>(CPLMalloc(psWO->nBandCount*sizeof(int)));

        for( int i = 0; i < psWO->nBandCount; i++ )
        {
            psWO->panSrcBands[i] = i+1;
            psWO->panDstBands[i] = i+1;
        }

/* -------------------------------------------------------------------- */
/*      Setup alpha bands used if any.                                  */
/* -------------------------------------------------------------------- */
        if( bEnableSrcAlpha )
            psWO->nSrcAlphaBand = GDALGetRasterCount(hWrkSrcDS);

        if( bEnableDstAlpha )
            psWO->nDstAlphaBand = GDALGetRasterCount(hDstDS);

/* -------------------------------------------------------------------- */
/*      Setup NODATA options.                                           */
/* -------------------------------------------------------------------- */
        if( psOptions->pszSrcNodata != nullptr && !EQUAL(psOptions->pszSrcNodata,"none") )
        {
            char **papszTokens = CSLTokenizeString( psOptions->pszSrcNodata );
            const int nTokenCount = CSLCount(papszTokens);

            psWO->padfSrcNoDataReal = static_cast<double *>(
                CPLMalloc(psWO->nBandCount*sizeof(double)));
            psWO->padfSrcNoDataImag = nullptr;

            for( int i = 0; i < psWO->nBandCount; i++ )
            {
                if( i < nTokenCount )
                {
                    if( strchr(papszTokens[i], 'i') != nullptr)
                    {
                        if( psWO->padfSrcNoDataImag == nullptr )
                        {
                            psWO->padfSrcNoDataImag = static_cast<double *>(
                                CPLCalloc(psWO->nBandCount, sizeof(double)));
                        }
                        CPLStringToComplex( papszTokens[i],
                                            psWO->padfSrcNoDataReal + i,
                                            psWO->padfSrcNoDataImag + i );
                    }
                    else
                    {
                        psWO->padfSrcNoDataReal[i] = CPLAtof(papszTokens[i]);
                    }
                }
                else
                {
                    psWO->padfSrcNoDataReal[i] = psWO->padfSrcNoDataReal[i-1];
                    if( psWO->padfSrcNoDataImag != nullptr )
                    {
                        psWO->padfSrcNoDataImag[i] = psWO->padfSrcNoDataImag[i-1];
                    }
                }
            }

            CSLDestroy( papszTokens );

            if( psWO->nBandCount > 1 &&
                CSLFetchNameValue(psWO->papszWarpOptions, "UNIFIED_SRC_NODATA") == nullptr )
            {
                CPLDebug("WARP", "Set UNIFIED_SRC_NODATA=YES");
                psWO->papszWarpOptions = CSLSetNameValue(psWO->papszWarpOptions,
                                               "UNIFIED_SRC_NODATA", "YES" );
            }
        }

/* -------------------------------------------------------------------- */
/*      If -srcnodata was not specified, but the data has nodata        */
/*      values, use them.                                               */
/* -------------------------------------------------------------------- */
        if( psOptions->pszSrcNodata == nullptr )
        {
            int bHaveNodata = FALSE;
            double dfReal = 0.0;

            for( int i = 0; !bHaveNodata && i < psWO->nBandCount; i++ )
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
                psWO->padfSrcNoDataReal = static_cast<double *>(
                    CPLMalloc(psWO->nBandCount*sizeof(double)));

                for( int i = 0; i < psWO->nBandCount; i++ )
                {
                    GDALRasterBandH hBand = GDALGetRasterBand( hWrkSrcDS, i+1 );

                    dfReal = GDALGetRasterNoDataValue( hBand, &bHaveNodata );

                    if( bHaveNodata )
                    {
                        psWO->padfSrcNoDataReal[i] = dfReal;
                    }
                    else
                    {
                        psWO->padfSrcNoDataReal[i] = -123456.789;
                    }
                }
            }
        }

/* -------------------------------------------------------------------- */
/*      If the output dataset was created, and we have a destination    */
/*      nodata value, go through marking the bands with the information.*/
/* -------------------------------------------------------------------- */
        if( psOptions->pszDstNodata != nullptr && !EQUAL(psOptions->pszDstNodata,"none") )
        {
            char **papszTokens = CSLTokenizeString( psOptions->pszDstNodata );
            const int nTokenCount = CSLCount(papszTokens);
            bool bDstNoDataNone = true;

            psWO->padfDstNoDataReal = static_cast<double *>(
                CPLMalloc(psWO->nBandCount*sizeof(double)));
            psWO->padfDstNoDataImag = static_cast<double *>(
                CPLMalloc(psWO->nBandCount*sizeof(double)));

            for( int i = 0; i < psWO->nBandCount; i++ )
            {
                psWO->padfDstNoDataReal[i] = -1.1e20;
                psWO->padfDstNoDataImag[i] = 0.0;

                if( i < nTokenCount )
                {
                    if ( papszTokens[i] != nullptr && EQUAL(papszTokens[i],"none") )
                    {
                        CPLDebug( "WARP", "dstnodata of band %d not set", i );
                        bDstNoDataNone = true;
                        continue;
                    }
                    else if ( papszTokens[i] == nullptr ) // this should not happen, but just in case
                    {
                        CPLError( CE_Failure, CPLE_AppDefined, "Error parsing dstnodata arg #%d", i );
                        bDstNoDataNone = true;
                        continue;
                    }
                    CPLStringToComplex( papszTokens[i],
                                        psWO->padfDstNoDataReal + i,
                                        psWO->padfDstNoDataImag + i );
                    bDstNoDataNone = false;
                    CPLDebug( "WARP", "dstnodata of band %d set to %f", i, psWO->padfDstNoDataReal[i] );
                }
                else
                {
                    if( !bDstNoDataNone )
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
                int bClamped = FALSE;
                int bRounded = FALSE;
                psWO->padfDstNoDataReal[i] = GDALAdjustValueToDataType(
                                                     GDALGetRasterDataType(hBand),
                                                     psWO->padfDstNoDataReal[i],
                                                     &bClamped, &bRounded );

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

                if( psOptions->bCreateOutput && iSrc == 0 )
                {
                    GDALSetRasterNoDataValue(
                        GDALGetRasterBand( hDstDS, psWO->panDstBands[i] ),
                        psWO->padfDstNoDataReal[i] );
                }
            }

            CSLDestroy( papszTokens );
        }

        /* check if the output dataset has already nodata */
        if ( psOptions->pszDstNodata == nullptr )
        {
            int bHaveNodataAll = TRUE;
            for( int i = 0; i < psWO->nBandCount; i++ )
            {
                GDALRasterBandH hBand = GDALGetRasterBand( hDstDS, i+1 );
                int bHaveNodata = FALSE;
                GDALGetRasterNoDataValue( hBand, &bHaveNodata );
                bHaveNodataAll &= bHaveNodata;
            }
            if( bHaveNodataAll )
            {
                psWO->padfDstNoDataReal = static_cast<double *>(
                    CPLMalloc(psWO->nBandCount*sizeof(double)));
                for( int i = 0; i < psWO->nBandCount; i++ )
                {
                    GDALRasterBandH hBand = GDALGetRasterBand( hDstDS, i+1 );
                    int bHaveNodata = FALSE;
                    psWO->padfDstNoDataReal[i] = GDALGetRasterNoDataValue( hBand, &bHaveNodata );
                    CPLDebug("WARP", "band=%d dstNoData=%f", i, psWO->padfDstNoDataReal[i] );
                }
            }
        }

        // If creating a new file that has default nodata value,
        // try to override the default output nodata values with the source ones.
        if( psOptions->pszDstNodata == nullptr &&
            psWO->padfSrcNoDataReal != nullptr &&
            psWO->padfDstNoDataReal != nullptr &&
            psOptions->bCreateOutput && iSrc == 0 && !bEnableDstAlpha )
        {
            for( int i = 0; i < psWO->nBandCount; i++ )
            {
                GDALRasterBandH hBand = GDALGetRasterBand( hDstDS, i+1 );
                int bHaveNodata = FALSE;
                CPLPushErrorHandler(CPLQuietErrorHandler);
                bool bRedefinedOK =
                  ( GDALSetRasterNoDataValue(hBand,
                            psWO->padfSrcNoDataReal[i]) == CE_None &&
                    GDALGetRasterNoDataValue(hBand,
                            &bHaveNodata) == psWO->padfSrcNoDataReal[i] &&
                    bHaveNodata );
                CPLPopErrorHandler();
                if( bRedefinedOK )
                {
                    if( i == 0 && !psOptions->bQuiet )
                        printf( "Copying nodata values from source %s "
                                "to destination %s.\n",
                                GDALGetDescription(hSrcDS), pszDest );
                    psWO->padfDstNoDataReal[i] = 
                        psWO->padfSrcNoDataReal[i];

                    if( i == 0 && psOptions->bCreateOutput &&
                        !bInitDestSetByUser && iSrc == 0 )
                    {
                        /* As we didn't know at the beginning if there was source nodata */
                        /* we have initialized INIT_DEST=0. Override this with NO_DATA now */
                        psWO->papszWarpOptions = CSLSetNameValue(
                            psWO->papszWarpOptions, "INIT_DEST", "NO_DATA" );
                    }
                }
                else
                {
                    break;
                }
            }
        }

        /* else try to fill dstNoData from source bands, unless -dstalpha is specified */
        else if ( psOptions->pszDstNodata == nullptr &&
                  psWO->padfSrcNoDataReal != nullptr &&
                  psWO->padfDstNoDataReal == nullptr && !bEnableDstAlpha )
        {
            psWO->padfDstNoDataReal = static_cast<double *>(
                CPLMalloc(psWO->nBandCount*sizeof(double)));

            if( psWO->padfSrcNoDataImag != nullptr)
            {
                psWO->padfDstNoDataImag = static_cast<double *>(
                    CPLMalloc(psWO->nBandCount*sizeof(double)));
            }

            if( !psOptions->bQuiet )
                printf( "Copying nodata values from source %s to destination %s.\n",
                        GDALGetDescription(hSrcDS), pszDest );

            for( int i = 0; i < psWO->nBandCount; i++ )
            {
                psWO->padfDstNoDataReal[i] = psWO->padfSrcNoDataReal[i];
                if( psWO->padfSrcNoDataImag != nullptr)
                {
                    psWO->padfDstNoDataImag[i] = psWO->padfSrcNoDataImag[i];
                }
                CPLDebug("WARP", "srcNoData=%f dstNoData=%f",
                            psWO->padfSrcNoDataReal[i], psWO->padfDstNoDataReal[i] );

                if( psOptions->bCreateOutput && iSrc == 0 )
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
        if( hCutline != nullptr )
        {
            CPLErr eError;
            eError = TransformCutlineToSource( hWrkSrcDS, hCutline,
                                      &(psWO->papszWarpOptions),
                                      psOptions->papszTO );
            if(eError == CE_Failure)
            {
                GDALDestroyTransformer( hTransformArg );
                GDALDestroyWarpOptions( psWO );
                GDALWarpAppOptionsFree(psOptions);
                OGR_G_DestroyGeometry( hCutline );
                GDALReleaseDataset(hWrkSrcDS);
                GDALReleaseDataset(hDstDS);
                return nullptr;
            }
        }

/* -------------------------------------------------------------------- */
/*      If we are producing VRT output, then just initialize it with    */
/*      the warp options and write out now rather than proceeding       */
/*      with the operations.                                            */
/* -------------------------------------------------------------------- */
        if( bVRT )
        {
            GDALSetMetadataItem(hDstDS, "SrcOvrLevel", CPLSPrintf("%d", psOptions->nOvLevel), nullptr);
            CPLErr eErr = GDALInitializeWarpedVRT( hDstDS, psWO );
            GDALDestroyWarpOptions( psWO );
            GDALWarpAppOptionsFree(psOptions);
            OGR_G_DestroyGeometry( hCutline );
            GDALReleaseDataset(hWrkSrcDS);
            if( eErr != CE_None )
            {
                GDALDestroyTransformer( hTransformArg );
                GDALReleaseDataset(hDstDS);
                return nullptr;
            }
            // In case of success, hDstDS has become the owner of hTransformArg
            // so do not free it.
            if( !EQUAL(pszDest, "") )
            {
                CPLErr eErrBefore = CPLGetLastErrorType();
                GDALFlushCache( hDstDS );
                if (eErrBefore == CE_None &&
                    CPLGetLastErrorType() != CE_None)
                {
                    GDALReleaseDataset(hDstDS);
                    hDstDS = nullptr;
                }
            }

            if( hDstDS )
                oProgress.Do(1);

            return hDstDS;
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
                bHasGotErr = true;
        }
        else
        {
            bHasGotErr = true;
        }

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
        GDALDestroyTransformer( hTransformArg );

        GDALDestroyWarpOptions( psWO );

        GDALReleaseDataset(hWrkSrcDS);
    }

/* -------------------------------------------------------------------- */
/*      Final Cleanup.                                                  */
/* -------------------------------------------------------------------- */
    CPLErr eErrBefore = CPLGetLastErrorType();
    GDALFlushCache( hDstDS );
    if (eErrBefore == CE_None &&
        CPLGetLastErrorType() != CE_None)
    {
        bHasGotErr = true;
    }

    OGR_G_DestroyGeometry( hCutline );

    GDALWarpAppOptionsFree(psOptions);

    if( bHasGotErr || bDropDstDSRef )
        GDALReleaseDataset(hDstDS);

#ifdef DEBUG
    if( !bHasGotErr || bDropDstDSRef )
    {
        CPLAssert( poDstDS->GetRefCount() == nExpectedRefCountAtEnd );
    }
#endif

    return bHasGotErr ? nullptr : hDstDS;
}

/************************************************************************/
/*                          ValidateCutline()                           */
/************************************************************************/

static bool ValidateCutline(OGRGeometryH hGeom)
{
    OGRwkbGeometryType eType = wkbFlatten(OGR_G_GetGeometryType( hGeom ));
    if( eType == wkbMultiPolygon )
    {
        for( int iGeom = 0; iGeom < OGR_G_GetGeometryCount( hGeom ); iGeom++ )
        {
            OGRGeometryH hPoly = OGR_G_GetGeometryRef(hGeom,iGeom);
            if( !ValidateCutline(hPoly) )
                return false;
        }
    }
    else if( eType == wkbPolygon )
    {
        if( OGRGeometryFactory::haveGEOS() && !OGR_G_IsValid(hGeom) )
        {
            char *pszWKT = nullptr;
            OGR_G_ExportToWkt( hGeom, &pszWKT );
            CPLDebug("GDALWARP", "WKT = \"%s\"", pszWKT ? pszWKT : "(null)");
            const char* pszFile = CPLGetConfigOption("GDALWARP_DUMP_WKT_TO_FILE", nullptr);
            if( pszFile && pszWKT )
            {
                FILE* f = EQUAL(pszFile, "stderr") ? stderr : fopen(pszFile, "wb");
                if( f )
                {
                    fprintf(f, "id,WKT\n");
                    fprintf(f, "1,\"%s\"\n", pszWKT);
                    if( !EQUAL(pszFile, "stderr") )
                        fclose(f);
                }
            }
            CPLFree( pszWKT );

            if( CPLTestBool(CPLGetConfigOption("GDALWARP_IGNORE_BAD_CUTLINE", "NO")) )
                CPLError(CE_Warning, CPLE_AppDefined, "Cutline polygon is invalid.");
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Cutline polygon is invalid.");
                return false;
            }
        }
    }
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined, "Cutline not of polygon type." );
        return false;
    }

    return true;
}

/************************************************************************/
/*                     LooseValidateCutline()                           */
/*                                                                      */
/*  Same as OGR_G_IsValid() except that it processes polygon per polygon*/
/*  without paying attention to MultiPolygon specific validity rules.   */
/************************************************************************/

static bool LooseValidateCutline(OGRGeometryH hGeom)
{
    OGRwkbGeometryType eType = wkbFlatten(OGR_G_GetGeometryType( hGeom ));
    if( eType == wkbMultiPolygon )
    {
        for( int iGeom = 0; iGeom < OGR_G_GetGeometryCount( hGeom ); iGeom++ )
        {
            OGRGeometryH hPoly = OGR_G_GetGeometryRef(hGeom,iGeom);
            if( !LooseValidateCutline(hPoly) )
                return false;
        }
    }
    else if( eType == wkbPolygon )
    {
        if( OGRGeometryFactory::haveGEOS() && !OGR_G_IsValid(hGeom) )
        {
            return false;
        }
    }
    else
    {
        return false;
    }

    return true;
}

/************************************************************************/
/*                            LoadCutline()                             */
/*                                                                      */
/*      Load blend cutline from OGR datasource.                         */
/************************************************************************/

static CPLErr
LoadCutline( const char *pszCutlineDSName, const char *pszCLayer,
             const char *pszCWHERE, const char *pszCSQL,
             OGRGeometryH* phCutlineRet )

{
    OGRRegisterAll();

/* -------------------------------------------------------------------- */
/*      Open source vector dataset.                                     */
/* -------------------------------------------------------------------- */
    OGRDataSourceH hSrcDS;

    hSrcDS = OGROpen( pszCutlineDSName, FALSE, nullptr );
    if( hSrcDS == nullptr )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "Cannot open %s.", pszCutlineDSName);
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Get the source layer                                            */
/* -------------------------------------------------------------------- */
    OGRLayerH hLayer = nullptr;

    if( pszCSQL != nullptr )
        hLayer = OGR_DS_ExecuteSQL( hSrcDS, pszCSQL, nullptr, nullptr );
    else if( pszCLayer != nullptr )
        hLayer = OGR_DS_GetLayerByName( hSrcDS, pszCLayer );
    else
        hLayer = OGR_DS_GetLayer( hSrcDS, 0 );

    if( hLayer == nullptr )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "Failed to identify source layer from datasource." );
        OGR_DS_Destroy( hSrcDS );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Apply WHERE clause if there is one.                             */
/* -------------------------------------------------------------------- */
    if( pszCWHERE != nullptr )
        OGR_L_SetAttributeFilter( hLayer, pszCWHERE );

/* -------------------------------------------------------------------- */
/*      Collect the geometries from this layer, and build list of       */
/*      burn values.                                                    */
/* -------------------------------------------------------------------- */
    OGRFeatureH hFeat;
    OGRGeometryH hMultiPolygon = OGR_G_CreateGeometry( wkbMultiPolygon );

    OGR_L_ResetReading( hLayer );

    while( (hFeat = OGR_L_GetNextFeature( hLayer )) != nullptr )
    {
        OGRGeometryH hGeom = OGR_F_GetGeometryRef(hFeat);

        if( hGeom == nullptr )
        {
            CPLError( CE_Failure, CPLE_AppDefined, "Cutline feature without a geometry." );
            OGR_F_Destroy( hFeat );
            goto error;
        }

        if( !ValidateCutline(hGeom) )
        {
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

    *phCutlineRet = hMultiPolygon;

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    if( pszCSQL != nullptr )
        OGR_DS_ReleaseResultSet( hSrcDS, hLayer );

    OGR_DS_Destroy( hSrcDS );

    return CE_None;

error:
    OGR_G_DestroyGeometry(hMultiPolygon);
    if( pszCSQL != nullptr )
        OGR_DS_ReleaseResultSet( hSrcDS, hLayer );
    OGR_DS_Destroy( hSrcDS );
    return CE_Failure;
}

/************************************************************************/
/*                        GDALWarpCreateOutput()                        */
/*                                                                      */
/*      Create the output file based on various command line options,    */
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
                      bool bSetColorInterpretation,
                      GDALWarpAppOptions *psOptions)

{
    GDALDriverH hDriver;
    GDALDatasetH hDstDS;
    void *hTransformArg;
    GDALColorTableH hCT = nullptr;
    double dfWrkMinX=0, dfWrkMaxX=0, dfWrkMinY=0, dfWrkMaxY=0;
    double dfWrkResX=0, dfWrkResY=0;
    int nDstBandCount = 0;
    std::vector<GDALColorInterp> apeColorInterpretations;
    bool bVRT = false;

    if( EQUAL(pszFormat,"VRT") )
        bVRT = true;

    /* If (-ts and -te) or (-tr and -te) are specified, we don't need to compute the suggested output extent */
    const bool bNeedsSuggestedWarpOutput =
        !( ((psOptions->nForcePixels != 0 && psOptions->nForceLines != 0) ||
            (psOptions->dfXRes != 0 && psOptions->dfYRes != 0)) &&
           !(psOptions->dfMinX == 0.0 && psOptions->dfMinY == 0.0 &&
             psOptions->dfMaxX == 0.0 && psOptions->dfMaxY == 0.0) );

    *phTransformArg = nullptr;

/* -------------------------------------------------------------------- */
/*      Find the output driver.                                         */
/* -------------------------------------------------------------------- */
    hDriver = GDALGetDriverByName( pszFormat );
    if( hDriver == nullptr
        || GDALGetMetadataItem( hDriver, GDAL_DCAP_CREATE, nullptr ) == nullptr )
    {
        printf( "Output driver `%s' not recognised or does not support\n",
                pszFormat );
        printf( "direct output file creation.  The following format drivers are configured\n"
                "and support direct output:\n" );

        for( int iDr = 0; iDr < GDALGetDriverCount(); iDr++ )
        {
            hDriver = GDALGetDriver(iDr);

            if( GDALGetMetadataItem( hDriver, GDAL_DCAP_RASTER, nullptr) != nullptr &&
                GDALGetMetadataItem( hDriver, GDAL_DCAP_CREATE, nullptr) != nullptr )
            {
                printf( "  %s: %s\n",
                        GDALGetDriverShortName( hDriver  ),
                        GDALGetDriverLongName( hDriver ) );
            }
        }
        printf( "\n" );
        return nullptr;
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
    CPLString osThisTargetSRS;
    {
        const char *pszThisTargetSRS = CSLFetchNameValue( papszTO, "DST_SRS" );
        if( pszThisTargetSRS != nullptr )
            osThisTargetSRS = pszThisTargetSRS;
    }

    CPLStringList aoTOList(papszTO, FALSE);

    for( int iSrc = 0; iSrc < nSrcCount; iSrc++ )
    {
        if( pahSrcDS[iSrc] == nullptr )
        {
            if( hCT != nullptr )
                GDALDestroyColorTable( hCT );
            return nullptr;
        }

/* -------------------------------------------------------------------- */
/*      Check that there's at least one raster band                     */
/* -------------------------------------------------------------------- */
        if ( GDALGetRasterCount(pahSrcDS[iSrc]) == 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Input file %s has no raster bands.", GDALGetDescription(pahSrcDS[iSrc]) );
            if( hCT != nullptr )
                GDALDestroyColorTable( hCT );
            return nullptr;
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
            if( hCT != nullptr )
            {
                hCT = GDALCloneColorTable( hCT );
                if( !psOptions->bQuiet )
                    printf( "Copying color table from %s to new file.\n",
                            GDALGetDescription(pahSrcDS[iSrc]) );
            }

            for(int iBand = 0; iBand < nDstBandCount; iBand++)
            {
                GDALColorInterp eInterp =
                    GDALGetRasterColorInterpretation(GDALGetRasterBand(pahSrcDS[iSrc],iBand+1));
                apeColorInterpretations.push_back( eInterp );
            }
        }

/* -------------------------------------------------------------------- */
/*      If we are processing the first file, get the source srs from    */
/*      dataset, if not set already.                                    */
/* -------------------------------------------------------------------- */
        if( iSrc == 0 && osThisTargetSRS.empty() )
        {
            const char* pszThisSourceSRS = GetSrcDSProjection(
                                                    pahSrcDS[iSrc], papszTO );
            if( pszThisSourceSRS != nullptr && pszThisSourceSRS[0] != '\0' )
            {
                osThisTargetSRS = pszThisSourceSRS;
                aoTOList.SetNameValue("DST_SRS", pszThisSourceSRS);
            }
        }

/* -------------------------------------------------------------------- */
/*      Create a transformation object from the source to               */
/*      destination coordinate system.                                  */
/* -------------------------------------------------------------------- */
        hTransformArg =
            GDALCreateGenImgProjTransformer2( pahSrcDS[iSrc], nullptr, aoTOList.List() );

        if( hTransformArg == nullptr )
        {
            if( hCT != nullptr )
                GDALDestroyColorTable( hCT );
            return nullptr;
        }

        GDALTransformerInfo* psInfo = static_cast<GDALTransformerInfo*>(hTransformArg);

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
                if( hCT != nullptr )
                    GDALDestroyColorTable( hCT );
                GDALDestroyGenImgProjTransformer( hTransformArg );
                return nullptr;
            }

            if ( CPLGetConfigOption( "CHECK_WITH_INVERT_PROJ", nullptr ) == nullptr )
            {
                double MinX = adfExtent[0];
                double MaxX = adfExtent[2];
                double MaxY = adfExtent[3];
                double MinY = adfExtent[1];
                int bSuccess = TRUE;

                /* Check that the edges of the target image are in the validity area */
                /* of the target projection */
                const int N_STEPS = 20;
                for( int i = 0; i <= N_STEPS && bSuccess; i++ )
                {
                    for( int j = 0; j <= N_STEPS && bSuccess; j++ )
                    {
                        const double dfRatioI = i * 1.0 / N_STEPS;
                        const double dfRatioJ = j * 1.0 / N_STEPS;
                        const double expected_x =
                            (1 - dfRatioI) * MinX + dfRatioI * MaxX;
                        const double expected_y =
                            (1 - dfRatioJ) * MinY + dfRatioJ * MaxY;
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

                    const CPLErr eErr = GDALSuggestedWarpOutput2(
                        pahSrcDS[iSrc],
                        psInfo->pfnTransform, hTransformArg,
                        adfThisGeoTransform,
                        &nThisPixels, &nThisLines,
                        adfExtent, 0);
                    CPLSetThreadLocalConfigOption( "CHECK_WITH_INVERT_PROJ", nullptr );
                    if( eErr != CE_None )
                    {
                        if( hCT != nullptr )
                            GDALDestroyColorTable( hCT );
                        GDALDestroyGenImgProjTransformer( hTransformArg );
                        return nullptr;
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
                dfWrkResY = std::abs(adfThisGeoTransform[5]);
            }
            else
            {
                dfWrkMinX = std::min(dfWrkMinX, adfExtent[0]);
                dfWrkMaxX = std::max(dfWrkMaxX, adfExtent[2]);
                dfWrkMaxY = std::max(dfWrkMaxY, adfExtent[3]);
                dfWrkMinY = std::min(dfWrkMinY,adfExtent[1]);
                dfWrkResX = std::min(dfWrkResX,adfThisGeoTransform[1]);
                dfWrkResY =
                    std::min(dfWrkResY, std::abs(adfThisGeoTransform[5]));
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
        if( hCT != nullptr )
            GDALDestroyColorTable( hCT );
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Turn the suggested region into a geotransform and suggested     */
/*      number of pixels and lines.                                     */
/* -------------------------------------------------------------------- */
    double adfDstGeoTransform[6] = { 0, 0, 0, 0, 0, 0 };
    int nPixels = 0;
    int nLines = 0;

    if( bNeedsSuggestedWarpOutput )
    {
        adfDstGeoTransform[0] = dfWrkMinX;
        adfDstGeoTransform[1] = dfWrkResX;
        adfDstGeoTransform[2] = 0.0;
        adfDstGeoTransform[3] = dfWrkMaxY;
        adfDstGeoTransform[4] = 0.0;
        adfDstGeoTransform[5] = -1 * dfWrkResY;

        nPixels = static_cast<int>((dfWrkMaxX - dfWrkMinX) / dfWrkResX + 0.5);
        nLines = static_cast<int>((dfWrkMaxY - dfWrkMinY) / dfWrkResY + 0.5);
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

        nPixels = static_cast<int>(
            (psOptions->dfMaxX - psOptions->dfMinX + (psOptions->dfXRes/2.0)) /
            psOptions->dfXRes);
        nLines = static_cast<int>(
            (psOptions->dfMaxY - psOptions->dfMinY + (psOptions->dfYRes/2.0)) /
            psOptions->dfYRes);
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
        nLines = static_cast<int>(
            (psOptions->dfMaxY - psOptions->dfMinY + (psOptions->dfYRes/2.0)) /
            psOptions->dfYRes);
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

        nPixels = static_cast<int>(
            (psOptions->dfMaxX - psOptions->dfMinX + (psOptions->dfXRes/2.0)) /
            psOptions->dfXRes);
        nLines = psOptions->nForceLines;
    }

    else if( psOptions->dfMinX != 0.0 || psOptions->dfMinY != 0.0 || psOptions->dfMaxX != 0.0 || psOptions->dfMaxY != 0.0 )
    {
        psOptions->dfXRes = adfDstGeoTransform[1];
        psOptions->dfYRes = fabs(adfDstGeoTransform[5]);

        nPixels = static_cast<int>(
            (psOptions->dfMaxX - psOptions->dfMinX + (psOptions->dfXRes/2.0)) /
            psOptions->dfXRes);
        nLines = static_cast<int>(
            (psOptions->dfMaxY - psOptions->dfMinY + (psOptions->dfYRes/2.0)) /
            psOptions->dfYRes);

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

    if( EQUAL(pszFormat, "GTiff") )
    {

/* -------------------------------------------------------------------- */
/*      Automatically set PHOTOMETRIC=RGB for GTiff when appropriate    */
/* -------------------------------------------------------------------- */
        if ( apeColorInterpretations.size() >= 3 &&
            apeColorInterpretations[0] == GCI_RedBand &&
            apeColorInterpretations[1] == GCI_GreenBand &&
            apeColorInterpretations[2] == GCI_BlueBand &&
            CSLFetchNameValue( *ppapszCreateOptions, "PHOTOMETRIC" ) == nullptr )
        {
            *ppapszCreateOptions = CSLSetNameValue(*ppapszCreateOptions,
                                                "PHOTOMETRIC", "RGB");
        }

        /* The GTiff driver now supports writing band color interpretation */
        /* in the TIFF_GDAL_METADATA tag */
        bSetColorInterpretation = true;
    }

/* -------------------------------------------------------------------- */
/*      Create the output file.                                         */
/* -------------------------------------------------------------------- */
    if( !psOptions->bQuiet )
        printf( "Creating output file that is %dP x %dL.\n", nPixels, nLines );

    hDstDS = GDALCreate( hDriver, pszFilename, nPixels, nLines,
                         nDstBandCount, eDT, *ppapszCreateOptions );

    if( hDstDS == nullptr )
    {
        if( hCT != nullptr )
            GDALDestroyColorTable( hCT );
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Write out the projection definition.                            */
/* -------------------------------------------------------------------- */
    const char *pszDstMethod = CSLFetchNameValue(papszTO,"DST_METHOD");
    if( pszDstMethod == nullptr || !EQUAL(pszDstMethod, "NO_GEOTRANSFORM") )
    {
        if( GDALSetProjection( hDstDS, osThisTargetSRS.c_str() ) == CE_Failure ||
            GDALSetGeoTransform( hDstDS, adfDstGeoTransform ) == CE_Failure )
        {
            if( hCT != nullptr )
                GDALDestroyColorTable( hCT );
            GDALClose(hDstDS);
            return nullptr;
        }
    }
    else
    {
        adfDstGeoTransform[3] += adfDstGeoTransform[5] * nLines;
        adfDstGeoTransform[5] = fabs(adfDstGeoTransform[5]);
    }

    if (*phTransformArg != nullptr)
        GDALSetGenImgProjTransformerDstGeoTransform( *phTransformArg, adfDstGeoTransform);

/* -------------------------------------------------------------------- */
/*      Try to set color interpretation of source bands to target       */
/*      dataset.                                                        */
/*      FIXME? We should likely do that for other drivers than VRT &    */
/*      GTiff  but it might create spurious .aux.xml files (at least    */
/*      with HFA, and netCDF)                                           */
/* -------------------------------------------------------------------- */
    if( bVRT || bSetColorInterpretation )
    {
        int nBandsToCopy = static_cast<int>(apeColorInterpretations.size());
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
    if( hCT != nullptr )
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

    virtual OGRSpatialReference *GetSourceCS() override { return nullptr; }
    virtual OGRSpatialReference *GetTargetCS() override { return nullptr; }

    virtual int Transform( int nCount,
                           double *x, double *y, double *z = nullptr ) override {
        int *pabSuccess = static_cast<int *>(CPLCalloc(sizeof(int), nCount));
        const int nResult = TransformEx(nCount, x, y, z, pabSuccess);
        CPLFree( pabSuccess );

        return nResult;
    }

    virtual int TransformEx( int nCount,
                             double *x, double *y, double *z = nullptr,
                             int *pabSuccess = nullptr ) override {
        return GDALGenImgProjTransform( hSrcImageTransformer, TRUE,
                                        nCount, x, y, z, pabSuccess );
    }
};

static
double GetMaximumSegmentLength( OGRGeometry* poGeom )
{
    switch( wkbFlatten(poGeom->getGeometryType()) )
    {
        case wkbLineString:
        {
            OGRLineString* poLS = static_cast<OGRLineString*>(poGeom);
            double dfMaxSquaredLength = 0.0;
            for(int i=0; i<poLS->getNumPoints()-1;i++)
            {
                double dfDeltaX = poLS->getX(i+1) - poLS->getX(i);
                double dfDeltaY = poLS->getY(i+1) - poLS->getY(i);
                double dfSquaredLength = dfDeltaX * dfDeltaX + dfDeltaY * dfDeltaY;
                dfMaxSquaredLength = std::max( dfMaxSquaredLength, dfSquaredLength );
            }
            return sqrt(dfMaxSquaredLength);
        }

        case wkbPolygon:
        {
            OGRPolygon* poPoly = static_cast<OGRPolygon*>(poGeom);
            double dfMaxLength = GetMaximumSegmentLength( poPoly->getExteriorRing() );
            for(int i=0; i<poPoly->getNumInteriorRings();i++)
            {
                dfMaxLength = std::max( dfMaxLength,
                    GetMaximumSegmentLength( poPoly->getInteriorRing(i) ) );
            }
            return dfMaxLength;
        }

        case wkbMultiPolygon:
        {
            OGRMultiPolygon* poMP = static_cast<OGRMultiPolygon*>(poGeom);
            double dfMaxLength = 0.0;
            for(int i=0; i<poMP->getNumGeometries();i++)
            {
                dfMaxLength = std::max( dfMaxLength,
                    GetMaximumSegmentLength( poMP->getGeometryRef(i) ) );
            }
            return dfMaxLength;
        }

        default:
            CPLAssert(false);
            return 0.0;
    }
}

/************************************************************************/
/*                      TransformCutlineToSource()                      */
/*                                                                      */
/*      Transform cutline from its SRS to source pixel/line coordinates.*/
/************************************************************************/
static CPLErr
TransformCutlineToSource( GDALDatasetH hSrcDS, OGRGeometryH hCutline,
                          char ***ppapszWarpOptions, char **papszTO_In )

{
    OGRGeometryH hMultiPolygon = OGR_G_Clone( hCutline );

/* -------------------------------------------------------------------- */
/*      Checkout that if there's a cutline SRS, there's also a raster   */
/*      one.                                                            */
/* -------------------------------------------------------------------- */
    OGRSpatialReferenceH  hRasterSRS = nullptr;
    const char *pszProjection = GetSrcDSProjection( hSrcDS, papszTO_In);
    if( pszProjection != nullptr )
    {
        hRasterSRS = OSRNewSpatialReference(nullptr);
        if( OSRImportFromWkt( hRasterSRS, const_cast<char **>(&pszProjection) ) != OGRERR_NONE )
        {
            OSRDestroySpatialReference(hRasterSRS);
            hRasterSRS = nullptr;
        }
    }

/* -------------------------------------------------------------------- */
/*      Extract the cutline SRS.                                        */
/* -------------------------------------------------------------------- */
    OGRSpatialReferenceH hCutlineSRS = OGR_G_GetSpatialReference( hMultiPolygon );

/* -------------------------------------------------------------------- */
/*      Detect if there's no transform at all involved, in which case   */
/*      we can avoid densification.                                     */
/* -------------------------------------------------------------------- */
    bool bMayNeedDensify = true;
    if( hRasterSRS != nullptr && hCutlineSRS != nullptr &&
        OSRIsSame(hRasterSRS, hCutlineSRS) &&
        GDALGetGCPCount( hSrcDS ) == 0 &&
        GDALGetMetadata( hSrcDS, "RPC" ) == nullptr &&
        GDALGetMetadata( hSrcDS, "GEOLOCATION" ) == nullptr )
    {
        char **papszTOTmp = CSLDuplicate( papszTO_In );
        papszTOTmp = CSLSetNameValue(papszTOTmp, "SRC_SRS", nullptr);
        papszTOTmp = CSLSetNameValue(papszTOTmp, "DST_SRS", nullptr);
        if( CSLCount(papszTOTmp) == 0 )
        {
            bMayNeedDensify = false;
        }
        CSLDestroy(papszTOTmp);
    }

/* -------------------------------------------------------------------- */
/*      Compare source raster SRS and cutline SRS                       */
/* -------------------------------------------------------------------- */
    if( hRasterSRS != nullptr && hCutlineSRS != nullptr )
    {
        /* OK, we will reproject */
    }
    else if( hRasterSRS != nullptr && hCutlineSRS == nullptr )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                "the source raster dataset has a SRS, but the cutline features\n"
                "not.  We assume that the cutline coordinates are expressed in the destination SRS.\n"
                "If not, cutline results may be incorrect.");
    }
    else if( hRasterSRS == nullptr && hCutlineSRS != nullptr )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                "the input vector layer has a SRS, but the source raster dataset does not.\n"
                "Cutline results may be incorrect.");
    }

    if( hRasterSRS != nullptr )
        OSRDestroySpatialReference(hRasterSRS);

    char **papszTO = CSLDuplicate( papszTO_In );
    if( hCutlineSRS != nullptr )
    {
        char *pszCutlineSRS_WKT = nullptr;

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
        GDALCreateGenImgProjTransformer2( hSrcDS, nullptr, papszTO );

    CSLDestroy( papszTO );

    if( oTransformer.hSrcImageTransformer == nullptr )
    {
        OGR_G_DestroyGeometry( hMultiPolygon );
        return CE_Failure;
    }

    // Some transforms like RPC can transform a valid geometry into an invalid
    // one if the node density of the input geometry isn't sufficient before
    // reprojection. So after an initial reprojection, we check that the
    // maximum length of a segment is no longer than 1 pixel, and if not,
    // we densify the input geometry before doing a new reprojection
    const double dfMaxLengthInSpatUnits = GetMaximumSegmentLength(
                reinterpret_cast<OGRGeometry*>(hMultiPolygon) );
    OGRErr eErr = OGR_G_Transform( hMultiPolygon,
                     reinterpret_cast<OGRCoordinateTransformationH>(&oTransformer) );
    const double dfInitialMaxLengthInPixels = GetMaximumSegmentLength(
                            reinterpret_cast<OGRGeometry*>(hMultiPolygon) );

    CPLPushErrorHandler(CPLQuietErrorHandler);
    const bool bWasValidInitialy = LooseValidateCutline(hMultiPolygon);
    CPLPopErrorHandler();
    if( !bWasValidInitialy )
    {
        CPLDebug("WARP", "Cutline is not valid after initial reprojection");
        char *pszWKT = nullptr;
        OGR_G_ExportToWkt( hMultiPolygon, &pszWKT );
        CPLDebug("GDALWARP", "WKT = \"%s\"", pszWKT ? pszWKT : "(null)");
        CPLFree(pszWKT);
    }

    bool bDensify = false;
    if( bMayNeedDensify && eErr == OGRERR_NONE && dfInitialMaxLengthInPixels > 1.0 )
    {
        const char* pszDensifyCutline = CPLGetConfigOption("GDALWARP_DENSIFY_CUTLINE", "YES");
        if( EQUAL(pszDensifyCutline, "ONLY_IF_INVALID") )
        {
            bDensify = ( OGRGeometryFactory::haveGEOS() && !bWasValidInitialy );
        }
        else if( CSLFetchNameValue( *ppapszWarpOptions, "CUTLINE_BLEND_DIST" ) != nullptr &&
                 CPLGetConfigOption("GDALWARP_DENSIFY_CUTLINE", nullptr) == nullptr )
        {
            // TODO: we should only emit this message if a transform/reprojection will be actually done
            CPLDebug("WARP", "Densification of cutline could perhaps be useful but as "
                     "CUTLINE_BLEND_DIST is used, this could be very slow. So disabled "
                     "unless GDALWARP_DENSIFY_CUTLINE=YES is explicitly specified as configuration option");
        }
        else
        {
            bDensify = CPLTestBool(pszDensifyCutline);
        }
    }
    if( bDensify )
    {
        CPLDebug("WARP", "Cutline maximum segment size was %.0f pixel after reprojection to source coordinates.",
                 dfInitialMaxLengthInPixels);

        // Densify and reproject with the aim of having a 1 pixel density
        double dfSegmentSize = dfMaxLengthInSpatUnits / dfInitialMaxLengthInPixels;
        const int MAX_ITERATIONS = 10;
        for(int i=0;i<MAX_ITERATIONS;i++)
        {
            OGR_G_DestroyGeometry( hMultiPolygon );
            hMultiPolygon = OGR_G_Clone( hCutline );
            OGR_G_Segmentize(hMultiPolygon, dfSegmentSize);
            if( i == MAX_ITERATIONS - 1 )
            {
                char* pszWKT = nullptr;
                OGR_G_ExportToWkt(hMultiPolygon, &pszWKT);
                CPLDebug("WARP", "WKT of polygon after densification with segment size = %f: %s",
                         dfSegmentSize, pszWKT);
                CPLFree(pszWKT);
            }
            eErr = OGR_G_Transform( hMultiPolygon,
                        reinterpret_cast<OGRCoordinateTransformationH>(&oTransformer) );
            if( eErr == OGRERR_NONE )
            {
                const double dfMaxLengthInPixels = GetMaximumSegmentLength(
                                    reinterpret_cast<OGRGeometry*>(hMultiPolygon) );
                if( bWasValidInitialy )
                {
                    // In some cases, the densification itself results in a reprojected
                    // invalid polygon due to the non-linearity of RPC DEM transformation,
                    // so in those cases, try a less dense cutline
                    CPLPushErrorHandler(CPLQuietErrorHandler);
                    const bool bIsValid = LooseValidateCutline(hMultiPolygon);
                    CPLPopErrorHandler();
                    if( !bIsValid )
                    {
                        if( i == MAX_ITERATIONS - 1 )
                        {
                            char* pszWKT = nullptr;
                            OGR_G_ExportToWkt(hMultiPolygon, &pszWKT);
                            CPLDebug("WARP",
                                     "After densification, cutline maximum "
                                     "segment size is now %.0f pixel, "
                                     "but cutline is invalid. %s",
                                     dfMaxLengthInPixels,
                                     pszWKT);
                            CPLFree(pszWKT);
                            break;
                        }
                        CPLDebug("WARP", "After densification, cutline maximum segment size is now %.0f pixel, "
                                 "but cutline is invalid. So trying a less dense cutline.",
                                 dfMaxLengthInPixels);
                        dfSegmentSize *= 2;
                        continue;
                    }
                }
                CPLDebug("WARP", "After densification, cutline maximum segment size is now %.0f pixel.",
                        dfMaxLengthInPixels);
            }
            break;
        }
    }

    GDALDestroyGenImgProjTransformer( oTransformer.hSrcImageTransformer );

    if( eErr == OGRERR_FAILURE )
    {
        if( CPLTestBool(CPLGetConfigOption("GDALWARP_IGNORE_BAD_CUTLINE", "NO")) )
            CPLError(CE_Warning, CPLE_AppDefined, "Cutline transformation failed");
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cutline transformation failed");
            OGR_G_DestroyGeometry( hMultiPolygon );
            return CE_Failure;
        }
    }
    else if( !ValidateCutline(hMultiPolygon) )
    {
        OGR_G_DestroyGeometry( hMultiPolygon );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Convert aggregate geometry into WKT.                            */
/* -------------------------------------------------------------------- */
    char *pszWKT = nullptr;

    OGR_G_ExportToWkt( hMultiPolygon, &pszWKT );
    //fprintf(stderr, "WKT = \"%s\"\n", pszWKT ? pszWKT : "(null)");
    OGR_G_DestroyGeometry( hMultiPolygon );

    *ppapszWarpOptions = CSLSetNameValue( *ppapszWarpOptions,
                                          "CUTLINE", pszWKT );
    CPLFree( pszWKT );
    return CE_None;
}

static void
RemoveConflictingMetadata( GDALMajorObjectH hObj, char **papszMetadata,
                           const char *pszValueConflict )
{
    if ( hObj == nullptr ) return;

    char ** papszMetadataRef = CSLDuplicate( papszMetadata );
    int nCount = CSLCount( papszMetadataRef );

    for( int i = 0; i < nCount; i++ )
    {
        char *pszKey = nullptr;
        const char *pszValueRef =
            CPLParseNameValue(papszMetadataRef[i], &pszKey);
        if( pszKey != nullptr )
        {
            const char *pszValueComp = GDALGetMetadataItem(hObj, pszKey, nullptr);
            if ( ( pszValueRef == nullptr || pszValueComp == nullptr ||
                ! EQUAL( pszValueRef, pszValueComp ) ) &&
                ( pszValueComp == nullptr ||
                ! EQUAL( pszValueComp, pszValueConflict ) ) )
            {
                if( STARTS_WITH(pszKey, "STATISTICS_") )
                    GDALSetMetadataItem( hObj, pszKey, nullptr, nullptr );
                else
                    GDALSetMetadataItem( hObj, pszKey, pszValueConflict, nullptr );
            }
            CPLFree( pszKey );
        }
    }

    CSLDestroy( papszMetadataRef );
}

/************************************************************************/
/*                             SanitizeSRS                              */
/************************************************************************/

static char *SanitizeSRS( const char *pszUserInput )

{
    OGRSpatialReferenceH hSRS;
    char *pszResult = nullptr;

    CPLErrorReset();

    hSRS = OSRNewSpatialReference( nullptr );
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
    GDALWarpAppOptions *psOptions = static_cast<GDALWarpAppOptions *>(
        CPLCalloc(1, sizeof(GDALWarpAppOptions)));

    psOptions->dfMinX = 0.0;
    psOptions->dfMinY = 0.0;
    psOptions->dfMaxX = 0.0;
    psOptions->dfMaxY = 0.0;
    psOptions->dfXRes = 0.0;
    psOptions->dfYRes = 0.0;
    psOptions->bTargetAlignedPixels = false;
    psOptions->nForcePixels = 0;
    psOptions->nForceLines = 0;
    psOptions->bQuiet = true;
    psOptions->pfnProgress = GDALDummyProgress;
    psOptions->pProgressData = nullptr;
    psOptions->bEnableDstAlpha = false;
    psOptions->bEnableSrcAlpha = false;
    psOptions->bDisableSrcAlpha = false;
    psOptions->pszFormat = nullptr;
    psOptions->bCreateOutput = false;
    psOptions->papszWarpOptions = nullptr;
    psOptions->dfErrorThreshold = -1;
    psOptions->dfWarpMemoryLimit = 0.0;
    psOptions->papszCreateOptions = nullptr;
    psOptions->eOutputType = GDT_Unknown;
    psOptions->eWorkingType = GDT_Unknown;
    psOptions->eResampleAlg = GRA_NearestNeighbour;
    psOptions->pszSrcNodata = nullptr;
    psOptions->pszDstNodata = nullptr;
    psOptions->bMulti = false;
    psOptions->papszTO = nullptr;
    psOptions->pszCutlineDSName = nullptr;
    psOptions->pszCLayer = nullptr;
    psOptions->pszCWHERE = nullptr;
    psOptions->pszCSQL = nullptr;
    psOptions->bCropToCutline = false;
    psOptions->bCopyMetadata = true;
    psOptions->bCopyBandInfo = true;
    psOptions->pszMDConflictValue = CPLStrdup("*");
    psOptions->bSetColorInterpretation = false;
    psOptions->nOvLevel = -2;
    psOptions->bNoVShiftGrid = false;

/* -------------------------------------------------------------------- */
/*      Parse arguments.                                                */
/* -------------------------------------------------------------------- */
    int argc = CSLCount(papszArgv);
    for( int i = 0; papszArgv != nullptr && i < argc; i++ )
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
            psOptions->bCreateOutput = true;
        }
        else if( EQUAL(papszArgv[i],"-wo") && i+1 < argc )
        {
            psOptions->papszWarpOptions = CSLAddString( psOptions->papszWarpOptions, papszArgv[++i] );
        }
        else if( EQUAL(papszArgv[i],"-multi") )
        {
            psOptions->bMulti = true;
        }
        else if( EQUAL(papszArgv[i],"-q") || EQUAL(papszArgv[i],"-quiet"))
        {
            if( psOptionsForBinary )
                psOptionsForBinary->bQuiet = true;
        }
        else if( EQUAL(papszArgv[i],"-dstalpha") )
        {
            psOptions->bEnableDstAlpha = true;
        }
        else if( EQUAL(papszArgv[i],"-srcalpha") )
        {
            psOptions->bEnableSrcAlpha = true;
        }
        else if( EQUAL(papszArgv[i],"-nosrcalpha") )
        {
            psOptions->bDisableSrcAlpha = true;
        }
        else if( (EQUAL(papszArgv[i],"-of") || EQUAL(papszArgv[i],"-f")) && i+1 < argc )
        {
            CPLFree(psOptions->pszFormat);
            psOptions->pszFormat = CPLStrdup(papszArgv[++i]);
            psOptions->bCreateOutput = true;
        }
        else if( EQUAL(papszArgv[i],"-t_srs") && i+1 < argc )
        {
            char *pszSRS = SanitizeSRS(papszArgv[++i]);
            if(pszSRS == nullptr)
            {
                GDALWarpAppOptionsFree(psOptions);
                return nullptr;
            }
            psOptions->papszTO = CSLSetNameValue( psOptions->papszTO, "DST_SRS", pszSRS );
            CPLFree( pszSRS );
        }
        else if( EQUAL(papszArgv[i],"-s_srs") && i+1 < argc )
        {
            char *pszSRS = SanitizeSRS(papszArgv[++i]);
            if(pszSRS == nullptr)
            {
                GDALWarpAppOptionsFree(psOptions);
                return nullptr;
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
                return nullptr;
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
                return nullptr;
            }
            psOptions->bCreateOutput = true;
        }
        else if( EQUAL(papszArgv[i],"-tap") )
        {
            psOptions->bTargetAlignedPixels = true;
        }
        else if( EQUAL(papszArgv[i],"-ot") && i+1 < argc )
        {
            int iType;

            for( iType = 1; iType < GDT_TypeCount; iType++ )
            {
                if( GDALGetDataTypeName(static_cast<GDALDataType>(iType)) != nullptr
                    && EQUAL(GDALGetDataTypeName(static_cast<GDALDataType>(iType)),
                             papszArgv[i+1]) )
                {
                    psOptions->eOutputType = static_cast<GDALDataType>(iType);
                }
            }

            if( psOptions->eOutputType == GDT_Unknown )
            {
                CPLError(CE_Failure, CPLE_IllegalArg, "Unknown output pixel type: %s.", papszArgv[i+1]);
                GDALWarpAppOptionsFree(psOptions);
                return nullptr;
            }
            i++;
            psOptions->bCreateOutput = true;
        }
        else if( EQUAL(papszArgv[i],"-wt") && i+1 < argc )
        {
            int iType;

            for( iType = 1; iType < GDT_TypeCount; iType++ )
            {
                if( GDALGetDataTypeName(static_cast<GDALDataType>(iType)) != nullptr
                    && EQUAL(GDALGetDataTypeName(static_cast<GDALDataType>(iType)),
                             papszArgv[i+1]) )
                {
                    psOptions->eWorkingType = static_cast<GDALDataType>(iType);
                }
            }

            if( psOptions->eWorkingType == GDT_Unknown )
            {
                CPLError(CE_Failure, CPLE_IllegalArg, "Unknown working pixel type: %s.", papszArgv[i+1]);
                GDALWarpAppOptionsFree(psOptions);
                return nullptr;
            }
            i++;
        }
        else if( EQUAL(papszArgv[i],"-ts") && i+2 < argc )
        {
            psOptions->nForcePixels = atoi(papszArgv[++i]);
            psOptions->nForceLines = atoi(papszArgv[++i]);
            psOptions->bCreateOutput = true;
        }
        else if( EQUAL(papszArgv[i],"-te") && i+4 < argc )
        {
            psOptions->dfMinX = CPLAtofM(papszArgv[++i]);
            psOptions->dfMinY = CPLAtofM(papszArgv[++i]);
            psOptions->dfMaxX = CPLAtofM(papszArgv[++i]);
            psOptions->dfMaxY = CPLAtofM(papszArgv[++i]);
            psOptions->bCreateOutput = true;
        }
        else if( EQUAL(papszArgv[i],"-te_srs") && i+1 < argc )
        {
            char *pszSRS = SanitizeSRS(papszArgv[++i]);
            if(pszSRS == nullptr)
            {
                GDALWarpAppOptionsFree(psOptions);
                return nullptr;
            }
            CPLFree(psOptions->pszTE_SRS);
            psOptions->pszTE_SRS = CPLStrdup(pszSRS);
            CPLFree(pszSRS);
            psOptions->bCreateOutput = true;
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
                return nullptr;
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
            psOptions->bCropToCutline = true;
            psOptions->bCreateOutput = true;
        }
        else if( EQUAL(papszArgv[i],"-overwrite") )
        {
            if( psOptionsForBinary )
                psOptionsForBinary->bOverwrite = TRUE;
        }
        else if( EQUAL(papszArgv[i],"-nomd") )
        {
            psOptions->bCopyMetadata = false;
            psOptions->bCopyBandInfo = false;
        }
        else if( EQUAL(papszArgv[i],"-cvmd") && i+1 < argc )
        {
            CPLFree(psOptions->pszMDConflictValue);
            psOptions->pszMDConflictValue = CPLStrdup(papszArgv[++i]);
        }
        else if( EQUAL(papszArgv[i],"-setci") )
            psOptions->bSetColorInterpretation = true;
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
            else if( STARTS_WITH_CI(pszOvLevel, "AUTO-") )
                psOptions->nOvLevel = -2-atoi(pszOvLevel + 5);
            else if( EQUAL(pszOvLevel, "NONE") )
                psOptions->nOvLevel = -1;
            else if( CPLGetValueType(pszOvLevel) == CPL_VALUE_INTEGER )
                psOptions->nOvLevel = atoi(pszOvLevel);
            else
            {
                CPLError(CE_Failure, CPLE_IllegalArg, "Invalid value '%s' for -ov option", pszOvLevel);
                GDALWarpAppOptionsFree(psOptions);
                return nullptr;
            }
        }
        else if( EQUAL(papszArgv[i],"-novshiftgrid") )
        {
            psOptions->bNoVShiftGrid = true;
        }
        else if( papszArgv[i][0] == '-' )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Unknown option name '%s'", papszArgv[i]);
            GDALWarpAppOptionsFree(psOptions);
            return nullptr;
        }

        else
        {
            if( psOptionsForBinary )
            {
                psOptionsForBinary->papszSrcFiles = CSLAddString( psOptionsForBinary->papszSrcFiles, papszArgv[i] );
            }
        }
    }

    if( psOptions->bEnableSrcAlpha && psOptions->bDisableSrcAlpha )
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "-srcalpha and -nosrcalpha cannot be used together");
        GDALWarpAppOptionsFree(psOptions);
        return nullptr;
    }

    if( psOptionsForBinary )
        psOptionsForBinary->bCreateOutput = psOptions->bCreateOutput;

/* -------------------------------------------------------------------- */
/*      The last filename in the file list is really our destination    */
/*      file.                                                           */
/* -------------------------------------------------------------------- */
    if( psOptionsForBinary && CSLCount(psOptionsForBinary->papszSrcFiles) > 1 )
    {
        psOptionsForBinary->pszDstFilename = psOptionsForBinary->papszSrcFiles[CSLCount(psOptionsForBinary->papszSrcFiles)-1];
        psOptionsForBinary->papszSrcFiles[CSLCount(psOptionsForBinary->papszSrcFiles)-1] = nullptr;
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
 * @param psOptions the options struct for GDALWarp().
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
        psOptions->bQuiet = false;
}

/************************************************************************/
/*                    GDALWarpAppOptionsSetQuiet()                      */
/************************************************************************/

/**
 * Set a progress function.
 *
 * @param psOptions the options struct for GDALWarp().
 * @param bQuiet whether GDALWarp() should emit messages on stdout.
 *
 * @since GDAL 2.3
 */

void GDALWarpAppOptionsSetQuiet( GDALWarpAppOptions *psOptions,
                                 int bQuiet )
{
    psOptions->bQuiet = CPL_TO_BOOL(bQuiet);
}

/************************************************************************/
/*                 GDALWarpAppOptionsSetWarpOption()                    */
/************************************************************************/

/**
 * Set a warp option
 *
 * @param psOptions the options struct for GDALWarp().
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
