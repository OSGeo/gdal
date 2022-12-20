///////////////////////////////////////////////////////////////////////////////
//
// Project:  C++ Test Suite for GDAL/OGR
// Purpose:  Test read/write functionality for GeoTIFF format.
//           Ported from gcore/tiff_read.py, gcore/tiff_write.py.
// Author:   Mateusz Loskot <mateusz@loskot.net>
//
///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2006, Mateusz Loskot <mateusz@loskot.net>
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
struct test_gdal_gtiff : public ::testing::Test
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

    test_gdal_gtiff() : drv_(nullptr), drv_name_("GTiff")
    {
        drv_ = GDALGetDriverByName(drv_name_.c_str());

        // Compose data path for test group
        data_ = tut::common::data_basedir;
        data_tmp_ = tut::common::tmp_basedir;

        // Collection of test GeoTIFF rasters
        rasters_.push_back(raster_t("byte.tif", 1, 4672));
        rasters_.push_back(raster_t("int16.tif", 1, 4672));
        rasters_.push_back(raster_t("uint16.tif", 1, 4672));
        rasters_.push_back(raster_t("int32.tif", 1, 4672));
        rasters_.push_back(raster_t("uint32.tif", 1, 4672));
        rasters_.push_back(raster_t("float32.tif", 1, 4672));
        rasters_.push_back(raster_t("float64.tif", 1, 4672));
        rasters_.push_back(raster_t("cint16.tif", 1, 5028));
        rasters_.push_back(raster_t("cint32.tif", 1, 5028));
        rasters_.push_back(raster_t("cfloat32.tif", 1, 5028));
        rasters_.push_back(raster_t("cfloat64.tif", 1, 5028));
        rasters_.push_back(raster_t("utmsmall.tif", 1, 50054));
    }

    void SetUp() override
    {
        if (drv_ == nullptr)
            GTEST_SKIP() << "GTiff driver missing";
    }
};

// Test open dataset
TEST_F(test_gdal_gtiff, open)
{
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
TEST_F(test_gdal_gtiff, checksum)
{
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

        EXPECT_EQ(raster.checksum_, checksum);

        GDALClose(ds);
    }
}

// Test GeoTIFF driver metadata
TEST_F(test_gdal_gtiff, driver_metadata)
{
    const char *mdItem = GDALGetMetadataItem(drv_, "DMD_MIMETYPE", nullptr);
    ASSERT_TRUE(nullptr != mdItem);

    EXPECT_STREQ(mdItem, "image/tiff");
}

// Create a simple file by copying from an existing one
TEST_F(test_gdal_gtiff, copy)
{
    // Index of test file being copied
    const std::size_t fileIdx = 10;

    std::string src(data_ + SEP);
    src += rasters_.at(fileIdx).file_;
    GDALDatasetH dsSrc = GDALOpen(src.c_str(), GA_ReadOnly);
    ASSERT_TRUE(nullptr != dsSrc);

    std::string dst(data_tmp_ + "\\test_2.tif");
    GDALDatasetH dsDst = nullptr;
    dsDst = GDALCreateCopy(drv_, dst.c_str(), dsSrc, FALSE, nullptr, nullptr,
                           nullptr);
    ASSERT_TRUE(nullptr != dsDst);

    GDALClose(dsDst);
    GDALClose(dsSrc);

    // Re-open copied dataset and test it
    dsDst = GDALOpen(dst.c_str(), GA_ReadOnly);
    GDALRasterBandH band = GDALGetRasterBand(dsDst, rasters_.at(fileIdx).band_);
    ASSERT_TRUE(nullptr != band);

    const int xsize = GDALGetRasterXSize(dsDst);
    const int ysize = GDALGetRasterYSize(dsDst);
    const int checksum = GDALChecksumImage(band, 0, 0, xsize, ysize);

    EXPECT_EQ(rasters_.at(fileIdx).checksum_, checksum);

    GDALClose(dsDst);
    GDALDeleteDataset(drv_, dst.c_str());
}

// Create a simple file by copying from an existing one using creation options
TEST_F(test_gdal_gtiff, copy_creation_options)
{
    // Index of test file being copied
    const std::size_t fileIdx = 11;
    std::string src(data_ + SEP);
    src += rasters_.at(fileIdx).file_;
    GDALDatasetH dsSrc = GDALOpen(src.c_str(), GA_ReadOnly);
    ASSERT_TRUE(nullptr != dsSrc);

    std::string dst(data_tmp_ + "\\test_3.tif");

    char **options = nullptr;
    options = CSLSetNameValue(options, "TILED", "YES");
    options = CSLSetNameValue(options, "BLOCKXSIZE", "32");
    options = CSLSetNameValue(options, "BLOCKYSIZE", "32");

    GDALDatasetH dsDst = nullptr;
    dsDst = GDALCreateCopy(drv_, dst.c_str(), dsSrc, FALSE, options, nullptr,
                           nullptr);
    ASSERT_TRUE(nullptr != dsDst);

    GDALClose(dsDst);
    CSLDestroy(options);
    GDALClose(dsSrc);

    // Re-open copied dataset and test it
    dsDst = GDALOpen(dst.c_str(), GA_ReadOnly);
    GDALRasterBandH band = GDALGetRasterBand(dsDst, rasters_.at(fileIdx).band_);
    ASSERT_TRUE(nullptr != band);

    const int xsize = GDALGetRasterXSize(dsDst);
    const int ysize = GDALGetRasterYSize(dsDst);
    const int checksum = GDALChecksumImage(band, 0, 0, xsize, ysize);

    EXPECT_EQ(rasters_.at(fileIdx).checksum_, checksum);

    GDALClose(dsDst);
    GDALDeleteDataset(drv_, dst.c_str());
}

// Test raster min/max calculation
TEST_F(test_gdal_gtiff, raster_min_max)
{
    // Index of test file being copied
    const std::size_t fileIdx = 10;

    std::string src(data_ + SEP);
    src += rasters_.at(fileIdx).file_;
    GDALDatasetH ds = GDALOpen(src.c_str(), GA_ReadOnly);
    ASSERT_TRUE(nullptr != ds);

    GDALRasterBandH band = GDALGetRasterBand(ds, rasters_.at(fileIdx).band_);
    ASSERT_TRUE(nullptr != band);

    double expect[2] = {74.0, 255.0};
    double minmax[2] = {0};
    GDALComputeRasterMinMax(band, TRUE, minmax);

    EXPECT_EQ(expect[0], minmax[0]);
    EXPECT_EQ(expect[1], minmax[1]);

    GDALClose(ds);
}

}  // namespace
