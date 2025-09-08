///////////////////////////////////////////////////////////////////////////////
//
// Project:  C++ Test Suite for GDAL/OGR
// Purpose:  Test gdal_minmax_element.hpp
// Author:   Even Rouault <even.rouault at spatialys.com>
//
///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2023, Even Rouault <even.rouault at spatialys.com>
/*
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdal_unit_test.h"

#include "gdal_minmax_element.hpp"

#include "gtest_include.h"

#include <limits>

namespace
{

struct test_gdal_minmax_element : public ::testing::Test
{
};

TEST_F(test_gdal_minmax_element, uint8)
{
    using T = uint8_t;
    constexpr GDALDataType eDT = GDT_Byte;
    T min_v = 3;
    T max_v = 7;
    {
        T nodata = 0;
        std::vector<T> v{max_v, nodata, min_v};
        {
            auto idx_min = gdal::min_element(v.data(), 0, eDT, true, nodata);
            EXPECT_EQ(idx_min, 0U);
        }
        {
            auto idx_min = gdal::min_element(v.data(), 0, eDT, false, 0);
            EXPECT_EQ(idx_min, 0U);
        }
        {
            auto idx_min =
                gdal::min_element(v.data(), v.size(), eDT, true, nodata);
            EXPECT_EQ(v[idx_min], min_v);
            auto idx_max =
                gdal::max_element(v.data(), v.size(), eDT, true, nodata);
            EXPECT_EQ(v[idx_max], max_v);
        }
        {
            auto [idx_min, idx_max] =
                gdal::minmax_element(v.data(), v.size(), eDT, true, nodata);
            EXPECT_EQ(v[idx_min], min_v);
            EXPECT_EQ(v[idx_max], max_v);
        }
    }
    {
        T nodata = 0;
        std::vector<T> v{nodata, max_v, min_v};
        auto idx_min = gdal::min_element(v.data(), v.size(), eDT, true, nodata);
        EXPECT_EQ(v[idx_min], min_v);
        auto idx_max = gdal::max_element(v.data(), v.size(), eDT, true, nodata);
        EXPECT_EQ(v[idx_max], max_v);
    }
    {
        std::vector<T> v{static_cast<T>((min_v + max_v) / 2),
                         static_cast<T>(max_v - 1),
                         max_v,
                         static_cast<T>(max_v - 1),
                         static_cast<T>(min_v + 1),
                         min_v,
                         static_cast<T>(min_v + 1)};
        auto idx_min = gdal::min_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_min], min_v);
        auto idx_max = gdal::max_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_max], max_v);
    }
    {
        std::vector<T> v(257, static_cast<T>((min_v + max_v) / 2));
        v[125] = static_cast<T>(min_v + 1);
        v[126] = min_v;
        v[127] = static_cast<T>(min_v + 1);
        v[128] = static_cast<T>(max_v - 1);
        v[129] = max_v;
        v[130] = static_cast<T>(max_v - 1);
        auto idx_min = gdal::min_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_min], min_v);
        auto idx_max = gdal::max_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_max], max_v);
    }
    {
        std::vector<T> v(257, static_cast<T>(min_v + 2));
        v[128] = static_cast<T>(min_v + 1);
        v[256] = min_v;
        auto idx_min = gdal::min_element(v.data(), v.size(), eDT, true,
                                         static_cast<T>(min_v + 1));
        EXPECT_EQ(v[idx_min], min_v);
    }
    {
        std::vector<T> v(257, 0);
        auto idx_min = gdal::min_element(v.data(), v.size(), eDT, true, 0);
        EXPECT_TRUE(idx_min == 0 || idx_min == 256) << idx_min;
    }
    {
        std::vector<T> v(257, 0);
        v[127] = static_cast<T>(min_v + 1);
        v[255] = min_v;
        auto idx_min = gdal::min_element(v.data(), v.size(), eDT, true, 0);
        EXPECT_EQ(v[idx_min], min_v);
    }
    {
        std::vector<T> v(259, static_cast<T>((min_v + max_v) / 2));
        v[0] = min_v;
        v[256] = static_cast<T>(max_v - 1);
        v[257] = max_v;
        v[258] = static_cast<T>(max_v - 1);
        auto idx_min = gdal::min_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_min], min_v);
        auto idx_max = gdal::max_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_max], max_v);
    }
    {
        std::vector<T> v(257, static_cast<T>((min_v + max_v) / 2));
        v[0] = min_v;
        v[127] = max_v;
        auto idx_min = gdal::min_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_min], min_v);
        auto idx_max = gdal::max_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_max], max_v);
    }
    {
        std::vector<T> v(257, static_cast<T>((min_v + max_v) / 2));
        v[127] = min_v;
        v[0] = max_v;
        auto idx_min = gdal::min_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_min], min_v);
        auto idx_max = gdal::max_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_max], max_v);
    }
    {
        std::vector<T> v(257, static_cast<T>((min_v + max_v) / 2));
        v[0] = min_v;
        v[129] = max_v;
        auto idx_min = gdal::min_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_min], min_v);
        auto idx_max = gdal::max_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_max], max_v);
    }
    {
        std::vector<T> v(257, static_cast<T>((min_v + max_v) / 2));
        v[129] = min_v;
        v[0] = max_v;
        auto idx_min = gdal::min_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_min], min_v);
        auto idx_max = gdal::max_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_max], max_v);
    }
    {
        std::vector<T> v(257, static_cast<T>((min_v + max_v) / 2));
        v[129] = min_v;
        v[256] = max_v;
        auto idx_min = gdal::min_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_min], min_v);
        auto idx_max = gdal::max_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_max], max_v);
    }
    {
        std::vector<T> v(257, static_cast<T>((min_v + max_v) / 2));
        v[256] = min_v;
        v[129] = max_v;
        auto idx_min = gdal::min_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_min], min_v);
        auto idx_max = gdal::max_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_max], max_v);
    }
    {
        std::vector<T> v(257, 0);
        v[65] = static_cast<T>(max_v - 2);
        v[66] = static_cast<T>(max_v - 1);
        v[129] = max_v;
        auto idx_max = gdal::max_element(v.data(), v.size(), eDT, true, 0);
        EXPECT_EQ(v[idx_max], max_v);
    }
}

TEST_F(test_gdal_minmax_element, int8)
{
    using T = int8_t;
    T min_v = -1;
    T max_v = 3;
    constexpr GDALDataType eDT = GDT_Int8;
    {
        T nodata = 0;
        std::vector<T> v{max_v, nodata, min_v};
        {
            auto idx_min = gdal::min_element(v.data(), 0, eDT, true, nodata);
            EXPECT_EQ(idx_min, 0U);
        }
        {
            auto idx_min = gdal::min_element(v.data(), 0, eDT, false, 0);
            EXPECT_EQ(idx_min, 0U);
        }
        {
            auto idx_min =
                gdal::min_element(v.data(), v.size(), eDT, true, nodata);
            EXPECT_EQ(v[idx_min], min_v);
            auto idx_max =
                gdal::max_element(v.data(), v.size(), eDT, true, nodata);
            EXPECT_EQ(v[idx_max], max_v);
        }
        {
            auto [idx_min, idx_max] =
                gdal::minmax_element(v.data(), v.size(), eDT, true, nodata);
            EXPECT_EQ(v[idx_min], min_v);
            EXPECT_EQ(v[idx_max], max_v);
        }
    }
    {
        T nodata = 0;
        std::vector<T> v{nodata, max_v, min_v};
        auto idx_min = gdal::min_element(v.data(), v.size(), eDT, true, nodata);
        EXPECT_EQ(v[idx_min], min_v);
        auto idx_max = gdal::max_element(v.data(), v.size(), eDT, true, nodata);
        EXPECT_EQ(v[idx_max], max_v);
    }
    {
        std::vector<T> v{static_cast<T>((min_v + max_v) / 2), max_v, min_v};
        auto idx_min = gdal::min_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_min], min_v);
        auto idx_max = gdal::max_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_max], max_v);
    }
    {
        std::vector<T> v(257, static_cast<T>((min_v + max_v) / 2));
        v[5] = min_v;
        v[31] = max_v;
        auto idx_min = gdal::min_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_min], min_v);
        auto idx_max = gdal::max_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_max], max_v);
    }
    {
        std::vector<T> v(257, static_cast<T>(min_v + 2));
        v[128] = static_cast<T>(min_v + 1);
        v[256] = min_v;
        auto idx_min = gdal::min_element(v.data(), v.size(), eDT, true,
                                         static_cast<T>(min_v + 1));
        EXPECT_EQ(v[idx_min], min_v);
    }
}

TEST_F(test_gdal_minmax_element, uint16)
{
    using T = uint16_t;
    constexpr GDALDataType eDT = GDT_UInt16;
    T min_v = 1000;
    T max_v = 2000;
    {
        T nodata = 0;
        std::vector<T> v{max_v, nodata, min_v};
        {
            auto idx_min = gdal::min_element(v.data(), 0, eDT, true, nodata);
            EXPECT_EQ(idx_min, 0U);
        }
        {
            auto idx_min = gdal::min_element(v.data(), 0, eDT, false, 0);
            EXPECT_EQ(idx_min, 0U);
        }
        {
            auto idx_min =
                gdal::min_element(v.data(), v.size(), eDT, true, nodata);
            EXPECT_EQ(v[idx_min], min_v);
            auto idx_max =
                gdal::max_element(v.data(), v.size(), eDT, true, nodata);
            EXPECT_EQ(v[idx_max], max_v);
        }
        {
            auto [idx_min, idx_max] =
                gdal::minmax_element(v.data(), v.size(), eDT, true, nodata);
            EXPECT_EQ(v[idx_min], min_v);
            EXPECT_EQ(v[idx_max], max_v);
        }
    }
    {
        T nodata = 0;
        std::vector<T> v{nodata, max_v, min_v};
        auto idx_min = gdal::min_element(v.data(), v.size(), eDT, true, nodata);
        EXPECT_EQ(v[idx_min], min_v);
        auto idx_max = gdal::max_element(v.data(), v.size(), eDT, true, nodata);
        EXPECT_EQ(v[idx_max], max_v);
    }
    {
        std::vector<T> v{static_cast<T>((min_v + max_v) / 2), max_v, min_v};
        auto idx_min = gdal::min_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_min], min_v);
        auto idx_max = gdal::max_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_max], max_v);
    }
    {
        std::vector<T> v(257, static_cast<T>((min_v + max_v) / 2));
        v[5] = min_v;
        v[31] = max_v;
        auto idx_min = gdal::min_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_min], min_v);
        auto idx_max = gdal::max_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_max], max_v);
    }
    {
        std::vector<T> v(257, static_cast<T>(min_v + 2));
        v[128] = static_cast<T>(min_v + 1);
        v[256] = min_v;
        auto idx_min = gdal::min_element(v.data(), v.size(), eDT, true,
                                         static_cast<T>(min_v + 1));
        EXPECT_EQ(v[idx_min], min_v);
    }
}

TEST_F(test_gdal_minmax_element, int16)
{
    using T = int16_t;
    constexpr GDALDataType eDT = GDT_Int16;
    T min_v = -1000;
    T max_v = 2000;
    {
        T nodata = 0;
        std::vector<T> v{max_v, nodata, min_v};
        {
            auto idx_min = gdal::min_element(v.data(), 0, eDT, true, nodata);
            EXPECT_EQ(idx_min, 0U);
        }
        {
            auto idx_min = gdal::min_element(v.data(), 0, eDT, false, 0);
            EXPECT_EQ(idx_min, 0U);
        }
        {
            auto idx_min =
                gdal::min_element(v.data(), v.size(), eDT, true, nodata);
            EXPECT_EQ(v[idx_min], min_v);
            auto idx_max =
                gdal::max_element(v.data(), v.size(), eDT, true, nodata);
            EXPECT_EQ(v[idx_max], max_v);
        }
        {
            auto [idx_min, idx_max] =
                gdal::minmax_element(v.data(), v.size(), eDT, true, nodata);
            EXPECT_EQ(v[idx_min], min_v);
            EXPECT_EQ(v[idx_max], max_v);
        }
    }
    {
        T nodata = 0;
        std::vector<T> v{nodata, max_v, min_v};
        auto idx_min = gdal::min_element(v.data(), v.size(), eDT, true, nodata);
        EXPECT_EQ(v[idx_min], min_v);
        auto idx_max = gdal::max_element(v.data(), v.size(), eDT, true, nodata);
        EXPECT_EQ(v[idx_max], max_v);
    }
    {
        std::vector<T> v{static_cast<T>((min_v + max_v) / 2), max_v, min_v};
        auto idx_min = gdal::min_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_min], min_v);
        auto idx_max = gdal::max_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_max], max_v);
    }
    {
        std::vector<T> v(257, static_cast<T>((min_v + max_v) / 2));
        v[5] = min_v;
        v[31] = max_v;
        auto idx_min = gdal::min_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_min], min_v);
        auto idx_max = gdal::max_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_max], max_v);
    }
    {
        std::vector<T> v(257, static_cast<T>(min_v + 2));
        v[128] = static_cast<T>(min_v + 1);
        v[256] = min_v;
        auto idx_min = gdal::min_element(v.data(), v.size(), eDT, true,
                                         static_cast<T>(min_v + 1));
        EXPECT_EQ(v[idx_min], min_v);
    }
}

TEST_F(test_gdal_minmax_element, uint32)
{
    using T = uint32_t;
    constexpr GDALDataType eDT = GDT_UInt32;
    T min_v = 10000000;
    T max_v = 20000000;
    {
        T nodata = 0;
        std::vector<T> v{max_v, nodata, min_v};
        {
            auto idx_min = gdal::min_element(v.data(), 0, eDT, true, nodata);
            EXPECT_EQ(idx_min, 0U);
        }
        {
            auto idx_min = gdal::min_element(v.data(), 0, eDT, false, 0);
            EXPECT_EQ(idx_min, 0U);
        }
        {
            auto idx_min =
                gdal::min_element(v.data(), v.size(), eDT, true, nodata);
            EXPECT_EQ(v[idx_min], min_v);
            auto idx_max =
                gdal::max_element(v.data(), v.size(), eDT, true, nodata);
            EXPECT_EQ(v[idx_max], max_v);
        }
        {
            auto [idx_min, idx_max] =
                gdal::minmax_element(v.data(), v.size(), eDT, true, nodata);
            EXPECT_EQ(v[idx_min], min_v);
            EXPECT_EQ(v[idx_max], max_v);
        }
    }
    {
        T nodata = 0;
        std::vector<T> v{nodata, max_v, min_v};
        auto idx_min = gdal::min_element(v.data(), v.size(), eDT, true, nodata);
        EXPECT_EQ(v[idx_min], min_v);
        auto idx_max = gdal::max_element(v.data(), v.size(), eDT, true, nodata);
        EXPECT_EQ(v[idx_max], max_v);
    }
    {
        std::vector<T> v{static_cast<T>((min_v + max_v) / 2), max_v, min_v};
        auto idx_min = gdal::min_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_min], min_v);
        auto idx_max = gdal::max_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_max], max_v);
    }
    {
        std::vector<T> v(257, static_cast<T>((min_v + max_v) / 2));
        v[5] = min_v;
        v[31] = max_v;
        auto idx_min = gdal::min_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_min], min_v);
        auto idx_max = gdal::max_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_max], max_v);
    }
    {
        std::vector<T> v(257, static_cast<T>(min_v + 2));
        v[128] = static_cast<T>(min_v + 1);
        v[256] = min_v;
        auto idx_min = gdal::min_element(v.data(), v.size(), eDT, true,
                                         static_cast<T>(min_v + 1));
        EXPECT_EQ(v[idx_min], min_v);
    }
}

TEST_F(test_gdal_minmax_element, int32)
{
    using T = int32_t;
    constexpr GDALDataType eDT = GDT_Int32;
    T min_v = -10000000;
    T max_v = 20000000;
    {
        T nodata = 0;
        std::vector<T> v{max_v, nodata, min_v};
        {
            auto idx_min = gdal::min_element(v.data(), 0, eDT, true, nodata);
            EXPECT_EQ(idx_min, 0U);
        }
        {
            auto idx_min = gdal::min_element(v.data(), 0, eDT, false, 0);
            EXPECT_EQ(idx_min, 0U);
        }
        {
            auto idx_min =
                gdal::min_element(v.data(), v.size(), eDT, true, nodata);
            EXPECT_EQ(v[idx_min], min_v);
            auto idx_max =
                gdal::max_element(v.data(), v.size(), eDT, true, nodata);
            EXPECT_EQ(v[idx_max], max_v);
        }
        {
            auto [idx_min, idx_max] =
                gdal::minmax_element(v.data(), v.size(), eDT, true, nodata);
            EXPECT_EQ(v[idx_min], min_v);
            EXPECT_EQ(v[idx_max], max_v);
        }
    }
    {
        T nodata = 0;
        std::vector<T> v{nodata, max_v, min_v};
        auto idx_min = gdal::min_element(v.data(), v.size(), eDT, true, nodata);
        EXPECT_EQ(v[idx_min], min_v);
        auto idx_max = gdal::max_element(v.data(), v.size(), eDT, true, nodata);
        EXPECT_EQ(v[idx_max], max_v);
    }
    {
        std::vector<T> v{static_cast<T>((min_v + max_v) / 2), max_v, min_v};
        auto idx_min = gdal::min_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_min], min_v);
        auto idx_max = gdal::max_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_max], max_v);
    }
    {
        std::vector<T> v(257, static_cast<T>((min_v + max_v) / 2));
        v[5] = min_v;
        v[31] = max_v;
        auto idx_min = gdal::min_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_min], min_v);
        auto idx_max = gdal::max_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_max], max_v);
    }
    {
        std::vector<T> v(257, static_cast<T>(min_v + 2));
        v[128] = static_cast<T>(min_v + 1);
        v[256] = min_v;
        auto idx_min = gdal::min_element(v.data(), v.size(), eDT, true,
                                         static_cast<T>(min_v + 1));
        EXPECT_EQ(v[idx_min], min_v);
    }
}

TEST_F(test_gdal_minmax_element, uint64)
{
    using T = uint64_t;
    constexpr GDALDataType eDT = GDT_UInt64;
    T min_v = 100000000000000;
    T max_v = 200000000000000;
    {
        double nodata = 0;
        std::vector<T> v{max_v, static_cast<T>(nodata), min_v};
        {
            auto idx_min = gdal::min_element(v.data(), 0, eDT, true, nodata);
            EXPECT_EQ(idx_min, 0U);
        }
        {
            auto idx_min = gdal::min_element(v.data(), 0, eDT, false, 0);
            EXPECT_EQ(idx_min, 0U);
        }
        {
            auto idx_min =
                gdal::min_element(v.data(), v.size(), eDT, true, nodata);
            EXPECT_EQ(v[idx_min], min_v);
            auto idx_max =
                gdal::max_element(v.data(), v.size(), eDT, true, nodata);
            EXPECT_EQ(v[idx_max], max_v);
        }
        {
            auto [idx_min, idx_max] =
                gdal::minmax_element(v.data(), v.size(), eDT, true, nodata);
            EXPECT_EQ(v[idx_min], min_v);
            EXPECT_EQ(v[idx_max], max_v);
        }
    }
    {
        double nodata = 0;
        std::vector<T> v{static_cast<T>(nodata), max_v, min_v};
        auto idx_min = gdal::min_element(v.data(), v.size(), eDT, true, nodata);
        EXPECT_EQ(v[idx_min], min_v);
        auto idx_max = gdal::max_element(v.data(), v.size(), eDT, true, nodata);
        EXPECT_EQ(v[idx_max], max_v);
    }
    {
        std::vector<T> v{static_cast<T>((min_v + max_v) / 2), max_v, min_v};
        auto idx_min = gdal::min_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_min], min_v);
        auto idx_max = gdal::max_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_max], max_v);
    }
    {
        std::vector<T> v(257, static_cast<T>((min_v + max_v) / 2));
        v[5] = min_v;
        v[31] = max_v;
        auto idx_min = gdal::min_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_min], min_v);
        auto idx_max = gdal::max_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_max], max_v);
    }
    {
        std::vector<T> v(257, static_cast<T>(min_v + 2));
        v[128] = static_cast<T>(min_v + 1);
        v[256] = min_v;
        auto idx_min =
            gdal::min_element(v.data(), v.size(), eDT, true,
                              static_cast<double>(static_cast<T>(min_v + 1)));
        EXPECT_EQ(v[idx_min], min_v);
    }
}

TEST_F(test_gdal_minmax_element, int64)
{
    using T = int64_t;
    constexpr GDALDataType eDT = GDT_Int64;
    T min_v = -100000000000000;
    T max_v = 200000000000000;
    {
        double nodata = 0;
        std::vector<T> v{max_v, static_cast<T>(nodata), min_v};
        {
            auto idx_min = gdal::min_element(v.data(), 0, eDT, true, nodata);
            EXPECT_EQ(idx_min, 0U);
        }
        {
            auto idx_min = gdal::min_element(v.data(), 0, eDT, false, 0);
            EXPECT_EQ(idx_min, 0U);
        }
        {
            auto idx_min =
                gdal::min_element(v.data(), v.size(), eDT, true, nodata);
            EXPECT_EQ(v[idx_min], min_v);
            auto idx_max =
                gdal::max_element(v.data(), v.size(), eDT, true, nodata);
            EXPECT_EQ(v[idx_max], max_v);
        }
        {
            auto [idx_min, idx_max] =
                gdal::minmax_element(v.data(), v.size(), eDT, true, nodata);
            EXPECT_EQ(v[idx_min], min_v);
            EXPECT_EQ(v[idx_max], max_v);
        }
    }
    {
        double nodata = 0;
        std::vector<T> v{static_cast<T>(nodata), max_v, min_v};
        auto idx_min = gdal::min_element(v.data(), v.size(), eDT, true, nodata);
        EXPECT_EQ(v[idx_min], min_v);
        auto idx_max = gdal::max_element(v.data(), v.size(), eDT, true, nodata);
        EXPECT_EQ(v[idx_max], max_v);
    }
    {
        std::vector<T> v{static_cast<T>((min_v + max_v) / 2),
                         max_v - 1,
                         max_v,
                         max_v - 1,
                         min_v + 1,
                         min_v,
                         min_v + 1};
        auto idx_min = gdal::min_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_min], min_v);
        auto idx_max = gdal::max_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_max], max_v);
    }
    {
        std::vector<T> v(257, static_cast<T>((min_v + max_v) / 2));
        v[5] = min_v;
        v[31] = max_v;
        auto idx_min = gdal::min_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_min], min_v);
        auto idx_max = gdal::max_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_max], max_v);
    }
    {
        std::vector<T> v(257, static_cast<T>(min_v + 2));
        v[128] = static_cast<T>(min_v + 1);
        v[256] = min_v;
        auto idx_min =
            gdal::min_element(v.data(), v.size(), eDT, true,
                              static_cast<double>(static_cast<T>(min_v + 1)));
        EXPECT_EQ(v[idx_min], min_v);
    }
}

TEST_F(test_gdal_minmax_element, float16)
{
    using T = GFloat16;
    constexpr GDALDataType eDT = GDT_Float16;
    T min_v = static_cast<T>(-10.0f);
    T max_v = static_cast<T>(1.5f);
    {
        T nodata = static_cast<T>(2.0f);
        std::vector<T> v{max_v, nodata, min_v};
        {
            auto idx_min = gdal::min_element(v.data(), 0, eDT, true, nodata);
            EXPECT_EQ(idx_min, 0U);
        }
        {
            auto idx_min = gdal::min_element(v.data(), 0, eDT, false, 0);
            EXPECT_EQ(idx_min, 0U);
        }
        {
            auto idx_min =
                gdal::min_element(v.data(), v.size(), eDT, true, nodata);
            EXPECT_EQ(v[idx_min], min_v);
            auto idx_max =
                gdal::max_element(v.data(), v.size(), eDT, true, nodata);
            EXPECT_EQ(v[idx_max], max_v);
        }
        {
            auto [idx_min, idx_max] =
                gdal::minmax_element(v.data(), v.size(), eDT, true, nodata);
            EXPECT_EQ(v[idx_min], min_v);
            EXPECT_EQ(v[idx_max], max_v);
        }
    }
    {
        T nodata = static_cast<T>(2.0f);
        std::vector<T> v{nodata, max_v, min_v};
        auto idx_min = gdal::min_element(v.data(), v.size(), eDT, true, nodata);
        EXPECT_EQ(v[idx_min], min_v);
        auto idx_max = gdal::max_element(v.data(), v.size(), eDT, true, nodata);
        EXPECT_EQ(v[idx_max], max_v);
    }
    {
        T nodata = static_cast<T>(2.0f);
        std::vector<T> v{cpl::NumericLimits<T>::quiet_NaN(),
                         cpl::NumericLimits<T>::quiet_NaN(), nodata, max_v,
                         min_v};
        auto idx_min = gdal::min_element(v.data(), v.size(), eDT, true, nodata);
        EXPECT_EQ(static_cast<float>(v[idx_min]), static_cast<float>(min_v));
        auto idx_max = gdal::max_element(v.data(), v.size(), eDT, true, nodata);
        EXPECT_EQ(v[idx_max], max_v);
    }
    {
        T nodata = cpl::NumericLimits<T>::quiet_NaN();
        std::vector<T> v{cpl::NumericLimits<T>::quiet_NaN(),
                         cpl::NumericLimits<T>::quiet_NaN(), nodata, max_v,
                         min_v};
        auto idx_min = gdal::min_element(v.data(), v.size(), eDT, true, nodata);
        EXPECT_EQ(v[idx_min], min_v);
        auto idx_max = gdal::max_element(v.data(), v.size(), eDT, true, nodata);
        EXPECT_EQ(v[idx_max], max_v);
    }
    {
        std::vector<T> v{cpl::NumericLimits<T>::quiet_NaN(),
                         cpl::NumericLimits<T>::quiet_NaN(),
                         max_v,
                         cpl::NumericLimits<T>::quiet_NaN(),
                         min_v,
                         cpl::NumericLimits<T>::quiet_NaN()};
        auto idx_min = gdal::min_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_min], min_v);
        auto idx_max = gdal::max_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_max], max_v);
    }
    {
        std::vector<T> v{max_v, cpl::NumericLimits<T>::quiet_NaN(), min_v};
        auto idx_min = gdal::min_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_min], min_v);
        auto idx_max = gdal::max_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_max], max_v);
    }
    {
        std::vector<T> v(257, cpl::NumericLimits<T>::quiet_NaN());
        v[125] = static_cast<T>(min_v + 0.1f);
        v[126] = min_v;
        v[127] = static_cast<T>(min_v + 0.1f);
        v[128] = static_cast<T>(max_v - 0.1f);
        v[129] = max_v;
        v[130] = static_cast<T>(max_v - 0.1f);
        auto idx_min = gdal::min_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_min], min_v);
        auto idx_max = gdal::max_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_max], max_v);
    }
    {
        std::vector<T> v(33, static_cast<T>(1.2f));
        v[5] = min_v;
        v[15] = max_v;
        auto idx_min = gdal::min_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_min], min_v);
        auto idx_max = gdal::max_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_max], max_v);
    }
    {
        std::vector<T> v(255, cpl::NumericLimits<T>::quiet_NaN());
        v[v.size() - 2] = min_v;
        v.back() = max_v;
        auto idx_min = gdal::min_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_min], min_v);
        auto idx_max = gdal::max_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_max], max_v);
    }
    {
        std::vector<T> v(257, static_cast<T>(min_v + 0.2f));
        v[128] = static_cast<T>(min_v + 0.1f);
        v[256] = min_v;
        auto idx_min = gdal::min_element(v.data(), v.size(), eDT, true,
                                         static_cast<T>(min_v + 0.1f));
        EXPECT_EQ(v[idx_min], min_v);
    }
    {
        std::vector<T> v(257, static_cast<T>(max_v - 0.2f));
        v[128] = static_cast<T>(max_v - 0.1f);
        v[256] = max_v;
        auto idx_max = gdal::max_element(v.data(), v.size(), eDT, true,
                                         static_cast<T>(max_v - 0.1f));
        EXPECT_EQ(v[idx_max], max_v);
    }
}

TEST_F(test_gdal_minmax_element, float32)
{
    using T = float;
    constexpr GDALDataType eDT = GDT_Float32;
    T min_v = 1.0f;
    T max_v = 1.5f;
    {
        T nodata = 2.0f;
        std::vector<T> v{max_v, nodata, min_v};
        {
            auto idx_min = gdal::min_element(v.data(), 0, eDT, true, nodata);
            EXPECT_EQ(idx_min, 0U);
        }
        {
            auto idx_min = gdal::min_element(v.data(), 0, eDT, false, 0);
            EXPECT_EQ(idx_min, 0U);
        }
        {
            auto idx_min =
                gdal::min_element(v.data(), v.size(), eDT, true, nodata);
            EXPECT_EQ(v[idx_min], min_v);
            auto idx_max =
                gdal::max_element(v.data(), v.size(), eDT, true, nodata);
            EXPECT_EQ(v[idx_max], max_v);
        }
        {
            auto [idx_min, idx_max] =
                gdal::minmax_element(v.data(), v.size(), eDT, true, nodata);
            EXPECT_EQ(v[idx_min], min_v);
            EXPECT_EQ(v[idx_max], max_v);
        }
    }
    {
        T nodata = 2.0f;
        std::vector<T> v{nodata, max_v, min_v};
        auto idx_min = gdal::min_element(v.data(), v.size(), eDT, true, nodata);
        EXPECT_EQ(v[idx_min], min_v);
        auto idx_max = gdal::max_element(v.data(), v.size(), eDT, true, nodata);
        EXPECT_EQ(v[idx_max], max_v);
    }
    {
        T nodata = 2.0f;
        std::vector<T> v{cpl::NumericLimits<T>::quiet_NaN(),
                         cpl::NumericLimits<T>::quiet_NaN(), nodata, max_v,
                         min_v};
        auto idx_min = gdal::min_element(v.data(), v.size(), eDT, true, nodata);
        EXPECT_EQ(v[idx_min], min_v);
        auto idx_max = gdal::max_element(v.data(), v.size(), eDT, true, nodata);
        EXPECT_EQ(v[idx_max], max_v);
    }
    {
        T nodata = cpl::NumericLimits<T>::quiet_NaN();
        std::vector<T> v{cpl::NumericLimits<T>::quiet_NaN(),
                         cpl::NumericLimits<T>::quiet_NaN(), nodata, max_v,
                         min_v};
        auto idx_min = gdal::min_element(v.data(), v.size(), eDT, true, nodata);
        EXPECT_EQ(v[idx_min], min_v);
        auto idx_max = gdal::max_element(v.data(), v.size(), eDT, true, nodata);
        EXPECT_EQ(v[idx_max], max_v);
    }
    {
        std::vector<T> v{cpl::NumericLimits<T>::quiet_NaN(),
                         cpl::NumericLimits<T>::quiet_NaN(),
                         max_v,
                         cpl::NumericLimits<T>::quiet_NaN(),
                         min_v,
                         cpl::NumericLimits<T>::quiet_NaN()};
        auto idx_min = gdal::min_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_min], min_v);
        auto idx_max = gdal::max_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_max], max_v);
    }
    {
        std::vector<T> v{max_v, cpl::NumericLimits<T>::quiet_NaN(), min_v};
        auto idx_min = gdal::min_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_min], min_v);
        auto idx_max = gdal::max_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_max], max_v);
    }
    {
        std::vector<T> v(257, cpl::NumericLimits<T>::quiet_NaN());
        v[125] = static_cast<T>(min_v + 0.1f);
        v[126] = min_v;
        v[127] = static_cast<T>(min_v + 0.1f);
        v[128] = static_cast<T>(max_v - 0.1f);
        v[129] = max_v;
        v[130] = static_cast<T>(max_v - 0.1f);
        auto idx_min = gdal::min_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_min], min_v);
        auto idx_max = gdal::max_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_max], max_v);
    }
    {
        std::vector<T> v(33, 1.2f);
        v[5] = min_v;
        v[15] = max_v;
        auto idx_min = gdal::min_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_min], min_v);
        auto idx_max = gdal::max_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_max], max_v);
    }
    {
        std::vector<T> v(255, cpl::NumericLimits<T>::quiet_NaN());
        v[v.size() - 2] = min_v;
        v.back() = max_v;
        auto idx_min = gdal::min_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_min], min_v);
        auto idx_max = gdal::max_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_max], max_v);
    }
    {
        std::vector<T> v(257, static_cast<T>(min_v + 0.2f));
        v[128] = static_cast<T>(min_v + 0.1f);
        v[256] = min_v;
        auto idx_min = gdal::min_element(v.data(), v.size(), eDT, true,
                                         static_cast<T>(min_v + 0.1f));
        EXPECT_EQ(v[idx_min], min_v);
    }
}

TEST_F(test_gdal_minmax_element, float64)
{
    using T = double;
    constexpr GDALDataType eDT = GDT_Float64;
    T min_v = 1.0;
    T max_v = 1.5;
    {
        T nodata = 2.0;
        std::vector<T> v{max_v, nodata, min_v};
        {
            auto idx_min = gdal::min_element(v.data(), 0, eDT, true, nodata);
            EXPECT_EQ(idx_min, 0U);
        }
        {
            auto idx_min = gdal::min_element(v.data(), 0, eDT, false, 0);
            EXPECT_EQ(idx_min, 0U);
        }
        {
            auto idx_min =
                gdal::min_element(v.data(), v.size(), eDT, true, nodata);
            EXPECT_EQ(v[idx_min], min_v);
            auto idx_max =
                gdal::max_element(v.data(), v.size(), eDT, true, nodata);
            EXPECT_EQ(v[idx_max], max_v);
        }
        {
            auto [idx_min, idx_max] =
                gdal::minmax_element(v.data(), v.size(), eDT, true, nodata);
            EXPECT_EQ(v[idx_min], min_v);
            EXPECT_EQ(v[idx_max], max_v);
        }
    }
    {
        T nodata = 2.0;
        std::vector<T> v{nodata, max_v, min_v};
        auto idx_min = gdal::min_element(v.data(), v.size(), eDT, true, nodata);
        EXPECT_EQ(v[idx_min], min_v);
        auto idx_max = gdal::max_element(v.data(), v.size(), eDT, true, nodata);
        EXPECT_EQ(v[idx_max], max_v);
    }
    {
        T nodata = 2.0;
        std::vector<T> v{cpl::NumericLimits<T>::quiet_NaN(),
                         cpl::NumericLimits<T>::quiet_NaN(), nodata, max_v,
                         min_v};
        auto idx_min = gdal::min_element(v.data(), v.size(), eDT, true, nodata);
        EXPECT_EQ(v[idx_min], min_v);
        auto idx_max = gdal::max_element(v.data(), v.size(), eDT, true, nodata);
        EXPECT_EQ(v[idx_max], max_v);
    }
    {
        std::vector<T> v{max_v, cpl::NumericLimits<T>::quiet_NaN(), min_v};
        auto idx_min = gdal::min_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_min], min_v);
        auto idx_max = gdal::max_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_max], max_v);
    }
    {
        std::vector<T> v{cpl::NumericLimits<T>::quiet_NaN(),
                         cpl::NumericLimits<T>::quiet_NaN(),
                         max_v,
                         cpl::NumericLimits<T>::quiet_NaN(),
                         min_v,
                         cpl::NumericLimits<T>::quiet_NaN()};
        auto idx_min = gdal::min_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_min], min_v);
        auto idx_max = gdal::max_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_max], max_v);
    }
    {
        std::vector<T> v(33, cpl::NumericLimits<T>::quiet_NaN());
        v[5] = min_v;
        v[15] = max_v;
        auto idx_min = gdal::min_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_min], min_v);
        auto idx_max = gdal::max_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_max], max_v);
    }
    {
        std::vector<T> v(255, cpl::NumericLimits<T>::quiet_NaN());
        v[v.size() - 2] = min_v;
        v.back() = max_v;
        auto idx_min = gdal::min_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_min], min_v);
        auto idx_max = gdal::max_element(v.data(), v.size(), eDT, false, 0);
        EXPECT_EQ(v[idx_max], max_v);
    }
    {
        std::vector<T> v(257, static_cast<T>(min_v + 0.2));
        v[128] = static_cast<T>(min_v + 0.1);
        v[256] = min_v;
        auto idx_min = gdal::min_element(v.data(), v.size(), eDT, true,
                                         static_cast<T>(min_v + 0.1));
        EXPECT_EQ(v[idx_min], min_v);
    }
}

TEST_F(test_gdal_minmax_element, unsupported)
{
    float v[2] = {0, 0};
    CPLErrorHandlerPusher oErrorHandler(CPLQuietErrorHandler);
    {
        CPLErrorReset();
        EXPECT_EQ(gdal::min_element(v, 1, GDT_CFloat32, false, 0), 0);
        EXPECT_EQ(CPLGetLastErrorNo(), CPLE_NotSupported);
    }
    {
        CPLErrorReset();
        EXPECT_EQ(gdal::max_element(v, 1, GDT_CFloat32, false, 0), 0);
        EXPECT_EQ(CPLGetLastErrorNo(), CPLE_NotSupported);
    }
    {
        CPLErrorReset();
        auto [idx_min, idx_max] =
            gdal::minmax_element(v, 1, GDT_CFloat32, false, 0);
        EXPECT_EQ(idx_min, 0U);
        EXPECT_EQ(idx_max, 0);
        EXPECT_EQ(CPLGetLastErrorNo(), CPLE_NotSupported);
    }
}

}  // namespace
