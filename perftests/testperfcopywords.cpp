/******************************************************************************
 * $Id$
 *
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

int main(int /* argc */, char * /* argv */[])
{
    void *in = calloc(1, 256 * 256 * 16);
    void *out = malloc(256 * 256 * 16);

    int i;
    int intype, outtype;

    clock_t start, end;

    for (intype = GDT_Byte; intype < GDT_TypeCount; intype++)
    {
        for (outtype = GDT_Byte; outtype < GDT_TypeCount; outtype++)
        {
            start = clock();

            for (i = 0; i < 1000; i++)
                GDALCopyWords(in, (GDALDataType)intype, 16, out,
                              (GDALDataType)outtype, 16, 256 * 256);

            end = clock();

            printf("%s -> %s : %.2f s\n",
                   GDALGetDataTypeName((GDALDataType)intype),
                   GDALGetDataTypeName((GDALDataType)outtype),
                   (end - start) * 1.0 / CLOCKS_PER_SEC);

            start = clock();

            for (i = 0; i < 1000; i++)
                GDALCopyWords(in, (GDALDataType)intype,
                              GDALGetDataTypeSize((GDALDataType)intype) / 8,
                              out, (GDALDataType)outtype,
                              GDALGetDataTypeSize((GDALDataType)outtype) / 8,
                              256 * 256);

            end = clock();

            printf("%s -> %s (packed) : %.2f s\n",
                   GDALGetDataTypeName((GDALDataType)intype),
                   GDALGetDataTypeName((GDALDataType)outtype),
                   (end - start) * 1.0 / CLOCKS_PER_SEC);
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
        start = clock();
        for (i = 0; i < 100000; i++)
            GDALCopyWords(in, GDT_Byte, 2, out, GDT_Byte, 1, 256 * 256);
        end = clock();
        printf("2-byte stride Byte ->packed Byte : %.2f\n",
               (end - start) * 1.0 / CLOCKS_PER_SEC);

        // 3 byte stride --> packed byte
        start = clock();
        for (i = 0; i < 100000; i++)
            GDALCopyWords(in, GDT_Byte, 3, out, GDT_Byte, 1, 256 * 256);
        end = clock();
        printf("3-byte stride Byte ->packed Byte : %.2f\n",
               (end - start) * 1.0 / CLOCKS_PER_SEC);

        // 4 byte stride --> packed byte
        start = clock();
        for (i = 0; i < 100000; i++)
            GDALCopyWords(in, GDT_Byte, 4, out, GDT_Byte, 1, 256 * 256);
        end = clock();
        printf("4-byte stride Byte ->packed Byte : %.2f\n",
               (end - start) * 1.0 / CLOCKS_PER_SEC);
    }
    CPLSetConfigOption("GDAL_USE_SSSE3", nullptr);

    return 0;
}
