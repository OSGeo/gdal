// SPDX-License-Identifier: MIT
// Copyright 2024, Even Rouault <even.rouault at spatialys.com>

#include "gdal_unit_test.h"

#include "gdal_typetraits.h"

#include "gtest_include.h"

namespace
{

struct test_gdal_typetraits : public ::testing::Test
{
};

TEST_F(test_gdal_typetraits, CXXTypeTraits)
{
    static_assert(gdal::CXXTypeTraits<int8_t>::gdal_type == GDT_Int8);
    static_assert(gdal::CXXTypeTraits<int8_t>::size == 1);
    EXPECT_EQ(
        gdal::CXXTypeTraits<int8_t>::GetExtendedDataType().GetNumericDataType(),
        GDT_Int8);
    static_assert(gdal::CXXTypeTraits<uint8_t>::gdal_type == GDT_Byte);
    static_assert(gdal::CXXTypeTraits<uint8_t>::size == 1);
    EXPECT_EQ(gdal::CXXTypeTraits<uint8_t>::GetExtendedDataType()
                  .GetNumericDataType(),
              GDT_Byte);
    static_assert(gdal::CXXTypeTraits<int16_t>::gdal_type == GDT_Int16);
    static_assert(gdal::CXXTypeTraits<int16_t>::size == 2);
    EXPECT_EQ(gdal::CXXTypeTraits<int16_t>::GetExtendedDataType()
                  .GetNumericDataType(),
              GDT_Int16);
    static_assert(gdal::CXXTypeTraits<uint16_t>::gdal_type == GDT_UInt16);
    static_assert(gdal::CXXTypeTraits<uint16_t>::size == 2);
    EXPECT_EQ(gdal::CXXTypeTraits<uint16_t>::GetExtendedDataType()
                  .GetNumericDataType(),
              GDT_UInt16);
    static_assert(gdal::CXXTypeTraits<int32_t>::gdal_type == GDT_Int32);
    static_assert(gdal::CXXTypeTraits<int32_t>::size == 4);
    EXPECT_EQ(gdal::CXXTypeTraits<int32_t>::GetExtendedDataType()
                  .GetNumericDataType(),
              GDT_Int32);
    static_assert(gdal::CXXTypeTraits<uint32_t>::gdal_type == GDT_UInt32);
    static_assert(gdal::CXXTypeTraits<uint32_t>::size == 4);
    EXPECT_EQ(gdal::CXXTypeTraits<uint32_t>::GetExtendedDataType()
                  .GetNumericDataType(),
              GDT_UInt32);
    static_assert(gdal::CXXTypeTraits<int64_t>::gdal_type == GDT_Int64);
    static_assert(gdal::CXXTypeTraits<int64_t>::size == 8);
    EXPECT_EQ(gdal::CXXTypeTraits<int64_t>::GetExtendedDataType()
                  .GetNumericDataType(),
              GDT_Int64);
    static_assert(gdal::CXXTypeTraits<uint64_t>::gdal_type == GDT_UInt64);
    static_assert(gdal::CXXTypeTraits<uint64_t>::size == 8);
    EXPECT_EQ(gdal::CXXTypeTraits<uint64_t>::GetExtendedDataType()
                  .GetNumericDataType(),
              GDT_UInt64);
    static_assert(gdal::CXXTypeTraits<float>::gdal_type == GDT_Float32);
    static_assert(gdal::CXXTypeTraits<float>::size == 4);
    EXPECT_EQ(
        gdal::CXXTypeTraits<float>::GetExtendedDataType().GetNumericDataType(),
        GDT_Float32);
    static_assert(gdal::CXXTypeTraits<double>::gdal_type == GDT_Float64);
    static_assert(gdal::CXXTypeTraits<double>::size == 8);
    EXPECT_EQ(
        gdal::CXXTypeTraits<double>::GetExtendedDataType().GetNumericDataType(),
        GDT_Float64);
    static_assert(gdal::CXXTypeTraits<std::complex<float>>::gdal_type ==
                  GDT_CFloat32);
    static_assert(gdal::CXXTypeTraits<std::complex<float>>::size == 8);
    EXPECT_EQ(gdal::CXXTypeTraits<std::complex<float>>::GetExtendedDataType()
                  .GetNumericDataType(),
              GDT_CFloat32);
    static_assert(gdal::CXXTypeTraits<std::complex<double>>::gdal_type ==
                  GDT_CFloat64);
    static_assert(gdal::CXXTypeTraits<std::complex<double>>::size == 16);
    EXPECT_EQ(gdal::CXXTypeTraits<std::complex<double>>::GetExtendedDataType()
                  .GetNumericDataType(),
              GDT_CFloat64);
    static_assert(gdal::CXXTypeTraits<std::string>::size == 0);
    EXPECT_EQ(
        gdal::CXXTypeTraits<std::string>::GetExtendedDataType().GetClass(),
        GEDTC_STRING);
}

TEST_F(test_gdal_typetraits, GDALDataTypeTraits)
{
    EXPECT_EQ(gdal::GDALDataTypeTraits<GDT_Byte>::GetExtendedDataType()
                  .GetNumericDataType(),
              GDT_Byte);
    static_assert(
        std::is_same_v<gdal::GDALDataTypeTraits<GDT_Byte>::type, uint8_t>);
    static_assert(gdal::GDALDataTypeTraits<GDT_Byte>::size == 1);
    EXPECT_EQ(gdal::GDALDataTypeTraits<GDT_Int8>::GetExtendedDataType()
                  .GetNumericDataType(),
              GDT_Int8);
    static_assert(
        std::is_same_v<gdal::GDALDataTypeTraits<GDT_Int8>::type, int8_t>);
    static_assert(gdal::GDALDataTypeTraits<GDT_Int8>::size == 1);
    EXPECT_EQ(gdal::GDALDataTypeTraits<GDT_Int16>::GetExtendedDataType()
                  .GetNumericDataType(),
              GDT_Int16);
    static_assert(
        std::is_same_v<gdal::GDALDataTypeTraits<GDT_Int16>::type, int16_t>);
    static_assert(gdal::GDALDataTypeTraits<GDT_Int16>::size == 2);
    EXPECT_EQ(gdal::GDALDataTypeTraits<GDT_UInt16>::GetExtendedDataType()
                  .GetNumericDataType(),
              GDT_UInt16);
    static_assert(
        std::is_same_v<gdal::GDALDataTypeTraits<GDT_UInt16>::type, uint16_t>);
    static_assert(gdal::GDALDataTypeTraits<GDT_UInt16>::size == 2);
    EXPECT_EQ(gdal::GDALDataTypeTraits<GDT_Int32>::GetExtendedDataType()
                  .GetNumericDataType(),
              GDT_Int32);
    static_assert(
        std::is_same_v<gdal::GDALDataTypeTraits<GDT_Int32>::type, int32_t>);
    static_assert(gdal::GDALDataTypeTraits<GDT_Int32>::size == 4);
    EXPECT_EQ(gdal::GDALDataTypeTraits<GDT_UInt32>::GetExtendedDataType()
                  .GetNumericDataType(),
              GDT_UInt32);
    static_assert(
        std::is_same_v<gdal::GDALDataTypeTraits<GDT_UInt32>::type, uint32_t>);
    static_assert(gdal::GDALDataTypeTraits<GDT_UInt32>::size == 4);
    EXPECT_EQ(gdal::GDALDataTypeTraits<GDT_Int64>::GetExtendedDataType()
                  .GetNumericDataType(),
              GDT_Int64);
    static_assert(
        std::is_same_v<gdal::GDALDataTypeTraits<GDT_Int64>::type, int64_t>);
    static_assert(gdal::GDALDataTypeTraits<GDT_Int64>::size == 8);
    EXPECT_EQ(gdal::GDALDataTypeTraits<GDT_UInt64>::GetExtendedDataType()
                  .GetNumericDataType(),
              GDT_UInt64);
    static_assert(
        std::is_same_v<gdal::GDALDataTypeTraits<GDT_UInt64>::type, uint64_t>);
    static_assert(gdal::GDALDataTypeTraits<GDT_UInt64>::size == 8);
    EXPECT_EQ(gdal::GDALDataTypeTraits<GDT_Float32>::GetExtendedDataType()
                  .GetNumericDataType(),
              GDT_Float32);
    static_assert(
        std::is_same_v<gdal::GDALDataTypeTraits<GDT_Float32>::type, float>);
    static_assert(gdal::GDALDataTypeTraits<GDT_Float32>::size == 4);
    EXPECT_EQ(gdal::GDALDataTypeTraits<GDT_Float64>::GetExtendedDataType()
                  .GetNumericDataType(),
              GDT_Float64);
    static_assert(
        std::is_same_v<gdal::GDALDataTypeTraits<GDT_Float64>::type, double>);
    static_assert(gdal::GDALDataTypeTraits<GDT_Float64>::size == 8);
    EXPECT_EQ(gdal::GDALDataTypeTraits<GDT_CInt16>::GetExtendedDataType()
                  .GetNumericDataType(),
              GDT_CInt16);
    EXPECT_EQ(gdal::GDALDataTypeTraits<GDT_CInt32>::GetExtendedDataType()
                  .GetNumericDataType(),
              GDT_CInt32);
    EXPECT_EQ(gdal::GDALDataTypeTraits<GDT_CFloat32>::GetExtendedDataType()
                  .GetNumericDataType(),
              GDT_CFloat32);
    static_assert(std::is_same_v<gdal::GDALDataTypeTraits<GDT_CFloat32>::type,
                                 std::complex<float>>);
    static_assert(gdal::GDALDataTypeTraits<GDT_CFloat32>::size == 8);
    EXPECT_EQ(gdal::GDALDataTypeTraits<GDT_CFloat64>::GetExtendedDataType()
                  .GetNumericDataType(),
              GDT_CFloat64);
    static_assert(std::is_same_v<gdal::GDALDataTypeTraits<GDT_CFloat64>::type,
                                 std::complex<double>>);
    static_assert(gdal::GDALDataTypeTraits<GDT_CFloat64>::size == 16);
}

TEST_F(test_gdal_typetraits, GetOGRFieldType)
{
    EXPECT_EQ(gdal::GetOGRFieldType(GDT_Byte), OFTInteger);
    EXPECT_EQ(gdal::GetOGRFieldType(GDT_Int8), OFTInteger);
    EXPECT_EQ(gdal::GetOGRFieldType(GDT_Int16), OFTInteger);
    EXPECT_EQ(gdal::GetOGRFieldType(GDT_Int32), OFTInteger);
    EXPECT_EQ(gdal::GetOGRFieldType(GDT_UInt16), OFTInteger);
    EXPECT_EQ(gdal::GetOGRFieldType(GDT_UInt32), OFTInteger64);
    EXPECT_EQ(gdal::GetOGRFieldType(GDT_Int64), OFTInteger64);
    EXPECT_EQ(gdal::GetOGRFieldType(GDT_UInt64), OFTReal);
    EXPECT_EQ(gdal::GetOGRFieldType(GDT_Float32), OFTReal);
    EXPECT_EQ(gdal::GetOGRFieldType(GDT_Float64), OFTReal);
    EXPECT_EQ(gdal::GetOGRFieldType(GDT_CInt16), OFTMaxType);
    EXPECT_EQ(gdal::GetOGRFieldType(GDT_CInt32), OFTMaxType);
    EXPECT_EQ(gdal::GetOGRFieldType(GDT_CFloat32), OFTMaxType);
    EXPECT_EQ(gdal::GetOGRFieldType(GDT_CFloat64), OFTMaxType);
    EXPECT_EQ(gdal::GetOGRFieldType(GDT_Unknown), OFTMaxType);
    EXPECT_EQ(gdal::GetOGRFieldType(GDT_TypeCount), OFTMaxType);

    EXPECT_EQ(gdal::GetOGRFieldType(GDALExtendedDataType::Create(GDT_Byte)),
              OFTInteger);
    EXPECT_EQ(gdal::GetOGRFieldType(GDALExtendedDataType::CreateString()),
              OFTString);
    EXPECT_EQ(
        gdal::GetOGRFieldType(GDALExtendedDataType::Create("compound", 0, {})),
        OFTMaxType);
}

}  // namespace
