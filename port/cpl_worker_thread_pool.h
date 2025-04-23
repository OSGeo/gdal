/**********************************************************************
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  CPL worker thread pool
 * Author:   Even Rouault, <even dot rouault at spatialys dot com>
 *
 **********************************************************************
 * Copyright (c) 2015, Even Rouault, <even dot rouault at spatialys dot com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef CPL_WORKER_THREAD_POOL_H_INCLUDED_
#define CPL_WORKER_THREAD_POOL_H_INCLUDED_

#include "cpl_multiproc.h"
#include "cpl_list.h"

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <vector>

/**
 * \file cpl_worker_thread_pool.h
 *
 * Class to manage a pool of worker threads.
 * @since GDAL 2.1
 */

#ifndef DOXYGEN_SKIP
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
/// Unique pointer to a job queue.
using CPLJobQueuePtr = std::unique_ptr<CPLJobQueue>;

/** Pool of worker threads */
class CPL_DLL CPLWorkerThreadPool
{
    CPL_DISALLOW_COPY_ASSIGN(CPLWorkerThreadPool)

    std::vector<std::unique_ptr<CPLWorkerThread>> aWT{};
    mutable std::mutex m_mutex{};
    std::condition_variable m_cv{};
    volatile CPLWorkerThreadState eState = CPLWTS_OK;
    std::queue<std::function<void()>> jobQueue;
    int nPendingJobs = 0;

    CPLList *psWaitingWorkerThreadsList = nullptr;
    int nWaitingWorkerThreads = 0;

    int m_nMaxThreads = 0;

    static void WorkerThreadFunction(void *user_data);

    void DeclareJobFinished();
    std::function<void()> GetNextJob(CPLWorkerThread *psWorkerThread);

  public:
    CPLWorkerThreadPool();
    explicit CPLWorkerThreadPool(int nThreads);
    ~CPLWorkerThreadPool();

    bool Setup(int nThreads, CPLThreadFunc pfnInitFunc, void **pasInitData);
    bool Setup(int nThreads, CPLThreadFunc pfnInitFunc, void **pasInitData,
               bool bWaitallStarted);

    CPLJobQueuePtr CreateJobQueue();

    bool SubmitJob(std::function<void()> task);
    bool SubmitJob(CPLThreadFunc pfnFunc, void *pData);
    bool SubmitJobs(CPLThreadFunc pfnFunc, const std::vector<void *> &apData);
    void WaitCompletion(int nMaxRemainingJobs = 0);
    void WaitEvent();

    /** Return the number of threads setup */
    int GetThreadCount() const;
};

/** Job queue */
class CPL_DLL CPLJobQueue
{
    CPL_DISALLOW_COPY_ASSIGN(CPLJobQueue)
    CPLWorkerThreadPool *m_poPool = nullptr;
    std::mutex m_mutex{};
    std::condition_variable m_cv{};
    int m_nPendingJobs = 0;

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
    bool SubmitJob(std::function<void()> task);
    void WaitCompletion(int nMaxRemainingJobs = 0);
    bool WaitEvent();
};

#endif  // CPL_WORKER_THREAD_POOL_H_INCLUDED_
