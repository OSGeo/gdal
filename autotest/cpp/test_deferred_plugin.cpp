/******************************************************************************
 * $Id$
 *
 * Project:  GDAL
 * Purpose:  Test deferred plugin loading
 * Author:   Even Rouault, even.rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2023, Even Rouault <even.rouault at spatialys.com>
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

#include "gdal_priv.h"

#include "gtest_include.h"

#include "test_data.h"

namespace
{

// ---------------------------------------------------------------------------

TEST(test_deferredplugin, test_missing)
{
#ifdef JPEG_PLUGIN
    CPLSetConfigOption("GDAL_DRIVER_PATH", "/i/do_not_exist");
    GDALAllRegister();
    CPLSetConfigOption("GDAL_DRIVER_PATH", nullptr);
    GDALDriverH hDrv = GDALGetDriverByName("JPEG");
    EXPECT_EQ(hDrv, nullptr);
    CPLPushErrorHandler(CPLQuietErrorHandler);
    GDALClose(
        GDALOpen(GDRIVERS_DIR "data/jpeg/byte_with_xmp.jpg", GA_ReadOnly));
    CPLPopErrorHandler();
    EXPECT_TRUE(
        strstr(CPLGetLastErrorMsg(),
               "It could have been recognized by driver JPEG, but plugin") !=
        nullptr);
#else
    GTEST_SKIP() << "JPEG driver not built or not built as a plugin";
#endif
}

TEST(test_deferredplugin, test_nominal)
{
#ifdef JPEG_PLUGIN
    GDALAllRegister();
    GDALDriverH hDrv = GDALGetDriverByName("JPEG");
    ASSERT_NE(hDrv, nullptr);
    EXPECT_NE(GDALDriver::FromHandle(hDrv)->pfnIdentify, nullptr);
    EXPECT_STREQ(GDALGetMetadataItem(hDrv, GDAL_DCAP_OPEN, nullptr), "YES");
    EXPECT_EQ(GDALDriver::FromHandle(hDrv)->pfnOpen, nullptr);
    GDALDatasetH hDS =
        GDALOpen(GDRIVERS_DIR "data/jpeg/byte_with_xmp.jpg", GA_ReadOnly);
    EXPECT_NE(hDS, nullptr);
    EXPECT_NE(GDALDriver::FromHandle(hDrv)->pfnOpen, nullptr);
    GDALClose(hDS);
#else
    GTEST_SKIP() << "JPEG driver not built or not built as a plugin";
#endif
}

}  // namespace
