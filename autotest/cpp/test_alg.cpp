/******************************************************************************
 * $Id$
 *
 * Project:  GDAL algorithms
 * Purpose:  Test alg
 * Author:   Even Rouault, even.rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2016, Even Rouault <even.rouault at spatialys.com>
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

#include "gdal_unit_test.h"

#include <gdal_alg.h>

namespace tut
{
    // Common fixture with test data
    struct test_alg_data
    {
    };

    // Register test group
    typedef test_group<test_alg_data> group;
    typedef group::object object;
    group test_alg_group("alg");

    typedef struct
    {
      double dfLevel;
      int nPoints;
      double x;
      double y;
    } writeCbkData;

    static CPLErr writeCbk( double dfLevel, int nPoints,
                            double *padfX, double *padfY, void * userData )
    {
        writeCbkData* data = (writeCbkData*)userData;
        data->dfLevel = dfLevel;
        data->nPoints = nPoints;
        if( nPoints == 1 )
        {
            data->x = padfX[0];
            data->y = padfY[0];
        }
        return CE_None;
    }

    // Dummy test
    template<>
    template<>
    void object::test<1>()
    {
        writeCbkData data;
        memset(&data, 0, sizeof(data));
        GDALContourGeneratorH hCG = GDAL_CG_Create(1,1,FALSE,0,1,0, writeCbk, &data);
        double scanline[] = { 0 };
        ensure_equals(GDAL_CG_FeedLine(hCG, scanline), CE_None);
        ensure_equals(data.dfLevel, 0);
        ensure_equals(data.nPoints, 0);
        ensure_approx_equals(data.x, 0.0);
        ensure_approx_equals(data.y, 0.0);
        GDAL_CG_Destroy(hCG);
    }
} // namespace tut
