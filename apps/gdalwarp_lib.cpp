/******************************************************************************
 *
 * Project:  High Performance Image Reprojector
 * Purpose:  Test program for high performance warper API.
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
 *
 ******************************************************************************
 * Copyright (c) 2002, i3 - information integration and imaging
 *                          Fort Collin, CO
 * Copyright (c) 2007-2015, Even Rouault <even dot rouault at spatialys.com>
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
#include "gdalargumentparser.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <array>
#include <limits>
#include <set>
#include <utility>
#include <vector>

// Suppress deprecation warning for GDALOpenVerticalShiftGrid and
// GDALApplyVerticalShiftGrid
#ifndef CPL_WARN_DEPRECATED_GDALOpenVerticalShiftGrid
#define CPL_WARN_DEPRECATED_GDALOpenVerticalShiftGrid(x)
#define CPL_WARN_DEPRECATED_GDALApplyVerticalShiftGrid(x)
#endif

#include "commonutils.h"
#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_progress.h"
#include "cpl_string.h"
#include "gdal.h"
#include "gdal_alg.h"
#include "gdal_alg_priv.h"
#include "gdal_priv.h"
#include "gdalwarper.h"
#include "ogr_api.h"
#include "ogr_core.h"
#include "ogr_geometry.h"
#include "ogr_spatialref.h"
#include "ogr_srs_api.h"
#include "ogr_proj_p.h"
#include "ogrsf_frmts.h"
#include "vrtdataset.h"
#include "../frmts/gtiff/cogdriver.h"

#if PROJ_VERSION_MAJOR > 6 || PROJ_VERSION_MINOR >= 3
#define USE_PROJ_BASED_VERTICAL_SHIFT_METHOD
#endif

/************************************************************************/
/*                        GDALWarpAppOptions                            */
/************************************************************************/

/** Options for use with GDALWarp(). GDALWarpAppOptions* must be allocated and
 * freed with GDALWarpAppOptionsNew() and GDALWarpAppOptionsFree() respectively.
 */
struct GDALWarpAppOptions
{
    /*! set georeferenced extents of output file to be created (in target SRS by
       default, or in the SRS specified with pszTE_SRS) */
    double dfMinX = 0;
    double dfMinY = 0;
    double dfMaxX = 0;
    double dfMaxY = 0;

    /*! the SRS in which to interpret the coordinates given in
       GDALWarpAppOptions::dfMinX, GDALWarpAppOptions::dfMinY,
       GDALWarpAppOptions::dfMaxX and GDALWarpAppOptions::dfMaxY. The SRS may be
       any of the usual GDAL/OGR forms, complete WKT, PROJ.4, EPSG:n or a file
       containing the WKT. It is a convenience e.g. when knowing the output
       coordinates in a geodetic long/lat SRS, but still wanting a result in a
       projected coordinate system. */
    std::string osTE_SRS{};

    /*! set output file resolution (in target georeferenced units) */
    double dfXRes = 0;
    double dfYRes = 0;

    /*! whether target pixels should have dfXRes == dfYRes */
    bool bSquarePixels = false;

    /*! align the coordinates of the extent of the output file to the values of
       the GDALWarpAppOptions::dfXRes and GDALWarpAppOptions::dfYRes, such that
       the aligned extent includes the minimum extent. */
    bool bTargetAlignedPixels = false;

    /*! set output file size in pixels and lines. If
       GDALWarpAppOptions::nForcePixels or GDALWarpAppOptions::nForceLines is
       set to 0, the other dimension will be guessed from the computed
       resolution. Note that GDALWarpAppOptions::nForcePixels and
        GDALWarpAppOptions::nForceLines cannot be used with
       GDALWarpAppOptions::dfXRes and GDALWarpAppOptions::dfYRes. */
    int nForcePixels = 0;
    int nForceLines = 0;

    /*! allow or suppress progress monitor and other non-error output */
    bool bQuiet = true;

    /*! the progress function to use */
    GDALProgressFunc pfnProgress = GDALDummyProgress;

    /*! pointer to the progress data variable */
    void *pProgressData = nullptr;

    /*! creates an output alpha band to identify nodata (unset/transparent)
       pixels when set to true */
    bool bEnableDstAlpha = false;

    /*! forces the last band of an input file to be considered as alpha band. */
    bool bEnableSrcAlpha = false;

    /*! Prevent a source alpha band from being considered as such */
    bool bDisableSrcAlpha = false;

    /*! output format. Use the short format name. */
    std::string osFormat{};

    bool bCreateOutput = false;

    /*! list of warp options. ("NAME1=VALUE1","NAME2=VALUE2",...). The
        GDALWarpOptions::aosWarpOptions docs show all options. */
    CPLStringList aosWarpOptions{};

    double dfErrorThreshold = -1;

    /*! the amount of memory (in megabytes) that the warp API is allowed
        to use for caching. */
    double dfWarpMemoryLimit = 0;

    /*! list of create options for the output format driver. See format
        specific documentation for legal creation options for each format. */
    CPLStringList aosCreateOptions{};

    /*! the data type of the output bands */
    GDALDataType eOutputType = GDT_Unknown;

    /*! working pixel data type. The data type of pixels in the source
        image and destination image buffers. */
    GDALDataType eWorkingType = GDT_Unknown;

    /*! the resampling method. Available methods are: near, bilinear,
        cubic, cubicspline, lanczos, average, mode, max, min, med,
        q1, q3, sum */
    GDALResampleAlg eResampleAlg = GRA_NearestNeighbour;

    /*! whether -r was specified */
    bool bResampleAlgSpecifiedByUser = false;

    /*! nodata masking values for input bands (different values can be supplied
        for each band). ("value1 value2 ..."). Masked values will not be used
        in interpolation. Use a value of "None" to ignore intrinsic nodata
        settings on the source dataset. */
    std::string osSrcNodata{};

    /*! nodata values for output bands (different values can be supplied for
        each band). ("value1 value2 ..."). New files will be initialized to
        this value and if possible the nodata value will be recorded in the
        output file. Use a value of "None" to ensure that nodata is not defined.
        If this argument is not used then nodata values will be copied from
        the source dataset. */
    std::string osDstNodata{};

    /*! use multithreaded warping implementation. Multiple threads will be used
        to process chunks of image and perform input/output operation
       simultaneously. */
    bool bMulti = false;

    /*! list of transformer options suitable to pass to
       GDALCreateGenImgProjTransformer2().
        ("NAME1=VALUE1","NAME2=VALUE2",...) */
    CPLStringList aosTransformerOptions{};

    /*! enable use of a blend cutline from a vector dataset name or a WKT
     * geometry
     */
    std::string osCutlineDSNameOrWKT{};

    /*! cutline SRS */
    std::string osCutlineSRS{};

    /*! the named layer to be selected from the cutline datasource */
    std::string osCLayer{};

    /*! restrict desired cutline features based on attribute query */
    std::string osCWHERE{};

    /*! SQL query to select the cutline features instead of from a layer
        with osCLayer */
    std::string osCSQL{};

    /*! crop the extent of the target dataset to the extent of the cutline */
    bool bCropToCutline = false;

    /*! copy dataset and band metadata will be copied from the first source
       dataset. Items that differ between source datasets will be set "*" (see
       GDALWarpAppOptions::pszMDConflictValue) */
    bool bCopyMetadata = true;

    /*! copy band information from the first source dataset */
    bool bCopyBandInfo = true;

    /*! value to set metadata items that conflict between source datasets
       (default is "*"). Use "" to remove conflicting items. */
    std::string osMDConflictValue = "*";

    /*! set the color interpretation of the bands of the target dataset from the
     * source dataset */
    bool bSetColorInterpretation = false;

    /*! overview level of source files to be used */
    int nOvLevel = OVR_LEVEL_AUTO;

    /*! Whether to enable vertical shift adjustment */
    bool bVShift = false;

    /*! Whether to disable vertical shift adjustment */
    bool bNoVShift = false;

    /*! Source bands */
    std::vector<int> anSrcBands{};

    /*! Destination bands */
    std::vector<int> anDstBands{};
};

static CPLErr
LoadCutline(const std::string &osCutlineDSNameOrWKT, const std::string &osSRS,
            const std::string &oszCLayer, const std::string &osCWHERE,
            const std::string &osCSQL, OGRGeometryH *phCutlineRet);
static CPLErr TransformCutlineToSource(GDALDataset *poSrcDS,
                                       OGRGeometry *poCutline,
                                       char ***ppapszWarpOptions,
                                       CSLConstList papszTO);

static GDALDatasetH GDALWarpCreateOutput(
    int nSrcCount, GDALDatasetH *pahSrcDS, const char *pszFilename,
    const char *pszFormat, char **papszTO, CSLConstList papszCreateOptions,
    GDALDataType eDT, void **phTransformArg, bool bSetColorInterpretation,
    GDALWarpAppOptions *psOptions);

static void RemoveConflictingMetadata(GDALMajorObjectH hObj,
                                      CSLConstList papszMetadata,
                                      const char *pszValueConflict);

static bool GetResampleAlg(const char *pszResampling,
                           GDALResampleAlg &eResampleAlg, bool bThrow = false);

static double GetAverageSegmentLength(const OGRGeometry *poGeom)
{
    if (!poGeom)
        return 0;
    switch (wkbFlatten(poGeom->getGeometryType()))
    {
        case wkbLineString:
        {
            const auto *poLS = poGeom->toLineString();
            double dfSum = 0;
            const int nPoints = poLS->getNumPoints();
            if (nPoints == 0)
                return 0;
            for (int i = 0; i < nPoints - 1; i++)
            {
                double dfX1 = poLS->getX(i);
                double dfY1 = poLS->getY(i);
                double dfX2 = poLS->getX(i + 1);
                double dfY2 = poLS->getY(i + 1);
                double dfDX = dfX2 - dfX1;
                double dfDY = dfY2 - dfY1;
                dfSum += sqrt(dfDX * dfDX + dfDY * dfDY);
            }
            return dfSum / nPoints;
        }

        case wkbPolygon:
        {
            if (poGeom->IsEmpty())
                return 0;
            double dfSum = 0;
            for (const auto *poLS : poGeom->toPolygon())
            {
                dfSum += GetAverageSegmentLength(poLS);
            }
            return dfSum / (1 + poGeom->toPolygon()->getNumInteriorRings());
        }

        case wkbMultiPolygon:
        case wkbMultiLineString:
        case wkbGeometryCollection:
        {
            if (poGeom->IsEmpty())
                return 0;
            double dfSum = 0;
            for (const auto *poSubGeom : poGeom->toGeometryCollection())
            {
                dfSum += GetAverageSegmentLength(poSubGeom);
            }
            return dfSum / poGeom->toGeometryCollection()->getNumGeometries();
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

static CPLString GetSrcDSProjection(GDALDatasetH hDS, CSLConstList papszTO)
{
    const char *pszProjection = CSLFetchNameValue(papszTO, "SRC_SRS");
    if (pszProjection != nullptr || hDS == nullptr)
    {
        return pszProjection ? pszProjection : "";
    }

    const char *pszMethod = CSLFetchNameValue(papszTO, "METHOD");
    char **papszMD = nullptr;
    const OGRSpatialReferenceH hSRS = GDALGetSpatialRef(hDS);
    const char *pszGeolocationDataset =
        CSLFetchNameValueDef(papszTO, "SRC_GEOLOC_ARRAY",
                             CSLFetchNameValue(papszTO, "GEOLOC_ARRAY"));
    if (pszGeolocationDataset != nullptr &&
        (pszMethod == nullptr || EQUAL(pszMethod, "GEOLOC_ARRAY")))
    {
        auto aosMD =
            GDALCreateGeolocationMetadata(hDS, pszGeolocationDataset, true);
        pszProjection = aosMD.FetchNameValue("SRS");
        if (pszProjection)
            return pszProjection;  // return in this scope so that aosMD is
                                   // still valid
    }
    else if (hSRS && (pszMethod == nullptr || EQUAL(pszMethod, "GEOTRANSFORM")))
    {
        char *pszWKT = nullptr;
        {
            CPLErrorStateBackuper oErrorStateBackuper(CPLQuietErrorHandler);
            if (OSRExportToWkt(hSRS, &pszWKT) != OGRERR_NONE)
            {
                CPLFree(pszWKT);
                pszWKT = nullptr;
                const char *const apszOptions[] = {"FORMAT=WKT2", nullptr};
                OSRExportToWktEx(hSRS, &pszWKT, apszOptions);
            }
        }
        CPLString osWKT = pszWKT ? pszWKT : "";
        CPLFree(pszWKT);
        return osWKT;
    }
    else if (GDALGetGCPProjection(hDS) != nullptr &&
             strlen(GDALGetGCPProjection(hDS)) > 0 &&
             GDALGetGCPCount(hDS) > 1 &&
             (pszMethod == nullptr || STARTS_WITH_CI(pszMethod, "GCP_")))
    {
        pszProjection = GDALGetGCPProjection(hDS);
    }
    else if (GDALGetMetadata(hDS, "RPC") != nullptr &&
             (pszMethod == nullptr || EQUAL(pszMethod, "RPC")))
    {
        pszProjection = SRS_WKT_WGS84_LAT_LONG;
    }
    else if ((papszMD = GDALGetMetadata(hDS, "GEOLOCATION")) != nullptr &&
             (pszMethod == nullptr || EQUAL(pszMethod, "GEOLOC_ARRAY")))
    {
        pszProjection = CSLFetchNameValue(papszMD, "SRS");
    }
    return pszProjection ? pszProjection : "";
}

/************************************************************************/
/*                      CreateCTCutlineToSrc()                          */
/************************************************************************/

static std::unique_ptr<OGRCoordinateTransformation> CreateCTCutlineToSrc(
    const OGRSpatialReference *poRasterSRS, const OGRSpatialReference *poDstSRS,
    const OGRSpatialReference *poCutlineSRS, CSLConstList papszTO)
{
    const OGRSpatialReference *poCutlineOrTargetSRS =
        poCutlineSRS ? poCutlineSRS : poDstSRS;
    std::unique_ptr<OGRCoordinateTransformation> poCTCutlineToSrc;
    if (poCutlineOrTargetSRS && poRasterSRS &&
        !poCutlineOrTargetSRS->IsSame(poRasterSRS))
    {
        OGRCoordinateTransformationOptions oOptions;
        // If the cutline SRS is the same as the target SRS and there is
        // an explicit -ct between the source SRS and the target SRS, then
        // use it in the reverse way to transform from the cutline SRS to
        // the source SRS.
        if (poDstSRS && poCutlineOrTargetSRS->IsSame(poDstSRS))
        {
            const char *pszCT =
                CSLFetchNameValue(papszTO, "COORDINATE_OPERATION");
            if (pszCT)
            {
                oOptions.SetCoordinateOperation(pszCT, /* bInverse = */ true);
            }
        }
        poCTCutlineToSrc.reset(OGRCreateCoordinateTransformation(
            poCutlineOrTargetSRS, poRasterSRS, oOptions));
    }
    return poCTCutlineToSrc;
}

/************************************************************************/
/*                           CropToCutline()                            */
/************************************************************************/

static CPLErr CropToCutline(const OGRGeometry *poCutline, CSLConstList papszTO,
                            CSLConstList papszWarpOptions, int nSrcCount,
                            GDALDatasetH *pahSrcDS, double &dfMinX,
                            double &dfMinY, double &dfMaxX, double &dfMaxY,
                            const GDALWarpAppOptions *psOptions)
{
    // We could possibly directly reproject from cutline SRS to target SRS,
    // but when applying the cutline, it is reprojected to source raster image
    // space using the source SRS. To be consistent, we reproject
    // the cutline from cutline SRS to source SRS and then from source SRS to
    // target SRS.
    const OGRSpatialReference *poCutlineSRS = poCutline->getSpatialReference();
    const char *pszThisTargetSRS = CSLFetchNameValue(papszTO, "DST_SRS");
    std::unique_ptr<OGRSpatialReference> poSrcSRS;
    std::unique_ptr<OGRSpatialReference> poDstSRS;

    const CPLString osThisSourceSRS =
        GetSrcDSProjection(nSrcCount > 0 ? pahSrcDS[0] : nullptr, papszTO);
    if (!osThisSourceSRS.empty())
    {
        poSrcSRS = std::make_unique<OGRSpatialReference>();
        poSrcSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        if (poSrcSRS->SetFromUserInput(osThisSourceSRS) != OGRERR_NONE)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot compute bounding box of cutline.");
            return CE_Failure;
        }
    }
    else if (!pszThisTargetSRS && !poCutlineSRS)
    {
        OGREnvelope sEnvelope;
        poCutline->getEnvelope(&sEnvelope);

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

    if (pszThisTargetSRS)
    {
        poDstSRS = std::make_unique<OGRSpatialReference>();
        poDstSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        if (poDstSRS->SetFromUserInput(pszThisTargetSRS) != OGRERR_NONE)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot compute bounding box of cutline.");
            return CE_Failure;
        }
    }
    else
    {
        poDstSRS.reset(poSrcSRS->Clone());
    }

    auto poCutlineGeom = std::unique_ptr<OGRGeometry>(poCutline->clone());
    auto poCTCutlineToSrc = CreateCTCutlineToSrc(poSrcSRS.get(), poDstSRS.get(),
                                                 poCutlineSRS, papszTO);

    std::unique_ptr<OGRCoordinateTransformation> poCTSrcToDst;
    if (!poSrcSRS->IsSame(poDstSRS.get()))
    {
        poCTSrcToDst.reset(
            OGRCreateCoordinateTransformation(poSrcSRS.get(), poDstSRS.get()));
    }

    // Reproject cutline to target SRS, by doing intermediate vertex
    // densification in source SRS.
    if (poCTSrcToDst || poCTCutlineToSrc)
    {
        OGREnvelope sLastEnvelope, sCurEnvelope;
        std::unique_ptr<OGRGeometry> poTransformedGeom;
        auto poGeomInSrcSRS =
            std::unique_ptr<OGRGeometry>(poCutlineGeom->clone());
        if (poCTCutlineToSrc)
        {
            poGeomInSrcSRS.reset(OGRGeometryFactory::transformWithOptions(
                poGeomInSrcSRS.get(), poCTCutlineToSrc.get(), nullptr));
            if (!poGeomInSrcSRS)
                return CE_Failure;
        }

        // Do not use a smaller epsilon, otherwise it could cause useless
        // segmentization (https://github.com/OSGeo/gdal/issues/4826)
        constexpr double epsilon = 1e-10;
        for (int nIter = 0; nIter < 10; nIter++)
        {
            poTransformedGeom.reset(poGeomInSrcSRS->clone());
            if (poCTSrcToDst)
            {
                poTransformedGeom.reset(
                    OGRGeometryFactory::transformWithOptions(
                        poTransformedGeom.get(), poCTSrcToDst.get(), nullptr));
                if (!poTransformedGeom)
                    return CE_Failure;
            }
            poTransformedGeom->getEnvelope(&sCurEnvelope);
            if (nIter > 0 || !poCTSrcToDst)
            {
                if (std::abs(sCurEnvelope.MinX - sLastEnvelope.MinX) <=
                        epsilon *
                            std::abs(sCurEnvelope.MinX + sLastEnvelope.MinX) &&
                    std::abs(sCurEnvelope.MinY - sLastEnvelope.MinY) <=
                        epsilon *
                            std::abs(sCurEnvelope.MinY + sLastEnvelope.MinY) &&
                    std::abs(sCurEnvelope.MaxX - sLastEnvelope.MaxX) <=
                        epsilon *
                            std::abs(sCurEnvelope.MaxX + sLastEnvelope.MaxX) &&
                    std::abs(sCurEnvelope.MaxY - sLastEnvelope.MaxY) <=
                        epsilon *
                            std::abs(sCurEnvelope.MaxY + sLastEnvelope.MaxY))
                {
                    break;
                }
            }
            double dfAverageSegmentLength =
                GetAverageSegmentLength(poGeomInSrcSRS.get());
            poGeomInSrcSRS->segmentize(dfAverageSegmentLength / 4);

            sLastEnvelope = sCurEnvelope;
        }

        poCutlineGeom = std::move(poTransformedGeom);
    }

    OGREnvelope sEnvelope;
    poCutlineGeom->getEnvelope(&sEnvelope);

    dfMinX = sEnvelope.MinX;
    dfMinY = sEnvelope.MinY;
    dfMaxX = sEnvelope.MaxX;
    dfMaxY = sEnvelope.MaxY;
    if (!poCTSrcToDst && nSrcCount > 0 && psOptions->dfXRes == 0.0 &&
        psOptions->dfYRes == 0.0)
    {
        // No raster reprojection: stick on exact pixel boundaries of the source
        // to preserve resolution and avoid resampling
        double adfGT[6];
        if (GDALGetGeoTransform(pahSrcDS[0], adfGT) == CE_None)
        {
            // We allow for a relative error in coordinates up to 0.1% of the
            // pixel size for rounding purposes.
            constexpr double REL_EPS_PIXEL = 1e-3;
            if (CPLFetchBool(papszWarpOptions, "CUTLINE_ALL_TOUCHED", false))
            {
                // All touched ? Then make the extent a bit larger than the
                // cutline envelope
                dfMinX = adfGT[0] +
                         floor((dfMinX - adfGT[0]) / adfGT[1] + REL_EPS_PIXEL) *
                             adfGT[1];
                dfMinY = adfGT[3] +
                         ceil((dfMinY - adfGT[3]) / adfGT[5] - REL_EPS_PIXEL) *
                             adfGT[5];
                dfMaxX = adfGT[0] +
                         ceil((dfMaxX - adfGT[0]) / adfGT[1] - REL_EPS_PIXEL) *
                             adfGT[1];
                dfMaxY = adfGT[3] +
                         floor((dfMaxY - adfGT[3]) / adfGT[5] + REL_EPS_PIXEL) *
                             adfGT[5];
            }
            else
            {
                // Otherwise, make it a bit smaller
                dfMinX = adfGT[0] +
                         ceil((dfMinX - adfGT[0]) / adfGT[1] - REL_EPS_PIXEL) *
                             adfGT[1];
                dfMinY = adfGT[3] +
                         floor((dfMinY - adfGT[3]) / adfGT[5] + REL_EPS_PIXEL) *
                             adfGT[5];
                dfMaxX = adfGT[0] +
                         floor((dfMaxX - adfGT[0]) / adfGT[1] + REL_EPS_PIXEL) *
                             adfGT[1];
                dfMaxY = adfGT[3] +
                         ceil((dfMaxY - adfGT[3]) / adfGT[5] - REL_EPS_PIXEL) *
                             adfGT[5];
            }
        }
    }

    return CE_None;
}

#ifdef USE_PROJ_BASED_VERTICAL_SHIFT_METHOD

static bool MustApplyVerticalShift(GDALDatasetH hWrkSrcDS,
                                   const GDALWarpAppOptions *psOptions,
                                   OGRSpatialReference &oSRSSrc,
                                   OGRSpatialReference &oSRSDst,
                                   bool &bSrcHasVertAxis, bool &bDstHasVertAxis)
{
    bool bApplyVShift = psOptions->bVShift;

    // Check if we must do vertical shift grid transform
    const char *pszSrcWKT =
        psOptions->aosTransformerOptions.FetchNameValue("SRC_SRS");
    if (pszSrcWKT)
        oSRSSrc.SetFromUserInput(pszSrcWKT);
    else
    {
        auto hSRS = GDALGetSpatialRef(hWrkSrcDS);
        if (hSRS)
            oSRSSrc = *(OGRSpatialReference::FromHandle(hSRS));
        else
            return false;
    }

    const char *pszDstWKT =
        psOptions->aosTransformerOptions.FetchNameValue("DST_SRS");
    if (pszDstWKT)
        oSRSDst.SetFromUserInput(pszDstWKT);
    else
        return false;

    if (oSRSSrc.IsSame(&oSRSDst))
        return false;

    bSrcHasVertAxis = oSRSSrc.IsCompound() ||
                      ((oSRSSrc.IsProjected() || oSRSSrc.IsGeographic()) &&
                       oSRSSrc.GetAxesCount() == 3);

    bDstHasVertAxis = oSRSDst.IsCompound() ||
                      ((oSRSDst.IsProjected() || oSRSDst.IsGeographic()) &&
                       oSRSDst.GetAxesCount() == 3);

    if ((GDALGetRasterCount(hWrkSrcDS) == 1 || psOptions->bVShift) &&
        (bSrcHasVertAxis || bDstHasVertAxis))
    {
        bApplyVShift = true;
    }
    return bApplyVShift;
}

/************************************************************************/
/*                      ApplyVerticalShift()                            */
/************************************************************************/

static bool ApplyVerticalShift(GDALDatasetH hWrkSrcDS,
                               const GDALWarpAppOptions *psOptions,
                               GDALWarpOptions *psWO)
{
    if (psOptions->bVShift)
    {
        psWO->papszWarpOptions = CSLSetNameValue(psWO->papszWarpOptions,
                                                 "APPLY_VERTICAL_SHIFT", "YES");
    }

    OGRSpatialReference oSRSSrc;
    OGRSpatialReference oSRSDst;
    bool bSrcHasVertAxis = false;
    bool bDstHasVertAxis = false;
    bool bApplyVShift =
        MustApplyVerticalShift(hWrkSrcDS, psOptions, oSRSSrc, oSRSDst,
                               bSrcHasVertAxis, bDstHasVertAxis);

    if ((GDALGetRasterCount(hWrkSrcDS) == 1 || psOptions->bVShift) &&
        (bSrcHasVertAxis || bDstHasVertAxis))
    {
        bApplyVShift = true;
        psWO->papszWarpOptions = CSLSetNameValue(psWO->papszWarpOptions,
                                                 "APPLY_VERTICAL_SHIFT", "YES");

        if (CSLFetchNameValue(psWO->papszWarpOptions,
                              "MULT_FACTOR_VERTICAL_SHIFT") == nullptr)
        {
            // Select how to go from input dataset units to meters
            double dfToMeterSrc = 1.0;
            const char *pszUnit =
                GDALGetRasterUnitType(GDALGetRasterBand(hWrkSrcDS, 1));

            double dfToMeterSrcAxis = 1.0;
            if (bSrcHasVertAxis)
            {
                oSRSSrc.GetAxis(nullptr, 2, nullptr, &dfToMeterSrcAxis);
            }

            if (pszUnit && (EQUAL(pszUnit, "m") || EQUAL(pszUnit, "meter") ||
                            EQUAL(pszUnit, "metre")))
            {
            }
            else if (pszUnit &&
                     (EQUAL(pszUnit, "ft") || EQUAL(pszUnit, "foot")))
            {
                dfToMeterSrc = CPLAtof(SRS_UL_FOOT_CONV);
            }
            else if (pszUnit && (EQUAL(pszUnit, "US survey foot")))
            {
                dfToMeterSrc = CPLAtof(SRS_UL_US_FOOT_CONV);
            }
            else if (pszUnit && !EQUAL(pszUnit, ""))
            {
                if (bSrcHasVertAxis)
                {
                    dfToMeterSrc = dfToMeterSrcAxis;
                }
                else
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Unknown units=%s. Assuming metre.", pszUnit);
                }
            }
            else
            {
                if (bSrcHasVertAxis)
                    oSRSSrc.GetAxis(nullptr, 2, nullptr, &dfToMeterSrc);
            }

            double dfToMeterDst = 1.0;
            if (bDstHasVertAxis)
                oSRSDst.GetAxis(nullptr, 2, nullptr, &dfToMeterDst);

            if (dfToMeterSrc > 0 && dfToMeterDst > 0)
            {
                const double dfMultFactorVerticalShift =
                    dfToMeterSrc / dfToMeterDst;
                CPLDebug("WARP", "Applying MULT_FACTOR_VERTICAL_SHIFT=%.18g",
                         dfMultFactorVerticalShift);
                psWO->papszWarpOptions = CSLSetNameValue(
                    psWO->papszWarpOptions, "MULT_FACTOR_VERTICAL_SHIFT",
                    CPLSPrintf("%.18g", dfMultFactorVerticalShift));

                const double dfMultFactorVerticalShiftPipeline =
                    dfToMeterSrcAxis / dfToMeterDst;
                CPLDebug("WARP",
                         "Applying MULT_FACTOR_VERTICAL_SHIFT_PIPELINE=%.18g",
                         dfMultFactorVerticalShiftPipeline);
                psWO->papszWarpOptions = CSLSetNameValue(
                    psWO->papszWarpOptions,
                    "MULT_FACTOR_VERTICAL_SHIFT_PIPELINE",
                    CPLSPrintf("%.18g", dfMultFactorVerticalShiftPipeline));
            }
        }
    }

    return bApplyVShift;
}

#else

/************************************************************************/
/*                      ApplyVerticalShiftGrid()                        */
/************************************************************************/

static GDALDatasetH ApplyVerticalShiftGrid(GDALDatasetH hWrkSrcDS,
                                           const GDALWarpAppOptions *psOptions,
                                           GDALDatasetH hVRTDS,
                                           bool &bErrorOccurredOut)
{
    bErrorOccurredOut = false;
    // Check if we must do vertical shift grid transform
    OGRSpatialReference oSRSSrc;
    OGRSpatialReference oSRSDst;
    const char *pszSrcWKT =
        psOptions->aosTransformerOptions.FetchNameValue("SRC_SRS");
    if (pszSrcWKT)
        oSRSSrc.SetFromUserInput(pszSrcWKT);
    else
    {
        auto hSRS = GDALGetSpatialRef(hWrkSrcDS);
        if (hSRS)
            oSRSSrc = *(OGRSpatialReference::FromHandle(hSRS));
    }

    const char *pszDstWKT =
        psOptions->aosTransformerOptions.FetchNameValue("DST_SRS");
    if (pszDstWKT)
        oSRSDst.SetFromUserInput(pszDstWKT);

    double adfGT[6] = {};
    if (GDALGetRasterCount(hWrkSrcDS) == 1 &&
        GDALGetGeoTransform(hWrkSrcDS, adfGT) == CE_None &&
        !oSRSSrc.IsEmpty() && !oSRSDst.IsEmpty())
    {
        if ((oSRSSrc.IsCompound() ||
             (oSRSSrc.IsGeographic() && oSRSSrc.GetAxesCount() == 3)) ||
            (oSRSDst.IsCompound() ||
             (oSRSDst.IsGeographic() && oSRSDst.GetAxesCount() == 3)))
        {
            const char *pszSrcProj4Geoids =
                oSRSSrc.GetExtension("VERT_DATUM", "PROJ4_GRIDS");
            const char *pszDstProj4Geoids =
                oSRSDst.GetExtension("VERT_DATUM", "PROJ4_GRIDS");

            if (oSRSSrc.IsCompound() && pszSrcProj4Geoids == nullptr)
            {
                CPLDebug("GDALWARP", "Source SRS is a compound CRS but lacks "
                                     "+geoidgrids");
            }

            if (oSRSDst.IsCompound() && pszDstProj4Geoids == nullptr)
            {
                CPLDebug("GDALWARP", "Target SRS is a compound CRS but lacks "
                                     "+geoidgrids");
            }

            if (pszSrcProj4Geoids != nullptr && pszDstProj4Geoids != nullptr &&
                EQUAL(pszSrcProj4Geoids, pszDstProj4Geoids))
            {
                pszSrcProj4Geoids = nullptr;
                pszDstProj4Geoids = nullptr;
            }

            // Select how to go from input dataset units to meters
            const char *pszUnit =
                GDALGetRasterUnitType(GDALGetRasterBand(hWrkSrcDS, 1));
            double dfToMeterSrc = 1.0;
            if (pszUnit && (EQUAL(pszUnit, "m") || EQUAL(pszUnit, "meter") ||
                            EQUAL(pszUnit, "metre")))
            {
            }
            else if (pszUnit &&
                     (EQUAL(pszUnit, "ft") || EQUAL(pszUnit, "foot")))
            {
                dfToMeterSrc = CPLAtof(SRS_UL_FOOT_CONV);
            }
            else if (pszUnit && (EQUAL(pszUnit, "US survey foot")))
            {
                dfToMeterSrc = CPLAtof(SRS_UL_US_FOOT_CONV);
            }
            else
            {
                if (pszUnit && !EQUAL(pszUnit, ""))
                {
                    CPLError(CE_Warning, CPLE_AppDefined, "Unknown units=%s",
                             pszUnit);
                }
                if (oSRSSrc.IsCompound())
                {
                    dfToMeterSrc = oSRSSrc.GetTargetLinearUnits("VERT_CS");
                }
                else if (oSRSSrc.IsProjected())
                {
                    dfToMeterSrc = oSRSSrc.GetLinearUnits();
                }
            }

            double dfToMeterDst = 1.0;
            if (oSRSDst.IsCompound())
            {
                dfToMeterDst = oSRSDst.GetTargetLinearUnits("VERT_CS");
            }
            else if (oSRSDst.IsProjected())
            {
                dfToMeterDst = oSRSDst.GetLinearUnits();
            }

            char **papszOptions = nullptr;
            if (psOptions->eOutputType != GDT_Unknown)
            {
                papszOptions = CSLSetNameValue(
                    papszOptions, "DATATYPE",
                    GDALGetDataTypeName(psOptions->eOutputType));
            }
            papszOptions =
                CSLSetNameValue(papszOptions, "ERROR_ON_MISSING_VERT_SHIFT",
                                psOptions->aosTransformerOptions.FetchNameValue(
                                    "ERROR_ON_MISSING_VERT_SHIFT"));
            papszOptions = CSLSetNameValue(papszOptions, "SRC_SRS", pszSrcWKT);

            if (pszSrcProj4Geoids != nullptr)
            {
                int bError = FALSE;
                GDALDatasetH hGridDataset =
                    GDALOpenVerticalShiftGrid(pszSrcProj4Geoids, &bError);
                if (bError && hGridDataset == nullptr)
                {
                    CPLError(CE_Failure, CPLE_AppDefined, "Cannot open %s.",
                             pszSrcProj4Geoids);
                    bErrorOccurredOut = true;
                    CSLDestroy(papszOptions);
                    return hWrkSrcDS;
                }
                else if (hGridDataset != nullptr)
                {
                    // Transform from source vertical datum to WGS84
                    GDALDatasetH hTmpDS = GDALApplyVerticalShiftGrid(
                        hWrkSrcDS, hGridDataset, FALSE, dfToMeterSrc, 1.0,
                        papszOptions);
                    GDALReleaseDataset(hGridDataset);
                    if (hTmpDS == nullptr)
                    {
                        bErrorOccurredOut = true;
                        CSLDestroy(papszOptions);
                        return hWrkSrcDS;
                    }
                    else
                    {
                        if (hVRTDS)
                        {
                            CPLError(
                                CE_Failure, CPLE_NotSupported,
                                "Warping to VRT with vertical transformation "
                                "not supported with PROJ < 6.3");
                            bErrorOccurredOut = true;
                            CSLDestroy(papszOptions);
                            return hWrkSrcDS;
                        }

                        CPLDebug("GDALWARP",
                                 "Adjusting source dataset "
                                 "with source vertical datum using %s",
                                 pszSrcProj4Geoids);
                        GDALReleaseDataset(hWrkSrcDS);
                        hWrkSrcDS = hTmpDS;
                        dfToMeterSrc = 1.0;
                    }
                }
            }

            if (pszDstProj4Geoids != nullptr)
            {
                int bError = FALSE;
                GDALDatasetH hGridDataset =
                    GDALOpenVerticalShiftGrid(pszDstProj4Geoids, &bError);
                if (bError && hGridDataset == nullptr)
                {
                    CPLError(CE_Failure, CPLE_AppDefined, "Cannot open %s.",
                             pszDstProj4Geoids);
                    bErrorOccurredOut = true;
                    CSLDestroy(papszOptions);
                    return hWrkSrcDS;
                }
                else if (hGridDataset != nullptr)
                {
                    // Transform from WGS84 to target vertical datum
                    GDALDatasetH hTmpDS = GDALApplyVerticalShiftGrid(
                        hWrkSrcDS, hGridDataset, TRUE, dfToMeterSrc,
                        dfToMeterDst, papszOptions);
                    GDALReleaseDataset(hGridDataset);
                    if (hTmpDS == nullptr)
                    {
                        bErrorOccurredOut = true;
                        CSLDestroy(papszOptions);
                        return hWrkSrcDS;
                    }
                    else
                    {
                        if (hVRTDS)
                        {
                            CPLError(
                                CE_Failure, CPLE_NotSupported,
                                "Warping to VRT with vertical transformation "
                                "not supported with PROJ < 6.3");
                            bErrorOccurredOut = true;
                            CSLDestroy(papszOptions);
                            return hWrkSrcDS;
                        }

                        CPLDebug("GDALWARP",
                                 "Adjusting source dataset "
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

#endif

/************************************************************************/
/*                        CanUseBuildVRT()                              */
/************************************************************************/

static bool CanUseBuildVRT(int nSrcCount, GDALDatasetH *pahSrcDS)
{

    bool bCanUseBuildVRT = true;
    std::vector<std::array<double, 4>> aoExtents;
    bool bSrcHasAlpha = false;
    int nPrevBandCount = 0;
    OGRSpatialReference oSRSPrev;
    double dfLastResX = 0;
    double dfLastResY = 0;
    for (int i = 0; i < nSrcCount; i++)
    {
        double adfGT[6];
        auto hSrcDS = pahSrcDS[i];
        if (EQUAL(GDALGetDescription(hSrcDS), ""))
        {
            bCanUseBuildVRT = false;
            break;
        }
        if (GDALGetGeoTransform(hSrcDS, adfGT) != CE_None || adfGT[2] != 0 ||
            adfGT[4] != 0 || adfGT[5] > 0)
        {
            bCanUseBuildVRT = false;
            break;
        }
        const double dfMinX = adfGT[0];
        const double dfMinY = adfGT[3] + GDALGetRasterYSize(hSrcDS) * adfGT[5];
        const double dfMaxX = adfGT[0] + GDALGetRasterXSize(hSrcDS) * adfGT[1];
        const double dfMaxY = adfGT[3];
        const int nBands = GDALGetRasterCount(hSrcDS);
        if (nBands > 1 && GDALGetRasterColorInterpretation(GDALGetRasterBand(
                              hSrcDS, nBands)) == GCI_AlphaBand)
        {
            bSrcHasAlpha = true;
        }
        aoExtents.emplace_back(
            std::array<double, 4>{{dfMinX, dfMinY, dfMaxX, dfMaxY}});
        const auto poSRS = GDALDataset::FromHandle(hSrcDS)->GetSpatialRef();
        if (i == 0)
        {
            nPrevBandCount = nBands;
            if (poSRS)
                oSRSPrev = *poSRS;
            dfLastResX = adfGT[1];
            dfLastResY = adfGT[5];
        }
        else
        {
            if (nPrevBandCount != nBands)
            {
                bCanUseBuildVRT = false;
                break;
            }
            if (poSRS == nullptr && !oSRSPrev.IsEmpty())
            {
                bCanUseBuildVRT = false;
                break;
            }
            if (poSRS != nullptr &&
                (oSRSPrev.IsEmpty() || !poSRS->IsSame(&oSRSPrev)))
            {
                bCanUseBuildVRT = false;
                break;
            }
            if (dfLastResX != adfGT[1] || dfLastResY != adfGT[5])
            {
                bCanUseBuildVRT = false;
                break;
            }
        }
    }
    if (bSrcHasAlpha && bCanUseBuildVRT)
    {
        // Quadratic performance loop. If that happens to be an issue,
        // we might need to build a quad tree
        for (size_t i = 0; i < aoExtents.size(); i++)
        {
            const double dfMinX = aoExtents[i][0];
            const double dfMinY = aoExtents[i][1];
            const double dfMaxX = aoExtents[i][2];
            const double dfMaxY = aoExtents[i][3];
            for (size_t j = i + 1; j < aoExtents.size(); j++)
            {
                const double dfOtherMinX = aoExtents[j][0];
                const double dfOtherMinY = aoExtents[j][1];
                const double dfOtherMaxX = aoExtents[j][2];
                const double dfOtherMaxY = aoExtents[j][3];
                if (dfMinX < dfOtherMaxX && dfOtherMinX < dfMaxX &&
                    dfMinY < dfOtherMaxY && dfOtherMinY < dfMaxY)
                {
                    bCanUseBuildVRT = false;
                    break;
                }
            }
            if (!bCanUseBuildVRT)
                break;
        }
    }
    return bCanUseBuildVRT;
}

/************************************************************************/
/*                      DealWithCOGOptions()                            */
/************************************************************************/

static bool DealWithCOGOptions(CPLStringList &aosCreateOptions, int nSrcCount,
                               GDALDatasetH *pahSrcDS,
                               GDALWarpAppOptions *psOptions)
{
    const auto SetDstSRS = [psOptions](const std::string &osTargetSRS)
    {
        const char *pszExistingDstSRS =
            psOptions->aosTransformerOptions.FetchNameValue("DST_SRS");
        if (pszExistingDstSRS)
        {
            OGRSpatialReference oSRS1;
            oSRS1.SetFromUserInput(pszExistingDstSRS);
            OGRSpatialReference oSRS2;
            oSRS2.SetFromUserInput(osTargetSRS.c_str());
            if (!oSRS1.IsSame(&oSRS2))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Target SRS implied by COG creation options is not "
                         "the same as the one specified by -t_srs");
                return false;
            }
        }
        psOptions->aosTransformerOptions.SetNameValue("DST_SRS",
                                                      osTargetSRS.c_str());
        return true;
    };

    if (!(psOptions->dfMinX == 0 && psOptions->dfMinY == 0 &&
          psOptions->dfMaxX == 0 && psOptions->dfMaxY == 0 &&
          psOptions->dfXRes == 0 && psOptions->dfYRes == 0 &&
          psOptions->nForcePixels == 0 && psOptions->nForceLines == 0))
    {
        CPLString osTargetSRS;
        if (COGGetTargetSRS(aosCreateOptions.List(), osTargetSRS))
        {
            if (!SetDstSRS(osTargetSRS))
                return false;
        }
        if (!psOptions->bResampleAlgSpecifiedByUser && nSrcCount > 0)
        {
            GetResampleAlg(
                COGGetResampling(GDALDataset::FromHandle(pahSrcDS[0]),
                                 aosCreateOptions.List())
                    .c_str(),
                psOptions->eResampleAlg);
        }
        return true;
    }

    GDALWarpAppOptions oClonedOptions(*psOptions);
    oClonedOptions.bQuiet = true;
    CPLString osTmpFilename;
    osTmpFilename.Printf("/vsimem/gdalwarp/%p.tif", &oClonedOptions);
    CPLStringList aosTmpGTiffCreateOptions;
    aosTmpGTiffCreateOptions.SetNameValue("SPARSE_OK", "YES");
    aosTmpGTiffCreateOptions.SetNameValue("TILED", "YES");
    aosTmpGTiffCreateOptions.SetNameValue("BLOCKXSIZE", "4096");
    aosTmpGTiffCreateOptions.SetNameValue("BLOCKYSIZE", "4096");
    auto hTmpDS = GDALWarpCreateOutput(
        nSrcCount, pahSrcDS, osTmpFilename, "GTiff",
        oClonedOptions.aosTransformerOptions.List(),
        aosTmpGTiffCreateOptions.List(), oClonedOptions.eOutputType, nullptr,
        false, &oClonedOptions);

    if (hTmpDS == nullptr)
    {
        return false;
    }

    CPLString osResampling;
    CPLString osTargetSRS;
    int nXSize = 0;
    int nYSize = 0;
    double dfMinX = 0;
    double dfMinY = 0;
    double dfMaxX = 0;
    double dfMaxY = 0;
    bool bRet = true;
    if (COGGetWarpingCharacteristics(GDALDataset::FromHandle(hTmpDS),
                                     aosCreateOptions.List(), osResampling,
                                     osTargetSRS, nXSize, nYSize, dfMinX,
                                     dfMinY, dfMaxX, dfMaxY))
    {
        if (!psOptions->bResampleAlgSpecifiedByUser)
            GetResampleAlg(osResampling, psOptions->eResampleAlg);
        if (!SetDstSRS(osTargetSRS))
            bRet = false;
        psOptions->dfMinX = dfMinX;
        psOptions->dfMinY = dfMinY;
        psOptions->dfMaxX = dfMaxX;
        psOptions->dfMaxY = dfMaxY;
        psOptions->nForcePixels = nXSize;
        psOptions->nForceLines = nYSize;
        COGRemoveWarpingOptions(aosCreateOptions);
    }
    GDALClose(hTmpDS);
    VSIUnlink(osTmpFilename);
    return bRet;
}

/************************************************************************/
/*                      GDALWarpIndirect()                              */
/************************************************************************/

static GDALDatasetH GDALWarpDirect(const char *pszDest, GDALDatasetH hDstDS,
                                   int nSrcCount, GDALDatasetH *pahSrcDS,
                                   GDALWarpAppOptions *psOptions,
                                   int *pbUsageError);

static int CPL_STDCALL myScaledProgress(double dfProgress, const char *,
                                        void *pProgressData)
{
    return GDALScaledProgress(dfProgress, nullptr, pProgressData);
}

static GDALDatasetH GDALWarpIndirect(const char *pszDest, GDALDriverH hDriver,
                                     int nSrcCount, GDALDatasetH *pahSrcDS,
                                     GDALWarpAppOptions *psOptions,
                                     int *pbUsageError)
{
    CPLStringList aosCreateOptions(psOptions->aosCreateOptions);
    psOptions->aosCreateOptions.Clear();

    // Do not use a warped VRT input for COG output, because that would cause
    // warping to be done both during overview computation and creation of
    // full resolution image. Better materialize a temporary GTiff a bit later
    // in that method.
    if (nSrcCount == 1 && !EQUAL(psOptions->osFormat.c_str(), "COG"))
    {
        psOptions->osFormat = "VRT";
        auto pfnProgress = psOptions->pfnProgress;
        psOptions->pfnProgress = GDALDummyProgress;
        auto pProgressData = psOptions->pProgressData;
        psOptions->pProgressData = nullptr;

        auto hTmpDS = GDALWarpDirect("", nullptr, nSrcCount, pahSrcDS,
                                     psOptions, pbUsageError);
        if (hTmpDS)
        {
            auto hRet = GDALCreateCopy(hDriver, pszDest, hTmpDS, FALSE,
                                       aosCreateOptions.List(), pfnProgress,
                                       pProgressData);
            GDALClose(hTmpDS);
            return hRet;
        }
        return nullptr;
    }

    // Detect a pure mosaicing situation where a BuildVRT approach is
    // sufficient.
    GDALDatasetH hTmpDS = nullptr;
    if (psOptions->aosTransformerOptions.empty() &&
        psOptions->eOutputType == GDT_Unknown && psOptions->dfMinX == 0 &&
        psOptions->dfMinY == 0 && psOptions->dfMaxX == 0 &&
        psOptions->dfMaxY == 0 && psOptions->dfXRes == 0 &&
        psOptions->dfYRes == 0 && psOptions->nForcePixels == 0 &&
        psOptions->nForceLines == 0 &&
        psOptions->osCutlineDSNameOrWKT.empty() &&
        CanUseBuildVRT(nSrcCount, pahSrcDS))
    {
        CPLStringList aosArgv;
        const int nBands = GDALGetRasterCount(pahSrcDS[0]);
        if ((nBands == 1 ||
             (nBands > 1 && GDALGetRasterColorInterpretation(GDALGetRasterBand(
                                pahSrcDS[0], nBands)) != GCI_AlphaBand)) &&
            (psOptions->bEnableDstAlpha ||
             (EQUAL(psOptions->osFormat.c_str(), "COG") &&
              COGHasWarpingOptions(aosCreateOptions.List()) &&
              CPLTestBool(
                  aosCreateOptions.FetchNameValueDef("ADD_ALPHA", "YES")))))
        {
            aosArgv.AddString("-addalpha");
        }
        auto psBuildVRTOptions =
            GDALBuildVRTOptionsNew(aosArgv.List(), nullptr);
        hTmpDS = GDALBuildVRT("", nSrcCount, pahSrcDS, nullptr,
                              psBuildVRTOptions, nullptr);
        GDALBuildVRTOptionsFree(psBuildVRTOptions);
    }
    auto pfnProgress = psOptions->pfnProgress;
    auto pProgressData = psOptions->pProgressData;
    CPLString osTmpFilename;
    double dfStartPctCreateCopy = 0.0;
    if (hTmpDS == nullptr)
    {
        // Special processing for COG output. As some of its options do
        // on-the-fly reprojection, take them into account now, and remove them
        // from the COG creation stage.
        if (EQUAL(psOptions->osFormat.c_str(), "COG") &&
            !DealWithCOGOptions(aosCreateOptions, nSrcCount, pahSrcDS,
                                psOptions))
        {
            return nullptr;
        }

        // Materialize a temporary GeoTIFF with the result of the warp
        psOptions->osFormat = "GTiff";
        psOptions->aosCreateOptions.AddString("SPARSE_OK=YES");
        psOptions->aosCreateOptions.AddString("COMPRESS=LZW");
        psOptions->aosCreateOptions.AddString("TILED=YES");
        psOptions->aosCreateOptions.AddString("BIGTIFF=YES");
        psOptions->pfnProgress = myScaledProgress;
        dfStartPctCreateCopy = 2. / 3;
        psOptions->pProgressData = GDALCreateScaledProgress(
            0, dfStartPctCreateCopy, pfnProgress, pProgressData);
        osTmpFilename = pszDest;
        osTmpFilename += ".tmp.tif";
        hTmpDS = GDALWarpDirect(osTmpFilename, nullptr, nSrcCount, pahSrcDS,
                                psOptions, pbUsageError);
        GDALDestroyScaledProgress(psOptions->pProgressData);
        psOptions->pfnProgress = nullptr;
        psOptions->pProgressData = nullptr;
    }
    if (hTmpDS)
    {
        auto pScaledProgressData = GDALCreateScaledProgress(
            dfStartPctCreateCopy, 1.0, pfnProgress, pProgressData);
        auto hRet = GDALCreateCopy(hDriver, pszDest, hTmpDS, FALSE,
                                   aosCreateOptions.List(), myScaledProgress,
                                   pScaledProgressData);
        GDALDestroyScaledProgress(pScaledProgressData);
        GDALClose(hTmpDS);
        if (!osTmpFilename.empty())
        {
            GDALDeleteDataset(GDALGetDriverByName("GTiff"), osTmpFilename);
        }
        return hRet;
    }
    return nullptr;
}

/************************************************************************/
/*                             GDALWarp()                               */
/************************************************************************/

/**
 * Image reprojection and warping function.
 *
 * This is the equivalent of the <a href="/programs/gdalwarp.html">gdalwarp</a>
 * utility.
 *
 * GDALWarpAppOptions* must be allocated and freed with GDALWarpAppOptionsNew()
 * and GDALWarpAppOptionsFree() respectively.
 * pszDest and hDstDS cannot be used at the same time.
 *
 * @param pszDest the destination dataset path or NULL.
 * @param hDstDS the destination dataset or NULL.
 * @param nSrcCount the number of input datasets.
 * @param pahSrcDS the list of input datasets. For practical purposes, the type
 * of this argument should be considered as "const GDALDatasetH* const*", that
 * is neither the array nor its values are mutated by this function.
 * @param psOptionsIn the options struct returned by GDALWarpAppOptionsNew() or
 * NULL.
 * @param pbUsageError pointer to a integer output variable to store if any
 * usage error has occurred, or NULL.
 * @return the output dataset (new dataset that must be closed using
 * GDALClose(), or hDstDS if not NULL) or NULL in case of error. If the output
 * format is a VRT dataset, then the returned VRT dataset has a reference to
 * pahSrcDS[0]. Hence pahSrcDS[0] should be closed after the returned dataset
 * if using GDALClose().
 * A safer alternative is to use GDALReleaseDataset() instead of using
 * GDALClose(), in which case you can close datasets in any order.
 *
 * @since GDAL 2.1
 */

GDALDatasetH GDALWarp(const char *pszDest, GDALDatasetH hDstDS, int nSrcCount,
                      GDALDatasetH *pahSrcDS,
                      const GDALWarpAppOptions *psOptionsIn, int *pbUsageError)
{
    CPLErrorReset();

    for (int i = 0; i < nSrcCount; i++)
    {
        if (!pahSrcDS[i])
            return nullptr;
    }

    GDALWarpAppOptions oOptionsTmp;
    if (psOptionsIn)
        oOptionsTmp = *psOptionsIn;
    GDALWarpAppOptions *psOptions = &oOptionsTmp;

    if (hDstDS == nullptr)
    {
        if (psOptions->osFormat.empty())
        {
            const std::string osFormat = GetOutputDriverForRaster(pszDest);
            if (osFormat.empty())
            {
                return nullptr;
            }
            psOptions->osFormat = osFormat;
        }

        auto hDriver = GDALGetDriverByName(psOptions->osFormat.c_str());
        if (hDriver != nullptr &&
            GDALGetMetadataItem(hDriver, GDAL_DCAP_CREATE, nullptr) ==
                nullptr &&
            GDALGetMetadataItem(hDriver, GDAL_DCAP_CREATECOPY, nullptr) !=
                nullptr)
        {
            auto ret = GDALWarpIndirect(pszDest, hDriver, nSrcCount, pahSrcDS,
                                        psOptions, pbUsageError);
            return ret;
        }
    }

    auto ret = GDALWarpDirect(pszDest, hDstDS, nSrcCount, pahSrcDS, psOptions,
                              pbUsageError);

    return ret;
}

/************************************************************************/
/*                    UseTEAndTSAndTRConsistently()                     */
/************************************************************************/

static bool UseTEAndTSAndTRConsistently(const GDALWarpAppOptions *psOptions)
{
    // We normally don't allow -te, -ts and -tr together, unless they are all
    // consistent. The interest of this is to use the -tr values to produce
    // exact pixel size, rather than inferring it from -te and -ts

    // Constant and logic to be kept in sync with cogdriver.cpp
    constexpr double RELATIVE_ERROR_RES_SHARED_BY_COG_AND_GDALWARP = 1e-8;
    return psOptions->nForcePixels != 0 && psOptions->nForceLines != 0 &&
           psOptions->dfXRes != 0 && psOptions->dfYRes != 0 &&
           !(psOptions->dfMinX == 0.0 && psOptions->dfMinY == 0.0 &&
             psOptions->dfMaxX == 0.0 && psOptions->dfMaxY == 0.0) &&
           fabs((psOptions->dfMaxX - psOptions->dfMinX) / psOptions->dfXRes -
                psOptions->nForcePixels) <=
               RELATIVE_ERROR_RES_SHARED_BY_COG_AND_GDALWARP &&
           fabs((psOptions->dfMaxY - psOptions->dfMinY) / psOptions->dfYRes -
                psOptions->nForceLines) <=
               RELATIVE_ERROR_RES_SHARED_BY_COG_AND_GDALWARP;
}

/************************************************************************/
/*                            CheckOptions()                            */
/************************************************************************/

static bool CheckOptions(const char *pszDest, GDALDatasetH hDstDS,
                         int nSrcCount, GDALDatasetH *pahSrcDS,
                         GDALWarpAppOptions *psOptions, bool &bVRT,
                         int *pbUsageError)
{

    if (hDstDS)
    {
        if (psOptions->bCreateOutput == true)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "All options related to creation ignored in update mode");
            psOptions->bCreateOutput = false;
        }
    }

    if ((psOptions->osFormat.empty() &&
         EQUAL(CPLGetExtension(pszDest), "VRT")) ||
        (EQUAL(psOptions->osFormat.c_str(), "VRT")))
    {
        if (hDstDS != nullptr)
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                     "VRT output not compatible with existing dataset.");
            return false;
        }

        bVRT = true;

        if (nSrcCount > 1)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "gdalwarp -of VRT just takes into account "
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
        (psOptions->dfXRes != 0 && psOptions->dfYRes != 0) &&
        !UseTEAndTSAndTRConsistently(psOptions))
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "-tr and -ts options cannot be used at the same time.");
        if (pbUsageError)
            *pbUsageError = TRUE;
        return false;
    }

    if (psOptions->bTargetAlignedPixels && psOptions->dfXRes == 0 &&
        psOptions->dfYRes == 0)
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "-tap option cannot be used without using -tr.");
        if (pbUsageError)
            *pbUsageError = TRUE;
        return false;
    }

    if (!psOptions->bQuiet &&
        !(psOptions->dfMinX == 0.0 && psOptions->dfMinY == 0.0 &&
          psOptions->dfMaxX == 0.0 && psOptions->dfMaxY == 0.0))
    {
        if (psOptions->dfMinX >= psOptions->dfMaxX)
            CPLError(CE_Warning, CPLE_AppDefined,
                     "-te values have minx >= maxx. This will result in a "
                     "horizontally flipped image.");
        if (psOptions->dfMinY >= psOptions->dfMaxY)
            CPLError(CE_Warning, CPLE_AppDefined,
                     "-te values have miny >= maxy. This will result in a "
                     "vertically flipped image.");
    }

    if (psOptions->dfErrorThreshold < 0)
    {
        // By default, use approximate transformer unless RPC_DEM is specified
        if (psOptions->aosTransformerOptions.FetchNameValue("RPC_DEM") !=
            nullptr)
            psOptions->dfErrorThreshold = 0.0;
        else
            psOptions->dfErrorThreshold = 0.125;
    }

    /* -------------------------------------------------------------------- */
    /*      -te_srs option                                                  */
    /* -------------------------------------------------------------------- */
    if (!psOptions->osTE_SRS.empty())
    {
        if (psOptions->dfMinX == 0.0 && psOptions->dfMinY == 0.0 &&
            psOptions->dfMaxX == 0.0 && psOptions->dfMaxY == 0.0)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "-te_srs ignored since -te is not specified.");
        }
        else
        {
            OGRSpatialReference oSRSIn;
            oSRSIn.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
            oSRSIn.SetFromUserInput(psOptions->osTE_SRS.c_str());
            OGRSpatialReference oSRSDS;
            oSRSDS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
            bool bOK = false;
            if (psOptions->aosTransformerOptions.FetchNameValue("DST_SRS") !=
                nullptr)
            {
                oSRSDS.SetFromUserInput(
                    psOptions->aosTransformerOptions.FetchNameValue("DST_SRS"));
                bOK = true;
            }
            else if (psOptions->aosTransformerOptions.FetchNameValue(
                         "SRC_SRS") != nullptr)
            {
                oSRSDS.SetFromUserInput(
                    psOptions->aosTransformerOptions.FetchNameValue("SRC_SRS"));
                bOK = true;
            }
            else
            {
                if (nSrcCount && GDALGetProjectionRef(pahSrcDS[0]) &&
                    GDALGetProjectionRef(pahSrcDS[0])[0])
                {
                    oSRSDS.SetFromUserInput(GDALGetProjectionRef(pahSrcDS[0]));
                    bOK = true;
                }
            }
            if (!bOK)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "-te_srs ignored since none of -t_srs, -s_srs is "
                         "specified or the input dataset has no projection.");
                return false;
            }
            if (!oSRSIn.IsSame(&oSRSDS))
            {
                double dfWestLongitudeDeg = 0.0;
                double dfSouthLatitudeDeg = 0.0;
                double dfEastLongitudeDeg = 0.0;
                double dfNorthLatitudeDeg = 0.0;

                OGRCoordinateTransformationOptions options;
                if (GDALComputeAreaOfInterest(
                        &oSRSIn, psOptions->dfMinX, psOptions->dfMinY,
                        psOptions->dfMaxX, psOptions->dfMaxY,
                        dfWestLongitudeDeg, dfSouthLatitudeDeg,
                        dfEastLongitudeDeg, dfNorthLatitudeDeg))
                {
                    options.SetAreaOfInterest(
                        dfWestLongitudeDeg, dfSouthLatitudeDeg,
                        dfEastLongitudeDeg, dfNorthLatitudeDeg);
                }
                OGRCoordinateTransformation *poCT =
                    OGRCreateCoordinateTransformation(&oSRSIn, &oSRSDS,
                                                      options);
                if (!(poCT &&
                      poCT->Transform(1, &psOptions->dfMinX,
                                      &psOptions->dfMinY) &&
                      poCT->Transform(1, &psOptions->dfMaxX,
                                      &psOptions->dfMaxY)))
                {
                    OGRCoordinateTransformation::DestroyCT(poCT);

                    CPLError(CE_Failure, CPLE_AppDefined,
                             "-te_srs ignored since coordinate transformation "
                             "failed.");
                    return false;
                }
                delete poCT;
            }
        }
    }
    return true;
}

/************************************************************************/
/*                       ProcessCutlineOptions()                        */
/************************************************************************/

static bool ProcessCutlineOptions(int nSrcCount, GDALDatasetH *pahSrcDS,
                                  GDALWarpAppOptions *psOptions,
                                  OGRGeometryH &hCutline)
{
    if (!psOptions->osCutlineDSNameOrWKT.empty())
    {
        CPLErr eError;
        eError = LoadCutline(psOptions->osCutlineDSNameOrWKT,
                             psOptions->osCutlineSRS, psOptions->osCLayer,
                             psOptions->osCWHERE, psOptions->osCSQL, &hCutline);
        if (eError == CE_Failure)
        {
            return false;
        }
    }

    if (psOptions->bCropToCutline && hCutline != nullptr)
    {
        CPLErr eError;
        eError = CropToCutline(OGRGeometry::FromHandle(hCutline),
                               psOptions->aosTransformerOptions.List(),
                               psOptions->aosWarpOptions.List(), nSrcCount,
                               pahSrcDS, psOptions->dfMinX, psOptions->dfMinY,
                               psOptions->dfMaxX, psOptions->dfMaxY, psOptions);
        if (eError == CE_Failure)
        {
            return false;
        }
    }

    const char *pszWarpThreads =
        psOptions->aosWarpOptions.FetchNameValue("NUM_THREADS");
    if (pszWarpThreads != nullptr)
    {
        /* Used by TPS transformer to parallelize direct and inverse matrix
         * computation */
        psOptions->aosTransformerOptions.SetNameValue("NUM_THREADS",
                                                      pszWarpThreads);
    }

    return true;
}

/************************************************************************/
/*                            CreateOutput()                            */
/************************************************************************/

static GDALDatasetH CreateOutput(const char *pszDest, int nSrcCount,
                                 GDALDatasetH *pahSrcDS,
                                 GDALWarpAppOptions *psOptions,
                                 const bool bInitDestSetByUser,
                                 void *&hUniqueTransformArg)
{
    if (nSrcCount == 1 && !psOptions->bDisableSrcAlpha)
    {
        if (GDALGetRasterCount(pahSrcDS[0]) > 0 &&
            GDALGetRasterColorInterpretation(GDALGetRasterBand(
                pahSrcDS[0], GDALGetRasterCount(pahSrcDS[0]))) == GCI_AlphaBand)
        {
            psOptions->bEnableSrcAlpha = true;
            psOptions->bEnableDstAlpha = true;
            if (!psOptions->bQuiet)
                printf("Using band %d of source image as alpha.\n",
                       GDALGetRasterCount(pahSrcDS[0]));
        }
    }

    auto hDstDS = GDALWarpCreateOutput(
        nSrcCount, pahSrcDS, pszDest, psOptions->osFormat.c_str(),
        psOptions->aosTransformerOptions.List(),
        psOptions->aosCreateOptions.List(), psOptions->eOutputType,
        &hUniqueTransformArg, psOptions->bSetColorInterpretation, psOptions);
    if (hDstDS == nullptr)
    {
        return nullptr;
    }
    psOptions->bCreateOutput = true;

    if (!bInitDestSetByUser)
    {
        if (psOptions->osDstNodata.empty())
        {
            psOptions->aosWarpOptions.SetNameValue("INIT_DEST", "0");
        }
        else
        {
            psOptions->aosWarpOptions.SetNameValue("INIT_DEST", "NO_DATA");
        }
    }

    return hDstDS;
}

/************************************************************************/
/*                           ProcessMetadata()                          */
/************************************************************************/

static void ProcessMetadata(int iSrc, GDALDatasetH hSrcDS, GDALDatasetH hDstDS,
                            GDALWarpAppOptions *psOptions,
                            const bool bEnableDstAlpha)
{
    if (psOptions->bCopyMetadata)
    {
        const char *pszSrcInfo = nullptr;
        GDALRasterBandH hSrcBand = nullptr;
        GDALRasterBandH hDstBand = nullptr;

        /* copy metadata from first dataset */
        if (iSrc == 0)
        {
            CPLDebug(
                "WARP",
                "Copying metadata from first source to destination dataset");
            /* copy dataset-level metadata */
            char **papszMetadata = GDALGetMetadata(hSrcDS, nullptr);

            char **papszMetadataNew = nullptr;
            for (int i = 0;
                 papszMetadata != nullptr && papszMetadata[i] != nullptr; i++)
            {
                // Do not preserve NODATA_VALUES when the output includes an
                // alpha band
                if (bEnableDstAlpha &&
                    STARTS_WITH_CI(papszMetadata[i], "NODATA_VALUES="))
                {
                    continue;
                }
                // Do not preserve the CACHE_PATH from the WMS driver
                if (STARTS_WITH_CI(papszMetadata[i], "CACHE_PATH="))
                {
                    continue;
                }

                papszMetadataNew =
                    CSLAddString(papszMetadataNew, papszMetadata[i]);
            }

            if (CSLCount(papszMetadataNew) > 0)
            {
                if (GDALSetMetadata(hDstDS, papszMetadataNew, nullptr) !=
                    CE_None)
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "error copying metadata to destination dataset.");
            }

            CSLDestroy(papszMetadataNew);

            /* ISIS3 -> ISIS3 special case */
            if (EQUAL(psOptions->osFormat.c_str(), "ISIS3"))
            {
                char **papszMD_ISIS3 = GDALGetMetadata(hSrcDS, "json:ISIS3");
                if (papszMD_ISIS3 != nullptr)
                    GDALSetMetadata(hDstDS, papszMD_ISIS3, "json:ISIS3");
            }
            else if (EQUAL(psOptions->osFormat.c_str(), "PDS4"))
            {
                char **papszMD_PDS4 = GDALGetMetadata(hSrcDS, "xml:PDS4");
                if (papszMD_PDS4 != nullptr)
                    GDALSetMetadata(hDstDS, papszMD_PDS4, "xml:PDS4");
            }
            else if (EQUAL(psOptions->osFormat.c_str(), "VICAR"))
            {
                char **papszMD_VICAR = GDALGetMetadata(hSrcDS, "json:VICAR");
                if (papszMD_VICAR != nullptr)
                    GDALSetMetadata(hDstDS, papszMD_VICAR, "json:VICAR");
            }

            /* copy band-level metadata and other info */
            if (GDALGetRasterCount(hSrcDS) == GDALGetRasterCount(hDstDS))
            {
                for (int iBand = 0; iBand < GDALGetRasterCount(hSrcDS); iBand++)
                {
                    hSrcBand = GDALGetRasterBand(hSrcDS, iBand + 1);
                    hDstBand = GDALGetRasterBand(hDstDS, iBand + 1);
                    /* copy metadata, except stats (#5319) */
                    papszMetadata = GDALGetMetadata(hSrcBand, nullptr);
                    if (CSLCount(papszMetadata) > 0)
                    {
                        // GDALSetMetadata( hDstBand, papszMetadata, NULL );
                        papszMetadataNew = nullptr;
                        for (int i = 0; papszMetadata != nullptr &&
                                        papszMetadata[i] != nullptr;
                             i++)
                        {
                            if (!STARTS_WITH(papszMetadata[i], "STATISTICS_"))
                                papszMetadataNew = CSLAddString(
                                    papszMetadataNew, papszMetadata[i]);
                        }
                        GDALSetMetadata(hDstBand, papszMetadataNew, nullptr);
                        CSLDestroy(papszMetadataNew);
                    }
                    /* copy other info (Description, Unit Type) - what else? */
                    if (psOptions->bCopyBandInfo)
                    {
                        pszSrcInfo = GDALGetDescription(hSrcBand);
                        if (pszSrcInfo != nullptr && strlen(pszSrcInfo) > 0)
                            GDALSetDescription(hDstBand, pszSrcInfo);
                        pszSrcInfo = GDALGetRasterUnitType(hSrcBand);
                        if (pszSrcInfo != nullptr && strlen(pszSrcInfo) > 0)
                            GDALSetRasterUnitType(hDstBand, pszSrcInfo);
                    }
                }
            }
        }
        /* remove metadata that conflicts between datasets */
        else
        {
            CPLDebug("WARP",
                     "Removing conflicting metadata from destination dataset "
                     "(source #%d)",
                     iSrc);
            /* remove conflicting dataset-level metadata */
            RemoveConflictingMetadata(hDstDS, GDALGetMetadata(hSrcDS, nullptr),
                                      psOptions->osMDConflictValue.c_str());

            /* remove conflicting copy band-level metadata and other info */
            if (GDALGetRasterCount(hSrcDS) == GDALGetRasterCount(hDstDS))
            {
                for (int iBand = 0; iBand < GDALGetRasterCount(hSrcDS); iBand++)
                {
                    hSrcBand = GDALGetRasterBand(hSrcDS, iBand + 1);
                    hDstBand = GDALGetRasterBand(hDstDS, iBand + 1);
                    /* remove conflicting metadata */
                    RemoveConflictingMetadata(
                        hDstBand, GDALGetMetadata(hSrcBand, nullptr),
                        psOptions->osMDConflictValue.c_str());
                    /* remove conflicting info */
                    if (psOptions->bCopyBandInfo)
                    {
                        pszSrcInfo = GDALGetDescription(hSrcBand);
                        const char *pszDstInfo = GDALGetDescription(hDstBand);
                        if (!(pszSrcInfo != nullptr && strlen(pszSrcInfo) > 0 &&
                              pszDstInfo != nullptr && strlen(pszDstInfo) > 0 &&
                              EQUAL(pszSrcInfo, pszDstInfo)))
                            GDALSetDescription(hDstBand, "");
                        pszSrcInfo = GDALGetRasterUnitType(hSrcBand);
                        pszDstInfo = GDALGetRasterUnitType(hDstBand);
                        if (!(pszSrcInfo != nullptr && strlen(pszSrcInfo) > 0 &&
                              pszDstInfo != nullptr && strlen(pszDstInfo) > 0 &&
                              EQUAL(pszSrcInfo, pszDstInfo)))
                            GDALSetRasterUnitType(hDstBand, "");
                    }
                }
            }
        }
    }
}

/************************************************************************/
/*                             SetupNoData()                            */
/************************************************************************/

static void SetupNoData(const char *pszDest, int iSrc, GDALDatasetH hSrcDS,
                        GDALDatasetH hWrkSrcDS, GDALDatasetH hDstDS,
                        GDALWarpOptions *psWO, GDALWarpAppOptions *psOptions,
                        const bool bEnableDstAlpha,
                        const bool bInitDestSetByUser)
{
    if (!psOptions->osSrcNodata.empty() &&
        !EQUAL(psOptions->osSrcNodata.c_str(), "none"))
    {
        char **papszTokens = CSLTokenizeString(psOptions->osSrcNodata.c_str());
        const int nTokenCount = CSLCount(papszTokens);

        psWO->padfSrcNoDataReal =
            static_cast<double *>(CPLMalloc(psWO->nBandCount * sizeof(double)));
        psWO->padfSrcNoDataImag = nullptr;

        for (int i = 0; i < psWO->nBandCount; i++)
        {
            if (i < nTokenCount)
            {
                if (strchr(papszTokens[i], 'i') != nullptr)
                {
                    if (psWO->padfSrcNoDataImag == nullptr)
                    {
                        psWO->padfSrcNoDataImag = static_cast<double *>(
                            CPLCalloc(psWO->nBandCount, sizeof(double)));
                    }
                    CPLStringToComplex(papszTokens[i],
                                       psWO->padfSrcNoDataReal + i,
                                       psWO->padfSrcNoDataImag + i);
                    psWO->padfSrcNoDataReal[i] =
                        GDALAdjustNoDataCloseToFloatMax(
                            psWO->padfSrcNoDataReal[i]);
                    psWO->padfSrcNoDataImag[i] =
                        GDALAdjustNoDataCloseToFloatMax(
                            psWO->padfSrcNoDataImag[i]);
                }
                else
                {
                    psWO->padfSrcNoDataReal[i] =
                        GDALAdjustNoDataCloseToFloatMax(
                            CPLAtof(papszTokens[i]));
                }
            }
            else
            {
                psWO->padfSrcNoDataReal[i] = psWO->padfSrcNoDataReal[i - 1];
                if (psWO->padfSrcNoDataImag != nullptr)
                {
                    psWO->padfSrcNoDataImag[i] = psWO->padfSrcNoDataImag[i - 1];
                }
            }
        }

        CSLDestroy(papszTokens);

        if (psWO->nBandCount > 1 &&
            CSLFetchNameValue(psWO->papszWarpOptions, "UNIFIED_SRC_NODATA") ==
                nullptr)
        {
            CPLDebug("WARP", "Set UNIFIED_SRC_NODATA=YES");
            psWO->papszWarpOptions = CSLSetNameValue(
                psWO->papszWarpOptions, "UNIFIED_SRC_NODATA", "YES");
        }
    }

    /* -------------------------------------------------------------------- */
    /*      If -srcnodata was not specified, but the data has nodata        */
    /*      values, use them.                                               */
    /* -------------------------------------------------------------------- */
    if (psOptions->osSrcNodata.empty())
    {
        int bHaveNodata = FALSE;
        double dfReal = 0.0;

        for (int i = 0; !bHaveNodata && i < psWO->nBandCount; i++)
        {
            GDALRasterBandH hBand =
                GDALGetRasterBand(hWrkSrcDS, psWO->panSrcBands[i]);
            dfReal = GDALGetRasterNoDataValue(hBand, &bHaveNodata);
        }

        if (bHaveNodata)
        {
            if (!psOptions->bQuiet)
            {
                if (CPLIsNan(dfReal))
                    printf("Using internal nodata values (e.g. nan) for image "
                           "%s.\n",
                           GDALGetDescription(hSrcDS));
                else
                    printf("Using internal nodata values (e.g. %g) for image "
                           "%s.\n",
                           dfReal, GDALGetDescription(hSrcDS));
            }
            psWO->padfSrcNoDataReal = static_cast<double *>(
                CPLMalloc(psWO->nBandCount * sizeof(double)));

            for (int i = 0; i < psWO->nBandCount; i++)
            {
                GDALRasterBandH hBand =
                    GDALGetRasterBand(hWrkSrcDS, psWO->panSrcBands[i]);

                dfReal = GDALGetRasterNoDataValue(hBand, &bHaveNodata);

                if (bHaveNodata)
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
    if (!psOptions->osDstNodata.empty() &&
        !EQUAL(psOptions->osDstNodata.c_str(), "none"))
    {
        char **papszTokens = CSLTokenizeString(psOptions->osDstNodata.c_str());
        const int nTokenCount = CSLCount(papszTokens);
        bool bDstNoDataNone = true;

        psWO->padfDstNoDataReal =
            static_cast<double *>(CPLMalloc(psWO->nBandCount * sizeof(double)));
        psWO->padfDstNoDataImag =
            static_cast<double *>(CPLMalloc(psWO->nBandCount * sizeof(double)));

        for (int i = 0; i < psWO->nBandCount; i++)
        {
            psWO->padfDstNoDataReal[i] = -1.1e20;
            psWO->padfDstNoDataImag[i] = 0.0;

            if (i < nTokenCount)
            {
                if (papszTokens[i] != nullptr && EQUAL(papszTokens[i], "none"))
                {
                    CPLDebug("WARP", "dstnodata of band %d not set", i);
                    bDstNoDataNone = true;
                    continue;
                }
                else if (papszTokens[i] ==
                         nullptr)  // this should not happen, but just in case
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Error parsing dstnodata arg #%d", i);
                    bDstNoDataNone = true;
                    continue;
                }
                CPLStringToComplex(papszTokens[i], psWO->padfDstNoDataReal + i,
                                   psWO->padfDstNoDataImag + i);
                psWO->padfDstNoDataReal[i] =
                    GDALAdjustNoDataCloseToFloatMax(psWO->padfDstNoDataReal[i]);
                psWO->padfDstNoDataImag[i] =
                    GDALAdjustNoDataCloseToFloatMax(psWO->padfDstNoDataImag[i]);
                bDstNoDataNone = false;
                CPLDebug("WARP", "dstnodata of band %d set to %f", i,
                         psWO->padfDstNoDataReal[i]);
            }
            else
            {
                if (!bDstNoDataNone)
                {
                    psWO->padfDstNoDataReal[i] = psWO->padfDstNoDataReal[i - 1];
                    psWO->padfDstNoDataImag[i] = psWO->padfDstNoDataImag[i - 1];
                    CPLDebug("WARP",
                             "dstnodata of band %d set from previous band", i);
                }
                else
                {
                    CPLDebug("WARP", "dstnodata value of band %d not set", i);
                    continue;
                }
            }

            GDALRasterBandH hBand =
                GDALGetRasterBand(hDstDS, psWO->panDstBands[i]);
            int bClamped = FALSE;
            int bRounded = FALSE;
            psWO->padfDstNoDataReal[i] = GDALAdjustValueToDataType(
                GDALGetRasterDataType(hBand), psWO->padfDstNoDataReal[i],
                &bClamped, &bRounded);

            if (bClamped)
            {
                CPLError(
                    CE_Warning, CPLE_AppDefined,
                    "for band %d, destination nodata value has been clamped "
                    "to %.0f, the original value being out of range.",
                    psWO->panDstBands[i], psWO->padfDstNoDataReal[i]);
            }
            else if (bRounded)
            {
                CPLError(
                    CE_Warning, CPLE_AppDefined,
                    "for band %d, destination nodata value has been rounded "
                    "to %.0f, %s being an integer datatype.",
                    psWO->panDstBands[i], psWO->padfDstNoDataReal[i],
                    GDALGetDataTypeName(GDALGetRasterDataType(hBand)));
            }

            if (psOptions->bCreateOutput && iSrc == 0)
            {
                GDALSetRasterNoDataValue(
                    GDALGetRasterBand(hDstDS, psWO->panDstBands[i]),
                    psWO->padfDstNoDataReal[i]);
            }
        }

        CSLDestroy(papszTokens);
    }

    /* check if the output dataset has already nodata */
    if (psOptions->osDstNodata.empty())
    {
        int bHaveNodataAll = TRUE;
        for (int i = 0; i < psWO->nBandCount; i++)
        {
            GDALRasterBandH hBand =
                GDALGetRasterBand(hDstDS, psWO->panDstBands[i]);
            int bHaveNodata = FALSE;
            GDALGetRasterNoDataValue(hBand, &bHaveNodata);
            bHaveNodataAll &= bHaveNodata;
        }
        if (bHaveNodataAll)
        {
            psWO->padfDstNoDataReal = static_cast<double *>(
                CPLMalloc(psWO->nBandCount * sizeof(double)));
            for (int i = 0; i < psWO->nBandCount; i++)
            {
                GDALRasterBandH hBand =
                    GDALGetRasterBand(hDstDS, psWO->panDstBands[i]);
                int bHaveNodata = FALSE;
                psWO->padfDstNoDataReal[i] =
                    GDALGetRasterNoDataValue(hBand, &bHaveNodata);
                CPLDebug("WARP", "band=%d dstNoData=%f", i,
                         psWO->padfDstNoDataReal[i]);
            }
        }
    }

    // If creating a new file that has default nodata value,
    // try to override the default output nodata values with the source ones.
    if (psOptions->osDstNodata.empty() && psWO->padfSrcNoDataReal != nullptr &&
        psWO->padfDstNoDataReal != nullptr && psOptions->bCreateOutput &&
        iSrc == 0 && !bEnableDstAlpha)
    {
        for (int i = 0; i < psWO->nBandCount; i++)
        {
            GDALRasterBandH hBand =
                GDALGetRasterBand(hDstDS, psWO->panDstBands[i]);
            int bHaveNodata = FALSE;
            CPLPushErrorHandler(CPLQuietErrorHandler);
            bool bRedefinedOK =
                (GDALSetRasterNoDataValue(hBand, psWO->padfSrcNoDataReal[i]) ==
                     CE_None &&
                 GDALGetRasterNoDataValue(hBand, &bHaveNodata) ==
                     psWO->padfSrcNoDataReal[i] &&
                 bHaveNodata);
            CPLPopErrorHandler();
            if (bRedefinedOK)
            {
                if (i == 0 && !psOptions->bQuiet)
                    printf("Copying nodata values from source %s "
                           "to destination %s.\n",
                           GDALGetDescription(hSrcDS), pszDest);
                psWO->padfDstNoDataReal[i] = psWO->padfSrcNoDataReal[i];

                if (i == 0 && !bInitDestSetByUser)
                {
                    /* As we didn't know at the beginning if there was source
                     * nodata */
                    /* we have initialized INIT_DEST=0. Override this with
                     * NO_DATA now */
                    psWO->papszWarpOptions = CSLSetNameValue(
                        psWO->papszWarpOptions, "INIT_DEST", "NO_DATA");
                }
            }
            else
            {
                break;
            }
        }
    }

    /* else try to fill dstNoData from source bands, unless -dstalpha is
     * specified */
    else if (psOptions->osDstNodata.empty() &&
             psWO->padfSrcNoDataReal != nullptr &&
             psWO->padfDstNoDataReal == nullptr && !bEnableDstAlpha)
    {
        psWO->padfDstNoDataReal =
            static_cast<double *>(CPLMalloc(psWO->nBandCount * sizeof(double)));

        if (psWO->padfSrcNoDataImag != nullptr)
        {
            psWO->padfDstNoDataImag = static_cast<double *>(
                CPLMalloc(psWO->nBandCount * sizeof(double)));
        }

        if (!psOptions->bQuiet)
            printf("Copying nodata values from source %s to destination %s.\n",
                   GDALGetDescription(hSrcDS), pszDest);

        for (int i = 0; i < psWO->nBandCount; i++)
        {
            psWO->padfDstNoDataReal[i] = psWO->padfSrcNoDataReal[i];
            if (psWO->padfSrcNoDataImag != nullptr)
            {
                psWO->padfDstNoDataImag[i] = psWO->padfSrcNoDataImag[i];
            }
            CPLDebug("WARP", "srcNoData=%f dstNoData=%f",
                     psWO->padfSrcNoDataReal[i], psWO->padfDstNoDataReal[i]);

            if (psOptions->bCreateOutput && iSrc == 0)
            {
                CPLDebug("WARP",
                         "calling GDALSetRasterNoDataValue() for band#%d", i);
                GDALSetRasterNoDataValue(
                    GDALGetRasterBand(hDstDS, psWO->panDstBands[i]),
                    psWO->padfDstNoDataReal[i]);
            }
        }

        if (psOptions->bCreateOutput && !bInitDestSetByUser && iSrc == 0)
        {
            /* As we didn't know at the beginning if there was source nodata */
            /* we have initialized INIT_DEST=0. Override this with NO_DATA now
             */
            psWO->papszWarpOptions =
                CSLSetNameValue(psWO->papszWarpOptions, "INIT_DEST", "NO_DATA");
        }
    }
}

/************************************************************************/
/*                         SetupSkipNoSource()                          */
/************************************************************************/

static void SetupSkipNoSource(int iSrc, GDALDatasetH hDstDS,
                              GDALWarpOptions *psWO,
                              GDALWarpAppOptions *psOptions)
{
    if (psOptions->bCreateOutput && iSrc == 0 &&
        CSLFetchNameValue(psWO->papszWarpOptions, "SKIP_NOSOURCE") == nullptr &&
        CSLFetchNameValue(psWO->papszWarpOptions, "STREAMABLE_OUTPUT") ==
            nullptr &&
        // This white list of drivers could potentially be extended.
        (EQUAL(psOptions->osFormat.c_str(), "MEM") ||
         EQUAL(psOptions->osFormat.c_str(), "GTiff") ||
         EQUAL(psOptions->osFormat.c_str(), "GPKG")))
    {
        // We can enable the optimization only if the user didn't specify
        // a INIT_DEST value that would contradict the destination nodata.

        bool bOKRegardingInitDest = false;
        const char *pszInitDest =
            CSLFetchNameValue(psWO->papszWarpOptions, "INIT_DEST");
        if (pszInitDest == nullptr || EQUAL(pszInitDest, "NO_DATA"))
        {
            bOKRegardingInitDest = true;

            // The MEM driver will return non-initialized blocks at 0
            // so make sure that the nodata value is 0.
            if (EQUAL(psOptions->osFormat.c_str(), "MEM"))
            {
                for (int i = 0; i < GDALGetRasterCount(hDstDS); i++)
                {
                    int bHasNoData = false;
                    double dfDstNoDataVal = GDALGetRasterNoDataValue(
                        GDALGetRasterBand(hDstDS, i + 1), &bHasNoData);
                    if (bHasNoData && dfDstNoDataVal != 0.0)
                    {
                        bOKRegardingInitDest = false;
                        break;
                    }
                }
            }
        }
        else
        {
            char **papszTokensInitDest = CSLTokenizeString(pszInitDest);
            const int nTokenCountInitDest = CSLCount(papszTokensInitDest);
            if (nTokenCountInitDest == 1 ||
                nTokenCountInitDest == GDALGetRasterCount(hDstDS))
            {
                bOKRegardingInitDest = true;
                for (int i = 0; i < GDALGetRasterCount(hDstDS); i++)
                {
                    double dfInitVal = GDALAdjustNoDataCloseToFloatMax(
                        CPLAtofM(papszTokensInitDest[std::min(
                            i, nTokenCountInitDest - 1)]));
                    int bHasNoData = false;
                    double dfDstNoDataVal = GDALGetRasterNoDataValue(
                        GDALGetRasterBand(hDstDS, i + 1), &bHasNoData);
                    if (!((bHasNoData && dfInitVal == dfDstNoDataVal) ||
                          (!bHasNoData && dfInitVal == 0.0)))
                    {
                        bOKRegardingInitDest = false;
                        break;
                    }
                    if (EQUAL(psOptions->osFormat.c_str(), "MEM") &&
                        bHasNoData && dfDstNoDataVal != 0.0)
                    {
                        bOKRegardingInitDest = false;
                        break;
                    }
                }
            }
            CSLDestroy(papszTokensInitDest);
        }

        if (bOKRegardingInitDest)
        {
            CPLDebug("GDALWARP", "Defining SKIP_NOSOURCE=YES");
            psWO->papszWarpOptions =
                CSLSetNameValue(psWO->papszWarpOptions, "SKIP_NOSOURCE", "YES");
        }
    }
}

/************************************************************************/
/*                     AdjustOutputExtentForRPC()                       */
/************************************************************************/

/** Returns false if there's no intersection between source extent defined
 * by RPC and target extent.
 */
static bool AdjustOutputExtentForRPC(GDALDatasetH hSrcDS, GDALDatasetH hDstDS,
                                     GDALTransformerFunc pfnTransformer,
                                     void *hTransformArg, GDALWarpOptions *psWO,
                                     GDALWarpAppOptions *psOptions,
                                     int &nWarpDstXOff, int &nWarpDstYOff,
                                     int &nWarpDstXSize, int &nWarpDstYSize)
{
    if (CPLTestBool(CSLFetchNameValueDef(psWO->papszWarpOptions,
                                         "SKIP_NOSOURCE", "NO")) &&
        GDALGetMetadata(hSrcDS, "RPC") != nullptr &&
        EQUAL(
            psOptions->aosTransformerOptions.FetchNameValueDef("METHOD", "RPC"),
            "RPC") &&
        CPLTestBool(
            CPLGetConfigOption("RESTRICT_OUTPUT_DATASET_UPDATE", "YES")))
    {
        double adfSuggestedGeoTransform[6];
        double adfExtent[4];
        int nPixels, nLines;
        if (GDALSuggestedWarpOutput2(hSrcDS, pfnTransformer, hTransformArg,
                                     adfSuggestedGeoTransform, &nPixels,
                                     &nLines, adfExtent, 0) == CE_None)
        {
            const double dfMinX = adfExtent[0];
            const double dfMinY = adfExtent[1];
            const double dfMaxX = adfExtent[2];
            const double dfMaxY = adfExtent[3];
            const double dfThreshold = static_cast<double>(INT_MAX) / 2;
            if (std::fabs(dfMinX) < dfThreshold &&
                std::fabs(dfMinY) < dfThreshold &&
                std::fabs(dfMaxX) < dfThreshold &&
                std::fabs(dfMaxY) < dfThreshold)
            {
                const int nPadding = 5;
                nWarpDstXOff =
                    std::max(nWarpDstXOff,
                             static_cast<int>(std::floor(dfMinX)) - nPadding);
                nWarpDstYOff =
                    std::max(nWarpDstYOff,
                             static_cast<int>(std::floor(dfMinY)) - nPadding);
                nWarpDstXSize = std::min(nWarpDstXSize - nWarpDstXOff,
                                         static_cast<int>(std::ceil(dfMaxX)) +
                                             nPadding - nWarpDstXOff);
                nWarpDstYSize = std::min(nWarpDstYSize - nWarpDstYOff,
                                         static_cast<int>(std::ceil(dfMaxY)) +
                                             nPadding - nWarpDstYOff);
                if (nWarpDstXSize <= 0 || nWarpDstYSize <= 0)
                {
                    CPLDebug("WARP",
                             "No intersection between source extent defined "
                             "by RPC and target extent");
                    return false;
                }
                if (nWarpDstXOff != 0 || nWarpDstYOff != 0 ||
                    nWarpDstXSize != GDALGetRasterXSize(hDstDS) ||
                    nWarpDstYSize != GDALGetRasterYSize(hDstDS))
                {
                    CPLDebug("WARP",
                             "Restricting warping to output dataset window "
                             "%d,%d,%dx%d",
                             nWarpDstXOff, nWarpDstYOff, nWarpDstXSize,
                             nWarpDstYSize);
                }
            }
        }
    }
    return true;
}

/************************************************************************/
/*                           GDALWarpDirect()                           */
/************************************************************************/

static GDALDatasetH GDALWarpDirect(const char *pszDest, GDALDatasetH hDstDS,
                                   int nSrcCount, GDALDatasetH *pahSrcDS,
                                   GDALWarpAppOptions *psOptions,
                                   int *pbUsageError)
{
    CPLErrorReset();
    if (pszDest == nullptr && hDstDS == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "pszDest == NULL && hDstDS == NULL");

        if (pbUsageError)
            *pbUsageError = TRUE;
        return nullptr;
    }
    if (pszDest == nullptr)
        pszDest = GDALGetDescription(hDstDS);

#ifdef DEBUG
    GDALDataset *poDstDS = GDALDataset::FromHandle(hDstDS);
    const int nExpectedRefCountAtEnd =
        (poDstDS != nullptr) ? poDstDS->GetRefCount() : 1;
    (void)nExpectedRefCountAtEnd;
#endif
    const bool bDropDstDSRef = (hDstDS != nullptr);
    if (hDstDS != nullptr)
        GDALReferenceDataset(hDstDS);

#if defined(USE_PROJ_BASED_VERTICAL_SHIFT_METHOD)
    if (psOptions->bNoVShift)
    {
        psOptions->aosTransformerOptions.SetNameValue("STRIP_VERT_CS", "YES");
    }
    else if (nSrcCount)
    {
        bool bSrcHasVertAxis = false;
        bool bDstHasVertAxis = false;
        OGRSpatialReference oSRSSrc;
        OGRSpatialReference oSRSDst;

        if (MustApplyVerticalShift(pahSrcDS[0], psOptions, oSRSSrc, oSRSDst,
                                   bSrcHasVertAxis, bDstHasVertAxis))
        {
            psOptions->aosTransformerOptions.SetNameValue("PROMOTE_TO_3D",
                                                          "YES");
        }
    }
#else
    psOptions->aosTransformerOptions.SetNameValue("STRIP_VERT_CS", "YES");
#endif

    bool bVRT = false;
    if (!CheckOptions(pszDest, hDstDS, nSrcCount, pahSrcDS, psOptions, bVRT,
                      pbUsageError))
    {
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      If we have a cutline datasource read it and attach it in the    */
    /*      warp options.                                                   */
    /* -------------------------------------------------------------------- */
    OGRGeometryH hCutline = nullptr;
    if (!ProcessCutlineOptions(nSrcCount, pahSrcDS, psOptions, hCutline))
    {
        OGR_G_DestroyGeometry(hCutline);
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      If the target dataset does not exist, we need to create it.     */
    /* -------------------------------------------------------------------- */
    void *hUniqueTransformArg = nullptr;
    const bool bInitDestSetByUser =
        (psOptions->aosWarpOptions.FetchNameValue("INIT_DEST") != nullptr);

    const bool bFigureoutCorrespondingWindow =
        (hDstDS != nullptr) ||
        (((psOptions->nForcePixels != 0 && psOptions->nForceLines != 0) ||
          (psOptions->dfXRes != 0 && psOptions->dfYRes != 0)) &&
         !(psOptions->dfMinX == 0.0 && psOptions->dfMinY == 0.0 &&
           psOptions->dfMaxX == 0.0 && psOptions->dfMaxY == 0.0));

    const char *pszMethod =
        psOptions->aosTransformerOptions.FetchNameValue("METHOD");
    if (pszMethod && EQUAL(pszMethod, "GCP_TPS") &&
        psOptions->dfErrorThreshold > 0 &&
        !psOptions->aosTransformerOptions.FetchNameValue(
            "SRC_APPROX_ERROR_IN_PIXEL"))
    {
        psOptions->aosTransformerOptions.SetNameValue(
            "SRC_APPROX_ERROR_IN_PIXEL",
            CPLSPrintf("%g", psOptions->dfErrorThreshold));
    }

    if (hDstDS == nullptr)
    {
        hDstDS = CreateOutput(pszDest, nSrcCount, pahSrcDS, psOptions,
                              bInitDestSetByUser, hUniqueTransformArg);
        if (!hDstDS)
        {
            GDALDestroyTransformer(hUniqueTransformArg);
            OGR_G_DestroyGeometry(hCutline);
            return nullptr;
        }
#ifdef DEBUG
        // Do not remove this if the #ifdef DEBUG before is still there !
        poDstDS = GDALDataset::FromHandle(hDstDS);
        CPL_IGNORE_RET_VAL(poDstDS);
#endif
    }
    else
    {
        if (psOptions->aosWarpOptions.FetchNameValue("SKIP_NOSOURCE") ==
            nullptr)
        {
            CPLDebug("GDALWARP", "Defining SKIP_NOSOURCE=YES");
            psOptions->aosWarpOptions.SetNameValue("SKIP_NOSOURCE", "YES");
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Detect if output has alpha channel.                             */
    /* -------------------------------------------------------------------- */
    bool bEnableDstAlpha = psOptions->bEnableDstAlpha;
    if (!bEnableDstAlpha && GDALGetRasterCount(hDstDS) &&
        GDALGetRasterColorInterpretation(GDALGetRasterBand(
            hDstDS, GDALGetRasterCount(hDstDS))) == GCI_AlphaBand &&
        !psOptions->bDisableSrcAlpha)
    {
        if (!psOptions->bQuiet)
            printf("Using band %d of destination image as alpha.\n",
                   GDALGetRasterCount(hDstDS));

        bEnableDstAlpha = true;
    }

    /* -------------------------------------------------------------------- */
    /*      Create global progress function.                                */
    /* -------------------------------------------------------------------- */
    struct Progress
    {
        GDALProgressFunc pfnExternalProgress;
        void *pExternalProgressData;
        int iSrc;
        int nSrcCount;
        GDALDatasetH *pahSrcDS;

        int Do(double dfComplete)
        {
            CPLString osMsg;
            osMsg.Printf("Processing %s [%d/%d]",
                         GDALGetDescription(pahSrcDS[iSrc]), iSrc + 1,
                         nSrcCount);
            return pfnExternalProgress((iSrc + dfComplete) / nSrcCount,
                                       osMsg.c_str(), pExternalProgressData);
        }

        static int CPL_STDCALL ProgressFunc(double dfComplete, const char *,
                                            void *pThis)
        {
            return static_cast<Progress *>(pThis)->Do(dfComplete);
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
    GDALTransformerFunc pfnTransformer = nullptr;
    void *hTransformArg = nullptr;
    bool bHasGotErr = false;
    for (int iSrc = 0; iSrc < nSrcCount; iSrc++)
    {
        GDALDatasetH hSrcDS;

        /* --------------------------------------------------------------------
         */
        /*      Open this file. */
        /* --------------------------------------------------------------------
         */
        hSrcDS = pahSrcDS[iSrc];
        oProgress.iSrc = iSrc;

        /* --------------------------------------------------------------------
         */
        /*      Check that there's at least one raster band */
        /* --------------------------------------------------------------------
         */
        if (GDALGetRasterCount(hSrcDS) == 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Input file %s has no raster bands.",
                     GDALGetDescription(hSrcDS));
            OGR_G_DestroyGeometry(hCutline);
            GDALReleaseDataset(hDstDS);
            return nullptr;
        }

        /* --------------------------------------------------------------------
         */
        /*      Do we have a source alpha band? */
        /* --------------------------------------------------------------------
         */
        bool bEnableSrcAlpha = psOptions->bEnableSrcAlpha;
        if (GDALGetRasterColorInterpretation(GDALGetRasterBand(
                hSrcDS, GDALGetRasterCount(hSrcDS))) == GCI_AlphaBand &&
            !bEnableSrcAlpha && !psOptions->bDisableSrcAlpha)
        {
            bEnableSrcAlpha = true;
            if (!psOptions->bQuiet)
                printf("Using band %d of source image as alpha.\n",
                       GDALGetRasterCount(hSrcDS));
        }

        /* --------------------------------------------------------------------
         */
        /*      Get the metadata of the first source DS and copy it to the */
        /*      destination DS. Copy Band-level metadata and other info, only */
        /*      if source and destination band count are equal. Any values that
         */
        /*      conflict between source datasets are set to pszMDConflictValue.
         */
        /* --------------------------------------------------------------------
         */
        ProcessMetadata(iSrc, hSrcDS, hDstDS, psOptions, bEnableDstAlpha);

        /* --------------------------------------------------------------------
         */
        /*      Warns if the file has a color table and something more */
        /*      complicated than nearest neighbour resampling is asked */
        /* --------------------------------------------------------------------
         */

        if (psOptions->eResampleAlg != GRA_NearestNeighbour &&
            psOptions->eResampleAlg != GRA_Mode &&
            GDALGetRasterColorTable(GDALGetRasterBand(hSrcDS, 1)) != nullptr)
        {
            if (!psOptions->bQuiet)
                CPLError(
                    CE_Warning, CPLE_AppDefined,
                    "Input file %s has a color table, which will likely lead "
                    "to "
                    "bad results when using a resampling method other than "
                    "nearest neighbour or mode. Converting the dataset prior "
                    "to 24/32 bit "
                    "is advised.",
                    GDALGetDescription(hSrcDS));
        }

        /* --------------------------------------------------------------------
         */
        /*      For RPC warping add a few extra source pixels by default */
        /*      (probably mostly needed in the RPC DEM case) */
        /* --------------------------------------------------------------------
         */

        if (iSrc == 0 && (GDALGetMetadata(hSrcDS, "RPC") != nullptr &&
                          (pszMethod == nullptr || EQUAL(pszMethod, "RPC"))))
        {
            if (!psOptions->aosWarpOptions.FetchNameValue("SOURCE_EXTRA"))
            {
                CPLDebug(
                    "WARP",
                    "Set SOURCE_EXTRA=5 warping options due to RPC warping");
                psOptions->aosWarpOptions.SetNameValue("SOURCE_EXTRA", "5");
            }

            if (!psOptions->aosWarpOptions.FetchNameValue("SAMPLE_STEPS") &&
                !psOptions->aosWarpOptions.FetchNameValue("SAMPLE_GRID") &&
                psOptions->aosTransformerOptions.FetchNameValue("RPC_DEM"))
            {
                CPLDebug("WARP", "Set SAMPLE_STEPS=ALL warping options due to "
                                 "RPC DEM warping");
                psOptions->aosWarpOptions.SetNameValue("SAMPLE_STEPS", "ALL");
            }
        }

        /* --------------------------------------------------------------------
         */
        /*      Create a transformation object from the source to */
        /*      destination coordinate system. */
        /* --------------------------------------------------------------------
         */
        if (hUniqueTransformArg)
            hTransformArg = hUniqueTransformArg;
        else
            hTransformArg = GDALCreateGenImgProjTransformer2(
                hSrcDS, hDstDS, psOptions->aosTransformerOptions.List());

        if (hTransformArg == nullptr)
        {
            OGR_G_DestroyGeometry(hCutline);
            GDALReleaseDataset(hDstDS);
            return nullptr;
        }

        pfnTransformer = GDALGenImgProjTransform;

        // Check if transformation is inversible
        {
            double dfX = GDALGetRasterXSize(hDstDS) / 2.0;
            double dfY = GDALGetRasterYSize(hDstDS) / 2.0;
            double dfZ = 0;
            int bSuccess = false;
            const auto nErrorCounterBefore = CPLGetErrorCounter();
            pfnTransformer(hTransformArg, TRUE, 1, &dfX, &dfY, &dfZ, &bSuccess);
            if (!bSuccess && CPLGetErrorCounter() > nErrorCounterBefore &&
                strstr(CPLGetLastErrorMsg(), "No inverse operation"))
            {
                GDALDestroyTransformer(hTransformArg);
                OGR_G_DestroyGeometry(hCutline);
                GDALReleaseDataset(hDstDS);
                return nullptr;
            }
        }

        /* --------------------------------------------------------------------
         */
        /*      Determine if we must work with the full-resolution source */
        /*      dataset, or one of its overview level. */
        /* --------------------------------------------------------------------
         */
        GDALDataset *poSrcDS = static_cast<GDALDataset *>(hSrcDS);
        GDALDataset *poSrcOvrDS = nullptr;
        int nOvCount = poSrcDS->GetRasterBand(1)->GetOverviewCount();
        if (psOptions->nOvLevel <= OVR_LEVEL_AUTO && nOvCount > 0)
        {
            double dfTargetRatio = 0;
            double dfTargetRatioX = 0;
            double dfTargetRatioY = 0;

            if (bFigureoutCorrespondingWindow)
            {
                // If the user has explicitly set the target bounds and
                // resolution, or we're updating an existing file, then figure
                // out which source window corresponds to the target raster.
                constexpr int nPointsOneDim = 10;
                constexpr int nPoints = nPointsOneDim * nPointsOneDim;
                std::vector<double> adfX(nPoints);
                std::vector<double> adfY(nPoints);
                std::vector<double> adfZ(nPoints);
                const int nDstXSize = GDALGetRasterXSize(hDstDS);
                const int nDstYSize = GDALGetRasterYSize(hDstDS);
                int iPoint = 0;
                for (int iX = 0; iX < nPointsOneDim; ++iX)
                {
                    for (int iY = 0; iY < nPointsOneDim; ++iY)
                    {
                        adfX[iPoint] = nDstXSize * static_cast<double>(iX) /
                                       (nPointsOneDim - 1);
                        adfY[iPoint] = nDstYSize * static_cast<double>(iY) /
                                       (nPointsOneDim - 1);
                        iPoint++;
                    }
                }
                std::vector<int> abSuccess(nPoints);
                if (pfnTransformer(hTransformArg, TRUE, nPoints, &adfX[0],
                                   &adfY[0], &adfZ[0], &abSuccess[0]))
                {
                    double dfMinSrcX = std::numeric_limits<double>::infinity();
                    double dfMaxSrcX = -std::numeric_limits<double>::infinity();
                    double dfMinSrcY = std::numeric_limits<double>::infinity();
                    double dfMaxSrcY = -std::numeric_limits<double>::infinity();
                    for (int i = 0; i < nPoints; i++)
                    {
                        if (abSuccess[i])
                        {
                            dfMinSrcX = std::min(dfMinSrcX, adfX[i]);
                            dfMaxSrcX = std::max(dfMaxSrcX, adfX[i]);
                            dfMinSrcY = std::min(dfMinSrcY, adfY[i]);
                            dfMaxSrcY = std::max(dfMaxSrcY, adfY[i]);
                        }
                    }
                    if (dfMaxSrcX > dfMinSrcX)
                    {
                        dfTargetRatioX = (dfMaxSrcX - dfMinSrcX) /
                                         GDALGetRasterXSize(hDstDS);
                    }
                    if (dfMaxSrcY > dfMinSrcY)
                    {
                        dfTargetRatioY = (dfMaxSrcY - dfMinSrcY) /
                                         GDALGetRasterYSize(hDstDS);
                    }
                    // take the minimum of these ratios #7019
                    dfTargetRatio = std::min(dfTargetRatioX, dfTargetRatioY);
                }
            }
            else
            {
                /* Compute what the "natural" output resolution (in pixels)
                 * would be for this */
                /* input dataset */
                double adfSuggestedGeoTransform[6];
                int nPixels, nLines;
                if (GDALSuggestedWarpOutput(
                        hSrcDS, pfnTransformer, hTransformArg,
                        adfSuggestedGeoTransform, &nPixels, &nLines) == CE_None)
                {
                    dfTargetRatio = 1.0 / adfSuggestedGeoTransform[1];
                }
            }

            if (dfTargetRatio > 1.0)
            {
                int iOvr = -1;
                for (; iOvr < nOvCount - 1; iOvr++)
                {
                    const double dfOvrRatio =
                        iOvr < 0
                            ? 1.0
                            : static_cast<double>(poSrcDS->GetRasterXSize()) /
                                  poSrcDS->GetRasterBand(1)
                                      ->GetOverview(iOvr)
                                      ->GetXSize();
                    const double dfNextOvrRatio =
                        static_cast<double>(poSrcDS->GetRasterXSize()) /
                        poSrcDS->GetRasterBand(1)
                            ->GetOverview(iOvr + 1)
                            ->GetXSize();
                    if (dfOvrRatio < dfTargetRatio &&
                        dfNextOvrRatio > dfTargetRatio)
                        break;
                    if (fabs(dfOvrRatio - dfTargetRatio) < 1e-1)
                        break;
                }
                iOvr += (psOptions->nOvLevel - OVR_LEVEL_AUTO);
                if (iOvr >= 0)
                {
                    CPLDebug("WARP", "Selecting overview level %d for %s", iOvr,
                             GDALGetDescription(hSrcDS));
                    poSrcOvrDS =
                        GDALCreateOverviewDataset(poSrcDS, iOvr,
                                                  /* bThisLevelOnly = */ false);
                }
            }
        }
        else if (psOptions->nOvLevel >= 0)
        {
            poSrcOvrDS = GDALCreateOverviewDataset(poSrcDS, psOptions->nOvLevel,
                                                   /* bThisLevelOnly = */ true);
            if (poSrcOvrDS == nullptr)
            {
                if (!psOptions->bQuiet)
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "cannot get overview level %d for "
                             "dataset %s. Defaulting to level %d",
                             psOptions->nOvLevel, GDALGetDescription(hSrcDS),
                             nOvCount - 1);
                }
                if (nOvCount > 0)
                    poSrcOvrDS =
                        GDALCreateOverviewDataset(poSrcDS, nOvCount - 1,
                                                  /* bThisLevelOnly = */ false);
            }
            else
            {
                CPLDebug("WARP", "Selecting overview level %d for %s",
                         psOptions->nOvLevel, GDALGetDescription(hSrcDS));
            }
        }

        if (poSrcOvrDS == nullptr)
            GDALReferenceDataset(hSrcDS);

        GDALDatasetH hWrkSrcDS =
            poSrcOvrDS ? static_cast<GDALDatasetH>(poSrcOvrDS) : hSrcDS;

#if !defined(USE_PROJ_BASED_VERTICAL_SHIFT_METHOD)
        if (!psOptions->bNoVShift)
        {
            bool bErrorOccurred = false;
            hWrkSrcDS = ApplyVerticalShiftGrid(
                hWrkSrcDS, psOptions, bVRT ? hDstDS : nullptr, bErrorOccurred);
            if (bErrorOccurred)
            {
                GDALDestroyTransformer(hTransformArg);
                OGR_G_DestroyGeometry(hCutline);
                GDALReleaseDataset(hWrkSrcDS);
                GDALReleaseDataset(hDstDS);
                return nullptr;
            }
        }
#endif

        /* --------------------------------------------------------------------
         */
        /*      Clear temporary INIT_DEST settings after the first image. */
        /* --------------------------------------------------------------------
         */
        if (psOptions->bCreateOutput && iSrc == 1)
            psOptions->aosWarpOptions.SetNameValue("INIT_DEST", nullptr);

        /* --------------------------------------------------------------------
         */
        /*      Define SKIP_NOSOURCE after the first image (since
         * initialization*/
        /*      has already be done). */
        /* --------------------------------------------------------------------
         */
        if (iSrc == 1 && psOptions->aosWarpOptions.FetchNameValue(
                             "SKIP_NOSOURCE") == nullptr)
        {
            CPLDebug("GDALWARP", "Defining SKIP_NOSOURCE=YES");
            psOptions->aosWarpOptions.SetNameValue("SKIP_NOSOURCE", "YES");
        }

        /* --------------------------------------------------------------------
         */
        /*      Setup warp options. */
        /* --------------------------------------------------------------------
         */
        GDALWarpOptions *psWO = GDALCreateWarpOptions();

        psWO->papszWarpOptions = CSLDuplicate(psOptions->aosWarpOptions.List());
        psWO->eWorkingDataType = psOptions->eWorkingType;

        psWO->eResampleAlg = psOptions->eResampleAlg;

        psWO->hSrcDS = hWrkSrcDS;
        psWO->hDstDS = hDstDS;

        if (!bVRT)
        {
            if (psOptions->pfnProgress == GDALDummyProgress)
            {
                psWO->pfnProgress = GDALDummyProgress;
                psWO->pProgressArg = nullptr;
            }
            else
            {
                psWO->pfnProgress = Progress::ProgressFunc;
                psWO->pProgressArg = &oProgress;
            }
        }

        if (psOptions->dfWarpMemoryLimit != 0.0)
            psWO->dfWarpMemoryLimit = psOptions->dfWarpMemoryLimit;

        /* --------------------------------------------------------------------
         */
        /*      Setup band mapping. */
        /* --------------------------------------------------------------------
         */
        if (psOptions->anSrcBands.empty())
        {
            if (bEnableSrcAlpha)
                psWO->nBandCount = GDALGetRasterCount(hWrkSrcDS) - 1;
            else
                psWO->nBandCount = GDALGetRasterCount(hWrkSrcDS);
        }
        else
        {
            psWO->nBandCount = static_cast<int>(psOptions->anSrcBands.size());
        }

        const int nNeededDstBands =
            psWO->nBandCount + (bEnableDstAlpha ? 1 : 0);
        if (nNeededDstBands > GDALGetRasterCount(hDstDS))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Destination dataset has %d bands, but at least %d "
                     "are needed",
                     GDALGetRasterCount(hDstDS), nNeededDstBands);
            GDALDestroyTransformer(hTransformArg);
            GDALDestroyWarpOptions(psWO);
            OGR_G_DestroyGeometry(hCutline);
            GDALReleaseDataset(hWrkSrcDS);
            GDALReleaseDataset(hDstDS);
            return nullptr;
        }

        psWO->panSrcBands =
            static_cast<int *>(CPLMalloc(psWO->nBandCount * sizeof(int)));
        psWO->panDstBands =
            static_cast<int *>(CPLMalloc(psWO->nBandCount * sizeof(int)));
        if (psOptions->anSrcBands.empty())
        {
            for (int i = 0; i < psWO->nBandCount; i++)
            {
                psWO->panSrcBands[i] = i + 1;
                psWO->panDstBands[i] = i + 1;
            }
        }
        else
        {
            for (int i = 0; i < psWO->nBandCount; i++)
            {
                if (psOptions->anSrcBands[i] <= 0 ||
                    psOptions->anSrcBands[i] > GDALGetRasterCount(hSrcDS))
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "-srcband[%d] = %d is invalid", i,
                             psOptions->anSrcBands[i]);
                    GDALDestroyTransformer(hTransformArg);
                    GDALDestroyWarpOptions(psWO);
                    OGR_G_DestroyGeometry(hCutline);
                    GDALReleaseDataset(hWrkSrcDS);
                    GDALReleaseDataset(hDstDS);
                    return nullptr;
                }
                if (psOptions->anDstBands[i] <= 0 ||
                    psOptions->anDstBands[i] > GDALGetRasterCount(hDstDS))
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "-dstband[%d] = %d is invalid", i,
                             psOptions->anDstBands[i]);
                    GDALDestroyTransformer(hTransformArg);
                    GDALDestroyWarpOptions(psWO);
                    OGR_G_DestroyGeometry(hCutline);
                    GDALReleaseDataset(hWrkSrcDS);
                    GDALReleaseDataset(hDstDS);
                    return nullptr;
                }
                psWO->panSrcBands[i] = psOptions->anSrcBands[i];
                psWO->panDstBands[i] = psOptions->anDstBands[i];
            }
        }

        /* --------------------------------------------------------------------
         */
        /*      Setup alpha bands used if any. */
        /* --------------------------------------------------------------------
         */
        if (bEnableSrcAlpha)
            psWO->nSrcAlphaBand = GDALGetRasterCount(hWrkSrcDS);

        if (bEnableDstAlpha)
        {
            if (psOptions->anSrcBands.empty())
                psWO->nDstAlphaBand = GDALGetRasterCount(hDstDS);
            else
                psWO->nDstAlphaBand =
                    static_cast<int>(psOptions->anDstBands.size()) + 1;
        }

        /* --------------------------------------------------------------------
         */
        /*      Setup NODATA options. */
        /* --------------------------------------------------------------------
         */
        SetupNoData(pszDest, iSrc, hSrcDS, hWrkSrcDS, hDstDS, psWO, psOptions,
                    bEnableDstAlpha, bInitDestSetByUser);

        oProgress.Do(0);

        /* --------------------------------------------------------------------
         */
        /*      For the first source image of a newly created dataset, decide */
        /*      if we can safely enable SKIP_NOSOURCE optimization. */
        /* --------------------------------------------------------------------
         */
        SetupSkipNoSource(iSrc, hDstDS, psWO, psOptions);

        /* --------------------------------------------------------------------
         */
        /*      In some cases, RPC evaluation can find valid input pixel for */
        /*      output pixels that are outside the footprint of the source */
        /*      dataset, so limit the area we update in the target dataset from
         */
        /*      the suggested warp output (only in cases where
         * SKIP_NOSOURCE=YES) */
        /* --------------------------------------------------------------------
         */
        int nWarpDstXOff = 0;
        int nWarpDstYOff = 0;
        int nWarpDstXSize = GDALGetRasterXSize(hDstDS);
        int nWarpDstYSize = GDALGetRasterYSize(hDstDS);

        if (!AdjustOutputExtentForRPC(
                hSrcDS, hDstDS, pfnTransformer, hTransformArg, psWO, psOptions,
                nWarpDstXOff, nWarpDstYOff, nWarpDstXSize, nWarpDstYSize))
        {
            GDALDestroyTransformer(hTransformArg);
            GDALDestroyWarpOptions(psWO);
            GDALReleaseDataset(hWrkSrcDS);
            continue;
        }

        /* We need to recreate the transform when operating on an overview */
        if (poSrcOvrDS != nullptr)
        {
            GDALDestroyGenImgProjTransformer(hTransformArg);
            hTransformArg = GDALCreateGenImgProjTransformer2(
                hWrkSrcDS, hDstDS, psOptions->aosTransformerOptions.List());
        }

        bool bUseApproxTransformer = psOptions->dfErrorThreshold != 0.0;
#ifdef USE_PROJ_BASED_VERTICAL_SHIFT_METHOD
        if (!psOptions->bNoVShift)
        {
            // Can modify psWO->papszWarpOptions
            if (ApplyVerticalShift(hWrkSrcDS, psOptions, psWO))
            {
                bUseApproxTransformer = false;
            }
        }
#endif

        /* --------------------------------------------------------------------
         */
        /*      Warp the transformer with a linear approximator unless the */
        /*      acceptable error is zero. */
        /* --------------------------------------------------------------------
         */
        if (bUseApproxTransformer)
        {
            hTransformArg = GDALCreateApproxTransformer(
                GDALGenImgProjTransform, hTransformArg,
                psOptions->dfErrorThreshold);
            pfnTransformer = GDALApproxTransform;
            GDALApproxTransformerOwnsSubtransformer(hTransformArg, TRUE);
        }

        psWO->pfnTransformer = pfnTransformer;
        psWO->pTransformerArg = hTransformArg;

        /* --------------------------------------------------------------------
         */
        /*      If we have a cutline, transform it into the source */
        /*      pixel/line coordinate system and insert into warp options. */
        /* --------------------------------------------------------------------
         */
        if (hCutline != nullptr)
        {
            CPLErr eError;
            eError = TransformCutlineToSource(
                GDALDataset::FromHandle(hWrkSrcDS),
                OGRGeometry::FromHandle(hCutline), &(psWO->papszWarpOptions),
                psOptions->aosTransformerOptions.List());
            if (eError == CE_Failure)
            {
                GDALDestroyTransformer(hTransformArg);
                GDALDestroyWarpOptions(psWO);
                OGR_G_DestroyGeometry(hCutline);
                GDALReleaseDataset(hWrkSrcDS);
                GDALReleaseDataset(hDstDS);
                return nullptr;
            }
        }

        /* --------------------------------------------------------------------
         */
        /*      If we are producing VRT output, then just initialize it with */
        /*      the warp options and write out now rather than proceeding */
        /*      with the operations. */
        /* --------------------------------------------------------------------
         */
        if (bVRT)
        {
            GDALSetMetadataItem(hDstDS, "SrcOvrLevel",
                                CPLSPrintf("%d", psOptions->nOvLevel), nullptr);
            CPLErr eErr = GDALInitializeWarpedVRT(hDstDS, psWO);
            GDALDestroyWarpOptions(psWO);
            OGR_G_DestroyGeometry(hCutline);
            GDALReleaseDataset(hWrkSrcDS);
            if (eErr != CE_None)
            {
                GDALDestroyTransformer(hTransformArg);
                GDALReleaseDataset(hDstDS);
                return nullptr;
            }
            // In case of success, hDstDS has become the owner of hTransformArg
            // so do not free it.
            if (!EQUAL(pszDest, ""))
            {
                const bool bWasFailureBefore =
                    (CPLGetLastErrorType() == CE_Failure);
                GDALFlushCache(hDstDS);
                if (!bWasFailureBefore && CPLGetLastErrorType() == CE_Failure)
                {
                    GDALReleaseDataset(hDstDS);
                    hDstDS = nullptr;
                }
            }

            if (hDstDS)
                oProgress.Do(1);

            return hDstDS;
        }

        /* --------------------------------------------------------------------
         */
        /*      Initialize and execute the warp. */
        /* --------------------------------------------------------------------
         */
        GDALWarpOperation oWO;

        if (oWO.Initialize(psWO) == CE_None)
        {
            CPLErr eErr;
            if (psOptions->bMulti)
                eErr = oWO.ChunkAndWarpMulti(nWarpDstXOff, nWarpDstYOff,
                                             nWarpDstXSize, nWarpDstYSize);
            else
                eErr = oWO.ChunkAndWarpImage(nWarpDstXOff, nWarpDstYOff,
                                             nWarpDstXSize, nWarpDstYSize);
            if (eErr != CE_None)
                bHasGotErr = true;
        }
        else
        {
            bHasGotErr = true;
        }

        /* --------------------------------------------------------------------
         */
        /*      Cleanup */
        /* --------------------------------------------------------------------
         */
        GDALDestroyTransformer(hTransformArg);

        GDALDestroyWarpOptions(psWO);

        GDALReleaseDataset(hWrkSrcDS);
    }

    /* -------------------------------------------------------------------- */
    /*      Final Cleanup.                                                  */
    /* -------------------------------------------------------------------- */
    const bool bWasFailureBefore = (CPLGetLastErrorType() == CE_Failure);
    GDALFlushCache(hDstDS);
    if (!bWasFailureBefore && CPLGetLastErrorType() == CE_Failure)
    {
        bHasGotErr = true;
    }

    OGR_G_DestroyGeometry(hCutline);

    if (bHasGotErr || bDropDstDSRef)
        GDALReleaseDataset(hDstDS);

#ifdef DEBUG
    if (!bHasGotErr || bDropDstDSRef)
    {
        CPLAssert(poDstDS->GetRefCount() == nExpectedRefCountAtEnd);
    }
#endif

    return bHasGotErr ? nullptr : hDstDS;
}

/************************************************************************/
/*                          ValidateCutline()                           */
/*  Same as OGR_G_IsValid() except that it processes polygon per polygon*/
/*  without paying attention to MultiPolygon specific validity rules.   */
/************************************************************************/

static bool ValidateCutline(const OGRGeometry *poGeom, bool bVerbose)
{
    const OGRwkbGeometryType eType = wkbFlatten(poGeom->getGeometryType());
    if (eType == wkbMultiPolygon)
    {
        for (const auto *poSubGeom : *(poGeom->toMultiPolygon()))
        {
            if (!ValidateCutline(poSubGeom, bVerbose))
                return false;
        }
    }
    else if (eType == wkbPolygon)
    {
        if (OGRGeometryFactory::haveGEOS() && !poGeom->IsValid())
        {
            if (!bVerbose)
                return false;

            char *pszWKT = nullptr;
            poGeom->exportToWkt(&pszWKT);
            CPLDebug("GDALWARP", "WKT = \"%s\"", pszWKT ? pszWKT : "(null)");
            const char *pszFile =
                CPLGetConfigOption("GDALWARP_DUMP_WKT_TO_FILE", nullptr);
            if (pszFile && pszWKT)
            {
                FILE *f =
                    EQUAL(pszFile, "stderr") ? stderr : fopen(pszFile, "wb");
                if (f)
                {
                    fprintf(f, "id,WKT\n");
                    fprintf(f, "1,\"%s\"\n", pszWKT);
                    if (!EQUAL(pszFile, "stderr"))
                        fclose(f);
                }
            }
            CPLFree(pszWKT);

            if (CPLTestBool(
                    CPLGetConfigOption("GDALWARP_IGNORE_BAD_CUTLINE", "NO")))
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Cutline polygon is invalid.");
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cutline polygon is invalid.");
                return false;
            }
        }
    }
    else
    {
        if (bVerbose)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cutline not of polygon type.");
        }
        return false;
    }

    return true;
}

/************************************************************************/
/*                            LoadCutline()                             */
/*                                                                      */
/*      Load blend cutline from OGR datasource.                         */
/************************************************************************/

static CPLErr LoadCutline(const std::string &osCutlineDSNameOrWKT,
                          const std::string &osSRS, const std::string &osCLayer,
                          const std::string &osCWHERE,
                          const std::string &osCSQL, OGRGeometryH *phCutlineRet)

{
    if (STARTS_WITH_CI(osCutlineDSNameOrWKT.c_str(), "POLYGON(") ||
        STARTS_WITH_CI(osCutlineDSNameOrWKT.c_str(), "POLYGON (") ||
        STARTS_WITH_CI(osCutlineDSNameOrWKT.c_str(), "MULTIPOLYGON(") ||
        STARTS_WITH_CI(osCutlineDSNameOrWKT.c_str(), "MULTIPOLYGON ("))
    {
        std::unique_ptr<OGRSpatialReference, OGRSpatialReferenceReleaser> poSRS;
        if (!osSRS.empty())
        {
            poSRS.reset(new OGRSpatialReference());
            poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
            poSRS->SetFromUserInput(osSRS.c_str());
        }
        OGRGeometry *poGeom = nullptr;
        OGRGeometryFactory::createFromWkt(osCutlineDSNameOrWKT.c_str(),
                                          poSRS.get(), &poGeom);
        *phCutlineRet = OGRGeometry::ToHandle(poGeom);
        return *phCutlineRet ? CE_None : CE_Failure;
    }

    /* -------------------------------------------------------------------- */
    /*      Open source vector dataset.                                     */
    /* -------------------------------------------------------------------- */
    auto poDS = std::unique_ptr<GDALDataset>(
        GDALDataset::Open(osCutlineDSNameOrWKT.c_str(), GDAL_OF_VECTOR));
    if (poDS == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot open %s.",
                 osCutlineDSNameOrWKT.c_str());
        return CE_Failure;
    }

    /* -------------------------------------------------------------------- */
    /*      Get the source layer                                            */
    /* -------------------------------------------------------------------- */
    OGRLayer *poLayer = nullptr;

    if (!osCSQL.empty())
        poLayer = poDS->ExecuteSQL(osCSQL.c_str(), nullptr, nullptr);
    else if (!osCLayer.empty())
        poLayer = poDS->GetLayerByName(osCLayer.c_str());
    else
        poLayer = poDS->GetLayer(0);

    if (poLayer == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Failed to identify source layer from datasource.");
        return CE_Failure;
    }

    /* -------------------------------------------------------------------- */
    /*      Apply WHERE clause if there is one.                             */
    /* -------------------------------------------------------------------- */
    if (!osCWHERE.empty())
        poLayer->SetAttributeFilter(osCWHERE.c_str());

    /* -------------------------------------------------------------------- */
    /*      Collect the geometries from this layer, and build list of       */
    /*      burn values.                                                    */
    /* -------------------------------------------------------------------- */
    auto poMultiPolygon = std::make_unique<OGRMultiPolygon>();

    for (auto &&poFeature : poLayer)
    {
        auto poGeom = std::unique_ptr<OGRGeometry>(poFeature->StealGeometry());
        if (poGeom == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cutline feature without a geometry.");
            goto error;
        }

        if (!ValidateCutline(poGeom.get(), true))
        {
            goto error;
        }

        OGRwkbGeometryType eType = wkbFlatten(poGeom->getGeometryType());

        if (eType == wkbPolygon)
            poMultiPolygon->addGeometryDirectly(poGeom.release());
        else if (eType == wkbMultiPolygon)
        {
            for (const auto *poSubGeom : poGeom->toMultiPolygon())
            {
                poMultiPolygon->addGeometry(poSubGeom);
            }
        }
    }

    if (poMultiPolygon->IsEmpty())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Did not get any cutline features.");
        goto error;
    }

    /* -------------------------------------------------------------------- */
    /*      Ensure the coordinate system gets set on the geometry.          */
    /* -------------------------------------------------------------------- */
    if (!osSRS.empty())
    {
        std::unique_ptr<OGRSpatialReference, OGRSpatialReferenceReleaser> poSRS(
            new OGRSpatialReference());
        poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        poSRS->SetFromUserInput(osSRS.c_str());
        poMultiPolygon->assignSpatialReference(poSRS.get());
    }
    else
    {
        poMultiPolygon->assignSpatialReference(poLayer->GetSpatialRef());
    }

    *phCutlineRet = OGRGeometry::ToHandle(poMultiPolygon.release());

    /* -------------------------------------------------------------------- */
    /*      Cleanup                                                         */
    /* -------------------------------------------------------------------- */
    if (!osCSQL.empty())
        poDS->ReleaseResultSet(poLayer);

    return CE_None;

error:
    if (!osCSQL.empty())
        poDS->ReleaseResultSet(poLayer);

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

static GDALDatasetH GDALWarpCreateOutput(
    int nSrcCount, GDALDatasetH *pahSrcDS, const char *pszFilename,
    const char *pszFormat, char **papszTO, CSLConstList papszCreateOptions,
    GDALDataType eDT, void **phTransformArg, bool bSetColorInterpretation,
    GDALWarpAppOptions *psOptions)

{
    GDALDriverH hDriver;
    GDALDatasetH hDstDS;
    void *hTransformArg;
    GDALColorTableH hCT = nullptr;
    GDALRasterAttributeTableH hRAT = nullptr;
    double dfWrkMinX = 0, dfWrkMaxX = 0, dfWrkMinY = 0, dfWrkMaxY = 0;
    double dfWrkResX = 0, dfWrkResY = 0;
    int nDstBandCount = 0;
    std::vector<GDALColorInterp> apeColorInterpretations;
    bool bVRT = false;

    if (EQUAL(pszFormat, "VRT"))
        bVRT = true;

    // Special case for geographic to Mercator (typically EPSG:4326 to EPSG:3857)
    // where latitudes close to 90 go to infinity
    // We clamp latitudes between ~ -85 and ~ 85 degrees.
    const char *pszDstSRS = CSLFetchNameValue(papszTO, "DST_SRS");
    if (nSrcCount == 1 && pszDstSRS && psOptions->dfMinX == 0.0 &&
        psOptions->dfMinY == 0.0 && psOptions->dfMaxX == 0.0 &&
        psOptions->dfMaxY == 0.0)
    {
        auto hSrcDS = pahSrcDS[0];
        const auto osSrcSRS = GetSrcDSProjection(pahSrcDS[0], papszTO);
        OGRSpatialReference oSrcSRS;
        oSrcSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        oSrcSRS.SetFromUserInput(osSrcSRS.c_str());
        OGRSpatialReference oDstSRS;
        oDstSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        oDstSRS.SetFromUserInput(pszDstSRS);
        const char *pszProjection = oDstSRS.GetAttrValue("PROJECTION");
        const char *pszMethod = CSLFetchNameValue(papszTO, "METHOD");
        double adfSrcGT[6] = {0};
        // This MAX_LAT values is equivalent to the semi_major_axis * PI
        // easting/northing value only for EPSG:3857, but it is also quite
        // reasonable for other Mercator projections
        constexpr double MAX_LAT = 85.0511287798066;
        constexpr double EPS = 1e-3;
        const auto GetMinLon = [&adfSrcGT]() { return adfSrcGT[0]; };
        const auto GetMaxLon = [&adfSrcGT, hSrcDS]()
        { return adfSrcGT[0] + adfSrcGT[1] * GDALGetRasterXSize(hSrcDS); };
        const auto GetMinLat = [&adfSrcGT, hSrcDS]()
        { return adfSrcGT[3] + adfSrcGT[5] * GDALGetRasterYSize(hSrcDS); };
        const auto GetMaxLat = [&adfSrcGT]() { return adfSrcGT[3]; };
        if (oSrcSRS.IsGeographic() && !oSrcSRS.IsDerivedGeographic() &&
            pszProjection && EQUAL(pszProjection, SRS_PT_MERCATOR_1SP) &&
            oDstSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0) == 0 &&
            (pszMethod == nullptr || EQUAL(pszMethod, "GEOTRANSFORM")) &&
            CSLFetchNameValue(papszTO, "COORDINATE_OPERATION") == nullptr &&
            CSLFetchNameValue(papszTO, "SRC_METHOD") == nullptr &&
            CSLFetchNameValue(papszTO, "DST_METHOD") == nullptr &&
            GDALGetGeoTransform(hSrcDS, adfSrcGT) == CE_None &&
            adfSrcGT[2] == 0 && adfSrcGT[4] == 0 && adfSrcGT[5] < 0 &&
            GetMinLon() >= -180 - EPS && GetMaxLon() <= 180 + EPS &&
            ((GetMaxLat() > MAX_LAT && GetMinLat() < MAX_LAT) ||
             (GetMaxLat() > -MAX_LAT && GetMinLat() < -MAX_LAT)) &&
            GDALGetMetadata(hSrcDS, "GEOLOC_ARRAY") == nullptr &&
            GDALGetMetadata(hSrcDS, "RPC") == nullptr)
        {
            auto poCT = std::unique_ptr<OGRCoordinateTransformation>(
                OGRCreateCoordinateTransformation(&oSrcSRS, &oDstSRS));
            if (poCT)
            {
                double xLL = std::max(GetMinLon(), -180.0);
                double yLL = std::max(GetMinLat(), -MAX_LAT);
                double xUR = std::min(GetMaxLon(), 180.0);
                double yUR = std::min(GetMaxLat(), MAX_LAT);
                if (poCT->Transform(1, &xLL, &yLL) &&
                    poCT->Transform(1, &xUR, &yUR))
                {
                    psOptions->dfMinX = xLL;
                    psOptions->dfMinY = yLL;
                    psOptions->dfMaxX = xUR;
                    psOptions->dfMaxY = yUR;
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Clamping output bounds to (%f,%f) -> (%f, %f)",
                             psOptions->dfMinX, psOptions->dfMinY,
                             psOptions->dfMaxX, psOptions->dfMaxY);
                }
            }
        }
    }

    /* If (-ts and -te) or (-tr and -te) are specified, we don't need to compute
     * the suggested output extent */
    const bool bNeedsSuggestedWarpOutput =
        !(((psOptions->nForcePixels != 0 && psOptions->nForceLines != 0) ||
           (psOptions->dfXRes != 0 && psOptions->dfYRes != 0)) &&
          !(psOptions->dfMinX == 0.0 && psOptions->dfMinY == 0.0 &&
            psOptions->dfMaxX == 0.0 && psOptions->dfMaxY == 0.0));

    // If -te is specified, not not -tr and -ts
    const bool bKnownTargetExtentButNotResolution =
        !(psOptions->dfMinX == 0.0 && psOptions->dfMinY == 0.0 &&
          psOptions->dfMaxX == 0.0 && psOptions->dfMaxY == 0.0) &&
        psOptions->nForcePixels == 0 && psOptions->nForceLines == 0 &&
        psOptions->dfXRes == 0 && psOptions->dfYRes == 0;

    if (phTransformArg)
        *phTransformArg = nullptr;

    /* -------------------------------------------------------------------- */
    /*      Find the output driver.                                         */
    /* -------------------------------------------------------------------- */
    hDriver = GDALGetDriverByName(pszFormat);
    if (hDriver == nullptr ||
        (GDALGetMetadataItem(hDriver, GDAL_DCAP_CREATE, nullptr) == nullptr &&
         GDALGetMetadataItem(hDriver, GDAL_DCAP_CREATECOPY, nullptr) ==
             nullptr))
    {
        printf("Output driver `%s' not recognised or does not support\n",
               pszFormat);
        printf("direct output file creation or CreateCopy. "
               "The following format drivers are eligible for warp output:\n");

        for (int iDr = 0; iDr < GDALGetDriverCount(); iDr++)
        {
            hDriver = GDALGetDriver(iDr);

            if (GDALGetMetadataItem(hDriver, GDAL_DCAP_RASTER, nullptr) !=
                    nullptr &&
                (GDALGetMetadataItem(hDriver, GDAL_DCAP_CREATE, nullptr) !=
                     nullptr ||
                 GDALGetMetadataItem(hDriver, GDAL_DCAP_CREATECOPY, nullptr) !=
                     nullptr))
            {
                printf("  %s: %s\n", GDALGetDriverShortName(hDriver),
                       GDALGetDriverLongName(hDriver));
            }
        }
        printf("\n");
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      For virtual output files, we have to set a special subclass     */
    /*      of dataset to create.                                           */
    /* -------------------------------------------------------------------- */
    CPLStringList aosCreateOptions(CSLDuplicate(papszCreateOptions));
    if (bVRT)
        aosCreateOptions.SetNameValue("SUBCLASS", "VRTWarpedDataset");

    /* -------------------------------------------------------------------- */
    /*      Loop over all input files to collect extents.                   */
    /* -------------------------------------------------------------------- */
    CPLString osThisTargetSRS;
    {
        const char *pszThisTargetSRS = CSLFetchNameValue(papszTO, "DST_SRS");
        if (pszThisTargetSRS != nullptr)
            osThisTargetSRS = pszThisTargetSRS;
    }

    CPLStringList aoTOList(papszTO, FALSE);

    double dfResFromSourceAndTargetExtent =
        std::numeric_limits<double>::infinity();

    /* -------------------------------------------------------------------- */
    /*      Establish list of files of output dataset if it already exists. */
    /* -------------------------------------------------------------------- */
    std::set<std::string> oSetExistingDestFiles;
    {
        CPLPushErrorHandler(CPLQuietErrorHandler);
        const char *const apszAllowedDrivers[] = {pszFormat, nullptr};
        auto poExistingOutputDS = std::unique_ptr<GDALDataset>(
            GDALDataset::Open(pszFilename, GDAL_OF_RASTER, apszAllowedDrivers));
        if (poExistingOutputDS)
        {
            for (const char *pszFilenameInList :
                 CPLStringList(poExistingOutputDS->GetFileList()))
            {
                oSetExistingDestFiles.insert(
                    CPLString(pszFilenameInList).replaceAll('\\', '/'));
            }
        }
        CPLPopErrorHandler();
    }
    std::set<std::string> oSetExistingDestFilesFoundInSource;

    for (int iSrc = 0; iSrc < nSrcCount; iSrc++)
    {
        /* --------------------------------------------------------------------
         */
        /*      Check that there's at least one raster band */
        /* --------------------------------------------------------------------
         */
        GDALDatasetH hSrcDS = pahSrcDS[iSrc];
        if (GDALGetRasterCount(hSrcDS) == 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Input file %s has no raster bands.",
                     GDALGetDescription(hSrcDS));
            if (hCT != nullptr)
                GDALDestroyColorTable(hCT);
            return nullptr;
        }

        // Examine desired overview level and retrieve the corresponding dataset
        // if it exists.
        std::unique_ptr<GDALDataset> oDstDSOverview;
        if (psOptions->nOvLevel >= 0)
        {
            oDstDSOverview.reset(GDALCreateOverviewDataset(
                GDALDataset::FromHandle(hSrcDS), psOptions->nOvLevel,
                /* bThisLevelOnly = */ true));
            if (oDstDSOverview)
                hSrcDS = oDstDSOverview.get();
        }

        /* --------------------------------------------------------------------
         */
        /*      Check if the source dataset shares some files with the dest
         * one.*/
        /* --------------------------------------------------------------------
         */
        if (!oSetExistingDestFiles.empty())
        {
            // We need to reopen in a temporary dataset for the particular
            // case of overwritten a .tif.ovr file from a .tif
            // If we probe the file list of the .tif, it will then open the
            // .tif.ovr !
            auto poSrcDS = GDALDataset::FromHandle(hSrcDS);
            const char *const apszAllowedDrivers[] = {
                poSrcDS->GetDriver() ? poSrcDS->GetDriver()->GetDescription()
                                     : nullptr,
                nullptr};
            auto poSrcDSTmp = std::unique_ptr<GDALDataset>(GDALDataset::Open(
                poSrcDS->GetDescription(), GDAL_OF_RASTER, apszAllowedDrivers));
            if (poSrcDSTmp)
            {
                for (const char *pszFilenameInList :
                     CPLStringList(poSrcDSTmp->GetFileList()))
                {
                    CPLString osFilename(pszFilenameInList);
                    osFilename.replaceAll('\\', '/');
                    if (oSetExistingDestFiles.find(osFilename) !=
                        oSetExistingDestFiles.end())
                    {
                        oSetExistingDestFilesFoundInSource.insert(osFilename);
                    }
                }
            }
        }

        if (eDT == GDT_Unknown)
            eDT = GDALGetRasterDataType(GDALGetRasterBand(hSrcDS, 1));

        /* --------------------------------------------------------------------
         */
        /*      If we are processing the first file, and it has a raster */
        /*      attribute table, then we will copy it to the destination file.
         */
        /* --------------------------------------------------------------------
         */
        if (iSrc == 0)
        {
            hRAT = GDALGetDefaultRAT(GDALGetRasterBand(hSrcDS, 1));
            if (hRAT != nullptr)
            {
                if (psOptions->eResampleAlg != GRA_NearestNeighbour &&
                    psOptions->eResampleAlg != GRA_Mode &&
                    GDALRATGetTableType(hRAT) == GRTT_THEMATIC)
                {
                    if (!psOptions->bQuiet)
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "Warning: Input file %s has a thematic RAT, "
                                 "which will likely lead "
                                 "to bad results when using a resampling "
                                 "method other than nearest neighbour "
                                 "or mode so we are discarding it.\n",
                                 GDALGetDescription(hSrcDS));
                    }
                    hRAT = nullptr;
                }
                else
                {
                    if (!psOptions->bQuiet)
                        printf("Copying raster attribute table from %s to new "
                               "file.\n",
                               GDALGetDescription(hSrcDS));
                }
            }
        }

        /* --------------------------------------------------------------------
         */
        /*      If we are processing the first file, and it has a color */
        /*      table, then we will copy it to the destination file. */
        /* --------------------------------------------------------------------
         */
        if (iSrc == 0)
        {
            hCT = GDALGetRasterColorTable(GDALGetRasterBand(hSrcDS, 1));
            if (hCT != nullptr)
            {
                hCT = GDALCloneColorTable(hCT);
                if (!psOptions->bQuiet)
                    printf("Copying color table from %s to new file.\n",
                           GDALGetDescription(hSrcDS));
            }

            if (psOptions->anDstBands.empty())
            {
                nDstBandCount = GDALGetRasterCount(hSrcDS);
                for (int iBand = 0; iBand < nDstBandCount; iBand++)
                {
                    if (psOptions->anDstBands.empty())
                    {
                        GDALColorInterp eInterp =
                            GDALGetRasterColorInterpretation(
                                GDALGetRasterBand(hSrcDS, iBand + 1));
                        apeColorInterpretations.push_back(eInterp);
                    }
                }

                // Do we want to generate an alpha band in the output file?
                if (psOptions->bEnableSrcAlpha)
                    nDstBandCount--;

                if (psOptions->bEnableDstAlpha)
                    nDstBandCount++;
            }
            else
            {
                for (int nSrcBand : psOptions->anSrcBands)
                {
                    auto hBand = GDALGetRasterBand(hSrcDS, nSrcBand);
                    GDALColorInterp eInterp =
                        hBand ? GDALGetRasterColorInterpretation(hBand)
                              : GCI_Undefined;
                    apeColorInterpretations.push_back(eInterp);
                }
                nDstBandCount = static_cast<int>(psOptions->anDstBands.size());
                if (psOptions->bEnableDstAlpha)
                {
                    nDstBandCount++;
                    apeColorInterpretations.push_back(GCI_AlphaBand);
                }
                else if (GDALGetRasterCount(hSrcDS) &&
                         GDALGetRasterColorInterpretation(GDALGetRasterBand(
                             hSrcDS, GDALGetRasterCount(hSrcDS))) ==
                             GCI_AlphaBand &&
                         !psOptions->bDisableSrcAlpha)
                {
                    nDstBandCount++;
                    apeColorInterpretations.push_back(GCI_AlphaBand);
                }
            }
        }

        /* --------------------------------------------------------------------
         */
        /*      If we are processing the first file, get the source srs from */
        /*      dataset, if not set already. */
        /* --------------------------------------------------------------------
         */
        const auto osThisSourceSRS = GetSrcDSProjection(hSrcDS, papszTO);
        if (iSrc == 0 && osThisTargetSRS.empty())
        {
            if (!osThisSourceSRS.empty())
            {
                osThisTargetSRS = osThisSourceSRS;
                aoTOList.SetNameValue("DST_SRS", osThisSourceSRS);
            }
        }

        /* --------------------------------------------------------------------
         */
        /*      Create a transformation object from the source to */
        /*      destination coordinate system. */
        /* --------------------------------------------------------------------
         */
        hTransformArg =
            GDALCreateGenImgProjTransformer2(hSrcDS, nullptr, aoTOList.List());

        if (hTransformArg == nullptr)
        {
            if (hCT != nullptr)
                GDALDestroyColorTable(hCT);
            return nullptr;
        }

        GDALTransformerInfo *psInfo =
            static_cast<GDALTransformerInfo *>(hTransformArg);

        /* --------------------------------------------------------------------
         */
        /*      Get approximate output resolution */
        /* --------------------------------------------------------------------
         */

        if (bKnownTargetExtentButNotResolution)
        {
            // Sample points along a grid in target CRS
            constexpr int nPointsX = 10;
            constexpr int nPointsY = 10;
            constexpr int nPoints = 3 * nPointsX * nPointsY;
            std::vector<double> padfX;
            std::vector<double> padfY;
            std::vector<double> padfZ(nPoints);
            std::vector<int> pabSuccess(nPoints);
            const double dfEps =
                std::min(psOptions->dfMaxX - psOptions->dfMinX,
                         std::abs(psOptions->dfMaxY - psOptions->dfMinY)) /
                1000;
            for (int iY = 0; iY < nPointsY; iY++)
            {
                for (int iX = 0; iX < nPointsX; iX++)
                {
                    const double dfX =
                        psOptions->dfMinX +
                        static_cast<double>(iX) *
                            (psOptions->dfMaxX - psOptions->dfMinX) /
                            (nPointsX - 1);
                    const double dfY =
                        psOptions->dfMinY +
                        static_cast<double>(iY) *
                            (psOptions->dfMaxY - psOptions->dfMinY) /
                            (nPointsY - 1);

                    // Reproject each destination sample point and its
                    // neighbours at (x+1,y) and (x,y+1), so as to get the local
                    // scale.
                    padfX.push_back(dfX);
                    padfY.push_back(dfY);

                    padfX.push_back((iX == nPointsX - 1) ? dfX - dfEps
                                                         : dfX + dfEps);
                    padfY.push_back(dfY);

                    padfX.push_back(dfX);
                    padfY.push_back((iY == nPointsY - 1) ? dfY - dfEps
                                                         : dfY + dfEps);
                }
            }

            bool transformedToSrcCRS{false};

            GDALGenImgProjTransformInfo *psTransformInfo{
                static_cast<GDALGenImgProjTransformInfo *>(hTransformArg)};

            // If a transformer is available, use an extent that covers the
            // target extent instead of the real source image extent, but also
            // check for target extent compatibility with source CRS extent
            if (psTransformInfo && psTransformInfo->pReprojectArg &&
                psTransformInfo->pSrcTransformer == nullptr)
            {
                const GDALReprojectionTransformInfo *psRTI =
                    static_cast<const GDALReprojectionTransformInfo *>(
                        psTransformInfo->pReprojectArg);
                if (psRTI && psRTI->poReverseTransform)
                {

                    // Compute new geotransform from transformed target extent
                    double adfGeoTransform[6];
                    if (GDALGetGeoTransform(hSrcDS, adfGeoTransform) ==
                            CE_None &&
                        adfGeoTransform[2] == 0 && adfGeoTransform[4] == 0)
                    {

                        // Transform target extent to source CRS
                        double dfMinX = psOptions->dfMinX;
                        double dfMinY = psOptions->dfMinY;

                        // Need this to check if the target extent is compatible with the source extent
                        double dfMaxX = psOptions->dfMaxX;
                        double dfMaxY = psOptions->dfMaxY;

                        // Clone of psRTI->poReverseTransform with CHECK_WITH_INVERT_PROJ set to TRUE
                        // to detect out of source CRS bounds destination extent and fall back to original
                        // algorithm if needed
                        CPLConfigOptionSetter oSetter("CHECK_WITH_INVERT_PROJ",
                                                      "TRUE", false);
                        OGRCoordinateTransformationOptions options;
                        auto poReverseTransform =
                            std::unique_ptr<OGRCoordinateTransformation>(
                                OGRCreateCoordinateTransformation(
                                    psRTI->poReverseTransform->GetSourceCS(),
                                    psRTI->poReverseTransform->GetTargetCS(),
                                    options));

                        if (poReverseTransform)
                        {

                            poReverseTransform->Transform(
                                1, &dfMinX, &dfMinY, nullptr, &pabSuccess[0]);

                            if (pabSuccess[0])
                            {
                                adfGeoTransform[0] = dfMinX;
                                adfGeoTransform[3] = dfMinY;

                                poReverseTransform->Transform(1, &dfMaxX,
                                                              &dfMaxY, nullptr,
                                                              &pabSuccess[0]);

                                if (pabSuccess[0])
                                {

                                    // Reproject to source image CRS
                                    psRTI->poReverseTransform->Transform(
                                        nPoints, &padfX[0], &padfY[0],
                                        &padfZ[0], &pabSuccess[0]);

                                    // Transform back to source image coordinate space using geotransform
                                    for (size_t i = 0; i < padfX.size(); i++)
                                    {
                                        padfX[i] =
                                            (padfX[i] - adfGeoTransform[0]) /
                                            adfGeoTransform[1];
                                        padfY[i] = std::abs(
                                            (padfY[i] - adfGeoTransform[3]) /
                                            adfGeoTransform[5]);
                                    }

                                    transformedToSrcCRS = true;
                                }
                            }
                        }
                    }
                }
            }

            if (!transformedToSrcCRS)
            {
                // Transform to source image coordinate space
                psInfo->pfnTransform(hTransformArg, TRUE, nPoints, &padfX[0],
                                     &padfY[0], &padfZ[0], &pabSuccess[0]);
            }

            // Compute the resolution at sampling points
            std::vector<std::pair<double, double>> aoResPairs;

            const auto Distance = [](double x, double y)
            { return sqrt(x * x + y * y); };

            const int nSrcXSize = GDALGetRasterXSize(hSrcDS);
            const int nSrcYSize = GDALGetRasterYSize(hSrcDS);

            for (int i = 0; i < nPoints; i += 3)
            {
                if (pabSuccess[i] && pabSuccess[i + 1] && pabSuccess[i + 2] &&
                    padfX[i] >= 0 && padfY[i] >= 0 &&
                    (transformedToSrcCRS ||
                     (padfX[i] <= nSrcXSize && padfY[i] <= nSrcYSize)))
                {
                    const double dfRes1 =
                        std::abs(dfEps) / Distance(padfX[i + 1] - padfX[i],
                                                   padfY[i + 1] - padfY[i]);
                    const double dfRes2 =
                        std::abs(dfEps) / Distance(padfX[i + 2] - padfX[i],
                                                   padfY[i + 2] - padfY[i]);
                    if (std::isfinite(dfRes1) && std::isfinite(dfRes2))
                    {
                        aoResPairs.push_back(
                            std::pair<double, double>(dfRes1, dfRes2));
                    }
                }
            }

            // Find the minimum resolution that is at least 10 times greater
            // than the median, to remove outliers.
            // Start first by doing that on dfRes1, then dfRes2 and then their
            // average.
            std::sort(aoResPairs.begin(), aoResPairs.end(),
                      [](const std::pair<double, double> &oPair1,
                         const std::pair<double, double> &oPair2)
                      { return oPair1.first < oPair2.first; });

            if (!aoResPairs.empty())
            {
                std::vector<std::pair<double, double>> aoResPairsNew;
                const double dfMedian1 =
                    aoResPairs[aoResPairs.size() / 2].first;
                for (const auto &oPair : aoResPairs)
                {
                    if (oPair.first > dfMedian1 / 10)
                    {
                        aoResPairsNew.push_back(oPair);
                    }
                }

                aoResPairs = std::move(aoResPairsNew);
                std::sort(aoResPairs.begin(), aoResPairs.end(),
                          [](const std::pair<double, double> &oPair1,
                             const std::pair<double, double> &oPair2)
                          { return oPair1.second < oPair2.second; });
                if (!aoResPairs.empty())
                {
                    std::vector<double> adfRes;
                    const double dfMedian2 =
                        aoResPairs[aoResPairs.size() / 2].second;
                    for (const auto &oPair : aoResPairs)
                    {
                        if (oPair.second > dfMedian2 / 10)
                        {
                            adfRes.push_back((oPair.first + oPair.second) / 2);
                        }
                    }

                    std::sort(adfRes.begin(), adfRes.end());
                    if (!adfRes.empty())
                    {
                        const double dfMedian = adfRes[adfRes.size() / 2];
                        for (const double dfRes : adfRes)
                        {
                            if (dfRes > dfMedian / 10)
                            {
                                dfResFromSourceAndTargetExtent = std::min(
                                    dfResFromSourceAndTargetExtent, dfRes);
                                break;
                            }
                        }
                    }
                }
            }
        }

        /* --------------------------------------------------------------------
         */
        /*      Get approximate output definition. */
        /* --------------------------------------------------------------------
         */
        double adfThisGeoTransform[6];
        double adfExtent[4];
        if (bNeedsSuggestedWarpOutput)
        {
            int nThisPixels, nThisLines;

            // For sum, round-up dimension, to be sure that the output extent
            // includes all source pixels, to have the sum preserving property.
            int nOptions = (psOptions->eResampleAlg == GRA_Sum)
                               ? GDAL_SWO_ROUND_UP_SIZE
                               : 0;
            if (psOptions->bSquarePixels)
            {
                nOptions |= GDAL_SWO_FORCE_SQUARE_PIXEL;
            }

            if (GDALSuggestedWarpOutput2(hSrcDS, psInfo->pfnTransform,
                                         hTransformArg, adfThisGeoTransform,
                                         &nThisPixels, &nThisLines, adfExtent,
                                         nOptions) != CE_None)
            {
                if (hCT != nullptr)
                    GDALDestroyColorTable(hCT);
                GDALDestroyGenImgProjTransformer(hTransformArg);
                return nullptr;
            }

            if (CPLGetConfigOption("CHECK_WITH_INVERT_PROJ", nullptr) ==
                nullptr)
            {
                double MinX = adfExtent[0];
                double MaxX = adfExtent[2];
                double MaxY = adfExtent[3];
                double MinY = adfExtent[1];
                int bSuccess = TRUE;

                // +/-180 deg in longitude do not roundtrip sometimes
                if (MinX == -180)
                    MinX += 1e-6;
                if (MaxX == 180)
                    MaxX -= 1e-6;

                // +/-90 deg in latitude do not roundtrip sometimes
                if (MinY == -90)
                    MinY += 1e-6;
                if (MaxY == 90)
                    MaxY -= 1e-6;

                /* Check that the edges of the target image are in the validity
                 * area */
                /* of the target projection */
                const int N_STEPS = 20;
                for (int i = 0; i <= N_STEPS && bSuccess; i++)
                {
                    for (int j = 0; j <= N_STEPS && bSuccess; j++)
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
                        /* Target SRS coordinates to source image pixel
                         * coordinates */
                        if (!psInfo->pfnTransform(hTransformArg, TRUE, 1, &x,
                                                  &y, &z, &bSuccess) ||
                            !bSuccess)
                            bSuccess = FALSE;
                        /* Source image pixel coordinates to target SRS
                         * coordinates */
                        if (!psInfo->pfnTransform(hTransformArg, FALSE, 1, &x,
                                                  &y, &z, &bSuccess) ||
                            !bSuccess)
                            bSuccess = FALSE;
                        if (fabs(x - expected_x) >
                                (MaxX - MinX) / nThisPixels ||
                            fabs(y - expected_y) > (MaxY - MinY) / nThisLines)
                            bSuccess = FALSE;
                    }
                }

                /* If not, retry with CHECK_WITH_INVERT_PROJ=TRUE that forces
                 * ogrct.cpp */
                /* to check the consistency of each requested projection result
                 * with the */
                /* invert projection */
                if (!bSuccess)
                {
                    CPLSetThreadLocalConfigOption("CHECK_WITH_INVERT_PROJ",
                                                  "TRUE");
                    CPLDebug("WARP", "Recompute out extent with "
                                     "CHECK_WITH_INVERT_PROJ=TRUE");

                    const CPLErr eErr = GDALSuggestedWarpOutput2(
                        hSrcDS, psInfo->pfnTransform, hTransformArg,
                        adfThisGeoTransform, &nThisPixels, &nThisLines,
                        adfExtent, 0);
                    CPLSetThreadLocalConfigOption("CHECK_WITH_INVERT_PROJ",
                                                  nullptr);
                    if (eErr != CE_None)
                    {
                        if (hCT != nullptr)
                            GDALDestroyColorTable(hCT);
                        GDALDestroyGenImgProjTransformer(hTransformArg);
                        return nullptr;
                    }
                }
            }
        }

        // If no reprojection or geometry change is involved, and that the
        // source image is north-up, preserve source resolution instead of
        // forcing square pixels.
        const char *pszMethod = CSLFetchNameValue(papszTO, "METHOD");
        double adfThisGeoTransformTmp[6];
        if (!psOptions->bSquarePixels && bNeedsSuggestedWarpOutput &&
            psOptions->dfXRes == 0 && psOptions->dfYRes == 0 &&
            psOptions->nForcePixels == 0 && psOptions->nForceLines == 0 &&
            (pszMethod == nullptr || EQUAL(pszMethod, "GEOTRANSFORM")) &&
            CSLFetchNameValue(papszTO, "COORDINATE_OPERATION") == nullptr &&
            CSLFetchNameValue(papszTO, "SRC_METHOD") == nullptr &&
            CSLFetchNameValue(papszTO, "DST_METHOD") == nullptr &&
            GDALGetGeoTransform(hSrcDS, adfThisGeoTransformTmp) == CE_None &&
            adfThisGeoTransformTmp[2] == 0 && adfThisGeoTransformTmp[4] == 0 &&
            adfThisGeoTransformTmp[5] < 0 &&
            GDALGetMetadata(hSrcDS, "GEOLOC_ARRAY") == nullptr &&
            GDALGetMetadata(hSrcDS, "RPC") == nullptr)
        {
            bool bIsSameHorizontal = osThisSourceSRS == osThisTargetSRS;
            if (!bIsSameHorizontal)
            {
                OGRSpatialReference oSrcSRS;
                OGRSpatialReference oDstSRS;
                CPLErrorStateBackuper oErrorStateBackuper(CPLQuietErrorHandler);
                // DemoteTo2D requires PROJ >= 6.3
                if (oSrcSRS.SetFromUserInput(osThisSourceSRS.c_str()) ==
                        OGRERR_NONE &&
                    oDstSRS.SetFromUserInput(osThisTargetSRS.c_str()) ==
                        OGRERR_NONE &&
                    (oSrcSRS.GetAxesCount() == 3 ||
                     oDstSRS.GetAxesCount() == 3) &&
                    oSrcSRS.DemoteTo2D(nullptr) == OGRERR_NONE &&
                    oDstSRS.DemoteTo2D(nullptr) == OGRERR_NONE)
                {
                    bIsSameHorizontal = oSrcSRS.IsSame(&oDstSRS);
                }
            }
            if (bIsSameHorizontal)
            {
                memcpy(adfThisGeoTransform, adfThisGeoTransformTmp,
                       6 * sizeof(double));
                adfExtent[0] = adfThisGeoTransform[0];
                adfExtent[1] =
                    adfThisGeoTransform[3] +
                    GDALGetRasterYSize(hSrcDS) * adfThisGeoTransform[5];
                adfExtent[2] =
                    adfThisGeoTransform[0] +
                    GDALGetRasterXSize(hSrcDS) * adfThisGeoTransform[1];
                adfExtent[3] = adfThisGeoTransform[3];
                dfResFromSourceAndTargetExtent =
                    std::numeric_limits<double>::infinity();
            }
        }

        if (bNeedsSuggestedWarpOutput)
        {
            /* --------------------------------------------------------------------
             */
            /*      Expand the working bounds to include this region, ensure the
             */
            /*      working resolution is no more than this resolution. */
            /* --------------------------------------------------------------------
             */
            if (dfWrkMaxX == 0.0 && dfWrkMinX == 0.0)
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
                dfWrkMinY = std::min(dfWrkMinY, adfExtent[1]);
                dfWrkResX = std::min(dfWrkResX, adfThisGeoTransform[1]);
                dfWrkResY =
                    std::min(dfWrkResY, std::abs(adfThisGeoTransform[5]));
            }
        }

        if (nSrcCount == 1 && phTransformArg)
        {
            *phTransformArg = hTransformArg;
        }
        else
        {
            GDALDestroyGenImgProjTransformer(hTransformArg);
        }
    }

    // If the source file(s) and the dest one share some files in common,
    // only remove the files that are *not* in common
    if (!oSetExistingDestFilesFoundInSource.empty())
    {
        for (const std::string &osFilename : oSetExistingDestFiles)
        {
            if (oSetExistingDestFilesFoundInSource.find(osFilename) ==
                oSetExistingDestFilesFoundInSource.end())
            {
                VSIUnlink(osFilename.c_str());
            }
        }
    }

    if (std::isfinite(dfResFromSourceAndTargetExtent))
    {
        dfWrkResX = dfResFromSourceAndTargetExtent;
        dfWrkResY = dfResFromSourceAndTargetExtent;
    }

    /* -------------------------------------------------------------------- */
    /*      Did we have any usable sources?                                 */
    /* -------------------------------------------------------------------- */
    if (nDstBandCount == 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "No usable source images.");
        if (hCT != nullptr)
            GDALDestroyColorTable(hCT);
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Turn the suggested region into a geotransform and suggested     */
    /*      number of pixels and lines.                                     */
    /* -------------------------------------------------------------------- */
    double adfDstGeoTransform[6] = {0, 0, 0, 0, 0, 0};
    int nPixels = 0;
    int nLines = 0;

    if (bNeedsSuggestedWarpOutput)
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
    if (UseTEAndTSAndTRConsistently(psOptions))
    {
        adfDstGeoTransform[0] = psOptions->dfMinX;
        adfDstGeoTransform[3] = psOptions->dfMaxY;
        adfDstGeoTransform[1] = psOptions->dfXRes;
        adfDstGeoTransform[5] = -psOptions->dfYRes;

        nPixels = psOptions->nForcePixels;
        nLines = psOptions->nForceLines;
    }
    else if (psOptions->dfXRes != 0.0 && psOptions->dfYRes != 0.0)
    {
        bool bDetectBlankBorders = false;

        if (psOptions->dfMinX == 0.0 && psOptions->dfMinY == 0.0 &&
            psOptions->dfMaxX == 0.0 && psOptions->dfMaxY == 0.0)
        {
            bDetectBlankBorders = bNeedsSuggestedWarpOutput;

            psOptions->dfMinX = adfDstGeoTransform[0];
            psOptions->dfMaxX =
                adfDstGeoTransform[0] + adfDstGeoTransform[1] * nPixels;
            psOptions->dfMaxY = adfDstGeoTransform[3];
            psOptions->dfMinY =
                adfDstGeoTransform[3] + adfDstGeoTransform[5] * nLines;
        }

        if (psOptions->bTargetAlignedPixels ||
            (psOptions->bCropToCutline &&
             psOptions->aosWarpOptions.FetchBool("CUTLINE_ALL_TOUCHED", false)))
        {
            if ((psOptions->bTargetAlignedPixels &&
                 bNeedsSuggestedWarpOutput) ||
                (psOptions->bCropToCutline &&
                 psOptions->aosWarpOptions.FetchBool("CUTLINE_ALL_TOUCHED",
                                                     false)))
            {
                bDetectBlankBorders = true;
            }
            constexpr double EPS = 1e-8;
            psOptions->dfMinX =
                floor(psOptions->dfMinX / psOptions->dfXRes + EPS) *
                psOptions->dfXRes;
            psOptions->dfMaxX =
                ceil(psOptions->dfMaxX / psOptions->dfXRes - EPS) *
                psOptions->dfXRes;
            psOptions->dfMinY =
                floor(psOptions->dfMinY / psOptions->dfYRes + EPS) *
                psOptions->dfYRes;
            psOptions->dfMaxY =
                ceil(psOptions->dfMaxY / psOptions->dfYRes - EPS) *
                psOptions->dfYRes;
        }

        const auto UpdateGeoTransformandAndPixelLines = [&]()
        {
            nPixels = static_cast<int>((psOptions->dfMaxX - psOptions->dfMinX +
                                        (psOptions->dfXRes / 2.0)) /
                                       psOptions->dfXRes);
            nLines = static_cast<int>(
                (std::fabs(psOptions->dfMaxY - psOptions->dfMinY) +
                 (psOptions->dfYRes / 2.0)) /
                psOptions->dfYRes);
            adfDstGeoTransform[0] = psOptions->dfMinX;
            adfDstGeoTransform[3] = psOptions->dfMaxY;
            adfDstGeoTransform[1] = psOptions->dfXRes;
            adfDstGeoTransform[5] = (psOptions->dfMaxY > psOptions->dfMinY)
                                        ? -psOptions->dfYRes
                                        : psOptions->dfYRes;
        };

        if (bDetectBlankBorders && nSrcCount == 1 && phTransformArg &&
            *phTransformArg != nullptr)
        {
            // Try to detect if the edge of the raster would be blank
            // Cf https://github.com/OSGeo/gdal/issues/7905
            while (true)
            {
                UpdateGeoTransformandAndPixelLines();

                GDALSetGenImgProjTransformerDstGeoTransform(*phTransformArg,
                                                            adfDstGeoTransform);

                std::vector<double> adfX(std::max(nPixels, nLines));
                std::vector<double> adfY(adfX.size());
                std::vector<double> adfZ(adfX.size());
                std::vector<int> abSuccess(adfX.size());

                const auto DetectBlankBorder =
                    [&](int nValues,
                        std::function<bool(double, double)> funcIsOK)
                {
                    if (nValues > 3)
                    {
                        // First try with just a subsample of 3 points
                        double adf3X[3] = {adfX[0], adfX[nValues / 2],
                                           adfX[nValues - 1]};
                        double adf3Y[3] = {adfY[0], adfY[nValues / 2],
                                           adfY[nValues - 1]};
                        double adf3Z[3] = {0};
                        if (GDALGenImgProjTransform(*phTransformArg, TRUE, 3,
                                                    &adf3X[0], &adf3Y[0],
                                                    &adf3Z[0], &abSuccess[0]))
                        {
                            for (int i = 0; i < 3; ++i)
                            {
                                if (abSuccess[i] &&
                                    funcIsOK(adf3X[i], adf3Y[i]))
                                {
                                    return false;
                                }
                            }
                        }
                    }

                    // Do on full border to confirm
                    if (GDALGenImgProjTransform(*phTransformArg, TRUE, nValues,
                                                &adfX[0], &adfY[0], &adfZ[0],
                                                &abSuccess[0]))
                    {
                        for (int i = 0; i < nValues; ++i)
                        {
                            if (abSuccess[i] && funcIsOK(adfX[i], adfY[i]))
                            {
                                return false;
                            }
                        }
                    }

                    return true;
                };

                for (int i = 0; i < nPixels; ++i)
                {
                    adfX[i] = i + 0.5;
                    adfY[i] = 0.5;
                    adfZ[i] = 0;
                }
                const bool bTopBlankLine = DetectBlankBorder(
                    nPixels, [](double, double y) { return y >= 0; });

                for (int i = 0; i < nPixels; ++i)
                {
                    adfX[i] = i + 0.5;
                    adfY[i] = nLines - 0.5;
                    adfZ[i] = 0;
                }
                const int nSrcLines = GDALGetRasterYSize(pahSrcDS[0]);
                const bool bBottomBlankLine =
                    DetectBlankBorder(nPixels, [nSrcLines](double, double y)
                                      { return y <= nSrcLines; });

                for (int i = 0; i < nLines; ++i)
                {
                    adfX[i] = 0.5;
                    adfY[i] = i + 0.5;
                    adfZ[i] = 0;
                }
                const bool bLeftBlankCol = DetectBlankBorder(
                    nLines, [](double x, double) { return x >= 0; });

                for (int i = 0; i < nLines; ++i)
                {
                    adfX[i] = nPixels - 0.5;
                    adfY[i] = i + 0.5;
                    adfZ[i] = 0;
                }
                const int nSrcCols = GDALGetRasterXSize(pahSrcDS[0]);
                const bool bRightBlankCol =
                    DetectBlankBorder(nLines, [nSrcCols](double x, double)
                                      { return x <= nSrcCols; });

                if (!bTopBlankLine && !bBottomBlankLine && !bLeftBlankCol &&
                    !bRightBlankCol)
                    break;

                if (bTopBlankLine)
                {
                    psOptions->dfMaxY -= psOptions->dfYRes;
                }
                if (bBottomBlankLine)
                {
                    psOptions->dfMinY += psOptions->dfYRes;
                }
                if (bLeftBlankCol)
                {
                    psOptions->dfMinX += psOptions->dfXRes;
                }
                if (bRightBlankCol)
                {
                    psOptions->dfMaxX -= psOptions->dfXRes;
                }
            }
        }

        UpdateGeoTransformandAndPixelLines();
    }

    else if (psOptions->nForcePixels != 0 && psOptions->nForceLines != 0)
    {
        if (psOptions->dfMinX == 0.0 && psOptions->dfMinY == 0.0 &&
            psOptions->dfMaxX == 0.0 && psOptions->dfMaxY == 0.0)
        {
            psOptions->dfMinX = dfWrkMinX;
            psOptions->dfMaxX = dfWrkMaxX;
            psOptions->dfMaxY = dfWrkMaxY;
            psOptions->dfMinY = dfWrkMinY;
        }

        psOptions->dfXRes =
            (psOptions->dfMaxX - psOptions->dfMinX) / psOptions->nForcePixels;
        psOptions->dfYRes =
            (psOptions->dfMaxY - psOptions->dfMinY) / psOptions->nForceLines;

        adfDstGeoTransform[0] = psOptions->dfMinX;
        adfDstGeoTransform[3] = psOptions->dfMaxY;
        adfDstGeoTransform[1] = psOptions->dfXRes;
        adfDstGeoTransform[5] = -psOptions->dfYRes;

        nPixels = psOptions->nForcePixels;
        nLines = psOptions->nForceLines;
    }

    else if (psOptions->nForcePixels != 0)
    {
        if (psOptions->dfMinX == 0.0 && psOptions->dfMinY == 0.0 &&
            psOptions->dfMaxX == 0.0 && psOptions->dfMaxY == 0.0)
        {
            psOptions->dfMinX = dfWrkMinX;
            psOptions->dfMaxX = dfWrkMaxX;
            psOptions->dfMaxY = dfWrkMaxY;
            psOptions->dfMinY = dfWrkMinY;
        }

        psOptions->dfXRes =
            (psOptions->dfMaxX - psOptions->dfMinX) / psOptions->nForcePixels;
        psOptions->dfYRes = psOptions->dfXRes;

        adfDstGeoTransform[0] = psOptions->dfMinX;
        adfDstGeoTransform[3] = psOptions->dfMaxY;
        adfDstGeoTransform[1] = psOptions->dfXRes;
        adfDstGeoTransform[5] = (psOptions->dfMaxY > psOptions->dfMinY)
                                    ? -psOptions->dfYRes
                                    : psOptions->dfYRes;

        nPixels = psOptions->nForcePixels;
        nLines =
            static_cast<int>((std::fabs(psOptions->dfMaxY - psOptions->dfMinY) +
                              (psOptions->dfYRes / 2.0)) /
                             psOptions->dfYRes);
    }

    else if (psOptions->nForceLines != 0)
    {
        if (psOptions->dfMinX == 0.0 && psOptions->dfMinY == 0.0 &&
            psOptions->dfMaxX == 0.0 && psOptions->dfMaxY == 0.0)
        {
            psOptions->dfMinX = dfWrkMinX;
            psOptions->dfMaxX = dfWrkMaxX;
            psOptions->dfMaxY = dfWrkMaxY;
            psOptions->dfMinY = dfWrkMinY;
        }

        psOptions->dfYRes =
            (psOptions->dfMaxY - psOptions->dfMinY) / psOptions->nForceLines;
        psOptions->dfXRes = std::fabs(psOptions->dfYRes);

        adfDstGeoTransform[0] = psOptions->dfMinX;
        adfDstGeoTransform[3] = psOptions->dfMaxY;
        adfDstGeoTransform[1] = psOptions->dfXRes;
        adfDstGeoTransform[5] = -psOptions->dfYRes;

        nPixels = static_cast<int>((psOptions->dfMaxX - psOptions->dfMinX +
                                    (psOptions->dfXRes / 2.0)) /
                                   psOptions->dfXRes);
        nLines = psOptions->nForceLines;
    }

    else if (psOptions->dfMinX != 0.0 || psOptions->dfMinY != 0.0 ||
             psOptions->dfMaxX != 0.0 || psOptions->dfMaxY != 0.0)
    {
        psOptions->dfXRes = adfDstGeoTransform[1];
        psOptions->dfYRes = fabs(adfDstGeoTransform[5]);

        nPixels = static_cast<int>((psOptions->dfMaxX - psOptions->dfMinX +
                                    (psOptions->dfXRes / 2.0)) /
                                   psOptions->dfXRes);
        nLines =
            static_cast<int>((std::fabs(psOptions->dfMaxY - psOptions->dfMinY) +
                              (psOptions->dfYRes / 2.0)) /
                             psOptions->dfYRes);

        psOptions->dfXRes = (psOptions->dfMaxX - psOptions->dfMinX) / nPixels;
        psOptions->dfYRes = (psOptions->dfMaxY - psOptions->dfMinY) / nLines;

        adfDstGeoTransform[0] = psOptions->dfMinX;
        adfDstGeoTransform[3] = psOptions->dfMaxY;
        adfDstGeoTransform[1] = psOptions->dfXRes;
        adfDstGeoTransform[5] = -psOptions->dfYRes;
    }

    if (EQUAL(pszFormat, "GTiff"))
    {

        /* --------------------------------------------------------------------
         */
        /*      Automatically set PHOTOMETRIC=RGB for GTiff when appropriate */
        /* --------------------------------------------------------------------
         */
        if (apeColorInterpretations.size() >= 3 &&
            apeColorInterpretations[0] == GCI_RedBand &&
            apeColorInterpretations[1] == GCI_GreenBand &&
            apeColorInterpretations[2] == GCI_BlueBand &&
            aosCreateOptions.FetchNameValue("PHOTOMETRIC") == nullptr)
        {
            aosCreateOptions.SetNameValue("PHOTOMETRIC", "RGB");
        }

        /* The GTiff driver now supports writing band color interpretation */
        /* in the TIFF_GDAL_METADATA tag */
        bSetColorInterpretation = true;
    }

    /* -------------------------------------------------------------------- */
    /*      Create the output file.                                         */
    /* -------------------------------------------------------------------- */
    if (!psOptions->bQuiet)
        printf("Creating output file that is %dP x %dL.\n", nPixels, nLines);

    hDstDS = GDALCreate(hDriver, pszFilename, nPixels, nLines, nDstBandCount,
                        eDT, aosCreateOptions.List());

    if (hDstDS == nullptr)
    {
        if (hCT != nullptr)
            GDALDestroyColorTable(hCT);
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Write out the projection definition.                            */
    /* -------------------------------------------------------------------- */
    const char *pszDstMethod = CSLFetchNameValue(papszTO, "DST_METHOD");
    if (pszDstMethod == nullptr || !EQUAL(pszDstMethod, "NO_GEOTRANSFORM"))
    {
        OGRSpatialReference oTargetSRS;
        oTargetSRS.SetFromUserInput(osThisTargetSRS);
        oTargetSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

        if (oTargetSRS.IsDynamic())
        {
            double dfCoordEpoch = CPLAtof(CSLFetchNameValueDef(
                papszTO, "DST_COORDINATE_EPOCH",
                CSLFetchNameValueDef(papszTO, "COORDINATE_EPOCH", "0")));
            if (dfCoordEpoch == 0)
            {
                const OGRSpatialReferenceH hSrcSRS =
                    GDALGetSpatialRef(pahSrcDS[0]);
                const char *pszMethod = CSLFetchNameValue(papszTO, "METHOD");
                if (hSrcSRS &&
                    (pszMethod == nullptr || EQUAL(pszMethod, "GEOTRANSFORM")))
                {
                    dfCoordEpoch = OSRGetCoordinateEpoch(hSrcSRS);
                }
            }
            if (dfCoordEpoch > 0)
                oTargetSRS.SetCoordinateEpoch(dfCoordEpoch);
        }

        if (GDALSetSpatialRef(hDstDS, OGRSpatialReference::ToHandle(
                                          &oTargetSRS)) == CE_Failure ||
            GDALSetGeoTransform(hDstDS, adfDstGeoTransform) == CE_Failure)
        {
            if (hCT != nullptr)
                GDALDestroyColorTable(hCT);
            GDALClose(hDstDS);
            return nullptr;
        }
    }
    else
    {
        adfDstGeoTransform[3] += adfDstGeoTransform[5] * nLines;
        adfDstGeoTransform[5] = fabs(adfDstGeoTransform[5]);
    }

    if (phTransformArg && *phTransformArg != nullptr)
        GDALSetGenImgProjTransformerDstGeoTransform(*phTransformArg,
                                                    adfDstGeoTransform);

    /* -------------------------------------------------------------------- */
    /*      Try to set color interpretation of source bands to target       */
    /*      dataset.                                                        */
    /*      FIXME? We should likely do that for other drivers than VRT &    */
    /*      GTiff  but it might create spurious .aux.xml files (at least    */
    /*      with HFA, and netCDF)                                           */
    /* -------------------------------------------------------------------- */
    if (bVRT || bSetColorInterpretation)
    {
        int nBandsToCopy = static_cast<int>(apeColorInterpretations.size());
        if (psOptions->bEnableSrcAlpha)
            nBandsToCopy--;
        for (int iBand = 0; iBand < nBandsToCopy; iBand++)
        {
            GDALSetRasterColorInterpretation(
                GDALGetRasterBand(hDstDS, iBand + 1),
                apeColorInterpretations[iBand]);
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Try to set color interpretation of output file alpha band.      */
    /* -------------------------------------------------------------------- */
    if (psOptions->bEnableDstAlpha)
    {
        GDALSetRasterColorInterpretation(
            GDALGetRasterBand(hDstDS, nDstBandCount), GCI_AlphaBand);
    }

    /* -------------------------------------------------------------------- */
    /*      Copy the raster attribute table, if required.                   */
    /* -------------------------------------------------------------------- */
    if (hRAT != nullptr)
    {
        GDALSetDefaultRAT(GDALGetRasterBand(hDstDS, 1), hRAT);
    }

    /* -------------------------------------------------------------------- */
    /*      Copy the color table, if required.                              */
    /* -------------------------------------------------------------------- */
    if (hCT != nullptr)
    {
        GDALSetRasterColorTable(GDALGetRasterBand(hDstDS, 1), hCT);
        GDALDestroyColorTable(hCT);
    }

    /* -------------------------------------------------------------------- */
    /*      Copy scale/offset if found on source                            */
    /* -------------------------------------------------------------------- */
    if (nSrcCount == 1)
    {
        GDALDataset *poSrcDS = GDALDataset::FromHandle(pahSrcDS[0]);
        GDALDataset *poDstDS = GDALDataset::FromHandle(hDstDS);

        int nBandsToCopy = nDstBandCount;
        if (psOptions->bEnableDstAlpha)
            nBandsToCopy--;
        nBandsToCopy = std::min(nBandsToCopy, poSrcDS->GetRasterCount());

        for (int i = 0; i < nBandsToCopy; i++)
        {
            auto poSrcBand = poSrcDS->GetRasterBand(
                psOptions->anSrcBands.empty() ? i + 1
                                              : psOptions->anSrcBands[i]);
            auto poDstBand = poDstDS->GetRasterBand(
                psOptions->anDstBands.empty() ? i + 1
                                              : psOptions->anDstBands[i]);
            if (poSrcBand && poDstBand)
            {
                int bHasScale = FALSE;
                const double dfScale = poSrcBand->GetScale(&bHasScale);
                if (bHasScale)
                    poDstBand->SetScale(dfScale);

                int bHasOffset = FALSE;
                const double dfOffset = poSrcBand->GetOffset(&bHasOffset);
                if (bHasOffset)
                    poDstBand->SetOffset(dfOffset);
            }
        }
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
    void *hSrcImageTransformer = nullptr;

    explicit CutlineTransformer(void *hTransformArg)
        : hSrcImageTransformer(hTransformArg)
    {
    }

    virtual const OGRSpatialReference *GetSourceCS() const override
    {
        return nullptr;
    }

    virtual const OGRSpatialReference *GetTargetCS() const override
    {
        return nullptr;
    }

    virtual ~CutlineTransformer()
    {
        GDALDestroyTransformer(hSrcImageTransformer);
    }

    virtual int Transform(size_t nCount, double *x, double *y, double *z,
                          double * /* t */, int *pabSuccess) override
    {
        CPLAssert(nCount <=
                  static_cast<size_t>(std::numeric_limits<int>::max()));
        return GDALGenImgProjTransform(hSrcImageTransformer, TRUE,
                                       static_cast<int>(nCount), x, y, z,
                                       pabSuccess);
    }

    virtual OGRCoordinateTransformation *Clone() const override
    {
        return new CutlineTransformer(
            GDALCloneTransformer(hSrcImageTransformer));
    }

    virtual OGRCoordinateTransformation *GetInverse() const override
    {
        return nullptr;
    }
};

static double GetMaximumSegmentLength(OGRGeometry *poGeom)
{
    switch (wkbFlatten(poGeom->getGeometryType()))
    {
        case wkbLineString:
        {
            OGRLineString *poLS = static_cast<OGRLineString *>(poGeom);
            double dfMaxSquaredLength = 0.0;
            for (int i = 0; i < poLS->getNumPoints() - 1; i++)
            {
                double dfDeltaX = poLS->getX(i + 1) - poLS->getX(i);
                double dfDeltaY = poLS->getY(i + 1) - poLS->getY(i);
                double dfSquaredLength =
                    dfDeltaX * dfDeltaX + dfDeltaY * dfDeltaY;
                dfMaxSquaredLength =
                    std::max(dfMaxSquaredLength, dfSquaredLength);
            }
            return sqrt(dfMaxSquaredLength);
        }

        case wkbPolygon:
        {
            OGRPolygon *poPoly = static_cast<OGRPolygon *>(poGeom);
            double dfMaxLength =
                GetMaximumSegmentLength(poPoly->getExteriorRing());
            for (int i = 0; i < poPoly->getNumInteriorRings(); i++)
            {
                dfMaxLength = std::max(
                    dfMaxLength,
                    GetMaximumSegmentLength(poPoly->getInteriorRing(i)));
            }
            return dfMaxLength;
        }

        case wkbMultiPolygon:
        {
            OGRMultiPolygon *poMP = static_cast<OGRMultiPolygon *>(poGeom);
            double dfMaxLength = 0.0;
            for (int i = 0; i < poMP->getNumGeometries(); i++)
            {
                dfMaxLength =
                    std::max(dfMaxLength,
                             GetMaximumSegmentLength(poMP->getGeometryRef(i)));
            }
            return dfMaxLength;
        }

        default:
            CPLAssert(false);
            return 0.0;
    }
}

/************************************************************************/
/*                      RemoveZeroWidthSlivers()                        */
/*                                                                      */
/* Such slivers can cause issues after reprojection.                    */
/************************************************************************/

static void RemoveZeroWidthSlivers(OGRGeometry *poGeom)
{
    const OGRwkbGeometryType eType = wkbFlatten(poGeom->getGeometryType());
    if (eType == wkbMultiPolygon)
    {
        auto poMP = poGeom->toMultiPolygon();
        int nNumGeometries = poMP->getNumGeometries();
        for (int i = 0; i < nNumGeometries; /* incremented in loop */)
        {
            auto poPoly = poMP->getGeometryRef(i);
            RemoveZeroWidthSlivers(poPoly);
            if (poPoly->IsEmpty())
            {
                CPLDebug("WARP",
                         "RemoveZeroWidthSlivers: removing empty polygon");
                poMP->removeGeometry(i, /* bDelete = */ true);
                --nNumGeometries;
            }
            else
            {
                ++i;
            }
        }
    }
    else if (eType == wkbPolygon)
    {
        auto poPoly = poGeom->toPolygon();
        if (auto poExteriorRing = poPoly->getExteriorRing())
        {
            RemoveZeroWidthSlivers(poExteriorRing);
            if (poExteriorRing->getNumPoints() < 4)
            {
                poPoly->empty();
                return;
            }
        }
        int nNumInteriorRings = poPoly->getNumInteriorRings();
        for (int i = 0; i < nNumInteriorRings; /* incremented in loop */)
        {
            auto poRing = poPoly->getInteriorRing(i);
            RemoveZeroWidthSlivers(poRing);
            if (poRing->getNumPoints() < 4)
            {
                CPLDebug(
                    "WARP",
                    "RemoveZeroWidthSlivers: removing empty interior ring");
                constexpr int OFFSET_EXTERIOR_RING = 1;
                poPoly->removeRing(i + OFFSET_EXTERIOR_RING,
                                   /* bDelete = */ true);
                --nNumInteriorRings;
            }
            else
            {
                ++i;
            }
        }
    }
    else if (eType == wkbLineString)
    {
        OGRLineString *poLS = poGeom->toLineString();
        int numPoints = poLS->getNumPoints();
        for (int i = 1; i < numPoints - 1;)
        {
            const double x1 = poLS->getX(i - 1);
            const double y1 = poLS->getY(i - 1);
            const double x2 = poLS->getX(i);
            const double y2 = poLS->getY(i);
            const double x3 = poLS->getX(i + 1);
            const double y3 = poLS->getY(i + 1);
            const double dx1 = x2 - x1;
            const double dy1 = y2 - y1;
            const double dx2 = x3 - x2;
            const double dy2 = y3 - y2;
            const double scalar_product = dx1 * dx2 + dy1 * dy2;
            const double square_scalar_product =
                scalar_product * scalar_product;
            const double square_norm1 = dx1 * dx1 + dy1 * dy1;
            const double square_norm2 = dx2 * dx2 + dy2 * dy2;
            const double square_norm1_mult_norm2 = square_norm1 * square_norm2;
            if (scalar_product < 0 &&
                fabs(square_scalar_product - square_norm1_mult_norm2) <=
                    1e-15 * square_norm1_mult_norm2)
            {
                CPLDebug("WARP",
                         "RemoveZeroWidthSlivers: removing point %.10g %.10g",
                         x2, y2);
                poLS->removePoint(i);
                numPoints--;
            }
            else
            {
                ++i;
            }
        }
    }
}

/************************************************************************/
/*                      TransformCutlineToSource()                      */
/*                                                                      */
/*      Transform cutline from its SRS to source pixel/line coordinates.*/
/************************************************************************/
static CPLErr TransformCutlineToSource(GDALDataset *poSrcDS,
                                       OGRGeometry *poCutline,
                                       char ***ppapszWarpOptions,
                                       CSLConstList papszTO_In)

{
    RemoveZeroWidthSlivers(poCutline);

    auto poMultiPolygon = std::unique_ptr<OGRGeometry>(poCutline->clone());

    /* -------------------------------------------------------------------- */
    /*      Checkout that if there's a cutline SRS, there's also a raster   */
    /*      one.                                                            */
    /* -------------------------------------------------------------------- */
    std::unique_ptr<OGRSpatialReference> poRasterSRS;
    const CPLString osProjection =
        GetSrcDSProjection(GDALDataset::ToHandle(poSrcDS), papszTO_In);
    if (!osProjection.empty())
    {
        poRasterSRS = std::make_unique<OGRSpatialReference>();
        poRasterSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        if (poRasterSRS->SetFromUserInput(osProjection) != OGRERR_NONE)
        {
            poRasterSRS.reset();
        }
    }

    std::unique_ptr<OGRSpatialReference> poDstSRS;
    const char *pszThisTargetSRS = CSLFetchNameValue(papszTO_In, "DST_SRS");
    if (pszThisTargetSRS)
    {
        poDstSRS = std::make_unique<OGRSpatialReference>();
        poDstSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        if (poDstSRS->SetFromUserInput(pszThisTargetSRS) != OGRERR_NONE)
        {
            return CE_Failure;
        }
    }
    else if (poRasterSRS)
    {
        poDstSRS.reset(poRasterSRS->Clone());
    }

    /* -------------------------------------------------------------------- */
    /*      Extract the cutline SRS.                                        */
    /* -------------------------------------------------------------------- */
    const OGRSpatialReference *poCutlineSRS =
        poMultiPolygon->getSpatialReference();

    /* -------------------------------------------------------------------- */
    /*      Detect if there's no transform at all involved, in which case   */
    /*      we can avoid densification.                                     */
    /* -------------------------------------------------------------------- */
    bool bMayNeedDensify = true;
    if (poRasterSRS && poCutlineSRS && poRasterSRS->IsSame(poCutlineSRS) &&
        poSrcDS->GetGCPCount() == 0 && !poSrcDS->GetMetadata("RPC") &&
        !poSrcDS->GetMetadata("GEOLOCATION") &&
        !CSLFetchNameValue(papszTO_In, "GEOLOC_ARRAY") &&
        !CSLFetchNameValue(papszTO_In, "SRC_GEOLOC_ARRAY"))
    {
        CPLStringList aosTOTmp(papszTO_In);
        aosTOTmp.SetNameValue("SRC_SRS", nullptr);
        aosTOTmp.SetNameValue("DST_SRS", nullptr);
        if (aosTOTmp.size() == 0)
        {
            bMayNeedDensify = false;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Compare source raster SRS and cutline SRS                       */
    /* -------------------------------------------------------------------- */
    if (poRasterSRS && poCutlineSRS)
    {
        /* OK, we will reproject */
    }
    else if (poRasterSRS && !poCutlineSRS)
    {
        CPLError(
            CE_Warning, CPLE_AppDefined,
            "the source raster dataset has a SRS, but the cutline features\n"
            "not.  We assume that the cutline coordinates are expressed in the "
            "destination SRS.\n"
            "If not, cutline results may be incorrect.");
    }
    else if (!poRasterSRS && poCutlineSRS)
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "the input vector layer has a SRS, but the source raster "
                 "dataset does not.\n"
                 "Cutline results may be incorrect.");
    }

    auto poCTCutlineToSrc = CreateCTCutlineToSrc(
        poRasterSRS.get(), poDstSRS.get(), poCutlineSRS, papszTO_In);

    CPLStringList aosTO(papszTO_In);

    if (pszThisTargetSRS && !osProjection.empty())
    {
        // Avoid any reprojection when using the GenImgProjTransformer
        aosTO.SetNameValue("DST_SRS", osProjection.c_str());
    }
    aosTO.SetNameValue("SRC_COORDINATE_EPOCH", nullptr);
    aosTO.SetNameValue("DST_COORDINATE_EPOCH", nullptr);
    aosTO.SetNameValue("COORDINATE_OPERATION", nullptr);

    /* -------------------------------------------------------------------- */
    /*      It may be unwise to let the mask geometry be re-wrapped by      */
    /*      the CENTER_LONG machinery as this can easily screw up world     */
    /*      spanning masks and invert the mask topology.                    */
    /* -------------------------------------------------------------------- */
    aosTO.SetNameValue("INSERT_CENTER_LONG", "FALSE");

    /* -------------------------------------------------------------------- */
    /*      Transform the geometry to pixel/line coordinates.               */
    /* -------------------------------------------------------------------- */
    /* The cutline transformer will *invert* the hSrcImageTransformer */
    /* so it will convert from the source SRS to the source pixel/line */
    /* coordinates */
    CutlineTransformer oTransformer(GDALCreateGenImgProjTransformer2(
        GDALDataset::ToHandle(poSrcDS), nullptr, aosTO.List()));

    if (oTransformer.hSrcImageTransformer == nullptr)
    {
        return CE_Failure;
    }

    // Some transforms like RPC can transform a valid geometry into an invalid
    // one if the node density of the input geometry isn't sufficient before
    // reprojection. So after an initial reprojection, we check that the
    // maximum length of a segment is no longer than 1 pixel, and if not,
    // we densify the input geometry before doing a new reprojection
    const double dfMaxLengthInSpatUnits =
        GetMaximumSegmentLength(poMultiPolygon.get());
    OGRErr eErr = OGRERR_NONE;
    if (poCTCutlineToSrc)
    {
        poMultiPolygon.reset(OGRGeometryFactory::transformWithOptions(
            poMultiPolygon.get(), poCTCutlineToSrc.get(), nullptr));
        if (!poMultiPolygon)
        {
            eErr = OGRERR_FAILURE;
            poMultiPolygon.reset(poCutline->clone());
            poMultiPolygon->transform(poCTCutlineToSrc.get());
        }
    }
    if (poMultiPolygon->transform(&oTransformer) != OGRERR_NONE)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "poMultiPolygon->transform(&oTransformer) failed at line %d",
                 __LINE__);
        eErr = OGRERR_FAILURE;
    }
    const double dfInitialMaxLengthInPixels =
        GetMaximumSegmentLength(poMultiPolygon.get());

    CPLPushErrorHandler(CPLQuietErrorHandler);
    const bool bWasValidInitially =
        ValidateCutline(poMultiPolygon.get(), false);
    CPLPopErrorHandler();
    if (!bWasValidInitially)
    {
        CPLDebug("WARP", "Cutline is not valid after initial reprojection");
        char *pszWKT = nullptr;
        poMultiPolygon->exportToWkt(&pszWKT);
        CPLDebug("GDALWARP", "WKT = \"%s\"", pszWKT ? pszWKT : "(null)");
        CPLFree(pszWKT);
    }

    bool bDensify = false;
    if (bMayNeedDensify && eErr == OGRERR_NONE &&
        dfInitialMaxLengthInPixels > 1.0)
    {
        const char *pszDensifyCutline =
            CPLGetConfigOption("GDALWARP_DENSIFY_CUTLINE", "YES");
        if (EQUAL(pszDensifyCutline, "ONLY_IF_INVALID"))
        {
            bDensify = (OGRGeometryFactory::haveGEOS() && !bWasValidInitially);
        }
        else if (CSLFetchNameValue(*ppapszWarpOptions, "CUTLINE_BLEND_DIST") !=
                     nullptr &&
                 CPLGetConfigOption("GDALWARP_DENSIFY_CUTLINE", nullptr) ==
                     nullptr)
        {
            // TODO: we should only emit this message if a
            // transform/reprojection will be actually done
            CPLDebug("WARP",
                     "Densification of cutline could perhaps be useful but as "
                     "CUTLINE_BLEND_DIST is used, this could be very slow. So "
                     "disabled "
                     "unless GDALWARP_DENSIFY_CUTLINE=YES is explicitly "
                     "specified as configuration option");
        }
        else
        {
            bDensify = CPLTestBool(pszDensifyCutline);
        }
    }
    if (bDensify)
    {
        CPLDebug("WARP",
                 "Cutline maximum segment size was %.0f pixel after "
                 "reprojection to source coordinates.",
                 dfInitialMaxLengthInPixels);

        // Densify and reproject with the aim of having a 1 pixel density
        double dfSegmentSize =
            dfMaxLengthInSpatUnits / dfInitialMaxLengthInPixels;
        const int MAX_ITERATIONS = 10;
        for (int i = 0; i < MAX_ITERATIONS; i++)
        {
            poMultiPolygon.reset(poCutline->clone());
            poMultiPolygon->segmentize(dfSegmentSize);
            if (i == MAX_ITERATIONS - 1)
            {
                char *pszWKT = nullptr;
                poMultiPolygon->exportToWkt(&pszWKT);
                CPLDebug("WARP",
                         "WKT of polygon after densification with segment size "
                         "= %f: %s",
                         dfSegmentSize, pszWKT);
                CPLFree(pszWKT);
            }
            eErr = OGRERR_NONE;
            if (poCTCutlineToSrc)
            {
                poMultiPolygon.reset(OGRGeometryFactory::transformWithOptions(
                    poMultiPolygon.get(), poCTCutlineToSrc.get(), nullptr));
                if (!poMultiPolygon)
                {
                    eErr = OGRERR_FAILURE;
                    break;
                }
            }
            if (poMultiPolygon->transform(&oTransformer) != OGRERR_NONE)
                eErr = OGRERR_FAILURE;
            if (eErr == OGRERR_NONE)
            {
                const double dfMaxLengthInPixels =
                    GetMaximumSegmentLength(poMultiPolygon.get());
                if (bWasValidInitially)
                {
                    // In some cases, the densification itself results in a
                    // reprojected invalid polygon due to the non-linearity of
                    // RPC DEM transformation, so in those cases, try a less
                    // dense cutline
                    CPLPushErrorHandler(CPLQuietErrorHandler);
                    const bool bIsValid =
                        ValidateCutline(poMultiPolygon.get(), false);
                    CPLPopErrorHandler();
                    if (!bIsValid)
                    {
                        if (i == MAX_ITERATIONS - 1)
                        {
                            char *pszWKT = nullptr;
                            poMultiPolygon->exportToWkt(&pszWKT);
                            CPLDebug("WARP",
                                     "After densification, cutline maximum "
                                     "segment size is now %.0f pixel, "
                                     "but cutline is invalid. %s",
                                     dfMaxLengthInPixels, pszWKT);
                            CPLFree(pszWKT);
                            break;
                        }
                        CPLDebug("WARP",
                                 "After densification, cutline maximum segment "
                                 "size is now %.0f pixel, "
                                 "but cutline is invalid. So trying a less "
                                 "dense cutline.",
                                 dfMaxLengthInPixels);
                        dfSegmentSize *= 2;
                        continue;
                    }
                }
                CPLDebug("WARP",
                         "After densification, cutline maximum segment size is "
                         "now %.0f pixel.",
                         dfMaxLengthInPixels);
            }
            break;
        }
    }

    if (eErr == OGRERR_FAILURE)
    {
        if (CPLTestBool(
                CPLGetConfigOption("GDALWARP_IGNORE_BAD_CUTLINE", "NO")))
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Cutline transformation failed");
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cutline transformation failed");
            return CE_Failure;
        }
    }
    else if (!ValidateCutline(poMultiPolygon.get(), true))
    {
        return CE_Failure;
    }

    // Optimization: if the cutline contains the footprint of the source
    // dataset, no need to use the cutline.
    if (OGRGeometryFactory::haveGEOS()
#ifdef DEBUG
        // Env var just for debugging purposes
        && !CPLTestBool(CPLGetConfigOption(
               "GDALWARP_SKIP_CUTLINE_CONTAINMENT_TEST", "NO"))
#endif
    )
    {
        const double dfCutlineBlendDist = CPLAtof(CSLFetchNameValueDef(
            *ppapszWarpOptions, "CUTLINE_BLEND_DIST", "0"));
        OGRLinearRing *poRing = new OGRLinearRing();
        poRing->addPoint(-dfCutlineBlendDist, -dfCutlineBlendDist);
        poRing->addPoint(-dfCutlineBlendDist,
                         dfCutlineBlendDist + poSrcDS->GetRasterYSize());
        poRing->addPoint(dfCutlineBlendDist + poSrcDS->GetRasterXSize(),
                         dfCutlineBlendDist + poSrcDS->GetRasterYSize());
        poRing->addPoint(dfCutlineBlendDist + poSrcDS->GetRasterXSize(),
                         -dfCutlineBlendDist);
        poRing->addPoint(-dfCutlineBlendDist, -dfCutlineBlendDist);
        OGRPolygon oSrcDSFootprint;
        oSrcDSFootprint.addRingDirectly(poRing);
        OGREnvelope sSrcDSEnvelope;
        oSrcDSFootprint.getEnvelope(&sSrcDSEnvelope);
        OGREnvelope sCutlineEnvelope;
        poMultiPolygon->getEnvelope(&sCutlineEnvelope);
        if (sCutlineEnvelope.Contains(sSrcDSEnvelope) &&
            poMultiPolygon->Contains(&oSrcDSFootprint))
        {
            CPLDebug("WARP", "Source dataset fully contained within cutline.");
            return CE_None;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Convert aggregate geometry into WKT.                            */
    /* -------------------------------------------------------------------- */
    char *pszWKT = nullptr;
    poMultiPolygon->exportToWkt(&pszWKT);
    // fprintf(stderr, "WKT = \"%s\"\n", pszWKT ? pszWKT : "(null)");

    *ppapszWarpOptions = CSLSetNameValue(*ppapszWarpOptions, "CUTLINE", pszWKT);
    CPLFree(pszWKT);
    return CE_None;
}

static void RemoveConflictingMetadata(GDALMajorObjectH hObj,
                                      CSLConstList papszSrcMetadata,
                                      const char *pszValueConflict)
{
    if (hObj == nullptr)
        return;

    for (const auto &[pszKey, pszValue] :
         cpl::IterateNameValue(papszSrcMetadata))
    {
        const char *pszValueComp = GDALGetMetadataItem(hObj, pszKey, nullptr);
        if (pszValueComp == nullptr || (!EQUAL(pszValue, pszValueComp) &&
                                        !EQUAL(pszValueComp, pszValueConflict)))
        {
            if (STARTS_WITH(pszKey, "STATISTICS_"))
                GDALSetMetadataItem(hObj, pszKey, nullptr, nullptr);
            else
                GDALSetMetadataItem(hObj, pszKey, pszValueConflict, nullptr);
        }
    }
}

/************************************************************************/
/*                             IsValidSRS                               */
/************************************************************************/

static bool IsValidSRS(const char *pszUserInput)

{
    OGRSpatialReferenceH hSRS;
    bool bRes = true;

    hSRS = OSRNewSpatialReference(nullptr);
    if (OSRSetFromUserInput(hSRS, pszUserInput) != OGRERR_NONE)
    {
        bRes = false;
    }

    OSRDestroySpatialReference(hSRS);

    return bRes;
}

/************************************************************************/
/*                     GDALWarpAppOptionsGetParser()                    */
/************************************************************************/

static std::unique_ptr<GDALArgumentParser>
GDALWarpAppOptionsGetParser(GDALWarpAppOptions *psOptions,
                            GDALWarpAppOptionsForBinary *psOptionsForBinary)
{
    auto argParser = std::make_unique<GDALArgumentParser>(
        "gdalwarp", /* bForBinary=*/psOptionsForBinary != nullptr);

    argParser->add_description(_("Image reprojection and warping utility."));

    argParser->add_epilog(
        _("For more details, consult https://gdal.org/programs/gdalwarp.html"));

    argParser->add_quiet_argument(
        psOptionsForBinary ? &psOptionsForBinary->bQuiet : nullptr);

    argParser->add_argument("-overwrite")
        .flag()
        .action(
            [psOptionsForBinary](const std::string &)
            {
                if (psOptionsForBinary)
                    psOptionsForBinary->bOverwrite = true;
            })
        .help(_("Overwrite the target dataset if it already exists."));

    argParser->add_output_format_argument(psOptions->osFormat);

    argParser->add_argument("-co")
        .metavar("<NAME>=<VALUE>")
        .append()
        .action(
            [psOptions, psOptionsForBinary](const std::string &s)
            {
                psOptions->aosCreateOptions.AddString(s.c_str());
                psOptions->bCreateOutput = true;

                if (psOptionsForBinary)
                    psOptionsForBinary->aosCreateOptions.AddString(s.c_str());
            })
        .help(_("Creation option(s)."));

    argParser->add_argument("-s_srs")
        .metavar("<srs_def>")
        .action(
            [psOptions](const std::string &s)
            {
                if (!IsValidSRS(s.c_str()))
                {
                    throw std::invalid_argument("Invalid SRS for -s_srs");
                }
                psOptions->aosTransformerOptions.SetNameValue("SRC_SRS",
                                                              s.c_str());
            })
        .help(_("Set source spatial reference."));

    argParser->add_argument("-t_srs")
        .metavar("<srs_def>")
        .action(
            [psOptions](const std::string &s)
            {
                if (!IsValidSRS(s.c_str()))
                {
                    throw std::invalid_argument("Invalid SRS for -t_srs");
                }
                psOptions->aosTransformerOptions.SetNameValue("DST_SRS",
                                                              s.c_str());
            })
        .help(_("Set target spatial reference."));

    {
        auto &group = argParser->add_mutually_exclusive_group();
        group.add_argument("-srcalpha")
            .flag()
            .store_into(psOptions->bEnableSrcAlpha)
            .help(_("Force the last band of a source image to be considered as "
                    "a source alpha band."));
        group.add_argument("-nosrcalpha")
            .flag()
            .store_into(psOptions->bDisableSrcAlpha)
            .help(_("Prevent the alpha band of a source image to be considered "
                    "as such."));
    }

    argParser->add_argument("-dstalpha")
        .flag()
        .store_into(psOptions->bEnableDstAlpha)
        .help(_("Create an output alpha band to identify nodata "
                "(unset/transparent) pixels."));

    // Parsing of that option is done in a preprocessing stage
    argParser->add_argument("-tr")
        .metavar("<xres> <yres>|square")
        .help(_("Target resolution."));

    argParser->add_argument("-ts")
        .metavar("<width> <height>")
        .nargs(2)
        .scan<'i', int>()
        .help(_("Set output file size in pixels and lines."));

    argParser->add_argument("-te")
        .metavar("<xmin> <ymin> <xmax> <ymax>")
        .nargs(4)
        .scan<'g', double>()
        .help(_("Set georeferenced extents of output file to be created."));

    argParser->add_argument("-te_srs")
        .metavar("<srs_def>")
        .action(
            [psOptions](const std::string &s)
            {
                if (!IsValidSRS(s.c_str()))
                {
                    throw std::invalid_argument("Invalid SRS for -te_srs");
                }
                psOptions->osTE_SRS = s;
                psOptions->bCreateOutput = true;
            })
        .help(_("Set source spatial reference."));

    argParser->add_argument("-r")
        .metavar("near|bilinear|cubic|cubicspline|lanczos|average|rms|mode|min|"
                 "max|med|q1|q3|sum")
        .action(
            [psOptions](const std::string &s)
            {
                GetResampleAlg(s.c_str(), psOptions->eResampleAlg,
                               /*bThrow=*/true);
                psOptions->bResampleAlgSpecifiedByUser = true;
            })
        .help(_("Resampling method to use."));

    argParser->add_output_type_argument(psOptions->eOutputType);

    ///////////////////////////////////////////////////////////////////////
    argParser->add_group("Advanced options");

    const auto CheckSingleMethod = [psOptions]()
    {
        const char *pszMethod =
            psOptions->aosTransformerOptions.FetchNameValue("METHOD");
        if (pszMethod)
            CPLError(CE_Warning, CPLE_IllegalArg,
                     "Warning: only one METHOD can be used. Method %s is "
                     "already defined.",
                     pszMethod);
        const char *pszMAX_GCP_ORDER =
            psOptions->aosTransformerOptions.FetchNameValue("MAX_GCP_ORDER");
        if (pszMAX_GCP_ORDER)
            CPLError(CE_Warning, CPLE_IllegalArg,
                     "Warning: only one METHOD can be used. -order %s "
                     "option was specified, so it is likely that "
                     "GCP_POLYNOMIAL was implied.",
                     pszMAX_GCP_ORDER);
    };

    argParser->add_argument("-wo")
        .metavar("<NAME>=<VALUE>")
        .append()
        .action([psOptions](const std::string &s)
                { psOptions->aosWarpOptions.AddString(s.c_str()); })
        .help(_("Warping option(s)."));

    argParser->add_argument("-multi")
        .flag()
        .store_into(psOptions->bMulti)
        .help(_("Multithreaded input/output."));

    argParser->add_argument("-s_coord_epoch")
        .metavar("<epoch>")
        .action(
            [psOptions](const std::string &s)
            {
                psOptions->aosTransformerOptions.SetNameValue(
                    "SRC_COORDINATE_EPOCH", s.c_str());
            })
        .help(_("Assign a coordinate epoch, linked with the source SRS when "
                "-s_srs is used."));

    argParser->add_argument("-t_coord_epoch")
        .metavar("<epoch>")
        .action(
            [psOptions](const std::string &s)
            {
                psOptions->aosTransformerOptions.SetNameValue(
                    "DST_COORDINATE_EPOCH", s.c_str());
            })
        .help(_("Assign a coordinate epoch, linked with the output SRS when "
                "-t_srs is used."));

    argParser->add_argument("-ct")
        .metavar("<string>")
        .action(
            [psOptions](const std::string &s)
            {
                psOptions->aosTransformerOptions.SetNameValue(
                    "COORDINATE_OPERATION", s.c_str());
            })
        .help(_("Set a coordinate transformation."));

    {
        auto &group = argParser->add_mutually_exclusive_group();
        group.add_argument("-tps")
            .flag()
            .action(
                [psOptions, CheckSingleMethod](const std::string &)
                {
                    CheckSingleMethod();
                    psOptions->aosTransformerOptions.SetNameValue("METHOD",
                                                                  "GCP_TPS");
                })
            .help(_("Force use of thin plate spline transformer based on "
                    "available GCPs."));

        group.add_argument("-rpc")
            .flag()
            .action(
                [psOptions, CheckSingleMethod](const std::string &)
                {
                    CheckSingleMethod();
                    psOptions->aosTransformerOptions.SetNameValue("METHOD",
                                                                  "RPC");
                })
            .help(_("Force use of RPCs."));

        group.add_argument("-geoloc")
            .flag()
            .action(
                [psOptions, CheckSingleMethod](const std::string &)
                {
                    CheckSingleMethod();
                    psOptions->aosTransformerOptions.SetNameValue(
                        "METHOD", "GEOLOC_ARRAY");
                })
            .help(_("Force use of Geolocation Arrays."));
    }

    argParser->add_argument("-order")
        .metavar("<1|2|3>")
        .choices("1", "2", "3")
        .action(
            [psOptions](const std::string &s)
            {
                const char *pszMethod =
                    psOptions->aosTransformerOptions.FetchNameValue("METHOD");
                if (pszMethod)
                    CPLError(
                        CE_Warning, CPLE_IllegalArg,
                        "Warning: only one METHOD can be used. Method %s is "
                        "already defined",
                        pszMethod);
                psOptions->aosTransformerOptions.SetNameValue("MAX_GCP_ORDER",
                                                              s.c_str());
            })
        .help(_("Order of polynomial used for GCP warping."));

    // Parsing of that option is done in a preprocessing stage
    argParser->add_argument("-refine_gcps")
        .metavar("<tolerance> [<minimum_gcps>]")
        .help(_("Refines the GCPs by automatically eliminating outliers."));

    argParser->add_argument("-to")
        .metavar("<NAME>=<VALUE>")
        .append()
        .action([psOptions](const std::string &s)
                { psOptions->aosTransformerOptions.AddString(s.c_str()); })
        .help(_("Transform option(s)."));

    argParser->add_argument("-et")
        .metavar("<err_threshold>")
        .action(
            [psOptions](const std::string &s)
            {
                psOptions->dfErrorThreshold = CPLAtofM(s.c_str());
                psOptions->aosWarpOptions.AddString(CPLSPrintf(
                    "ERROR_THRESHOLD=%.16g", psOptions->dfErrorThreshold));
            })
        .help(_("Error threshold."));

    argParser->add_argument("-wm")
        .metavar("<memory_in_mb>")
        .action(
            [psOptions](const std::string &s)
            {
                if (CPLAtofM(s.c_str()) < 10000)
                    psOptions->dfWarpMemoryLimit =
                        CPLAtofM(s.c_str()) * 1024 * 1024;
                else
                    psOptions->dfWarpMemoryLimit = CPLAtofM(s.c_str());
            })
        .help(_("Set max warp memory."));

    argParser->add_argument("-srcnodata")
        .metavar("\"<value>[ <value>]...\"")
        .store_into(psOptions->osSrcNodata)
        .help(_("Nodata masking values for input bands."));

    argParser->add_argument("-dstnodata")
        .metavar("\"<value>[ <value>]...\"")
        .store_into(psOptions->osDstNodata)
        .help(_("Nodata masking values for output bands."));

    argParser->add_argument("-tap")
        .flag()
        .store_into(psOptions->bTargetAlignedPixels)
        .help(_("Force target aligned pixels."));

    argParser->add_argument("-wt")
        .metavar("Byte|Int8|[U]Int{16|32|64}|CInt{16|32}|[C]Float{32|64}")
        .action(
            [psOptions](const std::string &s)
            {
                psOptions->eWorkingType = GDALGetDataTypeByName(s.c_str());
                if (psOptions->eWorkingType == GDT_Unknown)
                {
                    throw std::invalid_argument(
                        std::string("Unknown output pixel type: ").append(s));
                }
            })
        .help(_("Working data type."));

    // Non-documented alias of -r nearest
    argParser->add_argument("-rn")
        .flag()
        .hidden()
        .action([psOptions](const std::string &)
                { psOptions->eResampleAlg = GRA_NearestNeighbour; })
        .help(_("Nearest neighbour resampling."));

    // Non-documented alias of -r bilinear
    argParser->add_argument("-rb")
        .flag()
        .hidden()
        .action([psOptions](const std::string &)
                { psOptions->eResampleAlg = GRA_Bilinear; })
        .help(_("Bilinear resampling."));

    // Non-documented alias of -r cubic
    argParser->add_argument("-rc")
        .flag()
        .hidden()
        .action([psOptions](const std::string &)
                { psOptions->eResampleAlg = GRA_Cubic; })
        .help(_("Cubic resampling."));

    // Non-documented alias of -r cubicspline
    argParser->add_argument("-rcs")
        .flag()
        .hidden()
        .action([psOptions](const std::string &)
                { psOptions->eResampleAlg = GRA_CubicSpline; })
        .help(_("Cubic spline resampling."));

    // Non-documented alias of -r lanczos
    argParser->add_argument("-rl")
        .flag()
        .hidden()
        .action([psOptions](const std::string &)
                { psOptions->eResampleAlg = GRA_Lanczos; })
        .help(_("Lanczos resampling."));

    // Non-documented alias of -r average
    argParser->add_argument("-ra")
        .flag()
        .hidden()
        .action([psOptions](const std::string &)
                { psOptions->eResampleAlg = GRA_Average; })
        .help(_("Average resampling."));

    // Non-documented alias of -r rms
    argParser->add_argument("-rrms")
        .flag()
        .hidden()
        .action([psOptions](const std::string &)
                { psOptions->eResampleAlg = GRA_RMS; })
        .help(_("RMS resampling."));

    // Non-documented alias of -r mode
    argParser->add_argument("-rm")
        .flag()
        .hidden()
        .action([psOptions](const std::string &)
                { psOptions->eResampleAlg = GRA_Mode; })
        .help(_("Mode resampling."));

    argParser->add_argument("-cutline")
        .metavar("<datasource>|<WKT>")
        .store_into(psOptions->osCutlineDSNameOrWKT)
        .help(_("Enable use of a blend cutline from the name of a vector "
                "dataset or a WKT geometry."));

    argParser->add_argument("-cutline_srs")
        .metavar("<srs_def>")
        .action(
            [psOptions](const std::string &s)
            {
                if (!IsValidSRS(s.c_str()))
                {
                    throw std::invalid_argument("Invalid SRS for -cutline_srs");
                }
                psOptions->osCutlineSRS = s;
            })
        .help(_("Sets/overrides cutline SRS."));

    argParser->add_argument("-cwhere")
        .metavar("<expression>")
        .store_into(psOptions->osCWHERE)
        .help(_("Restrict desired cutline features based on attribute query."));

    {
        auto &group = argParser->add_mutually_exclusive_group();
        group.add_argument("-cl")
            .metavar("<layername>")
            .store_into(psOptions->osCLayer)
            .help(_("Select the named layer from the cutline datasource."));

        group.add_argument("-csql")
            .metavar("<query>")
            .store_into(psOptions->osCSQL)
            .help(_("Select cutline features using an SQL query."));
    }

    argParser->add_argument("-cblend")
        .metavar("<distance>")
        .action(
            [psOptions](const std::string &s) {
                psOptions->aosWarpOptions.SetNameValue("CUTLINE_BLEND_DIST",
                                                       s.c_str());
            })
        .help(_(
            "Set a blend distance to use to blend over cutlines (in pixels)."));

    argParser->add_argument("-crop_to_cutline")
        .flag()
        .action(
            [psOptions](const std::string &)
            {
                psOptions->bCropToCutline = true;
                psOptions->bCreateOutput = true;
            })
        .help(_("Crop the extent of the target dataset to the extent of the "
                "cutline."));

    argParser->add_argument("-nomd")
        .flag()
        .action(
            [psOptions](const std::string &)
            {
                psOptions->bCopyMetadata = false;
                psOptions->bCopyBandInfo = false;
            })
        .help(_("Do not copy metadata."));

    argParser->add_argument("-cvmd")
        .metavar("<meta_conflict_value>")
        .store_into(psOptions->osMDConflictValue)
        .help(_("Value to set metadata items that conflict between source "
                "datasets."));

    argParser->add_argument("-setci")
        .flag()
        .store_into(psOptions->bSetColorInterpretation)
        .help(_("Set the color interpretation of the bands of the target "
                "dataset from the source dataset."));

    argParser->add_open_options_argument(
        psOptionsForBinary ? &(psOptionsForBinary->aosOpenOptions) : nullptr);

    argParser->add_argument("-doo")
        .metavar("<NAME>=<VALUE>")
        .append()
        .action(
            [psOptionsForBinary](const std::string &s)
            {
                if (psOptionsForBinary)
                    psOptionsForBinary->aosDestOpenOptions.AddString(s.c_str());
            })
        .help(_("Open option(s) for output dataset."));

    argParser->add_argument("-ovr")
        .metavar("<level>|AUTO|AUTO-<n>|NONE")
        .action(
            [psOptions](const std::string &s)
            {
                const char *pszOvLevel = s.c_str();
                if (EQUAL(pszOvLevel, "AUTO"))
                    psOptions->nOvLevel = OVR_LEVEL_AUTO;
                else if (STARTS_WITH_CI(pszOvLevel, "AUTO-"))
                    psOptions->nOvLevel =
                        OVR_LEVEL_AUTO - atoi(pszOvLevel + strlen("AUTO-"));
                else if (EQUAL(pszOvLevel, "NONE"))
                    psOptions->nOvLevel = OVR_LEVEL_NONE;
                else if (CPLGetValueType(pszOvLevel) == CPL_VALUE_INTEGER)
                    psOptions->nOvLevel = atoi(pszOvLevel);
                else
                {
                    throw std::invalid_argument(CPLSPrintf(
                        "Invalid value '%s' for -ov option", pszOvLevel));
                }
            })
        .help(_("Specify which overview level of source files must be used."));

    {
        auto &group = argParser->add_mutually_exclusive_group();
        group.add_argument("-vshift")
            .flag()
            .store_into(psOptions->bVShift)
            .help(_("Force the use of vertical shift."));
        group.add_argument("-novshift", "-novshiftgrid")
            .flag()
            .store_into(psOptions->bNoVShift)
            .help(_("Disable the use of vertical shift."));
    }

    argParser->add_input_format_argument(
        psOptionsForBinary ? &psOptionsForBinary->aosAllowedInputDrivers
                           : nullptr);

    argParser->add_argument("-b", "-srcband")
        .metavar("<band>")
        .append()
        .store_into(psOptions->anSrcBands)
        .help(_("Specify input band(s) number to warp."));

    argParser->add_argument("-dstband")
        .metavar("<band>")
        .append()
        .store_into(psOptions->anDstBands)
        .help(_("Specify the output band number in which to warp."));

    if (psOptionsForBinary)
    {
        argParser->add_argument("src_dataset_name")
            .metavar("<src_dataset_name>")
            .nargs(argparse::nargs_pattern::at_least_one)
            .action([psOptionsForBinary](const std::string &s)
                    { psOptionsForBinary->aosSrcFiles.AddString(s.c_str()); })
            .help(_("Input dataset(s)."));

        argParser->add_argument("dst_dataset_name")
            .metavar("<dst_dataset_name>")
            .store_into(psOptionsForBinary->osDstFilename)
            .help(_("Output dataset."));
    }

    return argParser;
}

/************************************************************************/
/*                       GDALWarpAppGetParserUsage()                    */
/************************************************************************/

std::string GDALWarpAppGetParserUsage()
{
    try
    {
        GDALWarpAppOptions sOptions;
        GDALWarpAppOptionsForBinary sOptionsForBinary;
        auto argParser =
            GDALWarpAppOptionsGetParser(&sOptions, &sOptionsForBinary);
        return argParser->usage();
    }
    catch (const std::exception &err)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Unexpected exception: %s",
                 err.what());
        return std::string();
    }
}

/************************************************************************/
/*                             GDALWarpAppOptionsNew()                  */
/************************************************************************/

#ifndef CheckHasEnoughAdditionalArgs_defined
#define CheckHasEnoughAdditionalArgs_defined

static bool CheckHasEnoughAdditionalArgs(CSLConstList papszArgv, int i,
                                         int nExtraArg, int nArgc)
{
    if (i + nExtraArg >= nArgc)
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "%s option requires %d argument%s", papszArgv[i], nExtraArg,
                 nExtraArg == 1 ? "" : "s");
        return false;
    }
    return true;
}
#endif

#define CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(nExtraArg)                            \
    if (!CheckHasEnoughAdditionalArgs(papszArgv, i, nExtraArg, nArgc))         \
    {                                                                          \
        return nullptr;                                                        \
    }

/**
 * Allocates a GDALWarpAppOptions struct.
 *
 * @param papszArgv NULL terminated list of options (potentially including
 * filename and open options too), or NULL. The accepted options are the ones of
 * the <a href="/programs/gdalwarp.html">gdalwarp</a> utility.
 * @param psOptionsForBinary (output) may be NULL (and should generally be
 * NULL), otherwise (gdal_translate_bin.cpp use case) must be allocated with
 *                           GDALWarpAppOptionsForBinaryNew() prior to this
 * function. Will be filled with potentially present filename, open options,...
 * @return pointer to the allocated GDALWarpAppOptions struct. Must be freed
 * with GDALWarpAppOptionsFree().
 *
 * @since GDAL 2.1
 */

GDALWarpAppOptions *
GDALWarpAppOptionsNew(char **papszArgv,
                      GDALWarpAppOptionsForBinary *psOptionsForBinary)
{
    auto psOptions = std::make_unique<GDALWarpAppOptions>();

    /* -------------------------------------------------------------------- */
    /*      Pre-processing for custom syntax that ArgumentParser does not   */
    /*      support.                                                        */
    /* -------------------------------------------------------------------- */

    CPLStringList aosArgv;
    const int nArgc = CSLCount(papszArgv);
    for (int i = 0;
         i < nArgc && papszArgv != nullptr && papszArgv[i] != nullptr; i++)
    {
        if (EQUAL(papszArgv[i], "-refine_gcps"))
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            psOptions->aosTransformerOptions.SetNameValue("REFINE_TOLERANCE",
                                                          papszArgv[++i]);
            if (CPLAtof(papszArgv[i]) < 0)
            {
                CPLError(CE_Failure, CPLE_IllegalArg,
                         "The tolerance for -refine_gcps may not be negative.");
                return nullptr;
            }
            if (i < nArgc - 1 && atoi(papszArgv[i + 1]) >= 0 &&
                isdigit(static_cast<unsigned char>(papszArgv[i + 1][0])))
            {
                psOptions->aosTransformerOptions.SetNameValue(
                    "REFINE_MINIMUM_GCPS", papszArgv[++i]);
            }
            else
            {
                psOptions->aosTransformerOptions.SetNameValue(
                    "REFINE_MINIMUM_GCPS", "-1");
            }
        }
        else if (EQUAL(papszArgv[i], "-tr") && i + 1 < nArgc &&
                 EQUAL(papszArgv[i + 1], "square"))
        {
            ++i;
            psOptions->bSquarePixels = true;
            psOptions->bCreateOutput = true;
        }
        else if (EQUAL(papszArgv[i], "-tr"))
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(2);
            psOptions->dfXRes = CPLAtofM(papszArgv[++i]);
            psOptions->dfYRes = fabs(CPLAtofM(papszArgv[++i]));
            if (psOptions->dfXRes == 0 || psOptions->dfYRes == 0)
            {
                CPLError(CE_Failure, CPLE_IllegalArg,
                         "Wrong value for -tr parameters.");
                return nullptr;
            }
            psOptions->bCreateOutput = true;
        }
        // argparser will be confused if the value of a string argument
        // starts with a negative sign.
        else if (EQUAL(papszArgv[i], "-srcnodata") && i + 1 < nArgc)
        {
            ++i;
            psOptions->osSrcNodata = papszArgv[i];
        }
        // argparser will be confused if the value of a string argument
        // starts with a negative sign.
        else if (EQUAL(papszArgv[i], "-dstnodata") && i + 1 < nArgc)
        {
            ++i;
            psOptions->osDstNodata = papszArgv[i];
        }
        else
        {
            aosArgv.AddString(papszArgv[i]);
        }
    }

    try
    {
        auto argParser =
            GDALWarpAppOptionsGetParser(psOptions.get(), psOptionsForBinary);

        argParser->parse_args_without_binary_name(aosArgv.List());

        if (auto oTS = argParser->present<std::vector<int>>("-ts"))
        {
            psOptions->nForcePixels = (*oTS)[0];
            psOptions->nForceLines = (*oTS)[1];
            psOptions->bCreateOutput = true;
        }

        if (auto oTE = argParser->present<std::vector<double>>("-te"))
        {
            psOptions->dfMinX = (*oTE)[0];
            psOptions->dfMinY = (*oTE)[1];
            psOptions->dfMaxX = (*oTE)[2];
            psOptions->dfMaxY = (*oTE)[3];
            psOptions->bCreateOutput = true;
        }

        if (!psOptions->anDstBands.empty() &&
            psOptions->anSrcBands.size() != psOptions->anDstBands.size())
        {
            CPLError(
                CE_Failure, CPLE_IllegalArg,
                "-srcband should be specified as many times as -dstband is");
            return nullptr;
        }
        else if (!psOptions->anSrcBands.empty() &&
                 psOptions->anDstBands.empty())
        {
            for (int i = 0; i < static_cast<int>(psOptions->anSrcBands.size());
                 ++i)
            {
                psOptions->anDstBands.push_back(i + 1);
            }
        }

        if (!psOptions->osFormat.empty() ||
            psOptions->eOutputType != GDT_Unknown)
        {
            psOptions->bCreateOutput = true;
        }

        if (psOptionsForBinary)
            psOptionsForBinary->bCreateOutput = psOptions->bCreateOutput;

        return psOptions.release();
    }
    catch (const std::exception &err)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s", err.what());
        return nullptr;
    }
}

/************************************************************************/
/*                            GetResampleAlg()                          */
/************************************************************************/

static bool GetResampleAlg(const char *pszResampling,
                           GDALResampleAlg &eResampleAlg, bool bThrow)
{
    if (STARTS_WITH_CI(pszResampling, "near"))
        eResampleAlg = GRA_NearestNeighbour;
    else if (EQUAL(pszResampling, "bilinear"))
        eResampleAlg = GRA_Bilinear;
    else if (EQUAL(pszResampling, "cubic"))
        eResampleAlg = GRA_Cubic;
    else if (EQUAL(pszResampling, "cubicspline"))
        eResampleAlg = GRA_CubicSpline;
    else if (EQUAL(pszResampling, "lanczos"))
        eResampleAlg = GRA_Lanczos;
    else if (EQUAL(pszResampling, "average"))
        eResampleAlg = GRA_Average;
    else if (EQUAL(pszResampling, "rms"))
        eResampleAlg = GRA_RMS;
    else if (EQUAL(pszResampling, "mode"))
        eResampleAlg = GRA_Mode;
    else if (EQUAL(pszResampling, "max"))
        eResampleAlg = GRA_Max;
    else if (EQUAL(pszResampling, "min"))
        eResampleAlg = GRA_Min;
    else if (EQUAL(pszResampling, "med"))
        eResampleAlg = GRA_Med;
    else if (EQUAL(pszResampling, "q1"))
        eResampleAlg = GRA_Q1;
    else if (EQUAL(pszResampling, "q3"))
        eResampleAlg = GRA_Q3;
    else if (EQUAL(pszResampling, "sum"))
        eResampleAlg = GRA_Sum;
    else
    {
        if (bThrow)
        {
            throw std::invalid_argument("Unknown resampling method");
        }
        else
        {
            CPLError(CE_Failure, CPLE_IllegalArg,
                     "Unknown resampling method: %s.", pszResampling);
            return false;
        }
    }
    return true;
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

void GDALWarpAppOptionsFree(GDALWarpAppOptions *psOptions)
{
    delete psOptions;
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

void GDALWarpAppOptionsSetProgress(GDALWarpAppOptions *psOptions,
                                   GDALProgressFunc pfnProgress,
                                   void *pProgressData)
{
    psOptions->pfnProgress = pfnProgress ? pfnProgress : GDALDummyProgress;
    psOptions->pProgressData = pProgressData;
    if (pfnProgress == GDALTermProgress)
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

void GDALWarpAppOptionsSetQuiet(GDALWarpAppOptions *psOptions, int bQuiet)
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

void GDALWarpAppOptionsSetWarpOption(GDALWarpAppOptions *psOptions,
                                     const char *pszKey, const char *pszValue)
{
    psOptions->aosWarpOptions.SetNameValue(pszKey, pszValue);
}

#undef CHECK_HAS_ENOUGH_ADDITIONAL_ARGS
