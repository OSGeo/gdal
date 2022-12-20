/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Core
 * Purpose:  Test performance of GDALDeinterleave().
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2022, Even Rouault <even dot rouault at spatialys.com>
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
#include "cpl_conv.h"

#include <cstdio>
#include <cstdlib>
#include <ctime>

int main(int /* argc */, char * /* argv */[])
{
    constexpr int SIZE = 1024;
    void *src = calloc(SIZE * SIZE, 4);
    void *dst0 = malloc(SIZE * SIZE);
    void *dst1 = malloc(SIZE * SIZE);
    void *dst2 = malloc(SIZE * SIZE);
    void *dst3 = malloc(SIZE * SIZE);

    for (int k = 0; k < 2; k++)
    {
        if (k == 1)
        {
            printf("Disabling SSSE3\n");
            CPLSetConfigOption("GDAL_USE_SSSE3", "NO");
        }

        {
            void *threeDstBuffers[] = {dst0, dst1, dst2};
            const auto start = clock();
            for (int i = 0; i < 2000 * (1024 / SIZE) * (1024 / SIZE); ++i)
                GDALDeinterleave(src, GDT_Byte, 3, threeDstBuffers, GDT_Byte,
                                 SIZE * SIZE);
            const auto end = clock();
            printf("GDALDeinterleave Byte 3 : %.2f\n",
                   (end - start) * 1.0 / CLOCKS_PER_SEC);
        }

        {
            void *fourDstBuffers[] = {dst0, dst1, dst2, dst3};
            const auto start = clock();
            for (int i = 0; i < 2000 * (1024 / SIZE) * (1024 / SIZE); ++i)
                GDALDeinterleave(src, GDT_Byte, 4, fourDstBuffers, GDT_Byte,
                                 SIZE * SIZE);
            const auto end = clock();
            printf("GDALDeinterleave Byte 4 : %.2f\n",
                   (end - start) * 1.0 / CLOCKS_PER_SEC);
        }

        {
            void *threeDstBuffers[] = {dst0, dst1, dst2};
            const auto start = clock();
            for (int i = 0; i < 2000 * (1024 / SIZE) * (1024 / SIZE); ++i)
                GDALDeinterleave(src, GDT_UInt16, 3, threeDstBuffers,
                                 GDT_UInt16, SIZE * SIZE / 2);
            const auto end = clock();
            printf("GDALDeinterleave UInt16 3 : %.2f\n",
                   (end - start) * 1.0 / CLOCKS_PER_SEC);
        }

        {
            void *fourDstBuffers[] = {dst0, dst1, dst2, dst3};
            const auto start = clock();
            for (int i = 0; i < 2000 * (1024 / SIZE) * (1024 / SIZE); ++i)
                GDALDeinterleave(src, GDT_UInt16, 4, fourDstBuffers, GDT_UInt16,
                                 SIZE * SIZE / 2);
            const auto end = clock();
            printf("GDALDeinterleave UInt16 4 : %.2f\n",
                   (end - start) * 1.0 / CLOCKS_PER_SEC);
        }
    }
    CPLSetConfigOption("GDAL_USE_SSSE3", nullptr);

    VSIFree(src);
    VSIFree(dst0);
    VSIFree(dst1);
    VSIFree(dst2);
    VSIFree(dst3);

    return 0;
}
