/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Core
 * Purpose:  Test block cache & writing behaviour under multi-threading
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

#include "cpl_conv.h"
#include "cpl_multiproc.h"
#include "cpl_string.h"
#include "gdal_priv.h"
#include "test_data.h"

#include <cassert>

class MyRasterBand: public GDALRasterBand
{
        int bBusy;

    public:
        MyRasterBand()
        {
            nBlockXSize = 1;
            nBlockYSize = 1;
            bBusy = FALSE;
        }

        CPLErr IReadBlock(int, int, void*) CPL_OVERRIDE { CPLAssert(FALSE); return CE_Failure; }
        CPLErr IWriteBlock(int nXBlock, int nYBlock, void*) CPL_OVERRIDE
        {
            printf("Entering IWriteBlock(%d, %d)\n", nXBlock, nYBlock);
            assert(!bBusy);
            bBusy = TRUE;
            CPLSleep(0.5);
            bBusy = FALSE;
            printf("Leaving IWriteBlock(%d, %d)\n", nXBlock, nYBlock);
            return CE_None;
        }
};

class MyDataset: public GDALDataset
{
    public:
        MyDataset()
        {
            eAccess = GA_Update;
            nRasterXSize = 2;
            nRasterYSize = 1;
            SetBand(1, new MyRasterBand());
        }

        ~MyDataset()
        {
            FlushCache(true);
        }
};

static void thread_func1(void* /* unused */ )
{
    printf("begin thread\n");
    GDALFlushCacheBlock();
    printf("end of thread\n\n");
}

static void test1()
{
    CPLJoinableThread* hThread;

    printf("Start test1\n");
    printf("main thread %p\n", (void*)CPLGetPID());

    GDALSetCacheMax(0);

    MyDataset* poDS = new MyDataset();

    char buf1[] = { 1 } ;
    CPL_IGNORE_RET_VAL(GDALRasterIO(GDALGetRasterBand(poDS, 1), GF_Write, 0, 0, 1, 1, buf1, 1, 1, GDT_Byte, 0, 0));

    hThread = CPLCreateJoinableThread(thread_func1, nullptr);
    CPLSleep(0.3);
    CPL_IGNORE_RET_VAL(GDALRasterIO(GDALGetRasterBand(poDS, 1), GF_Write, 1, 0, 1, 1, buf1, 1, 1, GDT_Byte, 0, 0));
    GDALFlushCacheBlock();

    CPLJoinThread(hThread);

    delete poDS;
    printf("End test1\n");
}


static void thread_func2(void* /* unused */)
{
    printf("begin thread %p\n", (void*)CPLGetPID());
    GDALDatasetH hDS = GDALOpen(TUT_ROOT_DATA_DIR "/byte.tif", GA_ReadOnly);
    GByte c = 0;
    CPL_IGNORE_RET_VAL(GDALDataset::FromHandle(hDS)->GetRasterBand(1)->RasterIO(GF_Read, 0,0,1,1,&c,1,1,GDT_Byte,0,0,nullptr));
    GDALClose(hDS);
    printf("end of thread\n\n");
}

static void test2()
{
    printf("Start test2\n");
    printf("main thread %p\n", (void*)CPLGetPID());

    CPLJoinableThread* hThread;

    CPLSetConfigOption("GDAL_RB_INTERNALIZE_SLEEP_AFTER_DETACH_BEFORE_WRITE", "0.5");
    GDALSetCacheMax(1000*1000);

    auto poDS = GetGDALDriverManager()->GetDriverByName("GTiff")->Create("/vsimem/foo.tif", 1, 1, 2, GDT_Byte, nullptr);
    poDS->GetRasterBand(1)->Fill(0);
    poDS->GetRasterBand(2)->Fill(0);
    poDS->FlushCache(false);
    GDALSetCacheMax(0);

    poDS->GetRasterBand(1)->Fill(1);
    hThread = CPLCreateJoinableThread(thread_func2, nullptr);
    CPLSleep(0.2);

    GByte c = 0;
    CPL_IGNORE_RET_VAL(poDS->GetRasterBand(1)->RasterIO(GF_Read,0,0,1,1,&c,1,1,GDT_Byte,0,0,nullptr));
    printf("%d\n", c);
    assert(c == 1);
    CPLJoinThread(hThread);

    CPLSetConfigOption("GDAL_RB_INTERNALIZE_SLEEP_AFTER_DETACH_BEFORE_WRITE", nullptr);
    delete poDS;
    VSIUnlink("/vsimem/foo.tif");
    printf("End test2\n");
}


int main(int argc, char* argv[])
{
    argc = GDALGeneralCmdLineProcessor( argc, &argv, 0 );

    CPLSetConfigOption("GDAL_DEBUG_BLOCK_CACHE", "ON");
    GDALGetCacheMax();

    GDALAllRegister();

    test1();
    test2();

    GDALDestroyDriverManager();
    CSLDestroy( argv );

    return 0;
}
