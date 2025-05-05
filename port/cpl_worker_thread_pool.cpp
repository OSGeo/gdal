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

#include "cpl_port.h"
#include "cpl_worker_thread_pool.h"

#include <cstddef>
#include <memory>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_vsi.h"

static thread_local CPLWorkerThreadPool *threadLocalCurrentThreadPool = nullptr;

/************************************************************************/
/*                         CPLWorkerThreadPool()                        */
/************************************************************************/

/** Instantiate a new pool of worker threads.
 *
 * The pool is in an uninitialized state after this call. The Setup() method
 * must be called.
 */
CPLWorkerThreadPool::CPLWorkerThreadPool() : jobQueue{}
{
}

/** Instantiate a new pool of worker threads.
 *
 * \param nThreads  Number of threads in the pool.
 */
CPLWorkerThreadPool::CPLWorkerThreadPool(int nThreads) : jobQueue{}
{
    Setup(nThreads, nullptr, nullptr);
}

/************************************************************************/
/*                          ~CPLWorkerThreadPool()                      */
/************************************************************************/

/** Destroys a pool of worker threads.
 *
 * Any still pending job will be completed before the destructor returns.
 */
CPLWorkerThreadPool::~CPLWorkerThreadPool()
{
    WaitCompletion();

    {
        std::lock_guard<std::mutex> oGuard(m_mutex);
        eState = CPLWTS_STOP;
    }

    for (auto &wt : aWT)
    {
        {
            std::lock_guard<std::mutex> oGuard(wt->m_mutex);
            wt->m_cv.notify_one();
        }
        CPLJoinThread(wt->hThread);
    }

    CPLListDestroy(psWaitingWorkerThreadsList);
}

/************************************************************************/
/*                        GetThreadCount()                              */
/************************************************************************/

int CPLWorkerThreadPool::GetThreadCount() const
{
    std::unique_lock<std::mutex> oGuard(m_mutex);
    return m_nMaxThreads;
}

/************************************************************************/
/*                       WorkerThreadFunction()                         */
/************************************************************************/

void CPLWorkerThreadPool::WorkerThreadFunction(void *user_data)
{
    CPLWorkerThread *psWT = static_cast<CPLWorkerThread *>(user_data);
    CPLWorkerThreadPool *poTP = psWT->poTP;

    threadLocalCurrentThreadPool = poTP;

    if (psWT->pfnInitFunc)
        psWT->pfnInitFunc(psWT->pInitData);

    while (true)
    {
        std::function<void()> task = poTP->GetNextJob(psWT);
        if (!task)
            break;

        task();
#if DEBUG_VERBOSE
        CPLDebug("JOB", "%p finished a job", psWT);
#endif
        poTP->DeclareJobFinished();
    }
}

/************************************************************************/
/*                             SubmitJob()                              */
/************************************************************************/

/** Queue a new job.
 *
 * @param pfnFunc Function to run for the job.
 * @param pData User data to pass to the job function.
 * @return true in case of success.
 */
bool CPLWorkerThreadPool::SubmitJob(CPLThreadFunc pfnFunc, void *pData)
{
    return SubmitJob([=] { pfnFunc(pData); });
}

/** Queue a new job.
 *
 * @param task  Void function to execute.
 * @return true in case of success.
 */
bool CPLWorkerThreadPool::SubmitJob(std::function<void()> task)
{
#ifdef DEBUG
    {
        std::unique_lock<std::mutex> oGuard(m_mutex);
        CPLAssert(m_nMaxThreads > 0);
    }
#endif

    bool bMustIncrementWaitingWorkerThreadsAfterSubmission = false;
    if (threadLocalCurrentThreadPool == this)
    {
        // If there are waiting threads or we have not started all allowed
        // threads, we can submit this job asynchronously
        {
            std::unique_lock<std::mutex> oGuard(m_mutex);
            if (nWaitingWorkerThreads > 0 ||
                static_cast<int>(aWT.size()) < m_nMaxThreads)
            {
                bMustIncrementWaitingWorkerThreadsAfterSubmission = true;
                nWaitingWorkerThreads--;
            }
        }
        if (!bMustIncrementWaitingWorkerThreadsAfterSubmission)
        {
            // otherwise there is a risk of deadlock, so execute synchronously.
            task();
            return true;
        }
    }

    std::unique_lock<std::mutex> oGuard(m_mutex);

    if (bMustIncrementWaitingWorkerThreadsAfterSubmission)
        nWaitingWorkerThreads++;

    if (static_cast<int>(aWT.size()) < m_nMaxThreads)
    {
        // CPLDebug("CPL", "Starting new thread...");
        auto wt = std::make_unique<CPLWorkerThread>();
        wt->poTP = this;
        //ABELL - Why should this fail? And this is a *pool* thread, not necessarily
        //  tied to the submitted job. The submitted job still needs to run, even if
        //  this fails. If we can't create a thread, should the entire pool become invalid?
        wt->hThread = CPLCreateJoinableThread(WorkerThreadFunction, wt.get());
        /**
        if (!wt->hThread)
        {
            VSIFree(psJob);
            VSIFree(psItem);
            return false;
        }
        **/
        if (wt->hThread)
            aWT.emplace_back(std::move(wt));
    }

    jobQueue.emplace(task);
    nPendingJobs++;

    if (psWaitingWorkerThreadsList)
    {
        CPLWorkerThread *psWorkerThread =
            static_cast<CPLWorkerThread *>(psWaitingWorkerThreadsList->pData);

        CPLAssert(psWorkerThread->bMarkedAsWaiting);
        psWorkerThread->bMarkedAsWaiting = false;

        CPLList *psNext = psWaitingWorkerThreadsList->psNext;
        CPLList *psToFree = psWaitingWorkerThreadsList;
        psWaitingWorkerThreadsList = psNext;
        nWaitingWorkerThreads--;

#if DEBUG_VERBOSE
        CPLDebug("JOB", "Waking up %p", psWorkerThread);
#endif

        {
            std::lock_guard<std::mutex> oGuardWT(psWorkerThread->m_mutex);
            // coverity[uninit_use_in_call]
            oGuard.unlock();
            psWorkerThread->m_cv.notify_one();
        }

        CPLFree(psToFree);
    }

    // coverity[double_unlock]
    return true;
}

/************************************************************************/
/*                             SubmitJobs()                              */
/************************************************************************/

/** Queue several jobs
 *
 * @param pfnFunc Function to run for the job.
 * @param apData User data instances to pass to the job function.
 * @return true in case of success.
 */
bool CPLWorkerThreadPool::SubmitJobs(CPLThreadFunc pfnFunc,
                                     const std::vector<void *> &apData)
{
    if (apData.empty())
        return false;

#ifdef DEBUG
    {
        std::unique_lock<std::mutex> oGuard(m_mutex);
        CPLAssert(m_nMaxThreads > 0);
    }
#endif

    if (threadLocalCurrentThreadPool == this)
    {
        // If SubmitJob() is called from a worker thread of this queue,
        // then synchronously run the task to avoid deadlock.
        for (void *pData : apData)
            pfnFunc(pData);
        return true;
    }

    std::unique_lock<std::mutex> oGuard(m_mutex);

    for (void *pData : apData)
    {
        if (static_cast<int>(aWT.size()) < m_nMaxThreads)
        {
            std::unique_ptr<CPLWorkerThread> wt(new CPLWorkerThread);
            wt->poTP = this;
            wt->hThread =
                CPLCreateJoinableThread(WorkerThreadFunction, wt.get());
            if (wt->hThread == nullptr)
            {
                if (aWT.empty())
                    return false;
            }
            else
            {
                aWT.emplace_back(std::move(wt));
            }
        }

        jobQueue.emplace([=] { pfnFunc(pData); });
        nPendingJobs++;
    }

    for (size_t i = 0; i < apData.size(); i++)
    {
        if (psWaitingWorkerThreadsList)
        {
            CPLWorkerThread *psWorkerThread;

            psWorkerThread = static_cast<CPLWorkerThread *>(
                psWaitingWorkerThreadsList->pData);

            CPLAssert(psWorkerThread->bMarkedAsWaiting);
            psWorkerThread->bMarkedAsWaiting = false;

            CPLList *psNext = psWaitingWorkerThreadsList->psNext;
            CPLList *psToFree = psWaitingWorkerThreadsList;
            psWaitingWorkerThreadsList = psNext;
            nWaitingWorkerThreads--;

#if DEBUG_VERBOSE
            CPLDebug("JOB", "Waking up %p", psWorkerThread);
#endif
            {
                std::lock_guard<std::mutex> oGuardWT(psWorkerThread->m_mutex);
                // coverity[uninit_use_in_call]
                oGuard.unlock();
                psWorkerThread->m_cv.notify_one();
            }

            CPLFree(psToFree);
            oGuard.lock();
        }
        else
        {
            break;
        }
    }

    return true;
}

/************************************************************************/
/*                            WaitCompletion()                          */
/************************************************************************/

/** Wait for completion of part or whole jobs.
 *
 * @param nMaxRemainingJobs Maximum number of pendings jobs that are allowed
 *                          in the queue after this method has completed. Might
 * be 0 to wait for all jobs.
 */
void CPLWorkerThreadPool::WaitCompletion(int nMaxRemainingJobs)
{
    if (nMaxRemainingJobs < 0)
        nMaxRemainingJobs = 0;
    std::unique_lock<std::mutex> oGuard(m_mutex);
    m_cv.wait(oGuard, [this, nMaxRemainingJobs]
              { return nPendingJobs <= nMaxRemainingJobs; });
}

/************************************************************************/
/*                            WaitEvent()                               */
/************************************************************************/

/** Wait for completion of at least one job, if there are any remaining
 */
void CPLWorkerThreadPool::WaitEvent()
{
    // NOTE - This isn't quite right. After nPendingJobsBefore is set but before
    // a notification occurs, jobs could be submitted which would increase
    // nPendingJobs, so a job completion may looks like a spurious wakeup.
    std::unique_lock<std::mutex> oGuard(m_mutex);
    if (nPendingJobs == 0)
        return;
    const int nPendingJobsBefore = nPendingJobs;
    m_cv.wait(oGuard, [this, nPendingJobsBefore]
              { return nPendingJobs < nPendingJobsBefore; });
}

/************************************************************************/
/*                                Setup()                               */
/************************************************************************/

/** Setup the pool.
 *
 * @param nThreads Number of threads to launch
 * @param pfnInitFunc Initialization function to run in each thread. May be NULL
 * @param pasInitData Array of initialization data. Its length must be nThreads,
 *                    or it should be NULL.
 * @return true if initialization was successful.
 */
bool CPLWorkerThreadPool::Setup(int nThreads, CPLThreadFunc pfnInitFunc,
                                void **pasInitData)
{
    return Setup(nThreads, pfnInitFunc, pasInitData, true);
}

/** Setup the pool.
 *
 * @param nThreads Number of threads to launch
 * @param pfnInitFunc Initialization function to run in each thread. May be NULL
 * @param pasInitData Array of initialization data. Its length must be nThreads,
 *                    or it should be NULL.
 * @param bWaitallStarted Whether to wait for all threads to be fully started.
 * @return true if initialization was successful.
 */
bool CPLWorkerThreadPool::Setup(int nThreads, CPLThreadFunc pfnInitFunc,
                                void **pasInitData, bool bWaitallStarted)
{
    CPLAssert(nThreads > 0);

    if (nThreads > static_cast<int>(aWT.size()) && pfnInitFunc == nullptr &&
        pasInitData == nullptr && !bWaitallStarted)
    {
        std::lock_guard<std::mutex> oGuard(m_mutex);
        if (nThreads > m_nMaxThreads)
            m_nMaxThreads = nThreads;
        return true;
    }

    bool bRet = true;
    for (int i = static_cast<int>(aWT.size()); i < nThreads; i++)
    {
        auto wt = std::make_unique<CPLWorkerThread>();
        wt->pfnInitFunc = pfnInitFunc;
        wt->pInitData = pasInitData ? pasInitData[i] : nullptr;
        wt->poTP = this;
        wt->hThread = CPLCreateJoinableThread(WorkerThreadFunction, wt.get());
        if (wt->hThread == nullptr)
        {
            nThreads = i;
            bRet = false;
            break;
        }
        aWT.emplace_back(std::move(wt));
    }

    {
        std::lock_guard<std::mutex> oGuard(m_mutex);
        if (nThreads > m_nMaxThreads)
            m_nMaxThreads = nThreads;
    }

    if (bWaitallStarted)
    {
        // Wait all threads to be started
        std::unique_lock<std::mutex> oGuard(m_mutex);
        while (nWaitingWorkerThreads < nThreads)
        {
            m_cv.wait(oGuard);
        }
    }

    if (eState == CPLWTS_ERROR)
        bRet = false;

    return bRet;
}

/************************************************************************/
/*                          DeclareJobFinished()                        */
/************************************************************************/

void CPLWorkerThreadPool::DeclareJobFinished()
{
    std::lock_guard<std::mutex> oGuard(m_mutex);
    nPendingJobs--;
    m_cv.notify_one();
}

/************************************************************************/
/*                             GetNextJob()                             */
/************************************************************************/

std::function<void()>
CPLWorkerThreadPool::GetNextJob(CPLWorkerThread *psWorkerThread)
{
    std::unique_lock<std::mutex> oGuard(m_mutex);
    while (true)
    {
        if (eState == CPLWTS_STOP)
            return std::function<void()>();

        if (jobQueue.size())
        {
#if DEBUG_VERBOSE
            CPLDebug("JOB", "%p got a job", psWorkerThread);
#endif
            auto task = std::move(jobQueue.front());
            jobQueue.pop();
            return task;
        }

        if (!psWorkerThread->bMarkedAsWaiting)
        {
            psWorkerThread->bMarkedAsWaiting = true;
            nWaitingWorkerThreads++;

            CPLList *psItem =
                static_cast<CPLList *>(VSI_MALLOC_VERBOSE(sizeof(CPLList)));
            if (psItem == nullptr)
            {
                eState = CPLWTS_ERROR;
                m_cv.notify_one();

                return nullptr;
            }

            psItem->pData = psWorkerThread;
            psItem->psNext = psWaitingWorkerThreadsList;
            psWaitingWorkerThreadsList = psItem;

#if DEBUG_VERBOSE
            CPLAssert(CPLListCount(psWaitingWorkerThreadsList) ==
                      nWaitingWorkerThreads);
#endif
        }

        m_cv.notify_one();

#if DEBUG_VERBOSE
        CPLDebug("JOB", "%p sleeping", psWorkerThread);
#endif

        std::unique_lock<std::mutex> oGuardThisThread(psWorkerThread->m_mutex);
        // coverity[uninit_use_in_call]
        oGuard.unlock();
        // coverity[wait_not_in_locked_loop]
        psWorkerThread->m_cv.wait(oGuardThisThread);
        oGuard.lock();
    }
}

/************************************************************************/
/*                         CreateJobQueue()                             */
/************************************************************************/

/** Create a new job queue based on this worker thread pool.
 *
 * The worker thread pool must remain alive while the returned object is
 * itself alive.
 *
 * @since GDAL 3.2
 */
std::unique_ptr<CPLJobQueue> CPLWorkerThreadPool::CreateJobQueue()
{
    return std::unique_ptr<CPLJobQueue>(new CPLJobQueue(this));
}

/************************************************************************/
/*                            CPLJobQueue()                             */
/************************************************************************/

//! @cond Doxygen_Suppress
CPLJobQueue::CPLJobQueue(CPLWorkerThreadPool *poPool) : m_poPool(poPool)
{
}

//! @endcond

/************************************************************************/
/*                           ~CPLJobQueue()                             */
/************************************************************************/

CPLJobQueue::~CPLJobQueue()
{
    WaitCompletion();
}

/************************************************************************/
/*                          DeclareJobFinished()                        */
/************************************************************************/

void CPLJobQueue::DeclareJobFinished()
{
    std::lock_guard<std::mutex> oGuard(m_mutex);
    m_nPendingJobs--;
    m_cv.notify_one();
}

/************************************************************************/
/*                             SubmitJob()                              */
/************************************************************************/

/** Queue a new job.
 *
 * @param pfnFunc Function to run for the job.
 * @param pData User data to pass to the job function.
 * @return true in case of success.
 */
bool CPLJobQueue::SubmitJob(CPLThreadFunc pfnFunc, void *pData)
{
    return SubmitJob([=] { pfnFunc(pData); });
}

/** Queue a new job.
 *
 * @param task  Task to execute.
 * @return true in case of success.
 */
bool CPLJobQueue::SubmitJob(std::function<void()> task)
{
    {
        std::lock_guard<std::mutex> oGuard(m_mutex);
        m_nPendingJobs++;
    }

    // coverity[uninit_member,copy_constructor_call]
    const auto lambda = [this, task]
    {
        task();
        DeclareJobFinished();
    };
    // cppcheck-suppress knownConditionTrueFalse
    return m_poPool->SubmitJob(lambda);
}

/************************************************************************/
/*                            WaitCompletion()                          */
/************************************************************************/

/** Wait for completion of part or whole jobs.
 *
 * @param nMaxRemainingJobs Maximum number of pendings jobs that are allowed
 *                          in the queue after this method has completed. Might
 * be 0 to wait for all jobs.
 */
void CPLJobQueue::WaitCompletion(int nMaxRemainingJobs)
{
    std::unique_lock<std::mutex> oGuard(m_mutex);
    m_cv.wait(oGuard, [this, nMaxRemainingJobs]
              { return m_nPendingJobs <= nMaxRemainingJobs; });
}

/************************************************************************/
/*                             WaitEvent()                              */
/************************************************************************/

/** Wait for completion for at least one job.
 *
 * @return true if there are remaining jobs.
 */
bool CPLJobQueue::WaitEvent()
{
    // NOTE - This isn't quite right. After nPendingJobsBefore is set but before
    // a notification occurs, jobs could be submitted which would increase
    // nPendingJobs, so a job completion may looks like a spurious wakeup.
    std::unique_lock<std::mutex> oGuard(m_mutex);
    if (m_nPendingJobs == 0)
        return false;

    const int nPendingJobsBefore = m_nPendingJobs;
    m_cv.wait(oGuard, [this, nPendingJobsBefore]
              { return m_nPendingJobs < nPendingJobsBefore; });
    return m_nPendingJobs > 0;
}
