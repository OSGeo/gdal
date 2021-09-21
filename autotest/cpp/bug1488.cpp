/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Core
 * Purpose:  Test fix for https://github.com/OSGeo/gdal/issues/1488 (concurrency issue with overviews)
 * Author:   Even Rouault, <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2019, Even Rouault <even dot rouault at spatialys dot com>
 * Copyright (c) 2019, Thomas Bonfort <thomas.bonfort at gmail.com>
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
#include "cpl_multiproc.h"

#include <assert.h>

#include "test_data.h"

static GDALDriverH hTIFFDrv = nullptr;
static volatile int bThread1Finished = FALSE;
static volatile int bThread2Finished = FALSE;
static volatile int bContinue = TRUE;

const char* szSrcDataset = TUT_ROOT_DATA_DIR "/bug1488.tif";

static int CPL_STDCALL myProgress(double, const char *, void *);

static int CPL_STDCALL myProgress(double, const char *, void *)
{
    return bContinue;
}

static void CPL_STDCALL myErrorHandler(CPLErr, CPLErrorNum, const char* msg);

static void CPL_STDCALL myErrorHandler(CPLErr, CPLErrorNum errorNum, const char* msg)
{
    if( errorNum != CPLE_UserInterrupt && strstr(msg, "User terminated") == nullptr )
    {
        fprintf(stderr, "An error occurred: %s\n", msg);
        fprintf(stderr, "Likely a threading issue !\n");
        exit(1);
    }
}


static void worker_thread1(void *) {

    GDALDatasetH hDataset = GDALOpen("/vsimem/thread1.tif", GA_Update);
    assert(hDataset);

    int levels[1]={2};
    int bands[3]={1,2,3};
    CPLErr eErr = GDALBuildOverviews(hDataset,"AVERAGE",1,levels,3,bands,
                                     myProgress,nullptr);
    (void)eErr;
    GDALClose(hDataset);
    VSIUnlink("/vsimem/thread1.tif");
    bThread1Finished = TRUE;
}

static void worker_thread2(void *) {

    GDALDatasetH hSrc = GDALOpen(szSrcDataset, GA_ReadOnly);
    assert(hSrc);
    const char * const tops[] = {"TILED=YES","COMPRESS=WEBP",nullptr};
    GDALDatasetH hDataset = GDALCreateCopy(GDALGetDriverByName("GTiff"), 
                                       "/vsimem/thread2.tif",hSrc,TRUE,tops,
                                       myProgress,nullptr);
    GDALClose(hDataset);
    GDALClose(hSrc);
    VSIUnlink("/vsimem/thread2.tif");
    bThread2Finished = TRUE;
}

int main()
{
    GDALAllRegister();

    hTIFFDrv = GDALGetDriverByName("GTiff");
    if( !hTIFFDrv )
    {
        printf("GTIFF driver missing. Skipping\n");
        exit(0);
    }
    const char* pszCO = GDALGetMetadataItem(hTIFFDrv,
                                            GDAL_DMD_CREATIONOPTIONLIST, nullptr);
    if( pszCO == nullptr || strstr(pszCO, "WEBP") == nullptr )
    {
        printf("WEBP missing. Skipping\n");
        exit(0);
    }

    GDALSetCacheMax(30* 1000 * 1000);

    CPLSetErrorHandler(myErrorHandler);

    VSISync(szSrcDataset, "/vsimem/thread1.tif", nullptr, nullptr, nullptr, nullptr);

    CPLJoinableThread* t1 = CPLCreateJoinableThread(worker_thread1, nullptr);
    CPLJoinableThread* t2 = CPLCreateJoinableThread(worker_thread2, nullptr);
    int nCountSeconds = 0;
    while( !bThread1Finished && !bThread2Finished )
    {
        CPLSleep(1);
        nCountSeconds ++;
        if( nCountSeconds == 2 )
        {
            /* After 2 seconds without errors, assume no threading issue, and */
            /* early exit */
            bContinue = FALSE;
        }
    }
    CPLJoinThread(t1);
    CPLJoinThread(t2);

    return 0;
}
