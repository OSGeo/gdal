///////////////////////////////////////////////////////////////////////////////
//
// Project:  C++ Test Suite for GDAL/OGR
// Purpose:  Test constant and builtin arguments for C++ pixel functions
// Author:   Momtchil Momtchev <momtchil@momtchev.com>
//
///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2022, Momtchil Momtchev <momtchil@momtchev.com>
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
#include "../../frmts/vrt/vrtdataset.h"

#include <vector>

#include "gtest_include.h"

CPLErr CustomPixelFuncWithMetadata(void **papoSources, int nSources,
                                   void *pData, int nXSize, int nYSize,
                                   GDALDataType eSrcType, GDALDataType eBufType,
                                   int nPixelSpace, int nLineSpace,
                                   CSLConstList papszArgs);
CPLErr CustomPixelFunc(void **papoSources, int nSources, void *pData,
                       int nXSize, int nYSize, GDALDataType eSrcType,
                       GDALDataType eBufType, int nPixelSpace, int nLineSpace,
                       CSLConstList papszArgs);
CPLErr CustomPixelFuncNoArgs(void **papoSources, int nSources, void *pData,
                             int nXSize, int nYSize, GDALDataType eSrcType,
                             GDALDataType eBufType, int nPixelSpace,
                             int nLineSpace);

CPLErr CustomPixelFuncWithMetadata(void **papoSources, int nSources,
                                   void *pData, int nXSize, int nYSize,
                                   GDALDataType eSrcType, GDALDataType eBufType,
                                   int nPixelSpace, int nLineSpace,
                                   CSLConstList papszArgs)
{

    /* ---- Init ---- */
    if (nSources != 1)
        return CE_Failure;
    const char *pszConstant = CSLFetchNameValue(papszArgs, "customConstant");
    if (pszConstant == nullptr)
        return CE_Failure;
    if (strncmp(pszConstant, "something", strlen("something")))
        return CE_Failure;
    const char *pszScale = CSLFetchNameValue(papszArgs, "scale");
    if (pszScale == nullptr)
        return CE_Failure;

    /* ---- Set pixels ---- */
    size_t ii = 0;
    for (int iLine = 0; iLine < nYSize; ++iLine)
    {
        for (int iCol = 0; iCol < nXSize; ++iCol, ++ii)
        {
            const double dfPixVal = SRCVAL(papoSources[0], eSrcType, ii) * 2;
            GDALCopyWords(&dfPixVal, GDT_Float64, 0,
                          static_cast<GByte *>(pData) +
                              static_cast<GSpacing>(nLineSpace) * iLine +
                              iCol * nPixelSpace,
                          eBufType, nPixelSpace, 1);
        }
    }

    /* ---- Return success ---- */
    return CE_None;
}

CPLErr CustomPixelFunc(void **papoSources, int nSources, void *pData,
                       int nXSize, int nYSize, GDALDataType eSrcType,
                       GDALDataType eBufType, int nPixelSpace, int nLineSpace,
                       CSLConstList papszArgs)
{

    /* ---- Init ---- */
    if (nSources != 1)
        return CE_Failure;

    (void)papszArgs;

    /* ---- Set pixels ---- */
    size_t ii = 0;
    for (int iLine = 0; iLine < nYSize; ++iLine)
    {
        for (int iCol = 0; iCol < nXSize; ++iCol, ++ii)
        {
            const double dfPixVal = SRCVAL(papoSources[0], eSrcType, ii) * 3;
            GDALCopyWords(&dfPixVal, GDT_Float64, 0,
                          static_cast<GByte *>(pData) +
                              static_cast<GSpacing>(nLineSpace) * iLine +
                              iCol * nPixelSpace,
                          eBufType, nPixelSpace, 1);
        }
    }

    /* ---- Return success ---- */
    return CE_None;
}

CPLErr CustomPixelFuncNoArgs(void **papoSources, int nSources, void *pData,
                             int nXSize, int nYSize, GDALDataType eSrcType,
                             GDALDataType eBufType, int nPixelSpace,
                             int nLineSpace)
{

    /* ---- Init ---- */
    if (nSources != 1)
        return CE_Failure;

    /* ---- Set pixels ---- */
    size_t ii = 0;
    for (int iLine = 0; iLine < nYSize; ++iLine)
    {
        for (int iCol = 0; iCol < nXSize; ++iCol, ++ii)
        {
            const double dfPixVal = SRCVAL(papoSources[0], eSrcType, ii) * 4;
            GDALCopyWords(&dfPixVal, GDT_Float64, 0,
                          static_cast<GByte *>(pData) +
                              static_cast<GSpacing>(nLineSpace) * iLine +
                              iCol * nPixelSpace,
                          eBufType, nPixelSpace, 1);
        }
    }

    /* ---- Return success ---- */
    return CE_None;
}

namespace
{
const char pszFuncMetadata[] =
    "<PixelFunctionArgumentsList>"
    "   <Argument name='customConstant' type='constant' value='something'>"
    "   </Argument>"
    "   <Argument type='builtin' value='scale'>"
    "   </Argument>"
    "</PixelFunctionArgumentsList>";

struct test_gdal_pixelfn : public ::testing::Test
{
    std::string src_;
    test_gdal_pixelfn()
    {
        src_ = tut::common::data_basedir;
        src_ += SEP;
        src_ += "pixelfn.vrt";
    }
};

// Test constant parameters in a custom pixel function
TEST_F(test_gdal_pixelfn, custom_pixel_fn_constant_parameters)
{
    GDALAddDerivedBandPixelFuncWithArgs("custom", CustomPixelFuncWithMetadata,
                                        pszFuncMetadata);
    GDALDatasetH ds = GDALOpen(src_.c_str(), GA_ReadOnly);
    ASSERT_TRUE(nullptr != ds);

    GDALRasterBandH band = GDALGetRasterBand(ds, 1);
    ASSERT_TRUE(nullptr != band);

    float buf[20 * 20];
    CPL_IGNORE_RET_VAL(GDALRasterIO(band, GF_Read, 0, 0, 20, 20, buf, 20, 20,
                                    GDT_Float32, 0, 0));

    EXPECT_EQ(buf[0], 107 * 2);

    GDALClose(ds);
}

// Test registering of a custom pixel function without metadata
TEST_F(test_gdal_pixelfn, custom_pixel_fn_without_metadata)
{
    GDALAddDerivedBandPixelFuncWithArgs("custom2", CustomPixelFunc, nullptr);
    GDALDatasetH ds = GDALOpen(src_.c_str(), GA_ReadOnly);
    ASSERT_TRUE(nullptr != ds);

    GDALRasterBandH band = GDALGetRasterBand(ds, 1);
    ASSERT_TRUE(nullptr != band);

    VRTDerivedRasterBand *derived = reinterpret_cast<VRTDerivedRasterBand *>(
        GDALRasterBand::FromHandle(band));
    derived->SetPixelFunctionName("custom2");

    float buf[20 * 20];
    CPL_IGNORE_RET_VAL(GDALRasterIO(band, GF_Read, 0, 0, 20, 20, buf, 20, 20,
                                    GDT_Float32, 0, 0));

    EXPECT_EQ(buf[0], 107 * 3);

    GDALClose(ds);
}

// Test the registering of a custom pixel function without args
TEST_F(test_gdal_pixelfn, custom_pixel_fn_without_args)
{
    GDALAddDerivedBandPixelFunc("custom3", CustomPixelFuncNoArgs);
    GDALDatasetH ds = GDALOpen(src_.c_str(), GA_ReadOnly);
    ASSERT_TRUE(nullptr != ds);

    GDALRasterBandH band = GDALGetRasterBand(ds, 1);
    ASSERT_TRUE(nullptr != band);

    VRTDerivedRasterBand *derived = reinterpret_cast<VRTDerivedRasterBand *>(
        GDALRasterBand::FromHandle(band));
    derived->SetPixelFunctionName("custom3");

    float buf[20 * 20];
    CPL_IGNORE_RET_VAL(GDALRasterIO(band, GF_Read, 0, 0, 20, 20, buf, 20, 20,
                                    GDT_Float32, 0, 0));

    EXPECT_EQ(buf[0], 107 * 4);

    GDALClose(ds);
}

}  // namespace
