///////////////////////////////////////////////////////////////////////////////
//
// Project:  C++ Test Suite for GDAL/OGR
// Purpose:  Test DTED support. Ported from gdrivers/dted.py.
// Author:   Mateusz Loskot <mateusz@loskot.net>
//
///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2006, Mateusz Loskot <mateusz@loskot.net>
// Copyright (c) 2010, Even Rouault <even dot rouault at spatialys.com>
/*
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

#include "cpl_string.h"
#include "gdal_alg.h"
#include "gdal_priv.h"
#include "gdal.h"

#include <vector>

#include "gtest_include.h"

namespace
{

// Common fixture with test data
struct test_gdal_dted : public ::testing::Test
{
    struct raster_t
    {
        std::string file_;
        int band_;
        int checksum_;

        raster_t(std::string const &f, int b, int c)
            : file_(f), band_(b), checksum_(c)
        {
        }
    };

    typedef std::vector<raster_t> rasters_t;

    GDALDriverH drv_;
    std::string drv_name_;
    std::string data_;
    std::string data_tmp_;
    rasters_t rasters_;

    test_gdal_dted() : drv_(nullptr), drv_name_("DTED")
    {
        drv_ = GDALGetDriverByName(drv_name_.c_str());

        // Compose data path for test group
        data_ = tut::common::data_basedir;
        data_tmp_ = tut::common::tmp_basedir;

        // Collection of test DEM datasets

        // TODO: Verify value of this checksum
        rasters_.push_back(raster_t("n43.dt0", 1, 49187));
    }

    void SetUp() override
    {
        if (drv_ == nullptr)
            GTEST_SKIP() << "DTED driver missing";
    }
};

// Test open dataset
TEST_F(test_gdal_dted, open)
{
    if (drv_ == nullptr)
        return;
    for (const auto &raster : rasters_)
    {
        std::string file(data_ + SEP);
        file += raster.file_;
        GDALDatasetH ds = GDALOpen(file.c_str(), GA_ReadOnly);
        ASSERT_TRUE(nullptr != ds);
        GDALClose(ds);
    }
}

// Test dataset checksums
TEST_F(test_gdal_dted, checksums)
{
    if (drv_ == nullptr)
        return;
    for (const auto &raster : rasters_)
    {
        std::string file(data_ + SEP);
        file += raster.file_;

        GDALDatasetH ds = GDALOpen(file.c_str(), GA_ReadOnly);
        ASSERT_TRUE(nullptr != ds);

        GDALRasterBandH band = GDALGetRasterBand(ds, raster.band_);
        ASSERT_TRUE(nullptr != band);

        const int xsize = GDALGetRasterXSize(ds);
        const int ysize = GDALGetRasterYSize(ds);
        const int checksum = GDALChecksumImage(band, 0, 0, xsize, ysize);

        EXPECT_EQ(checksum, raster.checksum_);

        GDALClose(ds);
    }
}

// Test affine transformation coefficients
TEST_F(test_gdal_dted, geotransform)
{
    // Index of test file being tested
    const std::size_t fileIdx = 0;

    std::string file(data_ + SEP);
    file += rasters_.at(fileIdx).file_;

    GDALDatasetH ds = GDALOpen(file.c_str(), GA_ReadOnly);
    ASSERT_TRUE(nullptr != ds);

    double geoTransform[6] = {0};
    CPLErr err = GDALGetGeoTransform(ds, geoTransform);
    ASSERT_EQ(err, CE_None);

    // Test affine transformation coefficients
    const double maxError = 0.000001;
    const double expect[6] = {
        -80.004166666666663,   0.0083333333333333332, 0, 44.00416666666667, 0,
        -0.0083333333333333332};
    EXPECT_NEAR(expect[0], geoTransform[0], maxError);
    EXPECT_NEAR(expect[1], geoTransform[1], maxError);
    EXPECT_NEAR(expect[2], geoTransform[2], maxError);
    EXPECT_NEAR(expect[3], geoTransform[3], maxError);
    EXPECT_NEAR(expect[4], geoTransform[4], maxError);
    EXPECT_NEAR(expect[5], geoTransform[5], maxError);

    GDALClose(ds);
}

// Test projection definition
TEST_F(test_gdal_dted, projection)
{
    // Index of test file being tested
    const std::size_t fileIdx = 0;

    std::string file(data_ + SEP);
    file += rasters_.at(fileIdx).file_;

    GDALDatasetH ds = GDALOpen(file.c_str(), GA_ReadOnly);
    ASSERT_TRUE(nullptr != ds);

    std::string proj(GDALGetProjectionRef(ds));
    ASSERT_TRUE(!proj.empty());

    std::string expect(
        "GEOGCS[\"WGS 84\",DATUM[\"WGS_1984\",SPHEROID[\"WGS "
        "84\",6378137,298.257223563,AUTHORITY[\"EPSG\",\"7030\"]],AUTHORITY["
        "\"EPSG\",\"6326\"]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\","
        "\"8901\"]],UNIT[\"degree\",0.0174532925199433,AUTHORITY[\"EPSG\","
        "\"9122\"]],AXIS[\"Latitude\",NORTH],AXIS[\"Longitude\",EAST],"
        "AUTHORITY[\"EPSG\",\"4326\"]]");
    EXPECT_EQ(proj, expect);

    GDALClose(ds);
}

// Test band data type and NODATA value
TEST_F(test_gdal_dted, nodata)
{
    // Index of test file being tested
    const std::size_t fileIdx = 0;

    std::string file(data_ + SEP);
    file += rasters_.at(fileIdx).file_;

    GDALDatasetH ds = GDALOpen(file.c_str(), GA_ReadOnly);
    ASSERT_TRUE(nullptr != ds);

    GDALRasterBandH band = GDALGetRasterBand(ds, rasters_.at(fileIdx).band_);
    ASSERT_TRUE(nullptr != band);

    const double noData = GDALGetRasterNoDataValue(band, nullptr);
    EXPECT_EQ(noData, -32767);

    EXPECT_EQ(GDALGetRasterDataType(band), GDT_Int16);

    GDALClose(ds);
}

// Create simple copy and check
TEST_F(test_gdal_dted, copy)
{
    // Index of test file being tested
    const std::size_t fileIdx = 0;

    std::string src(data_ + SEP);
    src += rasters_.at(fileIdx).file_;

    GDALDatasetH dsSrc = GDALOpen(src.c_str(), GA_ReadOnly);
    ASSERT_TRUE(nullptr != dsSrc);

    std::string dst(data_tmp_ + SEP);
    dst += rasters_.at(fileIdx).file_;

    GDALDatasetH dsDst = nullptr;
    dsDst = GDALCreateCopy(drv_, dst.c_str(), dsSrc, FALSE, nullptr, nullptr,
                           nullptr);
    GDALClose(dsSrc);
    ASSERT_TRUE(nullptr != dsDst);

    std::string proj(GDALGetProjectionRef(dsDst));
    ASSERT_TRUE(!proj.empty());

    std::string expect(
        "GEOGCS[\"WGS 84\",DATUM[\"WGS_1984\",SPHEROID[\"WGS "
        "84\",6378137,298.257223563,AUTHORITY[\"EPSG\",\"7030\"]],AUTHORITY["
        "\"EPSG\",\"6326\"]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\","
        "\"8901\"]],UNIT[\"degree\",0.0174532925199433,AUTHORITY[\"EPSG\","
        "\"9122\"]],AXIS[\"Latitude\",NORTH],AXIS[\"Longitude\",EAST],"
        "AUTHORITY[\"EPSG\",\"4326\"]]");
    EXPECT_EQ(proj, expect);

    GDALRasterBandH band = GDALGetRasterBand(dsDst, rasters_.at(fileIdx).band_);
    ASSERT_TRUE(nullptr != band);

    const int xsize = GDALGetRasterXSize(dsDst);
    const int ysize = GDALGetRasterYSize(dsDst);
    const int checksum = GDALChecksumImage(band, 0, 0, xsize, ysize);

    EXPECT_EQ(checksum, rasters_.at(fileIdx).checksum_);

    GDALClose(dsDst);
}

// Test subwindow read and the tail recursion problem.
TEST_F(test_gdal_dted, subwindow_read)
{
    // Index of test file being tested
    const std::size_t fileIdx = 0;

    std::string file(data_ + SEP);
    file += rasters_.at(fileIdx).file_;

    GDALDatasetH ds = GDALOpen(file.c_str(), GA_ReadOnly);
    ASSERT_TRUE(nullptr != ds);

    GDALRasterBandH band = GDALGetRasterBand(ds, rasters_.at(fileIdx).band_);
    ASSERT_TRUE(nullptr != band);

    // Sub-windows size
    const int win[4] = {5, 5, 5, 5};
    // subwindow checksum
    const int winChecksum = 305;
    const int checksum =
        GDALChecksumImage(band, win[0], win[1], win[2], win[3]);

    EXPECT_EQ(checksum, winChecksum);

    GDALClose(ds);
}

}  // namespace
