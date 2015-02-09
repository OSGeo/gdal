/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Core
 * Purpose:  Test block cache under multi-threading
 * Author:   Even Rouault, <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2015, Even Rouault <even dot rouault at spatialys dot com>
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

#include <stdlib.h>
#include <assert.h>
#include <vector>

#include "cpl_multiproc.h"
#include "gdal_priv.h"

// Not sure if all pthread implementations have spin lock but that's good
// enough for now
#ifdef CPL_MULTIPROC_PTHREAD
#if !defined(__APPLE__)
#define HAVE_SPINLOCK
#endif
#endif

#ifdef HAVE_SPINLOCK
#include <pthread.h>
pthread_spinlock_t* psLock = NULL;
#endif

void Usage()
{
    printf("Usage: testblockcache [-threads X] [-loops X] [-strategy random|line|block]\n");
    printf("                      [-migrate] [ filename |\n");
    printf("                       [[-xsize val] [-ysize val] [-bands val] [-co key=value]*\n");
    printf("                       [[-memdriver] | [-ondisk]] [-check]] ]\n");
    exit(1);
}

int nLoops = 1;
const char* pszDataset = NULL;
int bCheck = FALSE;

typedef enum
{
    STRATEGY_RANDOM,
    STRATEGY_LINE,
    STRATEGY_BLOCK,
} Strategy;

typedef struct _Request Request;
struct _Request
{
    int nXOff, nYOff, nXWin, nYWin;
    int nBands;
    Request* psNext;
};

typedef struct _Resource Resource;
struct _Resource
{
    GDALDataset* poDS;
    void* pBuffer;
    Resource* psNext;
    Resource* psPrev;
};

typedef struct
{
    GDALDataset* poDS;
    Request* psRequestList;
    int nBufferSize;
} ThreadDescription;

static void* hMutex = NULL;
static Request* psGlobalRequestList = NULL;
static Resource* psGlobalResourceList = NULL;
static Resource* psGlobalResourceLast = NULL;

/* according to rand() man page, POSIX.1-2001 proposes the following implementation */
/* RAND_MAX assumed to be 32767 */
#define MYRAND_MAX 32767
int myrand_r(unsigned long* pseed) {
    *pseed = *pseed * 1103515245 + 12345;
    return((unsigned)(*pseed/65536) % (MYRAND_MAX+1));
}

static void Check(GByte* pBuffer, int nXSize, int nYSize, int nBands,
                  int nXOff, int nYOff, int nXWin, int nYWin)
{
    for(int iBand=0;iBand<nBands;iBand++)
    {
        for(int iY=0;iY<nYWin;iY++)
        {
            for(int iX=0;iX<nXWin;iX++)
            {
                unsigned long seed = iBand * nXSize * nYSize + (iY + nYOff) * nXSize + iX + nXOff;
                GByte expected = (GByte)myrand_r(&seed);
                assert( pBuffer[iBand * nXWin * nYWin + iY * nXWin + iX] == expected );
            }
        }
    }
}

static void ReadRaster(GDALDataset* poDS, int nXSize, int nYSize, int nBands,
                       GByte* pBuffer, int nXOff, int nYOff, int nXWin, int nYWin)
{
    poDS->RasterIO(GF_Read, nXOff, nYOff, nXWin, nYWin,
                    pBuffer, nXWin, nYWin,
                    GDT_Byte,
                    nBands, NULL,
                    0, 0, 0);
    if( bCheck )
    {
        Check(pBuffer, nXSize, nYSize, nBands,
              nXOff, nYOff, nXWin, nYWin);
    }
}

static void AddRequest(Request*& psRequestList, Request*& psRequestLast,
                       int nXOff, int nYOff, int nXWin, int nYWin, int nBands)
{
    Request* psRequest = (Request*)CPLMalloc(sizeof(Request));
    psRequest->nXOff = nXOff;
    psRequest->nYOff = nYOff;
    psRequest->nXWin = nXWin;
    psRequest->nYWin = nYWin;
    psRequest->nBands = nBands;
    if( psRequestLast )
        psRequestLast->psNext = psRequest;
    else
        psRequestList = psRequest;
    psRequestLast = psRequest;
    psRequest->psNext = NULL;
}

static Request* GetNextRequest(Request*& psRequestList)
{
    if( hMutex ) CPLAcquireMutex(hMutex, 1000.0);
#ifdef HAVE_SPINLOCK
    if( psLock ) pthread_spin_lock(psLock);
#endif
    Request* psRet = psRequestList;
    if( psRequestList )
    {
        psRequestList = psRequestList->psNext;
        psRet->psNext = NULL;
    }
    if( hMutex ) CPLReleaseMutex(hMutex);
#ifdef HAVE_SPINLOCK
    if( psLock ) pthread_spin_unlock(psLock);
#endif
    return psRet;
}

static Resource* AcquireFirstResource()
{
    if( hMutex ) CPLAcquireMutex(hMutex, 1000.0);
#ifdef HAVE_SPINLOCK
    if( psLock ) pthread_spin_lock(psLock);
#endif
    Resource* psRet = psGlobalResourceList;
    psGlobalResourceList = psGlobalResourceList->psNext;
    if( psGlobalResourceList )
        psGlobalResourceList->psPrev = NULL;
    else
        psGlobalResourceLast = NULL;
    psRet->psNext = NULL;
    assert(psRet->psPrev == NULL);
    if( hMutex ) CPLReleaseMutex(hMutex);
#ifdef HAVE_SPINLOCK
    if( psLock ) pthread_spin_unlock(psLock);
#endif
    return psRet;
}

static void PutResourceAtEnd(Resource* psResource)
{
    if( hMutex ) CPLAcquireMutex(hMutex, 1000.0);
#ifdef HAVE_SPINLOCK
    if( psLock ) pthread_spin_lock(psLock);
#endif
    psResource->psPrev = psGlobalResourceLast;
    psResource->psNext = NULL;
    if( psGlobalResourceList == NULL )
        psGlobalResourceList = psResource;
    else
        psGlobalResourceLast->psNext = psResource;
    psGlobalResourceLast = psResource;
    if( hMutex ) CPLReleaseMutex(hMutex);
#ifdef HAVE_SPINLOCK
    if( psLock ) pthread_spin_unlock(psLock);
#endif
}

static void ThreadFuncDedicatedDataset(void* _psThreadDescription)
{
    ThreadDescription* psThreadDescription = (ThreadDescription*)_psThreadDescription;
    int nXSize = psThreadDescription->poDS->GetRasterXSize();
    int nYSize = psThreadDescription->poDS->GetRasterYSize();
    void* pBuffer = CPLMalloc(psThreadDescription->nBufferSize);
    while( psThreadDescription->psRequestList != NULL )
    {
        Request* psRequest = GetNextRequest(psThreadDescription->psRequestList);
        ReadRaster(psThreadDescription->poDS, nXSize, nYSize, psRequest->nBands,
                   (GByte*)pBuffer, psRequest->nXOff, psRequest->nYOff, psRequest->nXWin, psRequest->nYWin);
        CPLFree(psRequest);
    }
    CPLFree(pBuffer);
}

static void ThreadFuncWithMigration(void* _unused)
{
    Request* psRequest;
    while( (psRequest = GetNextRequest(psGlobalRequestList)) != NULL )
    {
        Resource* psResource = AcquireFirstResource();
        assert(psResource);
        int nXSize = psResource->poDS->GetRasterXSize();
        int nYSize = psResource->poDS->GetRasterYSize();
        ReadRaster(psResource->poDS, nXSize, nYSize, psRequest->nBands,
                   (GByte*)psResource->pBuffer,
                   psRequest->nXOff, psRequest->nYOff, psRequest->nXWin, psRequest->nYWin);
        CPLFree(psRequest);
        PutResourceAtEnd(psResource);
    }
}

static int CreateRandomStrategyRequests(GDALDataset* poDS,
                                        Request*& psRequestList,
                                        Request*& psRequestLast)
{
    unsigned long seed = 1;
    int nXSize = poDS->GetRasterXSize();
    int nYSize = poDS->GetRasterYSize();
    int nMaxXWin = MIN(1000, nXSize/10+1);
    int nMaxYWin = MIN(1000, nYSize/10+1);
    int nQueriedBands = MIN(4, poDS->GetRasterCount());
    int nAverageIterationsToReadWholeFile =
        ((nXSize + nMaxXWin/2-1) / (nMaxXWin/2)) * ((nYSize + nMaxYWin/2-1) / (nMaxYWin/2));
    int nLocalLoops = nLoops * nAverageIterationsToReadWholeFile;
    for(int iLoop=0;iLoop<nLocalLoops;iLoop++)
    {
        int nXOff = (int)((GIntBig)myrand_r(&seed) * (nXSize-1) / MYRAND_MAX);
        int nYOff = (int)((GIntBig)myrand_r(&seed) * (nYSize-1) / MYRAND_MAX);
        int nXWin = 1+(int)((GIntBig)myrand_r(&seed) * nMaxXWin / MYRAND_MAX);
        int nYWin = 1+(int)((GIntBig)myrand_r(&seed) * nMaxYWin / MYRAND_MAX);
        if( nXOff + nXWin > nXSize )
            nXWin = nXSize - nXOff;
        if( nYOff + nYWin > nYSize )
            nYWin = nYSize - nYOff;
        AddRequest(psRequestList, psRequestLast, nXOff, nYOff, nXWin, nYWin, nQueriedBands);
    }
    return nQueriedBands * nMaxXWin * nMaxYWin;
}

static int CreateLineStrategyRequests(GDALDataset* poDS,
                                      Request*& psRequestList,
                                      Request*& psRequestLast)
{
    int nXSize = poDS->GetRasterXSize();
    int nYSize = poDS->GetRasterYSize();
    int nQueriedBands = MIN(4, poDS->GetRasterCount());
    for(int iLoop=0;iLoop<nLoops;iLoop++)
    {
        for(int nYOff=0;nYOff<nYSize;nYOff++)
        {
            AddRequest(psRequestList, psRequestLast, 0, nYOff, nXSize, 1, nQueriedBands);
        }
    }
    return nQueriedBands * nXSize;
}

static int CreateBlockStrategyRequests(GDALDataset* poDS,
                                       Request*& psRequestList,
                                       Request*& psRequestLast)
{
    int nXSize = poDS->GetRasterXSize();
    int nYSize = poDS->GetRasterYSize();
    int nMaxXWin = MIN(1000, nXSize/10+1);
    int nMaxYWin = MIN(1000, nYSize/10+1);
    int nQueriedBands = MIN(4, poDS->GetRasterCount());
    for(int iLoop=0;iLoop<nLoops;iLoop++)
    {
        for(int nYOff=0;nYOff<nYSize;nYOff+=nMaxYWin)
        {
            int nReqYSize = (nYOff + nMaxYWin > nYSize) ? nYSize - nYOff : nMaxYWin;
            for(int nXOff=0;nXOff<nXSize;nXOff+=nMaxXWin)
            {
                int nReqXSize = (nXOff + nMaxXWin > nXSize) ? nXSize - nXOff : nMaxXWin;
                AddRequest(psRequestList, psRequestLast, nXOff, nYOff, nReqXSize, nReqYSize, nQueriedBands);
            }
        }
    }
    return nQueriedBands * nMaxXWin * nMaxYWin;
}

int main(int argc, char* argv[])
{
    int i;
    int nThreads = CPLGetNumCPUs();
    std::vector<void*> apsThreads;
    Strategy eStrategy = STRATEGY_RANDOM;
    int bNewDatasetOption = FALSE;
    int nXSize = 5000;
    int nYSize = 5000;
    int nBands = 4;
    char** papszOptions = NULL;
    int bOnDisk = FALSE;
    std::vector<ThreadDescription> asThreadDescription;
    int bMemDriver = FALSE;
    GDALDataset* poMEMDS = NULL;
    int bMigrate = FALSE;

    argc = GDALGeneralCmdLineProcessor( argc, &argv, 0 );

    GDALAllRegister();

    for(i = 1; i < argc; i++)
    {
        if( EQUAL(argv[i], "-threads") && i + 1 < argc)
        {
            i ++;
            nThreads = atoi(argv[i]);
        }
        else if( EQUAL(argv[i], "-loops") && i + 1 < argc)
        {
            i ++;
            nLoops = atoi(argv[i]);
            if( nLoops <= 0 )
                nLoops = INT_MAX;
        }
        else if( EQUAL(argv[i], "-strategy") && i + 1 < argc)
        {
            i ++;
            if( EQUAL(argv[i], "random") )
                eStrategy = STRATEGY_RANDOM;
            else if( EQUAL(argv[i], "line") )
                eStrategy = STRATEGY_LINE;
            else if( EQUAL(argv[i], "block") )
                eStrategy = STRATEGY_BLOCK;
            else
                Usage();
        }
        else if( EQUAL(argv[i], "-xsize") && i + 1 < argc)
        {
            i ++;
            nXSize = atoi(argv[i]);
            bNewDatasetOption = TRUE;
        }
        else if( EQUAL(argv[i], "-ysize") && i + 1 < argc)
        {
            i ++;
            nYSize = atoi(argv[i]);
            bNewDatasetOption = TRUE;
        }
        else if( EQUAL(argv[i], "-bands") && i + 1 < argc)
        {
            i ++;
            nBands = atoi(argv[i]);
            bNewDatasetOption = TRUE;
        }
        else if( EQUAL(argv[i], "-co") && i + 1 < argc)
        {
            i ++;
            papszOptions = CSLAddString(papszOptions, argv[i]);
            bNewDatasetOption = TRUE;
        }
        else if( EQUAL(argv[i], "-ondisk"))
        {
            bOnDisk = TRUE;
            bNewDatasetOption = TRUE;
        }
        else if( EQUAL(argv[i], "-check"))
        {
            bCheck = TRUE;
            bNewDatasetOption = TRUE;
        }
        else if( EQUAL(argv[i], "-memdriver"))
        {
            bMemDriver = TRUE;
            bNewDatasetOption = TRUE;
        }
        else if( EQUAL(argv[i], "-migrate"))
            bMigrate = TRUE;
        else if( argv[i][0] == '-' )
            Usage();
        else if( pszDataset == NULL )
            pszDataset = argv[i];
        else
        {
            Usage();
        }
    }

    if( pszDataset != NULL && bNewDatasetOption )
        Usage();

    CPLDebug("TEST", "Using %d threads", nThreads);

    int bCreatedDataset = FALSE;
    if( pszDataset == NULL )
    {
        bCreatedDataset = TRUE;
        if( bOnDisk )
            pszDataset = "/tmp/tmp.tif";
        else
            pszDataset = "/vsimem/tmp.tif";
        GDALDataset* poDS = ((GDALDriver*)GDALGetDriverByName((bMemDriver) ? "MEM" : "GTiff"))->Create(pszDataset,
                                nXSize, nYSize, nBands, GDT_Byte, papszOptions);
        if( bCheck )
        {
            GByte* pabyLine = (GByte*) VSIMalloc(nBands * nXSize);
            for(int iY=0;iY<nYSize;iY++)
            {
                for(int iX=0;iX<nXSize;iX++)
                {
                    for(int iBand=0;iBand<nBands;iBand++)
                    {
                        unsigned long seed = iBand * nXSize * nYSize + iY * nXSize + iX;
                        pabyLine[iBand * nXSize + iX] = (GByte)myrand_r(&seed);
                    }
                }
                poDS->RasterIO(GF_Write, 0, iY, nXSize, 1,
                               pabyLine, nXSize, 1,
                               GDT_Byte,
                               nBands, NULL,
                               0, 0, 0);
            }
            VSIFree(pabyLine);
        }
        if( bMemDriver ) 
            poMEMDS = poDS;
        else
            GDALClose(poDS);
    }
    else
    {
        bCheck = FALSE;
    }
    CSLDestroy(papszOptions);
    papszOptions = NULL;
    
    Request* psGlobalRequestLast = NULL;

    for(i = 0; i < nThreads; i++ )
    {
        GDALDataset* poDS;
        // Since GDAL 2.0, the MEM driver is thread-safe, i.e. does not use the block
        // cache, but only for operations not involving resampling, which is
        // the case here
        if( poMEMDS ) 
            poDS = poMEMDS;
        else
        {
            poDS = (GDALDataset*)GDALOpen(pszDataset, GA_ReadOnly);
            if( poDS == NULL )
                exit(1);
        }
        if( bMigrate )
        {
            Resource* psResource = (Resource*)CPLMalloc(sizeof(Resource));
            psResource->poDS = poDS;
            int nBufferSize;
            if( eStrategy == STRATEGY_RANDOM )
                nBufferSize = CreateRandomStrategyRequests(
                        poDS, psGlobalRequestList, psGlobalRequestLast);
            else if( eStrategy == STRATEGY_LINE )
                nBufferSize = CreateLineStrategyRequests(
                        poDS, psGlobalRequestList, psGlobalRequestLast);
            else
                nBufferSize = CreateBlockStrategyRequests(
                        poDS, psGlobalRequestList, psGlobalRequestLast);
            psResource->pBuffer = CPLMalloc(nBufferSize);
            PutResourceAtEnd(psResource);
        }
        else
        {
            ThreadDescription sThreadDescription;
            sThreadDescription.poDS = poDS;
            sThreadDescription.psRequestList = NULL;
            Request* psRequestLast = NULL;
            if( eStrategy == STRATEGY_RANDOM )
                sThreadDescription.nBufferSize = CreateRandomStrategyRequests(
                        poDS, sThreadDescription.psRequestList, psRequestLast);
            else if( eStrategy == STRATEGY_LINE )
                sThreadDescription.nBufferSize = CreateLineStrategyRequests(
                        poDS, sThreadDescription.psRequestList, psRequestLast);
            else
                sThreadDescription.nBufferSize = CreateBlockStrategyRequests(
                        poDS, sThreadDescription.psRequestList, psRequestLast);
            asThreadDescription.push_back(sThreadDescription);
        }
    }

    if( bCreatedDataset && poMEMDS == NULL )
    {
        CPLPushErrorHandler(CPLQuietErrorHandler);
        VSIUnlink(pszDataset);
        CPLPopErrorHandler();
    }
    
    if( bMigrate )
    {
#ifdef HAVE_SPINLOCK
        psLock = (pthread_spinlock_t*)CPLMalloc(sizeof(pthread_spinlock_t));
        pthread_spin_init(psLock, PTHREAD_PROCESS_PRIVATE);
#else
        hMutex = CPLCreateMutex();
        CPLReleaseMutex(hMutex);
#endif
    }

    for(i = 0; i < nThreads; i++ )
    {
        void* pThread;
        if( bMigrate )
            pThread = CPLCreateJoinableThread(ThreadFuncWithMigration, NULL);
        else
            pThread = CPLCreateJoinableThread(ThreadFuncDedicatedDataset,
                                              &(asThreadDescription[i]));
        apsThreads.push_back(pThread);
    }
    for(i = 0; i < nThreads; i++ )
    {
        CPLJoinThread(apsThreads[i]);
        if( !bMigrate && poMEMDS == NULL )
            GDALClose(asThreadDescription[i].poDS);
    }
    while( psGlobalResourceList != NULL )
    {
        CPLFree( psGlobalResourceList->pBuffer);
        Resource* psNext = psGlobalResourceList->psNext;
        CPLFree( psGlobalResourceList );
        psGlobalResourceList = psNext;
    }
    if( hMutex )
        CPLDestroyMutex(hMutex);
#ifdef HAVE_SPINLOCK
    if( psLock )
    {
        pthread_spin_destroy(psLock);
        CPLFree((void*)psLock);
    }
#endif

    if( bCreatedDataset && poMEMDS == NULL  )
    {
        CPLPushErrorHandler(CPLQuietErrorHandler);
        VSIUnlink(pszDataset);
        CPLPopErrorHandler();
    }
    if( poMEMDS )
        GDALClose(poMEMDS);

    GDALDestroyDriverManager();
    CSLDestroy( argv );

    return 0;
}

