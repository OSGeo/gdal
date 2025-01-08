/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Core
 * Purpose:  Test performance of GDADLTranspose2D().
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdal.h"
#include "cpl_conv.h"

#include <cstdio>
#include <cstdlib>
#include <ctime>

#define SIZE (1024 * 1024 + 1) * 100

static void test(const void *pSrc, GDALDataType eSrcType, void *pDst,
                 GDALDataType eDstType, int W, int H, int reducFactor,
                 const char *extraMsg = "")
{
    CPLAssert(W * H <= SIZE);

    const int niters =
        static_cast<int>(4000U * 1000 * 1000 / reducFactor / W / H);
    const auto start = clock();
    for (int i = 0; i < niters; ++i)
        GDALTranspose2D(pSrc, eSrcType, pDst, eDstType, W, H);
    const auto end = clock();
    printf("W=%d, H=%d, reducFactor=%d%s: %0.2f sec\n", W, H, reducFactor,
           extraMsg, (end - start) * reducFactor * 1.0 / CLOCKS_PER_SEC);
}

int main(int /* argc */, char * /* argv */[])
{
    if (strstr(GDALVersionInfo("--version"), "debug build"))
    {
        printf("Skipping testperftranspose as this a debug build!\n");
        return 0;
    }

    void *src = CPLCalloc(1, SIZE);
    void *dst = CPLCalloc(1, SIZE);

    test(src, GDT_Byte, dst, GDT_Byte, 1024 * 1024 + 1, 2, 1);
    test(src, GDT_Byte, dst, GDT_Byte, 1024 * 1024 + 1, 3, 1);
#if defined(HAVE_SSSE3_AT_COMPILE_TIME) && defined(DEBUG)
    {
        CPLConfigOptionSetter oSetters("GDAL_USE_SSSE3", "NO", false);
        test(src, GDT_Byte, dst, GDT_Byte, 1024 * 1024 + 1, 3, 10,
             " (no SSSE3)");
    }
#endif
    test(src, GDT_Byte, dst, GDT_Byte, 1024 * 1024 + 1, 4, 1);
    test(src, GDT_Byte, dst, GDT_Byte, 1024 * 1024 + 1, 5, 1);
#if defined(HAVE_SSSE3_AT_COMPILE_TIME) && defined(DEBUG)
    {
        CPLConfigOptionSetter oSetters("GDAL_USE_SSSE3", "NO", false);
        test(src, GDT_Byte, dst, GDT_Byte, 1024 * 1024 + 1, 5, 10,
             " (no SSSE3)");
    }
#endif
    test(src, GDT_Byte, dst, GDT_Byte, 1024 * 1024 + 1, 16 + 1, 10);
#if defined(HAVE_SSSE3_AT_COMPILE_TIME) && defined(DEBUG)
    {
        CPLConfigOptionSetter oSetters("GDAL_USE_SSSE3", "NO", false);
        test(src, GDT_Byte, dst, GDT_Byte, 1024 * 1024 + 1, 16 + 1, 10,
             " (no SSSE3)");
    }
#endif
    test(src, GDT_Byte, dst, GDT_Byte, 1024 * 1024 + 1, 100, 10);
    test(src, GDT_Byte, dst, GDT_Byte, 70 * 1024 + 1, 1024 + 1, 10);
#if defined(HAVE_SSSE3_AT_COMPILE_TIME) && defined(DEBUG)
    {
        CPLConfigOptionSetter oSetters("GDAL_USE_SSSE3", "NO", false);
        test(src, GDT_Byte, dst, GDT_Byte, 70 * 1024 + 1, 1024 + 1, 10,
             " (no SSSE3)");
    }
#endif
    test(src, GDT_Byte, dst, GDT_Byte, 7 * 1024 + 1, 10 * 1024 + 1, 10);
#if defined(HAVE_SSSE3_AT_COMPILE_TIME) && defined(DEBUG)
    {
        CPLConfigOptionSetter oSetters("GDAL_USE_SSSE3", "NO", false);
        test(src, GDT_Byte, dst, GDT_Byte, 7 * 1024 + 1, 10 * 1024 + 1, 10,
             " (no SSSE3)");
    }
#endif

    VSIFree(src);
    VSIFree(dst);
    return 0;
}
