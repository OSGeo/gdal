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

void Usage()
{
    printf("Usage: testblockcache [-threads X] [-loops X] [-strategy random|line|block]\n");
    printf("                      [[-dataset filename] |\n");
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

static void ThreadFuncRandomStrategy(void* _poDS)
{
    unsigned long seed = 1;
    GDALDataset* poDS = (GDALDataset*)_poDS;
    int nXSize = poDS->GetRasterXSize();
    int nYSize = poDS->GetRasterYSize();
    int nMaxXWin = MIN(1000, nXSize/10+1);
    int nMaxYWin = MIN(1000, nYSize/10+1);
    int nQueriedBands = MIN(4, poDS->GetRasterCount());
    void* pBuffer = malloc(nQueriedBands * nMaxXWin * nMaxYWin);
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

        ReadRaster(poDS, nXSize, nYSize, nQueriedBands,
                   (GByte*)pBuffer, nXOff, nYOff, nXWin, nYWin);
    }
    free(pBuffer);
}

static void ThreadFuncLineStrategy(void* _poDS)
{
    GDALDataset* poDS = (GDALDataset*)_poDS;
    int nXSize = poDS->GetRasterXSize();
    int nYSize = poDS->GetRasterYSize();
    int nQueriedBands = MIN(4, poDS->GetRasterCount());
    void* pBuffer = malloc(nQueriedBands * nXSize);
    for(int iLoop=0;iLoop<nLoops;iLoop++)
    {
        for(int nYOff=0;nYOff<nYSize;nYOff++)
        {
            ReadRaster(poDS, nXSize, nYSize, nQueriedBands,
                       (GByte*)pBuffer, 0, nYOff, nXSize, 1);
        }
    }
    free(pBuffer);
}

static void ThreadFuncBlockStrategy(void* _poDS)
{
    GDALDataset* poDS = (GDALDataset*)_poDS;
    int nXSize = poDS->GetRasterXSize();
    int nYSize = poDS->GetRasterYSize();
    int nMaxXWin = MIN(1000, nXSize/10+1);
    int nMaxYWin = MIN(1000, nYSize/10+1);
    int nQueriedBands = MIN(4, poDS->GetRasterCount());
    void* pBuffer = malloc(nQueriedBands * nMaxXWin * nMaxYWin);
    for(int iLoop=0;iLoop<nLoops;iLoop++)
    {
        for(int nYOff=0;nYOff<nYSize;nYOff+=nMaxYWin)
        {
            int nReqYSize = (nYOff + nMaxYWin > nYSize) ? nYSize - nYOff : nMaxYWin;
            for(int nXOff=0;nXOff<nXSize;nXOff+=nMaxXWin)
            {
                int nReqXSize = (nXOff + nMaxXWin > nXSize) ? nXSize - nXOff : nMaxXWin;
                ReadRaster(poDS, nXSize, nYSize, nQueriedBands,
                           (GByte*)pBuffer, nXOff, nYOff, nReqXSize, nReqYSize);
            }
        }
    }
    free(pBuffer);
}


int main(int argc, char* argv[])
{
    int i;
    int nThreads = CPLGetNumCPUs();
    std::vector<void*> apsThreads;
    Strategy eStrategy = STRATEGY_RANDOM;
    int nXSize = 5000;
    int nYSize = 5000;
    int nBands = 4;
    char** papszOptions = NULL;
    int bOnDisk = FALSE;
    std::vector<GDALDataset*> apoDatasets;
    int bMemDriver = FALSE;
    GDALDataset* poMEMDS = NULL;

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
        else if( EQUAL(argv[i], "-dataset") && i + 1 < argc)
        {
            i ++;
            pszDataset = argv[i];
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
        }
        else if( EQUAL(argv[i], "-ysize") && i + 1 < argc)
        {
            i ++;
            nYSize = atoi(argv[i]);
        }
        else if( EQUAL(argv[i], "-bands") && i + 1 < argc)
        {
            i ++;
            nBands = atoi(argv[i]);
        }
        else if( EQUAL(argv[i], "-co") && i + 1 < argc)
        {
            i ++;
            papszOptions = CSLAddString(papszOptions, argv[i]);
        }
        else if( EQUAL(argv[i], "-ondisk"))
            bOnDisk = TRUE;
        else if( EQUAL(argv[i], "-check"))
            bCheck = TRUE;
        else if( EQUAL(argv[i], "-memdriver"))
            bMemDriver = TRUE;
        else
        {
            Usage();
        }
    }

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
    
    for(i = 0; i < nThreads; i++ )
    {
        // Since GDAL 2.0, the MEM driver is thread-safe, i.e. does not use the block
        // cache, but only for operations not involving resampling, which is
        // the case here
        if( poMEMDS ) 
            apoDatasets.push_back(poMEMDS);
        else
        {
            GDALDataset* poDS = (GDALDataset*)GDALOpen(pszDataset, GA_ReadOnly);
            if( poDS == NULL )
                exit(1);
            apoDatasets.push_back(poDS);
        }
    }

    if( bCreatedDataset && poMEMDS == NULL )
    {
        CPLPushErrorHandler(CPLQuietErrorHandler);
        VSIUnlink(pszDataset);
        CPLPopErrorHandler();
    }

    for(i = 0; i < nThreads; i++ )
    {
        void* pThread;
        void (*pfnThreadFunc)(void*) = NULL;
        if( eStrategy == STRATEGY_RANDOM )
            pfnThreadFunc = ThreadFuncRandomStrategy;
        else if( eStrategy == STRATEGY_LINE )
            pfnThreadFunc = ThreadFuncLineStrategy;
        else
            pfnThreadFunc = ThreadFuncBlockStrategy;
        pThread = CPLCreateJoinableThread(pfnThreadFunc, apoDatasets[i]);
        apsThreads.push_back(pThread);
    }
    for(i = 0; i < nThreads; i++ )
    {
        CPLJoinThread(apsThreads[i]);
        if( poMEMDS == NULL )
            GDALClose(apoDatasets[i]);
    }

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

