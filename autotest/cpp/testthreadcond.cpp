/******************************************************************************
 *
 * Project:  GDAL Core
 * Purpose:  Test thread API
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2012, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef DEBUG
#define DEBUG
#endif

#include "cpl_multiproc.h"
#include "cpl_string.h"

#include "gtest_include.h"

namespace
{

// ---------------------------------------------------------------------------

CPLCond *hCond = nullptr;
CPLCond *hCondJobFinished = nullptr;
CPLMutex *hClientMutex = nullptr;

struct _JobItem
{
    int nJobNumber;
    struct _JobItem *psNext;
};
typedef struct _JobItem JobItem;

JobItem *psJobList = nullptr;
int nJobListSize = 0;
int nThreadTotal = 0;
int bProducedFinished = 0;
int bVerbose = FALSE;

static void ProducerThread(void * /* unused */)
{
    int i;
    int jobNumber = 0;
    JobItem *psItem;

    while (jobNumber < 1000)
    {
        CPLAcquireMutex(hClientMutex, 1000.0);

        for (i = 0; i < nThreadTotal; i++)
        {
            jobNumber++;
            nJobListSize++;
            psItem = (JobItem *)malloc(sizeof(JobItem));
            psItem->nJobNumber = jobNumber;
            psItem->psNext = psJobList;
            psJobList = psItem;
        }

        CPLCondBroadcast(hCond);

        while (nJobListSize > nThreadTotal)
        {
            CPLCondWait(hCondJobFinished, hClientMutex);
        }
        CPLReleaseMutex(hClientMutex);
    }

    CPLAcquireMutex(hClientMutex, 1000.0);
    bProducedFinished = 1;
    CPLCondBroadcast(hCond);
    CPLReleaseMutex(hClientMutex);
}

static void ConsumerThread(void *pIndex)
{
    int nJobNumber;
    int nThreadIndex;
    JobItem *psNext;

    nThreadIndex = *(int *)pIndex;
    free(pIndex);

    if (bVerbose)
        printf("Thread %d created\n", nThreadIndex);

    nThreadTotal++;

    while (TRUE)
    {
        CPLAcquireMutex(hClientMutex, 1000.0);
        while (psJobList == nullptr && !bProducedFinished)
            CPLCondWait(hCond, hClientMutex);
        if (bProducedFinished)
        {
            CPLReleaseMutex(hClientMutex);
            break;
        }

        nJobNumber = psJobList->nJobNumber;
        psNext = psJobList->psNext;
        free(psJobList);
        psJobList = psNext;
        CPLReleaseMutex(hClientMutex);

        if (bVerbose)
            printf("Thread %d consumed job %d\n", nThreadIndex, nJobNumber);

        CPLAcquireMutex(hClientMutex, 1000.0);
        nJobListSize--;
        CPLCondSignal(hCondJobFinished);
        CPLReleaseMutex(hClientMutex);
    }
}

TEST(testthreadcond, test)
{
    int i;
    CPLJoinableThread *apThreads[10];

    bVerbose = CPLTestBool(CPLGetConfigOption("VERBOSE", "NO"));

    hCond = CPLCreateCond();
    hCondJobFinished = CPLCreateCond();

    hClientMutex = CPLCreateMutex();
    CPLReleaseMutex(hClientMutex);

    CPLCreateThread(ProducerThread, nullptr);

    for (i = 0; i < 10; i++)
    {
        int *pi = (int *)malloc(sizeof(int));
        *pi = i;
        apThreads[i] = CPLCreateJoinableThread(ConsumerThread, pi);
    }

    for (i = 0; i < 10; i++)
    {
        CPLJoinThread(apThreads[i]);
    }

    CPLDestroyCond(hCond);
    CPLDestroyCond(hCondJobFinished);
    CPLDestroyMutex(hClientMutex);
}

}  // namespace
