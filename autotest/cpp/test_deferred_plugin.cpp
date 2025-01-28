/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Test deferred plugin loading
 * Author:   Even Rouault, even.rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2023, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
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
