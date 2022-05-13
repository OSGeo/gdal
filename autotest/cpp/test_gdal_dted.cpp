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

#include <sstream>
#include <string>
#include <vector>

namespace tut
{

    // Common fixture with test data
    struct test_dted_data
    {
        struct raster_t
        {
            std::string file_;
            int band_;
            int checksum_;
            raster_t(std::string const& f, int b, int c)
                : file_(f), band_(b), checksum_(c)
            {}
        };
        typedef std::vector<raster_t> rasters_t;

        GDALDriverH drv_;
        std::string drv_name_;
        std::string data_;
        std::string data_tmp_;
        rasters_t rasters_;

        test_dted_data()
            : drv_(nullptr), drv_name_("DTED")
        {
            drv_ = GDALGetDriverByName(drv_name_.c_str());

            // Compose data path for test group
            data_ = tut::common::data_basedir;
            data_tmp_ = tut::common::tmp_basedir;

            // Collection of test DEM datasets

            // TODO: Verify value of this checksum
            rasters_.push_back(raster_t("n43.dt0", 1, 49187));

        }
    };

    // Register test group
    typedef test_group<test_dted_data> group;
    typedef group::object object;
    group test_dted_group("GDAL::DTED");

    // Test driver availability
    template<>
    template<>
    void object::test<1>()
    {
        ensure("GDAL::DTED driver not available", nullptr != drv_);
    }

    // Test open dataset
    template<>
    template<>
    void object::test<2>()
    {
        rasters_t::const_iterator it;
        for (it = rasters_.begin(); it != rasters_.end(); ++it)
        {
            std::string file(data_ + SEP);
            file += it->file_;
            GDALDatasetH ds = GDALOpen(file.c_str(), GA_ReadOnly);
            ensure("Can't open dataset: " + file, nullptr != ds);
            GDALClose(ds);
        }
    }

    // Test dataset checksums
    template<>
    template<>
    void object::test<3>()
    {

        rasters_t::const_iterator it;
        for (it = rasters_.begin(); it != rasters_.end(); ++it)
        {
            std::string file(data_ + SEP);
            file += it->file_;

            GDALDatasetH ds = GDALOpen(file.c_str(), GA_ReadOnly);
            ensure("Can't open dataset: " + file, nullptr != ds);

            GDALRasterBandH band = GDALGetRasterBand(ds, it->band_);
            ensure("Can't get raster band", nullptr != band);

            const int xsize = GDALGetRasterXSize(ds);
            const int ysize = GDALGetRasterYSize(ds);
            const int checksum = GDALChecksumImage(band, 0, 0, xsize, ysize);

            std::stringstream os;
            os << "Checksums for '" << file << "' not equal";
            ensure_equals(os.str().c_str(), checksum, it->checksum_);

            GDALClose(ds);
        }
    }

    // Test affine transformation coefficients
    template<>
    template<>
    void object::test<4>()
    {
        // Index of test file being tested
        const std::size_t fileIdx = 0;

        std::string file(data_ + SEP);
        file += rasters_.at(fileIdx).file_;

        GDALDatasetH ds = GDALOpen(file.c_str(), GA_ReadOnly);
        ensure("Can't open dataset: " + file, nullptr != ds);

        double geoTransform[6] = { 0 };
        CPLErr err = GDALGetGeoTransform(ds, geoTransform);
        ensure_equals("Can't fetch affine transformation coefficients", err, CE_None);

        // Test affine transformation coefficients
        const double maxError = 0.000001;
        const double expect[6] = {
            -80.004166666666663, 0.0083333333333333332, 0,
            44.00416666666667, 0, -0.0083333333333333332
        };
        const std::string msg("Geotransform is incorrect");
        ensure_distance(msg.c_str(), expect[0], geoTransform[0], maxError);
        ensure_distance(msg.c_str(), expect[1], geoTransform[1], maxError);
        ensure_distance(msg.c_str(), expect[2], geoTransform[2], maxError);
        ensure_distance(msg.c_str(), expect[3], geoTransform[3], maxError);
        ensure_distance(msg.c_str(), expect[4], geoTransform[4], maxError);
        ensure_distance(msg.c_str(), expect[5], geoTransform[5], maxError);

        GDALClose(ds);
    }

    // Test projection definition
    template<>
    template<>
    void object::test<5>()
    {
        // Index of test file being tested
        const std::size_t fileIdx = 0;

        std::string file(data_ + SEP);
        file += rasters_.at(fileIdx).file_;

        GDALDatasetH ds = GDALOpen(file.c_str(), GA_ReadOnly);
        ensure("Can't open dataset: " + file, nullptr != ds);

        std::string proj(GDALGetProjectionRef(ds));
        ensure_equals("Projection definition is not available", proj.empty(), false);

        std::string expect("GEOGCS[\"WGS 84\",DATUM[\"WGS_1984\",SPHEROID[\"WGS 84\",6378137,298.257223563,AUTHORITY[\"EPSG\",\"7030\"]],AUTHORITY[\"EPSG\",\"6326\"]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",\"8901\"]],UNIT[\"degree\",0.0174532925199433,AUTHORITY[\"EPSG\",\"9122\"]],AXIS[\"Latitude\",NORTH],AXIS[\"Longitude\",EAST],AUTHORITY[\"EPSG\",\"4326\"]]");
        ensure_equals("Projection does not match expected", proj, expect);

        GDALClose(ds);
    }

    // Test band data type and NODATA value
    template<>
    template<>
    void object::test<6>()
    {
        // Index of test file being tested
        const std::size_t fileIdx = 0;

        std::string file(data_ + SEP);
        file += rasters_.at(fileIdx).file_;

        GDALDatasetH ds = GDALOpen(file.c_str(), GA_ReadOnly);
        ensure("Can't open dataset: " + file, nullptr != ds);

        GDALRasterBandH band = GDALGetRasterBand(ds, rasters_.at(fileIdx).band_);
        ensure("Can't get raster band", nullptr != band);

        const double noData = GDALGetRasterNoDataValue(band, nullptr);
        ensure_equals("Grid NODATA value wrong or missing", noData, -32767);

        ensure_equals("Data type is not GDT_Int16", GDALGetRasterDataType(band), GDT_Int16);

        GDALClose(ds);
    }

    // Create simple copy and check
    template<>
    template<>
    void object::test<7>()
    {
        // Index of test file being tested
        const std::size_t fileIdx = 0;

        std::string src(data_ + SEP);
        src += rasters_.at(fileIdx).file_;

        GDALDatasetH dsSrc = GDALOpen(src.c_str(), GA_ReadOnly);
        ensure("Can't open source dataset: " + src, nullptr != dsSrc);

        std::string dst(data_tmp_ + SEP);
        dst += rasters_.at(fileIdx).file_;

        GDALDatasetH dsDst = nullptr;
        dsDst = GDALCreateCopy(drv_, dst.c_str(), dsSrc, FALSE, nullptr, nullptr, nullptr);
        GDALClose(dsSrc);
        ensure("Can't copy dataset", nullptr != dsDst);

        std::string proj(GDALGetProjectionRef(dsDst));
        ensure_equals("Projection definition is not available", proj.empty(), false);

        std::string expect("GEOGCS[\"WGS 84\",DATUM[\"WGS_1984\",SPHEROID[\"WGS 84\",6378137,298.257223563,AUTHORITY[\"EPSG\",\"7030\"]],AUTHORITY[\"EPSG\",\"6326\"]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",\"8901\"]],UNIT[\"degree\",0.0174532925199433,AUTHORITY[\"EPSG\",\"9122\"]],AXIS[\"Latitude\",NORTH],AXIS[\"Longitude\",EAST],AUTHORITY[\"EPSG\",\"4326\"]]");
        ensure_equals("Projection does not match expected", proj, expect);

        GDALRasterBandH band = GDALGetRasterBand(dsDst, rasters_.at(fileIdx).band_);
        ensure("Can't get raster band", nullptr != band);

        const int xsize = GDALGetRasterXSize(dsDst);
        const int ysize = GDALGetRasterYSize(dsDst);
        const int checksum = GDALChecksumImage(band, 0, 0, xsize, ysize);

        std::stringstream os;
        os << "Checksums for '" << dst << "' not equal";
        ensure_equals(os.str().c_str(), checksum, rasters_.at(fileIdx).checksum_);

        GDALClose(dsDst);
    }

    // Test subwindow read and the tail recursion problem.
    template<>
    template<>
    void object::test<8>()
    {
        // Index of test file being tested
        const std::size_t fileIdx = 0;

        std::string file(data_ + SEP);
        file += rasters_.at(fileIdx).file_;

        GDALDatasetH ds = GDALOpen(file.c_str(), GA_ReadOnly);
        ensure("Can't open dataset: " + file, nullptr != ds);

        GDALRasterBandH band = GDALGetRasterBand(ds, rasters_.at(fileIdx).band_);
        ensure("Can't get raster band", nullptr != band);

        // Sub-windows size
        const int win[4] = { 5, 5, 5, 5 };
        // subwindow checksum
        const int winChecksum = 305;
        const int checksum = GDALChecksumImage(band, win[0], win[1], win[2], win[3]);

        std::stringstream os;
        os << "Checksums for '" << file << "' not equal";
        ensure_equals(os.str().c_str(), checksum, winChecksum);

        GDALClose(ds);
    }

 } // namespace tut
