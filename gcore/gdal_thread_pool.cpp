/**********************************************************************
 *
 * Project:  GDAL
 * Purpose:  Global thread pool
 * Author:   Even Rouault, <even dot rouault at spatialys dot com>
 *
 **********************************************************************
 * Copyright (c) 2020, Even Rouault, <even dot rouault at spatialys dot com>
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "gdal_thread_pool.h"

#include <mutex>

static std::mutex gMutexThreadPool;
static CPLWorkerThreadPool *gpoCompressThreadPool = nullptr;

CPLWorkerThreadPool* GDALGetGlobalThreadPool(int nThreads)
{
    std::lock_guard<std::mutex> oGuard(gMutexThreadPool);
    if( gpoCompressThreadPool == nullptr )
    {
        gpoCompressThreadPool = new CPLWorkerThreadPool();
        if( !gpoCompressThreadPool->Setup(nThreads, nullptr, nullptr) )
        {
            delete gpoCompressThreadPool;
            gpoCompressThreadPool = nullptr;
        }
    }
    else if( nThreads > gpoCompressThreadPool->GetThreadCount() )
    {
        // Increase size of thread pool
        gpoCompressThreadPool->Setup(nThreads, nullptr, nullptr, false);
    }
    return gpoCompressThreadPool;
}

void GDALDestroyGlobalThreadPool()
{
    delete gpoCompressThreadPool;
    gpoCompressThreadPool = nullptr;
}
