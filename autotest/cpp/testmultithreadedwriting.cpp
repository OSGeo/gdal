/******************************************************************************
 * Project:  GDAL Core
 * Purpose:  Test block cache & writing behaviour under multi-threading
 * Author:   Even Rouault, <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2016, Even Rouault <even dot rouault at spatialys dot com>
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
#include "gdal_alg.h"
#include "gdal_priv.h"

#include "gtest_include.h"

namespace
{

// ---------------------------------------------------------------------------

static void thread_func(void *ptr)
{
    int num = *(int *)ptr;
    GDALDriver *poDriver = (GDALDriver *)GDALGetDriverByName("ENVI");
    GDALDataset *poDSRef =
        (GDALDataset *)GDALOpen("/vsimem/test_ref", GA_ReadOnly);
    GDALDataset *poDS = poDriver->Create(CPLSPrintf("/vsimem/test%d", num), 100,
                                         2000, 1, GDT_Byte, nullptr);
    GDALRasterBand *poBand = poDS->GetRasterBand(1);
    GDALRasterBand *poBandRef = poDSRef->GetRasterBand(1);
    for (int i = 0; i < 2000; i++)
    {
        GDALRasterBlock *poBlockRef = poBandRef->GetLockedBlockRef(0, i);
        GDALRasterBlock *poBlockRW = poBand->GetLockedBlockRef(0, i);
        poBlockRW->MarkDirty();
        memset(poBlockRW->GetDataRef(), 0xFF, 100);
        poBlockRef->DropLock();
        poBlockRW->DropLock();
    }
    GDALClose(poDS);
    GDALClose(poDSRef);
}

TEST(testmultithreadedwriting, test)
{
    bool bEndlessLoop = CPLTestBool(CPLGetConfigOption("ENDLESS_LOOPS", "NO"));

    CPLJoinableThread *hThread1;
    CPLJoinableThread *hThread2;

    GDALAllRegister();
    GDALSetCacheMax(10000);

    int one = 1;
    int two = 2;
    GDALDriver *poDriver = (GDALDriver *)GDALGetDriverByName("ENVI");
    if (poDriver == nullptr)
    {
        GTEST_SKIP() << "ENVI driver missing";
        return;
    }
    GDALDataset *poDS =
        poDriver->Create("/vsimem/test_ref", 100, 2000, 1, GDT_Byte, nullptr);
    GDALClose(poDS);

    int counter = 0;
    const int nloops = bEndlessLoop ? 2 * 1000 * 1000 * 1000 : 1;
    for (int i = 0; i < nloops; ++i)
    {
        ++i;
        if ((i % 20) == 0)
            printf("%d\n", counter);

        hThread1 = CPLCreateJoinableThread(thread_func, &one);
        hThread2 = CPLCreateJoinableThread(thread_func, &two);

        CPLJoinThread(hThread1);
        CPLJoinThread(hThread2);

        GDALDataset *poDSRef =
            (GDALDataset *)GDALOpen("/vsimem/test1", GA_ReadOnly);
        const int cs =
            GDALChecksumImage(poDSRef->GetRasterBand(1), 0, 0, 100, 2000);
        EXPECT_EQ(cs, 29689);
        GDALClose(poDSRef);

        poDriver->Delete("/vsimem/test1");
        poDriver->Delete("/vsimem/test2");
    }

    poDriver->Delete("/vsimem/test_ref");

    GDALDestroyDriverManager();
}

}  // namespace
