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

typedef struct
{
    CPLThreadFunc  pfnFunc;
    void          *pData;
} CPLWorkerThreadJob;

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
    hCondWarnSubmitter = NULL;
    hCondMutex = CPLCreateMutex();
    CPLReleaseMutex(hCondMutex);
    bStop = FALSE;
    psJobQueue = NULL;
    nPendingJobs = 0;
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

        CPLAcquireMutex(hCondMutex, 1000.0);
        bStop = TRUE;
        CPLCondBroadcast(hCond);
        CPLReleaseMutex(hCondMutex);

        for(size_t i=0;i<aWT.size();i++)
        {
            CPLJoinThread(aWT[i].hThread);
        }

        CPLDestroyCond(hCond);
    }
    if( hCondWarnSubmitter )
        CPLDestroyCond(hCondWarnSubmitter);
    CPLDestroyMutex(hCondMutex);
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
    CPLAcquireMutex(poTP->hCondMutex, 1000.0);
    poTP->nPendingJobs --;
    CPLCondSignal(poTP->hCondWarnSubmitter);
    CPLReleaseMutex(poTP->hCondMutex);

    while( TRUE )
    {
        CPLAcquireMutex(poTP->hCondMutex, 1000.0);
        int bStop = poTP->bStop;
        CPLList* psItem = poTP->psJobQueue;
        if( psItem )
        {
            poTP->psJobQueue = psItem->psNext;
            psItem->psNext = NULL;
        }
        else if( !bStop )
            CPLCondWait(poTP->hCond, poTP->hCondMutex);
        CPLReleaseMutex(poTP->hCondMutex);
        if( bStop )
            break;
        if( psItem )
        {
            CPLWorkerThreadJob* psJob = (CPLWorkerThreadJob*)psItem->pData;
            if( psJob && psJob->pfnFunc )
            {
                psJob->pfnFunc(psJob->pData);
            }
            CPLFree(psJob);
            CPLFree(psItem);

            CPLAcquireMutex(poTP->hCondMutex, 1000.0);
            poTP->nPendingJobs --;
            CPLCondSignal(poTP->hCondWarnSubmitter);
            CPLReleaseMutex(poTP->hCondMutex);
        }
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

    CPLAcquireMutex(hCondMutex, 1000.0);
    nPendingJobs ++;
    psItem->psNext = psJobQueue;
    psJobQueue = psItem;
    CPLCondSignal(hCond);
    CPLReleaseMutex(hCondMutex);
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
        CPLAcquireMutex(hCondMutex, 1000.0);
        int nPendingJobsLocal = nPendingJobs;
        if( nPendingJobsLocal > nMaxRemainingJobs )
            CPLCondWait(hCondWarnSubmitter, hCondMutex);
        CPLReleaseMutex(hCondMutex);
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
    hCondWarnSubmitter = CPLCreateCond();
    if( hCond == NULL || hCondWarnSubmitter == NULL )
        return FALSE;

    nPendingJobs = nThreads;

    aWT.resize(nThreads);
    for(int i=0;i<nThreads;i++)
    {
        aWT[i].pfnInitFunc = pfnInitFunc;
        aWT[i].pInitData = pasInitData ? pasInitData[i] : NULL;
        aWT[i].poTP = this;
        aWT[i].hThread = CPLCreateJoinableThread(WorkerThreadFunction, &(aWT[i]));
    }

    WaitCompletion();

    return TRUE;
}
