/******************************************************************************
 * Project:  GDAL Core
 * Purpose:  Test performance of GDALCopyWords().
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2009-2010, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdal.h"
#include "cpl_conv.h"

#include <cstdio>
#include <cstdlib>
#include <ctime>

static void bench(void *in, void *out, int intype, int outtype)
{
    clock_t start = clock();

    for (int i = 0; i < 1000; i++)
        GDALCopyWords(in, (GDALDataType)intype, 16, out, (GDALDataType)outtype,
                      16, 256 * 256);

    clock_t end = clock();

    printf("%s -> %s : %.2f s\n", GDALGetDataTypeName((GDALDataType)intype),
           GDALGetDataTypeName((GDALDataType)outtype),
           (end - start) * 1.0 / CLOCKS_PER_SEC);

    start = clock();

    for (int i = 0; i < 1000; i++)
        GDALCopyWords(in, (GDALDataType)intype,
                      GDALGetDataTypeSizeBytes((GDALDataType)intype), out,
                      (GDALDataType)outtype,
                      GDALGetDataTypeSizeBytes((GDALDataType)outtype),
                      256 * 256);

    end = clock();

    printf("%s -> %s (packed) : %.2f s\n",
           GDALGetDataTypeName((GDALDataType)intype),
           GDALGetDataTypeName((GDALDataType)outtype),
           (end - start) * 1.0 / CLOCKS_PER_SEC);
}

int main(int /* argc */, char * /* argv */[])
{
    void *in = calloc(1, 256 * 256 * 16);
    void *out = malloc(256 * 256 * 16);

    for (int intype = GDT_Byte; intype < GDT_TypeCount; intype++)
    {
        for (int outtype = GDT_Byte; outtype < GDT_TypeCount; outtype++)
        {
            bench(in, out, intype, outtype);
        }
    }

    for (int k = 0; k < 2; k++)
    {
        if (k == 1)
        {
            printf("Disabling SSSE3\n");
            CPLSetConfigOption("GDAL_USE_SSSE3", "NO");
        }

        // 2 byte stride --> packed byte
        clock_t start = clock();
        for (int i = 0; i < 100000; i++)
            GDALCopyWords(in, GDT_Byte, 2, out, GDT_Byte, 1, 256 * 256);
        clock_t end = clock();
        printf("2-byte stride Byte ->packed Byte : %.2f\n",
               (end - start) * 1.0 / CLOCKS_PER_SEC);

        // 3 byte stride --> packed byte
        start = clock();
        for (int i = 0; i < 100000; i++)
            GDALCopyWords(in, GDT_Byte, 3, out, GDT_Byte, 1, 256 * 256);
        end = clock();
        printf("3-byte stride Byte ->packed Byte : %.2f\n",
               (end - start) * 1.0 / CLOCKS_PER_SEC);

        // 4 byte stride --> packed byte
        start = clock();
        for (int i = 0; i < 100000; i++)
            GDALCopyWords(in, GDT_Byte, 4, out, GDT_Byte, 1, 256 * 256);
        end = clock();
        printf("4-byte stride Byte ->packed Byte : %.2f\n",
               (end - start) * 1.0 / CLOCKS_PER_SEC);
    }
    CPLSetConfigOption("GDAL_USE_SSSE3", nullptr);

    return 0;
}
