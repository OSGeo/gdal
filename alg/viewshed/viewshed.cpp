/******************************************************************************
 *
 * Project:  Viewshed Generation
 * Purpose:  Core algorithm implementation for viewshed generation.
 * Author:   Tamas Szekeres, szekerest@gmail.com
 *
 * (c) 2024 info@hobu.co
 *
 ******************************************************************************
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include <algorithm>
#include <array>

#include "gdal_alg.h"
#include "gdal_priv_templates.hpp"

#include "progress.h"
#include "util.h"
#include "viewshed.h"
#include "viewshed_executor.h"

/************************************************************************/
/*                        GDALViewshedGenerate()                        */
/************************************************************************/

/**
 * Create viewshed from raster DEM.
 *
 * This algorithm will generate a viewshed raster from an input DEM raster
 * by using a modified algorithm of "Generating Viewsheds without Using
 * Sightlines" published at
 * https://www.asprs.org/wp-content/uploads/pers/2000journal/january/2000_jan_87-90.pdf
 * This approach provides a relatively fast calculation, since the output raster
 * is generated in a single scan. The gdal/apps/gdal_viewshed.cpp mainline can
 * be used as an example of how to use this function. The output raster will be
 * of type Byte or Float64.
 *
 * \note The algorithm as implemented currently will only output meaningful
 * results if the georeferencing is in a projected coordinate reference system.
 *
 * @param hBand The band to read the DEM data from. Only the part of the raster
 * within the specified maxdistance around the observer point is processed.
 *
 * @param pszDriverName Driver name (GTiff if set to NULL)
 *
 * @param pszTargetRasterName The name of the target raster to be generated.
 * Must not be NULL
 *
 * @param papszCreationOptions creation options.
 *
 * @param dfObserverX observer X value (in SRS units)
 *
 * @param dfObserverY observer Y value (in SRS units)
 *
 * @param dfObserverHeight The height of the observer above the DEM surface.
 *
 * @param dfTargetHeight The height of the target above the DEM surface.
 * (default 0)
 *
 * @param dfVisibleVal pixel value for visibility (default 255)
 *
 * @param dfInvisibleVal pixel value for invisibility (default 0)
 *
 * @param dfOutOfRangeVal The value to be set for the cells that fall outside of
 * the range specified by dfMaxDistance.
 *
 * @param dfNoDataVal The value to be set for the cells that have no data.
 *                    If set to a negative value, nodata is not set.
 *                    Note: currently, no special processing of input cells at a
 * nodata value is done (which may result in erroneous results).
 *
 * @param dfCurvCoeff Coefficient to consider the effect of the curvature and
 * refraction. The height of the DEM is corrected according to the following
 * formula: [Height] -= dfCurvCoeff * [Target Distance]^2 / [Earth Diameter] For
 * the effect of the atmospheric refraction we can use 0.85714.
 *
 * @param eMode The mode of the viewshed calculation.
 * Possible values GVM_Diagonal = 1, GVM_Edge = 2 (default), GVM_Max = 3,
 * GVM_Min = 4.
 *
 * @param dfMaxDistance maximum distance range to compute viewshed.
 *                      It is also used to clamp the extent of the output
 * raster. If set to 0, then unlimited range is assumed, that is to say the
 *                      computation is performed on the extent of the whole
 * raster.
 *
 * @param pfnProgress A GDALProgressFunc that may be used to report progress
 * to the user, or to interrupt the algorithm.  May be NULL if not required.
 *
 * @param pProgressArg The callback data for the pfnProgress function.
 *
 * @param heightMode Type of information contained in output raster. Possible
 * values GVOT_NORMAL = 1 (default), GVOT_MIN_TARGET_HEIGHT_FROM_DEM = 2,
 *                   GVOT_MIN_TARGET_HEIGHT_FROM_GROUND = 3
 *
 *                   GVOT_NORMAL returns a raster of type Byte containing
 * visible locations.
 *
 *                   GVOT_MIN_TARGET_HEIGHT_FROM_DEM and
 * GVOT_MIN_TARGET_HEIGHT_FROM_GROUND will return a raster of type Float64
 * containing the minimum target height for target to be visible from the DEM
 * surface or ground level respectively. Parameters dfTargetHeight, dfVisibleVal
 * and dfInvisibleVal will be ignored.
 *
 *
 * @param papszExtraOptions Future extra options. Must be set to NULL currently.
 *
 * @return not NULL output dataset on success (to be closed with GDALClose()) or
 * NULL if an error occurs.
 *
 * @since GDAL 3.1
 */
GDALDatasetH GDALViewshedGenerate(
    GDALRasterBandH hBand, const char *pszDriverName,
    const char *pszTargetRasterName, CSLConstList papszCreationOptions,
    double dfObserverX, double dfObserverY, double dfObserverHeight,
    double dfTargetHeight, double dfVisibleVal, double dfInvisibleVal,
    double dfOutOfRangeVal, double dfNoDataVal, double dfCurvCoeff,
    GDALViewshedMode eMode, double dfMaxDistance, GDALProgressFunc pfnProgress,
    void *pProgressArg, GDALViewshedOutputType heightMode,
    [[maybe_unused]] CSLConstList papszExtraOptions)
{
    using namespace gdal;

    viewshed::Options oOpts;
    oOpts.outputFormat = pszDriverName;
    oOpts.outputFilename = pszTargetRasterName;
    oOpts.creationOpts = papszCreationOptions;
    oOpts.observer.x = dfObserverX;
    oOpts.observer.y = dfObserverY;
    oOpts.observer.z = dfObserverHeight;
    oOpts.targetHeight = dfTargetHeight;
    oOpts.curveCoeff = dfCurvCoeff;
    oOpts.maxDistance = dfMaxDistance;
    oOpts.nodataVal = dfNoDataVal;

    switch (eMode)
    {
        case GVM_Edge:
            oOpts.cellMode = viewshed::CellMode::Edge;
            break;
        case GVM_Diagonal:
            oOpts.cellMode = viewshed::CellMode::Diagonal;
            break;
        case GVM_Min:
            oOpts.cellMode = viewshed::CellMode::Min;
            break;
        case GVM_Max:
            oOpts.cellMode = viewshed::CellMode::Max;
            break;
    }

    switch (heightMode)
    {
        case GVOT_MIN_TARGET_HEIGHT_FROM_DEM:
            oOpts.outputMode = viewshed::OutputMode::DEM;
            break;
        case GVOT_MIN_TARGET_HEIGHT_FROM_GROUND:
            oOpts.outputMode = viewshed::OutputMode::Ground;
            break;
        case GVOT_NORMAL:
            oOpts.outputMode = viewshed::OutputMode::Normal;
            break;
    }

    if (!GDALIsValueInRange<uint8_t>(dfVisibleVal))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "dfVisibleVal out of range. Must be [0, 255].");
        return nullptr;
    }
    if (!GDALIsValueInRange<uint8_t>(dfInvisibleVal))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "dfInvisibleVal out of range. Must be [0, 255].");
        return nullptr;
    }
    if (!GDALIsValueInRange<uint8_t>(dfOutOfRangeVal))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "dfOutOfRangeVal out of range. Must be [0, 255].");
        return nullptr;
    }
    oOpts.visibleVal = dfVisibleVal;
    oOpts.invisibleVal = dfInvisibleVal;
    oOpts.outOfRangeVal = dfOutOfRangeVal;

    gdal::viewshed::Viewshed v(oOpts);

    if (!pfnProgress)
        pfnProgress = GDALDummyProgress;
    v.run(hBand, pfnProgress, pProgressArg);

    return GDALDataset::FromHandle(v.output().release());
}

namespace gdal
{
namespace viewshed
{

namespace
{

bool getTransforms(GDALRasterBand &band, double *pFwdTransform,
                   double *pRevTransform)
{
    band.GetDataset()->GetGeoTransform(pFwdTransform);
    if (!GDALInvGeoTransform(pFwdTransform, pRevTransform))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot invert geotransform");
        return false;
    }
    return true;
}

}  // unnamed namespace

Viewshed::Viewshed(const Options &opts) : oOpts{opts}
{
}

Viewshed::~Viewshed() = default;

/// Calculate the extent of the output raster in terms of the input raster and
/// save the input raster extent.
///
/// @return  false on error, true otherwise
bool Viewshed::calcExtents(int nX, int nY,
                           const std::array<double, 6> &adfInvTransform)
{
    // We start with the assumption that the output size matches the input.
    oOutExtent.xStop = GDALGetRasterBandXSize(pSrcBand);
    oOutExtent.yStop = GDALGetRasterBandYSize(pSrcBand);

    if (!oOutExtent.contains(nX, nY))
        CPLError(CE_Warning, CPLE_AppDefined,
                 "NOTE: The observer location falls outside of the DEM area");

    constexpr double EPSILON = 1e-8;
    if (oOpts.maxDistance > 0)
    {
        //ABELL - This assumes that the transformation is only a scaling. Should be fixed.
        //  Find the distance in the direction of the transformed unit vector in the X and Y
        //  directions and use those factors to determine the limiting values in the raster space.
        int nXStart = static_cast<int>(
            std::floor(nX - adfInvTransform[1] * oOpts.maxDistance + EPSILON));
        int nXStop = static_cast<int>(
            std::ceil(nX + adfInvTransform[1] * oOpts.maxDistance - EPSILON) +
            1);
        int nYStart =
            static_cast<int>(std::floor(
                nY - std::fabs(adfInvTransform[5]) * oOpts.maxDistance +
                EPSILON)) -
            (adfInvTransform[5] > 0 ? 1 : 0);
        int nYStop = static_cast<int>(
            std::ceil(nY + std::fabs(adfInvTransform[5]) * oOpts.maxDistance -
                      EPSILON) +
            (adfInvTransform[5] < 0 ? 1 : 0));

        // If the limits are invalid, set the window size to zero to trigger the error below.
        if (nXStart >= oOutExtent.xStop || nXStop < 0 ||
            nYStart >= oOutExtent.yStop || nYStop < 0)
        {
            oOutExtent = Window();
        }
        else
        {
            oOutExtent.xStart = std::max(nXStart, 0);
            oOutExtent.xStop = std::min(nXStop, oOutExtent.xStop);

            oOutExtent.yStart = std::max(nYStart, 0);
            oOutExtent.yStop = std::min(nYStop, oOutExtent.yStop);
        }
    }

    if (oOutExtent.xSize() == 0 || oOutExtent.ySize() == 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid target raster size due to transform "
                 "and/or distance limitation.");
        return false;
    }

    // normalize horizontal index to [ 0, oOutExtent.xSize() )
    oCurExtent = oOutExtent;
    oCurExtent.shiftX(-oOutExtent.xStart);

    return true;
}

/// Compute the viewshed of a raster band.
///
/// @param band  Pointer to the raster band to be processed.
/// @param pfnProgress  Pointer to the progress function. Can be null.
/// @param pProgressArg  Argument passed to the progress function
/// @return  True on success, false otherwise.
bool Viewshed::run(GDALRasterBandH band, GDALProgressFunc pfnProgress,
                   void *pProgressArg)
{
    pSrcBand = static_cast<GDALRasterBand *>(band);

    std::array<double, 6> adfFwdTransform;
    std::array<double, 6> adfInvTransform;
    if (!getTransforms(*pSrcBand, adfFwdTransform.data(),
                       adfInvTransform.data()))
        return false;

    // calculate observer position
    double dfX, dfY;
    GDALApplyGeoTransform(adfInvTransform.data(), oOpts.observer.x,
                          oOpts.observer.y, &dfX, &dfY);
    if (!GDALIsValueInRange<int>(dfX))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Observer X value out of range");
        return false;
    }
    if (!GDALIsValueInRange<int>(dfY))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Observer Y value out of range");
        return false;
    }
    int nX = static_cast<int>(dfX);
    int nY = static_cast<int>(dfY);

    // Must calculate extents in order to make the output dataset.
    if (!calcExtents(nX, nY, adfInvTransform))
        return false;

    poDstDS = createOutputDataset(*pSrcBand, oOpts, oOutExtent);
    if (!poDstDS)
        return false;

    // Create the progress reporter.
    Progress oProgress(pfnProgress, pProgressArg, oOutExtent.ySize());

    // Execute the viewshed algorithm.
    GDALRasterBand *pDstBand = poDstDS->GetRasterBand(1);
    ViewshedExecutor executor(*pSrcBand, *pDstBand, nX, nY, oOutExtent,
                              oCurExtent, oOpts, oProgress,
                              /* emitWarningIfNoData = */ true);
    executor.run();
    oProgress.emit(1);
    return static_cast<bool>(poDstDS);
}

// Adjust the coefficient of curvature for non-earth SRS.
/// \param curveCoeff  Current curve coefficient
/// \param hSrcDS  Source dataset
/// \return  Adjusted curve coefficient.
double adjustCurveCoeff(double curveCoeff, GDALDatasetH hSrcDS)
{
    const OGRSpatialReference *poSRS =
        GDALDataset::FromHandle(hSrcDS)->GetSpatialRef();
    if (poSRS)
    {
        OGRErr eSRSerr;
        const double dfSemiMajor = poSRS->GetSemiMajor(&eSRSerr);
        if (eSRSerr != OGRERR_FAILURE &&
            fabs(dfSemiMajor - SRS_WGS84_SEMIMAJOR) >
                0.05 * SRS_WGS84_SEMIMAJOR)
        {
            curveCoeff = 1.0;
            CPLDebug("gdal_viewshed",
                     "Using -cc=1.0 as a non-Earth CRS has been detected");
        }
    }
    return curveCoeff;
}

}  // namespace viewshed
}  // namespace gdal
