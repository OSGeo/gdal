/**********************************************************************
 * $Id$
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  CPL worker thread pool
 * Author:   Even Rouault, <even dot rouault at spatialys dot com>
 *
 **********************************************************************
 * Copyright (c) 2015, Even Rouault, <even dot rouault at spatialys dot com>
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

#ifndef CPL_WORKER_THREAD_POOL_H_INCLUDED_
#define CPL_WORKER_THREAD_POOL_H_INCLUDED_

#include "cpl_multiproc.h"
#include "cpl_list.h"

#include <condition_variable>
#include <memory>
#include <mutex>
#include <vector>

/**
 * \file cpl_worker_thread_pool.h
 *
 * Class to manage a pool of worker threads.
 * @since GDAL 2.1
 */

#ifndef DOXYGEN_SKIP
struct CPLWorkerThreadJob;
class CPLWorkerThreadPool;

struct CPLWorkerThread
{
    CPL_DISALLOW_COPY_ASSIGN(CPLWorkerThread)
    CPLWorkerThread() = default;

    CPLThreadFunc pfnInitFunc = nullptr;
    void *pInitData = nullptr;
    CPLWorkerThreadPool *poTP = nullptr;
    CPLJoinableThread *hThread = nullptr;
    bool bMarkedAsWaiting = false;

    std::mutex m_mutex{};
    std::condition_variable m_cv{};
};

typedef enum
{
    CPLWTS_OK,
    CPLWTS_STOP,
    CPLWTS_ERROR
} CPLWorkerThreadState;
#endif  // ndef DOXYGEN_SKIP

class CPLJobQueue;

/** Pool of worker threads */
class CPL_DLL CPLWorkerThreadPool
{
    CPL_DISALLOW_COPY_ASSIGN(CPLWorkerThreadPool)

    std::vector<std::unique_ptr<CPLWorkerThread>> aWT{};
    std::mutex m_mutex{};
    std::condition_variable m_cv{};
    volatile CPLWorkerThreadState eState = CPLWTS_OK;
    CPLList *psJobQueue = nullptr;
    int nPendingJobs = 0;

    CPLList *psWaitingWorkerThreadsList = nullptr;
    int nWaitingWorkerThreads = 0;

    int m_nMaxThreads = 0;

    static void WorkerThreadFunction(void *user_data);

    void DeclareJobFinished();
    CPLWorkerThreadJob *GetNextJob(CPLWorkerThread *psWorkerThread);

  public:
    CPLWorkerThreadPool();
    ~CPLWorkerThreadPool();

    bool Setup(int nThreads, CPLThreadFunc pfnInitFunc, void **pasInitData);
    bool Setup(int nThreads, CPLThreadFunc pfnInitFunc, void **pasInitData,
               bool bWaitallStarted);

    std::unique_ptr<CPLJobQueue> CreateJobQueue();

    bool SubmitJob(CPLThreadFunc pfnFunc, void *pData);
    bool SubmitJobs(CPLThreadFunc pfnFunc, const std::vector<void *> &apData);
    void WaitCompletion(int nMaxRemainingJobs = 0);
    void WaitEvent();

    /** Return the number of threads setup */
    int GetThreadCount() const
    {
        return m_nMaxThreads;
    }
};

/** Job queue */
class CPL_DLL CPLJobQueue
{
    CPL_DISALLOW_COPY_ASSIGN(CPLJobQueue)
    CPLWorkerThreadPool *m_poPool = nullptr;
    std::mutex m_mutex{};
    std::condition_variable m_cv{};
    int m_nPendingJobs = 0;

    static void JobQueueFunction(void *);
    void DeclareJobFinished();

    //! @cond Doxygen_Suppress
  protected:
    friend class CPLWorkerThreadPool;
    explicit CPLJobQueue(CPLWorkerThreadPool *poPool);
    //! @endcond

  public:
    ~CPLJobQueue();

    /** Return the owning worker thread pool */
    CPLWorkerThreadPool *GetPool()
    {
        return m_poPool;
    }

    bool SubmitJob(CPLThreadFunc pfnFunc, void *pData);
    void WaitCompletion(int nMaxRemainingJobs = 0);
};

#endif  // CPL_WORKER_THREAD_POOL_H_INCLUDED_
