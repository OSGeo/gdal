/******************************************************************************
 * (c) 2024 info@hobu.co
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

#pragma once

#include <atomic>
#include <vector>

#include "notifyqueue.h"
#include "progress.h"
#include "viewshed_types.h"

namespace gdal
{
namespace viewshed
{

class Progress;

/// Generates a cumulative viewshed from a matrix of observers.
class Cumulative
{
  public:
    CPL_DLL explicit Cumulative(const Options &opts);
    CPL_DLL bool run(const std::string &srcFilename,
                     GDALProgressFunc pfnProgress = GDALDummyProgress,
                     void *pProgressArg = nullptr);

  private:
    friend class Combiner;  // Provides access to the queue types.

    struct Location
    {
        int x;
        int y;
    };

    using Buf32 = std::vector<uint32_t>;
    using ObserverQueue = NotifyQueue<Location>;
    using DatasetQueue = NotifyQueue<DatasetPtr>;

    Window m_extent{};
    Options m_opts;
    ObserverQueue m_observerQueue{};
    DatasetQueue m_datasetQueue{};
    DatasetQueue m_rollupQueue{};
    Buf32 m_finalBuf{};

    void runExecutor(const std::string &srcFilename, Progress &progress,
                     std::atomic<bool> &err, std::atomic<int> &running);
    void rollupRasters();
    void scaleOutput();
    bool writeOutput(DatasetPtr pDstDS);
};

}  // namespace viewshed
}  // namespace gdal
