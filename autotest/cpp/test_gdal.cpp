///////////////////////////////////////////////////////////////////////////////
//
// Project:  C++ Test Suite for GDAL/OGR
// Purpose:  Test general GDAL features.
// Author:   Mateusz Loskot <mateusz@loskot.net>
//
///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2006, Mateusz Loskot <mateusz@loskot.net>
/*
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdal_unit_test.h"

#include "gdal_alg.h"
#include "gdal_priv.h"
#include "gdal_utils.h"
#include "gdal_priv_templates.hpp"
#include "gdal.h"
#include "tilematrixset.hpp"
#include "gdalcachedpixelaccessor.h"

#include <limits>
#include <string>

#include "test_data.h"

#include "gtest_include.h"

namespace
{
// Common fixture with test data
struct test_gdal : public ::testing::Test
{
};

// Test GDAL driver manager access
TEST_F(test_gdal, driver_manager)
{
    GDALDriverManager *drv_mgr = nullptr;
    drv_mgr = GetGDALDriverManager();
    ASSERT_TRUE(nullptr != drv_mgr);
}

// Test that GDALRegisterPlugins can be called
TEST_F(test_gdal, register_plugins)
{
    GDALRegisterPlugins();
}

// Test that GDALRegisterPlugin can be called and returns an error for a non
// existing plugin name
TEST_F(test_gdal, register_plugin)
{
    ASSERT_EQ(GDALRegisterPlugin("rtbreg_non_existing_plugin"), CE_Failure);
}

// Test number of registered GDAL drivers
TEST_F(test_gdal, number_of_registered_drivers)
{
#ifdef WIN32CE
    ASSERT_EQ(GDALGetDriverCount(), drv_count_);
#endif
}

// Test if AAIGrid driver is registered
TEST_F(test_gdal, aaigrid_is_registered)
{
    GDALDriverH drv = GDALGetDriverByName("AAIGrid");

#ifdef FRMT_aaigrid
    ASSERT_TRUE(NULL != drv);
#else
    (void)drv;
#endif
}

// Test if DTED driver is registered
TEST_F(test_gdal, dted_is_registered)
{
    GDALDriverH drv = GDALGetDriverByName("DTED");

#ifdef FRMT_dted
    ASSERT_TRUE(NULL != drv);
#else
    (void)drv;
#endif
}

// Test if GeoTIFF driver is registered
TEST_F(test_gdal, gtiff_is_registered)
{
    GDALDriverH drv = GDALGetDriverByName("GTiff");

#ifdef FRMT_gtiff
    ASSERT_TRUE(NULL != drv);
#else
    (void)drv;
#endif
}

class DataTypeTupleFixture : public test_gdal,
                             public ::testing::WithParamInterface<
                                 std::tuple<GDALDataType, GDALDataType>>
{
  public:
    static std::vector<std::tuple<GDALDataType, GDALDataType>> GetTupleValues()
    {
        std::vector<std::tuple<GDALDataType, GDALDataType>> ret;
        for (GDALDataType eIn = GDT_Byte; eIn < GDT_TypeCount;
             eIn = static_cast<GDALDataType>(eIn + 1))
        {
            for (GDALDataType eOut = GDT_Byte; eOut < GDT_TypeCount;
                 eOut = static_cast<GDALDataType>(eOut + 1))
            {
                ret.emplace_back(std::make_tuple(eIn, eOut));
            }
        }
        return ret;
    }
};

// Test GDALDataTypeUnion() on all (GDALDataType, GDALDataType) combinations
TEST_P(DataTypeTupleFixture, GDALDataTypeUnion_generic)
{
    GDALDataType eDT1 = std::get<0>(GetParam());
    GDALDataType eDT2 = std::get<1>(GetParam());
    GDALDataType eDT = GDALDataTypeUnion(eDT1, eDT2);
    EXPECT_EQ(eDT, GDALDataTypeUnion(eDT2, eDT1));
    EXPECT_GE(GDALGetDataTypeSize(eDT), GDALGetDataTypeSize(eDT1));
    EXPECT_GE(GDALGetDataTypeSize(eDT), GDALGetDataTypeSize(eDT2));
    EXPECT_TRUE((GDALDataTypeIsComplex(eDT) && (GDALDataTypeIsComplex(eDT1) ||
                                                GDALDataTypeIsComplex(eDT2))) ||
                (!GDALDataTypeIsComplex(eDT) && !GDALDataTypeIsComplex(eDT1) &&
                 !GDALDataTypeIsComplex(eDT2)));

    EXPECT_TRUE(
        !(GDALDataTypeIsFloating(eDT1) || GDALDataTypeIsFloating(eDT2)) ||
        GDALDataTypeIsFloating(eDT));
    EXPECT_TRUE(!(GDALDataTypeIsSigned(eDT1) || GDALDataTypeIsSigned(eDT2)) ||
                GDALDataTypeIsSigned(eDT));
}

INSTANTIATE_TEST_SUITE_P(
    test_gdal, DataTypeTupleFixture,
    ::testing::ValuesIn(DataTypeTupleFixture::GetTupleValues()),
    [](const ::testing::TestParamInfo<DataTypeTupleFixture::ParamType> &l_info)
    {
        GDALDataType eDT1 = std::get<0>(l_info.param);
        GDALDataType eDT2 = std::get<1>(l_info.param);
        return std::string(GDALGetDataTypeName(eDT1)) + "_" +
               GDALGetDataTypeName(eDT2);
    });

// Test GDALDataTypeUnion()
TEST_F(test_gdal, GDALDataTypeUnion_special_cases)
{
    EXPECT_EQ(GDALDataTypeUnion(GDT_Int16, GDT_UInt16), GDT_Int32);
    EXPECT_EQ(GDALDataTypeUnion(GDT_Int16, GDT_UInt32), GDT_Int64);
    EXPECT_EQ(GDALDataTypeUnion(GDT_UInt32, GDT_Int16), GDT_Int64);
    EXPECT_EQ(GDALDataTypeUnion(GDT_Int64, GDT_UInt64), GDT_Float64);
    EXPECT_EQ(GDALDataTypeUnion(GDT_Int64, GDT_Float32), GDT_Float64);
    EXPECT_EQ(GDALDataTypeUnion(GDT_Int64, GDT_Float64), GDT_Float64);
    EXPECT_EQ(GDALDataTypeUnion(GDT_UInt64, GDT_Float32), GDT_Float64);
    EXPECT_EQ(GDALDataTypeUnion(GDT_UInt64, GDT_Float64), GDT_Float64);
    EXPECT_EQ(GDALDataTypeUnion(GDT_UInt32, GDT_CInt16), GDT_CFloat64);
    EXPECT_EQ(GDALDataTypeUnion(GDT_Float32, GDT_CInt32), GDT_CFloat64);
    EXPECT_EQ(GDALDataTypeUnion(GDT_CInt16, GDT_UInt32), GDT_CFloat64);
    EXPECT_EQ(GDALDataTypeUnion(GDT_CInt16, GDT_CFloat32), GDT_CFloat32);
    EXPECT_EQ(GDALDataTypeUnion(GDT_CInt32, GDT_Byte), GDT_CInt32);
    EXPECT_EQ(GDALDataTypeUnion(GDT_CInt32, GDT_UInt16), GDT_CInt32);
    EXPECT_EQ(GDALDataTypeUnion(GDT_CInt32, GDT_Int16), GDT_CInt32);
    EXPECT_EQ(GDALDataTypeUnion(GDT_CInt32, GDT_UInt32), GDT_CFloat64);
    EXPECT_EQ(GDALDataTypeUnion(GDT_CInt32, GDT_Int32), GDT_CInt32);
    EXPECT_EQ(GDALDataTypeUnion(GDT_CInt32, GDT_Float32), GDT_CFloat64);
    EXPECT_EQ(GDALDataTypeUnion(GDT_CInt32, GDT_CInt16), GDT_CInt32);
    EXPECT_EQ(GDALDataTypeUnion(GDT_CInt32, GDT_CFloat32), GDT_CFloat64);
    EXPECT_EQ(GDALDataTypeUnion(GDT_CFloat32, GDT_Byte), GDT_CFloat32);
    EXPECT_EQ(GDALDataTypeUnion(GDT_CFloat32, GDT_UInt16), GDT_CFloat32);
    EXPECT_EQ(GDALDataTypeUnion(GDT_CFloat32, GDT_Int16), GDT_CFloat32);
    EXPECT_EQ(GDALDataTypeUnion(GDT_CFloat32, GDT_UInt32), GDT_CFloat64);
    EXPECT_EQ(GDALDataTypeUnion(GDT_CFloat32, GDT_Int32), GDT_CFloat64);
    EXPECT_EQ(GDALDataTypeUnion(GDT_CFloat32, GDT_Float32), GDT_CFloat32);
    EXPECT_EQ(GDALDataTypeUnion(GDT_CFloat32, GDT_CInt16), GDT_CFloat32);
    EXPECT_EQ(GDALDataTypeUnion(GDT_CFloat32, GDT_CInt32), GDT_CFloat64);

    EXPECT_EQ(GDALFindDataType(0, false /* signed */, false /* floating */,
                               false /* complex */),
              GDT_Byte);
    EXPECT_EQ(GDALFindDataType(0, true /* signed */, false /* floating */,
                               false /* complex */),
              GDT_Int8);
    EXPECT_EQ(GDALFindDataType(0, false /* signed */, false /* floating */,
                               true /* complex */),
              GDT_CInt32);
    EXPECT_EQ(GDALFindDataType(0, true /* signed */, false /* floating */,
                               true /* complex */),
              GDT_CInt16);
    EXPECT_EQ(GDALFindDataType(0, false /* signed */, true /* floating */,
                               false /* complex */),
              GDT_Float32);
    EXPECT_EQ(GDALFindDataType(0, true /* signed */, true /* floating */,
                               false /* complex */),
              GDT_Float32);
    EXPECT_EQ(GDALFindDataType(0, false /* signed */, true /* floating */,
                               true /* complex */),
              GDT_CFloat32);
    EXPECT_EQ(GDALFindDataType(0, true /* signed */, true /* floating */,
                               true /* complex */),
              GDT_CFloat32);

    EXPECT_EQ(GDALFindDataType(8, false /* signed */, false /* floating */,
                               false /* complex */),
              GDT_Byte);
    EXPECT_EQ(GDALFindDataType(8, true /* signed */, false /* floating */,
                               false /* complex */),
              GDT_Int8);

    EXPECT_EQ(GDALFindDataType(16, false /* signed */, false /* floating */,
                               false /* complex */),
              GDT_UInt16);
    EXPECT_EQ(GDALFindDataType(16, true /* signed */, false /* floating */,
                               false /* complex */),
              GDT_Int16);

    EXPECT_EQ(GDALFindDataType(32, false /* signed */, false /* floating */,
                               false /* complex */),
              GDT_UInt32);
    EXPECT_EQ(GDALFindDataType(32, true /* signed */, false /* floating */,
                               false /* complex */),
              GDT_Int32);

    EXPECT_EQ(GDALFindDataType(64, false /* signed */, true /* floating */,
                               false /* complex */),
              GDT_Float64);
    EXPECT_EQ(GDALFindDataType(64, false /* signed */, true /* floating */,
                               true /* complex */),
              GDT_CFloat64);

    EXPECT_EQ(GDALFindDataType(64, false /* signed */, false /* floating */,
                               false /* complex */),
              GDT_UInt64);
    EXPECT_EQ(GDALFindDataType(64, true /* signed */, false /* floating */,
                               false /* complex */),
              GDT_Int64);

    EXPECT_EQ(GDALDataTypeUnionWithValue(GDT_Byte, -128, false), GDT_Int16);
    EXPECT_EQ(GDALDataTypeUnionWithValue(GDT_Byte, -32768, false), GDT_Int16);
    EXPECT_EQ(GDALDataTypeUnionWithValue(GDT_Byte, -32769, false), GDT_Int32);

    EXPECT_EQ(GDALDataTypeUnionWithValue(GDT_Int8, 127, false), GDT_Int8);
    EXPECT_EQ(GDALDataTypeUnionWithValue(GDT_Int8, 128, false), GDT_Int16);

    EXPECT_EQ(GDALDataTypeUnionWithValue(GDT_Int16, 32767, false), GDT_Int16);
    EXPECT_EQ(GDALDataTypeUnionWithValue(GDT_Int16, 32768, false), GDT_Int32);

    EXPECT_EQ(GDALDataTypeUnionWithValue(GDT_UInt16, 65535, false), GDT_UInt16);
    EXPECT_EQ(GDALDataTypeUnionWithValue(GDT_UInt16, 65536, false), GDT_UInt32);

    EXPECT_EQ(GDALDataTypeUnionWithValue(GDT_Int32, INT32_MAX, false),
              GDT_Int32);
    EXPECT_EQ(GDALDataTypeUnionWithValue(GDT_Int32, INT32_MAX + 1.0, false),
              GDT_Int64);

    EXPECT_EQ(GDALDataTypeUnionWithValue(GDT_UInt32, UINT32_MAX, false),
              GDT_UInt32);
    EXPECT_EQ(GDALDataTypeUnionWithValue(GDT_UInt32, UINT32_MAX + 1.0, false),
              GDT_UInt64);

    // (1 << 63) - 1024
    EXPECT_EQ(
        GDALDataTypeUnionWithValue(GDT_Int64, 9223372036854774784.0, false),
        GDT_Int64);
    // (1 << 63) - 512
    EXPECT_EQ(
        GDALDataTypeUnionWithValue(GDT_Int64, 9223372036854775296.0, false),
        GDT_Float64);

    // (1 << 64) - 2048
    EXPECT_EQ(
        GDALDataTypeUnionWithValue(GDT_UInt64, 18446744073709549568.0, false),
        GDT_UInt64);
    // (1 << 64) + 4096
    EXPECT_EQ(
        GDALDataTypeUnionWithValue(GDT_UInt64, 18446744073709555712.0, false),
        GDT_Float64);

    EXPECT_EQ(GDALDataTypeUnionWithValue(GDT_Float32, -99999, false),
              GDT_Float32);
    EXPECT_EQ(GDALDataTypeUnionWithValue(GDT_Float32, -99999.9876, false),
              GDT_Float64);
    EXPECT_EQ(GDALDataTypeUnionWithValue(
                  GDT_Float32, std::numeric_limits<double>::quiet_NaN(), false),
              GDT_Float32);
    EXPECT_EQ(GDALDataTypeUnionWithValue(
                  GDT_Float32, -std::numeric_limits<double>::infinity(), false),
              GDT_Float32);
    EXPECT_EQ(GDALDataTypeUnionWithValue(
                  GDT_Float32, -std::numeric_limits<double>::infinity(), false),
              GDT_Float32);

    EXPECT_EQ(GDALDataTypeUnionWithValue(GDT_Float64, -99999.9876, false),
              GDT_Float64);
    EXPECT_EQ(GDALDataTypeUnionWithValue(
                  GDT_Float64, std::numeric_limits<double>::quiet_NaN(), false),
              GDT_Float64);
    EXPECT_EQ(GDALDataTypeUnionWithValue(
                  GDT_Float64, -std::numeric_limits<double>::infinity(), false),
              GDT_Float64);
    EXPECT_EQ(GDALDataTypeUnionWithValue(
                  GDT_Float64, -std::numeric_limits<double>::infinity(), false),
              GDT_Float64);

    EXPECT_EQ(GDALDataTypeUnionWithValue(GDT_Unknown, 0, false), GDT_Byte);
}

// Test GDALAdjustValueToDataType()
TEST_F(test_gdal, GDALAdjustValueToDataType)
{
    int bClamped, bRounded;

    EXPECT_TRUE(GDALAdjustValueToDataType(GDT_Byte, 255.0, nullptr, nullptr) ==
                255.0);
    EXPECT_TRUE(GDALAdjustValueToDataType(GDT_Byte, 255.0, &bClamped,
                                          &bRounded) == 255.0 &&
                !bClamped && !bRounded);
    EXPECT_TRUE(GDALAdjustValueToDataType(GDT_Byte, 254.4, &bClamped,
                                          &bRounded) == 254.0 &&
                !bClamped && bRounded);
    EXPECT_TRUE(GDALAdjustValueToDataType(GDT_Byte, -1, &bClamped, &bRounded) ==
                    0.0 &&
                bClamped && !bRounded);
    EXPECT_TRUE(GDALAdjustValueToDataType(GDT_Byte, 256.0, &bClamped,
                                          &bRounded) == 255.0 &&
                bClamped && !bRounded);

    EXPECT_TRUE(GDALAdjustValueToDataType(GDT_Int8, -128.0, &bClamped,
                                          &bRounded) == -128.0 &&
                !bClamped && !bRounded);
    EXPECT_TRUE(GDALAdjustValueToDataType(GDT_Int8, 127.0, &bClamped,
                                          &bRounded) == 127.0 &&
                !bClamped && !bRounded);
    EXPECT_TRUE(GDALAdjustValueToDataType(GDT_Int8, -127.4, &bClamped,
                                          &bRounded) == -127.0 &&
                !bClamped && bRounded);
    EXPECT_TRUE(GDALAdjustValueToDataType(GDT_Int8, 126.4, &bClamped,
                                          &bRounded) == 126.0 &&
                !bClamped && bRounded);
    EXPECT_TRUE(GDALAdjustValueToDataType(GDT_Int8, -129.0, &bClamped,
                                          &bRounded) == -128.0 &&
                bClamped && !bRounded);
    EXPECT_TRUE(GDALAdjustValueToDataType(GDT_Int8, 128.0, &bClamped,
                                          &bRounded) == 127.0 &&
                bClamped && !bRounded);

    EXPECT_TRUE(GDALAdjustValueToDataType(GDT_UInt16, 65535.0, &bClamped,
                                          &bRounded) == 65535.0 &&
                !bClamped && !bRounded);
    EXPECT_TRUE(GDALAdjustValueToDataType(GDT_UInt16, 65534.4, &bClamped,
                                          &bRounded) == 65534.0 &&
                !bClamped && bRounded);
    EXPECT_TRUE(GDALAdjustValueToDataType(GDT_UInt16, -1, &bClamped,
                                          &bRounded) == 0.0 &&
                bClamped && !bRounded);
    EXPECT_TRUE(GDALAdjustValueToDataType(GDT_UInt16, 65536.0, &bClamped,
                                          &bRounded) == 65535.0 &&
                bClamped && !bRounded);

    EXPECT_TRUE(GDALAdjustValueToDataType(GDT_Int16, -32768.0, &bClamped,
                                          &bRounded) == -32768.0 &&
                !bClamped && !bRounded);
    EXPECT_TRUE(GDALAdjustValueToDataType(GDT_Int16, 32767.0, &bClamped,
                                          &bRounded) == 32767.0 &&
                !bClamped && !bRounded);
    EXPECT_TRUE(GDALAdjustValueToDataType(GDT_Int16, -32767.4, &bClamped,
                                          &bRounded) == -32767.0 &&
                !bClamped && bRounded);
    EXPECT_TRUE(GDALAdjustValueToDataType(GDT_Int16, 32766.4, &bClamped,
                                          &bRounded) == 32766.0 &&
                !bClamped && bRounded);
    EXPECT_TRUE(GDALAdjustValueToDataType(GDT_Int16, -32769.0, &bClamped,
                                          &bRounded) == -32768.0 &&
                bClamped && !bRounded);
    EXPECT_TRUE(GDALAdjustValueToDataType(GDT_Int16, 32768.0, &bClamped,
                                          &bRounded) == 32767.0 &&
                bClamped && !bRounded);

    EXPECT_TRUE(GDALAdjustValueToDataType(GDT_UInt32, 10000000.0, &bClamped,
                                          &bRounded) == 10000000.0 &&
                !bClamped && !bRounded);
    EXPECT_TRUE(GDALAdjustValueToDataType(GDT_UInt32, 10000000.4, &bClamped,
                                          &bRounded) == 10000000.0 &&
                !bClamped && bRounded);
    EXPECT_TRUE(GDALAdjustValueToDataType(GDT_UInt32, -1, &bClamped,
                                          &bRounded) == 0.0 &&
                bClamped && !bRounded);

    EXPECT_TRUE(GDALAdjustValueToDataType(GDT_Int32, -10000000.0, &bClamped,
                                          &bRounded) == -10000000.0 &&
                !bClamped && !bRounded);
    EXPECT_TRUE(GDALAdjustValueToDataType(GDT_Int32, 10000000.0, &bClamped,
                                          &bRounded) == 10000000.0 &&
                !bClamped && !bRounded);

    EXPECT_TRUE(GDALAdjustValueToDataType(GDT_UInt64, 10000000000.0, &bClamped,
                                          &bRounded) == 10000000000.0 &&
                !bClamped && !bRounded);
    EXPECT_TRUE(GDALAdjustValueToDataType(GDT_UInt64, 10000000000.4, &bClamped,
                                          &bRounded) == 10000000000.0 &&
                !bClamped && bRounded);
    EXPECT_TRUE(GDALAdjustValueToDataType(GDT_UInt64, -1, &bClamped,
                                          &bRounded) == 0.0 &&
                bClamped && !bRounded);

    EXPECT_TRUE(GDALAdjustValueToDataType(GDT_Int64, -10000000000.0, &bClamped,
                                          &bRounded) == -10000000000.0 &&
                !bClamped && !bRounded);
    EXPECT_TRUE(GDALAdjustValueToDataType(GDT_Int64, 10000000000.0, &bClamped,
                                          &bRounded) == 10000000000.0 &&
                !bClamped && !bRounded);

    EXPECT_TRUE(GDALAdjustValueToDataType(GDT_Float32, 0.0, &bClamped,
                                          &bRounded) == 0.0 &&
                !bClamped && !bRounded);
    EXPECT_TRUE(GDALAdjustValueToDataType(GDT_Float32, 1e-50, &bClamped,
                                          &bRounded) == 0.0 &&
                !bClamped && !bRounded);
    EXPECT_TRUE(
        GDALAdjustValueToDataType(GDT_Float32, 1.23, &bClamped, &bRounded) ==
            static_cast<double>(1.23f) &&
        !bClamped && !bRounded);
    EXPECT_TRUE(
        GDALAdjustValueToDataType(GDT_Float32, -1e300, &bClamped, &bRounded) ==
            -std::numeric_limits<float>::max() &&
        bClamped && !bRounded);
    EXPECT_TRUE(
        GDALAdjustValueToDataType(GDT_Float32, 1e300, &bClamped, &bRounded) ==
            std::numeric_limits<float>::max() &&
        bClamped && !bRounded);
    EXPECT_TRUE(GDALAdjustValueToDataType(
                    GDT_Float32, std::numeric_limits<float>::infinity(),
                    &bClamped,
                    &bRounded) == std::numeric_limits<float>::infinity() &&
                !bClamped && !bRounded);
    EXPECT_TRUE(GDALAdjustValueToDataType(
                    GDT_Float32, -std::numeric_limits<float>::infinity(),
                    &bClamped,
                    &bRounded) == -std::numeric_limits<float>::infinity() &&
                !bClamped && !bRounded);
    {
        double dfNan = std::numeric_limits<double>::quiet_NaN();
        double dfGot =
            GDALAdjustValueToDataType(GDT_Float32, dfNan, &bClamped, &bRounded);
        EXPECT_TRUE(memcmp(&dfNan, &dfGot, sizeof(double)) == 0 && !bClamped &&
                    !bRounded);
    }

    EXPECT_TRUE(GDALAdjustValueToDataType(GDT_Float64, 0.0, &bClamped,
                                          &bRounded) == 0.0 &&
                !bClamped && !bRounded);
    EXPECT_TRUE(GDALAdjustValueToDataType(GDT_Float64, 1e-50, &bClamped,
                                          &bRounded) == 1e-50 &&
                !bClamped && !bRounded);
    EXPECT_TRUE(GDALAdjustValueToDataType(GDT_Float64, -1e40, &bClamped,
                                          &bRounded) == -1e40 &&
                !bClamped && !bRounded);
    EXPECT_TRUE(GDALAdjustValueToDataType(GDT_Float64, 1e40, &bClamped,
                                          &bRounded) == 1e40 &&
                !bClamped && !bRounded);
    EXPECT_TRUE(GDALAdjustValueToDataType(
                    GDT_Float64, std::numeric_limits<float>::infinity(),
                    &bClamped,
                    &bRounded) == std::numeric_limits<float>::infinity() &&
                !bClamped && !bRounded);
    EXPECT_TRUE(GDALAdjustValueToDataType(
                    GDT_Float64, -std::numeric_limits<float>::infinity(),
                    &bClamped,
                    &bRounded) == -std::numeric_limits<float>::infinity() &&
                !bClamped && !bRounded);
    {
        double dfNan = std::numeric_limits<double>::quiet_NaN();
        double dfGot =
            GDALAdjustValueToDataType(GDT_Float64, dfNan, &bClamped, &bRounded);
        EXPECT_TRUE(memcmp(&dfNan, &dfGot, sizeof(double)) == 0 && !bClamped &&
                    !bRounded);
    }
}

class FakeBand : public GDALRasterBand
{
  protected:
    virtual CPLErr IReadBlock(int, int, void *) override
    {
        return CE_None;
    }

    virtual CPLErr IWriteBlock(int, int, void *) override
    {
        return CE_None;
    }

  public:
    FakeBand(int nXSize, int nYSize)
    {
        nBlockXSize = nXSize;
        nBlockYSize = nYSize;
    }
};

class DatasetWithErrorInFlushCache : public GDALDataset
{
    bool bHasFlushCache;

  public:
    DatasetWithErrorInFlushCache() : bHasFlushCache(false)
    {
    }

    ~DatasetWithErrorInFlushCache()
    {
        FlushCache(true);
    }

    virtual CPLErr FlushCache(bool bAtClosing) override
    {
        CPLErr eErr = CE_None;
        if (!bHasFlushCache)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "some error");
            eErr = CE_Failure;
        }
        if (GDALDataset::FlushCache(bAtClosing) != CE_None)
            eErr = CE_Failure;
        bHasFlushCache = true;
        return eErr;
    }

    CPLErr SetSpatialRef(const OGRSpatialReference *) override
    {
        return CE_None;
    }

    virtual CPLErr SetGeoTransform(double *) override
    {
        return CE_None;
    }

    static GDALDataset *CreateCopy(const char *, GDALDataset *, int, char **,
                                   GDALProgressFunc, void *)
    {
        return new DatasetWithErrorInFlushCache();
    }

    static GDALDataset *Create(const char *, int nXSize, int nYSize, int,
                               GDALDataType, char **)
    {
        DatasetWithErrorInFlushCache *poDS = new DatasetWithErrorInFlushCache();
        poDS->eAccess = GA_Update;
        poDS->nRasterXSize = nXSize;
        poDS->nRasterYSize = nYSize;
        poDS->SetBand(1, new FakeBand(nXSize, nYSize));
        return poDS;
    }
};

// Test that GDALTranslate() detects error in flush cache
TEST_F(test_gdal, GDALTranslate_error_flush_cache)
{
    GDALDriver *poDriver = new GDALDriver();
    poDriver->SetDescription("DatasetWithErrorInFlushCache");
    poDriver->pfnCreateCopy = DatasetWithErrorInFlushCache::CreateCopy;
    GetGDALDriverManager()->RegisterDriver(poDriver);
    const char *args[] = {"-of", "DatasetWithErrorInFlushCache", nullptr};
    GDALTranslateOptions *psOptions =
        GDALTranslateOptionsNew((char **)args, nullptr);
    GDALDatasetH hSrcDS = GDALOpen(GCORE_DATA_DIR "byte.tif", GA_ReadOnly);
    CPLErrorReset();
    CPLPushErrorHandler(CPLQuietErrorHandler);
    GDALDatasetH hOutDS = GDALTranslate("", hSrcDS, psOptions, nullptr);
    CPLPopErrorHandler();
    GDALClose(hSrcDS);
    GDALTranslateOptionsFree(psOptions);
    EXPECT_TRUE(hOutDS == nullptr);
    EXPECT_TRUE(CPLGetLastErrorType() != CE_None);
    GetGDALDriverManager()->DeregisterDriver(poDriver);
    delete poDriver;
}

// Test that GDALWarp() detects error in flush cache
TEST_F(test_gdal, GDALWarp_error_flush_cache)
{
    GDALDriver *poDriver = new GDALDriver();
    poDriver->SetDescription("DatasetWithErrorInFlushCache");
    poDriver->pfnCreate = DatasetWithErrorInFlushCache::Create;
    GetGDALDriverManager()->RegisterDriver(poDriver);
    const char *args[] = {"-of", "DatasetWithErrorInFlushCache", nullptr};
    GDALWarpAppOptions *psOptions =
        GDALWarpAppOptionsNew((char **)args, nullptr);
    GDALDatasetH hSrcDS = GDALOpen(GCORE_DATA_DIR "byte.tif", GA_ReadOnly);
    CPLErrorReset();
    CPLPushErrorHandler(CPLQuietErrorHandler);
    GDALDatasetH hOutDS =
        GDALWarp("/", nullptr, 1, &hSrcDS, psOptions, nullptr);
    CPLPopErrorHandler();
    GDALClose(hSrcDS);
    GDALWarpAppOptionsFree(psOptions);
    EXPECT_TRUE(hOutDS == nullptr);
    EXPECT_TRUE(CPLGetLastErrorType() != CE_None);
    GetGDALDriverManager()->DeregisterDriver(poDriver);
    delete poDriver;
}

// Test GDALWarp() to VRT and that we can call GDALReleaseDataset() on the
// source dataset when we want.
TEST_F(test_gdal, GDALWarp_VRT)
{
    auto hDrv = GDALGetDriverByName("GTiff");
    if (!hDrv)
    {
        GTEST_SKIP() << "GTiff driver missing";
    }
    const char *args[] = {"-of", "VRT", nullptr};
    GDALWarpAppOptions *psOptions =
        GDALWarpAppOptionsNew((char **)args, nullptr);
    GDALDatasetH hSrcDS = GDALOpen(GCORE_DATA_DIR "byte.tif", GA_ReadOnly);
    GDALDatasetH hOutDS = GDALWarp("", nullptr, 1, &hSrcDS, psOptions, nullptr);
    GDALWarpAppOptionsFree(psOptions);
    GDALReleaseDataset(hSrcDS);
    EXPECT_EQ(GDALChecksumImage(GDALGetRasterBand(hOutDS, 1), 0, 0, 20, 20),
              4672);
    GDALReleaseDataset(hOutDS);
}

// Test GDALTranslate() to VRT and that we can call GDALReleaseDataset() on the
// source dataset when we want.
TEST_F(test_gdal, GDALTranslate_VRT)
{
    auto hDrv = GDALGetDriverByName("GTiff");
    if (!hDrv)
    {
        GTEST_SKIP() << "GTiff driver missing";
    }
    const char *args[] = {"-of", "VRT", nullptr};
    GDALTranslateOptions *psOptions =
        GDALTranslateOptionsNew((char **)args, nullptr);
    GDALDatasetH hSrcDS = GDALOpen(GCORE_DATA_DIR "byte.tif", GA_ReadOnly);
    GDALDatasetH hOutDS = GDALTranslate("", hSrcDS, psOptions, nullptr);
    GDALTranslateOptionsFree(psOptions);
    GDALReleaseDataset(hSrcDS);
    EXPECT_EQ(GDALChecksumImage(GDALGetRasterBand(hOutDS, 1), 0, 0, 20, 20),
              4672);
    GDALReleaseDataset(hOutDS);
}

// Test GDALBuildVRT() and that we can call GDALReleaseDataset() on the
// source dataset when we want.
TEST_F(test_gdal, GDALBuildVRT)
{
    auto hDrv = GDALGetDriverByName("GTiff");
    if (!hDrv)
    {
        GTEST_SKIP() << "GTiff driver missing";
    }
    GDALDatasetH hSrcDS = GDALOpen(GCORE_DATA_DIR "byte.tif", GA_ReadOnly);
    GDALDatasetH hOutDS =
        GDALBuildVRT("", 1, &hSrcDS, nullptr, nullptr, nullptr);
    GDALReleaseDataset(hSrcDS);
    EXPECT_EQ(GDALChecksumImage(GDALGetRasterBand(hOutDS, 1), 0, 0, 20, 20),
              4672);
    GDALReleaseDataset(hOutDS);
}

// Test that GDALSwapWords() with unaligned buffers
TEST_F(test_gdal, GDALSwapWords_unaligned_buffers)
{
    GByte abyBuffer[8 * 2 + 1] = {0, 1, 2, 3, 4, 5, 6, 7, 255,
                                  7, 6, 5, 4, 3, 2, 1, 0};
    GDALSwapWords(abyBuffer, 4, 2, 9);
    EXPECT_EQ(abyBuffer[0], 3);
    EXPECT_EQ(abyBuffer[1], 2);
    EXPECT_EQ(abyBuffer[2], 1);
    EXPECT_EQ(abyBuffer[3], 0);

    EXPECT_EQ(abyBuffer[9], 4);
    EXPECT_EQ(abyBuffer[10], 5);
    EXPECT_EQ(abyBuffer[11], 6);
    EXPECT_EQ(abyBuffer[12], 7);
    GDALSwapWords(abyBuffer, 4, 2, 9);

    GDALSwapWords(abyBuffer, 8, 2, 9);
    EXPECT_EQ(abyBuffer[0], 7);
    EXPECT_EQ(abyBuffer[1], 6);
    EXPECT_EQ(abyBuffer[2], 5);
    EXPECT_EQ(abyBuffer[3], 4);
    EXPECT_EQ(abyBuffer[4], 3);
    EXPECT_EQ(abyBuffer[5], 2);
    EXPECT_EQ(abyBuffer[6], 1);
    EXPECT_EQ(abyBuffer[7], 0);

    EXPECT_EQ(abyBuffer[9], 0);
    EXPECT_EQ(abyBuffer[10], 1);
    EXPECT_EQ(abyBuffer[11], 2);
    EXPECT_EQ(abyBuffer[12], 3);
    EXPECT_EQ(abyBuffer[13], 4);
    EXPECT_EQ(abyBuffer[14], 5);
    EXPECT_EQ(abyBuffer[15], 6);
    EXPECT_EQ(abyBuffer[16], 7);
    GDALSwapWords(abyBuffer, 4, 2, 9);
}

// Test ARE_REAL_EQUAL()
TEST_F(test_gdal, ARE_REAL_EQUAL)
{
    EXPECT_TRUE(ARE_REAL_EQUAL(0.0, 0.0));
    EXPECT_TRUE(!ARE_REAL_EQUAL(0.0, 0.1));
    EXPECT_TRUE(!ARE_REAL_EQUAL(0.1, 0.0));
    EXPECT_TRUE(ARE_REAL_EQUAL(1.0, 1.0));
    EXPECT_TRUE(!ARE_REAL_EQUAL(1.0, 0.99));
    EXPECT_TRUE(ARE_REAL_EQUAL(-std::numeric_limits<double>::min(),
                               -std::numeric_limits<double>::min()));
    EXPECT_TRUE(ARE_REAL_EQUAL(std::numeric_limits<double>::min(),
                               std::numeric_limits<double>::min()));
    EXPECT_TRUE(!ARE_REAL_EQUAL(std::numeric_limits<double>::min(), 0.0));
    EXPECT_TRUE(ARE_REAL_EQUAL(-std::numeric_limits<double>::max(),
                               -std::numeric_limits<double>::max()));
    EXPECT_TRUE(ARE_REAL_EQUAL(std::numeric_limits<double>::max(),
                               std::numeric_limits<double>::max()));
    EXPECT_TRUE(ARE_REAL_EQUAL(-std::numeric_limits<double>::infinity(),
                               -std::numeric_limits<double>::infinity()));
    EXPECT_TRUE(ARE_REAL_EQUAL(std::numeric_limits<double>::infinity(),
                               std::numeric_limits<double>::infinity()));
    EXPECT_TRUE(!ARE_REAL_EQUAL(std::numeric_limits<double>::infinity(),
                                std::numeric_limits<double>::max()));
    EXPECT_TRUE(ARE_REAL_EQUAL(-std::numeric_limits<double>::min(),
                               -std::numeric_limits<double>::min()));

    EXPECT_TRUE(ARE_REAL_EQUAL(0.0f, 0.0f));
    EXPECT_TRUE(!ARE_REAL_EQUAL(0.0f, 0.1f));
    EXPECT_TRUE(!ARE_REAL_EQUAL(0.1f, 0.0f));
    EXPECT_TRUE(ARE_REAL_EQUAL(1.0f, 1.0f));
    EXPECT_TRUE(!ARE_REAL_EQUAL(1.0f, 0.99f));
    EXPECT_TRUE(ARE_REAL_EQUAL(-std::numeric_limits<float>::min(),
                               -std::numeric_limits<float>::min()));
    EXPECT_TRUE(ARE_REAL_EQUAL(std::numeric_limits<float>::min(),
                               std::numeric_limits<float>::min()));
    EXPECT_TRUE(!ARE_REAL_EQUAL(std::numeric_limits<float>::min(), 0.0f));
    EXPECT_TRUE(ARE_REAL_EQUAL(-std::numeric_limits<float>::max(),
                               -std::numeric_limits<float>::max()));
    EXPECT_TRUE(ARE_REAL_EQUAL(std::numeric_limits<float>::max(),
                               std::numeric_limits<float>::max()));
    EXPECT_TRUE(ARE_REAL_EQUAL(-std::numeric_limits<float>::infinity(),
                               -std::numeric_limits<float>::infinity()));
    EXPECT_TRUE(ARE_REAL_EQUAL(std::numeric_limits<float>::infinity(),
                               std::numeric_limits<float>::infinity()));
    EXPECT_TRUE(!ARE_REAL_EQUAL(std::numeric_limits<float>::infinity(),
                                std::numeric_limits<float>::max()));
}

// Test GDALIsValueInRange()
TEST_F(test_gdal, GDALIsValueInRange)
{
    EXPECT_TRUE(GDALIsValueInRange<GByte>(0));
    EXPECT_TRUE(GDALIsValueInRange<GByte>(255));
    EXPECT_TRUE(!GDALIsValueInRange<GByte>(-1));
    EXPECT_TRUE(!GDALIsValueInRange<GByte>(256));
    EXPECT_TRUE(
        !GDALIsValueInRange<GByte>(std::numeric_limits<double>::quiet_NaN()));

    EXPECT_TRUE(GDALIsValueInRange<GInt8>(-128));
    EXPECT_TRUE(GDALIsValueInRange<GInt8>(127));
    EXPECT_TRUE(!GDALIsValueInRange<GInt8>(-129));
    EXPECT_TRUE(!GDALIsValueInRange<GInt8>(128));

    // -(1 << 63)
    EXPECT_TRUE(GDALIsValueInRange<int64_t>(-9223372036854775808.0));
    // (1 << 63) - 1024
    EXPECT_TRUE(GDALIsValueInRange<int64_t>(9223372036854774784.0));
    EXPECT_TRUE(GDALIsValueInRange<int64_t>(0.5));
    // (1 << 63) - 512
    EXPECT_TRUE(!GDALIsValueInRange<int64_t>(9223372036854775296.0));

    EXPECT_TRUE(GDALIsValueInRange<uint64_t>(0.0));
    EXPECT_TRUE(GDALIsValueInRange<uint64_t>(0.5));
    // (1 << 64) - 2048
    EXPECT_TRUE(GDALIsValueInRange<uint64_t>(18446744073709549568.0));
    // (1 << 64)
    EXPECT_TRUE(!GDALIsValueInRange<uint64_t>(18446744073709551616.0));
    EXPECT_TRUE(!GDALIsValueInRange<uint64_t>(-0.5));

    EXPECT_TRUE(GDALIsValueInRange<float>(-std::numeric_limits<float>::max()));
    EXPECT_TRUE(GDALIsValueInRange<float>(std::numeric_limits<float>::max()));
    EXPECT_TRUE(
        GDALIsValueInRange<float>(-std::numeric_limits<float>::infinity()));
    EXPECT_TRUE(
        GDALIsValueInRange<float>(std::numeric_limits<float>::infinity()));
    EXPECT_TRUE(
        !GDALIsValueInRange<float>(std::numeric_limits<double>::quiet_NaN()));
    EXPECT_TRUE(
        !GDALIsValueInRange<float>(-std::numeric_limits<double>::max()));
    EXPECT_TRUE(!GDALIsValueInRange<float>(std::numeric_limits<double>::max()));

    EXPECT_TRUE(
        GDALIsValueInRange<double>(-std::numeric_limits<double>::infinity()));
    EXPECT_TRUE(
        GDALIsValueInRange<double>(std::numeric_limits<double>::infinity()));
    EXPECT_TRUE(
        GDALIsValueInRange<double>(-std::numeric_limits<double>::max()));
    EXPECT_TRUE(GDALIsValueInRange<double>(std::numeric_limits<double>::max()));
    EXPECT_TRUE(
        !GDALIsValueInRange<double>(std::numeric_limits<double>::quiet_NaN()));
}

#ifdef _MSC_VER
#pragma warning(push)
// overflow in constant arithmetic
#pragma warning(disable : 4756)
#endif

// Test GDALIsValueExactAs()
TEST_F(test_gdal, GDALIsValueExactAs)
{
    EXPECT_TRUE(GDALIsValueExactAs<GByte>(0));
    EXPECT_TRUE(GDALIsValueExactAs<GByte>(255));
    EXPECT_TRUE(!GDALIsValueExactAs<GByte>(0.5));
    EXPECT_TRUE(!GDALIsValueExactAs<GByte>(-1));
    EXPECT_TRUE(!GDALIsValueExactAs<GByte>(-0.5));
    EXPECT_TRUE(!GDALIsValueExactAs<GByte>(255.5));
    EXPECT_TRUE(!GDALIsValueExactAs<GByte>(256));
    EXPECT_TRUE(
        !GDALIsValueExactAs<GByte>(std::numeric_limits<double>::quiet_NaN()));

    // -(1 << 63)
    EXPECT_TRUE(GDALIsValueExactAs<int64_t>(-9223372036854775808.0));
    // (1 << 63) - 1024
    EXPECT_TRUE(GDALIsValueExactAs<int64_t>(9223372036854774784.0));
    EXPECT_TRUE(!GDALIsValueExactAs<int64_t>(0.5));
    // (1 << 63) - 512
    EXPECT_TRUE(!GDALIsValueExactAs<int64_t>(9223372036854775296.0));

    EXPECT_TRUE(GDALIsValueExactAs<uint64_t>(0.0));
    EXPECT_TRUE(!GDALIsValueExactAs<uint64_t>(0.5));
    // (1 << 64) - 2048
    EXPECT_TRUE(GDALIsValueExactAs<uint64_t>(18446744073709549568.0));
    // (1 << 64)
    EXPECT_TRUE(!GDALIsValueExactAs<uint64_t>(18446744073709551616.0));
    EXPECT_TRUE(!GDALIsValueExactAs<uint64_t>(-0.5));

    EXPECT_TRUE(GDALIsValueExactAs<float>(-std::numeric_limits<float>::max()));
    EXPECT_TRUE(GDALIsValueExactAs<float>(std::numeric_limits<float>::max()));
    EXPECT_TRUE(
        GDALIsValueExactAs<float>(-std::numeric_limits<float>::infinity()));
    EXPECT_TRUE(
        GDALIsValueExactAs<float>(std::numeric_limits<float>::infinity()));
    EXPECT_TRUE(
        GDALIsValueExactAs<float>(std::numeric_limits<double>::quiet_NaN()));
    EXPECT_TRUE(
        !GDALIsValueExactAs<float>(-std::numeric_limits<double>::max()));
    EXPECT_TRUE(!GDALIsValueExactAs<float>(std::numeric_limits<double>::max()));

    EXPECT_TRUE(
        GDALIsValueExactAs<double>(-std::numeric_limits<double>::infinity()));
    EXPECT_TRUE(
        GDALIsValueExactAs<double>(std::numeric_limits<double>::infinity()));
    EXPECT_TRUE(
        GDALIsValueExactAs<double>(-std::numeric_limits<double>::max()));
    EXPECT_TRUE(GDALIsValueExactAs<double>(std::numeric_limits<double>::max()));
    EXPECT_TRUE(
        GDALIsValueExactAs<double>(std::numeric_limits<double>::quiet_NaN()));
}

// Test GDALIsValueExactAs()
TEST_F(test_gdal, GDALIsValueExactAs_C_func)
{
    EXPECT_TRUE(GDALIsValueExactAs(0, GDT_Byte));
    EXPECT_TRUE(GDALIsValueExactAs(255, GDT_Byte));
    EXPECT_FALSE(GDALIsValueExactAs(-1, GDT_Byte));
    EXPECT_FALSE(GDALIsValueExactAs(256, GDT_Byte));
    EXPECT_FALSE(GDALIsValueExactAs(0.5, GDT_Byte));

    EXPECT_TRUE(GDALIsValueExactAs(-128, GDT_Int8));
    EXPECT_TRUE(GDALIsValueExactAs(127, GDT_Int8));
    EXPECT_FALSE(GDALIsValueExactAs(-129, GDT_Int8));
    EXPECT_FALSE(GDALIsValueExactAs(128, GDT_Int8));
    EXPECT_FALSE(GDALIsValueExactAs(0.5, GDT_Int8));

    EXPECT_TRUE(GDALIsValueExactAs(0, GDT_UInt16));
    EXPECT_TRUE(GDALIsValueExactAs(65535, GDT_UInt16));
    EXPECT_FALSE(GDALIsValueExactAs(-1, GDT_UInt16));
    EXPECT_FALSE(GDALIsValueExactAs(65536, GDT_UInt16));
    EXPECT_FALSE(GDALIsValueExactAs(0.5, GDT_UInt16));

    EXPECT_TRUE(GDALIsValueExactAs(-32768, GDT_Int16));
    EXPECT_TRUE(GDALIsValueExactAs(32767, GDT_Int16));
    EXPECT_FALSE(GDALIsValueExactAs(-32769, GDT_Int16));
    EXPECT_FALSE(GDALIsValueExactAs(32768, GDT_Int16));
    EXPECT_FALSE(GDALIsValueExactAs(0.5, GDT_Int16));

    EXPECT_TRUE(GDALIsValueExactAs(std::numeric_limits<uint32_t>::lowest(),
                                   GDT_UInt32));
    EXPECT_TRUE(
        GDALIsValueExactAs(std::numeric_limits<uint32_t>::max(), GDT_UInt32));
    EXPECT_FALSE(GDALIsValueExactAs(
        std::numeric_limits<uint32_t>::lowest() - 1.0, GDT_UInt32));
    EXPECT_FALSE(GDALIsValueExactAs(std::numeric_limits<uint32_t>::max() + 1.0,
                                    GDT_UInt32));
    EXPECT_FALSE(GDALIsValueExactAs(0.5, GDT_UInt32));

    EXPECT_TRUE(
        GDALIsValueExactAs(std::numeric_limits<int32_t>::lowest(), GDT_Int32));
    EXPECT_TRUE(
        GDALIsValueExactAs(std::numeric_limits<int32_t>::max(), GDT_Int32));
    EXPECT_FALSE(GDALIsValueExactAs(
        std::numeric_limits<int32_t>::lowest() - 1.0, GDT_Int32));
    EXPECT_FALSE(GDALIsValueExactAs(std::numeric_limits<int32_t>::max() + 1.0,
                                    GDT_Int32));
    EXPECT_FALSE(GDALIsValueExactAs(0.5, GDT_Int32));

    EXPECT_TRUE(GDALIsValueExactAs(
        static_cast<double>(std::numeric_limits<uint64_t>::lowest()),
        GDT_UInt64));
    // (1 << 64) - 2048
    EXPECT_TRUE(GDALIsValueExactAs(18446744073709549568.0, GDT_UInt64));
    EXPECT_FALSE(GDALIsValueExactAs(
        static_cast<double>(std::numeric_limits<uint64_t>::lowest()) - 1.0,
        GDT_UInt64));
    // (1 << 64)
    EXPECT_FALSE(GDALIsValueExactAs(18446744073709551616.0, GDT_UInt64));
    EXPECT_FALSE(GDALIsValueExactAs(0.5, GDT_UInt64));

    EXPECT_TRUE(GDALIsValueExactAs(
        static_cast<double>(std::numeric_limits<int64_t>::lowest()),
        GDT_Int64));
    // (1 << 63) - 1024
    EXPECT_TRUE(GDALIsValueExactAs(9223372036854774784.0, GDT_Int64));
    EXPECT_FALSE(GDALIsValueExactAs(
        static_cast<double>(std::numeric_limits<int64_t>::lowest()) - 2048.0,
        GDT_Int64));
    // (1 << 63) - 512
    EXPECT_FALSE(GDALIsValueExactAs(9223372036854775296.0, GDT_Int64));
    EXPECT_FALSE(GDALIsValueExactAs(0.5, GDT_Int64));

    EXPECT_TRUE(
        GDALIsValueExactAs(-std::numeric_limits<float>::max(), GDT_Float32));
    EXPECT_TRUE(
        GDALIsValueExactAs(std::numeric_limits<float>::max(), GDT_Float32));
    EXPECT_TRUE(GDALIsValueExactAs(-std::numeric_limits<float>::infinity(),
                                   GDT_Float32));
    EXPECT_TRUE(GDALIsValueExactAs(std::numeric_limits<float>::infinity(),
                                   GDT_Float32));
    EXPECT_TRUE(GDALIsValueExactAs(std::numeric_limits<double>::quiet_NaN(),
                                   GDT_Float32));
    EXPECT_TRUE(
        !GDALIsValueExactAs(-std::numeric_limits<double>::max(), GDT_Float32));
    EXPECT_TRUE(
        !GDALIsValueExactAs(std::numeric_limits<double>::max(), GDT_Float32));

    EXPECT_TRUE(GDALIsValueExactAs(-std::numeric_limits<double>::infinity(),
                                   GDT_Float64));
    EXPECT_TRUE(GDALIsValueExactAs(std::numeric_limits<double>::infinity(),
                                   GDT_Float64));
    EXPECT_TRUE(
        GDALIsValueExactAs(-std::numeric_limits<double>::max(), GDT_Float64));
    EXPECT_TRUE(
        GDALIsValueExactAs(std::numeric_limits<double>::max(), GDT_Float64));
    EXPECT_TRUE(GDALIsValueExactAs(std::numeric_limits<double>::quiet_NaN(),
                                   GDT_Float64));

    EXPECT_TRUE(GDALIsValueExactAs(0, GDT_CInt16));
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif

// Test GDALDataTypeIsInteger()
TEST_F(test_gdal, GDALDataTypeIsInteger)
{
    EXPECT_TRUE(!GDALDataTypeIsInteger(GDT_Unknown));
    EXPECT_EQ(GDALDataTypeIsInteger(GDT_Byte), TRUE);
    EXPECT_EQ(GDALDataTypeIsInteger(GDT_Int8), TRUE);
    EXPECT_EQ(GDALDataTypeIsInteger(GDT_UInt16), TRUE);
    EXPECT_EQ(GDALDataTypeIsInteger(GDT_Int16), TRUE);
    EXPECT_EQ(GDALDataTypeIsInteger(GDT_UInt32), TRUE);
    EXPECT_EQ(GDALDataTypeIsInteger(GDT_Int32), TRUE);
    EXPECT_EQ(GDALDataTypeIsInteger(GDT_UInt64), TRUE);
    EXPECT_EQ(GDALDataTypeIsInteger(GDT_Int64), TRUE);
    EXPECT_TRUE(!GDALDataTypeIsInteger(GDT_Float32));
    EXPECT_TRUE(!GDALDataTypeIsInteger(GDT_Float64));
    EXPECT_EQ(GDALDataTypeIsInteger(GDT_CInt16), TRUE);
    EXPECT_EQ(GDALDataTypeIsInteger(GDT_CInt32), TRUE);
    EXPECT_TRUE(!GDALDataTypeIsInteger(GDT_CFloat32));
    EXPECT_TRUE(!GDALDataTypeIsInteger(GDT_CFloat64));
}

// Test GDALDataTypeIsFloating()
TEST_F(test_gdal, GDALDataTypeIsFloating)
{
    EXPECT_TRUE(!GDALDataTypeIsFloating(GDT_Unknown));
    EXPECT_TRUE(!GDALDataTypeIsFloating(GDT_Byte));
    EXPECT_TRUE(!GDALDataTypeIsFloating(GDT_Int8));
    EXPECT_TRUE(!GDALDataTypeIsFloating(GDT_UInt16));
    EXPECT_TRUE(!GDALDataTypeIsFloating(GDT_Int16));
    EXPECT_TRUE(!GDALDataTypeIsFloating(GDT_UInt32));
    EXPECT_TRUE(!GDALDataTypeIsFloating(GDT_Int32));
    EXPECT_TRUE(!GDALDataTypeIsFloating(GDT_UInt64));
    EXPECT_TRUE(!GDALDataTypeIsFloating(GDT_Int64));
    EXPECT_EQ(GDALDataTypeIsFloating(GDT_Float32), TRUE);
    EXPECT_EQ(GDALDataTypeIsFloating(GDT_Float64), TRUE);
    EXPECT_TRUE(!GDALDataTypeIsFloating(GDT_CInt16));
    EXPECT_TRUE(!GDALDataTypeIsFloating(GDT_CInt32));
    EXPECT_EQ(GDALDataTypeIsFloating(GDT_CFloat32), TRUE);
    EXPECT_EQ(GDALDataTypeIsFloating(GDT_CFloat64), TRUE);
}

// Test GDALDataTypeIsComplex()
TEST_F(test_gdal, GDALDataTypeIsComplex)
{
    EXPECT_TRUE(!GDALDataTypeIsComplex(GDT_Unknown));
    EXPECT_TRUE(!GDALDataTypeIsComplex(GDT_Byte));
    EXPECT_TRUE(!GDALDataTypeIsComplex(GDT_Int8));
    EXPECT_TRUE(!GDALDataTypeIsComplex(GDT_UInt16));
    EXPECT_TRUE(!GDALDataTypeIsComplex(GDT_Int16));
    EXPECT_TRUE(!GDALDataTypeIsComplex(GDT_UInt32));
    EXPECT_TRUE(!GDALDataTypeIsComplex(GDT_Int32));
    EXPECT_TRUE(!GDALDataTypeIsComplex(GDT_UInt64));
    EXPECT_TRUE(!GDALDataTypeIsComplex(GDT_Int64));
    EXPECT_TRUE(!GDALDataTypeIsComplex(GDT_Float32));
    EXPECT_TRUE(!GDALDataTypeIsComplex(GDT_Float64));
    EXPECT_EQ(GDALDataTypeIsComplex(GDT_CInt16), TRUE);
    EXPECT_EQ(GDALDataTypeIsComplex(GDT_CInt32), TRUE);
    EXPECT_EQ(GDALDataTypeIsComplex(GDT_CFloat32), TRUE);
    EXPECT_EQ(GDALDataTypeIsComplex(GDT_CFloat64), TRUE);
}

// Test GDALDataTypeIsConversionLossy()
TEST_F(test_gdal, GDALDataTypeIsConversionLossy)
{
    EXPECT_TRUE(!GDALDataTypeIsConversionLossy(GDT_Byte, GDT_Byte));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_Byte, GDT_Int8));
    EXPECT_TRUE(!GDALDataTypeIsConversionLossy(GDT_Byte, GDT_UInt16));
    EXPECT_TRUE(!GDALDataTypeIsConversionLossy(GDT_Byte, GDT_Int16));
    EXPECT_TRUE(!GDALDataTypeIsConversionLossy(GDT_Byte, GDT_UInt32));
    EXPECT_TRUE(!GDALDataTypeIsConversionLossy(GDT_Byte, GDT_Int32));
    EXPECT_TRUE(!GDALDataTypeIsConversionLossy(GDT_Byte, GDT_UInt64));
    EXPECT_TRUE(!GDALDataTypeIsConversionLossy(GDT_Byte, GDT_Int64));
    EXPECT_TRUE(!GDALDataTypeIsConversionLossy(GDT_Byte, GDT_Float32));
    EXPECT_TRUE(!GDALDataTypeIsConversionLossy(GDT_Byte, GDT_Float64));
    EXPECT_TRUE(!GDALDataTypeIsConversionLossy(GDT_Byte, GDT_CInt16));
    EXPECT_TRUE(!GDALDataTypeIsConversionLossy(GDT_Byte, GDT_CInt32));
    EXPECT_TRUE(!GDALDataTypeIsConversionLossy(GDT_Byte, GDT_CFloat32));
    EXPECT_TRUE(!GDALDataTypeIsConversionLossy(GDT_Byte, GDT_CFloat64));

    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_Int8, GDT_Byte));
    EXPECT_TRUE(!GDALDataTypeIsConversionLossy(GDT_Int8, GDT_Int8));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_Int8, GDT_UInt16));
    EXPECT_TRUE(!GDALDataTypeIsConversionLossy(GDT_Int8, GDT_Int16));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_Int8, GDT_UInt32));
    EXPECT_TRUE(!GDALDataTypeIsConversionLossy(GDT_Int8, GDT_Int32));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_Int8, GDT_UInt64));
    EXPECT_TRUE(!GDALDataTypeIsConversionLossy(GDT_Int8, GDT_Int64));
    EXPECT_TRUE(!GDALDataTypeIsConversionLossy(GDT_Int8, GDT_Float32));
    EXPECT_TRUE(!GDALDataTypeIsConversionLossy(GDT_Int8, GDT_Float64));
    EXPECT_TRUE(!GDALDataTypeIsConversionLossy(GDT_Int8, GDT_CInt16));
    EXPECT_TRUE(!GDALDataTypeIsConversionLossy(GDT_Int8, GDT_CInt32));
    EXPECT_TRUE(!GDALDataTypeIsConversionLossy(GDT_Int8, GDT_CFloat32));
    EXPECT_TRUE(!GDALDataTypeIsConversionLossy(GDT_Int8, GDT_CFloat64));

    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_UInt16, GDT_Byte));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_UInt16, GDT_Int8));
    EXPECT_TRUE(!GDALDataTypeIsConversionLossy(GDT_UInt16, GDT_UInt16));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_UInt16, GDT_Int16));
    EXPECT_TRUE(!GDALDataTypeIsConversionLossy(GDT_UInt16, GDT_UInt32));
    EXPECT_TRUE(!GDALDataTypeIsConversionLossy(GDT_UInt16, GDT_Int32));
    EXPECT_TRUE(!GDALDataTypeIsConversionLossy(GDT_UInt16, GDT_UInt64));
    EXPECT_TRUE(!GDALDataTypeIsConversionLossy(GDT_UInt16, GDT_Int64));
    EXPECT_TRUE(!GDALDataTypeIsConversionLossy(GDT_UInt16, GDT_Float32));
    EXPECT_TRUE(!GDALDataTypeIsConversionLossy(GDT_UInt16, GDT_Float64));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_UInt16, GDT_CInt16));
    EXPECT_TRUE(!GDALDataTypeIsConversionLossy(GDT_UInt16, GDT_CInt32));
    EXPECT_TRUE(!GDALDataTypeIsConversionLossy(GDT_UInt16, GDT_CFloat32));
    EXPECT_TRUE(!GDALDataTypeIsConversionLossy(GDT_UInt16, GDT_CFloat64));

    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_Int16, GDT_Byte));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_Int16, GDT_Int8));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_Int16, GDT_UInt16));
    EXPECT_TRUE(!GDALDataTypeIsConversionLossy(GDT_Int16, GDT_Int16));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_Int16, GDT_UInt32));
    EXPECT_TRUE(!GDALDataTypeIsConversionLossy(GDT_Int16, GDT_Int32));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_Int16, GDT_UInt64));
    EXPECT_TRUE(!GDALDataTypeIsConversionLossy(GDT_Int16, GDT_Int64));
    EXPECT_TRUE(!GDALDataTypeIsConversionLossy(GDT_Int16, GDT_Float32));
    EXPECT_TRUE(!GDALDataTypeIsConversionLossy(GDT_Int16, GDT_Float64));
    EXPECT_TRUE(!GDALDataTypeIsConversionLossy(GDT_Int16, GDT_CInt16));
    EXPECT_TRUE(!GDALDataTypeIsConversionLossy(GDT_Int16, GDT_CInt32));
    EXPECT_TRUE(!GDALDataTypeIsConversionLossy(GDT_Int16, GDT_CFloat32));
    EXPECT_TRUE(!GDALDataTypeIsConversionLossy(GDT_Int16, GDT_CFloat64));

    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_UInt32, GDT_Byte));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_UInt32, GDT_UInt16));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_UInt32, GDT_Int16));
    EXPECT_TRUE(!GDALDataTypeIsConversionLossy(GDT_UInt32, GDT_UInt32));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_UInt32, GDT_Int32));
    EXPECT_TRUE(!GDALDataTypeIsConversionLossy(GDT_UInt32, GDT_UInt64));
    EXPECT_TRUE(!GDALDataTypeIsConversionLossy(GDT_UInt32, GDT_Int64));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_UInt32, GDT_Float32));
    EXPECT_TRUE(!GDALDataTypeIsConversionLossy(GDT_UInt32, GDT_Float64));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_UInt32, GDT_CInt16));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_UInt32, GDT_CInt32));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_UInt32, GDT_CFloat32));
    EXPECT_TRUE(!GDALDataTypeIsConversionLossy(GDT_UInt32, GDT_CFloat64));

    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_Int32, GDT_Byte));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_Int32, GDT_UInt16));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_Int32, GDT_Int16));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_Int32, GDT_UInt32));
    EXPECT_TRUE(!GDALDataTypeIsConversionLossy(GDT_Int32, GDT_Int32));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_Int32, GDT_UInt64));
    EXPECT_TRUE(!GDALDataTypeIsConversionLossy(GDT_Int32, GDT_Int64));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_Int32, GDT_Float32));
    EXPECT_TRUE(!GDALDataTypeIsConversionLossy(GDT_Int32, GDT_Float64));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_Int32, GDT_CInt16));
    EXPECT_TRUE(!GDALDataTypeIsConversionLossy(GDT_Int32, GDT_CInt32));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_Int32, GDT_CFloat32));
    EXPECT_TRUE(!GDALDataTypeIsConversionLossy(GDT_Int32, GDT_CFloat64));

    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_UInt64, GDT_Byte));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_UInt64, GDT_UInt16));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_UInt64, GDT_Int16));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_UInt64, GDT_UInt32));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_UInt64, GDT_Int32));
    EXPECT_TRUE(!GDALDataTypeIsConversionLossy(GDT_UInt64, GDT_UInt64));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_UInt64, GDT_Int64));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_UInt64, GDT_Float32));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_UInt64, GDT_Float64));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_UInt64, GDT_CInt16));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_UInt64, GDT_CInt32));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_UInt64, GDT_CFloat32));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_UInt64, GDT_CFloat64));

    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_Int64, GDT_Byte));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_Int64, GDT_UInt16));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_Int64, GDT_Int16));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_Int64, GDT_UInt32));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_Int64, GDT_Int32));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_Int64, GDT_UInt64));
    EXPECT_TRUE(!GDALDataTypeIsConversionLossy(GDT_Int64, GDT_Int64));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_Int64, GDT_Float32));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_Int64, GDT_Float64));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_Int64, GDT_CInt16));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_Int64, GDT_CInt32));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_Int64, GDT_CFloat32));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_Int64, GDT_CFloat64));

    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_Float32, GDT_Byte));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_Float32, GDT_UInt16));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_Float32, GDT_Int16));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_Float32, GDT_UInt32));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_Float32, GDT_Int32));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_Float32, GDT_UInt64));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_Float32, GDT_Int64));
    EXPECT_TRUE(!GDALDataTypeIsConversionLossy(GDT_Float32, GDT_Float32));
    EXPECT_TRUE(!GDALDataTypeIsConversionLossy(GDT_Float32, GDT_Float64));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_Float32, GDT_CInt16));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_Float32, GDT_CInt32));
    EXPECT_TRUE(!GDALDataTypeIsConversionLossy(GDT_Float32, GDT_CFloat32));
    EXPECT_TRUE(!GDALDataTypeIsConversionLossy(GDT_Float32, GDT_CFloat64));

    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_Float64, GDT_Byte));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_Float64, GDT_UInt16));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_Float64, GDT_Int16));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_Float64, GDT_UInt32));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_Float64, GDT_Int32));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_Float64, GDT_UInt64));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_Float64, GDT_Int64));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_Float64, GDT_Float32));
    EXPECT_TRUE(!GDALDataTypeIsConversionLossy(GDT_Float64, GDT_Float64));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_Float64, GDT_CInt16));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_Float64, GDT_CInt32));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_Float64, GDT_CFloat32));
    EXPECT_TRUE(!GDALDataTypeIsConversionLossy(GDT_Float64, GDT_CFloat64));

    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_CInt16, GDT_Byte));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_CInt16, GDT_UInt16));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_CInt16, GDT_Int16));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_CInt16, GDT_UInt32));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_CInt16, GDT_Int32));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_CInt16, GDT_UInt64));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_CInt16, GDT_Int64));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_CInt16, GDT_Float32));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_CInt16, GDT_Float64));
    EXPECT_TRUE(!GDALDataTypeIsConversionLossy(GDT_CInt16, GDT_CInt16));
    EXPECT_TRUE(!GDALDataTypeIsConversionLossy(GDT_CInt16, GDT_CInt32));
    EXPECT_TRUE(!GDALDataTypeIsConversionLossy(GDT_CInt16, GDT_CFloat32));
    EXPECT_TRUE(!GDALDataTypeIsConversionLossy(GDT_CInt16, GDT_CFloat64));

    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_CInt32, GDT_Byte));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_CInt32, GDT_UInt16));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_CInt32, GDT_Int16));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_CInt32, GDT_UInt32));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_CInt32, GDT_Int32));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_CInt32, GDT_UInt64));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_CInt32, GDT_Int64));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_CInt32, GDT_Float32));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_CInt32, GDT_Float64));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_CInt32, GDT_CInt16));
    EXPECT_TRUE(!GDALDataTypeIsConversionLossy(GDT_CInt32, GDT_CInt32));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_CInt32, GDT_CFloat32));
    EXPECT_TRUE(!GDALDataTypeIsConversionLossy(GDT_CInt32, GDT_CFloat64));

    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_CFloat32, GDT_Byte));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_CFloat32, GDT_UInt16));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_CFloat32, GDT_Int16));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_CFloat32, GDT_UInt32));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_CFloat32, GDT_Int32));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_CFloat32, GDT_UInt64));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_CFloat32, GDT_Int64));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_CFloat32, GDT_Float32));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_CFloat32, GDT_Float64));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_CFloat32, GDT_CInt16));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_CFloat32, GDT_CInt32));
    EXPECT_TRUE(!GDALDataTypeIsConversionLossy(GDT_CFloat32, GDT_CFloat32));
    EXPECT_TRUE(!GDALDataTypeIsConversionLossy(GDT_CFloat32, GDT_CFloat64));

    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_CFloat64, GDT_Byte));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_CFloat64, GDT_UInt16));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_CFloat64, GDT_Int16));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_CFloat64, GDT_UInt32));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_CFloat64, GDT_Int32));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_CFloat64, GDT_UInt64));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_CFloat64, GDT_Int64));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_CFloat64, GDT_Float32));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_CFloat64, GDT_Float64));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_CFloat64, GDT_CInt16));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_CFloat64, GDT_CInt32));
    EXPECT_TRUE(GDALDataTypeIsConversionLossy(GDT_CFloat64, GDT_CFloat32));
    EXPECT_TRUE(!GDALDataTypeIsConversionLossy(GDT_CFloat64, GDT_CFloat64));
}

// Test GDALDataset::GetBands()
TEST_F(test_gdal, GDALDataset_GetBands)
{
    GDALDatasetUniquePtr poDS(GDALDriver::FromHandle(GDALGetDriverByName("MEM"))
                                  ->Create("", 1, 1, 3, GDT_Byte, nullptr));
    int nExpectedNumber = 1;
    for (auto &&poBand : poDS->GetBands())
    {
        EXPECT_EQ(poBand->GetBand(), nExpectedNumber);
        nExpectedNumber++;
    }
    ASSERT_EQ(nExpectedNumber, 3 + 1);

    ASSERT_EQ(poDS->GetBands().size(), 3U);
    EXPECT_EQ(poDS->GetBands()[0], poDS->GetRasterBand(1));
    EXPECT_EQ(poDS->GetBands()[static_cast<size_t>(0)], poDS->GetRasterBand(1));
}

TEST_F(test_gdal, GDALExtendedDataType)
{
    // non-null string to string
    {
        const char *srcPtr = "foo";
        char *dstPtr = nullptr;
        GDALExtendedDataType::CopyValue(
            &srcPtr, GDALExtendedDataType::CreateString(), &dstPtr,
            GDALExtendedDataType::CreateString());
        EXPECT_TRUE(dstPtr != nullptr);
        // Coverity isn't smart enough to figure out that GetClass() of
        // CreateString() is GEDTC_STRING and then takes the wrong path
        // in CopyValue() and makes wrong assumptions.
        // coverity[string_null]
        EXPECT_STREQ(dstPtr, srcPtr);
        // coverity[incorrect_free]
        CPLFree(dstPtr);
    }
    // null string to string
    {
        const char *srcPtr = nullptr;
        char *dstPtr = nullptr;
        GDALExtendedDataType::CopyValue(
            &srcPtr, GDALExtendedDataType::CreateString(), &dstPtr,
            GDALExtendedDataType::CreateString());
        EXPECT_TRUE(dstPtr == nullptr);
    }
    // non-null string to Int32
    {
        const char *srcPtr = "2";
        int32_t nVal = 1;
        GDALExtendedDataType::CopyValue(
            &srcPtr, GDALExtendedDataType::CreateString(), &nVal,
            GDALExtendedDataType::Create(GDT_Int32));
        EXPECT_EQ(nVal, 2);
    }
    // null string to Int32
    {
        const char *srcPtr = nullptr;
        int32_t nVal = 1;
        GDALExtendedDataType::CopyValue(
            &srcPtr, GDALExtendedDataType::CreateString(), &nVal,
            GDALExtendedDataType::Create(GDT_Int32));
        EXPECT_EQ(nVal, 0);
    }
    // non-null string to Int64
    {
        const char *srcPtr = "2";
        int64_t nVal = 1;
        GDALExtendedDataType::CopyValue(
            &srcPtr, GDALExtendedDataType::CreateString(), &nVal,
            GDALExtendedDataType::Create(GDT_Int64));
        EXPECT_EQ(nVal, 2);
    }
    // null string to Int64
    {
        const char *srcPtr = nullptr;
        int64_t nVal = 1;
        GDALExtendedDataType::CopyValue(
            &srcPtr, GDALExtendedDataType::CreateString(), &nVal,
            GDALExtendedDataType::Create(GDT_Int64));
        EXPECT_EQ(nVal, 0);
    }
    // non-null string to UInt64
    {
        char *srcPtr = nullptr;
        uint64_t nVal = 1;
        GDALExtendedDataType::CopyValue(
            &srcPtr, GDALExtendedDataType::CreateString(), &nVal,
            GDALExtendedDataType::Create(GDT_UInt64));
        EXPECT_EQ(nVal, 0U);
    }
    // non-null string to Int64
    {
        const char *srcPtr = "2";
        uint64_t nVal = 1;
        GDALExtendedDataType::CopyValue(
            &srcPtr, GDALExtendedDataType::CreateString(), &nVal,
            GDALExtendedDataType::Create(GDT_UInt64));
        EXPECT_EQ(nVal, 2U);
    }

    class myArray : public GDALMDArray
    {
        GDALExtendedDataType m_dt;
        std::vector<std::shared_ptr<GDALDimension>> m_dims;
        std::vector<GUInt64> m_blockSize;
        const std::string m_osEmptyFilename{};

        static std::vector<std::shared_ptr<GDALDimension>>
        BuildDims(const std::vector<GUInt64> &sizes)
        {
            std::vector<std::shared_ptr<GDALDimension>> dims;
            for (const auto sz : sizes)
            {
                dims.emplace_back(
                    std::make_shared<GDALDimension>("", "", "", "", sz));
            }
            return dims;
        }

      protected:
        bool IRead(const GUInt64 *, const size_t *, const GInt64 *,
                   const GPtrDiff_t *, const GDALExtendedDataType &,
                   void *) const override
        {
            return false;
        }

      public:
        myArray(GDALDataType eDT, const std::vector<GUInt64> &sizes,
                const std::vector<GUInt64> &blocksizes)
            : GDALAbstractMDArray("", "array"), GDALMDArray("", "array"),
              m_dt(GDALExtendedDataType::Create(eDT)), m_dims(BuildDims(sizes)),
              m_blockSize(blocksizes)
        {
        }

        myArray(const GDALExtendedDataType &dt,
                const std::vector<GUInt64> &sizes,
                const std::vector<GUInt64> &blocksizes)
            : GDALAbstractMDArray("", "array"), GDALMDArray("", "array"),
              m_dt(dt), m_dims(BuildDims(sizes)), m_blockSize(blocksizes)
        {
        }

        bool IsWritable() const override
        {
            return true;
        }

        const std::string &GetFilename() const override
        {
            return m_osEmptyFilename;
        }

        static std::shared_ptr<myArray>
        Create(GDALDataType eDT, const std::vector<GUInt64> &sizes,
               const std::vector<GUInt64> &blocksizes)
        {
            auto ar(
                std::shared_ptr<myArray>(new myArray(eDT, sizes, blocksizes)));
            ar->SetSelf(ar);
            return ar;
        }

        static std::shared_ptr<myArray>
        Create(const GDALExtendedDataType &dt,
               const std::vector<GUInt64> &sizes,
               const std::vector<GUInt64> &blocksizes)
        {
            auto ar(
                std::shared_ptr<myArray>(new myArray(dt, sizes, blocksizes)));
            ar->SetSelf(ar);
            return ar;
        }

        const std::vector<std::shared_ptr<GDALDimension>> &
        GetDimensions() const override
        {
            return m_dims;
        }

        const GDALExtendedDataType &GetDataType() const override
        {
            return m_dt;
        }

        std::vector<GUInt64> GetBlockSize() const override
        {
            return m_blockSize;
        }
    };

    {
        auto ar(myArray::Create(GDT_UInt16, {3000, 1000, 2000}, {32, 64, 128}));
        EXPECT_EQ(ar->at(0)->GetDimensionCount(), 2U);
        EXPECT_EQ(ar->at(2999, 999, 1999)->GetDimensionCount(), 0U);
        CPLPushErrorHandler(CPLQuietErrorHandler);
        EXPECT_TRUE(ar->at(3000, 0, 0) == nullptr);
        EXPECT_TRUE(ar->at(0, 0, 0, 0) == nullptr);
        EXPECT_TRUE((*ar)["foo"] == nullptr);
        CPLPopErrorHandler();
    }

    {
        std::vector<std::unique_ptr<GDALEDTComponent>> comps;
        comps.emplace_back(
            std::unique_ptr<GDALEDTComponent>(new GDALEDTComponent(
                "f\\o\"o", 0, GDALExtendedDataType::Create(GDT_Int32))));
        auto dt(GDALExtendedDataType::Create("", 4, std::move(comps)));
        auto ar(myArray::Create(dt, {3000, 1000, 2000}, {32, 64, 128}));
        EXPECT_TRUE((*ar)["f\\o\"o"] != nullptr);
    }

    {
        myArray ar(GDT_UInt16, {}, {});

        CPLPushErrorHandler(CPLQuietErrorHandler);
        EXPECT_TRUE(ar.GetView("[...]") == nullptr);
        CPLPopErrorHandler();

        auto cs = ar.GetProcessingChunkSize(0);
        EXPECT_EQ(cs.size(), 0U);

        struct TmpStructNoDim
        {
            static bool func(GDALAbstractMDArray *p_ar,
                             const GUInt64 *chunk_array_start_idx,
                             const size_t *chunk_count, GUInt64 iCurChunk,
                             GUInt64 nChunkCount, void *user_data)
            {
                EXPECT_TRUE(p_ar->GetName() == "array");
                EXPECT_TRUE(chunk_array_start_idx == nullptr);
                EXPECT_TRUE(chunk_count == nullptr);
                EXPECT_EQ(iCurChunk, 1U);
                EXPECT_EQ(nChunkCount, 1U);
                *static_cast<bool *>(user_data) = true;
                return true;
            }
        };

        bool b = false;
        ar.ProcessPerChunk(nullptr, nullptr, nullptr, TmpStructNoDim::func, &b);
        EXPECT_TRUE(b);
    }

    struct ChunkDef
    {
        std::vector<GUInt64> array_start_idx;
        std::vector<GUInt64> count;
    };

    struct TmpStruct
    {
        static bool func(GDALAbstractMDArray *p_ar,
                         const GUInt64 *chunk_array_start_idx,
                         const size_t *chunk_count, GUInt64 iCurChunk,
                         GUInt64 nChunkCount, void *user_data)
        {
            EXPECT_EQ(p_ar->GetName(), "array");
            std::vector<ChunkDef> *p_chunkDefs =
                static_cast<std::vector<ChunkDef> *>(user_data);
            std::vector<GUInt64> v_chunk_array_start_idx;
            v_chunk_array_start_idx.insert(
                v_chunk_array_start_idx.end(), chunk_array_start_idx,
                chunk_array_start_idx + p_ar->GetDimensionCount());
            std::vector<GUInt64> v_chunk_count;
            v_chunk_count.insert(v_chunk_count.end(), chunk_count,
                                 chunk_count + p_ar->GetDimensionCount());
            ChunkDef chunkDef;
            chunkDef.array_start_idx = std::move(v_chunk_array_start_idx);
            chunkDef.count = std::move(v_chunk_count);
            p_chunkDefs->emplace_back(std::move(chunkDef));
            EXPECT_EQ(p_chunkDefs->size(), iCurChunk);
            EXPECT_TRUE(iCurChunk > 0);
            EXPECT_TRUE(iCurChunk <= nChunkCount);
            return true;
        }
    };

    {
        myArray ar(GDT_UInt16, {3000, 1000, 2000}, {32, 64, 128});
        {
            auto cs = ar.GetProcessingChunkSize(0);
            EXPECT_EQ(cs.size(), 3U);
            EXPECT_EQ(cs[0], 32U);
            EXPECT_EQ(cs[1], 64U);
            EXPECT_EQ(cs[2], 128U);
        }
        {
            auto cs = ar.GetProcessingChunkSize(40 * 1000 * 1000);
            EXPECT_EQ(cs.size(), 3U);
            EXPECT_EQ(cs[0], 32U);
            EXPECT_EQ(cs[1], 256U);
            EXPECT_EQ(cs[2], 2000U);

            std::vector<ChunkDef> chunkDefs;

            // Error cases of input parameters of ProcessPerChunk()
            {
                // array_start_idx[0] + count[0] > 3000
                std::vector<GUInt64> array_start_idx{1, 0, 0};
                std::vector<GUInt64> count{3000, 1000, 2000};
                CPLPushErrorHandler(CPLQuietErrorHandler);
                EXPECT_TRUE(!ar.ProcessPerChunk(array_start_idx.data(),
                                                count.data(), cs.data(),
                                                TmpStruct::func, &chunkDefs));
                CPLPopErrorHandler();
            }
            {
                // array_start_idx[0] >= 3000
                std::vector<GUInt64> array_start_idx{3000, 0, 0};
                std::vector<GUInt64> count{1, 1000, 2000};
                CPLPushErrorHandler(CPLQuietErrorHandler);
                EXPECT_TRUE(!ar.ProcessPerChunk(array_start_idx.data(),
                                                count.data(), cs.data(),
                                                TmpStruct::func, &chunkDefs));
                CPLPopErrorHandler();
            }
            {
                // count[0] > 3000
                std::vector<GUInt64> array_start_idx{0, 0, 0};
                std::vector<GUInt64> count{3001, 1000, 2000};
                CPLPushErrorHandler(CPLQuietErrorHandler);
                EXPECT_TRUE(!ar.ProcessPerChunk(array_start_idx.data(),
                                                count.data(), cs.data(),
                                                TmpStruct::func, &chunkDefs));
                CPLPopErrorHandler();
            }
            {
                // count[0] == 0
                std::vector<GUInt64> array_start_idx{0, 0, 0};
                std::vector<GUInt64> count{0, 1000, 2000};
                CPLPushErrorHandler(CPLQuietErrorHandler);
                EXPECT_TRUE(!ar.ProcessPerChunk(array_start_idx.data(),
                                                count.data(), cs.data(),
                                                TmpStruct::func, &chunkDefs));
                CPLPopErrorHandler();
            }
            {
                // myCustomChunkSize[0] == 0
                std::vector<GUInt64> array_start_idx{0, 0, 0};
                std::vector<GUInt64> count{3000, 1000, 2000};
                std::vector<size_t> myCustomChunkSize{0, 1000, 2000};
                CPLPushErrorHandler(CPLQuietErrorHandler);
                EXPECT_TRUE(!ar.ProcessPerChunk(
                    array_start_idx.data(), count.data(),
                    myCustomChunkSize.data(), TmpStruct::func, &chunkDefs));
                CPLPopErrorHandler();
            }
            {
                // myCustomChunkSize[0] > 3000
                std::vector<GUInt64> array_start_idx{0, 0, 0};
                std::vector<GUInt64> count{3000, 1000, 2000};
                std::vector<size_t> myCustomChunkSize{3001, 1000, 2000};
                CPLPushErrorHandler(CPLQuietErrorHandler);
                EXPECT_TRUE(!ar.ProcessPerChunk(
                    array_start_idx.data(), count.data(),
                    myCustomChunkSize.data(), TmpStruct::func, &chunkDefs));
                CPLPopErrorHandler();
            }

            std::vector<GUInt64> array_start_idx{1500, 256, 0};
            std::vector<GUInt64> count{99, 512, 2000};
            EXPECT_TRUE(ar.ProcessPerChunk(array_start_idx.data(), count.data(),
                                           cs.data(), TmpStruct::func,
                                           &chunkDefs));

            size_t nExpectedChunks = 1;
            for (size_t i = 0; i < ar.GetDimensionCount(); i++)
            {
                nExpectedChunks *= static_cast<size_t>(
                    1 + ((array_start_idx[i] + count[i] - 1) / cs[i]) -
                    (array_start_idx[i] / cs[i]));
            }
            EXPECT_EQ(chunkDefs.size(), nExpectedChunks);

            CPLString osChunks;
            for (const auto &chunkDef : chunkDefs)
            {
                osChunks += CPLSPrintf("{%u, %u, %u}, {%u, %u, %u}\n",
                                       (unsigned)chunkDef.array_start_idx[0],
                                       (unsigned)chunkDef.array_start_idx[1],
                                       (unsigned)chunkDef.array_start_idx[2],
                                       (unsigned)chunkDef.count[0],
                                       (unsigned)chunkDef.count[1],
                                       (unsigned)chunkDef.count[2]);
            }
            EXPECT_EQ(osChunks, "{1500, 256, 0}, {4, 256, 2000}\n"
                                "{1500, 512, 0}, {4, 256, 2000}\n"
                                "{1504, 256, 0}, {32, 256, 2000}\n"
                                "{1504, 512, 0}, {32, 256, 2000}\n"
                                "{1536, 256, 0}, {32, 256, 2000}\n"
                                "{1536, 512, 0}, {32, 256, 2000}\n"
                                "{1568, 256, 0}, {31, 256, 2000}\n"
                                "{1568, 512, 0}, {31, 256, 2000}\n");
        }
    }

    // Another error case of ProcessPerChunk
    {
        const auto M64 = std::numeric_limits<GUInt64>::max();
        const auto Msize_t = std::numeric_limits<size_t>::max();
        myArray ar(GDT_UInt16, {M64, M64, M64}, {32, 256, 128});

        // Product of myCustomChunkSize[] > Msize_t
        std::vector<GUInt64> array_start_idx{0, 0, 0};
        std::vector<GUInt64> count{3000, 1000, 2000};
        std::vector<size_t> myCustomChunkSize{Msize_t, Msize_t, Msize_t};
        std::vector<ChunkDef> chunkDefs;
        CPLPushErrorHandler(CPLQuietErrorHandler);
        EXPECT_TRUE(!ar.ProcessPerChunk(array_start_idx.data(), count.data(),
                                        myCustomChunkSize.data(),
                                        TmpStruct::func, &chunkDefs));
        CPLPopErrorHandler();
    }

    {
        const auto BIG = GUInt64(5000) * 1000 * 1000;
        myArray ar(GDT_UInt16, {BIG + 3000, BIG + 1000, BIG + 2000},
                   {32, 256, 128});
        std::vector<GUInt64> array_start_idx{BIG + 1500, BIG + 256, BIG + 0};
        std::vector<GUInt64> count{99, 512, 2000};
        std::vector<ChunkDef> chunkDefs;
        auto cs = ar.GetProcessingChunkSize(40 * 1000 * 1000);
        EXPECT_TRUE(ar.ProcessPerChunk(array_start_idx.data(), count.data(),
                                       cs.data(), TmpStruct::func, &chunkDefs));

        size_t nExpectedChunks = 1;
        for (size_t i = 0; i < ar.GetDimensionCount(); i++)
        {
            nExpectedChunks *= static_cast<size_t>(
                1 + ((array_start_idx[i] + count[i] - 1) / cs[i]) -
                (array_start_idx[i] / cs[i]));
        }
        EXPECT_EQ(chunkDefs.size(), nExpectedChunks);

        CPLString osChunks;
        for (const auto &chunkDef : chunkDefs)
        {
            osChunks += CPLSPrintf("{" CPL_FRMT_GUIB ", " CPL_FRMT_GUIB
                                   ", " CPL_FRMT_GUIB "}, {%u, %u, %u}\n",
                                   (GUIntBig)chunkDef.array_start_idx[0],
                                   (GUIntBig)chunkDef.array_start_idx[1],
                                   (GUIntBig)chunkDef.array_start_idx[2],
                                   (unsigned)chunkDef.count[0],
                                   (unsigned)chunkDef.count[1],
                                   (unsigned)chunkDef.count[2]);
        }
        EXPECT_EQ(osChunks,
                  "{5000001500, 5000000256, 5000000000}, {4, 256, 2000}\n"
                  "{5000001500, 5000000512, 5000000000}, {4, 256, 2000}\n"
                  "{5000001504, 5000000256, 5000000000}, {32, 256, 2000}\n"
                  "{5000001504, 5000000512, 5000000000}, {32, 256, 2000}\n"
                  "{5000001536, 5000000256, 5000000000}, {32, 256, 2000}\n"
                  "{5000001536, 5000000512, 5000000000}, {32, 256, 2000}\n"
                  "{5000001568, 5000000256, 5000000000}, {31, 256, 2000}\n"
                  "{5000001568, 5000000512, 5000000000}, {31, 256, 2000}\n");
    }

    {
        // Test with 0 in GetBlockSize()
        myArray ar(GDT_UInt16, {500, 1000, 2000}, {0, 0, 128});
        {
            auto cs = ar.GetProcessingChunkSize(300 * 2);
            EXPECT_EQ(cs.size(), 3U);
            EXPECT_EQ(cs[0], 1U);
            EXPECT_EQ(cs[1], 1U);
            EXPECT_EQ(cs[2], 256U);
        }
        {
            auto cs = ar.GetProcessingChunkSize(40 * 1000 * 1000);
            EXPECT_EQ(cs.size(), 3U);
            EXPECT_EQ(cs[0], 10U);
            EXPECT_EQ(cs[1], 1000U);
            EXPECT_EQ(cs[2], 2000U);
        }
        {
            auto cs = ar.GetProcessingChunkSize(500U * 1000 * 2000 * 2);
            EXPECT_EQ(cs.size(), 3U);
            EXPECT_EQ(cs[0], 500U);
            EXPECT_EQ(cs[1], 1000U);
            EXPECT_EQ(cs[2], 2000U);
        }
        {
            auto cs = ar.GetProcessingChunkSize(500U * 1000 * 2000 * 2 - 1);
            EXPECT_EQ(cs.size(), 3U);
            EXPECT_EQ(cs[0], 499U);
            EXPECT_EQ(cs[1], 1000U);
            EXPECT_EQ(cs[2], 2000U);
        }
    }
    {
        const auto M = std::numeric_limits<GUInt64>::max();
        myArray ar(GDT_UInt16, {M, M, M}, {M, M, M / 2});
        {
            auto cs = ar.GetProcessingChunkSize(0);
            EXPECT_EQ(cs.size(), 3U);
            EXPECT_EQ(cs[0], 1U);
            EXPECT_EQ(cs[1], 1U);
#if SIZEOF_VOIDP == 8
            EXPECT_EQ(cs[2], static_cast<size_t>(M / 2));
#else
            EXPECT_EQ(cs[2], 1U);
#endif
        }
    }
#if SIZEOF_VOIDP == 8
    {
        const auto M = std::numeric_limits<GUInt64>::max();
        myArray ar(GDT_UInt16, {M, M, M}, {M, M, M / 4});
        {
            auto cs =
                ar.GetProcessingChunkSize(std::numeric_limits<size_t>::max());
            EXPECT_EQ(cs.size(), 3U);
            EXPECT_EQ(cs[0], 1U);
            EXPECT_EQ(cs[1], 1U);
            EXPECT_EQ(cs[2], (std::numeric_limits<size_t>::max() / 4) * 2);
        }
    }
#endif
}

// Test GDALDataset::GetRawBinaryLayout() implementations
TEST_F(test_gdal, GetRawBinaryLayout_ENVI)
{
    if (GDALGetDriverByName("ENVI") == nullptr)
    {
        GTEST_SKIP() << "ENVI driver missing";
    }

    {
        GDALDatasetUniquePtr poDS(
            GDALDataset::Open(GDRIVERS_DATA_DIR "envi/envi_rgbsmall_bip.img"));
        EXPECT_TRUE(poDS != nullptr);
        GDALDataset::RawBinaryLayout sLayout;
        EXPECT_TRUE(poDS->GetRawBinaryLayout(sLayout));
        EXPECT_EQ(sLayout.osRawFilename, poDS->GetDescription());
        EXPECT_EQ(
            static_cast<int>(sLayout.eInterleaving),
            static_cast<int>(GDALDataset::RawBinaryLayout::Interleaving::BIP));
        EXPECT_EQ(sLayout.eDataType, GDT_Byte);
        EXPECT_TRUE(sLayout.bLittleEndianOrder);
        EXPECT_EQ(sLayout.nImageOffset, 0U);
        EXPECT_EQ(sLayout.nPixelOffset, 3);
        EXPECT_EQ(sLayout.nLineOffset, 3 * 50);
        EXPECT_EQ(sLayout.nBandOffset, 1);
    }

    {
        GDALDatasetUniquePtr poDS(
            GDALDataset::Open(GDRIVERS_DATA_DIR "envi/envi_rgbsmall_bil.img"));
        EXPECT_TRUE(poDS != nullptr);
        GDALDataset::RawBinaryLayout sLayout;
        EXPECT_TRUE(poDS->GetRawBinaryLayout(sLayout));
        EXPECT_EQ(sLayout.osRawFilename, poDS->GetDescription());
        EXPECT_EQ(
            static_cast<int>(sLayout.eInterleaving),
            static_cast<int>(GDALDataset::RawBinaryLayout::Interleaving::BIL));
        EXPECT_EQ(sLayout.eDataType, GDT_Byte);
        EXPECT_TRUE(sLayout.bLittleEndianOrder);
        EXPECT_EQ(sLayout.nImageOffset, 0U);
        EXPECT_EQ(sLayout.nPixelOffset, 1);
        EXPECT_EQ(sLayout.nLineOffset, 3 * 50);
        EXPECT_EQ(sLayout.nBandOffset, 50);
    }

    {
        GDALDatasetUniquePtr poDS(
            GDALDataset::Open(GDRIVERS_DATA_DIR "envi/envi_rgbsmall_bsq.img"));
        EXPECT_TRUE(poDS != nullptr);
        GDALDataset::RawBinaryLayout sLayout;
        EXPECT_TRUE(poDS->GetRawBinaryLayout(sLayout));
        EXPECT_EQ(sLayout.osRawFilename, poDS->GetDescription());
        EXPECT_EQ(
            static_cast<int>(sLayout.eInterleaving),
            static_cast<int>(GDALDataset::RawBinaryLayout::Interleaving::BSQ));
        EXPECT_EQ(sLayout.eDataType, GDT_Byte);
        EXPECT_TRUE(sLayout.bLittleEndianOrder);
        EXPECT_EQ(sLayout.nImageOffset, 0U);
        EXPECT_EQ(sLayout.nPixelOffset, 1);
        EXPECT_EQ(sLayout.nLineOffset, 50);
        EXPECT_EQ(sLayout.nBandOffset, 50 * 49);
    }
}

// Test GDALDataset::GetRawBinaryLayout() implementations
TEST_F(test_gdal, GetRawBinaryLayout_GTIFF)
{
    if (GDALGetDriverByName("GTIFF") == nullptr)
    {
        GTEST_SKIP() << "GTIFF driver missing";
    }

    {
        GDALDatasetUniquePtr poDS(
            GDALDataset::Open(GCORE_DATA_DIR "uint16.tif"));
        EXPECT_TRUE(poDS != nullptr);
        GDALDataset::RawBinaryLayout sLayout;
        EXPECT_TRUE(poDS->GetRawBinaryLayout(sLayout));
        EXPECT_EQ(sLayout.osRawFilename, poDS->GetDescription());
        EXPECT_EQ(static_cast<int>(sLayout.eInterleaving),
                  static_cast<int>(
                      GDALDataset::RawBinaryLayout::Interleaving::UNKNOWN));
        EXPECT_EQ(sLayout.eDataType, GDT_UInt16);
        EXPECT_TRUE(sLayout.bLittleEndianOrder);
        EXPECT_EQ(sLayout.nImageOffset, 8U);
        EXPECT_EQ(sLayout.nPixelOffset, 2);
        EXPECT_EQ(sLayout.nLineOffset, 40);
        EXPECT_EQ(sLayout.nBandOffset, 0);
    }

    {
        GDALDatasetUniquePtr poDS(
            GDALDataset::Open(GCORE_DATA_DIR "rgbsmall.tif"));
        EXPECT_TRUE(poDS != nullptr);
        GDALDataset::RawBinaryLayout sLayout;
        // Compressed
        EXPECT_TRUE(!poDS->GetRawBinaryLayout(sLayout));
    }

    {
        GDALDatasetUniquePtr poDS(
            GDALDataset::Open(GCORE_DATA_DIR "stefan_full_rgba.tif"));
        EXPECT_TRUE(poDS != nullptr);
        GDALDataset::RawBinaryLayout sLayout;
        EXPECT_TRUE(poDS->GetRawBinaryLayout(sLayout));
        EXPECT_EQ(sLayout.osRawFilename, poDS->GetDescription());
        EXPECT_EQ(
            static_cast<int>(sLayout.eInterleaving),
            static_cast<int>(GDALDataset::RawBinaryLayout::Interleaving::BIP));
        EXPECT_EQ(sLayout.eDataType, GDT_Byte);
        EXPECT_EQ(sLayout.nImageOffset, 278U);
        EXPECT_EQ(sLayout.nPixelOffset, 4);
        EXPECT_EQ(sLayout.nLineOffset, 162 * 4);
        EXPECT_EQ(sLayout.nBandOffset, 1);
    }

    {
        GDALDatasetUniquePtr poSrcDS(
            GDALDataset::Open(GCORE_DATA_DIR "rgbsmall.tif"));
        EXPECT_TRUE(poSrcDS != nullptr);
        auto tmpFilename = "/vsimem/tmp.tif";
        auto poDrv = GDALDriver::FromHandle(GDALGetDriverByName("GTiff"));
        const char *options[] = {"INTERLEAVE=BAND", nullptr};
        auto poDS(GDALDatasetUniquePtr(
            poDrv->CreateCopy(tmpFilename, poSrcDS.get(), false,
                              const_cast<char **>(options), nullptr, nullptr)));
        EXPECT_TRUE(poDS != nullptr);
        GDALDataset::RawBinaryLayout sLayout;
        EXPECT_TRUE(poDS->GetRawBinaryLayout(sLayout));
        EXPECT_EQ(sLayout.osRawFilename, poDS->GetDescription());
        EXPECT_EQ(
            static_cast<int>(sLayout.eInterleaving),
            static_cast<int>(GDALDataset::RawBinaryLayout::Interleaving::BSQ));
        EXPECT_EQ(sLayout.eDataType, GDT_Byte);
        EXPECT_TRUE(sLayout.nImageOffset >= 396U);
        EXPECT_EQ(sLayout.nPixelOffset, 1);
        EXPECT_EQ(sLayout.nLineOffset, 50);
        EXPECT_EQ(sLayout.nBandOffset, 50 * 50);
        poDS.reset();
        VSIUnlink(tmpFilename);
    }

    {
        GDALDatasetUniquePtr poSrcDS(
            GDALDataset::Open(GCORE_DATA_DIR "rgbsmall.tif"));
        EXPECT_TRUE(poSrcDS != nullptr);
        auto tmpFilename = "/vsimem/tmp.tif";
        const char *options[] = {"-srcwin",
                                 "0",
                                 "0",
                                 "48",
                                 "32",
                                 "-co",
                                 "INTERLEAVE=PIXEL",
                                 "-co",
                                 "TILED=YES",
                                 "-co",
                                 "BLOCKXSIZE=48",
                                 "-co",
                                 "BLOCKYSIZE=32",
                                 nullptr};
        auto psOptions =
            GDALTranslateOptionsNew(const_cast<char **>(options), nullptr);
        auto poDS(GDALDatasetUniquePtr(GDALDataset::FromHandle(
            GDALTranslate(tmpFilename, GDALDataset::ToHandle(poSrcDS.get()),
                          psOptions, nullptr))));
        GDALTranslateOptionsFree(psOptions);
        EXPECT_TRUE(poDS != nullptr);
        GDALDataset::RawBinaryLayout sLayout;
        EXPECT_TRUE(poDS->GetRawBinaryLayout(sLayout));
        EXPECT_EQ(sLayout.osRawFilename, poDS->GetDescription());
        EXPECT_EQ(
            static_cast<int>(sLayout.eInterleaving),
            static_cast<int>(GDALDataset::RawBinaryLayout::Interleaving::BIP));
        EXPECT_EQ(sLayout.eDataType, GDT_Byte);
        EXPECT_TRUE(sLayout.nImageOffset >= 390U);
        EXPECT_EQ(sLayout.nPixelOffset, 3);
        EXPECT_EQ(sLayout.nLineOffset, 48 * 3);
        EXPECT_EQ(sLayout.nBandOffset, 1);
        poDS.reset();
        VSIUnlink(tmpFilename);
    }

    {
        GDALDatasetUniquePtr poSrcDS(
            GDALDataset::Open(GCORE_DATA_DIR "rgbsmall.tif"));
        EXPECT_TRUE(poSrcDS != nullptr);
        auto tmpFilename = "/vsimem/tmp.tif";
        const char *options[] = {"-srcwin",
                                 "0",
                                 "0",
                                 "48",
                                 "32",
                                 "-ot",
                                 "UInt16",
                                 "-co",
                                 "TILED=YES",
                                 "-co",
                                 "BLOCKXSIZE=48",
                                 "-co",
                                 "BLOCKYSIZE=32",
                                 "-co",
                                 "INTERLEAVE=BAND",
                                 "-co",
                                 "ENDIANNESS=BIG",
                                 nullptr};
        auto psOptions =
            GDALTranslateOptionsNew(const_cast<char **>(options), nullptr);
        auto poDS(GDALDatasetUniquePtr(GDALDataset::FromHandle(
            GDALTranslate(tmpFilename, GDALDataset::ToHandle(poSrcDS.get()),
                          psOptions, nullptr))));
        GDALTranslateOptionsFree(psOptions);
        EXPECT_TRUE(poDS != nullptr);
        GDALDataset::RawBinaryLayout sLayout;
        EXPECT_TRUE(poDS->GetRawBinaryLayout(sLayout));
        EXPECT_EQ(sLayout.osRawFilename, poDS->GetDescription());
        EXPECT_EQ(
            static_cast<int>(sLayout.eInterleaving),
            static_cast<int>(GDALDataset::RawBinaryLayout::Interleaving::BSQ));
        EXPECT_EQ(sLayout.eDataType, GDT_UInt16);
        EXPECT_TRUE(!sLayout.bLittleEndianOrder);
        EXPECT_TRUE(sLayout.nImageOffset >= 408U);
        EXPECT_EQ(sLayout.nPixelOffset, 2);
        EXPECT_EQ(sLayout.nLineOffset, 2 * 48);
        EXPECT_EQ(sLayout.nBandOffset, 2 * 48 * 32);
        poDS.reset();
        VSIUnlink(tmpFilename);
    }
}

// Test GDALDataset::GetRawBinaryLayout() implementations
TEST_F(test_gdal, GetRawBinaryLayout_ISIS3)
{
    if (GDALGetDriverByName("ISIS3") == nullptr)
    {
        GTEST_SKIP() << "ISIS3 driver missing";
    }

    {
        GDALDatasetUniquePtr poDS(
            GDALDataset::Open(GDRIVERS_DATA_DIR "isis3/isis3_detached.lbl"));
        EXPECT_TRUE(poDS != nullptr);
        GDALDataset::RawBinaryLayout sLayout;
        EXPECT_TRUE(poDS->GetRawBinaryLayout(sLayout));
        EXPECT_TRUE(sLayout.osRawFilename.find("isis3_detached.cub") !=
                    std::string::npos);
        EXPECT_EQ(static_cast<int>(sLayout.eInterleaving),
                  static_cast<int>(
                      GDALDataset::RawBinaryLayout::Interleaving::UNKNOWN));
        EXPECT_EQ(sLayout.eDataType, GDT_Byte);
        EXPECT_TRUE(sLayout.bLittleEndianOrder);
        EXPECT_EQ(sLayout.nImageOffset, 0U);
        EXPECT_EQ(sLayout.nPixelOffset, 1);
        EXPECT_EQ(sLayout.nLineOffset, 317);
        // EXPECT_EQ( sLayout.nBandOffset, 9510 ); // doesn't matter on single
        // band
    }
}

// Test GDALDataset::GetRawBinaryLayout() implementations
TEST_F(test_gdal, GetRawBinaryLayout_VICAR)
{
    if (GDALGetDriverByName("VICAR") == nullptr)
    {
        GTEST_SKIP() << "VICAR driver missing";
    }

    {
        GDALDatasetUniquePtr poDS(GDALDataset::Open(
            GDRIVERS_DATA_DIR "vicar/test_vicar_truncated.bin"));
        EXPECT_TRUE(poDS != nullptr);
        GDALDataset::RawBinaryLayout sLayout;
        EXPECT_TRUE(poDS->GetRawBinaryLayout(sLayout));
        EXPECT_EQ(sLayout.osRawFilename, poDS->GetDescription());
        EXPECT_EQ(static_cast<int>(sLayout.eInterleaving),
                  static_cast<int>(
                      GDALDataset::RawBinaryLayout::Interleaving::UNKNOWN));
        EXPECT_EQ(sLayout.eDataType, GDT_Byte);
        EXPECT_TRUE(sLayout.bLittleEndianOrder);
        EXPECT_EQ(sLayout.nImageOffset, 9680U);
        EXPECT_EQ(sLayout.nPixelOffset, 1);
        EXPECT_EQ(sLayout.nLineOffset, 400);
        EXPECT_EQ(sLayout.nBandOffset, 0);  // doesn't matter on single band
    }
}

// Test GDALDataset::GetRawBinaryLayout() implementations
TEST_F(test_gdal, GetRawBinaryLayout_FITS)
{
    if (GDALGetDriverByName("FITS") == nullptr)
    {
        GTEST_SKIP() << "FITS driver missing";
    }

    {
        GDALDatasetUniquePtr poSrcDS(
            GDALDataset::Open(GCORE_DATA_DIR "int16.tif"));
        EXPECT_TRUE(poSrcDS != nullptr);
        CPLString tmpFilename(CPLGenerateTempFilename(nullptr));
        tmpFilename += ".fits";
        auto poDrv = GDALDriver::FromHandle(GDALGetDriverByName("FITS"));
        if (poDrv)
        {
            auto poDS(GDALDatasetUniquePtr(poDrv->CreateCopy(
                tmpFilename, poSrcDS.get(), false, nullptr, nullptr, nullptr)));
            EXPECT_TRUE(poDS != nullptr);
            poDS.reset();
            poDS.reset(GDALDataset::Open(tmpFilename));
            EXPECT_TRUE(poDS != nullptr);
            GDALDataset::RawBinaryLayout sLayout;
            EXPECT_TRUE(poDS->GetRawBinaryLayout(sLayout));
            EXPECT_EQ(sLayout.osRawFilename, poDS->GetDescription());
            EXPECT_EQ(static_cast<int>(sLayout.eInterleaving),
                      static_cast<int>(
                          GDALDataset::RawBinaryLayout::Interleaving::UNKNOWN));
            EXPECT_EQ(sLayout.eDataType, GDT_Int16);
            EXPECT_TRUE(!sLayout.bLittleEndianOrder);
            EXPECT_EQ(sLayout.nImageOffset, 2880U);
            EXPECT_EQ(sLayout.nPixelOffset, 2);
            EXPECT_EQ(sLayout.nLineOffset, 2 * 20);
            EXPECT_EQ(sLayout.nBandOffset, 2 * 20 * 20);
            poDS.reset();
            VSIUnlink(tmpFilename);
        }
    }
}

// Test GDALDataset::GetRawBinaryLayout() implementations
TEST_F(test_gdal, GetRawBinaryLayout_PDS)
{
    if (GDALGetDriverByName("PDS") == nullptr)
    {
        GTEST_SKIP() << "PDS driver missing";
    }

    {
        GDALDatasetUniquePtr poDS(
            GDALDataset::Open(GDRIVERS_DATA_DIR "pds/mc02_truncated.img"));
        EXPECT_TRUE(poDS != nullptr);
        GDALDataset::RawBinaryLayout sLayout;
        EXPECT_TRUE(poDS->GetRawBinaryLayout(sLayout));
        EXPECT_EQ(sLayout.osRawFilename, poDS->GetDescription());
        EXPECT_EQ(static_cast<int>(sLayout.eInterleaving),
                  static_cast<int>(
                      GDALDataset::RawBinaryLayout::Interleaving::UNKNOWN));
        EXPECT_EQ(sLayout.eDataType, GDT_Byte);
        EXPECT_TRUE(sLayout.bLittleEndianOrder);
        EXPECT_EQ(sLayout.nImageOffset, 3840U);
        EXPECT_EQ(sLayout.nPixelOffset, 1);
        EXPECT_EQ(sLayout.nLineOffset, 3840);
        EXPECT_EQ(sLayout.nBandOffset, 0);  // doesn't matter on single band
    }
}

// Test GDALDataset::GetRawBinaryLayout() implementations
TEST_F(test_gdal, GetRawBinaryLayout_PDS4)
{
    if (GDALGetDriverByName("PDS4") == nullptr)
    {
        GTEST_SKIP() << "PDS4 driver missing";
    }

    {
        GDALDatasetUniquePtr poDS(GDALDataset::Open(
            GDRIVERS_DATA_DIR "pds4/byte_pds4_cart_1700.xml"));
        EXPECT_TRUE(poDS != nullptr);
        GDALDataset::RawBinaryLayout sLayout;
        EXPECT_TRUE(poDS->GetRawBinaryLayout(sLayout));
        EXPECT_TRUE(sLayout.osRawFilename.find("byte_pds4_cart_1700.img") !=
                    std::string::npos);
        EXPECT_EQ(static_cast<int>(sLayout.eInterleaving),
                  static_cast<int>(
                      GDALDataset::RawBinaryLayout::Interleaving::UNKNOWN));
        EXPECT_EQ(sLayout.eDataType, GDT_Byte);
        EXPECT_TRUE(!sLayout.bLittleEndianOrder);
        EXPECT_EQ(sLayout.nImageOffset, 0U);
        EXPECT_EQ(sLayout.nPixelOffset, 1);
        EXPECT_EQ(sLayout.nLineOffset, 20);
        EXPECT_EQ(sLayout.nBandOffset, 0);  // doesn't matter on single band
    }
}

// Test TileMatrixSet
TEST_F(test_gdal, TileMatrixSet)
{
    if (getenv("SKIP_TILEMATRIXSET_TEST") != nullptr)
        GTEST_SKIP() << "Test skipped due to SKIP_TILEMATRIXSET_TEST being set";

    {
        auto l = gdal::TileMatrixSet::listPredefinedTileMatrixSets();
        EXPECT_TRUE(l.find("GoogleMapsCompatible") != l.end());
        EXPECT_TRUE(l.find("NZTM2000") != l.end());
    }

    {
        CPLPushErrorHandler(CPLQuietErrorHandler);
        EXPECT_TRUE(gdal::TileMatrixSet::parse("i_dont_exist") == nullptr);
        CPLPopErrorHandler();
    }

    {
        CPLErrorReset();
        CPLPushErrorHandler(CPLQuietErrorHandler);
        // Invalid JSON
        EXPECT_TRUE(gdal::TileMatrixSet::parse(
                        "http://127.0.0.1:32767/example.json") == nullptr);
        CPLPopErrorHandler();
        EXPECT_TRUE(CPLGetLastErrorType() != 0);
    }

    {
        CPLPushErrorHandler(CPLQuietErrorHandler);
        // Invalid JSON
        EXPECT_TRUE(gdal::TileMatrixSet::parse(
                        "{\"type\": \"TileMatrixSetType\" invalid") == nullptr);
        CPLPopErrorHandler();
    }

    {
        CPLPushErrorHandler(CPLQuietErrorHandler);
        // No tileMatrix
        EXPECT_TRUE(gdal::TileMatrixSet::parse(
                        "{\"type\": \"TileMatrixSetType\" }") == nullptr);
        CPLPopErrorHandler();
    }

    {
        auto poTMS = gdal::TileMatrixSet::parse("LINZAntarticaMapTileGrid");
        EXPECT_TRUE(poTMS != nullptr);
        if (poTMS)
        {
            EXPECT_TRUE(poTMS->haveAllLevelsSameTopLeft());
            EXPECT_TRUE(poTMS->haveAllLevelsSameTileSize());
            EXPECT_TRUE(poTMS->hasOnlyPowerOfTwoVaryingScales());
            EXPECT_TRUE(!poTMS->hasVariableMatrixWidth());
        }
    }

    {
        auto poTMS = gdal::TileMatrixSet::parse("NZTM2000");
        EXPECT_TRUE(poTMS != nullptr);
        if (poTMS)
        {
            EXPECT_TRUE(poTMS->haveAllLevelsSameTopLeft());
            EXPECT_TRUE(poTMS->haveAllLevelsSameTileSize());
            EXPECT_TRUE(!poTMS->hasOnlyPowerOfTwoVaryingScales());
            EXPECT_TRUE(!poTMS->hasVariableMatrixWidth());
        }
    }

    // Inline JSON with minimal structure
    {
        auto poTMS = gdal::TileMatrixSet::parse(
            "{\"type\": \"TileMatrixSetType\", \"supportedCRS\": "
            "\"http://www.opengis.net/def/crs/OGC/1.3/CRS84\", \"tileMatrix\": "
            "[{ \"topLeftCorner\": [-180, 90],\"scaleDenominator\":1.0}] }");
        EXPECT_TRUE(poTMS != nullptr);
        if (poTMS)
        {
            EXPECT_TRUE(poTMS->haveAllLevelsSameTopLeft());
            EXPECT_TRUE(poTMS->haveAllLevelsSameTileSize());
            EXPECT_TRUE(poTMS->hasOnlyPowerOfTwoVaryingScales());
            EXPECT_TRUE(!poTMS->hasVariableMatrixWidth());
        }
    }

    // Invalid scaleDenominator
    {
        CPLPushErrorHandler(CPLQuietErrorHandler);
        EXPECT_TRUE(gdal::TileMatrixSet::parse(
                        "{\"type\": \"TileMatrixSetType\", \"supportedCRS\": "
                        "\"http://www.opengis.net/def/crs/OGC/1.3/CRS84\", "
                        "\"tileMatrix\": [{ \"topLeftCorner\": [-180, "
                        "90],\"scaleDenominator\":0.0}] }") == nullptr);
        CPLPopErrorHandler();
    }

    {
        const char *pszJSON = "{"
                              "    \"type\": \"TileMatrixSetType\","
                              "    \"title\": \"CRS84 for the World\","
                              "    \"identifier\": \"WorldCRS84Quad\","
                              "    \"abstract\": \"my abstract\","
                              "    \"boundingBox\":"
                              "    {"
                              "        \"type\": \"BoundingBoxType\","
                              "        \"crs\": "
                              "\"http://www.opengis.net/def/crs/OGC/1.X/"
                              "CRS84\","  // 1.3 modified to 1.X to test
                                          // difference with supportedCRS
                              "        \"lowerCorner\": [-180, -90],"
                              "        \"upperCorner\": [180, 90]"
                              "    },"
                              "    \"supportedCRS\": "
                              "\"http://www.opengis.net/def/crs/OGC/1.3/"
                              "CRS84\","
                              "    \"wellKnownScaleSet\": "
                              "\"http://www.opengis.net/def/wkss/OGC/1.0/"
                              "GoogleCRS84Quad\","
                              "    \"tileMatrix\":"
                              "    ["
                              "        {"
                              "            \"type\": \"TileMatrixType\","
                              "            \"identifier\": \"0\","
                              "            \"scaleDenominator\": "
                              "279541132.014358,"
                              "            \"topLeftCorner\": [-180, 90],"
                              "            \"tileWidth\": 256,"
                              "            \"tileHeight\": 256,"
                              "            \"matrixWidth\": 2,"
                              "            \"matrixHeight\": 1"
                              "        },"
                              "        {"
                              "            \"type\": \"TileMatrixType\","
                              "            \"identifier\": \"1\","
                              "            \"scaleDenominator\": "
                              "139770566.007179,"
                              "            \"topLeftCorner\": [-180, 90],"
                              "            \"tileWidth\": 256,"
                              "            \"tileHeight\": 256,"
                              "            \"matrixWidth\": 4,"
                              "            \"matrixHeight\": 2"
                              "        }"
                              "    ]"
                              "}";
        VSIFCloseL(VSIFileFromMemBuffer(
            "/vsimem/tmp.json",
            reinterpret_cast<GByte *>(const_cast<char *>(pszJSON)),
            strlen(pszJSON), false));
        auto poTMS = gdal::TileMatrixSet::parse("/vsimem/tmp.json");
        VSIUnlink("/vsimem/tmp.json");

        EXPECT_TRUE(poTMS != nullptr);
        if (poTMS)
        {
            EXPECT_EQ(poTMS->title(), "CRS84 for the World");
            EXPECT_EQ(poTMS->identifier(), "WorldCRS84Quad");
            EXPECT_EQ(poTMS->abstract(), "my abstract");
            EXPECT_EQ(poTMS->crs(),
                      "http://www.opengis.net/def/crs/OGC/1.3/CRS84");
            EXPECT_EQ(
                poTMS->wellKnownScaleSet(),
                "http://www.opengis.net/def/wkss/OGC/1.0/GoogleCRS84Quad");
            EXPECT_EQ(poTMS->bbox().mCrs,
                      "http://www.opengis.net/def/crs/OGC/1.X/CRS84");
            EXPECT_EQ(poTMS->bbox().mLowerCornerX, -180.0);
            EXPECT_EQ(poTMS->bbox().mLowerCornerY, -90.0);
            EXPECT_EQ(poTMS->bbox().mUpperCornerX, 180.0);
            EXPECT_EQ(poTMS->bbox().mUpperCornerY, 90.0);
            ASSERT_EQ(poTMS->tileMatrixList().size(), 2U);
            EXPECT_TRUE(poTMS->haveAllLevelsSameTopLeft());
            EXPECT_TRUE(poTMS->haveAllLevelsSameTileSize());
            EXPECT_TRUE(poTMS->hasOnlyPowerOfTwoVaryingScales());
            EXPECT_TRUE(!poTMS->hasVariableMatrixWidth());
            const auto &tm = poTMS->tileMatrixList()[0];
            EXPECT_EQ(tm.mId, "0");
            EXPECT_EQ(tm.mScaleDenominator, 279541132.014358);
            EXPECT_TRUE(fabs(tm.mResX - tm.mScaleDenominator * 0.28e-3 /
                                            (6378137. * M_PI / 180)) < 1e-10);
            EXPECT_TRUE(fabs(tm.mResX - 180. / 256) < 1e-10);
            EXPECT_EQ(tm.mResY, tm.mResX);
            EXPECT_EQ(tm.mTopLeftX, -180.0);
            EXPECT_EQ(tm.mTopLeftY, 90.0);
            EXPECT_EQ(tm.mTileWidth, 256);
            EXPECT_EQ(tm.mTileHeight, 256);
            EXPECT_EQ(tm.mMatrixWidth, 2);
            EXPECT_EQ(tm.mMatrixHeight, 1);
        }
    }

    {
        auto poTMS = gdal::TileMatrixSet::parse(
            "{"
            "    \"type\": \"TileMatrixSetType\","
            "    \"title\": \"CRS84 for the World\","
            "    \"identifier\": \"WorldCRS84Quad\","
            "    \"boundingBox\":"
            "    {"
            "        \"type\": \"BoundingBoxType\","
            "        \"crs\": "
            "\"http://www.opengis.net/def/crs/OGC/1.X/"
            "CRS84\","  // 1.3 modified to 1.X to test
                        // difference with supportedCRS
            "        \"lowerCorner\": [-180, -90],"
            "        \"upperCorner\": [180, 90]"
            "    },"
            "    \"supportedCRS\": "
            "\"http://www.opengis.net/def/crs/OGC/1.3/"
            "CRS84\","
            "    \"wellKnownScaleSet\": "
            "\"http://www.opengis.net/def/wkss/OGC/1.0/"
            "GoogleCRS84Quad\","
            "    \"tileMatrix\":"
            "    ["
            "        {"
            "            \"type\": \"TileMatrixType\","
            "            \"identifier\": \"0\","
            "            \"scaleDenominator\": "
            "279541132.014358,"
            "            \"topLeftCorner\": [-180, 90],"
            "            \"tileWidth\": 256,"
            "            \"tileHeight\": 256,"
            "            \"matrixWidth\": 2,"
            "            \"matrixHeight\": 1"
            "        },"
            "        {"
            "            \"type\": \"TileMatrixType\","
            "            \"identifier\": \"1\","
            "            \"scaleDenominator\": 100000000,"
            "            \"topLeftCorner\": [-123, 90],"
            "            \"tileWidth\": 128,"
            "            \"tileHeight\": 256,"
            "            \"matrixWidth\": 4,"
            "            \"matrixHeight\": 2,"
            "            \"variableMatrixWidth\": [{"
            "               \"type\": "
            "\"VariableMatrixWidthType\","
            "               \"coalesce\" : 2,"
            "               \"minTileRow\": 0,"
            "               \"maxTileRow\": 1"
            "            }]"
            "        }"
            "    ]"
            "}");
        EXPECT_TRUE(poTMS != nullptr);
        if (poTMS)
        {
            ASSERT_EQ(poTMS->tileMatrixList().size(), 2U);
            EXPECT_TRUE(!poTMS->haveAllLevelsSameTopLeft());
            EXPECT_TRUE(!poTMS->haveAllLevelsSameTileSize());
            EXPECT_TRUE(!poTMS->hasOnlyPowerOfTwoVaryingScales());
            EXPECT_TRUE(poTMS->hasVariableMatrixWidth());
            const auto &tm = poTMS->tileMatrixList()[1];
            EXPECT_EQ(tm.mVariableMatrixWidthList.size(), 1U);
            const auto &vmw = tm.mVariableMatrixWidthList[0];
            EXPECT_EQ(vmw.mCoalesce, 2);
            EXPECT_EQ(vmw.mMinTileRow, 0);
            EXPECT_EQ(vmw.mMaxTileRow, 1);
        }
    }

    {
        auto poTMS = gdal::TileMatrixSet::parse(
            "{"
            "    \"identifier\" : \"CDBGlobalGrid\","
            "    \"title\" : \"CDBGlobalGrid\","
            "    \"boundingBox\" : {"
            "        \"crs\" : \"http://www.opengis.net/def/crs/EPSG/0/4326\","
            "        \"lowerCorner\" : ["
            "            -90,"
            "            -180"
            "        ],"
            "        \"upperCorner\" : ["
            "            90,"
            "            180"
            "        ]"
            "    },"
            "    \"supportedCRS\" : "
            "\"http://www.opengis.net/def/crs/EPSG/0/4326\","
            "    \"wellKnownScaleSet\" : "
            "\"http://www.opengis.net/def/wkss/OGC/1.0/CDBGlobalGrid\","
            "    \"tileMatrix\" : ["
            "        {"
            "            \"identifier\" : \"-10\","
            "            \"scaleDenominator\" : 397569609.975977063179,"
            "            \"matrixWidth\" : 360,"
            "            \"matrixHeight\" : 180,"
            "            \"tileWidth\" : 1,"
            "            \"tileHeight\" : 1,"
            "            \"topLeftCorner\" : ["
            "                90,"
            "                -180"
            "            ],"
            "            \"variableMatrixWidth\" : ["
            "                {"
            "                \"coalesce\" : 12,"
            "                \"minTileRow\" : 0,"
            "                \"maxTileRow\" : 0"
            "                },"
            "                {"
            "                \"coalesce\" : 12,"
            "                \"minTileRow\" : 179,"
            "                \"maxTileRow\" : 179"
            "                }"
            "            ]"
            "        }"
            "    ]"
            "}");
        EXPECT_TRUE(poTMS != nullptr);
        if (poTMS)
        {
            ASSERT_EQ(poTMS->tileMatrixList().size(), 1U);
            const auto &tm = poTMS->tileMatrixList()[0];
            EXPECT_EQ(tm.mVariableMatrixWidthList.size(), 2U);
            const auto &vmw = tm.mVariableMatrixWidthList[0];
            EXPECT_EQ(vmw.mCoalesce, 12);
            EXPECT_EQ(vmw.mMinTileRow, 0);
            EXPECT_EQ(vmw.mMaxTileRow, 0);
        }
    }

    // TMS v2 (truncated version of https://maps.gnosis.earth/ogcapi/tileMatrixSets/GNOSISGlobalGrid?f=json)
    {
        auto poTMS = gdal::TileMatrixSet::parse(
            "{"
            "   \"id\" : \"GNOSISGlobalGrid\","
            "   \"title\" : \"GNOSISGlobalGrid\","
            "   \"uri\" : "
            "\"http://www.opengis.net/def/tilematrixset/OGC/1.0/"
            "GNOSISGlobalGrid\","
            "   \"description\": \"added for testing\","
            "   \"crs\" : \"http://www.opengis.net/def/crs/EPSG/0/4326\","
            "   \"orderedAxes\" : ["
            "      \"Lat\","
            "      \"Lon\""
            "   ],"
            "   \"wellKnownScaleSet\" : "
            "\"http://www.opengis.net/def/wkss/OGC/1.0/GoogleCRS84Quad\","
            "   \"tileMatrices\" : ["
            "      {"
            "         \"id\" : \"0\","
            "         \"scaleDenominator\" : 139770566.0071794390678,"
            "         \"cellSize\" : 0.3515625,"
            "         \"cornerOfOrigin\" : \"topLeft\","
            "         \"pointOfOrigin\" : [ 90, -180 ],"
            "         \"matrixWidth\" : 4,"
            "         \"matrixHeight\" : 2,"
            "         \"tileWidth\" : 256,"
            "         \"tileHeight\" : 256"
            "      },"
            "      {"
            "         \"id\" : \"1\","
            "         \"scaleDenominator\" : 69885283.0035897195339,"
            "         \"cellSize\" : 0.17578125,"
            "         \"cornerOfOrigin\" : \"topLeft\","
            "         \"pointOfOrigin\" : [ 90, -180 ],"
            "         \"matrixWidth\" : 8,"
            "         \"matrixHeight\" : 4,"
            "         \"tileWidth\" : 256,"
            "         \"tileHeight\" : 256,"
            "         \"variableMatrixWidths\" : ["
            "            { \"coalesce\" : 2, \"minTileRow\" : 0, "
            "\"maxTileRow\" : 0 },"
            "            { \"coalesce\" : 2, \"minTileRow\" : 3, "
            "\"maxTileRow\" : 3 }"
            "         ]"
            "      }"
            "   ]"
            "}");
        EXPECT_TRUE(poTMS != nullptr);
        if (poTMS)
        {
            EXPECT_EQ(poTMS->title(), "GNOSISGlobalGrid");
            EXPECT_EQ(poTMS->identifier(), "GNOSISGlobalGrid");
            EXPECT_EQ(poTMS->abstract(), "added for testing");
            EXPECT_EQ(poTMS->crs(),
                      "http://www.opengis.net/def/crs/EPSG/0/4326");
            EXPECT_EQ(
                poTMS->wellKnownScaleSet(),
                "http://www.opengis.net/def/wkss/OGC/1.0/GoogleCRS84Quad");
            ASSERT_EQ(poTMS->tileMatrixList().size(), 2U);
            EXPECT_TRUE(poTMS->haveAllLevelsSameTopLeft());
            EXPECT_TRUE(poTMS->haveAllLevelsSameTileSize());
            EXPECT_TRUE(poTMS->hasOnlyPowerOfTwoVaryingScales());
            {
                const auto &tm = poTMS->tileMatrixList()[0];
                EXPECT_EQ(tm.mId, "0");
                EXPECT_EQ(tm.mScaleDenominator, 139770566.0071794390678);
                EXPECT_TRUE(fabs(tm.mResX - tm.mScaleDenominator * 0.28e-3 /
                                                (6378137. * M_PI / 180)) <
                            1e-10);
                EXPECT_EQ(tm.mResY, tm.mResX);
                EXPECT_EQ(tm.mTopLeftX, 90.0);
                EXPECT_EQ(tm.mTopLeftY, -180.0);
                EXPECT_EQ(tm.mTileWidth, 256);
                EXPECT_EQ(tm.mTileHeight, 256);
                EXPECT_EQ(tm.mMatrixWidth, 4);
                EXPECT_EQ(tm.mMatrixHeight, 2);
            }

            EXPECT_TRUE(poTMS->hasVariableMatrixWidth());
            {
                const auto &tm = poTMS->tileMatrixList()[1];
                EXPECT_EQ(tm.mVariableMatrixWidthList.size(), 2U);
                const auto &vmw = tm.mVariableMatrixWidthList[1];
                EXPECT_EQ(vmw.mCoalesce, 2);
                EXPECT_EQ(vmw.mMinTileRow, 3);
                EXPECT_EQ(vmw.mMaxTileRow, 3);
            }
        }
    }

    // TMS v2 with crs.uri
    {
        auto poTMS = gdal::TileMatrixSet::parse(
            "{"
            "   \"id\" : \"test\","
            "   \"title\" : \"test\","
            "   \"uri\" : "
            "\"http://www.opengis.net/def/tilematrixset/OGC/1.0/test\","
            "   \"crs\" : {\"uri\": "
            "\"http://www.opengis.net/def/crs/EPSG/0/4326\"},"
            "   \"orderedAxes\" : ["
            "      \"Lat\","
            "      \"Lon\""
            "   ],"
            "   \"wellKnownScaleSet\" : "
            "\"http://www.opengis.net/def/wkss/OGC/1.0/GoogleCRS84Quad\","
            "   \"tileMatrices\" : ["
            "      {"
            "         \"id\" : \"0\","
            "         \"scaleDenominator\" : 139770566.0071794390678,"
            "         \"cellSize\" : 0.3515625,"
            "         \"cornerOfOrigin\" : \"topLeft\","
            "         \"pointOfOrigin\" : [ 90, -180 ],"
            "         \"matrixWidth\" : 4,"
            "         \"matrixHeight\" : 2,"
            "         \"tileWidth\" : 256,"
            "         \"tileHeight\" : 256"
            "      }"
            "   ]"
            "}");
        EXPECT_TRUE(poTMS != nullptr);
        if (poTMS)
        {
            EXPECT_EQ(poTMS->crs(),
                      "http://www.opengis.net/def/crs/EPSG/0/4326");
        }
    }

    // TMS v2 with crs.wkt
    {
        auto poTMS = gdal::TileMatrixSet::parse(
            "{"
            "   \"id\" : \"test\","
            "   \"title\" : \"test\","
            "   \"uri\" : "
            "\"http://www.opengis.net/def/tilematrixset/OGC/1.0/test\","
            "   \"crs\" : {\"wkt\": \"GEOGCRS[\\\"WGS 84\\\","
            "ENSEMBLE[\\\"World Geodetic System 1984 ensemble\\\","
            "MEMBER[\\\"World Geodetic System 1984 (Transit)\\\"],"
            "MEMBER[\\\"World Geodetic System 1984 (G730)\\\"],"
            "MEMBER[\\\"World Geodetic System 1984 (G873)\\\"],"
            "MEMBER[\\\"World Geodetic System 1984 (G1150)\\\"],"
            "MEMBER[\\\"World Geodetic System 1984 (G1674)\\\"],"
            "MEMBER[\\\"World Geodetic System 1984 (G1762)\\\"],"
            "MEMBER[\\\"World Geodetic System 1984 (G2139)\\\"],"
            "MEMBER[\\\"World Geodetic System 1984 (G2296)\\\"],"
            "ELLIPSOID[\\\"WGS 84\\\",6378137,298.257223563,"
            "LENGTHUNIT[\\\"metre\\\",1]],"
            "ENSEMBLEACCURACY[2.0]],"
            "PRIMEM[\\\"Greenwich\\\",0,"
            "ANGLEUNIT[\\\"degree\\\",0.0174532925199433]],"
            "CS[ellipsoidal,2],"
            "AXIS[\\\"geodetic latitude (Lat)\\\",north,"
            "ORDER[1],"
            "ANGLEUNIT[\\\"degree\\\",0.0174532925199433]],"
            "AXIS[\\\"geodetic longitude (Lon)\\\",east,"
            "ORDER[2],"
            "ANGLEUNIT[\\\"degree\\\",0.0174532925199433]],"
            "USAGE["
            "SCOPE[\\\"Horizontal component of 3D system.\\\"],"
            "AREA[\\\"World.\\\"],"
            "BBOX[-90,-180,90,180]],"
            "ID[\\\"EPSG\\\",4326]]\" },"
            "   \"orderedAxes\" : ["
            "      \"Lat\","
            "      \"Lon\""
            "   ],"
            "   \"wellKnownScaleSet\" : "
            "\"http://www.opengis.net/def/wkss/OGC/1.0/GoogleCRS84Quad\","
            "   \"tileMatrices\" : ["
            "      {"
            "         \"id\" : \"0\","
            "         \"scaleDenominator\" : 139770566.0071794390678,"
            "         \"cellSize\" : 0.3515625,"
            "         \"cornerOfOrigin\" : \"topLeft\","
            "         \"pointOfOrigin\" : [ 90, -180 ],"
            "         \"matrixWidth\" : 4,"
            "         \"matrixHeight\" : 2,"
            "         \"tileWidth\" : 256,"
            "         \"tileHeight\" : 256"
            "      }"
            "   ]"
            "}");
        EXPECT_TRUE(poTMS != nullptr);
        if (poTMS)
        {
            EXPECT_TRUE(
                STARTS_WITH(poTMS->crs().c_str(), "GEOGCRS[\"WGS 84\""));
        }
    }

    // TMS v2 with crs.wkt with JSON content
    {
        auto poTMS = gdal::TileMatrixSet::parse(
            "{"
            "   \"id\" : \"test\","
            "   \"title\" : \"test\","
            "   \"uri\" : "
            "\"http://www.opengis.net/def/tilematrixset/OGC/1.0/test\","
            "   \"crs\" : {\"wkt\": "
            "{"
            "  \"type\": \"GeographicCRS\","
            "  \"name\": \"WGS 84\","
            "  \"datum_ensemble\": {"
            "    \"name\": \"World Geodetic System 1984 ensemble\","
            "    \"members\": ["
            "      {"
            "        \"name\": \"World Geodetic System 1984 (Transit)\","
            "        \"id\": {"
            "          \"authority\": \"EPSG\","
            "          \"code\": 1166"
            "        }"
            "      },"
            "      {"
            "        \"name\": \"World Geodetic System 1984 (G730)\","
            "        \"id\": {"
            "          \"authority\": \"EPSG\","
            "          \"code\": 1152"
            "        }"
            "      },"
            "      {"
            "        \"name\": \"World Geodetic System 1984 (G873)\","
            "        \"id\": {"
            "          \"authority\": \"EPSG\","
            "          \"code\": 1153"
            "        }"
            "      },"
            "      {"
            "        \"name\": \"World Geodetic System 1984 (G1150)\","
            "        \"id\": {"
            "          \"authority\": \"EPSG\","
            "          \"code\": 1154"
            "        }"
            "      },"
            "      {"
            "        \"name\": \"World Geodetic System 1984 (G1674)\","
            "        \"id\": {"
            "          \"authority\": \"EPSG\","
            "          \"code\": 1155"
            "        }"
            "      },"
            "      {"
            "        \"name\": \"World Geodetic System 1984 (G1762)\","
            "        \"id\": {"
            "          \"authority\": \"EPSG\","
            "          \"code\": 1156"
            "        }"
            "      },"
            "      {"
            "        \"name\": \"World Geodetic System 1984 (G2139)\","
            "        \"id\": {"
            "          \"authority\": \"EPSG\","
            "          \"code\": 1309"
            "        }"
            "      },"
            "      {"
            "        \"name\": \"World Geodetic System 1984 (G2296)\","
            "        \"id\": {"
            "          \"authority\": \"EPSG\","
            "          \"code\": 1383"
            "        }"
            "      }"
            "    ],"
            "    \"ellipsoid\": {"
            "      \"name\": \"WGS 84\","
            "      \"semi_major_axis\": 6378137,"
            "      \"inverse_flattening\": 298.257223563"
            "    },"
            "    \"accuracy\": \"2.0\","
            "    \"id\": {"
            "      \"authority\": \"EPSG\","
            "      \"code\": 6326"
            "    }"
            "  },"
            "  \"coordinate_system\": {"
            "    \"subtype\": \"ellipsoidal\","
            "    \"axis\": ["
            "      {"
            "        \"name\": \"Geodetic latitude\","
            "        \"abbreviation\": \"Lat\","
            "        \"direction\": \"north\","
            "        \"unit\": \"degree\""
            "      },"
            "      {"
            "        \"name\": \"Geodetic longitude\","
            "        \"abbreviation\": \"Lon\","
            "        \"direction\": \"east\","
            "        \"unit\": \"degree\""
            "      }"
            "    ]"
            "  },"
            "  \"scope\": \"Horizontal component of 3D system.\","
            "  \"area\": \"World.\","
            "  \"bbox\": {"
            "    \"south_latitude\": -90,"
            "    \"west_longitude\": -180,"
            "    \"north_latitude\": 90,"
            "    \"east_longitude\": 180"
            "  },"
            "  \"id\": {"
            "    \"authority\": \"EPSG\","
            "    \"code\": 4326"
            "  }"
            "}"
            "},"
            "   \"orderedAxes\" : ["
            "      \"Lat\","
            "      \"Lon\""
            "   ],"
            "   \"wellKnownScaleSet\" : "
            "\"http://www.opengis.net/def/wkss/OGC/1.0/GoogleCRS84Quad\","
            "   \"tileMatrices\" : ["
            "      {"
            "         \"id\" : \"0\","
            "         \"scaleDenominator\" : 139770566.0071794390678,"
            "         \"cellSize\" : 0.3515625,"
            "         \"cornerOfOrigin\" : \"topLeft\","
            "         \"pointOfOrigin\" : [ 90, -180 ],"
            "         \"matrixWidth\" : 4,"
            "         \"matrixHeight\" : 2,"
            "         \"tileWidth\" : 256,"
            "         \"tileHeight\" : 256"
            "      }"
            "   ]"
            "}");
        EXPECT_TRUE(poTMS != nullptr);
        if (poTMS)
        {
            EXPECT_TRUE(STARTS_WITH(poTMS->crs().c_str(),
                                    "{ \"type\": \"GeographicCRS\""));
        }
    }
}

// Test that PCIDSK GetMetadataItem() return is stable
TEST_F(test_gdal, PCIDSK_GetMetadataItem)
{
    auto poDrv = GDALDriver::FromHandle(GDALGetDriverByName("PCIDSK"));
    if (poDrv == nullptr)
        GTEST_SKIP() << "PCIDSK driver missing";

    GDALDatasetUniquePtr poDS(
        poDrv->Create("/vsimem/tmp.pix", 1, 1, 1, GDT_Byte, nullptr));
    EXPECT_TRUE(poDS != nullptr);
    poDS->SetMetadataItem("FOO", "BAR");
    poDS->SetMetadataItem("BAR", "BAZ");
    poDS->GetRasterBand(1)->SetMetadataItem("FOO", "BAR");
    poDS->GetRasterBand(1)->SetMetadataItem("BAR", "BAZ");

    {
        const char *psz1 = poDS->GetMetadataItem("FOO");
        const char *psz2 = poDS->GetMetadataItem("BAR");
        const char *pszNull = poDS->GetMetadataItem("I_DONT_EXIST");
        const char *psz3 = poDS->GetMetadataItem("FOO");
        const char *pszNull2 = poDS->GetMetadataItem("I_DONT_EXIST");
        const char *psz4 = poDS->GetMetadataItem("BAR");
        EXPECT_TRUE(psz1 != nullptr);
        EXPECT_TRUE(psz2 != nullptr);
        EXPECT_TRUE(psz3 != nullptr);
        EXPECT_TRUE(psz4 != nullptr);
        EXPECT_TRUE(pszNull == nullptr);
        EXPECT_TRUE(pszNull2 == nullptr);
        EXPECT_EQ(psz1, psz3);
        EXPECT_TRUE(psz1 != psz2);
        EXPECT_EQ(psz2, psz4);
        EXPECT_STREQ(psz1, "BAR");
        EXPECT_STREQ(psz2, "BAZ");
    }

    {
        auto poBand = poDS->GetRasterBand(1);
        const char *psz1 = poBand->GetMetadataItem("FOO");
        const char *psz2 = poBand->GetMetadataItem("BAR");
        const char *pszNull = poBand->GetMetadataItem("I_DONT_EXIST");
        const char *psz3 = poBand->GetMetadataItem("FOO");
        const char *pszNull2 = poBand->GetMetadataItem("I_DONT_EXIST");
        const char *psz4 = poBand->GetMetadataItem("BAR");
        EXPECT_TRUE(psz1 != nullptr);
        EXPECT_TRUE(psz2 != nullptr);
        EXPECT_TRUE(psz3 != nullptr);
        EXPECT_TRUE(psz4 != nullptr);
        EXPECT_TRUE(pszNull == nullptr);
        EXPECT_TRUE(pszNull2 == nullptr);
        EXPECT_EQ(psz1, psz3);
        EXPECT_TRUE(psz1 != psz2);
        EXPECT_EQ(psz2, psz4);
        EXPECT_STREQ(psz1, "BAR");
        EXPECT_STREQ(psz2, "BAZ");
    }

    poDS.reset();
    VSIUnlink("/vsimem/tmp.pix");
}

// Test GDALBufferHasOnlyNoData()
TEST_F(test_gdal, GDALBufferHasOnlyNoData)
{
    /* bool CPL_DLL GDALBufferHasOnlyNoData(const void* pBuffer,
                                 double dfNoDataValue,
                                 size_t nWidth, size_t nHeight,
                                 size_t nLineStride,
                                 size_t nComponents,
                                 int nBitsPerSample,
                                 GDALBufferSampleFormat nSampleFormat);
     */
    EXPECT_TRUE(
        GDALBufferHasOnlyNoData("\x00", 0.0, 1, 1, 1, 1, 8, GSF_UNSIGNED_INT));
    EXPECT_TRUE(
        !GDALBufferHasOnlyNoData("\x01", 0.0, 1, 1, 1, 1, 8, GSF_UNSIGNED_INT));
    EXPECT_TRUE(
        GDALBufferHasOnlyNoData("\x00", 0.0, 1, 1, 1, 1, 1, GSF_UNSIGNED_INT));
    EXPECT_TRUE(GDALBufferHasOnlyNoData("\x00\x00", 0.0, 1, 1, 1, 1, 16,
                                        GSF_UNSIGNED_INT));
    EXPECT_TRUE(!GDALBufferHasOnlyNoData("\x00\x01", 0.0, 1, 1, 1, 1, 16,
                                         GSF_UNSIGNED_INT));
    EXPECT_TRUE(GDALBufferHasOnlyNoData("\x00\x01", 0.0, 1, 2, 2, 1, 8,
                                        GSF_UNSIGNED_INT));
    EXPECT_TRUE(GDALBufferHasOnlyNoData(
        "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 0.0, 14, 1,
        14, 1, 8, GSF_UNSIGNED_INT));
    EXPECT_TRUE(!GDALBufferHasOnlyNoData(
        "\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 0.0, 14, 1,
        14, 1, 8, GSF_UNSIGNED_INT));
    EXPECT_TRUE(!GDALBufferHasOnlyNoData(
        "\x00\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00", 0.0, 14, 1,
        14, 1, 8, GSF_UNSIGNED_INT));
    EXPECT_TRUE(!GDALBufferHasOnlyNoData(
        "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01", 0.0, 14, 1,
        14, 1, 8, GSF_UNSIGNED_INT));

    uint8_t uint8val = 1;
    EXPECT_TRUE(GDALBufferHasOnlyNoData(&uint8val, 1.0, 1, 1, 1, 1, 8,
                                        GSF_UNSIGNED_INT));
    EXPECT_TRUE(!GDALBufferHasOnlyNoData(&uint8val, 0.0, 1, 1, 1, 1, 8,
                                         GSF_UNSIGNED_INT));
    EXPECT_TRUE(!GDALBufferHasOnlyNoData(&uint8val, 128 + 1, 1, 1, 1, 1, 8,
                                         GSF_UNSIGNED_INT));

    int8_t int8val = -1;
    EXPECT_TRUE(
        GDALBufferHasOnlyNoData(&int8val, -1.0, 1, 1, 1, 1, 8, GSF_SIGNED_INT));
    EXPECT_TRUE(
        !GDALBufferHasOnlyNoData(&int8val, 0.0, 1, 1, 1, 1, 8, GSF_SIGNED_INT));
    EXPECT_TRUE(
        !GDALBufferHasOnlyNoData(&int8val, 256, 1, 1, 1, 1, 8, GSF_SIGNED_INT));

    uint16_t uint16val = 1;
    EXPECT_TRUE(GDALBufferHasOnlyNoData(&uint16val, 1.0, 1, 1, 1, 1, 16,
                                        GSF_UNSIGNED_INT));
    EXPECT_TRUE(!GDALBufferHasOnlyNoData(&uint16val, 0.0, 1, 1, 1, 1, 16,
                                         GSF_UNSIGNED_INT));
    EXPECT_TRUE(!GDALBufferHasOnlyNoData(&uint16val, 65536 + 1, 1, 1, 1, 1, 16,
                                         GSF_UNSIGNED_INT));

    int16_t int16val = -1;
    EXPECT_TRUE(GDALBufferHasOnlyNoData(&int16val, -1.0, 1, 1, 1, 1, 16,
                                        GSF_SIGNED_INT));
    EXPECT_TRUE(!GDALBufferHasOnlyNoData(&int16val, 0.0, 1, 1, 1, 1, 16,
                                         GSF_SIGNED_INT));
    EXPECT_TRUE(!GDALBufferHasOnlyNoData(&int16val, 32768, 1, 1, 1, 1, 16,
                                         GSF_SIGNED_INT));

    uint32_t uint32val = 1;
    EXPECT_TRUE(GDALBufferHasOnlyNoData(&uint32val, 1.0, 1, 1, 1, 1, 32,
                                        GSF_UNSIGNED_INT));
    EXPECT_TRUE(!GDALBufferHasOnlyNoData(&uint32val, 0.0, 1, 1, 1, 1, 32,
                                         GSF_UNSIGNED_INT));
    EXPECT_TRUE(!GDALBufferHasOnlyNoData(&uint32val,
                                         static_cast<double>(0x100000000LL + 1),
                                         1, 1, 1, 1, 32, GSF_UNSIGNED_INT));

    int32_t int32val = -1;
    EXPECT_TRUE(GDALBufferHasOnlyNoData(&int32val, -1.0, 1, 1, 1, 1, 32,
                                        GSF_SIGNED_INT));
    EXPECT_TRUE(!GDALBufferHasOnlyNoData(&int32val, 0.0, 1, 1, 1, 1, 32,
                                         GSF_SIGNED_INT));
    EXPECT_TRUE(!GDALBufferHasOnlyNoData(&int32val, 0x80000000, 1, 1, 1, 1, 32,
                                         GSF_SIGNED_INT));

    float float32val = -1;
    EXPECT_TRUE(GDALBufferHasOnlyNoData(&float32val, -1.0, 1, 1, 1, 1, 32,
                                        GSF_FLOATING_POINT));
    EXPECT_TRUE(!GDALBufferHasOnlyNoData(&float32val, 0.0, 1, 1, 1, 1, 32,
                                         GSF_FLOATING_POINT));
    EXPECT_TRUE(!GDALBufferHasOnlyNoData(&float32val, 1e50, 1, 1, 1, 1, 32,
                                         GSF_FLOATING_POINT));

    float float32nan = std::numeric_limits<float>::quiet_NaN();
    EXPECT_TRUE(GDALBufferHasOnlyNoData(&float32nan, float32nan, 1, 1, 1, 1, 32,
                                        GSF_FLOATING_POINT));
    EXPECT_TRUE(!GDALBufferHasOnlyNoData(&float32nan, 0.0, 1, 1, 1, 1, 32,
                                         GSF_FLOATING_POINT));

    double float64val = -1;
    EXPECT_TRUE(GDALBufferHasOnlyNoData(&float64val, -1.0, 1, 1, 1, 1, 64,
                                        GSF_FLOATING_POINT));
    EXPECT_TRUE(!GDALBufferHasOnlyNoData(&float64val, 0.0, 1, 1, 1, 1, 64,
                                         GSF_FLOATING_POINT));

    double float64nan = std::numeric_limits<double>::quiet_NaN();
    EXPECT_TRUE(GDALBufferHasOnlyNoData(&float64nan, float64nan, 1, 1, 1, 1, 64,
                                        GSF_FLOATING_POINT));
    EXPECT_TRUE(!GDALBufferHasOnlyNoData(&float64nan, 0.0, 1, 1, 1, 1, 64,
                                         GSF_FLOATING_POINT));
}

// Test GetRasterNoDataReplacementValue()
TEST_F(test_gdal, GetRasterNoDataReplacementValue)
{
    // Test GDT_Byte
    EXPECT_EQ(GDALGetNoDataReplacementValue(
                  GDT_Byte, std::numeric_limits<double>::lowest()),
              0);
    EXPECT_EQ(GDALGetNoDataReplacementValue(GDT_Byte,
                                            std::numeric_limits<double>::max()),
              0);
    EXPECT_EQ(GDALGetNoDataReplacementValue(
                  GDT_Byte, std::numeric_limits<uint8_t>::lowest()),
              std::numeric_limits<uint8_t>::lowest() + 1);
    EXPECT_EQ(GDALGetNoDataReplacementValue(
                  GDT_Byte, std::numeric_limits<uint8_t>::max()),
              std::numeric_limits<uint8_t>::max() - 1);

    // Test GDT_Int8
    EXPECT_EQ(GDALGetNoDataReplacementValue(
                  GDT_Int8, std::numeric_limits<double>::lowest()),
              0);
    EXPECT_EQ(GDALGetNoDataReplacementValue(GDT_Int8,
                                            std::numeric_limits<double>::max()),
              0);
    EXPECT_EQ(GDALGetNoDataReplacementValue(
                  GDT_Int8, std::numeric_limits<int8_t>::lowest()),
              std::numeric_limits<int8_t>::lowest() + 1);
    EXPECT_EQ(GDALGetNoDataReplacementValue(GDT_Int8,
                                            std::numeric_limits<int8_t>::max()),
              std::numeric_limits<int8_t>::max() - 1);

    // Test GDT_UInt16
    EXPECT_EQ(GDALGetNoDataReplacementValue(
                  GDT_UInt16, std::numeric_limits<double>::lowest()),
              0);
    EXPECT_EQ(GDALGetNoDataReplacementValue(GDT_UInt16,
                                            std::numeric_limits<double>::max()),
              0);
    EXPECT_EQ(GDALGetNoDataReplacementValue(
                  GDT_UInt16, std::numeric_limits<uint16_t>::lowest()),
              std::numeric_limits<uint16_t>::lowest() + 1);
    EXPECT_EQ(GDALGetNoDataReplacementValue(
                  GDT_UInt16, std::numeric_limits<uint16_t>::max()),
              std::numeric_limits<uint16_t>::max() - 1);

    // Test GDT_Int16
    EXPECT_EQ(GDALGetNoDataReplacementValue(
                  GDT_Int16, std::numeric_limits<double>::lowest()),
              0);
    EXPECT_EQ(GDALGetNoDataReplacementValue(GDT_Int16,
                                            std::numeric_limits<double>::max()),
              0);
    EXPECT_EQ(GDALGetNoDataReplacementValue(
                  GDT_Int16, std::numeric_limits<int16_t>::lowest()),
              std::numeric_limits<int16_t>::lowest() + 1);
    EXPECT_EQ(GDALGetNoDataReplacementValue(
                  GDT_Int16, std::numeric_limits<int16_t>::max()),
              std::numeric_limits<int16_t>::max() - 1);

    // Test GDT_UInt32
    EXPECT_EQ(GDALGetNoDataReplacementValue(
                  GDT_UInt32, std::numeric_limits<double>::lowest()),
              0);
    EXPECT_EQ(GDALGetNoDataReplacementValue(GDT_UInt32,
                                            std::numeric_limits<double>::max()),
              0);
    EXPECT_EQ(GDALGetNoDataReplacementValue(
                  GDT_UInt32, std::numeric_limits<uint32_t>::lowest()),
              std::numeric_limits<uint32_t>::lowest() + 1);
    EXPECT_EQ(GDALGetNoDataReplacementValue(
                  GDT_UInt32, std::numeric_limits<uint32_t>::max()),
              std::numeric_limits<uint32_t>::max() - 1);

    // Test GDT_Int32
    EXPECT_EQ(GDALGetNoDataReplacementValue(
                  GDT_Int32, std::numeric_limits<double>::lowest()),
              0);
    EXPECT_EQ(GDALGetNoDataReplacementValue(GDT_Int32,
                                            std::numeric_limits<double>::max()),
              0);
    EXPECT_EQ(GDALGetNoDataReplacementValue(
                  GDT_Int32, std::numeric_limits<int32_t>::lowest()),
              std::numeric_limits<int32_t>::lowest() + 1);
    EXPECT_EQ(GDALGetNoDataReplacementValue(
                  GDT_Int32, std::numeric_limits<int32_t>::max()),
              std::numeric_limits<int32_t>::max() - 1);

    // Test GDT_UInt64
    EXPECT_EQ(GDALGetNoDataReplacementValue(
                  GDT_UInt64, std::numeric_limits<double>::lowest()),
              0);
    EXPECT_EQ(GDALGetNoDataReplacementValue(GDT_UInt64,
                                            std::numeric_limits<double>::max()),
              0);
    EXPECT_EQ(GDALGetNoDataReplacementValue(
                  GDT_UInt64,
                  static_cast<double>(std::numeric_limits<uint64_t>::lowest())),
              static_cast<double>(std::numeric_limits<uint64_t>::lowest()) + 1);
    // uin64_t max is not representable in double so we expect the next value to be returned
    EXPECT_EQ(
        GDALGetNoDataReplacementValue(
            GDT_UInt64,
            static_cast<double>(std::numeric_limits<uint64_t>::max())),
        std::nextafter(
            static_cast<double>(std::numeric_limits<uint64_t>::max()), 0) -
            1);

    // Test GDT_Int64
    EXPECT_EQ(GDALGetNoDataReplacementValue(
                  GDT_Int64, std::numeric_limits<double>::lowest()),
              0);
    EXPECT_EQ(GDALGetNoDataReplacementValue(GDT_Int64,
                                            std::numeric_limits<double>::max()),
              0);
    // in64_t max is not representable in double so we expect the next value to be returned
    EXPECT_EQ(GDALGetNoDataReplacementValue(
                  GDT_Int64,
                  static_cast<double>(std::numeric_limits<int64_t>::lowest())),
              static_cast<double>(std::numeric_limits<int64_t>::lowest()) + 1);
    EXPECT_EQ(GDALGetNoDataReplacementValue(
                  GDT_Int64,
                  static_cast<double>(std::numeric_limits<int64_t>::max())),
              std::nextafter(
                  static_cast<double>(std::numeric_limits<int64_t>::max()), 0) -
                  1);

    // Test floating point types

    // out of range for float32
    EXPECT_EQ(GDALGetNoDataReplacementValue(
                  GDT_Float32, std::numeric_limits<double>::lowest()),
              0.0);
    EXPECT_EQ(GDALGetNoDataReplacementValue(GDT_Float32,
                                            std::numeric_limits<double>::max()),
              0.0);
    EXPECT_EQ(GDALGetNoDataReplacementValue(
                  GDT_Float32, std::numeric_limits<double>::infinity()),
              0.0);
    EXPECT_EQ(GDALGetNoDataReplacementValue(
                  GDT_Float32, -std::numeric_limits<double>::infinity()),
              0.0);

    // in range for float 32
    EXPECT_EQ(
        static_cast<float>(GDALGetNoDataReplacementValue(GDT_Float32, -1.0)),
        std::nextafter(float(-1.0), 0.0f));
    EXPECT_EQ(
        static_cast<float>(GDALGetNoDataReplacementValue(GDT_Float32, 1.1)),
        std::nextafter(float(1.1), 2.0f));
    EXPECT_EQ(GDALGetNoDataReplacementValue(
                  GDT_Float32, std::numeric_limits<float>::lowest()),
              std::nextafter(std::numeric_limits<float>::lowest(), 0.0f));

    EXPECT_EQ(GDALGetNoDataReplacementValue(GDT_Float32,
                                            std::numeric_limits<float>::max()),
              static_cast<double>(
                  std::nextafter(std::numeric_limits<float>::max(), 0.0f)));

    // in range for float64
    EXPECT_EQ(GDALGetNoDataReplacementValue(
                  GDT_Float64, std::numeric_limits<double>::lowest()),
              std::nextafter(std::numeric_limits<double>::lowest(), 0.0));
    EXPECT_EQ(GDALGetNoDataReplacementValue(GDT_Float64,
                                            std::numeric_limits<double>::max()),
              std::nextafter(std::numeric_limits<double>::max(), 0.0));

    EXPECT_EQ(GDALGetNoDataReplacementValue(
                  GDT_Float64, std::numeric_limits<double>::lowest()),
              std::nextafter(std::numeric_limits<double>::lowest(), 0.0));
    EXPECT_EQ(GDALGetNoDataReplacementValue(GDT_Float64,
                                            std::numeric_limits<double>::max()),
              std::nextafter(std::numeric_limits<double>::max(), 0.0));

    EXPECT_EQ(GDALGetNoDataReplacementValue(GDT_Float64, double(-1.0)),
              std::nextafter(double(-1.0), 0.0));
    EXPECT_EQ(GDALGetNoDataReplacementValue(GDT_Float64, double(1.1)),
              std::nextafter(double(1.1), 2.0));

    // test infinity
    EXPECT_EQ(GDALGetNoDataReplacementValue(
                  GDT_Float64, std::numeric_limits<double>::infinity()),
              0.0);
    EXPECT_EQ(GDALGetNoDataReplacementValue(
                  GDT_Float64, -std::numeric_limits<double>::infinity()),
              0.0);
}

// Test GDALRasterBand::GetIndexColorTranslationTo()
TEST_F(test_gdal, GetIndexColorTranslationTo)
{
    GDALDatasetUniquePtr poSrcDS(
        GDALDriver::FromHandle(GDALGetDriverByName("MEM"))
            ->Create("", 1, 1, 1, GDT_Byte, nullptr));
    {
        GDALColorTable oCT;
        {
            GDALColorEntry e;
            e.c1 = 0;
            e.c2 = 0;
            e.c3 = 0;
            e.c4 = 255;
            oCT.SetColorEntry(0, &e);
        }
        {
            GDALColorEntry e;
            e.c1 = 1;
            e.c2 = 0;
            e.c3 = 0;
            e.c4 = 255;
            oCT.SetColorEntry(1, &e);
        }
        {
            GDALColorEntry e;
            e.c1 = 255;
            e.c2 = 255;
            e.c3 = 255;
            e.c4 = 255;
            oCT.SetColorEntry(2, &e);
        }
        {
            GDALColorEntry e;
            e.c1 = 125;
            e.c2 = 126;
            e.c3 = 127;
            e.c4 = 0;
            oCT.SetColorEntry(3, &e);
            poSrcDS->GetRasterBand(1)->SetNoDataValue(3);
        }
        poSrcDS->GetRasterBand(1)->SetColorTable(&oCT);
    }

    GDALDatasetUniquePtr poDstDS(
        GDALDriver::FromHandle(GDALGetDriverByName("MEM"))
            ->Create("", 1, 1, 1, GDT_Byte, nullptr));
    {
        GDALColorTable oCT;
        {
            GDALColorEntry e;
            e.c1 = 255;
            e.c2 = 255;
            e.c3 = 255;
            e.c4 = 255;
            oCT.SetColorEntry(0, &e);
        }
        {
            GDALColorEntry e;
            e.c1 = 0;
            e.c2 = 0;
            e.c3 = 1;
            e.c4 = 255;
            oCT.SetColorEntry(1, &e);
        }
        {
            GDALColorEntry e;
            e.c1 = 12;
            e.c2 = 13;
            e.c3 = 14;
            e.c4 = 0;
            oCT.SetColorEntry(2, &e);
            poSrcDS->GetRasterBand(1)->SetNoDataValue(2);
        }
        poDstDS->GetRasterBand(1)->SetColorTable(&oCT);
    }

    unsigned char *panTranslationTable =
        poSrcDS->GetRasterBand(1)->GetIndexColorTranslationTo(
            poDstDS->GetRasterBand(1));
    EXPECT_EQ(static_cast<int>(panTranslationTable[0]), 1);
    EXPECT_EQ(static_cast<int>(panTranslationTable[1]), 1);
    EXPECT_EQ(static_cast<int>(panTranslationTable[2]), 0);
    EXPECT_EQ(static_cast<int>(panTranslationTable[3]),
              2);  // special nodata mapping
    CPLFree(panTranslationTable);
}

// Test effect of MarkSuppressOnClose() with the final FlushCache() at dataset
// destruction
TEST_F(test_gdal, MarkSuppressOnClose)
{
    const char *pszFilename = "/vsimem/out.tif";
    const char *const apszOptions[] = {"PROFILE=BASELINE", nullptr};
    auto hDrv = GDALGetDriverByName("GTiff");
    if (!hDrv)
    {
        GTEST_SKIP() << "GTiff driver missing";
    }
    else
    {
        GDALDatasetUniquePtr poDstDS(GDALDriver::FromHandle(hDrv)->Create(
            pszFilename, 1, 1, 1, GDT_Byte, apszOptions));
        poDstDS->SetMetadataItem("FOO", "BAR");
        poDstDS->MarkSuppressOnClose();
        poDstDS->GetRasterBand(1)->Fill(255);
        poDstDS->FlushCache(true);
        // All buffers have been flushed, but our dirty block should not have
        // been written hence the checksum will be 0
        EXPECT_EQ(GDALChecksumImage(
                      GDALRasterBand::FromHandle(poDstDS->GetRasterBand(1)), 0,
                      0, 1, 1),
                  0);
    }
    {
        VSIStatBufL sStat;
        EXPECT_TRUE(VSIStatL(CPLSPrintf("%s.aux.xml", pszFilename), &sStat) !=
                    0);
    }
}

// Test effect of UnMarkSuppressOnClose()
TEST_F(test_gdal, UnMarkSuppressOnClose)
{
    const char *pszFilename = "/vsimem/out.tif";
    const char *const apszOptions[] = {"PROFILE=BASELINE", nullptr};
    auto hDrv = GDALGetDriverByName("GTiff");
    if (!hDrv)
    {
        GTEST_SKIP() << "GTiff driver missing";
    }
    else
    {
        GDALDatasetUniquePtr poDstDS(GDALDriver::FromHandle(hDrv)->Create(
            pszFilename, 1, 1, 1, GDT_Byte, apszOptions));
        poDstDS->MarkSuppressOnClose();
        poDstDS->GetRasterBand(1)->Fill(255);
        if (poDstDS->IsMarkedSuppressOnClose())
            poDstDS->UnMarkSuppressOnClose();
        poDstDS->FlushCache(true);
        // All buffers have been flushed, and our dirty block should have
        // been written hence the checksum will not be 0
        EXPECT_NE(GDALChecksumImage(
                      GDALRasterBand::FromHandle(poDstDS->GetRasterBand(1)), 0,
                      0, 1, 1),
                  0);
        VSIStatBufL sStat;
        EXPECT_TRUE(VSIStatL(pszFilename, &sStat) == 0);
        VSIUnlink(pszFilename);
    }
}

template <class T> void TestCachedPixelAccessor()
{
    constexpr auto eType = GDALCachedPixelAccessorGetDataType<T>::DataType;
    auto poDS = std::unique_ptr<GDALDataset>(
        GDALDriver::FromHandle(GDALGetDriverByName("MEM"))
            ->Create("", 11, 23, 1, eType, nullptr));
    auto poBand = poDS->GetRasterBand(1);
    GDALCachedPixelAccessor<T, 4> accessor(poBand);
    for (int iY = 0; iY < poBand->GetYSize(); iY++)
    {
        for (int iX = 0; iX < poBand->GetXSize(); iX++)
        {
            accessor.Set(iX, iY, static_cast<T>(iY * poBand->GetXSize() + iX));
        }
    }
    for (int iY = 0; iY < poBand->GetYSize(); iY++)
    {
        for (int iX = 0; iX < poBand->GetXSize(); iX++)
        {
            EXPECT_EQ(accessor.Get(iX, iY),
                      static_cast<T>(iY * poBand->GetXSize() + iX));
        }
    }

    std::vector<T> values(static_cast<size_t>(poBand->GetYSize()) *
                          poBand->GetXSize());
    accessor.FlushCache();
    EXPECT_EQ(poBand->RasterIO(GF_Read, 0, 0, poBand->GetXSize(),
                               poBand->GetYSize(), values.data(),
                               poBand->GetXSize(), poBand->GetYSize(), eType, 0,
                               0, nullptr),
              CE_None);
    for (int iY = 0; iY < poBand->GetYSize(); iY++)
    {
        for (int iX = 0; iX < poBand->GetXSize(); iX++)
        {
            EXPECT_EQ(values[iY * poBand->GetXSize() + iX],
                      static_cast<T>(iY * poBand->GetXSize() + iX));
        }
    }
}

// Test GDALCachedPixelAccessor
TEST_F(test_gdal, GDALCachedPixelAccessor)
{
    TestCachedPixelAccessor<GByte>();
    TestCachedPixelAccessor<GUInt16>();
    TestCachedPixelAccessor<GInt16>();
    TestCachedPixelAccessor<GUInt32>();
    TestCachedPixelAccessor<GInt32>();
    TestCachedPixelAccessor<GUInt64>();
    TestCachedPixelAccessor<GInt64>();
    TestCachedPixelAccessor<uint64_t>();
    TestCachedPixelAccessor<int64_t>();
    TestCachedPixelAccessor<float>();
    TestCachedPixelAccessor<double>();
}

// Test VRT and caching of sources w.r.t open options
// (https://github.com/OSGeo/gdal/issues/5989)
TEST_F(test_gdal, VRTCachingOpenOptions)
{
    if (GDALGetMetadataItem(GDALGetDriverByName("VRT"), GDAL_DMD_OPENOPTIONLIST,
                            nullptr) == nullptr)
    {
        GTEST_SKIP() << "VRT driver Open() missing";
    }

    class TestRasterBand : public GDALRasterBand
    {
      protected:
        CPLErr IReadBlock(int, int, void *pImage) override
        {
            static_cast<GByte *>(pImage)[0] = 0;
            return CE_None;
        }

      public:
        TestRasterBand()
        {
            nBlockXSize = 1;
            nBlockYSize = 1;
            eDataType = GDT_Byte;
        }
    };

    static int nCountZeroOpenOptions = 0;
    static int nCountWithOneOpenOptions = 0;

    class TestDataset : public GDALDataset
    {
      public:
        TestDataset()
        {
            nRasterXSize = 1;
            nRasterYSize = 1;
            SetBand(1, new TestRasterBand());
        }

        static GDALDataset *TestOpen(GDALOpenInfo *poOpenInfo)
        {
            if (strcmp(poOpenInfo->pszFilename, ":::DUMMY:::") != 0)
                return nullptr;
            if (poOpenInfo->papszOpenOptions == nullptr)
                nCountZeroOpenOptions++;
            else
                nCountWithOneOpenOptions++;
            return new TestDataset();
        }
    };

    std::unique_ptr<GDALDriver> driver(new GDALDriver());
    driver->SetDescription("TEST_VRT_SOURCE_OPEN_OPTION");
    driver->pfnOpen = TestDataset::TestOpen;
    GetGDALDriverManager()->RegisterDriver(driver.get());

    const char *pszVRT = R"(
<VRTDataset rasterXSize="1" rasterYSize="1">
  <VRTRasterBand dataType="Byte" band="1" subClass="VRTSourcedRasterBand">
    <SimpleSource>
      <SourceFilename relativeToVRT="0">:::DUMMY:::</SourceFilename>
    </SimpleSource>
    <SimpleSource>
      <SourceFilename relativeToVRT="0">:::DUMMY:::</SourceFilename>
    </SimpleSource>
    <SimpleSource>
      <SourceFilename relativeToVRT="0">:::DUMMY:::</SourceFilename>
      <OpenOptions>
          <OOI key="TESTARG">present</OOI>
      </OpenOptions>
    </SimpleSource>
    <SimpleSource>
      <SourceFilename relativeToVRT="0">:::DUMMY:::</SourceFilename>
      <OpenOptions>
          <OOI key="TESTARG">present</OOI>
      </OpenOptions>
    </SimpleSource>
    <SimpleSource>
      <SourceFilename relativeToVRT="0">:::DUMMY:::</SourceFilename>
      <OpenOptions>
          <OOI key="TESTARG">another_one</OOI>
      </OpenOptions>
    </SimpleSource>
  </VRTRasterBand>
</VRTDataset>)";
    auto ds = std::unique_ptr<GDALDataset>(GDALDataset::Open(pszVRT));

    // Trigger reading data, which triggers opening of source datasets
    auto rb = ds->GetRasterBand(1);
    double minmax[2];
    GDALComputeRasterMinMax(GDALRasterBand::ToHandle(rb), TRUE, minmax);

    ds.reset();
    GetGDALDriverManager()->DeregisterDriver(driver.get());

    EXPECT_EQ(nCountZeroOpenOptions, 1);
    EXPECT_EQ(nCountWithOneOpenOptions, 2);
}

// Test GDALDeinterleave 3 components Byte()
TEST_F(test_gdal, GDALDeinterleave3ComponentsByte)
{
    GByte *pabySrc = static_cast<GByte *>(CPLMalloc(3 * 4 * 15));
    for (int i = 0; i < 3 * 4 * 15; i++)
        pabySrc[i] = static_cast<GByte>(i);
    GByte *pabyDest0 = static_cast<GByte *>(CPLMalloc(4 * 15));
    GByte *pabyDest1 = static_cast<GByte *>(CPLMalloc(4 * 15));
    GByte *pabyDest2 = static_cast<GByte *>(CPLMalloc(4 * 15));
    void *ppabyDest[] = {pabyDest0, pabyDest1, pabyDest2};
    for (int nIters : {1, 4 * 15})
    {
        GDALDeinterleave(pabySrc, GDT_Byte, 3, ppabyDest, GDT_Byte, nIters);
        for (int i = 0; i < nIters; i++)
        {
            EXPECT_EQ(pabyDest0[i], 3 * i);
            EXPECT_EQ(pabyDest1[i], 3 * i + 1);
            EXPECT_EQ(pabyDest2[i], 3 * i + 2);
        }
    }
    VSIFree(pabySrc);
    VSIFree(pabyDest0);
    VSIFree(pabyDest1);
    VSIFree(pabyDest2);
}

// Test GDALDeinterleave 3 components Byte() without SSSE3
TEST_F(test_gdal, GDALDeinterleave3ComponentsByte_NOSSE3)
{
    GByte *pabySrc = static_cast<GByte *>(CPLMalloc(3 * 4 * 15));
    for (int i = 0; i < 3 * 4 * 15; i++)
        pabySrc[i] = static_cast<GByte>(i);
    GByte *pabyDest0 = static_cast<GByte *>(CPLMalloc(4 * 15));
    GByte *pabyDest1 = static_cast<GByte *>(CPLMalloc(4 * 15));
    GByte *pabyDest2 = static_cast<GByte *>(CPLMalloc(4 * 15));
    void *ppabyDest[] = {pabyDest0, pabyDest1, pabyDest2};
    for (int nIters : {1, 4 * 15})
    {
        CPLSetConfigOption("GDAL_USE_SSSE3", "NO");
        GDALDeinterleave(pabySrc, GDT_Byte, 3, ppabyDest, GDT_Byte, nIters);
        CPLSetConfigOption("GDAL_USE_SSSE3", nullptr);
        for (int i = 0; i < nIters; i++)
        {
            EXPECT_EQ(pabyDest0[i], 3 * i);
            EXPECT_EQ(pabyDest1[i], 3 * i + 1);
            EXPECT_EQ(pabyDest2[i], 3 * i + 2);
        }
    }
    VSIFree(pabySrc);
    VSIFree(pabyDest0);
    VSIFree(pabyDest1);
    VSIFree(pabyDest2);
}

// Test GDALDeinterleave 4 components Byte()
TEST_F(test_gdal, GDALDeinterleave4ComponentsByte)
{
    GByte *pabySrc = static_cast<GByte *>(CPLMalloc(3 * 4 * 15));
    for (int i = 0; i < 3 * 4 * 15; i++)
        pabySrc[i] = static_cast<GByte>(i);
    GByte *pabyDest0 = static_cast<GByte *>(CPLMalloc(3 * 15));
    GByte *pabyDest1 = static_cast<GByte *>(CPLMalloc(3 * 15));
    GByte *pabyDest2 = static_cast<GByte *>(CPLMalloc(3 * 15));
    GByte *pabyDest3 = static_cast<GByte *>(CPLMalloc(3 * 15));
    void *ppabyDest[] = {pabyDest0, pabyDest1, pabyDest2, pabyDest3};
    for (int nIters : {1, 3 * 15})
    {
        GDALDeinterleave(pabySrc, GDT_Byte, 4, ppabyDest, GDT_Byte, nIters);
        for (int i = 0; i < nIters; i++)
        {
            EXPECT_EQ(pabyDest0[i], 4 * i);
            EXPECT_EQ(pabyDest1[i], 4 * i + 1);
            EXPECT_EQ(pabyDest2[i], 4 * i + 2);
            EXPECT_EQ(pabyDest3[i], 4 * i + 3);
        }
    }
    VSIFree(pabySrc);
    VSIFree(pabyDest0);
    VSIFree(pabyDest1);
    VSIFree(pabyDest2);
    VSIFree(pabyDest3);
}

// Test GDALDeinterleave 4 components Byte without SSSE3
TEST_F(test_gdal, GDALDeinterleave4ComponentsByte_NOSSE3)
{
    GByte *pabySrc = static_cast<GByte *>(CPLMalloc(3 * 4 * 15));
    for (int i = 0; i < 3 * 4 * 15; i++)
        pabySrc[i] = static_cast<GByte>(i);
    GByte *pabyDest0 = static_cast<GByte *>(CPLMalloc(3 * 15));
    GByte *pabyDest1 = static_cast<GByte *>(CPLMalloc(3 * 15));
    GByte *pabyDest2 = static_cast<GByte *>(CPLMalloc(3 * 15));
    GByte *pabyDest3 = static_cast<GByte *>(CPLMalloc(3 * 15));
    void *ppabyDest[] = {pabyDest0, pabyDest1, pabyDest2, pabyDest3};
    for (int nIters : {1, 3 * 15})
    {
        CPLSetConfigOption("GDAL_USE_SSSE3", "NO");
        GDALDeinterleave(pabySrc, GDT_Byte, 4, ppabyDest, GDT_Byte, nIters);
        CPLSetConfigOption("GDAL_USE_SSSE3", nullptr);
        for (int i = 0; i < nIters; i++)
        {
            EXPECT_EQ(pabyDest0[i], 4 * i);
            EXPECT_EQ(pabyDest1[i], 4 * i + 1);
            EXPECT_EQ(pabyDest2[i], 4 * i + 2);
            EXPECT_EQ(pabyDest3[i], 4 * i + 3);
        }
    }
    VSIFree(pabySrc);
    VSIFree(pabyDest0);
    VSIFree(pabyDest1);
    VSIFree(pabyDest2);
    VSIFree(pabyDest3);
}

// Test GDALDeinterleave general case
TEST_F(test_gdal, GDALDeinterleaveGeneralCase)
{
    GByte *pabySrc = static_cast<GByte *>(CPLMalloc(3 * 2));
    for (int i = 0; i < 3 * 2; i++)
        pabySrc[i] = static_cast<GByte>(i);
    GUInt16 *panDest0 = static_cast<GUInt16 *>(CPLMalloc(3 * sizeof(uint16_t)));
    GUInt16 *panDest1 = static_cast<GUInt16 *>(CPLMalloc(3 * sizeof(uint16_t)));
    void *ppanDest[] = {panDest0, panDest1};
    GDALDeinterleave(pabySrc, GDT_Byte, 2, ppanDest, GDT_UInt16, 3);
    for (int i = 0; i < 3; i++)
    {
        EXPECT_EQ(panDest0[i], 2 * i);
        EXPECT_EQ(panDest1[i], 2 * i + 1);
    }
    VSIFree(pabySrc);
    VSIFree(panDest0);
    VSIFree(panDest1);
}

// Test GDALDeinterleave 3 components UInt16()
TEST_F(test_gdal, GDALDeinterleave3ComponentsUInt16)
{
    GUInt16 *panSrc =
        static_cast<GUInt16 *>(CPLMalloc(3 * 4 * 15 * sizeof(GUInt16)));
    for (int i = 0; i < 3 * 4 * 15; i++)
        panSrc[i] = static_cast<GUInt16>(i + 32767);
    GUInt16 *panDest0 =
        static_cast<GUInt16 *>(CPLMalloc(4 * 15 * sizeof(GUInt16)));
    GUInt16 *panDest1 =
        static_cast<GUInt16 *>(CPLMalloc(4 * 15 * sizeof(GUInt16)));
    GUInt16 *panDest2 =
        static_cast<GUInt16 *>(CPLMalloc(4 * 15 * sizeof(GUInt16)));
    void *ppanDest[] = {panDest0, panDest1, panDest2};
    for (int nIters : {1, 4 * 15})
    {
        GDALDeinterleave(panSrc, GDT_UInt16, 3, ppanDest, GDT_UInt16, nIters);
        for (int i = 0; i < nIters; i++)
        {
            EXPECT_EQ(panDest0[i], 3 * i + 32767);
            EXPECT_EQ(panDest1[i], 3 * i + 1 + 32767);
            EXPECT_EQ(panDest2[i], 3 * i + 2 + 32767);
        }
    }
    VSIFree(panSrc);
    VSIFree(panDest0);
    VSIFree(panDest1);
    VSIFree(panDest2);
}

// Test GDALDeinterleave 4 components UInt16()
TEST_F(test_gdal, GDALDeinterleave4ComponentsUInt16)
{
    GUInt16 *panSrc =
        static_cast<GUInt16 *>(CPLMalloc(3 * 4 * 15 * sizeof(GUInt16)));
    for (int i = 0; i < 3 * 4 * 15; i++)
        panSrc[i] = static_cast<GUInt16>(i + 32767);
    GUInt16 *panDest0 =
        static_cast<GUInt16 *>(CPLMalloc(4 * 15 * sizeof(GUInt16)));
    GUInt16 *panDest1 =
        static_cast<GUInt16 *>(CPLMalloc(4 * 15 * sizeof(GUInt16)));
    GUInt16 *panDest2 =
        static_cast<GUInt16 *>(CPLMalloc(4 * 15 * sizeof(GUInt16)));
    GUInt16 *panDest3 =
        static_cast<GUInt16 *>(CPLMalloc(4 * 15 * sizeof(GUInt16)));
    void *ppanDest[] = {panDest0, panDest1, panDest2, panDest3};
    for (int nIters : {1, 3 * 15})
    {
        GDALDeinterleave(panSrc, GDT_UInt16, 4, ppanDest, GDT_UInt16, nIters);
        for (int i = 0; i < nIters; i++)
        {
            EXPECT_EQ(panDest0[i], 4 * i + 32767);
            EXPECT_EQ(panDest1[i], 4 * i + 1 + 32767);
            EXPECT_EQ(panDest2[i], 4 * i + 2 + 32767);
            EXPECT_EQ(panDest3[i], 4 * i + 3 + 32767);
        }
    }
    VSIFree(panSrc);
    VSIFree(panDest0);
    VSIFree(panDest1);
    VSIFree(panDest2);
    VSIFree(panDest3);
}

// Test GDALDataset::ReportError()
TEST_F(test_gdal, GDALDatasetReportError)
{
    GDALDatasetUniquePtr poSrcDS(
        GDALDriver::FromHandle(GDALGetDriverByName("MEM"))
            ->Create("", 1, 1, 1, GDT_Byte, nullptr));

    CPLPushErrorHandler(CPLQuietErrorHandler);
    poSrcDS->ReportError("foo", CE_Warning, CPLE_AppDefined, "bar");
    CPLPopErrorHandler();
    EXPECT_STREQ(CPLGetLastErrorMsg(), "foo: bar");

    CPLPushErrorHandler(CPLQuietErrorHandler);
    poSrcDS->ReportError("%foo", CE_Warning, CPLE_AppDefined, "bar");
    CPLPopErrorHandler();
    EXPECT_STREQ(CPLGetLastErrorMsg(), "%foo: bar");

    CPLPushErrorHandler(CPLQuietErrorHandler);
    poSrcDS->ReportError(
        "this_is_"
        "wayyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy"
        "yyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy"
        "yyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy"
        "yyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy"
        "yyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy"
        "yyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy"
        "yyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy"
        "yyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy"
        "yyyyyyy_too_long/foo",
        CE_Warning, CPLE_AppDefined, "bar");
    CPLPopErrorHandler();
    EXPECT_STREQ(CPLGetLastErrorMsg(), "foo: bar");
}

// Test GDALDataset::GetCompressionFormats() and ReadCompressedData()
TEST_F(test_gdal, gtiff_ReadCompressedData)
{
    if (!GDALGetDriverByName("GTiff"))
    {
        GTEST_SKIP() << "GTiff driver missing";
    }
    if (GDALGetDriverByName("JPEG") == nullptr)
    {
        GTEST_SKIP() << "JPEG support missing";
    }

    GDALDatasetUniquePtr poSrcDS(GDALDataset::FromHandle(
        GDALDataset::Open((tut::common::data_basedir +
                           "/../../gcore/data/byte_jpg_unusual_jpegtable.tif")
                              .c_str())));
    ASSERT_TRUE(poSrcDS);

    const CPLStringList aosRet(GDALDatasetGetCompressionFormats(
        GDALDataset::ToHandle(poSrcDS.get()), 0, 0, 20, 20, 1, nullptr));
    EXPECT_EQ(aosRet.size(), 1);
    if (aosRet.size() == 1)
    {
        EXPECT_STREQ(aosRet[0], "JPEG");
    }

    {
        int nBand = 1;
        EXPECT_EQ(CPLStringList(GDALDatasetGetCompressionFormats(
                                    GDALDataset::ToHandle(poSrcDS.get()), 0, 0,
                                    20, 20, 1, &nBand))
                      .size(),
                  1);
    }

    // nBandCout > nBands
    EXPECT_EQ(CPLStringList(GDALDatasetGetCompressionFormats(
                                GDALDataset::ToHandle(poSrcDS.get()), 0, 0, 20,
                                20, 2, nullptr))
                  .size(),
              0);

    // Cannot subset just one pixel
    EXPECT_EQ(CPLStringList(GDALDatasetGetCompressionFormats(
                                GDALDataset::ToHandle(poSrcDS.get()), 0, 0, 1,
                                1, 1, nullptr))
                  .size(),
              0);

    // Wrong band number
    {
        int nBand = 2;
        EXPECT_EQ(CPLStringList(GDALDatasetGetCompressionFormats(
                                    GDALDataset::ToHandle(poSrcDS.get()), 0, 0,
                                    20, 20, 1, &nBand))
                      .size(),
                  0);
    }

    EXPECT_EQ(GDALDatasetReadCompressedData(
                  GDALDataset::ToHandle(poSrcDS.get()), "JPEG", 0, 0, 20, 20, 1,
                  nullptr, nullptr, nullptr, nullptr),
              CE_None);

    size_t nNeededSize;
    {
        char *pszDetailedFormat = nullptr;
        EXPECT_EQ(GDALDatasetReadCompressedData(
                      GDALDataset::ToHandle(poSrcDS.get()), "JPEG", 0, 0, 20,
                      20, 1, nullptr, nullptr, &nNeededSize,
                      &pszDetailedFormat),
                  CE_None);
        EXPECT_EQ(nNeededSize, 476);
        EXPECT_TRUE(pszDetailedFormat != nullptr);
        if (pszDetailedFormat)
        {
            ASSERT_STREQ(pszDetailedFormat, "JPEG");
            VSIFree(pszDetailedFormat);
        }
    }

    {
        const GByte abyCanary[] = {0xDE, 0xAD, 0xBE, 0xEF};
        std::vector<GByte> abyBuffer(nNeededSize + sizeof(abyCanary));
        memcpy(&abyBuffer[nNeededSize], abyCanary, sizeof(abyCanary));
        void *pabyBuffer = abyBuffer.data();
        void **ppabyBuffer = &pabyBuffer;
        size_t nProvidedSize = nNeededSize;
        char *pszDetailedFormat = nullptr;
        EXPECT_EQ(GDALDatasetReadCompressedData(
                      GDALDataset::ToHandle(poSrcDS.get()), "JPEG", 0, 0, 20,
                      20, 1, nullptr, ppabyBuffer, &nProvidedSize,
                      &pszDetailedFormat),
                  CE_None);
        ASSERT_EQ(nProvidedSize, nNeededSize);
        ASSERT_TRUE(*ppabyBuffer == pabyBuffer);
        EXPECT_TRUE(pszDetailedFormat != nullptr);
        if (pszDetailedFormat)
        {
            ASSERT_STREQ(pszDetailedFormat,
                         "JPEG;frame_type=SOF0_baseline;bit_depth=8;num_"
                         "components=1;colorspace=unknown");
            VSIFree(pszDetailedFormat);
        }
        EXPECT_TRUE(
            memcmp(&abyBuffer[nNeededSize], abyCanary, sizeof(abyCanary)) == 0);
        EXPECT_EQ(abyBuffer[0], 0xFF);
        EXPECT_EQ(abyBuffer[1], 0xD8);
        EXPECT_EQ(abyBuffer[nNeededSize - 2], 0xFF);
        EXPECT_EQ(abyBuffer[nNeededSize - 1], 0xD9);

        // Buffer larger than needed: OK
        nProvidedSize = nNeededSize + 1;
        EXPECT_EQ(GDALDatasetReadCompressedData(
                      GDALDataset::ToHandle(poSrcDS.get()), "JPEG", 0, 0, 20,
                      20, 1, nullptr, ppabyBuffer, &nProvidedSize, nullptr),
                  CE_None);

        // Too small buffer
        nProvidedSize = nNeededSize - 1;
        EXPECT_EQ(GDALDatasetReadCompressedData(
                      GDALDataset::ToHandle(poSrcDS.get()), "JPEG", 0, 0, 20,
                      20, 1, nullptr, ppabyBuffer, &nProvidedSize, nullptr),
                  CE_Failure);

        // Missing pointer to size
        EXPECT_EQ(GDALDatasetReadCompressedData(
                      GDALDataset::ToHandle(poSrcDS.get()), "JPEG", 0, 0, 20,
                      20, 1, nullptr, ppabyBuffer, nullptr, nullptr),
                  CE_Failure);
    }

    // Let GDAL allocate buffer
    {
        void *pBuffer = nullptr;
        size_t nGotSize = 0;
        EXPECT_EQ(GDALDatasetReadCompressedData(
                      GDALDataset::ToHandle(poSrcDS.get()), "JPEG", 0, 0, 20,
                      20, 1, nullptr, &pBuffer, &nGotSize, nullptr),
                  CE_None);
        EXPECT_EQ(nGotSize, nNeededSize);
        EXPECT_NE(pBuffer, nullptr);
        if (pBuffer != nullptr && nGotSize == nNeededSize && nNeededSize >= 2)
        {
            const GByte *pabyBuffer = static_cast<GByte *>(pBuffer);
            EXPECT_EQ(pabyBuffer[0], 0xFF);
            EXPECT_EQ(pabyBuffer[1], 0xD8);
            EXPECT_EQ(pabyBuffer[nNeededSize - 2], 0xFF);
            EXPECT_EQ(pabyBuffer[nNeededSize - 1], 0xD9);
        }
        VSIFree(pBuffer);
    }

    // Cannot subset just one pixel
    EXPECT_EQ(GDALDatasetReadCompressedData(
                  GDALDataset::ToHandle(poSrcDS.get()), "JPEG", 0, 0, 1, 1, 1,
                  nullptr, nullptr, nullptr, nullptr),
              CE_Failure);

    EXPECT_EQ(GDALDatasetReadCompressedData(
                  GDALDataset::ToHandle(poSrcDS.get()), "wrong_format", 0, 0,
                  20, 20, 1, nullptr, nullptr, nullptr, nullptr),
              CE_Failure);
}

// Test GDALDataset::GetCompressionFormats() and ReadCompressedData()
TEST_F(test_gdal, gtiff_ReadCompressedData_jpeg_rgba)
{
    if (!GDALGetDriverByName("GTiff"))
    {
        GTEST_SKIP() << "GTiff driver missing";
    }
    if (GDALGetDriverByName("JPEG") == nullptr)
    {
        GTEST_SKIP() << "JPEG support missing";
    }

    GDALDatasetUniquePtr poSrcDS(GDALDataset::FromHandle(
        GDALDataset::Open((tut::common::data_basedir +
                           "/../../gcore/data/stefan_full_rgba_jpeg_contig.tif")
                              .c_str())));
    ASSERT_TRUE(poSrcDS);

    const CPLStringList aosRet(GDALDatasetGetCompressionFormats(
        GDALDataset::ToHandle(poSrcDS.get()), 0, 0, 162, 16, 4, nullptr));
    EXPECT_EQ(aosRet.size(), 1);
    if (aosRet.size() == 1)
    {
        EXPECT_STREQ(aosRet[0], "JPEG;colorspace=RGBA");
    }

    // Let GDAL allocate buffer
    {
        void *pBuffer = nullptr;
        size_t nGotSize = 0;
        char *pszDetailedFormat = nullptr;
        EXPECT_EQ(GDALDatasetReadCompressedData(
                      GDALDataset::ToHandle(poSrcDS.get()), "JPEG", 0, 0, 162,
                      16, 4, nullptr, &pBuffer, &nGotSize, &pszDetailedFormat),
                  CE_None);
        if (pszDetailedFormat)
        {
            ASSERT_STREQ(pszDetailedFormat,
                         "JPEG;frame_type=SOF0_baseline;bit_depth=8;num_"
                         "components=4;colorspace=RGBA");
            VSIFree(pszDetailedFormat);
        }
        VSIFree(pBuffer);
    }
}

// Test GDALDataset::GetCompressionFormats() and ReadCompressedData()
TEST_F(test_gdal, jpeg_ReadCompressedData)
{
    if (GDALGetDriverByName("JPEG") == nullptr)
    {
        GTEST_SKIP() << "JPEG support missing";
    }

    GDALDatasetUniquePtr poSrcDS(GDALDataset::FromHandle(GDALDataset::Open(
        (tut::common::data_basedir + "/../../gdrivers/data/jpeg/albania.jpg")
            .c_str())));
    ASSERT_TRUE(poSrcDS);

    const CPLStringList aosRet(GDALDatasetGetCompressionFormats(
        GDALDataset::ToHandle(poSrcDS.get()), 0, 0, 361, 260, 3, nullptr));
    EXPECT_EQ(aosRet.size(), 1);
    if (aosRet.size() == 1)
    {
        EXPECT_STREQ(aosRet[0],
                     "JPEG;frame_type=SOF0_baseline;bit_depth=8;num_components="
                     "3;subsampling=4:2:0;colorspace=YCbCr");
    }

    size_t nUpperBoundSize;
    EXPECT_EQ(GDALDatasetReadCompressedData(
                  GDALDataset::ToHandle(poSrcDS.get()), "JPEG", 0, 0, 361, 260,
                  3, nullptr, nullptr, &nUpperBoundSize, nullptr),
              CE_None);
    EXPECT_EQ(nUpperBoundSize, 12574);

    {
        std::vector<GByte> abyBuffer(nUpperBoundSize);
        void *pabyBuffer = abyBuffer.data();
        void **ppabyBuffer = &pabyBuffer;
        size_t nSize = nUpperBoundSize;
        char *pszDetailedFormat = nullptr;
        EXPECT_EQ(GDALDatasetReadCompressedData(
                      GDALDataset::ToHandle(poSrcDS.get()), "JPEG", 0, 0, 361,
                      260, 3, nullptr, ppabyBuffer, &nSize, &pszDetailedFormat),
                  CE_None);
        ASSERT_LT(nSize, nUpperBoundSize);
        ASSERT_TRUE(*ppabyBuffer == pabyBuffer);
        EXPECT_TRUE(pszDetailedFormat != nullptr);
        if (pszDetailedFormat)
        {
            ASSERT_STREQ(pszDetailedFormat,
                         "JPEG;frame_type=SOF0_baseline;bit_depth=8;num_"
                         "components=3;subsampling=4:2:0;colorspace=YCbCr");
            VSIFree(pszDetailedFormat);
        }
        EXPECT_EQ(abyBuffer[0], 0xFF);
        EXPECT_EQ(abyBuffer[1], 0xD8);
        EXPECT_EQ(abyBuffer[nSize - 2], 0xFF);
        EXPECT_EQ(abyBuffer[nSize - 1], 0xD9);

        // Buffer larger than needed: OK
        nSize = nUpperBoundSize + 1;
        EXPECT_EQ(GDALDatasetReadCompressedData(
                      GDALDataset::ToHandle(poSrcDS.get()), "JPEG", 0, 0, 361,
                      260, 3, nullptr, ppabyBuffer, &nSize, nullptr),
                  CE_None);

        // Too small buffer
        nSize = nUpperBoundSize - 1;
        EXPECT_EQ(GDALDatasetReadCompressedData(
                      GDALDataset::ToHandle(poSrcDS.get()), "JPEG", 0, 0, 361,
                      260, 3, nullptr, ppabyBuffer, &nSize, nullptr),
                  CE_Failure);

        // Missing pointer to size
        EXPECT_EQ(GDALDatasetReadCompressedData(
                      GDALDataset::ToHandle(poSrcDS.get()), "JPEG", 0, 0, 361,
                      260, 3, nullptr, ppabyBuffer, nullptr, nullptr),
                  CE_Failure);
    }

    // Let GDAL allocate buffer
    {
        void *pBuffer = nullptr;
        size_t nSize = nUpperBoundSize;
        EXPECT_EQ(GDALDatasetReadCompressedData(
                      GDALDataset::ToHandle(poSrcDS.get()), "JPEG", 0, 0, 361,
                      260, 3, nullptr, &pBuffer, &nSize, nullptr),
                  CE_None);
        EXPECT_GT(nSize, 4);
        EXPECT_LT(nSize, nUpperBoundSize);
        EXPECT_NE(pBuffer, nullptr);
        if (pBuffer != nullptr && nSize >= 4 && nSize <= nUpperBoundSize)
        {
            const GByte *pabyBuffer = static_cast<GByte *>(pBuffer);
            EXPECT_EQ(pabyBuffer[0], 0xFF);
            EXPECT_EQ(pabyBuffer[1], 0xD8);
            EXPECT_EQ(pabyBuffer[nSize - 2], 0xFF);
            EXPECT_EQ(pabyBuffer[nSize - 1], 0xD9);
        }
        VSIFree(pBuffer);
    }
}

// Test GDALDataset::GetCompressionFormats() and ReadCompressedData()
TEST_F(test_gdal, jpegxl_ReadCompressedData)
{
    if (GDALGetDriverByName("JPEGXL") == nullptr)
    {
        GTEST_SKIP() << "JPEGXL support missing";
    }

    GDALDatasetUniquePtr poSrcDS(GDALDataset::FromHandle(GDALDataset::Open(
        (tut::common::data_basedir + "/../../gdrivers/data/jpegxl/byte.jxl")
            .c_str())));
    ASSERT_TRUE(poSrcDS);

    const CPLStringList aosRet(GDALDatasetGetCompressionFormats(
        GDALDataset::ToHandle(poSrcDS.get()), 0, 0, 20, 20, 1, nullptr));
    EXPECT_EQ(aosRet.size(), 1);
    if (aosRet.size() == 1)
    {
        EXPECT_STREQ(aosRet[0], "JXL");
    }

    size_t nUpperBoundSize;
    EXPECT_EQ(GDALDatasetReadCompressedData(
                  GDALDataset::ToHandle(poSrcDS.get()), "JXL", 0, 0, 20, 20, 1,
                  nullptr, nullptr, &nUpperBoundSize, nullptr),
              CE_None);
    EXPECT_EQ(nUpperBoundSize, 719);

    {
        std::vector<GByte> abyBuffer(nUpperBoundSize);
        void *pabyBuffer = abyBuffer.data();
        void **ppabyBuffer = &pabyBuffer;
        size_t nSize = nUpperBoundSize;
        char *pszDetailedFormat = nullptr;
        EXPECT_EQ(GDALDatasetReadCompressedData(
                      GDALDataset::ToHandle(poSrcDS.get()), "JXL", 0, 0, 20, 20,
                      1, nullptr, ppabyBuffer, &nSize, &pszDetailedFormat),
                  CE_None);
        ASSERT_LT(nSize, nUpperBoundSize);
        ASSERT_TRUE(*ppabyBuffer == pabyBuffer);
        EXPECT_TRUE(pszDetailedFormat != nullptr);
        if (pszDetailedFormat)
        {
            ASSERT_STREQ(pszDetailedFormat, "JXL");
            VSIFree(pszDetailedFormat);
        }
        EXPECT_EQ(abyBuffer[0], 0x00);
        EXPECT_EQ(abyBuffer[1], 0x00);
        EXPECT_EQ(abyBuffer[2], 0x00);
        EXPECT_EQ(abyBuffer[3], 0x0C);
        EXPECT_EQ(abyBuffer[nSize - 2], 0x4C);
        EXPECT_EQ(abyBuffer[nSize - 1], 0x01);

        // Buffer larger than needed: OK
        nSize = nUpperBoundSize + 1;
        EXPECT_EQ(GDALDatasetReadCompressedData(
                      GDALDataset::ToHandle(poSrcDS.get()), "JXL", 0, 0, 20, 20,
                      1, nullptr, ppabyBuffer, &nSize, nullptr),
                  CE_None);

        // Too small buffer
        nSize = nUpperBoundSize - 1;
        EXPECT_EQ(GDALDatasetReadCompressedData(
                      GDALDataset::ToHandle(poSrcDS.get()), "JXL", 0, 0, 20, 20,
                      1, nullptr, ppabyBuffer, &nSize, nullptr),
                  CE_Failure);

        // Missing pointer to size
        EXPECT_EQ(GDALDatasetReadCompressedData(
                      GDALDataset::ToHandle(poSrcDS.get()), "JXL", 0, 0, 20, 20,
                      1, nullptr, ppabyBuffer, nullptr, nullptr),
                  CE_Failure);
    }

    // Let GDAL allocate buffer
    {
        void *pBuffer = nullptr;
        size_t nSize = nUpperBoundSize;
        EXPECT_EQ(GDALDatasetReadCompressedData(
                      GDALDataset::ToHandle(poSrcDS.get()), "JXL", 0, 0, 20, 20,
                      1, nullptr, &pBuffer, &nSize, nullptr),
                  CE_None);
        EXPECT_GT(nSize, 6);
        EXPECT_LT(nSize, nUpperBoundSize);
        EXPECT_NE(pBuffer, nullptr);
        if (pBuffer != nullptr && nSize >= 6 && nSize <= nUpperBoundSize)
        {
            const GByte *pabyBuffer = static_cast<GByte *>(pBuffer);
            EXPECT_EQ(pabyBuffer[0], 0x00);
            EXPECT_EQ(pabyBuffer[1], 0x00);
            EXPECT_EQ(pabyBuffer[2], 0x00);
            EXPECT_EQ(pabyBuffer[3], 0x0C);
            EXPECT_EQ(pabyBuffer[nSize - 2], 0x4C);
            EXPECT_EQ(pabyBuffer[nSize - 1], 0x01);
        }
        VSIFree(pBuffer);
    }
}

// Test GDALDataset::GetCompressionFormats() and ReadCompressedData()
TEST_F(test_gdal, jpegxl_jpeg_compatible_ReadCompressedData)
{
    auto poDrv = GDALDriver::FromHandle(GDALGetDriverByName("JPEGXL"));
    if (poDrv == nullptr)
    {
        GTEST_SKIP() << "JPEGXL support missing";
    }

    GDALDatasetUniquePtr poSrcDS(GDALDataset::FromHandle(GDALDataset::Open(
        (tut::common::data_basedir +
         "/../../gdrivers/data/jpegxl/exif_orientation/F1.jxl")
            .c_str())));
    ASSERT_TRUE(poSrcDS);

    const CPLStringList aosRet(GDALDatasetGetCompressionFormats(
        GDALDataset::ToHandle(poSrcDS.get()), 0, 0, 3, 5, 1, nullptr));
    EXPECT_EQ(aosRet.size(), 2);
    if (aosRet.size() == 2)
    {
        EXPECT_STREQ(aosRet[0], "JXL");
        EXPECT_STREQ(aosRet[1], "JPEG");
    }

    size_t nUpperBoundSize;
    EXPECT_EQ(GDALDatasetReadCompressedData(
                  GDALDataset::ToHandle(poSrcDS.get()), "JPEG", 0, 0, 3, 5, 1,
                  nullptr, nullptr, &nUpperBoundSize, nullptr),
              CE_None);
    EXPECT_EQ(nUpperBoundSize, 235);

    {
        std::vector<GByte> abyBuffer(nUpperBoundSize);
        void *pabyBuffer = abyBuffer.data();
        void **ppabyBuffer = &pabyBuffer;
        size_t nSize = nUpperBoundSize;
        char *pszDetailedFormat = nullptr;
        EXPECT_EQ(GDALDatasetReadCompressedData(
                      GDALDataset::ToHandle(poSrcDS.get()), "JPEG", 0, 0, 3, 5,
                      1, nullptr, ppabyBuffer, &nSize, &pszDetailedFormat),
                  CE_None);
        ASSERT_LE(nSize, nUpperBoundSize);
        ASSERT_TRUE(*ppabyBuffer == pabyBuffer);
        EXPECT_TRUE(pszDetailedFormat != nullptr);
        if (pszDetailedFormat)
        {
            ASSERT_STREQ(pszDetailedFormat,
                         "JPEG;frame_type=SOF0_baseline;bit_depth=8;num_"
                         "components=1;colorspace=unknown");
            VSIFree(pszDetailedFormat);
        }
        EXPECT_EQ(abyBuffer[0], 0xFF);
        EXPECT_EQ(abyBuffer[1], 0xD8);
        EXPECT_EQ(abyBuffer[nSize - 2], 0xFF);
        EXPECT_EQ(abyBuffer[nSize - 1], 0xD9);

        // Buffer larger than needed: OK
        nSize = nUpperBoundSize + 1;
        EXPECT_EQ(GDALDatasetReadCompressedData(
                      GDALDataset::ToHandle(poSrcDS.get()), "JPEG", 0, 0, 3, 5,
                      1, nullptr, ppabyBuffer, &nSize, nullptr),
                  CE_None);

        // Too small buffer
        nSize = nUpperBoundSize - 1;
        EXPECT_EQ(GDALDatasetReadCompressedData(
                      GDALDataset::ToHandle(poSrcDS.get()), "JPEG", 0, 0, 3, 5,
                      1, nullptr, ppabyBuffer, &nSize, nullptr),
                  CE_Failure);

        // Missing pointer to size
        EXPECT_EQ(GDALDatasetReadCompressedData(
                      GDALDataset::ToHandle(poSrcDS.get()), "JPEG", 0, 0, 3, 5,
                      1, nullptr, ppabyBuffer, nullptr, nullptr),
                  CE_Failure);
    }

    // Let GDAL allocate buffer
    {
        void *pBuffer = nullptr;
        size_t nSize = nUpperBoundSize;
        EXPECT_EQ(GDALDatasetReadCompressedData(
                      GDALDataset::ToHandle(poSrcDS.get()), "JPEG", 0, 0, 3, 5,
                      1, nullptr, &pBuffer, &nSize, nullptr),
                  CE_None);
        EXPECT_GT(nSize, 4);
        EXPECT_LE(nSize, nUpperBoundSize);
        EXPECT_NE(pBuffer, nullptr);
        if (pBuffer != nullptr && nSize >= 4 && nSize <= nUpperBoundSize)
        {
            const GByte *pabyBuffer = static_cast<GByte *>(pBuffer);
            EXPECT_EQ(pabyBuffer[0], 0xFF);
            EXPECT_EQ(pabyBuffer[1], 0xD8);
            EXPECT_EQ(pabyBuffer[nSize - 2], 0xFF);
            EXPECT_EQ(pabyBuffer[nSize - 1], 0xD9);
        }
        VSIFree(pBuffer);
    }
}

// Test GDAL_OF_SHARED flag and open options
TEST_F(test_gdal, open_shared_open_options)
{
    if (!GDALGetDriverByName("GTiff"))
    {
        GTEST_SKIP() << "GTiff driver missing";
    }

    CPLErrorReset();
    const char *const apszOpenOptions[] = {"OVERVIEW_LEVEL=NONE", nullptr};
    {
        GDALDataset *poDS1 =
            GDALDataset::Open(GCORE_DATA_DIR "rgbsmall.tif", GDAL_OF_SHARED,
                              nullptr, apszOpenOptions);
        GDALDataset *poDS2 =
            GDALDataset::Open(GCORE_DATA_DIR "rgbsmall.tif", GDAL_OF_SHARED,
                              nullptr, apszOpenOptions);
        EXPECT_EQ(CPLGetLastErrorType(), CE_None);
        EXPECT_NE(poDS1, nullptr);
        EXPECT_NE(poDS2, nullptr);
        EXPECT_EQ(poDS1, poDS2);
        GDALClose(poDS1);
        GDALClose(poDS2);
    }
    {
        GDALDataset *poDS1 =
            GDALDataset::Open(GCORE_DATA_DIR "rgbsmall.tif", GDAL_OF_SHARED,
                              nullptr, apszOpenOptions);
        GDALDataset *poDS2 = GDALDataset::Open(
            GCORE_DATA_DIR "rgbsmall.tif", GDAL_OF_SHARED, nullptr, nullptr);
        GDALDataset *poDS3 =
            GDALDataset::Open(GCORE_DATA_DIR "rgbsmall.tif", GDAL_OF_SHARED,
                              nullptr, apszOpenOptions);
        EXPECT_EQ(CPLGetLastErrorType(), CE_None);
        EXPECT_NE(poDS1, nullptr);
        EXPECT_NE(poDS2, nullptr);
        EXPECT_NE(poDS3, nullptr);
        EXPECT_NE(poDS1, poDS2);
        EXPECT_EQ(poDS1, poDS3);
        GDALClose(poDS1);
        GDALClose(poDS2);
        GDALClose(poDS3);
    }
    {
        GDALDataset *poDS1 = GDALDataset::Open(GCORE_DATA_DIR "rgbsmall.tif",
                                               GDAL_OF_SHARED | GDAL_OF_UPDATE,
                                               nullptr, apszOpenOptions);
        // We allow to re-use a shared dataset in update mode when requesting it in read-only
        GDALDataset *poDS2 =
            GDALDataset::Open(GCORE_DATA_DIR "rgbsmall.tif", GDAL_OF_SHARED,
                              nullptr, apszOpenOptions);
        EXPECT_EQ(CPLGetLastErrorType(), CE_None);
        EXPECT_NE(poDS1, nullptr);
        EXPECT_NE(poDS2, nullptr);
        EXPECT_EQ(poDS1, poDS2);
        GDALClose(poDS1);
        GDALClose(poDS2);
    }
    {
        GDALDataset *poDS1 = GDALDataset::Open(
            GCORE_DATA_DIR "rgbsmall.tif", GDAL_OF_SHARED, nullptr, nullptr);
        GDALDataset *poDS2 =
            GDALDataset::Open(GCORE_DATA_DIR "rgbsmall.tif", GDAL_OF_SHARED,
                              nullptr, apszOpenOptions);
        GDALDataset *poDS3 =
            GDALDataset::Open(GCORE_DATA_DIR "rgbsmall.tif", GDAL_OF_SHARED,
                              nullptr, apszOpenOptions);
        EXPECT_EQ(CPLGetLastErrorType(), CE_None);
        EXPECT_NE(poDS1, nullptr);
        EXPECT_NE(poDS2, nullptr);
        EXPECT_NE(poDS3, nullptr);
        EXPECT_NE(poDS1, poDS2);
        EXPECT_EQ(poDS2, poDS3);
        GDALClose(poDS1);
        GDALClose(poDS2);
        GDALClose(poDS3);
    }
}

// Test DropCache() to check that no data is saved on disk
TEST_F(test_gdal, drop_cache)
{
    CPLErrorReset();
    {
        GDALDriverManager *gdalDriverManager = GetGDALDriverManager();
        if (!gdalDriverManager)
            return;
        GDALDriver *enviDriver = gdalDriverManager->GetDriverByName("ENVI");
        if (!enviDriver)
            return;
        const char *enviOptions[] = {"SUFFIX=ADD", "INTERLEAVE=BIL", nullptr};

        const char *filename = GCORE_DATA_DIR "test_drop_cache.bil";

        auto poDS = std::unique_ptr<GDALDataset>(enviDriver->Create(
            filename, 1, 1, 1, GDALDataType::GDT_Float32, enviOptions));
        if (!poDS)
            return;
        poDS->GetRasterBand(1)->Fill(1);
        poDS->DropCache();
        poDS.reset();

        poDS.reset(
            GDALDataset::Open(filename, GDAL_OF_SHARED, nullptr, nullptr));
        if (!poDS)
            return;

        EXPECT_EQ(GDALChecksumImage(poDS->GetRasterBand(1), 0, 0, 1, 1), 0);
        poDS->MarkSuppressOnClose();
        poDS.reset();
    }
}

// Test gdal::gcp class
TEST_F(test_gdal, gdal_gcp_class)
{
    {
        gdal::GCP gcp;
        EXPECT_STREQ(gcp.Id(), "");
        EXPECT_STREQ(gcp.Info(), "");
        EXPECT_EQ(gcp.Pixel(), 0.0);
        EXPECT_EQ(gcp.Line(), 0.0);
        EXPECT_EQ(gcp.X(), 0.0);
        EXPECT_EQ(gcp.Y(), 0.0);
        EXPECT_EQ(gcp.Z(), 0.0);
    }
    {
        gdal::GCP gcp("id", "info", 1.5, 2.5, 3.5, 4.5, 5.5);
        EXPECT_STREQ(gcp.Id(), "id");
        EXPECT_STREQ(gcp.Info(), "info");
        EXPECT_EQ(gcp.Pixel(), 1.5);
        EXPECT_EQ(gcp.Line(), 2.5);
        EXPECT_EQ(gcp.X(), 3.5);
        EXPECT_EQ(gcp.Y(), 4.5);
        EXPECT_EQ(gcp.Z(), 5.5);

        gcp.SetId("id2");
        gcp.SetInfo("info2");
        gcp.Pixel() = -1.5;
        gcp.Line() = -2.5;
        gcp.X() = -3.5;
        gcp.Y() = -4.5;
        gcp.Z() = -5.5;
        EXPECT_STREQ(gcp.Id(), "id2");
        EXPECT_STREQ(gcp.Info(), "info2");
        EXPECT_EQ(gcp.Pixel(), -1.5);
        EXPECT_EQ(gcp.Line(), -2.5);
        EXPECT_EQ(gcp.X(), -3.5);
        EXPECT_EQ(gcp.Y(), -4.5);
        EXPECT_EQ(gcp.Z(), -5.5);

        {
            gdal::GCP gcp_copy(gcp);
            EXPECT_STREQ(gcp_copy.Id(), "id2");
            EXPECT_STREQ(gcp_copy.Info(), "info2");
            EXPECT_EQ(gcp_copy.Pixel(), -1.5);
            EXPECT_EQ(gcp_copy.Line(), -2.5);
            EXPECT_EQ(gcp_copy.X(), -3.5);
            EXPECT_EQ(gcp_copy.Y(), -4.5);
            EXPECT_EQ(gcp_copy.Z(), -5.5);
        }

        {
            gdal::GCP gcp_copy;
            gcp_copy = gcp;
            EXPECT_STREQ(gcp_copy.Id(), "id2");
            EXPECT_STREQ(gcp_copy.Info(), "info2");
            EXPECT_EQ(gcp_copy.Pixel(), -1.5);
            EXPECT_EQ(gcp_copy.Line(), -2.5);
            EXPECT_EQ(gcp_copy.X(), -3.5);
            EXPECT_EQ(gcp_copy.Y(), -4.5);
            EXPECT_EQ(gcp_copy.Z(), -5.5);
        }

        {
            gdal::GCP gcp_copy(gcp);
            gdal::GCP gcp_from_moved(std::move(gcp_copy));
            EXPECT_STREQ(gcp_from_moved.Id(), "id2");
            EXPECT_STREQ(gcp_from_moved.Info(), "info2");
            EXPECT_EQ(gcp_from_moved.Pixel(), -1.5);
            EXPECT_EQ(gcp_from_moved.Line(), -2.5);
            EXPECT_EQ(gcp_from_moved.X(), -3.5);
            EXPECT_EQ(gcp_from_moved.Y(), -4.5);
            EXPECT_EQ(gcp_from_moved.Z(), -5.5);
        }

        {
            gdal::GCP gcp_copy(gcp);
            gdal::GCP gcp_from_moved;
            gcp_from_moved = std::move(gcp_copy);
            EXPECT_STREQ(gcp_from_moved.Id(), "id2");
            EXPECT_STREQ(gcp_from_moved.Info(), "info2");
            EXPECT_EQ(gcp_from_moved.Pixel(), -1.5);
            EXPECT_EQ(gcp_from_moved.Line(), -2.5);
            EXPECT_EQ(gcp_from_moved.X(), -3.5);
            EXPECT_EQ(gcp_from_moved.Y(), -4.5);
            EXPECT_EQ(gcp_from_moved.Z(), -5.5);
        }

        {
            const GDAL_GCP *c_gcp = gcp.c_ptr();
            EXPECT_STREQ(c_gcp->pszId, "id2");
            EXPECT_STREQ(c_gcp->pszInfo, "info2");
            EXPECT_EQ(c_gcp->dfGCPPixel, -1.5);
            EXPECT_EQ(c_gcp->dfGCPLine, -2.5);
            EXPECT_EQ(c_gcp->dfGCPX, -3.5);
            EXPECT_EQ(c_gcp->dfGCPY, -4.5);
            EXPECT_EQ(c_gcp->dfGCPZ, -5.5);

            const gdal::GCP gcp_from_c(*c_gcp);
            EXPECT_STREQ(gcp_from_c.Id(), "id2");
            EXPECT_STREQ(gcp_from_c.Info(), "info2");
            EXPECT_EQ(gcp_from_c.Pixel(), -1.5);
            EXPECT_EQ(gcp_from_c.Line(), -2.5);
            EXPECT_EQ(gcp_from_c.X(), -3.5);
            EXPECT_EQ(gcp_from_c.Y(), -4.5);
            EXPECT_EQ(gcp_from_c.Z(), -5.5);
        }
    }

    {
        const std::vector<gdal::GCP> gcps{
            gdal::GCP{nullptr, nullptr, 0, 0, 0, 0, 0},
            gdal::GCP{"id", "info", 1.5, 2.5, 3.5, 4.5, 5.5}};

        const GDAL_GCP *c_gcps = gdal::GCP::c_ptr(gcps);
        EXPECT_STREQ(c_gcps[1].pszId, "id");
        EXPECT_STREQ(c_gcps[1].pszInfo, "info");
        EXPECT_EQ(c_gcps[1].dfGCPPixel, 1.5);
        EXPECT_EQ(c_gcps[1].dfGCPLine, 2.5);
        EXPECT_EQ(c_gcps[1].dfGCPX, 3.5);
        EXPECT_EQ(c_gcps[1].dfGCPY, 4.5);
        EXPECT_EQ(c_gcps[1].dfGCPZ, 5.5);

        const auto gcps_from_c =
            gdal::GCP::fromC(c_gcps, static_cast<int>(gcps.size()));
        ASSERT_EQ(gcps_from_c.size(), gcps.size());
        for (size_t i = 0; i < gcps.size(); ++i)
        {
            EXPECT_STREQ(gcps_from_c[i].Id(), gcps[i].Id());
            EXPECT_STREQ(gcps_from_c[i].Info(), gcps[i].Info());
            EXPECT_EQ(gcps_from_c[i].Pixel(), gcps[i].Pixel());
            EXPECT_EQ(gcps_from_c[i].Line(), gcps[i].Line());
            EXPECT_EQ(gcps_from_c[i].X(), gcps[i].X());
            EXPECT_EQ(gcps_from_c[i].Y(), gcps[i].Y());
            EXPECT_EQ(gcps_from_c[i].Z(), gcps[i].Z());
        }
    }
}

TEST_F(test_gdal, RasterIO_gdt_unknown)
{
    GDALDatasetUniquePtr poDS(GDALDriver::FromHandle(GDALGetDriverByName("MEM"))
                                  ->Create("", 1, 1, 1, GDT_Float64, nullptr));
    CPLErrorHandlerPusher oErrorHandler(CPLQuietErrorHandler);
    GByte b = 0;
    GDALRasterIOExtraArg sExtraArg;
    INIT_RASTERIO_EXTRA_ARG(sExtraArg);
    EXPECT_EQ(poDS->RasterIO(GF_Read, 0, 0, 1, 1, &b, 1, 1, GDT_Unknown, 1,
                             nullptr, 0, 0, 0, &sExtraArg),
              CE_Failure);
    EXPECT_EQ(poDS->RasterIO(GF_Read, 0, 0, 1, 1, &b, 1, 1, GDT_TypeCount, 1,
                             nullptr, 0, 0, 0, &sExtraArg),
              CE_Failure);
    EXPECT_EQ(poDS->GetRasterBand(1)->RasterIO(GF_Read, 0, 0, 1, 1, &b, 1, 1,
                                               GDT_Unknown, 0, 0, &sExtraArg),
              CE_Failure);
    EXPECT_EQ(poDS->GetRasterBand(1)->RasterIO(GF_Read, 0, 0, 1, 1, &b, 1, 1,
                                               GDT_TypeCount, 0, 0, &sExtraArg),
              CE_Failure);
}

TEST_F(test_gdal, CopyWords_gdt_unknown)
{
    CPLErrorHandlerPusher oErrorHandler(CPLQuietErrorHandler);
    GByte b = 0;
    GByte b2 = 0;
    CPLErrorReset();
    GDALCopyWords(&b, GDT_Byte, 0, &b2, GDT_Unknown, 0, 1);
    EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
    CPLErrorReset();
    GDALCopyWords(&b, GDT_Unknown, 0, &b2, GDT_Byte, 0, 1);
    EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
}

// Test GDALRasterBand::ReadRaster
TEST_F(test_gdal, ReadRaster)
{
    GDALDatasetUniquePtr poDS(GDALDriver::FromHandle(GDALGetDriverByName("MEM"))
                                  ->Create("", 2, 3, 1, GDT_Float64, nullptr));
    std::array<double, 6> buffer = {
        -1e300, -1,     //////////////////////////////////////////////
        1,      128,    //////////////////////////////////////////////
        32768,  1e300,  //////////////////////////////////////////////
    };
    GDALRasterIOExtraArg sExtraArg;
    INIT_RASTERIO_EXTRA_ARG(sExtraArg);
    EXPECT_EQ(poDS->GetRasterBand(1)->RasterIO(
                  GF_Write, 0, 0, 2, 3, buffer.data(), 2, 3, GDT_Float64,
                  sizeof(double), 2 * sizeof(double), &sExtraArg),
              CE_None);

    {
        std::vector<uint8_t> res;
        EXPECT_EQ(poDS->GetRasterBand(1)->ReadRaster(res), CE_None);
        const auto expected_res = std::vector<uint8_t>{0, 0, 1, 128, 255, 255};
        EXPECT_EQ(res, expected_res);

        std::fill(res.begin(), res.end(), 0);
        EXPECT_EQ(poDS->GetRasterBand(1)->ReadRaster(res, 0, 0, 2, 3, 2, 3),
                  CE_None);
        EXPECT_EQ(res, expected_res);

        std::fill(res.begin(), res.end(), 0);
        EXPECT_EQ(poDS->GetRasterBand(1)->ReadRaster(res, 0, 0, 2, 3), CE_None);
        EXPECT_EQ(res, expected_res);

#if __cplusplus >= 202002L
        std::fill(res.begin(), res.end(), 0);
        EXPECT_EQ(poDS->GetRasterBand(1)->ReadRaster(std::span<uint8_t>(res)),
                  CE_None);
        EXPECT_EQ(res, expected_res);
#endif

        std::fill(res.begin(), res.end(), 0);
        EXPECT_EQ(poDS->GetRasterBand(1)->ReadRaster(res.data()), CE_None);
        EXPECT_EQ(res, expected_res);

        std::fill(res.begin(), res.end(), 0);
        EXPECT_EQ(poDS->GetRasterBand(1)->ReadRaster(res.data(), res.size()),
                  CE_None);
        EXPECT_EQ(res, expected_res);

        CPLPushErrorHandler(CPLQuietErrorHandler);
        // Too small buffer size
        EXPECT_EQ(
            poDS->GetRasterBand(1)->ReadRaster(res.data(), res.size() - 1),
            CE_Failure);
        CPLPopErrorHandler();

        std::fill(res.begin(), res.end(), 0);
        EXPECT_EQ(
            poDS->GetRasterBand(1)->ReadRaster(res.data(), 0, 0, 0, 2, 3, 2, 3),
            CE_None);
        EXPECT_EQ(res, expected_res);

        std::fill(res.begin(), res.end(), 0);
        EXPECT_EQ(poDS->GetRasterBand(1)->ReadRaster(res.data(), 0, 0, 0, 2, 3),
                  CE_None);
        EXPECT_EQ(res, expected_res);
    }

    {
        std::vector<double> res;
        CPLPushErrorHandler(CPLQuietErrorHandler);
        // Too large nBufXSize
        EXPECT_EQ(
            poDS->GetRasterBand(1)->ReadRaster(res, 0, 0, 1, 1, UINT32_MAX, 1),
            CE_Failure);

        EXPECT_EQ(poDS->GetRasterBand(1)->ReadRaster(res.data(), UINT32_MAX, 0,
                                                     0, 1, 1, UINT32_MAX, 1),
                  CE_Failure);

        // Too large nBufYSize
        EXPECT_EQ(
            poDS->GetRasterBand(1)->ReadRaster(res, 0, 0, 1, 1, 1, UINT32_MAX),
            CE_Failure);

        EXPECT_EQ(poDS->GetRasterBand(1)->ReadRaster(res.data(), UINT32_MAX, 0,
                                                     0, 1, 1, 1, UINT32_MAX),
                  CE_Failure);

        CPLPopErrorHandler();
    }

    {
        std::vector<double> res;
        CPLPushErrorHandler(CPLQuietErrorHandler);
        // Huge nBufXSize x nBufYSize
        EXPECT_EQ(poDS->GetRasterBand(1)->ReadRaster(res, 0, 0, 1, 1, INT32_MAX,
                                                     INT32_MAX),
                  CE_Failure);
        CPLPopErrorHandler();
    }

    {
        std::vector<double> res;
        EXPECT_EQ(poDS->GetRasterBand(1)->ReadRaster(res, 1, 2, 1, 1), CE_None);
        const auto expected_res = std::vector<double>{1e300};
        EXPECT_EQ(res, expected_res);
    }

    {
        std::vector<double> res;
        CPLPushErrorHandler(CPLQuietErrorHandler);
        EXPECT_EQ(poDS->GetRasterBand(1)->ReadRaster(res, 1.1, 2.1, 0.9, 0.9),
                  CE_Failure);
        CPLPopErrorHandler();

        EXPECT_EQ(
            poDS->GetRasterBand(1)->ReadRaster(res, 1.1, 2.1, 0.9, 0.9, 1, 1),
            CE_None);
        const auto expected_res = std::vector<double>{1e300};
        EXPECT_EQ(res, expected_res);
    }

    {
        std::vector<double> res;
        EXPECT_EQ(poDS->GetRasterBand(1)->ReadRaster(res, 0.4, 0.5, 1.4, 1.5, 1,
                                                     1, GRIORA_Bilinear),
                  CE_None);
        ASSERT_EQ(res.size(), 1U);
        const double expected_res = -8.64198e+298;
        EXPECT_NEAR(res[0], expected_res, std::fabs(expected_res) * 1e-6);
    }

    // Test int8_t
    {
        std::vector<int8_t> res;
        EXPECT_EQ(poDS->GetRasterBand(1)->ReadRaster(res), CE_None);
        const auto expected_res =
            std::vector<int8_t>{-128, -1, 1, 127, 127, 127};
        EXPECT_EQ(res, expected_res);

        std::fill(res.begin(), res.end(), 0);
        EXPECT_EQ(poDS->GetRasterBand(1)->ReadRaster(res.data()), CE_None);
        EXPECT_EQ(res, expected_res);
    }

    // Test uint16_t
    {
        std::vector<uint16_t> res;
        EXPECT_EQ(poDS->GetRasterBand(1)->ReadRaster(res), CE_None);
        const auto expected_res =
            std::vector<uint16_t>{0, 0, 1, 128, 32768, 65535};
        EXPECT_EQ(res, expected_res);

        std::fill(res.begin(), res.end(), 0);
        EXPECT_EQ(poDS->GetRasterBand(1)->ReadRaster(res.data()), CE_None);
        EXPECT_EQ(res, expected_res);

        std::fill(res.begin(), res.end(), 0);
        EXPECT_EQ(poDS->GetRasterBand(1)->ReadRaster(res.data(), res.size()),
                  CE_None);
        EXPECT_EQ(res, expected_res);
    }

    // Test int16_t
    {
        std::vector<int16_t> res;
        EXPECT_EQ(poDS->GetRasterBand(1)->ReadRaster(res), CE_None);
        const auto expected_res =
            std::vector<int16_t>{-32768, -1, 1, 128, 32767, 32767};
        EXPECT_EQ(res, expected_res);

        std::fill(res.begin(), res.end(), 0);
        EXPECT_EQ(poDS->GetRasterBand(1)->ReadRaster(res.data()), CE_None);
        EXPECT_EQ(res, expected_res);
    }

#if 0
    // Not allowed by C++ standard
    // Test complex<int16_t>
    {
        std::vector<std::complex<int16_t>> res;
        EXPECT_EQ(poDS->GetRasterBand(1)->ReadRaster(res), CE_None);
        const auto expected_res = std::vector<std::complex<int16_t>>{
            -32768, -1, 1, 128, 32767, 32767};
        EXPECT_EQ(res, expected_res);

        std::fill(res.begin(), res.end(), 0);
        EXPECT_EQ(poDS->GetRasterBand(1)->ReadRaster(res.data()), CE_None);
        EXPECT_EQ(res, expected_res);
    }
#endif

    // Test uint32_t
    {
        std::vector<uint32_t> res;
        EXPECT_EQ(poDS->GetRasterBand(1)->ReadRaster(res), CE_None);
        const auto expected_res =
            std::vector<uint32_t>{0, 0, 1, 128, 32768, UINT32_MAX};
        EXPECT_EQ(res, expected_res);

        std::fill(res.begin(), res.end(), 0);
        EXPECT_EQ(poDS->GetRasterBand(1)->ReadRaster(res.data()), CE_None);
        EXPECT_EQ(res, expected_res);
    }

    // Test int32_t
    {
        std::vector<int32_t> res;
        EXPECT_EQ(poDS->GetRasterBand(1)->ReadRaster(res), CE_None);
        const auto expected_res =
            std::vector<int32_t>{INT32_MIN, -1, 1, 128, 32768, INT32_MAX};
        EXPECT_EQ(res, expected_res);

        std::fill(res.begin(), res.end(), 0);
        EXPECT_EQ(poDS->GetRasterBand(1)->ReadRaster(res.data()), CE_None);
        EXPECT_EQ(res, expected_res);
    }

#if 0
    // Not allowed by C++ standard
    // Test complex<int32_t>
    {
        std::vector<std::complex<int32_t>> res;
        EXPECT_EQ(poDS->GetRasterBand(1)->ReadRaster(res), CE_None);
        const auto expected_res = std::vector<std::complex<int32_t>>{
            INT32_MIN, -1, 1, 128, 32768, INT32_MAX};
        EXPECT_EQ(res, expected_res);

        std::fill(res.begin(), res.end(), 0);
        EXPECT_EQ(poDS->GetRasterBand(1)->ReadRaster(res.data()), CE_None);
        EXPECT_EQ(res, expected_res);
    }
#endif

    // Test uint64_t
    {
        std::vector<uint64_t> res;
        EXPECT_EQ(poDS->GetRasterBand(1)->ReadRaster(res), CE_None);
        const auto expected_res =
            std::vector<uint64_t>{0, 0, 1, 128, 32768, UINT64_MAX};
        EXPECT_EQ(res, expected_res);

        std::fill(res.begin(), res.end(), 0);
        EXPECT_EQ(poDS->GetRasterBand(1)->ReadRaster(res.data()), CE_None);
        EXPECT_EQ(res, expected_res);
    }

    // Test int64_t
    {
        std::vector<int64_t> res;
        EXPECT_EQ(poDS->GetRasterBand(1)->ReadRaster(res), CE_None);
        const auto expected_res =
            std::vector<int64_t>{INT64_MIN, -1, 1, 128, 32768, INT64_MAX};
        EXPECT_EQ(res, expected_res);

        std::fill(res.begin(), res.end(), 0);
        EXPECT_EQ(poDS->GetRasterBand(1)->ReadRaster(res.data()), CE_None);
        EXPECT_EQ(res, expected_res);
    }

    // Test float
    {
        std::vector<float> res;
        EXPECT_EQ(poDS->GetRasterBand(1)->ReadRaster(res), CE_None);
        const auto expected_res =
            std::vector<float>{-std::numeric_limits<float>::infinity(),
                               -1.0f,
                               1.0f,
                               128.0f,
                               32768.0f,
                               std::numeric_limits<float>::infinity()};
        EXPECT_EQ(res, expected_res);

        std::fill(res.begin(), res.end(), 0.0f);
        EXPECT_EQ(poDS->GetRasterBand(1)->ReadRaster(res.data()), CE_None);
        EXPECT_EQ(res, expected_res);
    }

    // Test complex<float>
    {
        std::vector<std::complex<float>> res;
        EXPECT_EQ(poDS->GetRasterBand(1)->ReadRaster(res), CE_None);
        const auto expected_res = std::vector<std::complex<float>>{
            -std::numeric_limits<float>::infinity(),
            -1.0f,
            1.0f,
            128.0f,
            32768.0f,
            std::numeric_limits<float>::infinity()};
        EXPECT_EQ(res, expected_res);

        std::fill(res.begin(), res.end(), 0.0f);
        EXPECT_EQ(poDS->GetRasterBand(1)->ReadRaster(res.data()), CE_None);
        EXPECT_EQ(res, expected_res);
    }

    // Test double
    {
        std::vector<double> res;
        EXPECT_EQ(poDS->GetRasterBand(1)->ReadRaster(res), CE_None);
        const auto expected_res =
            std::vector<double>{-1e300, -1, 1, 128, 32768, 1e300};
        EXPECT_EQ(res, expected_res);

        std::fill(res.begin(), res.end(), 0);
        EXPECT_EQ(poDS->GetRasterBand(1)->ReadRaster(res.data()), CE_None);
        EXPECT_EQ(res, expected_res);
    }

    // Test complex<double>
    {
        std::vector<std::complex<double>> res;
        EXPECT_EQ(poDS->GetRasterBand(1)->ReadRaster(res), CE_None);
        const auto expected_res =
            std::vector<std::complex<double>>{-1e300, -1, 1, 128, 32768, 1e300};
        EXPECT_EQ(res, expected_res);

        std::fill(res.begin(), res.end(), 0);
        EXPECT_EQ(poDS->GetRasterBand(1)->ReadRaster(res.data()), CE_None);
        EXPECT_EQ(res, expected_res);
    }
}

// Test GDALComputeRasterMinMaxLocation
TEST_F(test_gdal, GDALComputeRasterMinMaxLocation)
{
    GDALDatasetH hDS = GDALOpen(GCORE_DATA_DIR "byte.tif", GA_ReadOnly);
    ASSERT_NE(hDS, nullptr);
    GDALRasterBandH hBand = GDALGetRasterBand(hDS, 1);
    {
        double dfMin = 0;
        double dfMax = 0;
        int nMinX = -1;
        int nMinY = -1;
        int nMaxX = -1;
        int nMaxY = -1;
        EXPECT_EQ(GDALComputeRasterMinMaxLocation(hBand, &dfMin, &dfMax, &nMinX,
                                                  &nMinY, &nMaxX, &nMaxY),
                  CE_None);
        EXPECT_EQ(dfMin, 74.0);
        EXPECT_EQ(dfMax, 255.0);
        EXPECT_EQ(nMinX, 9);
        EXPECT_EQ(nMinY, 17);
        EXPECT_EQ(nMaxX, 2);
        EXPECT_EQ(nMaxY, 18);
        GByte val = 0;
        EXPECT_EQ(GDALRasterIO(hBand, GF_Read, nMinX, nMinY, 1, 1, &val, 1, 1,
                               GDT_Byte, 0, 0),
                  CE_None);
        EXPECT_EQ(val, 74);
        EXPECT_EQ(GDALRasterIO(hBand, GF_Read, nMaxX, nMaxY, 1, 1, &val, 1, 1,
                               GDT_Byte, 0, 0),
                  CE_None);
        EXPECT_EQ(val, 255);
    }
    {
        int nMinX = -1;
        int nMinY = -1;
        EXPECT_EQ(GDALComputeRasterMinMaxLocation(hBand, nullptr, nullptr,
                                                  &nMinX, &nMinY, nullptr,
                                                  nullptr),
                  CE_None);
        EXPECT_EQ(nMinX, 9);
        EXPECT_EQ(nMinY, 17);
    }
    {
        int nMaxX = -1;
        int nMaxY = -1;
        EXPECT_EQ(GDALComputeRasterMinMaxLocation(hBand, nullptr, nullptr,
                                                  nullptr, nullptr, &nMaxX,
                                                  &nMaxY),
                  CE_None);
        EXPECT_EQ(nMaxX, 2);
        EXPECT_EQ(nMaxY, 18);
    }
    {
        EXPECT_EQ(GDALComputeRasterMinMaxLocation(hBand, nullptr, nullptr,
                                                  nullptr, nullptr, nullptr,
                                                  nullptr),
                  CE_None);
    }
    GDALClose(hDS);
}

// Test GDALComputeRasterMinMaxLocation
TEST_F(test_gdal, GDALComputeRasterMinMaxLocation_byte_min_max_optim)
{
    GDALDatasetUniquePtr poDS(GDALDriver::FromHandle(GDALGetDriverByName("MEM"))
                                  ->Create("", 1, 4, 1, GDT_Byte, nullptr));
    std::array<uint8_t, 4> buffer = {
        1,    //////////////////////////////////////////////////////////
        0,    //////////////////////////////////////////////////////////
        255,  //////////////////////////////////////////////////////////
        1,    //////////////////////////////////////////////////////////
    };
    GDALRasterIOExtraArg sExtraArg;
    INIT_RASTERIO_EXTRA_ARG(sExtraArg);
    EXPECT_EQ(poDS->GetRasterBand(1)->RasterIO(
                  GF_Write, 0, 0, 1, 4, buffer.data(), 1, 4, GDT_Byte,
                  sizeof(uint8_t), 1 * sizeof(uint8_t), &sExtraArg),
              CE_None);

    double dfMin = 0;
    double dfMax = 0;
    int nMinX = -1;
    int nMinY = -1;
    int nMaxX = -1;
    int nMaxY = -1;
    EXPECT_EQ(poDS->GetRasterBand(1)->ComputeRasterMinMaxLocation(
                  &dfMin, &dfMax, &nMinX, &nMinY, &nMaxX, &nMaxY),
              CE_None);
    EXPECT_EQ(dfMin, 0);
    EXPECT_EQ(dfMax, 255);
    EXPECT_EQ(nMinX, 0);
    EXPECT_EQ(nMinY, 1);
    EXPECT_EQ(nMaxX, 0);
    EXPECT_EQ(nMaxY, 2);
}

// Test GDALComputeRasterMinMaxLocation
TEST_F(test_gdal, GDALComputeRasterMinMaxLocation_with_mask)
{
    GDALDatasetUniquePtr poDS(GDALDriver::FromHandle(GDALGetDriverByName("MEM"))
                                  ->Create("", 2, 2, 1, GDT_Byte, nullptr));
    std::array<uint8_t, 6> buffer = {
        2, 10,  //////////////////////////////////////////////////////////
        4, 20,  //////////////////////////////////////////////////////////
    };
    GDALRasterIOExtraArg sExtraArg;
    INIT_RASTERIO_EXTRA_ARG(sExtraArg);
    EXPECT_EQ(poDS->GetRasterBand(1)->RasterIO(
                  GF_Write, 0, 0, 2, 2, buffer.data(), 2, 2, GDT_Byte,
                  sizeof(uint8_t), 2 * sizeof(uint8_t), &sExtraArg),
              CE_None);

    poDS->GetRasterBand(1)->CreateMaskBand(0);
    std::array<uint8_t, 6> buffer_mask = {
        0, 255,  //////////////////////////////////////////////////////////
        255, 0,  //////////////////////////////////////////////////////////
    };
    EXPECT_EQ(poDS->GetRasterBand(1)->GetMaskBand()->RasterIO(
                  GF_Write, 0, 0, 2, 2, buffer_mask.data(), 2, 2, GDT_Byte,
                  sizeof(uint8_t), 2 * sizeof(uint8_t), &sExtraArg),
              CE_None);

    double dfMin = 0;
    double dfMax = 0;
    int nMinX = -1;
    int nMinY = -1;
    int nMaxX = -1;
    int nMaxY = -1;
    EXPECT_EQ(poDS->GetRasterBand(1)->ComputeRasterMinMaxLocation(
                  &dfMin, &dfMax, &nMinX, &nMinY, &nMaxX, &nMaxY),
              CE_None);
    EXPECT_EQ(dfMin, 4);
    EXPECT_EQ(dfMax, 10);
    EXPECT_EQ(nMinX, 0);
    EXPECT_EQ(nMinY, 1);
    EXPECT_EQ(nMaxX, 1);
    EXPECT_EQ(nMaxY, 0);
}

TEST_F(test_gdal, GDALTranspose2D)
{
    constexpr int COUNT = 6;
    const GByte abyData[] = {1, 2, 3, 4, 5, 6};
    GByte abySrcData[COUNT * 2 * sizeof(double)];
    GByte abyDstData[COUNT * 2 * sizeof(double)];
    GByte abyDstAsByteData[COUNT * 2 * sizeof(double)];
    for (int eSrcDTInt = GDT_Byte; eSrcDTInt < GDT_TypeCount; ++eSrcDTInt)
    {
        const auto eSrcDT = static_cast<GDALDataType>(eSrcDTInt);
        GDALCopyWords(abyData, GDT_Byte, 1, abySrcData, eSrcDT,
                      GDALGetDataTypeSizeBytes(eSrcDT), COUNT);
        for (int eDstDTInt = GDT_Byte; eDstDTInt < GDT_TypeCount; ++eDstDTInt)
        {
            const auto eDstDT = static_cast<GDALDataType>(eDstDTInt);
            memset(abyDstData, 0, sizeof(abyDstData));
            GDALTranspose2D(abySrcData, eSrcDT, abyDstData, eDstDT, 3, 2);

            memset(abyDstAsByteData, 0, sizeof(abyDstAsByteData));
            GDALCopyWords(abyDstData, eDstDT, GDALGetDataTypeSizeBytes(eDstDT),
                          abyDstAsByteData, GDT_Byte, 1, COUNT);

            EXPECT_EQ(abyDstAsByteData[0], 1)
                << "eSrcDT=" << eSrcDT << ", eDstDT=" << eDstDT;
            EXPECT_EQ(abyDstAsByteData[1], 4)
                << "eSrcDT=" << eSrcDT << ", eDstDT=" << eDstDT;
            EXPECT_EQ(abyDstAsByteData[2], 2)
                << "eSrcDT=" << eSrcDT << ", eDstDT=" << eDstDT;
            EXPECT_EQ(abyDstAsByteData[3], 5)
                << "eSrcDT=" << eSrcDT << ", eDstDT=" << eDstDT;
            EXPECT_EQ(abyDstAsByteData[4], 3)
                << "eSrcDT=" << eSrcDT << ", eDstDT=" << eDstDT;
            EXPECT_EQ(abyDstAsByteData[5], 6)
                << "eSrcDT=" << eSrcDT << ", eDstDT=" << eDstDT;
        }
    }
}

TEST_F(test_gdal, GDALTranspose2D_Byte_optims)
{
    std::vector<GByte> in;
    for (int i = 0; i < 19 * 17; ++i)
        in.push_back(static_cast<GByte>(i % 256));

    std::vector<GByte> out(in.size());

    // SSSE3 optim (16x16) blocks
    {
        constexpr int W = 19;
        constexpr int H = 17;
        GDALTranspose2D(in.data(), GDT_Byte, out.data(), GDT_Byte, W, H);
        for (int y = 0; y < H; ++y)
        {
            for (int x = 0; x < W; ++x)
            {
                EXPECT_EQ(out[x * H + y], in[y * W + x]);
            }
        }
    }

    // Optim H = 2
    {
        constexpr int W = 19;
        constexpr int H = 2;
        GDALTranspose2D(in.data(), GDT_Byte, out.data(), GDT_Byte, W, H);
        for (int y = 0; y < H; ++y)
        {
            for (int x = 0; x < W; ++x)
            {
                EXPECT_EQ(out[x * H + y], in[y * W + x]);
            }
        }
    }

    // SSSE3 optim H = 3
    {
        constexpr int W = 19;
        constexpr int H = 3;
        GDALTranspose2D(in.data(), GDT_Byte, out.data(), GDT_Byte, W, H);
        for (int y = 0; y < H; ++y)
        {
            for (int x = 0; x < W; ++x)
            {
                EXPECT_EQ(out[x * H + y], in[y * W + x]);
            }
        }
    }

    // Optim H = 4
    {
        constexpr int W = 19;
        constexpr int H = 4;
        GDALTranspose2D(in.data(), GDT_Byte, out.data(), GDT_Byte, W, H);
        for (int y = 0; y < H; ++y)
        {
            for (int x = 0; x < W; ++x)
            {
                EXPECT_EQ(out[x * H + y], in[y * W + x]);
            }
        }
    }

    // SSSE3 optim H = 5 with W < 16
    {
        constexpr int W = 15;
        constexpr int H = 5;
        GDALTranspose2D(in.data(), GDT_Byte, out.data(), GDT_Byte, W, H);
        for (int y = 0; y < H; ++y)
        {
            for (int x = 0; x < W; ++x)
            {
                EXPECT_EQ(out[x * H + y], in[y * W + x]);
            }
        }
    }

    // SSSE3 optim H = 5 with W >= 16
    {
        constexpr int W = 19;
        constexpr int H = 5;
        GDALTranspose2D(in.data(), GDT_Byte, out.data(), GDT_Byte, W, H);
        for (int y = 0; y < H; ++y)
        {
            for (int x = 0; x < W; ++x)
            {
                EXPECT_EQ(out[x * H + y], in[y * W + x]);
            }
        }
    }
}

}  // namespace
