/******************************************************************************
 *
 * Project:  GDAL Core
 * Purpose:  Test fix for https://github.com/OSGeo/gdal/issues/1488 (concurrency
 *issue with overviews) Author:   Even Rouault, <even dot rouault at spatialys
 *dot com>
 *
 ******************************************************************************
 * Copyright (c) 2019, Even Rouault <even dot rouault at spatialys dot com>
 * Copyright (c) 2019, Thomas Bonfort <thomas.bonfort at gmail.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdal.h"
#include "cpl_multiproc.h"

#include "test_data.h"

#include "gtest_include.h"

namespace
{

// ---------------------------------------------------------------------------

static GDALDriverH hTIFFDrv = nullptr;
static volatile int bThread1Finished = FALSE;
static volatile int bThread2Finished = FALSE;
static volatile int bContinue = TRUE;

const char *szSrcDataset = TUT_ROOT_DATA_DIR "/bug1488.tif";

static int CPL_STDCALL myProgress(double, const char *, void *);

static int CPL_STDCALL myProgress(double, const char *, void *)
{
    return bContinue;
}

static void CPL_STDCALL myErrorHandler(CPLErr, CPLErrorNum, const char *msg);

static void CPL_STDCALL myErrorHandler(CPLErr, CPLErrorNum errorNum,
                                       const char *msg)
{
    if (errorNum != CPLE_UserInterrupt &&
        strstr(msg, "User terminated") == nullptr)
    {
        fprintf(stderr, "An error occurred: %s\n", msg);
        fprintf(stderr, "Likely a threading issue !\n");
        ASSERT_TRUE(false);
    }
}

static void worker_thread1(void *)
{

    GDALDatasetH hDataset = GDALOpen("/vsimem/thread1.tif", GA_Update);
    ASSERT_TRUE(hDataset != nullptr);

    int levels[1] = {2};
    int bands[3] = {1, 2, 3};
    CPLErr eErr = GDALBuildOverviews(hDataset, "AVERAGE", 1, levels, 3, bands,
                                     myProgress, nullptr);
    (void)eErr;
    GDALClose(hDataset);
    VSIUnlink("/vsimem/thread1.tif");
    bThread1Finished = TRUE;
}

static void worker_thread2(void *)
{

    GDALDatasetH hSrc = GDALOpen(szSrcDataset, GA_ReadOnly);
    ASSERT_TRUE(hSrc != nullptr);
    const char *const tops[] = {"TILED=YES", "COMPRESS=WEBP", nullptr};
    GDALDatasetH hDataset =
        GDALCreateCopy(GDALGetDriverByName("GTiff"), "/vsimem/thread2.tif",
                       hSrc, TRUE, tops, myProgress, nullptr);
    GDALClose(hDataset);
    GDALClose(hSrc);
    VSIUnlink("/vsimem/thread2.tif");
    bThread2Finished = TRUE;
}

TEST(bug1488, test)
{
    GDALAllRegister();

    hTIFFDrv = GDALGetDriverByName("GTiff");
    if (!hTIFFDrv)
    {
        GTEST_SKIP() << "GTIFF driver missing";
        return;
    }
    const char *pszCO =
        GDALGetMetadataItem(hTIFFDrv, GDAL_DMD_CREATIONOPTIONLIST, nullptr);
    if (pszCO == nullptr || strstr(pszCO, "WEBP") == nullptr)
    {
        GTEST_SKIP() << "WEBP driver missing";
        return;
    }

    GDALSetCacheMax(30 * 1000 * 1000);

    CPLSetErrorHandler(myErrorHandler);

    VSISync(szSrcDataset, "/vsimem/thread1.tif", nullptr, nullptr, nullptr,
            nullptr);

    CPLJoinableThread *t1 = CPLCreateJoinableThread(worker_thread1, nullptr);
    CPLJoinableThread *t2 = CPLCreateJoinableThread(worker_thread2, nullptr);
    int nCountSeconds = 0;
    while (!bThread1Finished && !bThread2Finished)
    {
        CPLSleep(1);
        nCountSeconds++;
        if (nCountSeconds == 2)
        {
            /* After 2 seconds without errors, assume no threading issue, and */
            /* early exit */
            bContinue = FALSE;
        }
    }
    CPLJoinThread(t1);
    CPLJoinThread(t2);
}

}  // namespace
