///////////////////////////////////////////////////////////////////////////////
// $Id: test_gdal_aaigrid.cpp,v 1.3 2006/12/06 15:39:13 mloskot Exp $
//
// Project:  C++ Test Suite for GDAL/OGR
// Purpose:  Test Arc/Info ASCII Grid support. Ported from gdrivers/aaigrid.py.
// Author:   Mateusz Loskot <mateusz@loskot.net>
//
///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2006, Mateusz Loskot <mateusz@loskot.net>
// Copyright (c) 2010, Even Rouault <even dot rouault at mines-paris dot org>
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Library General Public
// License as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Library General Public License for more details.
//
// You should have received a copy of the GNU Library General Public
// License along with this library; if not, write to the
// Free Software Foundation, Inc., 59 Temple Place - Suite 330,
// Boston, MA 02111-1307, USA.
///////////////////////////////////////////////////////////////////////////////
//
//  $Log: test_gdal_aaigrid.cpp,v $
//  Revision 1.3  2006/12/06 15:39:13  mloskot
//  Added file header comment and copyright note.
//
//
///////////////////////////////////////////////////////////////////////////////
#include <tut.h> // TUT
#include <tut_gdal.h>
#include <gdal_common.h>
#include <gdal.h> // GDAL
#include <gdal_alg.h>
#include <gdal_priv.h>
#include <cpl_string.h>
#include <sstream> // C++
#include <string>
#include <vector>

#include <fstream>

namespace tut
{
    // Common fixture with test data
    struct test_aaigrid_data
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
        rasters_t grids_;
        rasters_t rasters_;

        test_aaigrid_data()
            : drv_(NULL), drv_name_("AAIGrid")
        {
            drv_ = GDALGetDriverByName(drv_name_.c_str());

            // Compose data path for test group
            data_ = tut::common::data_basedir;
            data_tmp_ = tut::common::tmp_basedir;

            // Collection of test AAIGrid grids
            grids_.push_back(raster_t("byte.tif.grd", 1, 4672));
            grids_.push_back(raster_t("pixel_per_line.asc", 1, 1123));

            // Collection of non-AAIGrid rasters
            rasters_.push_back(raster_t("byte.tif", 1, 4672));
        }
    };

    // Register test group
    typedef test_group<test_aaigrid_data> group;
    typedef group::object object;
    group test_aaigrid_group("GDAL::AAIGrid");

    // Test driver availability
    template<>
    template<>
    void object::test<1>()
    {
        ensure("GDAL::AAIGrid driver not available", NULL != drv_);
    }

    // Test open dataset
    template<>
    template<>
    void object::test<2>()
    {
        rasters_t::const_iterator it;
        for (it = grids_.begin(); it != grids_.end(); ++it)
        {
            std::string file(data_ + SEP);
            file += it->file_;
            GDALDatasetH ds = GDALOpen(file.c_str(), GA_ReadOnly);
            ensure("Can't open dataset: " + file, NULL != ds);
            GDALClose(ds);
        }
    }

    // Test dataset checksums
    template<>
    template<>
    void object::test<3>()
    {
        rasters_t::const_iterator it;
        for (it = grids_.begin(); it != grids_.end(); ++it)
        {
            std::string file(data_ + SEP);
            file += it->file_;

            GDALDatasetH ds = GDALOpen(file.c_str(), GA_ReadOnly);
            ensure("Can't open dataset: " + file, NULL != ds);

            GDALRasterBandH band = GDALGetRasterBand(ds, it->band_);
            ensure("Can't get raster band", NULL != band);

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
        const std::size_t fileIdx = 1;

        std::string file(data_ + SEP);
        file += grids_.at(fileIdx).file_;

        GDALDatasetH ds = GDALOpen(file.c_str(), GA_ReadOnly);
        ensure("Can't open dataset: " + file, NULL != ds);

        double geoTransform[6] = { 0 };
        CPLErr err = GDALGetGeoTransform(ds, geoTransform);
        ensure_equals("Can't fetch affine transformation coefficients", err, CE_None);

        // Test affine transformation coefficients
        const double maxError = 0.000001;
        const double expect[6] = { 100000.0, 50, 0, 650600.0, 0, -50 };
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
        const std::size_t fileIdx = 1;

        std::string file(data_ + SEP);
        file += grids_.at(fileIdx).file_;

        GDALDatasetH ds = GDALOpen(file.c_str(), GA_ReadOnly);
        ensure("Can't open dataset: " + file, NULL != ds);

        std::string proj(GDALGetProjectionRef(ds));
        ensure_equals("Projection definition is not available", proj.empty(), false);

        std::string expect(
            "PROJCS[\"unnamed\",GEOGCS[\"NAD83\",DATUM[\"North_American_Datum_1983\""
            ",SPHEROID[\"GRS 1980\",6378137,298.257222101,AUTHORITY[\"EPSG\",\"7019\"]],"
            "TOWGS84[0,0,0,0,0,0,0],AUTHORITY[\"EPSG\",\"6269\"]],PRIMEM[\"Greenwich\",0,"
            "AUTHORITY[\"EPSG\",\"8901\"]],UNIT[\"degree\",0.0174532925199433,"
            "AUTHORITY[\"EPSG\",\"9122\"]],"
            "AUTHORITY[\"EPSG\",\"4269\"]],PROJECTION[\"Albers_Conic_Equal_Area\"],"
            "PARAMETER[\"standard_parallel_1\",61.66666666666666],PARAMETER[\"standard_parallel_2\",68],"
            "PARAMETER[\"latitude_of_center\",59],PARAMETER[\"longitude_of_center\",-132.5],"
            "PARAMETER[\"false_easting\",500000],PARAMETER[\"false_northing\",500000],UNIT[\"METERS\",1]]");

        ensure_equals("Projection does not match expected", proj, expect);

        GDALClose(ds);
    }

    // Test band data type and NODATA value
    template<>
    template<>
    void object::test<6>()
    {
        // Index of test file being tested
        const std::size_t fileIdx = 1;

        std::string file(data_ + SEP);
        file += grids_.at(fileIdx).file_;

        GDALDatasetH ds = GDALOpen(file.c_str(), GA_ReadOnly);
        ensure("Can't open dataset: " + file, NULL != ds);

        GDALRasterBandH band = GDALGetRasterBand(ds, grids_.at(fileIdx).band_);
        ensure("Can't get raster band", NULL != band);

        const double noData = GDALGetRasterNoDataValue(band, NULL);
        ensure_equals("Grid NODATA value wrong or missing", noData, -99999);

        ensure_equals("Data type is not GDT_Float32", GDALGetRasterDataType(band), GDT_Float32);

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
        ensure("Can't open source dataset: " + src, NULL != dsSrc);

        std::string dst(data_tmp_ + SEP);
        dst += rasters_.at(fileIdx).file_;
        dst += ".grd";

        GDALDatasetH dsDst = NULL;
        dsDst = GDALCreateCopy(drv_, dst.c_str(), dsSrc, FALSE, NULL, NULL, NULL);
        GDALClose(dsSrc);
        ensure("Can't copy dataset", NULL != dsDst);

        std::string proj(GDALGetProjectionRef(dsDst));
        ensure_equals("Projection definition is not available", proj.empty(), false);

        std::string expect("PROJCS[\"NAD_1927_UTM_Zone_11N\",GEOGCS[\"GCS_North_American_1927\","
            "DATUM[\"North_American_Datum_1927\",SPHEROID[\"Clarke_1866\",6378206.4,294.9786982]],"
            "PRIMEM[\"Greenwich\",0],UNIT[\"Degree\",0.017453292519943295]],"
            "PROJECTION[\"Transverse_Mercator\"],PARAMETER[\"latitude_of_origin\",0],"
            "PARAMETER[\"central_meridian\",-117],PARAMETER[\"scale_factor\",0.9996],"
            "PARAMETER[\"false_easting\",500000],PARAMETER[\"false_northing\",0],UNIT[\"Meter\",1]]");

        ensure_equals("Projection does not match expected", proj, expect);

        GDALRasterBandH band = GDALGetRasterBand(dsDst, rasters_.at(fileIdx).band_);
        ensure("Can't get raster band", NULL != band);

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
        const std::size_t fileIdx = 1;

        std::string file(data_ + SEP);
        file += grids_.at(fileIdx).file_;

        GDALDatasetH ds = GDALOpen(file.c_str(), GA_ReadOnly);
        ensure("Can't open dataset: " + file, NULL != ds);

        GDALRasterBandH band = GDALGetRasterBand(ds, grids_.at(fileIdx).band_);
        ensure("Can't get raster band", NULL != band);

        // Sub-windows size
        const int win[4] = { 5, 5, 5, 5 };
        // subwindow checksum
        const int winChecksum = 187;
        const int checksum = GDALChecksumImage(band, win[0], win[1], win[2], win[3]);

        std::stringstream os;
        os << "Checksums for '" << file << "' not equal";
        ensure_equals(os.str().c_str(), checksum, winChecksum);

        GDALClose(ds);
    }

 } // namespace tut
