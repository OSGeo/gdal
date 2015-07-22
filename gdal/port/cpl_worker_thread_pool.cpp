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

#include "cpl_worker_thread_pool.h"
#include "cpl_conv.h"

/************************************************************************/
/*                         CPLWorkerThreadPool()                        */
/************************************************************************/

/** Instanciate a new pool of worker threads.
 *
 * The pool is in an uninitialized state after this call. The Setup() method
 * must be called.
 */
CPLWorkerThreadPool::CPLWorkerThreadPool()
{
    hCond = NULL;
    hMutex = CPLCreateMutexEx(CPL_MUTEX_REGULAR);
    CPLReleaseMutex(hMutex);
    bStop = FALSE;
    psJobQueue = NULL;
    nPendingJobs = 0;
    psWaitingWorkerThreadsList = NULL;
    nWaitingWorkerThreads = 0;
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

        bStop = TRUE;

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

    while( TRUE )
    {
        CPLWorkerThreadJob* psJob = poTP->GetNextJob(psWT);
        if( psJob == NULL )
            break;

        if( psJob->pfnFunc )
        {
            psJob->pfnFunc(psJob->pData);
        }
        CPLFree(psJob);
        //CPLDebug("JOB", "%p finished a job", psWT);
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
 */
void CPLWorkerThreadPool::SubmitJob(CPLThreadFunc pfnFunc, void* pData)
{
    CPLAssert( aWT.size() > 0 );

    CPLWorkerThreadJob* psJob = (CPLWorkerThreadJob*)CPLMalloc(sizeof(CPLWorkerThreadJob));
    psJob->pfnFunc = pfnFunc;
    psJob->pData = pData;

    CPLList* psItem = (CPLList*) CPLMalloc(sizeof(CPLList));
    psItem->pData = psJob;
    
    CPLAcquireMutex(hMutex, 1000.0);

    psItem->psNext = psJobQueue;
    psJobQueue = psItem;
    nPendingJobs ++;

    if( psWaitingWorkerThreadsList )
    {
        CPLWorkerThread* psWorkerThread;

        psWorkerThread = (CPLWorkerThread*)psWaitingWorkerThreadsList->pData;
        
        CPLAssert( psWorkerThread->bMarkedAsWaiting );
        psWorkerThread->bMarkedAsWaiting = FALSE;
        
        CPLList* psNext = psWaitingWorkerThreadsList->psNext;
        CPLList* psToFree = psWaitingWorkerThreadsList;
        psWaitingWorkerThreadsList = psNext;
        nWaitingWorkerThreads --;

        //CPLAssert( CPLListCount(psWaitingWorkerThreadsList) == nWaitingWorkerThreads);

        //CPLDebug("JOB", "Waking up %p", psWorkerThread);
        CPLAcquireMutex(psWorkerThread->hMutex, 1000.0);
        CPLReleaseMutex(hMutex);
        CPLCondSignal(psWorkerThread->hCond);
        CPLReleaseMutex(psWorkerThread->hMutex);
        
        CPLFree(psToFree);
    }
    else
        CPLReleaseMutex(hMutex);
    
}

/************************************************************************/
/*                             SubmitJobs()                              */
/************************************************************************/

/** Queue several jobs
 *
 * @param pfnFunc Function to run for the job.
 * @param apData User data instances to pass to the job function.
 */
void CPLWorkerThreadPool::SubmitJobs(CPLThreadFunc pfnFunc, const std::vector<void*>& apData)
{
    CPLAssert( aWT.size() > 0 );
    
    CPLAcquireMutex(hMutex, 1000.0);

    for(size_t i=0;i<apData.size();i++)
    {
        CPLWorkerThreadJob* psJob = (CPLWorkerThreadJob*)CPLMalloc(sizeof(CPLWorkerThreadJob));
        psJob->pfnFunc = pfnFunc;
        psJob->pData = apData[i];

        CPLList* psItem = (CPLList*) CPLMalloc(sizeof(CPLList));
        psItem->pData = psJob;

        psItem->psNext = psJobQueue;
        psJobQueue = psItem;
        nPendingJobs ++;
    }
    CPLReleaseMutex(hMutex);
    
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
            nWaitingWorkerThreads --;

            //CPLAssert( CPLListCount(psWaitingWorkerThreadsList) == nWaitingWorkerThreads);

            //CPLDebug("JOB", "Waking up %p", psWorkerThread);
            CPLAcquireMutex(psWorkerThread->hMutex, 1000.0);
            
            //CPLAssert(psWorkerThread->psNextJob == NULL);
            //psWorkerThread->psNextJob = (CPLWorkerThreadJob*)psJobQueue->pData;
            //psNext = psJobQueue->psNext;
            //CPLFree(psJobQueue);
            //psJobQueue = psNext;
            
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
    while( TRUE )
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
 * @return TRUE if initialization was successful.
 */
int CPLWorkerThreadPool::Setup(int nThreads,
                            CPLThreadFunc pfnInitFunc,
                            void** pasInitData)
{
    CPLAssert( nThreads > 0 );

    hCond = CPLCreateCond();
    if( hCond == NULL )
        return FALSE;

    aWT.resize(nThreads);
    for(int i=0;i<nThreads;i++)
    {
        aWT[i].pfnInitFunc = pfnInitFunc;
        aWT[i].pInitData = pasInitData ? pasInitData[i] : NULL;
        aWT[i].poTP = this;
        
        aWT[i].hMutex = CPLCreateMutexEx(CPL_MUTEX_REGULAR);
        CPLReleaseMutex(aWT[i].hMutex);
        aWT[i].hCond = CPLCreateCond();
        
        aWT[i].bMarkedAsWaiting = FALSE;
        aWT[i].psNextJob = NULL;
        
        aWT[i].hThread = CPLCreateJoinableThread(WorkerThreadFunction, &(aWT[i]));
    }
    
    // Wait all threads to be started
    while( TRUE )
    {
        CPLAcquireMutex(hMutex, 1000.0);
        int nWaitingWorkerThreadsLocal = nWaitingWorkerThreads;
        if( nWaitingWorkerThreadsLocal < nThreads )
            CPLCondWait(hCond, hMutex);
        CPLReleaseMutex(hMutex);
        if( nWaitingWorkerThreadsLocal == nThreads )
            break;
    }

    return TRUE;
}

/************************************************************************/
/*                          DeclareJobFinished()                        */
/************************************************************************/

void CPLWorkerThreadPool::DeclareJobFinished()
{
    CPLAcquireMutex(hMutex, 1000.0);
    nPendingJobs --;
    CPLCondSignal(hCond);
    CPLReleaseMutex(hMutex);
}

/************************************************************************/
/*                             GetNextJob()                             */
/************************************************************************/

CPLWorkerThreadJob* CPLWorkerThreadPool::GetNextJob(CPLWorkerThread* psWorkerThread)
{
    while(TRUE)
    {
        CPLAcquireMutex(hMutex, 1000.0);
        if( bStop )
        {
            CPLReleaseMutex(hMutex);
            return NULL;
        }
        CPLList* psTopJobIter = psJobQueue;
        if( psTopJobIter )
        {
            psJobQueue = psTopJobIter->psNext;

            //CPLDebug("JOB", "%p got a job", psWorkerThread);
            CPLWorkerThreadJob* psJob = (CPLWorkerThreadJob*)psTopJobIter->pData;
            CPLReleaseMutex(hMutex);
            CPLFree(psTopJobIter);
            return psJob;
        }

        if( !psWorkerThread->bMarkedAsWaiting )
        {
            psWorkerThread->bMarkedAsWaiting = TRUE;
            nWaitingWorkerThreads ++;
            CPLAssert(nWaitingWorkerThreads <= (int)aWT.size());
            
            CPLList* psItem = (CPLList*) CPLMalloc(sizeof(CPLList));
            psItem->pData = psWorkerThread;
            psItem->psNext = psWaitingWorkerThreadsList;
            psWaitingWorkerThreadsList = psItem;
        
            //CPLAssert( CPLListCount(psWaitingWorkerThreadsList) == nWaitingWorkerThreads);
        }

        CPLCondSignal(hCond);

        CPLAcquireMutex(psWorkerThread->hMutex, 1000.0);
        //CPLDebug("JOB", "%p sleeping", psWorkerThread);
        CPLReleaseMutex(hMutex);
    
        CPLCondWait( psWorkerThread->hCond, psWorkerThread->hMutex );
        
        //CPLWorkerThreadJob* psJob = psWorkerThread->psNextJob;
        //psWorkerThread->psNextJob = NULL;
        
        CPLReleaseMutex(psWorkerThread->hMutex);
        
        //if( psJob )
        //    return psJob;
    }
}
