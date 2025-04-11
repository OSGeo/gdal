/******************************************************************************
 * (c) 2024 info@hobu.co
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef VIEWSHED_CUMULATIVE_H_INCLUDED
#define VIEWSHED_CUMULATIVE_H_INCLUDED

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
    // We define an explicit destructor, whose implementation is in libgdal,
    // otherwise with gcc 9.4 of Ubuntu 20.04 in debug mode, this would need to
    // redefinition of the NotifyQueue class in both libgdal and gdal_viewshed,
    // leading to weird things related to mutex.
    CPL_DLL ~Cumulative();
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
                     std::atomic<bool> &err, std::atomic<int> &running,
                     std::atomic<bool> &hasFoundNoData);
    void rollupRasters();
    void scaleOutput();
    bool writeOutput(DatasetPtr pDstDS);

    Cumulative(const Cumulative &) = delete;
    Cumulative &operator=(const Cumulative &) = delete;
};

}  // namespace viewshed
}  // namespace gdal

#endif
