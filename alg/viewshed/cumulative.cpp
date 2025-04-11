/******************************************************************************
 * (c) 2024 info@hobu.co
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include <algorithm>
#include <limits>
#include <thread>

#include "cpl_worker_thread_pool.h"

#include "combiner.h"
#include "cumulative.h"
#include "notifyqueue.h"
#include "util.h"
#include "viewshed_executor.h"

namespace gdal
{
namespace viewshed
{

/// Constructor
///
/// @param opts  Options for viewshed generation.
Cumulative::Cumulative(const Options &opts) : m_opts(opts)
{
}

/// Destructor
///
Cumulative::~Cumulative() = default;

/// Compute the cumulative viewshed of a raster band.
///
/// @param srcFilename  Source filename.
/// @param pfnProgress  Pointer to the progress function. Can be null.
/// @param pProgressArg  Argument passed to the progress function
/// @return  True on success, false otherwise.
bool Cumulative::run(const std::string &srcFilename,
                     GDALProgressFunc pfnProgress, void *pProgressArg)
{
    // In cumulative mode, we run the executors in normal mode and want "1" where things
    // are visible.
    m_opts.outputMode = OutputMode::Normal;
    m_opts.visibleVal = 1;

    DatasetPtr srcDS(
        GDALDataset::FromHandle(GDALOpen(srcFilename.c_str(), GA_ReadOnly)));
    if (!srcDS)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Unable open source file.");
        return false;
    }

    GDALRasterBand *pSrcBand = srcDS->GetRasterBand(1);

    // In cumulative mode, the output extent is always the entire source raster.
    m_extent.xStop = GDALGetRasterBandXSize(pSrcBand);
    m_extent.yStop = GDALGetRasterBandYSize(pSrcBand);

    // Make a bunch of observer locations based on the spacing and stick them on a queue
    // to be handled by viewshed executors.
    for (int x = 0; x < m_extent.xStop; x += m_opts.observerSpacing)
        for (int y = 0; y < m_extent.yStop; y += m_opts.observerSpacing)
            m_observerQueue.push({x, y});
    m_observerQueue.done();

    // Run executors.
    const int numThreads = m_opts.numJobs;
    std::atomic<bool> err = false;
    std::atomic<int> running = numThreads;
    std::atomic<bool> hasFoundNoData = false;
    Progress progress(pfnProgress, pProgressArg,
                      m_observerQueue.size() * m_extent.ySize());
    CPLWorkerThreadPool executorPool(numThreads);
    for (int i = 0; i < numThreads; ++i)
        executorPool.SubmitJob(
            [this, &srcFilename, &progress, &err, &running, &hasFoundNoData] {
                runExecutor(srcFilename, progress, err, running,
                            hasFoundNoData);
            });

    // Run combiners that create 8-bit sums of executor jobs.
    CPLWorkerThreadPool combinerPool(numThreads);
    std::vector<Combiner> combiners(numThreads,
                                    Combiner(m_datasetQueue, m_rollupQueue));
    for (Combiner &c : combiners)
        combinerPool.SubmitJob([&c] { c.run(); });

    // Run 32-bit rollup job that combines the 8-bit results from the combiners.
    std::thread sum([this] { rollupRasters(); });

    // When the combiner jobs are done, all the data is in the rollup queue.
    combinerPool.WaitCompletion();
    if (m_datasetQueue.isStopped())
        return false;
    m_rollupQueue.done();

    // Wait for finalBuf to be fully filled.
    sum.join();
    // The executors should exit naturally, but we wait here so that we don't outrun their
    // completion and exit with outstanding threads.
    executorPool.WaitCompletion();

    if (hasFoundNoData)
    {
        CPLError(
            CE_Warning, CPLE_AppDefined,
            "Nodata value found in input DEM. Output will be likely incorrect");
    }

    // Scale the data so that we can write an 8-bit raster output.
    scaleOutput();
    if (!writeOutput(createOutputDataset(*pSrcBand, m_opts, m_extent)))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unable to write to output file.");
        return false;
    }
    progress.emit(1);

    return true;
}

/// Run an executor (single viewshed)
/// @param srcFilename  Source filename
/// @param progress  Progress supporting support.
/// @param err  Shared error flag.
/// @param running  Shared count of number of executors running.
/// @param hasFoundNoData Shared flag to indicate if a point at nodata has been encountered.
void Cumulative::runExecutor(const std::string &srcFilename, Progress &progress,
                             std::atomic<bool> &err, std::atomic<int> &running,
                             std::atomic<bool> &hasFoundNoData)
{
    DatasetPtr srcDs(GDALDataset::Open(srcFilename.c_str(), GA_ReadOnly));
    if (!srcDs)
    {
        err = true;
    }
    else
    {
        Location loc;
        while (!err && m_observerQueue.pop(loc))
        {
            GDALDriver *memDriver =
                GetGDALDriverManager()->GetDriverByName("MEM");
            DatasetPtr dstDs(memDriver ? memDriver->Create("", m_extent.xSize(),
                                                           m_extent.ySize(), 1,
                                                           GDT_Byte, nullptr)
                                       : nullptr);
            if (!dstDs)
            {
                err = true;
            }
            else
            {
                ViewshedExecutor executor(
                    *srcDs->GetRasterBand(1), *dstDs->GetRasterBand(1), loc.x,
                    loc.y, m_extent, m_extent, m_opts, progress,
                    /* emitWarningIfNoData = */ false);
                err = !executor.run();
                if (!err)
                    m_datasetQueue.push(std::move(dstDs));
                if (executor.hasFoundNoData())
                {
                    hasFoundNoData = true;
                }
            }
        }
    }

    // Job done. Set the output queue state.  If all the executor jobs have completed,
    // set the dataset output queue done.
    if (err)
        m_datasetQueue.stop();
    else
    {
        running--;
        if (!running)
            m_datasetQueue.done();
    }
}

// Add 8-bit rasters into the 32-bit raster buffer.
void Cumulative::rollupRasters()
{
    DatasetPtr pDS;

    m_finalBuf.resize(m_extent.size());
    while (m_rollupQueue.pop(pDS))
    {
        uint8_t *srcP =
            static_cast<uint8_t *>(pDS->GetInternalHandle("MEMORY1"));
        for (size_t i = 0; i < m_extent.size(); ++i)
            m_finalBuf[i] += srcP[i];
    }
}

/// Scale the output so that it's fully spread in 8 bits. Perhaps this shouldn't happen if
/// the max is less than 255?
void Cumulative::scaleOutput()
{
    uint32_t m = 0;  // This gathers all the bits set.
    for (uint32_t &val : m_finalBuf)
        m = std::max(val, m);

    if (m == 0)
        return;

    double factor =
        std::numeric_limits<uint8_t>::max() / static_cast<double>(m);
    for (uint32_t &val : m_finalBuf)
        val = static_cast<uint32_t>(std::floor(factor * val));
}

/// Write the output dataset.
/// @param pDstDS  Pointer to the destination dataset.
/// @return True if the write was successful, false otherwise.
bool Cumulative::writeOutput(DatasetPtr pDstDS)
{
    if (!pDstDS)
        return false;

    GDALRasterBand *pDstBand = pDstDS->GetRasterBand(1);
    return (pDstBand->RasterIO(GF_Write, 0, 0, m_extent.xSize(),
                               m_extent.ySize(), m_finalBuf.data(),
                               m_extent.xSize(), m_extent.ySize(), GDT_UInt32,
                               0, 0, nullptr) == 0);
}

}  // namespace viewshed
}  // namespace gdal
