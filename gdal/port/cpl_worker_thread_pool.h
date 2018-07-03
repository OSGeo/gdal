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
#include <vector>

/**
 * \file cpl_worker_thread_pool.h
 *
 * Class to manage a pool of worker threads.
 * @since GDAL 2.1
 */

#ifndef DOXYGEN_SKIP
class CPLWorkerThreadPool;

typedef struct
{
    CPLThreadFunc  pfnFunc;
    void          *pData;
} CPLWorkerThreadJob;

typedef struct
{
    CPLThreadFunc        pfnInitFunc;
    void                *pInitData;
    CPLWorkerThreadPool *poTP;
    CPLJoinableThread   *hThread;
    int                  bMarkedAsWaiting;
    // CPLWorkerThreadJob  *psNextJob;

    CPLMutex            *hMutex;
    CPLCond             *hCond;
} CPLWorkerThread;

typedef enum
{
    CPLWTS_OK,
    CPLWTS_STOP,
    CPLWTS_ERROR
} CPLWorkerThreadState;
#endif  // ndef DOXYGEN_SKIP

/** Pool of worker threads */
class CPL_DLL CPLWorkerThreadPool
{
        CPL_DISALLOW_COPY_ASSIGN(CPLWorkerThreadPool)

        std::vector<CPLWorkerThread> aWT{};
        CPLCond* hCond = nullptr;
        CPLMutex* hMutex = nullptr;
        volatile CPLWorkerThreadState eState = CPLWTS_OK;
        CPLList* psJobQueue = nullptr;
        volatile int nPendingJobs = 0;

        CPLList* psWaitingWorkerThreadsList = nullptr;
        int nWaitingWorkerThreads = 0;

        static void WorkerThreadFunction(void* user_data);

        void DeclareJobFinished();
        CPLWorkerThreadJob* GetNextJob(CPLWorkerThread* psWorkerThread);

    public:
        CPLWorkerThreadPool();
       ~CPLWorkerThreadPool();

        bool Setup(int nThreads,
                   CPLThreadFunc pfnInitFunc,
                   void** pasInitData);
        bool Setup(int nThreads,
                   CPLThreadFunc pfnInitFunc,
                   void** pasInitData,
                   bool bWaitallStarted);
        bool SubmitJob(CPLThreadFunc pfnFunc, void* pData);
        bool SubmitJobs(CPLThreadFunc pfnFunc, const std::vector<void*>& apData);
        void WaitCompletion(int nMaxRemainingJobs = 0);
        void WaitEvent();

        /** Return the number of threads setup */
        int GetThreadCount() const { return static_cast<int>(aWT.size()); }
};

#endif // CPL_WORKER_THREAD_POOL_H_INCLUDED_
