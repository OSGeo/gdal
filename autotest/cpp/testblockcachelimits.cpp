/******************************************************************************
 *
 * Project:  GDAL Core
 * Purpose:  Test block cache under multi-threading
 * Author:   Even Rouault, <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2015, Even Rouault <even dot rouault at spatialys dot com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_conv.h"
#include "cpl_multiproc.h"
#include "cpl_string.h"
#include "gdal_priv.h"
#include "gdal.h"

#include "test_data.h"

#include "gtest_include.h"

namespace
{

// ---------------------------------------------------------------------------

static void thread_func(void * /* unused */)
{
    printf("begin thread %p\n", (void *)CPLGetPID());
    CPLSetThreadLocalConfigOption("GDAL_RB_INTERNALIZE_SLEEP_AFTER_DROP_LOCK",
                                  "0.6");
    GDALDatasetH hDS = GDALOpen(GCORE_DATA_DIR "byte.tif", GA_ReadOnly);
    char buf[20 * 20];
    CPL_IGNORE_RET_VAL(GDALRasterIO(GDALGetRasterBand(hDS, 1), GF_Read, 0, 0,
                                    20, 20, buf, 20, 20, GDT_Byte, 0, 0));
    CPLSetThreadLocalConfigOption("GDAL_RB_INTERNALIZE_SLEEP_AFTER_DROP_LOCK",
                                  "0");
    GDALClose(hDS);
    printf("end of thread\n\n");
}

static void thread_func2(void * /* unused */)
{
    printf("begin thread %p\n", (void *)CPLGetPID());
    CPLSetThreadLocalConfigOption("GDAL_RB_FLUSHBLOCK_SLEEP_AFTER_DROP_LOCK",
                                  "0.6");
    GDALFlushCacheBlock();
    CPLSetThreadLocalConfigOption("GDAL_RB_FLUSHBLOCK_SLEEP_AFTER_DROP_LOCK",
                                  "0");
    printf("end of thread\n\n");
}

static void thread_func3(void * /* unused */)
{
    printf("begin thread %p\n", (void *)CPLGetPID());
    CPLSleep(0.3);
    printf("begin GDALFlushCacheBlock\n");
    GDALFlushCacheBlock();
    printf("end of thread\n\n");
}

static void thread_func4(void * /* unused */)
{
    printf("begin thread %p\n", (void *)CPLGetPID());
    CPLSetThreadLocalConfigOption("GDAL_RB_FLUSHBLOCK_SLEEP_AFTER_RB_LOCK",
                                  "0.6");
    GDALFlushCacheBlock();
    CPLSetThreadLocalConfigOption("GDAL_RB_FLUSHBLOCK_SLEEP_AFTER_RB_LOCK",
                                  "0");
    printf("end of thread\n\n");
}

// ---------------------------------------------------------------------------

TEST(testblockcachelimits, test)
{
    CPLJoinableThread *hThread;

    // CPLSetConfigOption("CPL_DEBUG", "ON");

    printf("main thread %p\n", (void *)CPLGetPID());

    CPLSetConfigOption("GDAL_CACHEMAX", "0");
    CPLSetConfigOption("GDAL_DEBUG_BLOCK_CACHE", "ON");
    GDALAllRegister();

    GDALDatasetH hDS = GDALOpen(GCORE_DATA_DIR "byte.tif", GA_ReadOnly);

    char buf[20 * 20];
    printf("cache fill\n");
    CPL_IGNORE_RET_VAL(GDALRasterIO(GDALGetRasterBand(hDS, 1), GF_Read, 0, 0,
                                    20, 20, buf, 20, 20, GDT_Byte, 0, 0));
    printf("end of cache fill\n");
    printf("buf[0]=%d\n\n", (int)buf[0]);

    hThread = CPLCreateJoinableThread(thread_func, nullptr);
    CPLSleep(0.3);

    printf("re read block\n");
    CPL_IGNORE_RET_VAL(GDALRasterIO(GDALGetRasterBand(hDS, 1), GF_Read, 0, 0,
                                    20, 20, buf, 20, 20, GDT_Byte, 0, 0));
    printf("end of re read block\n");
    printf("buf[0]=%d\n", (int)buf[0]);
    CPLJoinThread(hThread);

    hThread = CPLCreateJoinableThread(thread_func2, nullptr);
    CPLSleep(0.3);

    printf("re read block\n");
    CPL_IGNORE_RET_VAL(GDALRasterIO(GDALGetRasterBand(hDS, 1), GF_Read, 0, 0,
                                    20, 20, buf, 20, 20, GDT_Byte, 0, 0));
    printf("end of re read block\n");
    printf("buf[0]=%d\n", (int)buf[0]);
    CPLJoinThread(hThread);

    hThread = CPLCreateJoinableThread(thread_func3, nullptr);

    printf("re read block\n");
    CPLSetThreadLocalConfigOption("GDAL_RB_TRYGET_SLEEP_AFTER_TAKE_LOCK",
                                  "0.6");
    CPL_IGNORE_RET_VAL(GDALRasterIO(GDALGetRasterBand(hDS, 1), GF_Read, 0, 0,
                                    20, 20, buf, 20, 20, GDT_Byte, 0, 0));
    CPLSetThreadLocalConfigOption("GDAL_RB_TRYGET_SLEEP_AFTER_TAKE_LOCK", "0");
    printf("end of re read block\n");
    printf("buf[0]=%d\n", (int)buf[0]);
    CPLJoinThread(hThread);

    hThread = CPLCreateJoinableThread(thread_func2, nullptr);
    CPLSleep(0.3);
    printf("before GDALFlushRasterCache\n");
    GDALFlushRasterCache(GDALGetRasterBand(hDS, 1));
    printf("after GDALFlushRasterCache\n");

    CPLJoinThread(hThread);
    ASSERT_EQ(GDALGetCacheUsed64(), 0);

    CPL_IGNORE_RET_VAL(GDALRasterIO(GDALGetRasterBand(hDS, 1), GF_Read, 0, 0,
                                    20, 20, buf, 20, 20, GDT_Byte, 0, 0));
    hThread = CPLCreateJoinableThread(thread_func2, nullptr);
    CPLSleep(0.3);
    GDALClose(hDS);
    CPLJoinThread(hThread);

    hDS = GDALOpen(GCORE_DATA_DIR "byte.tif", GA_ReadOnly);
    CPL_IGNORE_RET_VAL(GDALRasterIO(GDALGetRasterBand(hDS, 1), GF_Read, 0, 0,
                                    20, 20, buf, 20, 20, GDT_Byte, 0, 0));
    hThread = CPLCreateJoinableThread(thread_func4, nullptr);
    CPLSleep(0.3);
    GDALClose(hDS);
    CPLJoinThread(hThread);

    GDALDestroyDriverManager();
}

}  // namespace
