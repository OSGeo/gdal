/******************************************************************************
 *
 * Project:  Viewshed Generation
 * Purpose:  Core algorithm implementation for viewshed generation.
 * Author:   Tamas Szekeres, szekerest@gmail.com
 *
 ******************************************************************************
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

/**
#include <iostream>

#include <queue>
**/

#include <algorithm>
#include <array>

#include "gdal_alg.h"
#include "gdal_priv_templates.hpp"

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
 * This appoach provides a relatively fast calculation, since the output raster
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

    Viewshed::Options oOpts;
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
            oOpts.cellMode = Viewshed::CellMode::Edge;
            break;
        case GVM_Diagonal:
            oOpts.cellMode = Viewshed::CellMode::Diagonal;
            break;
        case GVM_Min:
            oOpts.cellMode = Viewshed::CellMode::Min;
            break;
        case GVM_Max:
            oOpts.cellMode = Viewshed::CellMode::Max;
            break;
    }

    switch (heightMode)
    {
        case GVOT_MIN_TARGET_HEIGHT_FROM_DEM:
            oOpts.outputMode = Viewshed::OutputMode::DEM;
            break;
        case GVOT_MIN_TARGET_HEIGHT_FROM_GROUND:
            oOpts.outputMode = Viewshed::OutputMode::Ground;
            break;
        case GVOT_NORMAL:
            oOpts.outputMode = Viewshed::OutputMode::Normal;
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

    gdal::Viewshed v(oOpts);

    if (!pfnProgress)
        pfnProgress = GDALDummyProgress;
    v.run(hBand, pfnProgress, pProgressArg);

    return GDALDataset::FromHandle(v.output().release());
}

namespace gdal
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

/// Emit progress information saying that a line has been written to output.
///
/// @return  True on success, false otherwise.
bool Viewshed::lineProgress()
{
    if (nLineCount < oCurExtent.ySize())
        nLineCount++;
    return emitProgress(nLineCount / static_cast<double>(oCurExtent.ySize()));
}

/// Emit progress information saying that a fraction of work has been completed.
///
/// @return  True on success, false otherwise.
bool Viewshed::emitProgress(double fraction)
{
    // Call the progress function.
    if (!oProgress(fraction, ""))
    {
        CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
        return false;
    }
    return true;
}

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
    //ABELL - verify this won't underflow.
    oCurExtent.shiftX(-oOutExtent.xStart);

    return true;
}

/// Create the output dataset.
///
/// @return  True on success, false otherwise.
Viewshed::DatasetPtr
Viewshed::createOutputDataset(GDALRasterBand &srcBand,
                              const std::string &outFilename)
{
    GDALDriverManager *hMgr = GetGDALDriverManager();
    GDALDriver *hDriver = hMgr->GetDriverByName(oOpts.outputFormat.c_str());
    if (!hDriver)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot get driver");
        return nullptr;
    }

    /* create output raster */
    DatasetPtr dataset(hDriver->Create(
        outFilename.c_str(), oOutExtent.xSize(), oOutExtent.ySize(), 1,
        oOpts.outputMode == OutputMode::Normal ? GDT_Byte : GDT_Float64,
        const_cast<char **>(oOpts.creationOpts.List())));
    if (!dataset)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot create dataset for %s",
                 outFilename.c_str());
        return nullptr;
    }

    /* copy srs */
    dataset->SetSpatialRef(srcBand.GetDataset()->GetSpatialRef());

    std::array<double, 6> adfSrcTransform;
    std::array<double, 6> adfDstTransform;
    srcBand.GetDataset()->GetGeoTransform(adfSrcTransform.data());
    adfDstTransform[0] = adfSrcTransform[0] +
                         adfSrcTransform[1] * oOutExtent.xStart +
                         adfSrcTransform[2] * oOutExtent.yStart;
    adfDstTransform[1] = adfSrcTransform[1];
    adfDstTransform[2] = adfSrcTransform[2];
    adfDstTransform[3] = adfSrcTransform[3] +
                         adfSrcTransform[4] * oOutExtent.xStart +
                         adfSrcTransform[5] * oOutExtent.yStart;
    adfDstTransform[4] = adfSrcTransform[4];
    adfDstTransform[5] = adfSrcTransform[5];
    dataset->SetGeoTransform(adfDstTransform.data());

    pDstBand = dataset->GetRasterBand(1);
    if (!pDstBand)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot get band for %s",
                 outFilename.c_str());
        return nullptr;
    }

    if (oOpts.nodataVal >= 0)
        GDALSetRasterNoDataValue(pDstBand, oOpts.nodataVal);
    return dataset;
}

bool Viewshed::setupProgress(GDALProgressFunc pfnProgress, void *pProgressArg)
{
    using namespace std::placeholders;

    nLineCount = 0;
    oProgress = std::bind(pfnProgress, _1, _2, pProgressArg);
    return emitProgress(0);
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
    if (!setupProgress(pfnProgress, pProgressArg))
        return false;

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

    poDstDS = createOutputDataset(*pSrcBand, oOpts.outputFilename);
    if (!poDstDS)
        return false;

    // Execute the viewshed algorithm.
    ViewshedExecutor executor(*pSrcBand, *pDstBand, nX, nY, oOutExtent,
                              oCurExtent, oOpts);
    executor.run();
    return static_cast<bool>(poDstDS);
}

/// Compute the cumulative viewshed of a raster band.
///
/// @param pfnProgress  Pointer to the progress function. Can be null.
/// @param pProgressArg  Argument passed to the progress function
/// @return  True on success, false otherwise.
bool Viewshed::runCumulative(GDALProgressFunc pfnProgress, void *pProgressArg)
{
    std::vector<std::pair<int, int>> observers;

    if (!setupProgress(pfnProgress, pProgressArg))
        return false;

    //ABELL - We need multiple source datasets to allow for threading.
    // pSrcBand = static_cast<GDALRasterBand *>(band);

    // In cumulative mode, the output extent is always the entire source raster.
    oOutExtent.xStop = GDALGetRasterBandXSize(pSrcBand);
    oOutExtent.yStop = GDALGetRasterBandYSize(pSrcBand);
    oCurExtent = oOutExtent;

    if (!createOutputDataset(*pSrcBand, oOpts.outputFilename))
        return false;

    for (int x = 0; x < oCurExtent.xStop; x += oOpts.observerSpacing)
        for (int y = 0; y < oCurExtent.yStop; y += oOpts.observerSpacing)
            observers.emplace_back(x, y);

    /**
    std::queue<DatasetPtr> outputQueue;

    const int numThreads = 5;
    GDALWorkerThreadPool pool(numThreads);
    m_remaining = observers.size();

    std::mutex mutex;

    // Queue all the jobs.
    while (observers.size())
    {
        [x, y] = observers.back();
        observers.resize(observers.size() - 1);
        pool.SubmitJob([x, y, &outputQueue, &mutex, &cv] {
                DatasetPtr pDataset;
                if (!err)
                {
                    pDataset.reset(execute(x, y));
                    if (!pDataset)
                        err = true;
                }
                {
                    std::lock_guard<std::mutex> lock(mutex);
                    outputQueue.emplace_back(pDataset);
                }
                cv.notify_one();
            }
        );
    }
    createCumDataset(
    **/

    (void)pfnProgress;
    (void)pProgressArg;
    return true;
}

/**
Viewshed::createCumDataset(size_t cnt, int xlen, int ylen)
{
    std::vector<DatasetPtr> ds;
    std::vector<std::future<void>> futures;

    // Make some combiners
    GDALDriver *driver = GDALGetDriverByName("MEM");
    for (size_t i = 0; i < cnt; ++i)
    {
        ds.emplace_back(driver->Create(GF_Writer, 0, 0, oOutExtent.xSize(), oOutExtent.ySize(),
            1, GDT_UInt8, nullptr));
        future.emplace_back(std::async([this]{combiner(ds.back().get());}));
    }

    // Wait for the combiners to complete.
    for (std::future<void>& f : futures)
        f.wait();

    // Add it all up into the final dataset.
    for (size_t i = 0; i < cnt; ++i)
        sum(opts.outDataset, datasets[i]);
}

void Viewshed::combine(GDALDataset *dstDataset)
{
    while (true)
    {
        std::unique_lock l(queueMutex);
        cv.wait(l) [this]{ return !m_stop && m_remaining != 0 && !m_queue.empty(); }
        if (!m_queue.empty())
        {
            DatasetPtr p = m_queue.front();
            m_queue.pop();
            m_remaining--;

            lock.unlock();
            sum(dstDataset, p);
        }
        else
            return;
    }
}

void Viewshed::sum(GDALDataset *dst, GDALDataset *src)
{
    GDALRasterBand *srcBand = src->GetRasterBand(1);
    GDALRasterBand *dstBand = dst->GetRasterBand(1);

    int xsize = srcBand->GetXSize();
    int ysize = srcBand->GetYSize();
    srcBand->GetBlockSize(&xBlockSize, &yBlockSize);

    std::vector srcBuf<uint8_t> (xBlockSize * yBlockSize);
    std::vector dstBuf<uint8_t> (xBlockSize * yBlockSize);

    int x = 0;
    int y = 0;
    while (y < ysize)
    {
        while (x < xsize)
        {
            srcBand->ReadBlock(xoff, yoff);
            dstBand->ReadBlock(xoff, yoff)

            for (size_t i = 0; i < srcBuf.size(); ++i)
                { dstBuf[i] += srcBuf[i]; }

            dstBand->WriteBlock(xoff, yoff);
            std::fill(srcBuf.begin(), srcBuf.end(), 0);
            xoff++;
            x += xBlockSize();
        }
        x = 0;
        xoff = 0;
        y += yBlockSize();
        yoff++;
    }
}
**/

}  // namespace gdal
