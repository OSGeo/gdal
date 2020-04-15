/******************************************************************************
 *
 * Project:  GDAL Utilities
 * Purpose:  Multi-threading test application.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam
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

#include "gdal.h"
#include "gdal_alg.h"
#include "cpl_multiproc.h"
#include "cpl_string.h"
#include <vector>

CPL_CVSID("$Id$")

static int nIterations = 1;
static bool bLockOnOpen = false;
static int nOpenIterations = 1;
static volatile int nPendingThreads = 0;
static const char *pszFilename = nullptr;
static int nChecksum = 0;
static int nWidth = 0;
static int nHeight = 0;

static CPLMutex *pGlobalMutex = nullptr;

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage()
{
    printf("multireadtest [-lock_on_open] [-open_in_main] [-t <thread#>]\n"
           "              [-i <iterations>] [-oi <iterations>]\n"
           "              [-width <val>] [-height <val>]\n"
           "              filename\n");
    exit(1);
}

/************************************************************************/
/*                             WorkerFunc()                             */
/************************************************************************/

static void WorkerFunc( void *arg )
{
    GDALDatasetH hDSIn = static_cast<GDALDatasetH>(arg);
    GDALDatasetH hDS = nullptr;

    for( int iOpenIter = 0; iOpenIter < nOpenIterations; iOpenIter++ )
    {
        if( hDSIn != nullptr )
        {
            hDS = hDSIn;
        }
        else
        {
            if( bLockOnOpen )
                CPLAcquireMutex(pGlobalMutex, 100.0);

            hDS = GDALOpen(pszFilename, GA_ReadOnly);

            if( bLockOnOpen )
                CPLReleaseMutex(pGlobalMutex);
        }

        for( int iIter = 0; iIter < nIterations && hDS != nullptr; iIter++ )
        {
            const int nMyChecksum = GDALChecksumImage(GDALGetRasterBand(hDS, 1),
                                                      0, 0,
                                                      nWidth ? nWidth : GDALGetRasterXSize(hDS),
                                                      nHeight ? nHeight : GDALGetRasterYSize(hDS));

            if( nMyChecksum != nChecksum )
            {
                printf("Checksum ERROR in worker thread!\n");
                break;
            }
        }

        if( hDS && hDSIn == nullptr )
        {
            if( bLockOnOpen )
                CPLAcquireMutex(pGlobalMutex, 100.0);
            GDALClose( hDS );
            if( bLockOnOpen )
                CPLReleaseMutex(pGlobalMutex);
        }
        else if ( hDSIn != nullptr )
        {
            GDALFlushCache(hDSIn);
        }
    }

    CPLAcquireMutex(pGlobalMutex, 100.0);
    nPendingThreads--;
    CPLReleaseMutex(pGlobalMutex);
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

int main( int argc, char ** argv )

{
/* -------------------------------------------------------------------- */
/*      Process arguments.                                              */
/* -------------------------------------------------------------------- */
    argc = GDALGeneralCmdLineProcessor(argc, &argv, 0);
    if( argc < 1 )
        exit(-argc);

    int nThreadCount = 4;
    bool bOpenInThreads = true;

    for( int iArg = 1; iArg < argc; iArg++ )
    {
        if( iArg < argc-1 && EQUAL(argv[iArg], "-i") )
        {
            nIterations = atoi(argv[++iArg]);
        }
        else if( iArg < argc-1 && EQUAL(argv[iArg], "-oi") )
        {
            nOpenIterations = atoi(argv[++iArg]);
        }
        else if( iArg < argc-1 && EQUAL(argv[iArg], "-t") )
        {
            nThreadCount = atoi(argv[++iArg]);
        }
        else if( iArg < argc-1 && EQUAL(argv[iArg], "-width") )
        {
            nWidth = atoi(argv[++iArg]);
        }
        else if( iArg < argc-1 && EQUAL(argv[iArg], "-height") )
        {
            nHeight = atoi(argv[++iArg]);
        }
        else if( EQUAL(argv[iArg], "-lock_on_open") )
        {
            bLockOnOpen = true;
        }
        else if( EQUAL(argv[iArg], "-open_in_main") )
        {
            bOpenInThreads = false;
        }
        else if( pszFilename == nullptr )
        {
            pszFilename = argv[iArg];
        }
        else
        {
            printf("Unrecognized argument: %s\n", argv[iArg]);
            Usage();
        }
    }

    if( pszFilename == nullptr )
    {
        printf("Need a file to operate on.\n");
        Usage();
        exit(1);
    }

    if( nOpenIterations > 0 )
        bLockOnOpen = false;

/* -------------------------------------------------------------------- */
/*      Get the checksum of band1.                                      */
/* -------------------------------------------------------------------- */
    GDALDatasetH hDS = nullptr;

    GDALAllRegister();
    for( int i = 0; i < 2; i++ )
    {
        hDS = GDALOpen( pszFilename, GA_ReadOnly );
        if( hDS == nullptr )
            exit( 1 );

        nChecksum = GDALChecksumImage(GDALGetRasterBand(hDS, 1),
                                      0, 0,
                                      nWidth ? nWidth : GDALGetRasterXSize(hDS),
                                      nHeight ? nHeight : GDALGetRasterYSize(hDS));

        GDALClose(hDS);
    }

    printf(
        "Got checksum %d, launching %d worker threads on %s, %d iterations.\n",
        nChecksum, nThreadCount, pszFilename, nIterations);

/* -------------------------------------------------------------------- */
/*      Fire off worker threads.                                        */
/* -------------------------------------------------------------------- */
    pGlobalMutex = CPLCreateMutex();
    CPLReleaseMutex(pGlobalMutex);

    nPendingThreads = nThreadCount;

    std::vector<GDALDatasetH> aoDS;
    for( int iThread = 0; iThread < nThreadCount; iThread++ )
    {
        hDS = nullptr;
        if( !bOpenInThreads )
        {
            hDS = GDALOpen(pszFilename, GA_ReadOnly);
            if( !hDS )
            {
                printf("GDALOpen() failed.\n");
                exit(1);
            }
            aoDS.push_back(hDS);
        }
        if( CPLCreateThread(WorkerFunc, hDS) == -1 )
        {
            printf("CPLCreateThread() failed.\n");
            exit(1);
        }
    }

    while( nPendingThreads > 0 )
        CPLSleep(0.5);

    CPLDestroyMutex(pGlobalMutex);

    for( size_t i = 0; i < aoDS.size(); ++i )
        GDALClose(aoDS[i]);

    printf("All threads complete.\n");

    CSLDestroy(argv);

    GDALDestroyDriverManager();

    return 0;
}
