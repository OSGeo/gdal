/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Core
 * Purpose:  Test thread API
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2012, Even Rouault <even dot rouault at mines-paris dot org>
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "cpl_multiproc.h"

void* hCond = NULL;
void* hCondJobFinished = NULL;
void* hClientMutex = NULL;

struct _JobItem
{
    int nJobNumber;
    struct _JobItem* psNext;
};
typedef struct _JobItem JobItem;

JobItem* psJobList = NULL;
int nJobListSize = 0;
int nThreadTotal = 0;
int bProducedFinished = 0;
int bVerbose = FALSE;

void ProducerThread(void *unused)
{
    int i;
    int jobNumber = 0;
    JobItem* psItem;

    while(jobNumber < 1000)
    {
        CPLAcquireMutex(hClientMutex, 1000.0);

        for(i=0;i<nThreadTotal;i++)
        {
            jobNumber ++;
            nJobListSize ++;
            psItem = (JobItem*)malloc(sizeof(JobItem));
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

void ConsumerThread(void* pIndex)
{
    int nJobNumber;
    int nThreadIndex;
    JobItem* psNext;

    nThreadIndex = *(int*)pIndex;
    free(pIndex);

    if (bVerbose)
        printf("Thread %d created\n", nThreadIndex);

    nThreadTotal ++;

    while(TRUE)
    {
        CPLAcquireMutex(hClientMutex, 1000.0);
        while(psJobList == NULL && !bProducedFinished)
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
        nJobListSize --;
        CPLCondSignal(hCondJobFinished);
        CPLReleaseMutex(hClientMutex);
    }
}

int main(int argc, char* argv[])
{
    int i;
    void* apThreads[10];

    for(i = 0; i < argc; i++)
    {
        if( EQUAL(argv[i], "-verbose") )
            bVerbose = TRUE;
    }

    hCond = CPLCreateCond();
    hCondJobFinished = CPLCreateCond();

    hClientMutex = CPLCreateMutex();
    CPLReleaseMutex(hClientMutex);

    CPLCreateThread(ProducerThread, NULL);

    for(i = 0; i < 10;i++)
    {
        int* pi = (int*)malloc(sizeof(int));
        *pi = i;
        apThreads[i] = CPLCreateJoinableThread(ConsumerThread, pi);
    }

    for(i = 0; i < 10;i++)
    {
        CPLJoinThread(apThreads[i]);
    }

    CPLDestroyCond(hCond);
    CPLDestroyCond(hCondJobFinished);
    CPLDestroyMutex(hClientMutex);
    return 0;
}
