/******************************************************************************
 *
 * Project:  GDAL Utilities
 * Purpose:  Multi-threading test application.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdal.h"
#include "gdal_alg.h"
#include "cpl_multiproc.h"
#include "cpl_string.h"

#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

static int nIterations = 1;
static bool bLockOnOpen = false;
static int nOpenIterations = 1;
static volatile int nPendingThreads = 0;
static bool bThreadCanFinish = false;
static std::mutex oMutex;
static std::condition_variable oCond;
static const char *pszFilename = nullptr;
static int nChecksum = 0;
static int nWidth = 0;
static int nHeight = 0;

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage()
{
    printf("multireadtest [[-thread_safe] | [[-lock_on_open] [-open_in_main]]\n"
           "              [-t <thread#>] [-i <iterations>] [-oi <iterations>]\n"
           "              [-width <val>] [-height <val>]\n"
           "              filename\n");
    exit(1);
}

/************************************************************************/
/*                             WorkerFunc()                             */
/************************************************************************/

static void WorkerFunc(void *arg)
{
    GDALDatasetH hDSIn = static_cast<GDALDatasetH>(arg);
    GDALDatasetH hDS = nullptr;

    for (int iOpenIter = 0; iOpenIter < nOpenIterations; iOpenIter++)
    {
        if (hDSIn != nullptr)
        {
            hDS = hDSIn;
        }
        else
        {
            if (bLockOnOpen)
                oMutex.lock();

            hDS = GDALOpen(pszFilename, GA_ReadOnly);

            if (bLockOnOpen)
                oMutex.unlock();
        }

        for (int iIter = 0; iIter < nIterations && hDS != nullptr; iIter++)
        {
            const int nMyChecksum =
                GDALChecksumImage(GDALGetRasterBand(hDS, 1), 0, 0,
                                  nWidth ? nWidth : GDALGetRasterXSize(hDS),
                                  nHeight ? nHeight : GDALGetRasterYSize(hDS));

            if (nMyChecksum != nChecksum)
            {
                printf("Checksum ERROR in worker thread!\n");
                break;
            }
        }

        if (hDS && hDSIn == nullptr)
        {
            if (bLockOnOpen)
                oMutex.lock();
            GDALClose(hDS);
            if (bLockOnOpen)
                oMutex.unlock();
        }
        else if (hDSIn != nullptr)
        {
            GDALFlushCache(hDSIn);
        }
    }

    {
        std::unique_lock oLock(oMutex);
        nPendingThreads--;
        oCond.notify_all();
        while (!bThreadCanFinish)
            oCond.wait(oLock);
    }
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

int main(int argc, char **argv)

{
    /* -------------------------------------------------------------------- */
    /*      Process arguments.                                              */
    /* -------------------------------------------------------------------- */
    argc = GDALGeneralCmdLineProcessor(argc, &argv, 0);
    if (argc < 1)
        exit(-argc);

    int nThreadCount = 4;
    bool bOpenInThreads = true;
    bool bThreadSafe = false;
    bool bJoinAfterClosing = false;
    bool bDetach = false;
    bool bClose = true;

    for (int iArg = 1; iArg < argc; iArg++)
    {
        if (iArg < argc - 1 && EQUAL(argv[iArg], "-i"))
        {
            nIterations = atoi(argv[++iArg]);
        }
        else if (iArg < argc - 1 && EQUAL(argv[iArg], "-oi"))
        {
            nOpenIterations = atoi(argv[++iArg]);
        }
        else if (iArg < argc - 1 && EQUAL(argv[iArg], "-t"))
        {
            nThreadCount = atoi(argv[++iArg]);
        }
        else if (iArg < argc - 1 && EQUAL(argv[iArg], "-width"))
        {
            nWidth = atoi(argv[++iArg]);
        }
        else if (iArg < argc - 1 && EQUAL(argv[iArg], "-height"))
        {
            nHeight = atoi(argv[++iArg]);
        }
        else if (EQUAL(argv[iArg], "-thread_safe"))
        {
            bThreadSafe = true;
        }
        else if (EQUAL(argv[iArg], "-lock_on_open"))
        {
            bLockOnOpen = true;
        }
        else if (EQUAL(argv[iArg], "-open_in_main"))
        {
            bOpenInThreads = false;
        }
        else if (EQUAL(argv[iArg], "-join_after_closing"))
        {
            bJoinAfterClosing = true;
        }
        else if (EQUAL(argv[iArg], "-detach"))
        {
            bDetach = true;
        }
        else if (EQUAL(argv[iArg], "-do_not_close"))
        {
            bClose = false;
        }
        else if (pszFilename == nullptr)
        {
            pszFilename = argv[iArg];
        }
        else
        {
            printf("Unrecognized argument: %s\n", argv[iArg]);
            Usage();
        }
    }

    if (pszFilename == nullptr)
    {
        printf("Need a file to operate on.\n");
        Usage();
        exit(1);
    }

    if (nOpenIterations > 0)
        bLockOnOpen = false;

    /* -------------------------------------------------------------------- */
    /*      Get the checksum of band1.                                      */
    /* -------------------------------------------------------------------- */
    GDALAllRegister();
    for (int i = 0; i < 2; i++)
    {
        GDALDatasetH hDS = GDALOpen(pszFilename, GA_ReadOnly);
        if (hDS == nullptr)
            exit(1);

        nChecksum =
            GDALChecksumImage(GDALGetRasterBand(hDS, 1), 0, 0,
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

    nPendingThreads = nThreadCount;

    GDALDatasetH hThreadSafeDS = nullptr;
    if (bThreadSafe)
    {
        hThreadSafeDS =
            GDALOpenEx(pszFilename, GDAL_OF_RASTER | GDAL_OF_THREAD_SAFE,
                       nullptr, nullptr, nullptr);
        if (!hThreadSafeDS)
            exit(1);
    }
    std::vector<std::thread> aoThreads;
    std::vector<GDALDatasetH> aoDS;
    for (int iThread = 0; iThread < nThreadCount; iThread++)
    {
        GDALDatasetH hDS = nullptr;
        if (bThreadSafe)
        {
            hDS = hThreadSafeDS;
        }
        else
        {
            if (!bOpenInThreads)
            {
                hDS = GDALOpen(pszFilename, GA_ReadOnly);
                if (!hDS)
                {
                    printf("GDALOpen() failed.\n");
                    exit(1);
                }
                aoDS.push_back(hDS);
            }
        }
        aoThreads.push_back(std::thread([hDS]() { WorkerFunc(hDS); }));
    }

    {
        std::unique_lock oLock(oMutex);
        while (nPendingThreads > 0)
        {
            // printf("nPendingThreads = %d\n", nPendingThreads);
            oCond.wait(oLock);
        }
    }

    if (!bJoinAfterClosing && !bDetach)
    {
        {
            std::lock_guard oLock(oMutex);
            bThreadCanFinish = true;
            oCond.notify_all();
        }
        for (auto &oThread : aoThreads)
            oThread.join();
    }

    for (size_t i = 0; i < aoDS.size(); ++i)
        GDALClose(aoDS[i]);
    if (bClose)
        GDALClose(hThreadSafeDS);

    if (bDetach)
    {
        for (auto &oThread : aoThreads)
            oThread.detach();
    }
    else if (bJoinAfterClosing)
    {
        {
            std::lock_guard oLock(oMutex);
            bThreadCanFinish = true;
            oCond.notify_all();
        }
        for (auto &oThread : aoThreads)
            oThread.join();
    }

    printf("All threads complete.\n");

    CSLDestroy(argv);

    GDALDestroyDriverManager();

    {
        std::lock_guard oLock(oMutex);
        bThreadCanFinish = true;
        oCond.notify_all();
    }

    printf("End of main.\n");

    return 0;
}
