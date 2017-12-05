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

/************************************************************************/
/*                         CPLWorkerThreadPool()                        */
/************************************************************************/

/** Instantiate a new pool of worker threads.
 *
 * The pool is in an uninitialized state after this call. The Setup() method
 * must be called.
 */
CPLWorkerThreadPool::CPLWorkerThreadPool() :
    hCond(NULL),
    eState(CPLWTS_OK),
    psJobQueue(NULL),
    nPendingJobs(0),
    psWaitingWorkerThreadsList(NULL),
    nWaitingWorkerThreads(0)
{
    hMutex = CPLCreateMutexEx(CPL_MUTEX_REGULAR);
    CPLReleaseMutex(hMutex);
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
    if( hCond )
    {
        WaitCompletion();

        CPLAcquireMutex(hMutex, 1000.0);
        eState = CPLWTS_STOP;
        CPLReleaseMutex(hMutex);

        for(size_t i=0;i<aWT.size();i++)
        {
            CPLAcquireMutex(aWT[i].hMutex, 1000.0);
            CPLCondSignal(aWT[i].hCond);
            CPLReleaseMutex(aWT[i].hMutex);
            CPLJoinThread(aWT[i].hThread);
            CPLDestroyCond(aWT[i].hCond);
            CPLDestroyMutex(aWT[i].hMutex);
        }

        CPLListDestroy(psWaitingWorkerThreadsList);

        CPLDestroyCond(hCond);
    }
    CPLDestroyMutex(hMutex);
}

/************************************************************************/
/*                       WorkerThreadFunction()                         */
/************************************************************************/

void CPLWorkerThreadPool::WorkerThreadFunction(void* user_data)
{
    CPLWorkerThread* psWT = (CPLWorkerThread* ) user_data;
    CPLWorkerThreadPool* poTP = psWT->poTP;

    if( psWT->pfnInitFunc )
        psWT->pfnInitFunc( psWT->pInitData );

    while( true )
    {
        CPLWorkerThreadJob* psJob = poTP->GetNextJob(psWT);
        if( psJob == NULL )
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
    if( psJob == NULL )
        return false;
    psJob->pfnFunc = pfnFunc;
    psJob->pData = pData;

    CPLList* psItem =
        static_cast<CPLList *>(VSI_MALLOC_VERBOSE(sizeof(CPLList)));
    if( psItem == NULL )
    {
        VSIFree(psJob);
        return false;
    }
    psItem->pData = psJob;

    CPLAcquireMutex(hMutex, 1000.0);

    psItem->psNext = psJobQueue;
    psJobQueue = psItem;
    nPendingJobs++;

    if( psWaitingWorkerThreadsList )
    {
        CPLWorkerThread* psWorkerThread =
            static_cast<CPLWorkerThread *>(psWaitingWorkerThreadsList->pData);

        CPLAssert( psWorkerThread->bMarkedAsWaiting );
        psWorkerThread->bMarkedAsWaiting = FALSE;

        CPLList* psNext = psWaitingWorkerThreadsList->psNext;
        CPLList* psToFree = psWaitingWorkerThreadsList;
        psWaitingWorkerThreadsList = psNext;
        nWaitingWorkerThreads--;

        // CPLAssert(
        //   CPLListCount(psWaitingWorkerThreadsList) == nWaitingWorkerThreads);

#if DEBUG_VERBOSE
        CPLDebug("JOB", "Waking up %p", psWorkerThread);
#endif
        CPLAcquireMutex(psWorkerThread->hMutex, 1000.0);
        CPLReleaseMutex(hMutex);
        CPLCondSignal(psWorkerThread->hCond);
        CPLReleaseMutex(psWorkerThread->hMutex);

        CPLFree(psToFree);
    }
    else
    {
        CPLReleaseMutex(hMutex);
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

    CPLAcquireMutex(hMutex, 1000.0);

    CPLList* psJobQueueInit = psJobQueue;
    bool bRet = true;

    for(size_t i=0;i<apData.size();i++)
    {
        CPLWorkerThreadJob* psJob = static_cast<CPLWorkerThreadJob*>(
            VSI_MALLOC_VERBOSE(sizeof(CPLWorkerThreadJob)));
        if( psJob == NULL )
        {
            bRet = false;
            break;
        }
        psJob->pfnFunc = pfnFunc;
        psJob->pData = apData[i];

        CPLList* psItem =
            static_cast<CPLList *>(VSI_MALLOC_VERBOSE(sizeof(CPLList)));
        if( psItem == NULL )
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
    }

    CPLReleaseMutex(hMutex);

    if( !bRet )
        return false;

    for(size_t i=0;i<apData.size();i++)
    {
        CPLAcquireMutex(hMutex, 1000.0);

        if( psWaitingWorkerThreadsList && psJobQueue )
        {
            CPLWorkerThread* psWorkerThread;

            psWorkerThread = (CPLWorkerThread*)psWaitingWorkerThreadsList->pData;

            CPLAssert( psWorkerThread->bMarkedAsWaiting );
            psWorkerThread->bMarkedAsWaiting = FALSE;

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
            CPLAcquireMutex(psWorkerThread->hMutex, 1000.0);

            // CPLAssert(psWorkerThread->psNextJob == NULL);
            // psWorkerThread->psNextJob =
            //     (CPLWorkerThreadJob*)psJobQueue->pData;
            // psNext = psJobQueue->psNext;
            // CPLFree(psJobQueue);
            // psJobQueue = psNext;

            CPLReleaseMutex(hMutex);
            CPLCondSignal(psWorkerThread->hCond);
            CPLReleaseMutex(psWorkerThread->hMutex);

            CPLFree(psToFree);
        }
        else
        {
            CPLReleaseMutex(hMutex);
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
    while( true )
    {
        CPLAcquireMutex(hMutex, 1000.0);
        int nPendingJobsLocal = nPendingJobs;
        if( nPendingJobsLocal > nMaxRemainingJobs )
            CPLCondWait(hCond, hMutex);
        CPLReleaseMutex(hMutex);
        if( nPendingJobsLocal <= nMaxRemainingJobs )
            break;
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
    CPLAssert( nThreads > 0 );

    hCond = CPLCreateCond();
    if( hCond == NULL )
        return false;

    bool bRet = true;
    aWT.resize(nThreads);
    for(int i=0;i<nThreads;i++)
    {
        aWT[i].pfnInitFunc = pfnInitFunc;
        aWT[i].pInitData = pasInitData ? pasInitData[i] : NULL;
        aWT[i].poTP = this;

        aWT[i].hMutex = CPLCreateMutexEx(CPL_MUTEX_REGULAR);
        if( aWT[i].hMutex == NULL )
        {
            nThreads = i;
            aWT.resize(nThreads);
            bRet = false;
            break;
        }
        CPLReleaseMutex(aWT[i].hMutex);
        aWT[i].hCond = CPLCreateCond();
        if( aWT[i].hCond == NULL )
        {
            CPLDestroyMutex(aWT[i].hMutex);
            nThreads = i;
            aWT.resize(nThreads);
            bRet = false;
            break;
        }

        aWT[i].bMarkedAsWaiting = FALSE;
        // aWT[i].psNextJob = NULL;

        aWT[i].hThread =
            CPLCreateJoinableThread(WorkerThreadFunction, &(aWT[i]));
        if( aWT[i].hThread == NULL )
        {
            nThreads = i;
            aWT.resize(nThreads);
            bRet = false;
            break;
        }
    }

    // Wait all threads to be started
    while( true )
    {
        CPLAcquireMutex(hMutex, 1000.0);
        int nWaitingWorkerThreadsLocal = nWaitingWorkerThreads;
        if( nWaitingWorkerThreadsLocal < nThreads )
            CPLCondWait(hCond, hMutex);
        CPLReleaseMutex(hMutex);
        if( nWaitingWorkerThreadsLocal == nThreads )
            break;
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
    CPLAcquireMutex(hMutex, 1000.0);
    nPendingJobs--;
    CPLCondSignal(hCond);
    CPLReleaseMutex(hMutex);
}

/************************************************************************/
/*                             GetNextJob()                             */
/************************************************************************/

CPLWorkerThreadJob *
CPLWorkerThreadPool::GetNextJob( CPLWorkerThread* psWorkerThread )
{
    while(true)
    {
        CPLAcquireMutex(hMutex, 1000.0);
        if( eState == CPLWTS_STOP )
        {
            CPLReleaseMutex(hMutex);
            return NULL;
        }
        CPLList* psTopJobIter = psJobQueue;
        if( psTopJobIter )
        {
            psJobQueue = psTopJobIter->psNext;

#if DEBUG_VERBOSE
            CPLDebug("JOB", "%p got a job", psWorkerThread);
#endif
            CPLWorkerThreadJob* psJob =
                (CPLWorkerThreadJob*)psTopJobIter->pData;
            CPLReleaseMutex(hMutex);
            CPLFree(psTopJobIter);
            return psJob;
        }

        if( !psWorkerThread->bMarkedAsWaiting )
        {
            psWorkerThread->bMarkedAsWaiting = TRUE;
            nWaitingWorkerThreads++;
            CPLAssert(nWaitingWorkerThreads <= static_cast<int>(aWT.size()));

            CPLList* psItem =
                static_cast<CPLList *>(VSI_MALLOC_VERBOSE(sizeof(CPLList)));
            if( psItem == NULL )
            {
                eState = CPLWTS_ERROR;
                CPLCondSignal(hCond);

                CPLReleaseMutex(hMutex);
                return NULL;
            }

            psItem->pData = psWorkerThread;
            psItem->psNext = psWaitingWorkerThreadsList;
            psWaitingWorkerThreadsList = psItem;

#if DEBUG_VERBOSE
            CPLAssert(CPLListCount(psWaitingWorkerThreadsList) ==
                      nWaitingWorkerThreads);
#endif
        }

        CPLCondSignal(hCond);

        CPLAcquireMutex(psWorkerThread->hMutex, 1000.0);
#if DEBUG_VERBOSE
        CPLDebug("JOB", "%p sleeping", psWorkerThread);
#endif
        CPLReleaseMutex(hMutex);

        CPLCondWait( psWorkerThread->hCond, psWorkerThread->hMutex );

        // TODO(rouault): Explain or delete.
        // CPLWorkerThreadJob* psJob = psWorkerThread->psNextJob;
        // psWorkerThread->psNextJob = NULL;

        CPLReleaseMutex(psWorkerThread->hMutex);

        // TODO(rouault): Explain or delete.
        // if( psJob )
        //    return psJob;
    }
}
