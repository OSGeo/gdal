/**********************************************************************
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

#include "cpl_port.h"
#include "cpl_worker_thread_pool.h"

#include <cstddef>
#include <memory>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_vsi.h"


CPL_CVSID("$Id$")

struct CPLWorkerThreadJob
{
    CPLThreadFunc  pfnFunc;
    void          *pData;
};

/************************************************************************/
/*                         CPLWorkerThreadPool()                        */
/************************************************************************/

/** Instantiate a new pool of worker threads.
 *
 * The pool is in an uninitialized state after this call. The Setup() method
 * must be called.
 */
CPLWorkerThreadPool::CPLWorkerThreadPool()
{
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

    for( auto& wt: aWT )
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
/*                       WorkerThreadFunction()                         */
/************************************************************************/

void CPLWorkerThreadPool::WorkerThreadFunction(void* user_data)
{
    CPLWorkerThread* psWT = static_cast<CPLWorkerThread*>(user_data);
    CPLWorkerThreadPool* poTP = psWT->poTP;

    if( psWT->pfnInitFunc )
        psWT->pfnInitFunc( psWT->pInitData );

    while( true )
    {
        CPLWorkerThreadJob* psJob = poTP->GetNextJob(psWT);
        if( psJob == nullptr )
            break;

        if( psJob->pfnFunc )
        {
            psJob->pfnFunc(psJob->pData);
        }
        CPLFree(psJob);
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
bool CPLWorkerThreadPool::SubmitJob( CPLThreadFunc pfnFunc, void* pData )
{
    CPLAssert( !aWT.empty() );

    CPLWorkerThreadJob* psJob = static_cast<CPLWorkerThreadJob *>(
        VSI_MALLOC_VERBOSE(sizeof(CPLWorkerThreadJob)));
    if( psJob == nullptr )
        return false;
    psJob->pfnFunc = pfnFunc;
    psJob->pData = pData;

    CPLList* psItem =
        static_cast<CPLList *>(VSI_MALLOC_VERBOSE(sizeof(CPLList)));
    if( psItem == nullptr )
    {
        VSIFree(psJob);
        return false;
    }
    psItem->pData = psJob;

    std::unique_lock<std::mutex> oGuard(m_mutex);

    psItem->psNext = psJobQueue;
    psJobQueue = psItem;
    nPendingJobs++;

    if( psWaitingWorkerThreadsList )
    {
        CPLWorkerThread* psWorkerThread =
            static_cast<CPLWorkerThread *>(psWaitingWorkerThreadsList->pData);

        CPLAssert( psWorkerThread->bMarkedAsWaiting );
        psWorkerThread->bMarkedAsWaiting = false;

        CPLList* psNext = psWaitingWorkerThreadsList->psNext;
        CPLList* psToFree = psWaitingWorkerThreadsList;
        psWaitingWorkerThreadsList = psNext;
        nWaitingWorkerThreads--;

        // CPLAssert(
        //   CPLListCount(psWaitingWorkerThreadsList) == nWaitingWorkerThreads);

#if DEBUG_VERBOSE
        CPLDebug("JOB", "Waking up %p", psWorkerThread);
#endif

        {
            std::lock_guard<std::mutex> oGuardWT(psWorkerThread->m_mutex);
            oGuard.unlock();
            psWorkerThread->m_cv.notify_one();
        }

        CPLFree(psToFree);
    }

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
                                     const std::vector<void*>& apData)
{
    CPLAssert( !aWT.empty() );

    std::unique_lock<std::mutex> oGuard(m_mutex);

    CPLList* psJobQueueInit = psJobQueue;
    bool bRet = true;

    for(size_t i=0;i<apData.size();i++)
    {
        CPLWorkerThreadJob* psJob = static_cast<CPLWorkerThreadJob*>(
            VSI_MALLOC_VERBOSE(sizeof(CPLWorkerThreadJob)));
        if( psJob == nullptr )
        {
            bRet = false;
            break;
        }
        psJob->pfnFunc = pfnFunc;
        psJob->pData = apData[i];

        CPLList* psItem =
            static_cast<CPLList *>(VSI_MALLOC_VERBOSE(sizeof(CPLList)));
        if( psItem == nullptr )
        {
            VSIFree(psJob);
            bRet = false;
            break;
        }
        psItem->pData = psJob;

        psItem->psNext = psJobQueue;
        psJobQueue = psItem;
        nPendingJobs++;
    }

    if( !bRet )
    {
        for( CPLList* psIter = psJobQueue; psIter != psJobQueueInit; )
        {
            CPLList* psNext = psIter->psNext;
            VSIFree(psIter->pData);
            VSIFree(psIter);
            nPendingJobs--;
            psIter = psNext;
        }
        return false;
    }

    for(size_t i=0;i<apData.size();i++)
    {
        if( psWaitingWorkerThreadsList && psJobQueue )
        {
            CPLWorkerThread* psWorkerThread;

            psWorkerThread = static_cast<CPLWorkerThread*>(psWaitingWorkerThreadsList->pData);

            CPLAssert( psWorkerThread->bMarkedAsWaiting );
            psWorkerThread->bMarkedAsWaiting = false;

            CPLList* psNext = psWaitingWorkerThreadsList->psNext;
            CPLList* psToFree = psWaitingWorkerThreadsList;
            psWaitingWorkerThreadsList = psNext;
            nWaitingWorkerThreads--;

            // CPLAssert(
            //    CPLListCount(psWaitingWorkerThreadsList) ==
            //    nWaitingWorkerThreads);

#if DEBUG_VERBOSE
            CPLDebug("JOB", "Waking up %p", psWorkerThread);
#endif
            {
                std::lock_guard<std::mutex> oGuardWT(psWorkerThread->m_mutex);
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
 *                          in the queue after this method has completed. Might be
 *                          0 to wait for all jobs.
 */
void CPLWorkerThreadPool::WaitCompletion(int nMaxRemainingJobs)
{
    if( nMaxRemainingJobs < 0 )
        nMaxRemainingJobs = 0;
    std::unique_lock<std::mutex> oGuard(m_mutex);
    while( nPendingJobs > nMaxRemainingJobs )
    {
        m_cv.wait(oGuard);
    }
}

/************************************************************************/
/*                            WaitEvent()                               */
/************************************************************************/

/** Wait for completion of at least one job, if there are any remaining
 */
void CPLWorkerThreadPool::WaitEvent()
{
    std::unique_lock<std::mutex> oGuard(m_mutex);
    while( true )
    {
        const int nPendingJobsBefore = nPendingJobs;
        if( nPendingJobsBefore == 0 )
        {
            break;
        }
        m_cv.wait(oGuard);
        if( nPendingJobs < nPendingJobsBefore )
        {
            break;
        }
    }
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
bool CPLWorkerThreadPool::Setup(int nThreads,
                            CPLThreadFunc pfnInitFunc,
                            void** pasInitData)
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
bool CPLWorkerThreadPool::Setup(int nThreads,
                            CPLThreadFunc pfnInitFunc,
                            void** pasInitData,
                            bool bWaitallStarted)
{
    CPLAssert( nThreads > 0 );

    bool bRet = true;
    for(int i=static_cast<int>(aWT.size());i<nThreads;i++)
    {
        std::unique_ptr<CPLWorkerThread> wt(new CPLWorkerThread);
        wt->pfnInitFunc = pfnInitFunc;
        wt->pInitData = pasInitData ? pasInitData[i] : nullptr;
        wt->poTP = this;
        wt->bMarkedAsWaiting = false;
        wt->hThread =
            CPLCreateJoinableThread(WorkerThreadFunction, wt.get());
        if( wt->hThread == nullptr )
        {
            nThreads = i;
            bRet = false;
            break;
        }
        aWT.emplace_back(std::move(wt));
    }

    if( bWaitallStarted )
    {
        // Wait all threads to be started
        std::unique_lock<std::mutex> oGuard(m_mutex);
        while( nWaitingWorkerThreads < nThreads )
        {
            m_cv.wait(oGuard);
        }
    }

    if( eState == CPLWTS_ERROR )
        bRet = false;

    return bRet;
}

/************************************************************************/
/*                          DeclareJobFinished()                        */
/************************************************************************/

void CPLWorkerThreadPool::DeclareJobFinished()
{
    std::lock_guard<std::mutex> oGuard(m_mutex);
    nPendingJobs --;
    m_cv.notify_one();
}

/************************************************************************/
/*                             GetNextJob()                             */
/************************************************************************/

CPLWorkerThreadJob *
CPLWorkerThreadPool::GetNextJob( CPLWorkerThread* psWorkerThread )
{
    while(true)
    {
        std::unique_lock<std::mutex> oGuard(m_mutex);
        if( eState == CPLWTS_STOP )
        {
            return nullptr;
        }
        CPLList* psTopJobIter = psJobQueue;
        if( psTopJobIter )
        {
            psJobQueue = psTopJobIter->psNext;

#if DEBUG_VERBOSE
            CPLDebug("JOB", "%p got a job", psWorkerThread);
#endif
            CPLWorkerThreadJob* psJob =
                static_cast<CPLWorkerThreadJob*>(psTopJobIter->pData);
            CPLFree(psTopJobIter);
            return psJob;
        }

        if( !psWorkerThread->bMarkedAsWaiting )
        {
            psWorkerThread->bMarkedAsWaiting = true;
            nWaitingWorkerThreads++;

            CPLList* psItem =
                static_cast<CPLList *>(VSI_MALLOC_VERBOSE(sizeof(CPLList)));
            if( psItem == nullptr )
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
        oGuard.unlock();
        psWorkerThread->m_cv.wait(oGuardThisThread);
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
CPLJobQueue::CPLJobQueue(CPLWorkerThreadPool* poPool): m_poPool(poPool) {}
//! @endcond

/************************************************************************/
/*                           ~CPLJobQueue()                             */
/************************************************************************/

CPLJobQueue::~CPLJobQueue()
{
    WaitCompletion();
}

/************************************************************************/
/*                           JobQueueJob                                */
/************************************************************************/

struct JobQueueJob
{
    CPLJobQueue* poQueue = nullptr;
    CPLThreadFunc pfnFunc = nullptr;
    void* pData = nullptr;
};

/************************************************************************/
/*                          JobQueueFunction()                          */
/************************************************************************/

void CPLJobQueue::JobQueueFunction(void* pData)
{
    JobQueueJob* poJob = static_cast<JobQueueJob*>(pData);
    poJob->pfnFunc(poJob->pData);
    poJob->poQueue->DeclareJobFinished();
    delete poJob;
}

/************************************************************************/
/*                          DeclareJobFinished()                        */
/************************************************************************/

void CPLJobQueue::DeclareJobFinished()
{
    std::lock_guard<std::mutex> oGuard(m_mutex);
    m_nPendingJobs --;
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
bool CPLJobQueue::SubmitJob(CPLThreadFunc pfnFunc, void* pData)
{
    JobQueueJob* poJob = new JobQueueJob;
    poJob->poQueue = this;
    poJob->pfnFunc = pfnFunc;
    poJob->pData = pData;
    {
        std::lock_guard<std::mutex> oGuard(m_mutex);
        m_nPendingJobs ++;
    }
    bool bRet = m_poPool->SubmitJob(JobQueueFunction, poJob);
    if( !bRet )
    {
        delete poJob;
    }
    return bRet;
}

/************************************************************************/
/*                            WaitCompletion()                          */
/************************************************************************/

/** Wait for completion of part or whole jobs.
 *
 * @param nMaxRemainingJobs Maximum number of pendings jobs that are allowed
 *                          in the queue after this method has completed. Might be
 *                          0 to wait for all jobs.
 */
void CPLJobQueue::WaitCompletion(int nMaxRemainingJobs)
{
    std::unique_lock<std::mutex> oGuard(m_mutex);
    while( m_nPendingJobs > nMaxRemainingJobs )
    {
        m_cv.wait(oGuard);
    }
}
