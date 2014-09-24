/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Gridding API.
 * Purpose:  Prototypes, and definitions for of GDAL scattered data gridder.
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "cpl_error.h"
#include "cpl_quad_tree.h"

typedef struct
{
    const double* padfX;
    const double* padfY;
} GDALGridXYArrays;

typedef struct
{
    GDALGridXYArrays* psXYArrays;
    int               i;
} GDALGridPoint;

typedef struct
{
    CPLQuadTree* hQuadTree;
    double       dfInitialSearchRadius;
    const float *pafX;
    const float *pafY;
    const float *pafZ;
} GDALGridExtraParameters;

#ifdef HAVE_AVX_AT_COMPILE_TIME
int CPLHaveRuntimeAVX();

CPLErr GDALGridInverseDistanceToAPower2NoSmoothingNoSearchAVX(
                                        const void *poOptions,
                                        GUInt32 nPoints,
                                        const double *unused_padfX,
                                        const double *unused_padfY,
                                        const double *unused_padfZ,
                                        double dfXPoint, double dfYPoint,
                                        double *pdfValue,
                                        void* hExtraParamsIn );
#endif

#if defined(__GNUC__) 
#define GCC_CPUID(level, a, b, c, d)            \
  __asm__ ("xchgl %%ebx, %1\n"                  \
           "cpuid\n"                            \
           "xchgl %%ebx, %1"                    \
       : "=a" (a), "=r" (b), "=c" (c), "=d" (d) \
       : "0" (level))
#endif
