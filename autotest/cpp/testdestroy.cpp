/******************************************************************************
 *
 * Project:  GDAL Core
 * Purpose:  Test GDALDestroy().
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2015, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdal.h"

#include "gtest_include.h"

namespace
{

// ---------------------------------------------------------------------------

TEST(testdestroy, test)
{
    GDALAllRegister();
    /* See corresponding bug reports: */
    /* https://trac.osgeo.org/gdal/ticket/6139 */
    /* https://trac.osgeo.org/gdal/ticket/6868 */
    CPLError(CE_None, CPLE_AppDefined,
             "Expected, CPLError called to trigger hErrorMutex allocation");
    GDALDestroy();
}

}  // namespace
