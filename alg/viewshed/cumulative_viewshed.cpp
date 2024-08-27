/******************************************************************************
 *
 * Project:  Viewshed Generation
 * Purpose:  Core algorithm implementation for viewshed generation.
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

const int NUM_JOBS = 2;

#include <thread>

#include "cpl_worker_thread_pool.h"

#include "combiner.h"
#include "cumulative_viewshed.h"
#include "notifyqueue.h"
#include "util.h"
#include "viewshed_executor.h"

namespace gdal
{
namespace viewshed
{

Cumulative::Cumulative(const Options &opts) : m_opts(opts)
{
}

/// Compute the cumulative viewshed of a raster band.
///
/// @param pfnProgress  Pointer to the progress function. Can be null.
/// @param pProgressArg  Argument passed to the progress function
/// @return  True on success, false otherwise.
bool Cumulative::run(const std::string &srcFilename,
                     GDALProgressFunc pfnProgress, void *pProgressArg)
{
    // In cumulative view, we run the executors in normal mode and want "1" where things
    // are visible.
    m_opts.outputMode = OutputMode::Normal;
    m_opts.visibleVal = 1;

    //ABELL
    /**
    if (!setupProgress(pfnProgress, pProgressArg))
        return false;
    **/
    DatasetPtr srcDS(
        GDALDataset::FromHandle(GDALOpen(srcFilename.c_str(), GA_ReadOnly)));
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
    const int numThreads = NUM_JOBS;
    std::atomic<bool> err = false;
    std::atomic<int> running = numThreads;
    CPLWorkerThreadPool executorPool(numThreads);
    for (int i = 0; i < numThreads; ++i)
        executorPool.SubmitJob([this, &srcFilename, &err, &running]
                               { runExecutor(srcFilename, err, running); });

    // Run combiners that create 8-bit sums of executor jobs.
    CPLWorkerThreadPool combinerPool(numThreads);
    std::vector<Combiner> combiners(numThreads,
                                    Combiner(m_datasetQueue, m_bufQueue));
    for (Combiner &c : combiners)
        combinerPool.SubmitJob([&c] { c.run(); });

    // Run 32-bit rollup job.
    std::thread sum([this] { rollupRasters(); });

    // When the combiner jobs are done, all the data is in the buf queue.
    combinerPool.WaitCompletion();
    if (m_datasetQueue.isStopped())
        return false;
    m_bufQueue.done();

    // Wait for finalBuf to be fully filled. Then scale the output data.
    sum.join();
    scaleOutput();
    writeOutput(*pSrcBand);

    executorPool.WaitCompletion();
    (void)pfnProgress;
    (void)pProgressArg;
    return true;
}

void Cumulative::runExecutor(const std::string &srcFilename,
                             std::atomic<bool> &err, std::atomic<int> &running)
{
    Location loc;
    while (!err && m_observerQueue.pop(loc))
    {
        DatasetPtr srcDs(GDALDataset::Open(srcFilename.c_str(), GA_ReadOnly));
        if (!srcDs)
        {
            err = true;
            break;
        }

        GDALDriver *memDriver = GetGDALDriverManager()->GetDriverByName("MEM");
        DatasetPtr dstDs(memDriver->Create(
            "", m_extent.xSize(), m_extent.ySize(), 1, GDT_Byte, nullptr));
        ViewshedExecutor executor(*srcDs->GetRasterBand(1),
                                  *dstDs->GetRasterBand(1), loc.x, loc.y,
                                  m_extent, m_extent, m_opts);

        if (!executor.run())
            err = true;  // Signal other threads to stop.
        else
            m_datasetQueue.push(std::move(dstDs));
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
    Buf8 buf;

    m_finalBuf.resize(m_extent.size());
    while (m_bufQueue.pop(buf))
        for (size_t i = 0; i < m_finalBuf.size(); ++i)
            m_finalBuf[i] += buf[i];
}

void Cumulative::scaleOutput()
{
    uint32_t m = 0;  // This gathers all the bits set.
    for (uint32_t &val : m_finalBuf)
        m = std::max(val, m);

    double factor =
        std::numeric_limits<uint8_t>::max() / static_cast<double>(m);
    for (uint32_t &val : m_finalBuf)
        val = static_cast<uint32_t>(std::floor(factor * val));
}

void Cumulative::writeOutput(GDALRasterBand &srcBand)
{
    DatasetPtr pDstDS = createOutputDataset(srcBand, m_opts, m_extent);
    GDALRasterBand *pDstBand = pDstDS->GetRasterBand(1);
    (void)pDstBand->RasterIO(GF_Write, 0, 0, m_extent.xSize(), m_extent.ySize(),
                             m_finalBuf.data(), m_extent.xSize(),
                             m_extent.ySize(), GDT_UInt32, 0, 0, nullptr);
}

}  // namespace viewshed
}  // namespace gdal
