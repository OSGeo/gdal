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

#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_multiproc.h"
#include "gdal.h"
#include <assert.h>

void thread_func(void* unused)
{
    printf("begin thread %p\n", (void*)CPLGetPID());
    CPLSetThreadLocalConfigOption("GDAL_RB_INTERNALIZE_SLEEP_AFTER_DROP_LOCK", "0.6");
    GDALDatasetH hDS = GDALOpen("../gcore/data/byte.tif", GA_ReadOnly);
    char buf[20*20];
    GDALRasterIO(GDALGetRasterBand(hDS, 1), GF_Read, 0, 0, 20, 20, buf, 20, 20, GDT_Byte, 0, 0);
    CPLSetThreadLocalConfigOption("GDAL_RB_INTERNALIZE_SLEEP_AFTER_DROP_LOCK", "0");
    GDALClose(hDS);
    printf("end of thread\n\n");
}


void thread_func2(void* unused)
{
    printf("begin thread %p\n", (void*)CPLGetPID());
    CPLSetThreadLocalConfigOption("GDAL_RB_FLUSHBLOCK_SLEEP_AFTER_DROP_LOCK", "0.6");
    GDALFlushCacheBlock();
    CPLSetThreadLocalConfigOption("GDAL_RB_FLUSHBLOCK_SLEEP_AFTER_DROP_LOCK", "0");
    printf("end of thread\n\n");
}


void thread_func3(void* unused)
{
    printf("begin thread %p\n", (void*)CPLGetPID());
    CPLSleep(0.3);
    printf("begin GDALFlushCacheBlock\n");
    GDALFlushCacheBlock();
    printf("end of thread\n\n");
}


void thread_func4(void* unused)
{
    printf("begin thread %p\n", (void*)CPLGetPID());
    CPLSetThreadLocalConfigOption("GDAL_RB_FLUSHBLOCK_SLEEP_AFTER_RB_LOCK", "0.6");
    GDALFlushCacheBlock();
    CPLSetThreadLocalConfigOption("GDAL_RB_FLUSHBLOCK_SLEEP_AFTER_RB_LOCK", "0");
    printf("end of thread\n\n");
}

int main(int argc, char* argv[])
{
    CPLJoinableThread* hThread;
    
    printf("main thread %p\n", (void*)CPLGetPID());

    argc = GDALGeneralCmdLineProcessor( argc, &argv, 0 );

    CPLSetConfigOption("GDAL_CACHEMAX", "0");
    CPLSetConfigOption("GDAL_DEBUG_BLOCK_CACHE", "ON");
    GDALAllRegister();

    GDALDatasetH hDS = GDALOpen("../gcore/data/byte.tif", GA_ReadOnly);
    
    char buf[20*20];
    printf("cache fill\n");
    GDALRasterIO(GDALGetRasterBand(hDS, 1), GF_Read, 0, 0, 20, 20, buf, 20, 20, GDT_Byte, 0, 0);
    printf("end of cache fill\n");
    printf("buf[0]=%d\n\n", (int)buf[0]);
   
    hThread = CPLCreateJoinableThread(thread_func, NULL);
    CPLSleep(0.3);
   
    printf("re read block\n");
    GDALRasterIO(GDALGetRasterBand(hDS, 1), GF_Read, 0, 0, 20, 20, buf, 20, 20, GDT_Byte, 0, 0);
    printf("end of re read block\n");
    printf("buf[0]=%d\n", (int)buf[0]);
    CPLJoinThread(hThread);
    
    

    hThread = CPLCreateJoinableThread(thread_func2, NULL);
    CPLSleep(0.3);
    
    printf("re read block\n");
    GDALRasterIO(GDALGetRasterBand(hDS, 1), GF_Read, 0, 0, 20, 20, buf, 20, 20, GDT_Byte, 0, 0);
    printf("end of re read block\n");
    printf("buf[0]=%d\n", (int)buf[0]);
    CPLJoinThread(hThread);

    
    
    hThread = CPLCreateJoinableThread(thread_func3, NULL);

    printf("re read block\n");
    CPLSetThreadLocalConfigOption("GDAL_RB_TRYGET_SLEEP_AFTER_TAKE_LOCK", "0.6");
    GDALRasterIO(GDALGetRasterBand(hDS, 1), GF_Read, 0, 0, 20, 20, buf, 20, 20, GDT_Byte, 0, 0);
    CPLSetThreadLocalConfigOption("GDAL_RB_TRYGET_SLEEP_AFTER_TAKE_LOCK", "0");
    printf("end of re read block\n");
    printf("buf[0]=%d\n", (int)buf[0]);
    CPLJoinThread(hThread);
    
    
    
    hThread = CPLCreateJoinableThread(thread_func2, NULL);
    CPLSleep(0.3);
    printf("before GDALFlushRasterCache\n");
    GDALFlushRasterCache(GDALGetRasterBand(hDS, 1));
    printf("after GDALFlushRasterCache\n");
    
    CPLJoinThread(hThread);
    assert( GDALGetCacheUsed64() == 0 );
    
    
    
    GDALRasterIO(GDALGetRasterBand(hDS, 1), GF_Read, 0, 0, 20, 20, buf, 20, 20, GDT_Byte, 0, 0);
    hThread = CPLCreateJoinableThread(thread_func2, NULL);
    CPLSleep(0.3);
    GDALClose(hDS);
    CPLJoinThread(hThread);
    
    
    hDS = GDALOpen("../gcore/data/byte.tif", GA_ReadOnly);
    GDALRasterIO(GDALGetRasterBand(hDS, 1), GF_Read, 0, 0, 20, 20, buf, 20, 20, GDT_Byte, 0, 0);
    hThread = CPLCreateJoinableThread(thread_func4, NULL);
    CPLSleep(0.3);
    GDALClose(hDS);
    CPLJoinThread(hThread);
    
    

    GDALDestroyDriverManager();
    CSLDestroy( argv );
   
    return 0;
}
