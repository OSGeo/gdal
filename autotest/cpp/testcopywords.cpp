/******************************************************************************
 *
 * Project:  GDAL Core
 * Purpose:  Test GDALCopyWords().
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2009-2011, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_conv.h"
#include "cpl_float.h"
#include "gdal.h"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <type_traits>

#include "gtest_include.h"

namespace
{

// ---------------------------------------------------------------------------

template <class OutType, class CT1, class CT2>
void AssertRes(GDALDataType intype, CT1 inval, GDALDataType outtype,
               CT2 expected_outval, OutType outval, int numLine)
{
    if (static_cast<double>(expected_outval) == static_cast<double>(outval) ||
        (std::isnan(static_cast<double>(expected_outval)) &&
         std::isnan(static_cast<double>(outval))))
    {
        // ok
    }
    else
    {
        EXPECT_NEAR((double)outval, (double)expected_outval, 1.0)
            << "Test failed at line " << numLine
            << " (intype=" << GDALGetDataTypeName(intype)
            << ",inval=" << (double)inval
            << ",outtype=" << GDALGetDataTypeName(outtype) << ",got "
            << (double)outval << " expected  " << expected_outval;
    }
}

#define MY_EXPECT(intype, inval, outtype, expected_outval, outval)             \
    AssertRes(intype, inval, outtype, expected_outval, outval, numLine)

class TestCopyWords : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        pIn = (GByte *)malloc(2048);
        pOut = (GByte *)malloc(2048);
    }

    void TearDown() override
    {

        free(pIn);
        free(pOut);
    }

    GByte *pIn;
    GByte *pOut;

    template <class InType, class OutType, class ConstantType>
    void Test(GDALDataType intype, ConstantType inval, ConstantType invali,
              GDALDataType outtype, ConstantType outval, ConstantType outvali,
              int numLine)
    {
        memset(pIn, 0xff, 1024);
        memset(pOut, 0xff, 1024);

        *(InType *)(pIn) = (InType)inval;
        *(InType *)(pIn + 32) = (InType)inval;
        if (GDALDataTypeIsComplex(intype))
        {
            ((InType *)(pIn))[1] = (InType)invali;
            ((InType *)(pIn + 32))[1] = (InType)invali;
        }

        /* Test positive offsets */
        GDALCopyWords(pIn, intype, 32, pOut, outtype, 32, 2);

        /* Test negative offsets */
        GDALCopyWords(pIn + 32, intype, -32, pOut + 1024 - 16, outtype, -32, 2);

        MY_EXPECT(intype, inval, outtype, outval, *(OutType *)(pOut));
        MY_EXPECT(intype, inval, outtype, outval, *(OutType *)(pOut + 32));
        MY_EXPECT(intype, inval, outtype, outval,
                  *(OutType *)(pOut + 1024 - 16));
        MY_EXPECT(intype, inval, outtype, outval,
                  *(OutType *)(pOut + 1024 - 16 - 32));

        if (GDALDataTypeIsComplex(outtype))
        {
            MY_EXPECT(intype, invali, outtype, outvali, ((OutType *)(pOut))[1]);
            MY_EXPECT(intype, invali, outtype, outvali,
                      ((OutType *)(pOut + 32))[1]);

            MY_EXPECT(intype, invali, outtype, outvali,
                      ((OutType *)(pOut + 1024 - 16))[1]);
            MY_EXPECT(intype, invali, outtype, outvali,
                      ((OutType *)(pOut + 1024 - 16 - 32))[1]);
        }
        else
        {
            constexpr int N = 32 + 31;
            for (int i = 0; i < N; ++i)
            {
                *(InType *)(pIn + i * GDALGetDataTypeSizeBytes(intype)) =
                    (InType)inval;
            }

            /* Test packed offsets */
            GDALCopyWords(pIn, intype, GDALGetDataTypeSizeBytes(intype), pOut,
                          outtype, GDALGetDataTypeSizeBytes(outtype), N);

            for (int i = 0; i < N; ++i)
            {
                MY_EXPECT(
                    intype, inval, outtype, outval,
                    *(OutType *)(pOut + i * GDALGetDataTypeSizeBytes(outtype)));
            }
        }
    }

    template <class InType, class ConstantType>
    void FromR_2(GDALDataType intype, ConstantType inval, ConstantType invali,
                 GDALDataType outtype, ConstantType outval,
                 ConstantType outvali, int numLine)
    {
        if (outtype == GDT_Byte)
            Test<InType, GByte, ConstantType>(intype, inval, invali, outtype,
                                              outval, outvali, numLine);
        else if (outtype == GDT_Int8)
            Test<InType, GInt8, ConstantType>(intype, inval, invali, outtype,
                                              outval, outvali, numLine);
        else if (outtype == GDT_Int16)
            Test<InType, GInt16, ConstantType>(intype, inval, invali, outtype,
                                               outval, outvali, numLine);
        else if (outtype == GDT_UInt16)
            Test<InType, GUInt16, ConstantType>(intype, inval, invali, outtype,
                                                outval, outvali, numLine);
        else if (outtype == GDT_Int32)
            Test<InType, GInt32, ConstantType>(intype, inval, invali, outtype,
                                               outval, outvali, numLine);
        else if (outtype == GDT_UInt32)
            Test<InType, GUInt32, ConstantType>(intype, inval, invali, outtype,
                                                outval, outvali, numLine);
        else if (outtype == GDT_Int64)
            Test<InType, std::int64_t, ConstantType>(
                intype, inval, invali, outtype, outval, outvali, numLine);
        else if (outtype == GDT_UInt64)
            Test<InType, std::uint64_t, ConstantType>(
                intype, inval, invali, outtype, outval, outvali, numLine);
        else if (outtype == GDT_Float16)
            Test<InType, GFloat16, ConstantType>(intype, inval, invali, outtype,
                                                 outval, outvali, numLine);
        else if (outtype == GDT_Float32)
            Test<InType, float, ConstantType>(intype, inval, invali, outtype,
                                              outval, outvali, numLine);
        else if (outtype == GDT_Float64)
            Test<InType, double, ConstantType>(intype, inval, invali, outtype,
                                               outval, outvali, numLine);
        else if (outtype == GDT_CInt16)
            Test<InType, GInt16, ConstantType>(intype, inval, invali, outtype,
                                               outval, outvali, numLine);
        else if (outtype == GDT_CInt32)
            Test<InType, GInt32, ConstantType>(intype, inval, invali, outtype,
                                               outval, outvali, numLine);
        else if (outtype == GDT_CFloat16)
            Test<InType, GFloat16, ConstantType>(intype, inval, invali, outtype,
                                                 outval, outvali, numLine);
        else if (outtype == GDT_CFloat32)
            Test<InType, float, ConstantType>(intype, inval, invali, outtype,
                                              outval, outvali, numLine);
        else if (outtype == GDT_CFloat64)
            Test<InType, double, ConstantType>(intype, inval, invali, outtype,
                                               outval, outvali, numLine);
    }

    template <class ConstantType>
    void FromR(GDALDataType intype, ConstantType inval, ConstantType invali,
               GDALDataType outtype, ConstantType outval, ConstantType outvali,
               int numLine)
    {
        if (intype == GDT_Byte)
            FromR_2<GByte, ConstantType>(intype, inval, invali, outtype, outval,
                                         outvali, numLine);
        else if (intype == GDT_Int8)
            FromR_2<GInt8, ConstantType>(intype, inval, invali, outtype, outval,
                                         outvali, numLine);
        else if (intype == GDT_Int16)
            FromR_2<GInt16, ConstantType>(intype, inval, invali, outtype,
                                          outval, outvali, numLine);
        else if (intype == GDT_UInt16)
            FromR_2<GUInt16, ConstantType>(intype, inval, invali, outtype,
                                           outval, outvali, numLine);
        else if (intype == GDT_Int32)
            FromR_2<GInt32, ConstantType>(intype, inval, invali, outtype,
                                          outval, outvali, numLine);
        else if (intype == GDT_UInt32)
            FromR_2<GUInt32, ConstantType>(intype, inval, invali, outtype,
                                           outval, outvali, numLine);
        else if (intype == GDT_Int64)
            FromR_2<std::int64_t, ConstantType>(intype, inval, invali, outtype,
                                                outval, outvali, numLine);
        else if (intype == GDT_UInt64)
            FromR_2<std::uint64_t, ConstantType>(intype, inval, invali, outtype,
                                                 outval, outvali, numLine);
        else if (intype == GDT_Float16)
            FromR_2<GFloat16, ConstantType>(intype, inval, invali, outtype,
                                            outval, outvali, numLine);
        else if (intype == GDT_Float32)
            FromR_2<float, ConstantType>(intype, inval, invali, outtype, outval,
                                         outvali, numLine);
        else if (intype == GDT_Float64)
            FromR_2<double, ConstantType>(intype, inval, invali, outtype,
                                          outval, outvali, numLine);
        else if (intype == GDT_CInt16)
            FromR_2<GInt16, ConstantType>(intype, inval, invali, outtype,
                                          outval, outvali, numLine);
        else if (intype == GDT_CInt32)
            FromR_2<GInt32, ConstantType>(intype, inval, invali, outtype,
                                          outval, outvali, numLine);
        else if (intype == GDT_CFloat16)
            FromR_2<GFloat16, ConstantType>(intype, inval, invali, outtype,
                                            outval, outvali, numLine);
        else if (intype == GDT_CFloat32)
            FromR_2<float, ConstantType>(intype, inval, invali, outtype, outval,
                                         outvali, numLine);
        else if (intype == GDT_CFloat64)
            FromR_2<double, ConstantType>(intype, inval, invali, outtype,
                                          outval, outvali, numLine);
    }
};

#define FROM_R(intype, inval, outtype, outval)                                 \
    FromR<GIntBig>(intype, inval, 0, outtype, outval, 0, __LINE__)
#define FROM_R_F(intype, inval, outtype, outval)                               \
    FromR<double>(intype, inval, 0, outtype, outval, 0, __LINE__)

#define FROM_C(intype, inval, invali, outtype, outval, outvali)                \
    FromR<GIntBig>(intype, inval, invali, outtype, outval, outvali, __LINE__)
#define FROM_C_F(intype, inval, invali, outtype, outval, outvali)              \
    FromR<double>(intype, inval, invali, outtype, outval, outvali, __LINE__)

#define IS_UNSIGNED(x)                                                         \
    (x == GDT_Byte || x == GDT_UInt16 || x == GDT_UInt32 || x == GDT_UInt64)
#define IS_FLOAT(x)                                                            \
    (x == GDT_Float16 || x == GDT_Float32 || x == GDT_Float64 ||               \
     x == GDT_CFloat16 || x == GDT_CFloat32 || x == GDT_CFloat64)

#define CST_3000000000 (((GIntBig)3000) * 1000 * 1000)
#define CST_5000000000 (((GIntBig)5000) * 1000 * 1000)

TEST_F(TestCopyWords, GDT_Byte)
{
    /* GDT_Byte */
    for (GDALDataType outtype = GDT_Byte; outtype < GDT_TypeCount;
         outtype = (GDALDataType)(outtype + 1))
    {
        FROM_R(GDT_Byte, 0, outtype, 0);
        FROM_R(GDT_Byte, 127, outtype, 127);
        if (outtype != GDT_Int8)
            FROM_R(GDT_Byte, 255, outtype, 255);
    }

    for (int i = 0; i < 17; i++)
    {
        pIn[i] = (GByte)i;
    }

    memset(pOut, 0xff, 128);
    GDALCopyWords(pIn, GDT_Byte, 1, pOut, GDT_Int32, 4, 17);
    for (int i = 0; i < 17; i++)
    {
        AssertRes(GDT_Byte, i, GDT_Int32, i, ((int *)pOut)[i], __LINE__);
    }

    memset(pOut, 0xff, 128);
    GDALCopyWords(pIn, GDT_Byte, 1, pOut, GDT_Float32, 4, 17);
    for (int i = 0; i < 17; i++)
    {
        AssertRes(GDT_Byte, i, GDT_Float32, i, ((float *)pOut)[i], __LINE__);
    }
}

TEST_F(TestCopyWords, GDT_Int8)
{
    /* GDT_Int8 */
    FROM_R(GDT_Int8, -128, GDT_Byte, 0);    /* clamp */
    FROM_R(GDT_Int8, -128, GDT_Int8, -128); /* clamp */
    FROM_R(GDT_Int8, -128, GDT_Int16, -128);
    FROM_R(GDT_Int8, -128, GDT_UInt16, 0); /* clamp */
    FROM_R(GDT_Int8, -128, GDT_Int32, -128);
    FROM_R(GDT_Int8, -128, GDT_UInt32, 0); /* clamp */
    FROM_R(GDT_Int8, -128, GDT_Int64, -128);
    FROM_R(GDT_Int8, -128, GDT_UInt64, 0); /* clamp */
    FROM_R(GDT_Int8, -128, GDT_Float16, -128);
    FROM_R(GDT_Int8, -128, GDT_Float32, -128);
    FROM_R(GDT_Int8, -128, GDT_Float64, -128);
    FROM_R(GDT_Int8, -128, GDT_CInt16, -128);
    FROM_R(GDT_Int8, -128, GDT_CInt32, -128);
    FROM_R(GDT_Int8, -128, GDT_CFloat16, -128);
    FROM_R(GDT_Int8, -128, GDT_CFloat32, -128);
    FROM_R(GDT_Int8, -128, GDT_CFloat64, -128);
    for (GDALDataType outtype = GDT_Byte; outtype < GDT_TypeCount;
         outtype = (GDALDataType)(outtype + 1))
    {
        FROM_R(GDT_Int8, 127, outtype, 127);
    }

    FROM_R(GDT_Int8, 127, GDT_Byte, 127);
    FROM_R(GDT_Int8, 127, GDT_Int8, 127);
    FROM_R(GDT_Int8, 127, GDT_Int16, 127);
    FROM_R(GDT_Int8, 127, GDT_UInt16, 127);
    FROM_R(GDT_Int8, 127, GDT_Int32, 127);
    FROM_R(GDT_Int8, 127, GDT_UInt32, 127);
    FROM_R(GDT_Int8, 127, GDT_Int64, 127);
    FROM_R(GDT_Int8, 127, GDT_UInt64, 127);
    FROM_R(GDT_Int8, 127, GDT_Float32, 127);
    FROM_R(GDT_Int8, 127, GDT_Float64, 127);
    FROM_R(GDT_Int8, 127, GDT_CInt16, 127);
    FROM_R(GDT_Int8, 127, GDT_CInt32, 127);
    FROM_R(GDT_Int8, 127, GDT_CFloat32, 127);
    FROM_R(GDT_Int8, 127, GDT_CFloat64, 127);
}

TEST_F(TestCopyWords, GDT_Int16)
{
    /* GDT_Int16 */
    FROM_R(GDT_Int16, -32000, GDT_Byte, 0); /* clamp */
    FROM_R(GDT_Int16, -32000, GDT_Int16, -32000);
    FROM_R(GDT_Int16, -32000, GDT_UInt16, 0); /* clamp */
    FROM_R(GDT_Int16, -32000, GDT_Int32, -32000);
    FROM_R(GDT_Int16, -32000, GDT_UInt32, 0); /* clamp */
    FROM_R(GDT_Int16, -32000, GDT_Int64, -32000);
    FROM_R(GDT_Int16, -32000, GDT_UInt64, 0); /* clamp */
    FROM_R(GDT_Int16, -32000, GDT_Float32, -32000);
    FROM_R(GDT_Int16, -32000, GDT_Float64, -32000);
    FROM_R(GDT_Int16, -32000, GDT_CInt16, -32000);
    FROM_R(GDT_Int16, -32000, GDT_CInt32, -32000);
    FROM_R(GDT_Int16, -32000, GDT_CFloat32, -32000);
    FROM_R(GDT_Int16, -32000, GDT_CFloat64, -32000);
    for (GDALDataType outtype = GDT_Byte; outtype < GDT_TypeCount;
         outtype = (GDALDataType)(outtype + 1))
    {
        FROM_R(GDT_Int16, 127, outtype, 127);
    }

    FROM_R(GDT_Int16, 32000, GDT_Byte, 255); /* clamp */
    FROM_R(GDT_Int16, 32000, GDT_Int16, 32000);
    FROM_R(GDT_Int16, 32000, GDT_UInt16, 32000);
    FROM_R(GDT_Int16, 32000, GDT_Int32, 32000);
    FROM_R(GDT_Int16, 32000, GDT_UInt32, 32000);
    FROM_R(GDT_Int16, 32000, GDT_Int64, 32000);
    FROM_R(GDT_Int16, 32000, GDT_UInt64, 32000);
    FROM_R(GDT_Int16, 32000, GDT_Float32, 32000);
    FROM_R(GDT_Int16, 32000, GDT_Float64, 32000);
    FROM_R(GDT_Int16, 32000, GDT_CInt16, 32000);
    FROM_R(GDT_Int16, 32000, GDT_CInt32, 32000);
    FROM_R(GDT_Int16, 32000, GDT_CFloat32, 32000);
    FROM_R(GDT_Int16, 32000, GDT_CFloat64, 32000);
}

TEST_F(TestCopyWords, GDT_UInt16)
{
    /* GDT_UInt16 */
    for (GDALDataType outtype = GDT_Byte; outtype < GDT_TypeCount;
         outtype = (GDALDataType)(outtype + 1))
    {
        FROM_R(GDT_UInt16, 0, outtype, 0);
        FROM_R(GDT_UInt16, 127, outtype, 127);
    }

    FROM_R(GDT_UInt16, 65000, GDT_Byte, 255);    /* clamp */
    FROM_R(GDT_UInt16, 65000, GDT_Int16, 32767); /* clamp */
    FROM_R(GDT_UInt16, 65000, GDT_UInt16, 65000);
    FROM_R(GDT_UInt16, 65000, GDT_Int32, 65000);
    FROM_R(GDT_UInt16, 65000, GDT_UInt32, 65000);
    FROM_R(GDT_UInt16, 65000, GDT_Int64, 65000);
    FROM_R(GDT_UInt16, 65000, GDT_UInt64, 65000);
    FROM_R(GDT_UInt16, 65000, GDT_Float32, 65000);
    FROM_R(GDT_UInt16, 65000, GDT_Float64, 65000);
    FROM_R(GDT_UInt16, 65000, GDT_CInt16, 32767); /* clamp */
    FROM_R(GDT_UInt16, 65000, GDT_CInt32, 65000);
    FROM_R(GDT_UInt16, 65000, GDT_CFloat32, 65000);
    FROM_R(GDT_UInt16, 65000, GDT_CFloat64, 65000);
}

TEST_F(TestCopyWords, GDT_Int32)
{
    /* GDT_Int32 */
    FROM_R(GDT_Int32, -33000, GDT_Byte, 0);       /* clamp */
    FROM_R(GDT_Int32, -33000, GDT_Int16, -32768); /* clamp */
    FROM_R(GDT_Int32, -33000, GDT_UInt16, 0);     /* clamp */
    FROM_R(GDT_Int32, -33000, GDT_Int32, -33000);
    FROM_R(GDT_Int32, -33000, GDT_UInt32, 0); /* clamp */
    FROM_R(GDT_Int32, -33000, GDT_Int64, -33000);
    FROM_R(GDT_Int32, -33000, GDT_UInt64, 0); /* clamp */
    FROM_R(GDT_Int32, -33000, GDT_Float32, -33000);
    FROM_R(GDT_Int32, -33000, GDT_Float64, -33000);
    FROM_R(GDT_Int32, -33000, GDT_CInt16, -32768); /* clamp */
    FROM_R(GDT_Int32, -33000, GDT_CInt32, -33000);
    FROM_R(GDT_Int32, -33000, GDT_CFloat32, -33000);
    FROM_R(GDT_Int32, -33000, GDT_CFloat64, -33000);
    for (GDALDataType outtype = GDT_Byte; outtype < GDT_TypeCount;
         outtype = (GDALDataType)(outtype + 1))
    {
        FROM_R(GDT_Int32, 127, outtype, 127);
    }

    FROM_R(GDT_Int32, 67000, GDT_Byte, 255);     /* clamp */
    FROM_R(GDT_Int32, 67000, GDT_Int16, 32767);  /* clamp */
    FROM_R(GDT_Int32, 67000, GDT_UInt16, 65535); /* clamp */
    FROM_R(GDT_Int32, 67000, GDT_Int32, 67000);
    FROM_R(GDT_Int32, 67000, GDT_UInt32, 67000);
    FROM_R(GDT_Int32, 67000, GDT_Int64, 67000);
    FROM_R(GDT_Int32, 67000, GDT_UInt64, 67000);
    FROM_R(GDT_Int32, 67000, GDT_Float32, 67000);
    FROM_R(GDT_Int32, 67000, GDT_Float64, 67000);
    FROM_R(GDT_Int32, 67000, GDT_CInt16, 32767); /* clamp */
    FROM_R(GDT_Int32, 67000, GDT_CInt32, 67000);
    FROM_R(GDT_Int32, 67000, GDT_CFloat32, 67000);
    FROM_R(GDT_Int32, 67000, GDT_CFloat64, 67000);
}

TEST_F(TestCopyWords, GDT_UInt32)
{
    /* GDT_UInt32 */
    for (GDALDataType outtype = GDT_Byte; outtype < GDT_TypeCount;
         outtype = (GDALDataType)(outtype + 1))
    {
        FROM_R(GDT_UInt32, 0, outtype, 0);
        FROM_R(GDT_UInt32, 127, outtype, 127);
    }

    FROM_R(GDT_UInt32, 3000000000U, GDT_Byte, 255);         /* clamp */
    FROM_R(GDT_UInt32, 3000000000U, GDT_Int16, 32767);      /* clamp */
    FROM_R(GDT_UInt32, 3000000000U, GDT_UInt16, 65535);     /* clamp */
    FROM_R(GDT_UInt32, 3000000000U, GDT_Int32, 2147483647); /* clamp */
    FROM_R(GDT_UInt32, 3000000000U, GDT_UInt32, 3000000000U);
    FROM_R(GDT_UInt32, 3000000000U, GDT_Int64, 3000000000U);
    FROM_R(GDT_UInt32, 3000000000U, GDT_UInt64, 3000000000U);
    FROM_R(GDT_UInt32, 3000000000U, GDT_Float32, 3000000000U);
    FROM_R(GDT_UInt32, 3000000000U, GDT_Float64, 3000000000U);
    FROM_R(GDT_UInt32, 3000000000U, GDT_CInt16, 32767);      /* clamp */
    FROM_R(GDT_UInt32, 3000000000U, GDT_CInt32, 2147483647); /* clamp */
    FROM_R(GDT_UInt32, 3000000000U, GDT_CFloat32, 3000000000U);
    FROM_R(GDT_UInt32, 3000000000U, GDT_CFloat64, 3000000000U);
}

TEST_F(TestCopyWords, check_GDT_Int64)
{
    /* GDT_Int64 */
    FROM_R(GDT_Int64, -33000, GDT_Byte, 0);       /* clamp */
    FROM_R(GDT_Int64, -33000, GDT_Int16, -32768); /* clamp */
    FROM_R(GDT_Int64, -33000, GDT_UInt16, 0);     /* clamp */
    FROM_R(GDT_Int64, -33000, GDT_Int32, -33000);
    FROM_R(GDT_Int64, -33000, GDT_UInt32, 0); /* clamp */
    FROM_R(GDT_Int64, -33000, GDT_Int64, -33000);
    FROM_R(GDT_Int64, -33000, GDT_UInt64, 0); /* clamp */
    FROM_R(GDT_Int64, -33000, GDT_Float32, -33000);
    FROM_R(GDT_Int64, -33000, GDT_Float64, -33000);
    FROM_R(GDT_Int64, -33000, GDT_CInt16, -32768); /* clamp */
    FROM_R(GDT_Int64, -33000, GDT_CInt32, -33000);
    FROM_R(GDT_Int64, -33000, GDT_CFloat32, -33000);
    FROM_R(GDT_Int64, -33000, GDT_CFloat64, -33000);
    for (GDALDataType outtype = GDT_Byte; outtype < GDT_TypeCount;
         outtype = (GDALDataType)(outtype + 1))
    {
        FROM_R(GDT_Int64, 127, outtype, 127);
    }

    FROM_R(GDT_Int64, 67000, GDT_Byte, 255);     /* clamp */
    FROM_R(GDT_Int64, 67000, GDT_Int16, 32767);  /* clamp */
    FROM_R(GDT_Int32, 67000, GDT_UInt16, 65535); /* clamp */
    FROM_R(GDT_Int32, 67000, GDT_Int32, 67000);
    FROM_R(GDT_Int64, 67000, GDT_UInt32, 67000);
    FROM_R(GDT_Int32, 67000, GDT_Int64, 67000);
    FROM_R(GDT_Int64, 67000, GDT_UInt64, 67000);
    FROM_R(GDT_Int64, 67000, GDT_Float32, 67000);
    FROM_R(GDT_Int64, 67000, GDT_Float64, 67000);
    FROM_R(GDT_Int64, 67000, GDT_CInt16, 32767); /* clamp */
    FROM_R(GDT_Int64, 67000, GDT_CInt32, 67000);
    FROM_R(GDT_Int64, 67000, GDT_CFloat32, 67000);
    FROM_R(GDT_Int64, 67000, GDT_CFloat64, 67000);
}

TEST_F(TestCopyWords, GDT_UInt64)
{
    /* GDT_UInt64 */
    for (GDALDataType outtype = GDT_Byte; outtype < GDT_TypeCount;
         outtype = (GDALDataType)(outtype + 1))
    {
        FROM_R(GDT_UInt64, 0, outtype, 0);
        FROM_R(GDT_UInt64, 127, outtype, 127);
    }

    std::uint64_t nVal = static_cast<std::uint64_t>(3000000000) * 1000;
    FROM_R(GDT_UInt64, nVal, GDT_Byte, 255);           /* clamp */
    FROM_R(GDT_UInt64, nVal, GDT_Int16, 32767);        /* clamp */
    FROM_R(GDT_UInt64, nVal, GDT_UInt16, 65535);       /* clamp */
    FROM_R(GDT_UInt64, nVal, GDT_Int32, 2147483647);   /* clamp */
    FROM_R(GDT_UInt64, nVal, GDT_UInt32, 4294967295U); /* clamp */
    FROM_R(GDT_UInt64, nVal, GDT_Int64, nVal);
    FROM_R(GDT_UInt64, nVal, GDT_UInt64, nVal);
    FROM_R(GDT_UInt64, nVal, GDT_Float32,
           static_cast<uint64_t>(static_cast<float>(nVal)));
    FROM_R(GDT_UInt64, nVal, GDT_Float64, nVal);
    FROM_R(GDT_UInt64, nVal, GDT_CInt16, 32767);      /* clamp */
    FROM_R(GDT_UInt64, nVal, GDT_CInt32, 2147483647); /* clamp */
    FROM_R(GDT_UInt64, nVal, GDT_CFloat32,
           static_cast<uint64_t>(static_cast<float>(nVal)));
    FROM_R(GDT_UInt64, nVal, GDT_CFloat64, nVal);
}

TEST_F(TestCopyWords, GDT_Float64)
{
    FROM_R_F(GDT_Float64, std::numeric_limits<double>::max(), GDT_Float32,
             std::numeric_limits<double>::infinity());
    FROM_R_F(GDT_Float64, -std::numeric_limits<double>::max(), GDT_Float32,
             -std::numeric_limits<double>::infinity());
    FROM_R_F(GDT_Float64, std::numeric_limits<double>::quiet_NaN(), GDT_Float32,
             std::numeric_limits<double>::quiet_NaN());
}

TEST_F(TestCopyWords, GDT_Float16only)
{
    GDALDataType intype = GDT_Float16;
    for (GDALDataType outtype = GDT_Byte; outtype < GDT_TypeCount;
         outtype = (GDALDataType)(outtype + 1))
    {
        if (IS_FLOAT(outtype))
        {
            FROM_R_F(intype, 127.1, outtype, 127.1);
            FROM_R_F(intype, -127.1, outtype, -127.1);
        }
        else
        {
            FROM_R_F(intype, 125.1, outtype, 125);
            FROM_R_F(intype, 125.9, outtype, 126);

            FROM_R_F(intype, 0.4, outtype, 0);
            FROM_R_F(intype, 0.5, outtype,
                     1); /* We could argue how to do this rounding */
            FROM_R_F(intype, 0.6, outtype, 1);
            FROM_R_F(intype, 126.5, outtype,
                     127); /* We could argue how to do this rounding */

            if (!IS_UNSIGNED(outtype))
            {
                FROM_R_F(intype, -125.9, outtype, -126);
                FROM_R_F(intype, -127.1, outtype, -127);

                FROM_R_F(intype, -0.4, outtype, 0);
                FROM_R_F(intype, -0.5, outtype,
                         -1); /* We could argue how to do this rounding */
                FROM_R_F(intype, -0.6, outtype, -1);
                FROM_R_F(intype, -127.5, outtype,
                         -128); /* We could argue how to do this rounding */
            }
        }
    }
    FROM_R(intype, -30000, GDT_Byte, 0);
    FROM_R(intype, -32768, GDT_Byte, 0);
    FROM_R(intype, -1, GDT_Byte, 0);
    FROM_R(intype, 256, GDT_Byte, 255);
    FROM_R(intype, 30000, GDT_Byte, 255);
    FROM_R(intype, -330000, GDT_Int16, -32768);
    FROM_R(intype, -33000, GDT_Int16, -32768);
    FROM_R(intype, 33000, GDT_Int16, 32767);
    FROM_R(intype, -33000, GDT_UInt16, 0);
    FROM_R(intype, -1, GDT_UInt16, 0);
    FROM_R(intype, 60000, GDT_UInt16, 60000);
    FROM_R(intype, -33000, GDT_Int32, -32992);
    FROM_R(intype, 33000, GDT_Int32, 32992);
    FROM_R(intype, -1, GDT_UInt32, 0);
    FROM_R(intype, 60000, GDT_UInt32, 60000U);
    FROM_R(intype, 33000, GDT_Float32, 32992);
    FROM_R(intype, -33000, GDT_Float32, -32992);
    FROM_R(intype, 33000, GDT_Float64, 32992);
    FROM_R(intype, -33000, GDT_Float64, -32992);
    FROM_R(intype, -33000, GDT_CInt16, -32768);
    FROM_R(intype, 33000, GDT_CInt16, 32767);
    FROM_R(intype, -33000, GDT_CInt32, -32992);
    FROM_R(intype, 33000, GDT_CInt32, 32992);
    FROM_R(intype, 33000, GDT_CFloat32, 32992);
    FROM_R(intype, -33000, GDT_CFloat32, -32992);
    FROM_R(intype, 33000, GDT_CFloat64, 32992);
    FROM_R(intype, -33000, GDT_CFloat64, -32992);

    FROM_R_F(GDT_Float32, std::numeric_limits<float>::min(), GDT_Float16, 0);
    FROM_R_F(GDT_Float32, -std::numeric_limits<float>::min(), GDT_Float16, 0);
    // smallest positive subnormal float16 number
    FROM_R_F(GDT_Float32, 0.000000059604645f, GDT_Float16, 0.000000059604645f);
    FROM_R_F(GDT_Float32, 65504.0f, GDT_Float16, 65504.0f);
    FROM_R_F(GDT_Float32, 65535.0f, GDT_Float16,
             std::numeric_limits<double>::infinity());
    FROM_R_F(GDT_Float32, std::numeric_limits<float>::max(), GDT_Float16,
             std::numeric_limits<double>::infinity());
    FROM_R_F(GDT_Float32, -std::numeric_limits<float>::max(), GDT_Float16,
             -std::numeric_limits<double>::infinity());
    FROM_R_F(GDT_Float32, std::numeric_limits<float>::quiet_NaN(), GDT_Float16,
             std::numeric_limits<double>::quiet_NaN());

    FROM_R_F(GDT_Float64, std::numeric_limits<double>::max(), GDT_Float16,
             std::numeric_limits<double>::infinity());
    FROM_R_F(GDT_Float64, -std::numeric_limits<double>::max(), GDT_Float16,
             -std::numeric_limits<double>::infinity());
    FROM_R_F(GDT_Float64, std::numeric_limits<double>::quiet_NaN(), GDT_Float16,
             std::numeric_limits<double>::quiet_NaN());

    // Float16 to Int64
    {
        GFloat16 in_value = cpl::NumericLimits<GFloat16>::quiet_NaN();
        int64_t out_value = 0;
        GDALCopyWords(&in_value, GDT_Float16, 0, &out_value, GDT_Int64, 0, 1);
        EXPECT_EQ(out_value, 0);
    }

    {
        GFloat16 in_value = -cpl::NumericLimits<GFloat16>::infinity();
        int64_t out_value = 0;
        GDALCopyWords(&in_value, GDT_Float16, 0, &out_value, GDT_Int64, 0, 1);
        EXPECT_EQ(out_value, INT64_MIN);
    }

    {
        GFloat16 in_value = -cpl::NumericLimits<GFloat16>::max();
        int64_t out_value = 0;
        GDALCopyWords(&in_value, GDT_Float16, 0, &out_value, GDT_Int64, 0, 1);
        EXPECT_EQ(out_value, -65504);
    }

    {
        GFloat16 in_value = cpl::NumericLimits<GFloat16>::max();
        int64_t out_value = 0;
        GDALCopyWords(&in_value, GDT_Float16, 0, &out_value, GDT_Int64, 0, 1);
        EXPECT_EQ(out_value, 65504);
    }

    {
        GFloat16 in_value = cpl::NumericLimits<GFloat16>::infinity();
        int64_t out_value = 0;
        GDALCopyWords(&in_value, GDT_Float16, 0, &out_value, GDT_Int64, 0, 1);
        EXPECT_EQ(out_value, INT64_MAX);
    }

    // Float16 to UInt64
    {
        GFloat16 in_value = cpl::NumericLimits<GFloat16>::quiet_NaN();
        uint64_t out_value = 0;
        GDALCopyWords(&in_value, GDT_Float16, 0, &out_value, GDT_UInt64, 0, 1);
        EXPECT_EQ(out_value, 0);
    }

    {
        GFloat16 in_value = -cpl::NumericLimits<GFloat16>::infinity();
        uint64_t out_value = 0;
        GDALCopyWords(&in_value, GDT_Float16, 0, &out_value, GDT_UInt64, 0, 1);
        EXPECT_EQ(out_value, 0);
    }

    {
        GFloat16 in_value = -cpl::NumericLimits<GFloat16>::max();
        uint64_t out_value = 0;
        GDALCopyWords(&in_value, GDT_Float16, 0, &out_value, GDT_UInt64, 0, 1);
        EXPECT_EQ(out_value, 0);
    }

    {
        GFloat16 in_value = cpl::NumericLimits<GFloat16>::max();
        uint64_t out_value = 0;
        GDALCopyWords(&in_value, GDT_Float16, 0, &out_value, GDT_UInt64, 0, 1);
        EXPECT_EQ(out_value, 65504);
    }

    {
        GFloat16 in_value = cpl::NumericLimits<GFloat16>::infinity();
        uint64_t out_value = 0;
        GDALCopyWords(&in_value, GDT_Float16, 0, &out_value, GDT_UInt64, 0, 1);
        EXPECT_EQ(out_value, UINT64_MAX);
    }
}

TEST_F(TestCopyWords, GDT_Float32and64)
{
    /* GDT_Float32 and GDT_Float64 */
    for (int i = 0; i < 2; i++)
    {
        GDALDataType intype = (i == 0) ? GDT_Float32 : GDT_Float64;
        for (GDALDataType outtype = GDT_Byte; outtype < GDT_TypeCount;
             outtype = (GDALDataType)(outtype + 1))
        {
            if (IS_FLOAT(outtype))
            {
                FROM_R_F(intype, 127.1, outtype, 127.1);
                FROM_R_F(intype, -127.1, outtype, -127.1);
            }
            else
            {
                FROM_R_F(intype, 125.1, outtype, 125);
                FROM_R_F(intype, 125.9, outtype, 126);

                FROM_R_F(intype, 0.4, outtype, 0);
                FROM_R_F(intype, 0.5, outtype,
                         1); /* We could argue how to do this rounding */
                FROM_R_F(intype, 0.6, outtype, 1);
                FROM_R_F(intype, 126.5, outtype,
                         127); /* We could argue how to do this rounding */

                if (!IS_UNSIGNED(outtype))
                {
                    FROM_R_F(intype, -125.9, outtype, -126);
                    FROM_R_F(intype, -127.1, outtype, -127);

                    FROM_R_F(intype, -0.4, outtype, 0);
                    FROM_R_F(intype, -0.5, outtype,
                             -1); /* We could argue how to do this rounding */
                    FROM_R_F(intype, -0.6, outtype, -1);
                    FROM_R_F(intype, -127.5, outtype,
                             -128); /* We could argue how to do this rounding */
                }
            }
        }
        FROM_R(intype, -CST_3000000000, GDT_Byte, 0);
        FROM_R(intype, -32768, GDT_Byte, 0);
        FROM_R(intype, -1, GDT_Byte, 0);
        FROM_R(intype, 256, GDT_Byte, 255);
        FROM_R(intype, 65536, GDT_Byte, 255);
        FROM_R(intype, CST_3000000000, GDT_Byte, 255);
        FROM_R(intype, -CST_3000000000, GDT_Int16, -32768);
        FROM_R(intype, -33000, GDT_Int16, -32768);
        FROM_R(intype, 33000, GDT_Int16, 32767);
        FROM_R(intype, CST_3000000000, GDT_Int16, 32767);
        FROM_R(intype, -CST_3000000000, GDT_UInt16, 0);
        FROM_R(intype, -1, GDT_UInt16, 0);
        FROM_R(intype, 66000, GDT_UInt16, 65535);
        FROM_R(intype, CST_3000000000, GDT_UInt16, 65535);
        FROM_R(intype, -CST_3000000000, GDT_Int32, INT_MIN);
        FROM_R(intype, CST_3000000000, GDT_Int32, 2147483647);
        FROM_R(intype, -1, GDT_UInt32, 0);
        FROM_R(intype, CST_5000000000, GDT_UInt32, 4294967295UL);
        FROM_R(intype, CST_5000000000, GDT_Float32, CST_5000000000);
        FROM_R(intype, -CST_5000000000, GDT_Float32, -CST_5000000000);
        FROM_R(intype, CST_5000000000, GDT_Float64, CST_5000000000);
        FROM_R(intype, -CST_5000000000, GDT_Float64, -CST_5000000000);
        FROM_R(intype, -33000, GDT_CInt16, -32768);
        FROM_R(intype, 33000, GDT_CInt16, 32767);
        FROM_R(intype, -CST_3000000000, GDT_CInt32, INT_MIN);
        FROM_R(intype, CST_3000000000, GDT_CInt32, 2147483647);
        FROM_R(intype, CST_5000000000, GDT_CFloat32, CST_5000000000);
        FROM_R(intype, -CST_5000000000, GDT_CFloat32, -CST_5000000000);
        FROM_R(intype, CST_5000000000, GDT_CFloat64, CST_5000000000);
        FROM_R(intype, -CST_5000000000, GDT_CFloat64, -CST_5000000000);
    }

    // Float32 to Int64
    {
        float in_value = cpl::NumericLimits<float>::quiet_NaN();
        int64_t out_value = 0;
        GDALCopyWords(&in_value, GDT_Float32, 0, &out_value, GDT_Int64, 0, 1);
        EXPECT_EQ(out_value, 0);
    }

    {
        float in_value = -cpl::NumericLimits<float>::infinity();
        int64_t out_value = 0;
        GDALCopyWords(&in_value, GDT_Float32, 0, &out_value, GDT_Int64, 0, 1);
        EXPECT_EQ(out_value, INT64_MIN);
    }

    {
        float in_value = -cpl::NumericLimits<float>::max();
        int64_t out_value = 0;
        GDALCopyWords(&in_value, GDT_Float32, 0, &out_value, GDT_Int64, 0, 1);
        EXPECT_EQ(out_value, INT64_MIN);
    }

    {
        float in_value = cpl::NumericLimits<float>::max();
        int64_t out_value = 0;
        GDALCopyWords(&in_value, GDT_Float32, 0, &out_value, GDT_Int64, 0, 1);
        EXPECT_EQ(out_value, INT64_MAX);
    }

    {
        float in_value = cpl::NumericLimits<float>::infinity();
        int64_t out_value = 0;
        GDALCopyWords(&in_value, GDT_Float32, 0, &out_value, GDT_Int64, 0, 1);
        EXPECT_EQ(out_value, INT64_MAX);
    }

    // Float64 to Int64
    {
        double in_value = cpl::NumericLimits<double>::quiet_NaN();
        int64_t out_value = 0;
        GDALCopyWords(&in_value, GDT_Float64, 0, &out_value, GDT_Int64, 0, 1);
        EXPECT_EQ(out_value, 0);
    }

    {
        double in_value = -cpl::NumericLimits<double>::infinity();
        int64_t out_value = 0;
        GDALCopyWords(&in_value, GDT_Float64, 0, &out_value, GDT_Int64, 0, 1);
        EXPECT_EQ(out_value, INT64_MIN);
    }

    {
        double in_value = -cpl::NumericLimits<double>::max();
        int64_t out_value = 0;
        GDALCopyWords(&in_value, GDT_Float64, 0, &out_value, GDT_Int64, 0, 1);
        EXPECT_EQ(out_value, INT64_MIN);
    }

    {
        double in_value = cpl::NumericLimits<double>::max();
        int64_t out_value = 0;
        GDALCopyWords(&in_value, GDT_Float64, 0, &out_value, GDT_Int64, 0, 1);
        EXPECT_EQ(out_value, INT64_MAX);
    }

    {
        double in_value = cpl::NumericLimits<double>::infinity();
        int64_t out_value = 0;
        GDALCopyWords(&in_value, GDT_Float64, 0, &out_value, GDT_Int64, 0, 1);
        EXPECT_EQ(out_value, INT64_MAX);
    }

    // Float32 to UInt64
    {
        float in_value = cpl::NumericLimits<float>::quiet_NaN();
        uint64_t out_value = 0;
        GDALCopyWords(&in_value, GDT_Float32, 0, &out_value, GDT_UInt64, 0, 1);
        EXPECT_EQ(out_value, 0);
    }

    {
        float in_value = -cpl::NumericLimits<float>::infinity();
        uint64_t out_value = 0;
        GDALCopyWords(&in_value, GDT_Float32, 0, &out_value, GDT_UInt64, 0, 1);
        EXPECT_EQ(out_value, 0);
    }

    {
        float in_value = -cpl::NumericLimits<float>::max();
        uint64_t out_value = 0;
        GDALCopyWords(&in_value, GDT_Float32, 0, &out_value, GDT_UInt64, 0, 1);
        EXPECT_EQ(out_value, 0);
    }

    {
        float in_value = cpl::NumericLimits<float>::max();
        uint64_t out_value = 0;
        GDALCopyWords(&in_value, GDT_Float32, 0, &out_value, GDT_UInt64, 0, 1);
        EXPECT_EQ(out_value, UINT64_MAX);
    }

    {
        float in_value = cpl::NumericLimits<float>::infinity();
        uint64_t out_value = 0;
        GDALCopyWords(&in_value, GDT_Float32, 0, &out_value, GDT_UInt64, 0, 1);
        EXPECT_EQ(out_value, UINT64_MAX);
    }

    // Float64 to UInt64
    {
        double in_value = -cpl::NumericLimits<double>::quiet_NaN();
        uint64_t out_value = 0;
        GDALCopyWords(&in_value, GDT_Float64, 0, &out_value, GDT_UInt64, 0, 1);
        EXPECT_EQ(out_value, 0);
    }

    {
        double in_value = -cpl::NumericLimits<double>::infinity();
        uint64_t out_value = 0;
        GDALCopyWords(&in_value, GDT_Float64, 0, &out_value, GDT_UInt64, 0, 1);
        EXPECT_EQ(out_value, 0);
    }

    {
        double in_value = -cpl::NumericLimits<double>::max();
        uint64_t out_value = 0;
        GDALCopyWords(&in_value, GDT_Float64, 0, &out_value, GDT_UInt64, 0, 1);
        EXPECT_EQ(out_value, 0);
    }

    {
        double in_value = cpl::NumericLimits<double>::max();
        uint64_t out_value = 0;
        GDALCopyWords(&in_value, GDT_Float64, 0, &out_value, GDT_UInt64, 0, 1);
        EXPECT_EQ(out_value, UINT64_MAX);
    }

    {
        double in_value = cpl::NumericLimits<double>::infinity();
        uint64_t out_value = 0;
        GDALCopyWords(&in_value, GDT_Float64, 0, &out_value, GDT_UInt64, 0, 1);
        EXPECT_EQ(out_value, UINT64_MAX);
    }
}

TEST_F(TestCopyWords, GDT_CInt16)
{
    /* GDT_CInt16 */
    FROM_C(GDT_CInt16, -32000, -32500, GDT_Byte, 0, 0); /* clamp */
    FROM_C(GDT_CInt16, -32000, -32500, GDT_Int16, -32000, 0);
    FROM_C(GDT_CInt16, -32000, -32500, GDT_UInt16, 0, 0); /* clamp */
    FROM_C(GDT_CInt16, -32000, -32500, GDT_Int32, -32000, 0);
    FROM_C(GDT_CInt16, -32000, -32500, GDT_UInt32, 0, 0); /* clamp */
    FROM_C(GDT_CInt16, -32000, -32500, GDT_Float32, -32000, 0);
    FROM_C(GDT_CInt16, -32000, -32500, GDT_Float64, -32000, 0);
    FROM_C(GDT_CInt16, -32000, -32500, GDT_CInt16, -32000, -32500);
    FROM_C(GDT_CInt16, -32000, -32500, GDT_CInt32, -32000, -32500);
    FROM_C(GDT_CInt16, -32000, -32500, GDT_CFloat32, -32000, -32500);
    FROM_C(GDT_CInt16, -32000, -32500, GDT_CFloat64, -32000, -32500);
    for (GDALDataType outtype = GDT_Byte; outtype < GDT_TypeCount;
         outtype = (GDALDataType)(outtype + 1))
    {
        FROM_C(GDT_CInt16, 127, 128, outtype, 127, 128);
    }

    FROM_C(GDT_CInt16, 32000, 32500, GDT_Byte, 255, 0); /* clamp */
    FROM_C(GDT_CInt16, 32000, 32500, GDT_Int16, 32000, 0);
    FROM_C(GDT_CInt16, 32000, 32500, GDT_UInt16, 32000, 0);
    FROM_C(GDT_CInt16, 32000, 32500, GDT_Int32, 32000, 0);
    FROM_C(GDT_CInt16, 32000, 32500, GDT_UInt32, 32000, 0);
    FROM_C(GDT_CInt16, 32000, 32500, GDT_Float32, 32000, 0);
    FROM_C(GDT_CInt16, 32000, 32500, GDT_Float64, 32000, 0);
    FROM_C(GDT_CInt16, 32000, 32500, GDT_CInt16, 32000, 32500);
    FROM_C(GDT_CInt16, 32000, 32500, GDT_CInt32, 32000, 32500);
    FROM_C(GDT_CInt16, 32000, 32500, GDT_CFloat32, 32000, 32500);
    FROM_C(GDT_CInt16, 32000, 32500, GDT_CFloat64, 32000, 32500);
}

TEST_F(TestCopyWords, GDT_CInt32)
{
    /* GDT_CInt32 */
    FROM_C(GDT_CInt32, -33000, -33500, GDT_Byte, 0, 0);       /* clamp */
    FROM_C(GDT_CInt32, -33000, -33500, GDT_Int16, -32768, 0); /* clamp */
    FROM_C(GDT_CInt32, -33000, -33500, GDT_UInt16, 0, 0);     /* clamp */
    FROM_C(GDT_CInt32, -33000, -33500, GDT_Int32, -33000, 0);
    FROM_C(GDT_CInt32, -33000, -33500, GDT_UInt32, 0, 0); /* clamp */
    FROM_C(GDT_CInt32, -33000, -33500, GDT_Float32, -33000, 0);
    FROM_C(GDT_CInt32, -33000, -33500, GDT_Float64, -33000, 0);
    FROM_C(GDT_CInt32, -33000, -33500, GDT_CInt16, -32768, -32768); /* clamp */
    FROM_C(GDT_CInt32, -33000, -33500, GDT_CInt32, -33000, -33500);
    FROM_C(GDT_CInt32, -33000, -33500, GDT_CFloat32, -33000, -33500);
    FROM_C(GDT_CInt32, -33000, -33500, GDT_CFloat64, -33000, -33500);
    for (GDALDataType outtype = GDT_Byte; outtype < GDT_TypeCount;
         outtype = (GDALDataType)(outtype + 1))
    {
        FROM_C(GDT_CInt32, 127, 128, outtype, 127, 128);
    }

    FROM_C(GDT_CInt32, 67000, 67500, GDT_Byte, 255, 0);     /* clamp */
    FROM_C(GDT_CInt32, 67000, 67500, GDT_Int16, 32767, 0);  /* clamp */
    FROM_C(GDT_CInt32, 67000, 67500, GDT_UInt16, 65535, 0); /* clamp */
    FROM_C(GDT_CInt32, 67000, 67500, GDT_Int32, 67000, 0);
    FROM_C(GDT_CInt32, 67000, 67500, GDT_UInt32, 67000, 0);
    FROM_C(GDT_CInt32, 67000, 67500, GDT_Float32, 67000, 0);
    FROM_C(GDT_CInt32, 67000, 67500, GDT_Float64, 67000, 0);
    FROM_C(GDT_CInt32, 67000, 67500, GDT_CInt16, 32767, 32767); /* clamp */
    FROM_C(GDT_CInt32, 67000, 67500, GDT_CInt32, 67000, 67500);
    FROM_C(GDT_CInt32, 67000, 67500, GDT_CFloat32, 67000, 67500);
    FROM_C(GDT_CInt32, 67000, 67500, GDT_CFloat64, 67000, 67500);
}

TEST_F(TestCopyWords, GDT_CFloat32and64)
{
    /* GDT_CFloat32 and GDT_CFloat64 */
    for (int i = 0; i < 2; i++)
    {
        GDALDataType intype = (i == 0) ? GDT_CFloat32 : GDT_CFloat64;
        for (GDALDataType outtype = GDT_Byte; outtype < GDT_TypeCount;
             outtype = (GDALDataType)(outtype + 1))
        {
            if (IS_FLOAT(outtype))
            {
                FROM_C_F(intype, 127.1, 127.9, outtype, 127.1, 127.9);
                FROM_C_F(intype, -127.1, -127.9, outtype, -127.1, -127.9);
            }
            else
            {
                FROM_C_F(intype, 126.1, 150.9, outtype, 126, 151);
                FROM_C_F(intype, 126.9, 150.1, outtype, 127, 150);
                if (!IS_UNSIGNED(outtype))
                {
                    FROM_C_F(intype, -125.9, -127.1, outtype, -126, -127);
                }
            }
        }
        FROM_C(intype, -1, 256, GDT_Byte, 0, 0);
        FROM_C(intype, 256, -1, GDT_Byte, 255, 0);
        FROM_C(intype, -33000, 33000, GDT_Int16, -32768, 0);
        FROM_C(intype, 33000, -33000, GDT_Int16, 32767, 0);
        FROM_C(intype, -1, 66000, GDT_UInt16, 0, 0);
        FROM_C(intype, 66000, -1, GDT_UInt16, 65535, 0);
        FROM_C(intype, -CST_3000000000, -CST_3000000000, GDT_Int32, INT_MIN, 0);
        FROM_C(intype, CST_3000000000, CST_3000000000, GDT_Int32, 2147483647,
               0);
        FROM_C(intype, -1, CST_5000000000, GDT_UInt32, 0, 0);
        FROM_C(intype, CST_5000000000, -1, GDT_UInt32, 4294967295UL, 0);
        FROM_C(intype, CST_5000000000, -1, GDT_Float32, CST_5000000000, 0);
        FROM_C(intype, CST_5000000000, -1, GDT_Float64, CST_5000000000, 0);
        FROM_C(intype, -CST_5000000000, -1, GDT_Float32, -CST_5000000000, 0);
        FROM_C(intype, -CST_5000000000, -1, GDT_Float64, -CST_5000000000, 0);
        FROM_C(intype, -33000, 33000, GDT_CInt16, -32768, 32767);
        FROM_C(intype, 33000, -33000, GDT_CInt16, 32767, -32768);
        FROM_C(intype, -CST_3000000000, -CST_3000000000, GDT_CInt32, INT_MIN,
               INT_MIN);
        FROM_C(intype, CST_3000000000, CST_3000000000, GDT_CInt32, 2147483647,
               2147483647);
        FROM_C(intype, CST_5000000000, -CST_5000000000, GDT_CFloat32,
               CST_5000000000, -CST_5000000000);
        FROM_C(intype, CST_5000000000, -CST_5000000000, GDT_CFloat64,
               CST_5000000000, -CST_5000000000);
    }
}

TEST_F(TestCopyWords, GDT_CFloat16only)
{
    GDALDataType intype = GDT_CFloat16;
    for (GDALDataType outtype = GDT_Byte; outtype < GDT_TypeCount;
         outtype = (GDALDataType)(outtype + 1))
    {
        if (IS_FLOAT(outtype))
        {
            FROM_C_F(intype, 127.1, 127.9, outtype, 127.1, 127.9);
            FROM_C_F(intype, -127.1, -127.9, outtype, -127.1, -127.9);
        }
        else
        {
            FROM_C_F(intype, 126.1, 150.9, outtype, 126, 151);
            FROM_C_F(intype, 126.9, 150.1, outtype, 127, 150);
            if (!IS_UNSIGNED(outtype))
            {
                FROM_C_F(intype, -125.9, -127.1, outtype, -126, -127);
            }
        }
    }
    FROM_C(intype, -1, 256, GDT_Byte, 0, 0);
    FROM_C(intype, 256, -1, GDT_Byte, 255, 0);
    FROM_C(intype, -33000, 33000, GDT_Int16, -32768, 0);
    FROM_C(intype, 33000, -33000, GDT_Int16, 32767, 0);
    FROM_C(intype, -1, 66000, GDT_UInt16, 0, 0);
    FROM_C(intype, 66000, -1, GDT_UInt16, 65535, 0);
    FROM_C(intype, -33000, -33000, GDT_Int32, -32992, 0);
    FROM_C(intype, 33000, 33000, GDT_Int32, 32992, 0);
    FROM_C(intype, -1, 33000, GDT_UInt32, 0, 0);
    FROM_C(intype, 33000, -1, GDT_UInt32, 32992, 0);
    FROM_C(intype, 33000, -1, GDT_Float32, 32992, 0);
    FROM_C(intype, 33000, -1, GDT_Float64, 32992, 0);
    FROM_C(intype, -33000, -1, GDT_Float32, -32992, 0);
    FROM_C(intype, -33000, -1, GDT_Float64, -32992, 0);
    FROM_C(intype, -33000, 33000, GDT_CInt16, -32768, 32767);
    FROM_C(intype, 33000, -33000, GDT_CInt16, 32767, -32768);
    FROM_C(intype, -33000, -33000, GDT_CInt32, -32992, -32992);
    FROM_C(intype, 33000, 33000, GDT_CInt32, 32992, 32992);
    FROM_C(intype, 33000, -33000, GDT_CFloat32, 32992, -32992);
    FROM_C(intype, 33000, -33000, GDT_CFloat64, 32992, -32992);
}

template <class Tin, class Tout>
void CheckPackedGeneric(GDALDataType eIn, GDALDataType eOut)
{
    const int N = 64 + 7;
    Tin arrayIn[N];
    Tout arrayOut[N];
    for (int i = 0; i < N; i++)
    {
        if constexpr (!std::is_integral_v<Tin> && std::is_integral_v<Tout>)
        {
            // Test correct rounding
            if (i == 0 && std::is_unsigned_v<Tout>)
                arrayIn[i] = cpl::NumericLimits<Tin>::quiet_NaN();
            else if ((i % 2) != 0)
                arrayIn[i] = static_cast<Tin>(i + 0.4);
            else
                arrayIn[i] = static_cast<Tin>(i + 0.6);
        }
        else
        {
            arrayIn[i] = static_cast<Tin>(i + 1);
        }
        arrayOut[i] = 0;
    }
    GDALCopyWords(arrayIn, eIn, GDALGetDataTypeSizeBytes(eIn), arrayOut, eOut,
                  GDALGetDataTypeSizeBytes(eOut), N);
    int numLine = 0;
    for (int i = 0; i < N; i++)
    {
        if constexpr (!std::is_integral_v<Tin> && std::is_integral_v<Tout>)
        {
            if (i == 0 && std::is_unsigned_v<Tout>)
            {
                MY_EXPECT(eIn, cpl::NumericLimits<Tin>::quiet_NaN(), eOut, 0,
                          arrayOut[i]);
            }
            else if ((i % 2) != 0)
            {
                MY_EXPECT(eIn, i + 0.4, eOut, i, arrayOut[i]);
            }
            else
            {
                MY_EXPECT(eIn, i + 0.6, eOut, i + 1, arrayOut[i]);
            }
        }
        else
        {
            MY_EXPECT(eIn, i + 1, eOut, i + 1, arrayOut[i]);
        }
    }
}

template <class Tin, class Tout>
void CheckPacked(GDALDataType eIn, GDALDataType eOut)
{
    CheckPackedGeneric<Tin, Tout>(eIn, eOut);
}

template <>
void CheckPacked<GUInt16, GByte>(GDALDataType eIn, GDALDataType eOut)
{
    CheckPackedGeneric<GUInt16, GByte>(eIn, eOut);

    const int N = 64 + 7;
    GUInt16 arrayIn[N] = {0};
    GByte arrayOut[N] = {0};
    for (int i = 0; i < N; i++)
    {
        arrayIn[i] = (i % 6) == 0   ? 254
                     : (i % 6) == 1 ? 255
                     : (i % 4) == 2 ? 256
                     : (i % 6) == 3 ? 32767
                     : (i % 6) == 4 ? 32768
                                    : 65535;
    }
    GDALCopyWords(arrayIn, eIn, GDALGetDataTypeSizeBytes(eIn), arrayOut, eOut,
                  GDALGetDataTypeSizeBytes(eOut), N);
    int numLine = 0;
    for (int i = 0; i < N; i++)
    {
        MY_EXPECT(eIn, (int)arrayIn[i], eOut, (i % 6) == 0 ? 254 : 255,
                  arrayOut[i]);
    }
}

template <>
void CheckPacked<GUInt16, GInt16>(GDALDataType eIn, GDALDataType eOut)
{
    CheckPackedGeneric<GUInt16, GInt16>(eIn, eOut);

    const int N = 64 + 7;
    GUInt16 arrayIn[N] = {0};
    GInt16 arrayOut[N] = {0};
    for (int i = 0; i < N; i++)
    {
        arrayIn[i] = 32766 + (i % 4);
    }
    GDALCopyWords(arrayIn, eIn, GDALGetDataTypeSizeBytes(eIn), arrayOut, eOut,
                  GDALGetDataTypeSizeBytes(eOut), N);
    int numLine = 0;
    for (int i = 0; i < N; i++)
    {
        MY_EXPECT(eIn, (int)arrayIn[i], eOut, (i % 4) == 0 ? 32766 : 32767,
                  arrayOut[i]);
    }
}

template <class Tin> void CheckPacked(GDALDataType eIn, GDALDataType eOut)
{
    switch (eOut)
    {
        case GDT_Byte:
            CheckPacked<Tin, GByte>(eIn, eOut);
            break;
        case GDT_Int8:
            CheckPacked<Tin, GInt8>(eIn, eOut);
            break;
        case GDT_UInt16:
            CheckPacked<Tin, GUInt16>(eIn, eOut);
            break;
        case GDT_Int16:
            CheckPacked<Tin, GInt16>(eIn, eOut);
            break;
        case GDT_UInt32:
            CheckPacked<Tin, GUInt32>(eIn, eOut);
            break;
        case GDT_Int32:
            CheckPacked<Tin, GInt32>(eIn, eOut);
            break;
        case GDT_UInt64:
            CheckPacked<Tin, std::uint64_t>(eIn, eOut);
            break;
        case GDT_Int64:
            CheckPacked<Tin, std::int64_t>(eIn, eOut);
            break;
        case GDT_Float16:
            CheckPacked<Tin, GFloat16>(eIn, eOut);
            break;
        case GDT_Float32:
            CheckPacked<Tin, float>(eIn, eOut);
            break;
        case GDT_Float64:
            CheckPacked<Tin, double>(eIn, eOut);
            break;
        default:
            CPLAssert(false);
    }
}

static void CheckPacked(GDALDataType eIn, GDALDataType eOut)
{
    switch (eIn)
    {
        case GDT_Byte:
            CheckPacked<GByte>(eIn, eOut);
            break;
        case GDT_Int8:
            CheckPacked<GInt8>(eIn, eOut);
            break;
        case GDT_UInt16:
            CheckPacked<GUInt16>(eIn, eOut);
            break;
        case GDT_Int16:
            CheckPacked<GInt16>(eIn, eOut);
            break;
        case GDT_UInt32:
            CheckPacked<GUInt32>(eIn, eOut);
            break;
        case GDT_Int32:
            CheckPacked<GInt32>(eIn, eOut);
            break;
        case GDT_UInt64:
            CheckPacked<std::uint64_t>(eIn, eOut);
            break;
        case GDT_Int64:
            CheckPacked<std::int64_t>(eIn, eOut);
            break;
        case GDT_Float16:
            CheckPacked<GFloat16>(eIn, eOut);
            break;
        case GDT_Float32:
            CheckPacked<float>(eIn, eOut);
            break;
        case GDT_Float64:
            CheckPacked<double>(eIn, eOut);
            break;
        default:
            CPLAssert(false);
    }
}

class TestCopyWordsCheckPackedFixture
    : public TestCopyWords,
      public ::testing::WithParamInterface<
          std::tuple<GDALDataType, GDALDataType>>
{
};

TEST_P(TestCopyWordsCheckPackedFixture, CheckPacked)
{
    GDALDataType eIn = std::get<0>(GetParam());
    GDALDataType eOut = std::get<1>(GetParam());
    CheckPacked(eIn, eOut);
}

static std::vector<std::tuple<GDALDataType, GDALDataType>>
GetGDALDataTypeTupleValues()
{
    std::vector<std::tuple<GDALDataType, GDALDataType>> ret;
    for (GDALDataType eIn = GDT_Byte; eIn < GDT_TypeCount;
         eIn = static_cast<GDALDataType>(eIn + 1))
    {
        if (GDALDataTypeIsComplex(eIn))
            continue;
        for (GDALDataType eOut = GDT_Byte; eOut < GDT_TypeCount;
             eOut = static_cast<GDALDataType>(eOut + 1))
        {
            if (GDALDataTypeIsComplex(eOut))
                continue;
            ret.emplace_back(std::make_tuple(eIn, eOut));
        }
    }
    return ret;
}

INSTANTIATE_TEST_SUITE_P(
    TestCopyWords, TestCopyWordsCheckPackedFixture,
    ::testing::ValuesIn(GetGDALDataTypeTupleValues()),
    [](const ::testing::TestParamInfo<
        TestCopyWordsCheckPackedFixture::ParamType> &l_info)
    {
        GDALDataType eIn = std::get<0>(l_info.param);
        GDALDataType eOut = std::get<1>(l_info.param);
        return std::string(GDALGetDataTypeName(eIn)) + "_" +
               GDALGetDataTypeName(eOut);
    });

TEST_F(TestCopyWords, ByteToByte)
{
    for (int k = 0; k < 2; k++)
    {
        if (k == 1)
            CPLSetConfigOption("GDAL_USE_SSSE3", "NO");

        for (int spacing = 2; spacing <= 4; spacing++)
        {
            memset(pIn, 0xff, 256);
            for (int i = 0; i < 17; i++)
            {
                pIn[spacing * i] = (GByte)(17 - i);
            }
            memset(pOut, 0xff, 256);
            GDALCopyWords(pIn, GDT_Byte, spacing, pOut, GDT_Byte, 1, 17);
            for (int i = 0; i < 17; i++)
            {
                AssertRes(GDT_Byte, 17 - i, GDT_Byte, 17 - i, pOut[i],
                          __LINE__);
            }

            memset(pIn, 0xff, 256);
            memset(pOut, 0xff, 256);
            for (int i = 0; i < 17; i++)
            {
                pIn[i] = (GByte)(17 - i);
            }
            GDALCopyWords(pIn, GDT_Byte, 1, pOut, GDT_Byte, spacing, 17);
            for (int i = 0; i < 17; i++)
            {
                AssertRes(GDT_Byte, 17 - i, GDT_Byte, 17 - i, pOut[i * spacing],
                          __LINE__);
                for (int j = 1; j < spacing; j++)
                {
                    AssertRes(GDT_Byte, 0xff, GDT_Byte, 0xff,
                              pOut[i * spacing + j], __LINE__);
                }
            }
        }
    }
    CPLSetConfigOption("GDAL_USE_SSSE3", nullptr);
}

TEST_F(TestCopyWords, Int16ToInt16)
{
    memset(pIn, 0xff, 256);
    GInt16 *pInShort = (GInt16 *)pIn;
    GInt16 *pOutShort = (GInt16 *)pOut;
    for (int i = 0; i < 9; i++)
    {
        pInShort[2 * i + 0] = 0x1234;
        pInShort[2 * i + 1] = 0x5678;
    }
    for (int iSpacing = 0; iSpacing < 4; iSpacing++)
    {
        memset(pOut, 0xff, 256);
        GDALCopyWords(pInShort, GDT_Int16, sizeof(short), pOutShort, GDT_Int16,
                      (iSpacing + 1) * sizeof(short), 18);
        for (int i = 0; i < 9; i++)
        {
            AssertRes(GDT_Int16, pInShort[2 * i + 0], GDT_Int16,
                      pInShort[2 * i + 0],
                      pOutShort[(iSpacing + 1) * (2 * i + 0)], __LINE__);
            AssertRes(GDT_Int16, pInShort[2 * i + 1], GDT_Int16,
                      pInShort[2 * i + 1],
                      pOutShort[(iSpacing + 1) * (2 * i + 1)], __LINE__);
        }
    }
    for (int iSpacing = 0; iSpacing < 4; iSpacing++)
    {
        memset(pIn, 0xff, 256);
        memset(pOut, 0xff, 256);
        for (int i = 0; i < 9; i++)
        {
            pInShort[(iSpacing + 1) * (2 * i + 0)] = 0x1234;
            pInShort[(iSpacing + 1) * (2 * i + 1)] = 0x5678;
        }
        GDALCopyWords(pInShort, GDT_Int16, (iSpacing + 1) * sizeof(short),
                      pOutShort, GDT_Int16, sizeof(short), 18);
        for (int i = 0; i < 9; i++)
        {
            AssertRes(GDT_Int16, pInShort[(iSpacing + 1) * (2 * i + 0)],
                      GDT_Int16, pInShort[(iSpacing + 1) * (2 * i + 0)],
                      pOutShort[2 * i + 0], __LINE__);
            AssertRes(GDT_Int16, pInShort[(iSpacing + 1) * (2 * i + 1)],
                      GDT_Int16, pInShort[(iSpacing + 1) * (2 * i + 1)],
                      pOutShort[2 * i + 1], __LINE__);
        }
    }
}

}  // namespace
