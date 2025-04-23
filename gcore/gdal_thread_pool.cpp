/**********************************************************************
 *
 * Project:  GDAL
 * Purpose:  Global thread pool
 * Author:   Even Rouault, <even dot rouault at spatialys dot com>
 *
 **********************************************************************
 * Copyright (c) 2020, Even Rouault, <even dot rouault at spatialys dot com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdal_thread_pool.h"

#include <mutex>

// For unclear reasons, attempts at making this a std::unique_ptr<>, even
// through a GetCompressThreadPool() method like GetMutexThreadPool(), lead
// to "ctest -R autotest_alg" (and other autotest components as well)
// to hang forever once the tests have terminated.
static CPLWorkerThreadPool *gpoCompressThreadPool = nullptr;

static std::mutex &GetMutexThreadPool()
{
    static std::mutex gMutexThreadPool;
    return gMutexThreadPool;
}

CPLWorkerThreadPool *GDALGetGlobalThreadPool(int nThreads)
{
    std::lock_guard oGuard(GetMutexThreadPool());
    if (gpoCompressThreadPool == nullptr)
    {
        gpoCompressThreadPool = new CPLWorkerThreadPool();
        if (!gpoCompressThreadPool->Setup(nThreads, nullptr, nullptr, false))
        {
            delete gpoCompressThreadPool;
            gpoCompressThreadPool = nullptr;
        }
    }
    else if (nThreads > gpoCompressThreadPool->GetThreadCount())
    {
        // Increase size of thread pool
        gpoCompressThreadPool->Setup(nThreads, nullptr, nullptr, false);
    }
    return gpoCompressThreadPool;
}

void GDALDestroyGlobalThreadPool()
{
    std::lock_guard oGuard(GetMutexThreadPool());
    delete gpoCompressThreadPool;
    gpoCompressThreadPool = nullptr;
}
