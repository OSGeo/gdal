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
#include "cpl_string.h"
#include "cpl_multiproc.h"
#include "gdal_priv.h"
#include <assert.h>

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

        CPLErr IReadBlock(int, int, void*) { CPLAssert(FALSE); return CE_Failure; }
        CPLErr IWriteBlock(int nXBlock, int nYBlock, void*)
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
            FlushCache();
        }
};

void thread_func(void* unused)
{
    printf("begin thread\n");
    GDALFlushCacheBlock();
    printf("end of thread\n\n");
}

int main(int argc, char* argv[])
{
    CPLJoinableThread* hThread;
    
    printf("main thread %p\n", (void*)CPLGetPID());

    argc = GDALGeneralCmdLineProcessor( argc, &argv, 0 );

    CPLSetConfigOption("GDAL_CACHEMAX", "0");
    CPLSetConfigOption("GDAL_DEBUG_BLOCK_CACHE", "ON");
    
    MyDataset* poDS = new MyDataset();
    
    char buf1[] = { 1 } ;
    GDALRasterIO(GDALGetRasterBand(poDS, 1), GF_Write, 0, 0, 1, 1, buf1, 1, 1, GDT_Byte, 0, 0);
   
    hThread = CPLCreateJoinableThread(thread_func, NULL);
    CPLSleep(0.3);
    GDALRasterIO(GDALGetRasterBand(poDS, 1), GF_Write, 1, 0, 1, 1, buf1, 1, 1, GDT_Byte, 0, 0);
    GDALFlushCacheBlock();

    CPLJoinThread(hThread);

    delete poDS;
    GDALDestroyDriverManager();
    CSLDestroy( argv );
   
    return 0;
}
