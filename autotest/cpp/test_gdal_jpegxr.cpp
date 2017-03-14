///////////////////////////////////////////////////////////////////////////////
//
// Project:  C++ Test Suite for GDAL/OGR
// Purpose:  Test read/write functionality for JPEG XR format.
// Author:   Mateusz Loskot <mateusz@loskot.net>
//
///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017, Mateusz Loskot <mateusz@loskot.net>
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

#include "gdal_unit_test.h"

#include <cpl_string.h>
#include <gdal_alg.h>
#include <gdal_priv.h>
#include <gdal.h>

#include <cstring>
#include <string>

namespace tut
{
    // RAII wrappers
    struct Dataset
    {
        Dataset() : hDS(NULL) {}
        Dataset(Dataset const&) /*= delete*/;
        Dataset(GDALDatasetH hDS) : hDS(hDS) {}
        ~Dataset() { GDALClose(hDS); }
        Dataset& operator=(Dataset const& rhs) /*= delete*/;
        Dataset& operator=(GDALDatasetH const& rhs)
        {
            if (hDS != rhs)
            {
                GDALClose(hDS);
                hDS = rhs;
            }
            return *this;
        }
        explicit operator GDALDatasetH() const { return hDS; }
        GDALDatasetH hDS;
    };

    // Common fixture with test data
    struct test_jpegxr_data
    {
        std::string drv_name_;
        std::string data_;
        std::string data_tmp_;
        GDALDriverH drv_;
        Dataset ds_;

        test_jpegxr_data()
            : drv_name_("JPEGXR"), drv_(NULL)
        {
            drv_ = GDALGetDriverByName(drv_name_.c_str());

            // Compose data path for test group
            data_ = tut::common::data_basedir;
            data_tmp_ = tut::common::tmp_basedir;
        }

        std::string get_file_path(char const* filename)
        {
            std::string path(data_ + SEP);
            return path + filename;
        }

        std::string get_temp_path(char const* filename)
        {
            std::string path(data_tmp_ + SEP);
            return path + filename;
        }

        GUIntBig get_file_size(char const* filename)
        {
            VSIStatBufL sStat;
            std::memset(&sStat, 0, sizeof(sStat));
            if (VSIStatL(filename, &sStat) != 0)
                throw std::runtime_error("failed to get file size");
            return sStat.st_size;
        }

        void ensure_stats(GDALDriverH hDS, int nBand, double dfMin, double dfMax, double dfMean, double dfStdD)
        {
            double dfMinAct(0.0);
            double dfMaxAct(0.0);
            double dfMeanAct(0.0);
            double dfStdDAct(0.0);
            ensure_equals(GDALComputeRasterStatistics(GDALGetRasterBand(hDS, nBand), false, &dfMinAct, &dfMaxAct, &dfMeanAct, &dfStdDAct, NULL, NULL), CE_None);
            ensure_equals("Min", static_cast<int>(dfMinAct), static_cast<int>(dfMin));
            ensure_equals("Max", static_cast<int>(dfMaxAct), static_cast<int>(dfMax));
            ensure_equals("Mean", static_cast<int>(dfMeanAct), static_cast<int>(dfMean));
            ensure_equals("StdD", static_cast<int>(dfStdDAct), static_cast<int>(dfStdD));
        }
    };

    // Register test group
    typedef test_group<test_jpegxr_data> group;
    typedef group::object object;
    group test_jpegxr_group("GDAL::JPEGXR");

    // Test driver availability
    template<>
    template<>
    void object::test<1>()
    {
        ensure("GDAL::JPEGXR driver not available", 0 != drv_);
    }

    // NOTE: Test categories do:
    // OPEN - open dataset, access basic properties without performing any I/O
    // READ - I/O with GDALRasterBand::IReadBlock or GDALRasterBand::IRasterIO
    // COPY - I/O with GDALDataset::CreateCopy

    // NOTE: Differences in checksum and statistics values have been observed
    //       on various operating systems/architectures.
    //       Number of factors might be affecting them:
    //       - platform-specific implementation details of jxrlib
    //       - rounding errors
    //       - values recovered from lossy compressed images might differ
    //       See also https://trac.osgeo.org/gdal/ticket/1838

    // OPEN ////////////////////////////////////////////////////////////////////

    // OPEN: 8bpp Gray
    template<>
    template<>
    void object::test<2>()
    {
        ensure(0 != drv_);
        std::string file = get_file_path("lenna-256x256-8bpp-Gray.jxr");
        ds_ = GDALOpen(file.c_str(), GA_ReadOnly);
        ensure(ds_.hDS);
        ensure_equals(GDALGetRasterXSize(ds_.hDS), 256);
        ensure_equals(GDALGetRasterYSize(ds_.hDS), 256);
        ensure_equals(GDALGetRasterCount(ds_.hDS), 1);
        ensure_equals(GDALGetRasterDataType(GDALGetRasterBand(ds_.hDS, 1)), GDT_Byte);
        ensure_equals(GDALGetRasterColorInterpretation(GDALGetRasterBand(ds_.hDS, 1)), GCI_GrayIndex);
    }

    // OPEN: 24bpp BGR
    template<>
    template<>
    void object::test<3>()
    {
        ensure(0 != drv_);
        std::string file = get_file_path("lenna-256x256-24bpp-BGR.jxr");
        ds_ = GDALOpen(file.c_str(), GA_ReadOnly);
        ensure(ds_.hDS);
        ensure_equals(GDALGetRasterXSize(ds_.hDS), 256);
        ensure_equals(GDALGetRasterYSize(ds_.hDS), 256);
        ensure_equals(GDALGetRasterCount(ds_.hDS), 3);
        for (int i = 0; i < GDALGetRasterCount(ds_.hDS); i++)
            ensure_equals(GDALGetRasterDataType(GDALGetRasterBand(ds_.hDS, i + 1)), GDT_Byte);
        ensure_equals(GDALGetRasterColorInterpretation(GDALGetRasterBand(ds_.hDS, 1)), GCI_BlueBand);
        ensure_equals(GDALGetRasterColorInterpretation(GDALGetRasterBand(ds_.hDS, 2)), GCI_GreenBand);
        ensure_equals(GDALGetRasterColorInterpretation(GDALGetRasterBand(ds_.hDS, 3)), GCI_RedBand);
    }

    // OPEN: 24bpp RGB
    template<>
    template<>
    void object::test<4>()
    {
        ensure(0 != drv_);
        std::string file = get_file_path("mandril-512x512-24bpp-RGB.jxr");
        ds_ = GDALOpen(file.c_str(), GA_ReadOnly);
        ensure(ds_.hDS);
        ensure_equals(GDALGetRasterXSize(ds_.hDS), 512);
        ensure_equals(GDALGetRasterYSize(ds_.hDS), 512);
        ensure_equals(GDALGetRasterCount(ds_.hDS), 3);
        for (int i = 0; i < GDALGetRasterCount(ds_.hDS); i++)
            ensure_equals(GDALGetRasterDataType(GDALGetRasterBand(ds_.hDS, i + 1)), GDT_Byte);
        ensure_equals(GDALGetRasterColorInterpretation(GDALGetRasterBand(ds_.hDS, 1)), GCI_RedBand);
        ensure_equals(GDALGetRasterColorInterpretation(GDALGetRasterBand(ds_.hDS, 2)), GCI_GreenBand);
        ensure_equals(GDALGetRasterColorInterpretation(GDALGetRasterBand(ds_.hDS, 3)), GCI_BlueBand);
    }

    // OPEN: 32bpp BGRA
    template<>
    template<>
    void object::test<5>()
    {
        ensure(0 != drv_);
        std::string file = get_file_path("lenna-256x256-32bpp-BGRA.jxr");
        ds_ = GDALOpen(file.c_str(), GA_ReadOnly);
        ensure(ds_.hDS);
        ensure_equals(GDALGetRasterXSize(ds_.hDS), 256);
        ensure_equals(GDALGetRasterYSize(ds_.hDS), 256);
        ensure_equals(GDALGetRasterCount(ds_.hDS), 4);
        for (int i = 0; i < GDALGetRasterCount(ds_.hDS); i++)
            ensure_equals(GDALGetRasterDataType(GDALGetRasterBand(ds_.hDS, i + 1)), GDT_Byte);
        ensure_equals(GDALGetRasterColorInterpretation(GDALGetRasterBand(ds_.hDS, 1)), GCI_BlueBand);
        ensure_equals(GDALGetRasterColorInterpretation(GDALGetRasterBand(ds_.hDS, 2)), GCI_GreenBand);
        ensure_equals(GDALGetRasterColorInterpretation(GDALGetRasterBand(ds_.hDS, 3)), GCI_RedBand);
        ensure_equals(GDALGetRasterColorInterpretation(GDALGetRasterBand(ds_.hDS, 4)), GCI_AlphaBand);
    }

    // OPEN: 32bpp RGBA
    template<>
    template<>
    void object::test<6>()
    {
        ensure(0 != drv_);
        std::string file = get_file_path("lenna-256x256-32bpp-RGBA.jxr");
        ds_ = GDALOpen(file.c_str(), GA_ReadOnly);
        ensure(ds_.hDS);
        ensure_equals(GDALGetRasterXSize(ds_.hDS), 256);
        ensure_equals(GDALGetRasterYSize(ds_.hDS), 256);
        ensure_equals(GDALGetRasterCount(ds_.hDS), 4);
        for (int i = 0; i < GDALGetRasterCount(ds_.hDS); i++)
            ensure_equals(GDALGetRasterDataType(GDALGetRasterBand(ds_.hDS, i + 1)), GDT_Byte);
        ensure_equals(GDALGetRasterColorInterpretation(GDALGetRasterBand(ds_.hDS, 1)), GCI_RedBand);
        ensure_equals(GDALGetRasterColorInterpretation(GDALGetRasterBand(ds_.hDS, 2)), GCI_GreenBand);
        ensure_equals(GDALGetRasterColorInterpretation(GDALGetRasterBand(ds_.hDS, 3)), GCI_BlueBand);
        ensure_equals(GDALGetRasterColorInterpretation(GDALGetRasterBand(ds_.hDS, 4)), GCI_AlphaBand);
    }

    // READ ////////////////////////////////////////////////////////////////////

    // READ: 8bpp Gray
    template<>
    template<>
    void object::test<7>()
    {
        ensure(0 != drv_);
        std::string file = get_file_path("lenna-256x256-8bpp-Gray.jxr");
        ds_ = GDALOpen(file.c_str(), GA_ReadOnly);
        ensure(ds_.hDS);
        ensure_stats(ds_.hDS, 1, 0, 253, 99, 52);
        // re-test with checksum
        ensure_equals(GDALChecksumImage(GDALGetRasterBand(ds_.hDS, 1), 0, 0, GDALGetRasterXSize(ds_.hDS), GDALGetRasterYSize(ds_.hDS)), 60269);
    }

    // READ: 24bpp BGR
    template<>
    template<>
    void object::test<8>()
    {
        ensure(0 != drv_);
        std::string file = get_file_path("lenna-256x256-24bpp-BGR.jxr");
        ds_ = GDALOpen(file.c_str(), GA_ReadOnly);
        ensure(ds_.hDS);
        ensure_stats(ds_.hDS, 1, 45, 214, 105, 33);
        ensure_stats(ds_.hDS, 2,  4, 239,  99, 52);
        ensure_stats(ds_.hDS, 3, 58, 255, 180, 48);
        // re-test with checksum
        ensure_equals(GDALChecksumImage(GDALGetRasterBand(ds_.hDS, 1), 0, 0, GDALGetRasterXSize(ds_.hDS), GDALGetRasterYSize(ds_.hDS)), 62731);
        ensure_equals(GDALChecksumImage(GDALGetRasterBand(ds_.hDS, 2), 0, 0, GDALGetRasterXSize(ds_.hDS), GDALGetRasterYSize(ds_.hDS)), 63106);
        ensure_equals(GDALChecksumImage(GDALGetRasterBand(ds_.hDS, 3), 0, 0, GDALGetRasterXSize(ds_.hDS), GDALGetRasterYSize(ds_.hDS)), 34990);
    }

    // READ: 24bpp RGB
    template<>
    template<>
    void object::test<9>()
    {
        ensure(0 != drv_);
        std::string file = get_file_path("mandril-512x512-24bpp-RGB.jxr");
        ds_ = GDALOpen(file.c_str(), GA_ReadOnly);
        ensure(ds_.hDS);
        ensure_stats(ds_.hDS, 1, 5, 250, 129, 56);
        ensure_stats(ds_.hDS, 2, 0, 208, 121, 48);
        ensure_stats(ds_.hDS, 3, 0, 244, 105, 62);
        // re-test with checksum
        ensure_equals(GDALChecksumImage(GDALGetRasterBand(ds_.hDS, 1), 0, 0, GDALGetRasterXSize(ds_.hDS), GDALGetRasterYSize(ds_.hDS)), 54211);
        ensure_equals(GDALChecksumImage(GDALGetRasterBand(ds_.hDS, 2), 0, 0, GDALGetRasterXSize(ds_.hDS), GDALGetRasterYSize(ds_.hDS)), 51131);
        ensure_equals(GDALChecksumImage(GDALGetRasterBand(ds_.hDS, 3), 0, 0, GDALGetRasterXSize(ds_.hDS), GDALGetRasterYSize(ds_.hDS)), 12543);
    }

    // READ: 32bpp BGRA
    template<>
    template<>
    void object::test<10>()
    {
        ensure(0 != drv_);
        std::string file = get_file_path("lenna-256x256-32bpp-BGRA.jxr");
        ds_ = GDALOpen(file.c_str(), GA_ReadOnly);
        ensure(ds_.hDS);
        ensure_stats(ds_.hDS, 1,  45, 214, 105, 33);
        ensure_stats(ds_.hDS, 2,   4, 239,  99, 52);
        ensure_stats(ds_.hDS, 3,  58, 255, 180, 48);
        ensure_stats(ds_.hDS, 4, 255, 255, 255,  0);
        // re-test with checksum
        ensure_equals(GDALChecksumImage(GDALGetRasterBand(ds_.hDS, 1), 0, 0, GDALGetRasterXSize(ds_.hDS), GDALGetRasterYSize(ds_.hDS)), 62731);
        ensure_equals(GDALChecksumImage(GDALGetRasterBand(ds_.hDS, 2), 0, 0, GDALGetRasterXSize(ds_.hDS), GDALGetRasterYSize(ds_.hDS)), 63106);
        ensure_equals(GDALChecksumImage(GDALGetRasterBand(ds_.hDS, 3), 0, 0, GDALGetRasterXSize(ds_.hDS), GDALGetRasterYSize(ds_.hDS)), 34990);
        ensure_equals(GDALChecksumImage(GDALGetRasterBand(ds_.hDS, 4), 0, 0, GDALGetRasterXSize(ds_.hDS), GDALGetRasterYSize(ds_.hDS)), 17849);
    }

    // READ: 32bpp RGBA
    template<>
    template<>
    void object::test<11>()
    {
        ensure(0 != drv_);
        std::string file = get_file_path("lenna-256x256-32bpp-RGBA.jxr");
        ds_ = GDALOpen(file.c_str(), GA_ReadOnly);
        ensure(ds_.hDS);
        ensure_stats(ds_.hDS, 1, 55, // BGRA: 58 (rounding error)
                              255, 180, 48);
        ensure_stats(ds_.hDS, 2,  3, // BGRA: 4 (rounding error)
                              239, 99, 52);
        ensure_stats(ds_.hDS, 3, 44, // BGRA: 45 (rounding error)
                              213, // BGRA: 214 (rounding error)
                              105, 33);
        ensure_stats(ds_.hDS, 4, 255, 255, 255, 0);
        // re-test with checksum
        ensure_equals(GDALChecksumImage(GDALGetRasterBand(ds_.hDS, 1), 0, 0, GDALGetRasterXSize(ds_.hDS), GDALGetRasterYSize(ds_.hDS)), 38199);
        ensure_equals(GDALChecksumImage(GDALGetRasterBand(ds_.hDS, 2), 0, 0, GDALGetRasterXSize(ds_.hDS), GDALGetRasterYSize(ds_.hDS)), 61818);
        ensure_equals(GDALChecksumImage(GDALGetRasterBand(ds_.hDS, 3), 0, 0, GDALGetRasterXSize(ds_.hDS), GDALGetRasterYSize(ds_.hDS)), 61402);
        ensure_equals(GDALChecksumImage(GDALGetRasterBand(ds_.hDS, 4), 0, 0, GDALGetRasterXSize(ds_.hDS), GDALGetRasterYSize(ds_.hDS)), 17849);
    }

    // COPY ////////////////////////////////////////////////////////////////////

    // COPY: 8bpp Gray, no options/encoding defaults
    template<>
    template<>
    void object::test<12>()
    {
        ensure(0 != drv_);
        std::string src_file = get_file_path("lenna-256x256-8bpp-Gray.tif");
        ds_ = GDALOpen(src_file.c_str(), GA_ReadOnly);
        ensure(ds_.hDS);

        std::string dst_file = get_temp_path("lenna-256x256-8bpp-Gray.jxr");
        // Create copy
        {
            Dataset dst = GDALCreateCopy(drv_, dst_file.c_str(), ds_.hDS, FALSE, 0, 0, 0);
            ensure(dst.hDS);
        }
        // Verify copy
        {
            Dataset dst = GDALOpen(dst_file.c_str(), GA_ReadOnly);
            ensure(dst.hDS);
            ensure_equals(GDALGetRasterXSize(dst.hDS), 256);
            ensure_equals(GDALGetRasterYSize(dst.hDS), 256);
            ensure_equals(GDALGetRasterCount(dst.hDS), 1);
            ensure_equals(GDALGetRasterDataType(GDALGetRasterBand(ds_.hDS, 1)), GDT_Byte);
        }
        GDALDeleteDataset(drv_, dst_file.c_str());
    }

    // COPY: 24bpp RGB, no options/encoding defaults
    template<>
    template<>
    void object::test<13>()
    {
        ensure(0 != drv_);
        std::string src_file = get_file_path("fabio-256x256-24bpp-RGB.png");
        ds_ = GDALOpen(src_file.c_str(), GA_ReadOnly);
        ensure(ds_.hDS);

        std::string dst_file = get_temp_path("fabio-256x256-24bpp-RGB.jxr");
        // Create copy
        {
            Dataset dst = GDALCreateCopy(drv_, dst_file.c_str(), ds_.hDS, FALSE, 0, 0, 0);
            ensure(dst.hDS);
        }
        // Verify copy
        {
            Dataset dst = GDALOpen(dst_file.c_str(), GA_ReadOnly);
            ensure(dst.hDS);
            ensure_equals(GDALGetRasterXSize(dst.hDS), 256);
            ensure_equals(GDALGetRasterYSize(dst.hDS), 256);
            ensure_equals(GDALGetRasterCount(dst.hDS), 3);
            for (int i = 0; i < GDALGetRasterCount(ds_.hDS); i++)
                ensure_equals(GDALGetRasterDataType(GDALGetRasterBand(ds_.hDS, i + 1)), GDT_Byte);

        }
        GDALDeleteDataset(drv_, dst_file.c_str());
    }

    // COPY: 8bpp Gray, QUALITY
    template<>
    template<>
    void object::test<14>()
    {
        ensure(0 != drv_);
        std::string src_file = get_file_path("lenna-256x256-8bpp-Gray.tif");
        ds_ = GDALOpen(src_file.c_str(), GA_ReadOnly);
        ensure(ds_.hDS);

        GUInt64 const src_size = get_file_size(src_file.c_str());

        char** options = 0;
        options = CSLSetNameValue(options, "QUALITY", "75");

        std::string dst_file = get_temp_path("lenna-256x256-8bpp-Gray.jxr");
        // Create copy
        {
            Dataset dst = GDALCreateCopy(drv_, dst_file.c_str(), ds_.hDS, false, options, 0, 0);
            ensure(dst.hDS);
        }
        // Verify copy
        {
            GUInt64 const dst_size = get_file_size(dst_file.c_str());
            ensure(src_size > dst_size);
            Dataset dst = GDALOpen(dst_file.c_str(), GA_ReadOnly);
            ensure(dst.hDS);
            ensure_equals(GDALGetRasterXSize(dst.hDS), 256);
            ensure_equals(GDALGetRasterYSize(dst.hDS), 256);
            ensure_equals(GDALGetRasterCount(dst.hDS), 1);
            ensure_equals(GDALGetRasterDataType(GDALGetRasterBand(ds_.hDS, 1)), GDT_Byte);
        }
        GDALDeleteDataset(drv_, dst_file.c_str());
    }

} // namespace tut
