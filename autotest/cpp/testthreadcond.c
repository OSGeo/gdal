
#include "cpl_multiproc.h"

void* hCond = NULL;
void* hCondJobFinished = NULL;
void* hCondProducerFinished = NULL;
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

void ProducerThread(void *unused)
{
    int i;
    int jobNumber = 0;
    JobItem* psItem;

    while(jobNumber < 10000)
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

        CPLReleaseMutex(hClientMutex);

        CPLCondBroadcast(hCond);

        CPLAcquireMutex(hClientMutex, 1000.0);
        while (nJobListSize > nThreadTotal)
        {
            CPLCondWait(hCondJobFinished, hClientMutex);
        }
        CPLReleaseMutex(hClientMutex);
    }

    CPLAcquireMutex(hClientMutex, 1000.0);
    bProducedFinished = 1;
    CPLReleaseMutex(hClientMutex);

    CPLCondSignal(hCondProducerFinished);
}

void ConsumerThread(void* pIndex)
{
    int nJobNumber;
    int nThreadIndex;
    JobItem* psNext;

    nThreadIndex = *(int*)pIndex;
    printf("Thread %d created\n", nThreadIndex);

    nThreadTotal ++;

    while(TRUE)
    {
        CPLAcquireMutex(hClientMutex, 1000.0);
        while(psJobList == NULL)
            CPLCondWait(hCond, hClientMutex);

        nJobNumber = psJobList->nJobNumber;
        psNext = psJobList->psNext;
        free(psJobList);
        psJobList = psNext;
        CPLReleaseMutex(hClientMutex);

        printf("Thread %d consumed job %d\n", nThreadIndex, nJobNumber);

        CPLAcquireMutex(hClientMutex, 1000.0);
        nJobListSize --;
        CPLReleaseMutex(hClientMutex);

        CPLCondBroadcast(hCondJobFinished);
    }
}

int main(int argc, char* argv[])
{
    int i;

    hCond = CPLCreateCond();
    hCondJobFinished = CPLCreateCond();
    hCondProducerFinished = CPLCreateCond();

    hClientMutex = CPLCreateMutex();
    CPLReleaseMutex(hClientMutex);

    CPLCreateThread(ProducerThread, NULL);

    for(i = 0; i < 10;i++)
    {
        int* pi = (int*)malloc(sizeof(int));
        *pi = i;
        CPLCreateThread(ConsumerThread, pi);
    }

    CPLAcquireMutex(hClientMutex, 1000.0);
    while (!bProducedFinished)
        CPLCondWait(hCondProducerFinished, hClientMutex);
    while (nJobListSize > 0)
        CPLCondWait(hCondJobFinished, hClientMutex);
    CPLReleaseMutex(hClientMutex);

    CPLDestroyCond(hCond);
    CPLDestroyCond(hCondJobFinished);
    CPLDestroyCond(hCondProducerFinished);
    CPLDestroyMutex(hClientMutex);
    return 0;
}
