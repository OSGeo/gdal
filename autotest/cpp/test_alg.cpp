/******************************************************************************
 * $Id$
 *
 * Project:  GDAL algorithms
 * Purpose:  Test alg
 * Author:   Even Rouault, even.rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2016, Even Rouault <even.rouault at spatialys.com>
 *
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

#include <array>

#include "gdal_unit_test.h"

#include "cpl_conv.h"

#include "gdal_alg.h"
#include "gdalwarper.h"
#include "gdal_priv.h"

#include "gtest_include.h"

namespace
{
// Common fixture with test data
struct test_alg : public ::testing::Test
{
};

typedef struct
{
    double dfLevel;
    int nPoints;
    double x;
    double y;
} writeCbkData;

static CPLErr writeCbk(double dfLevel, int nPoints, double *padfX,
                       double *padfY, void *userData)
{
    writeCbkData *data = (writeCbkData *)userData;
    data->dfLevel = dfLevel;
    data->nPoints = nPoints;
    if (nPoints == 1)
    {
        data->x = padfX[0];
        data->y = padfY[0];
    }
    return CE_None;
}

// Dummy test
TEST_F(test_alg, GDAL_CG_FeedLine_dummy)
{
    writeCbkData data;
    memset(&data, 0, sizeof(data));
    GDALContourGeneratorH hCG =
        GDAL_CG_Create(1, 1, FALSE, 0, 1, 0, writeCbk, &data);
    double scanline[] = {0};
    EXPECT_EQ(GDAL_CG_FeedLine(hCG, scanline), CE_None);
    EXPECT_EQ(data.dfLevel, 0);
    EXPECT_EQ(data.nPoints, 0);
    EXPECT_DOUBLE_EQ(data.x, 0.0);
    EXPECT_DOUBLE_EQ(data.y, 0.0);
    GDAL_CG_Destroy(hCG);
}

// GDALWarpResolveWorkingDataType: default type
TEST_F(test_alg, GDALWarpResolveWorkingDataType_default_type)
{
    GDALWarpOptions *psOptions = GDALCreateWarpOptions();
    GDALWarpResolveWorkingDataType(psOptions);
    EXPECT_EQ(psOptions->eWorkingDataType, GDT_Byte);
    GDALDestroyWarpOptions(psOptions);
}

// GDALWarpResolveWorkingDataType: do not change user specified type
TEST_F(test_alg, GDALWarpResolveWorkingDataType_keep_user_type)
{
    GDALWarpOptions *psOptions = GDALCreateWarpOptions();
    psOptions->eWorkingDataType = GDT_CFloat64;
    GDALWarpResolveWorkingDataType(psOptions);
    EXPECT_EQ(psOptions->eWorkingDataType, GDT_CFloat64);
    GDALDestroyWarpOptions(psOptions);
}

// GDALWarpResolveWorkingDataType: effect of padfSrcNoDataReal
TEST_F(test_alg, GDALWarpResolveWorkingDataType_padfSrcNoDataReal)
{
    GDALWarpOptions *psOptions = GDALCreateWarpOptions();
    psOptions->nBandCount = 1;
    psOptions->padfSrcNoDataReal =
        static_cast<double *>(CPLMalloc(sizeof(double)));
    psOptions->padfSrcNoDataReal[0] = 0.0;
    GDALWarpResolveWorkingDataType(psOptions);
    EXPECT_EQ(psOptions->eWorkingDataType, GDT_Byte);

    psOptions->eWorkingDataType = GDT_Unknown;
    psOptions->padfSrcNoDataReal[0] = -1.0;
    GDALWarpResolveWorkingDataType(psOptions);
    EXPECT_EQ(psOptions->eWorkingDataType, GDT_Int16);

    psOptions->eWorkingDataType = GDT_Unknown;
    psOptions->padfSrcNoDataReal[0] = 2.0;
    GDALWarpResolveWorkingDataType(psOptions);
    EXPECT_EQ(psOptions->eWorkingDataType, GDT_Byte);

    psOptions->eWorkingDataType = GDT_Unknown;
    psOptions->padfSrcNoDataReal[0] = 256.0;
    GDALWarpResolveWorkingDataType(psOptions);
    EXPECT_EQ(psOptions->eWorkingDataType, GDT_UInt16);

    psOptions->eWorkingDataType = GDT_Unknown;
    psOptions->padfSrcNoDataReal[0] = 2.5;
    GDALWarpResolveWorkingDataType(psOptions);
    EXPECT_EQ(psOptions->eWorkingDataType, GDT_Float32);

    psOptions->eWorkingDataType = GDT_Unknown;
    psOptions->padfSrcNoDataReal[0] = 2.12345678;
    GDALWarpResolveWorkingDataType(psOptions);
    EXPECT_EQ(psOptions->eWorkingDataType, GDT_Float64);

    GDALDestroyWarpOptions(psOptions);
}

// GDALWarpResolveWorkingDataType: effect of padfSrcNoDataImag
TEST_F(test_alg, GDALWarpResolveWorkingDataType_padfSrcNoDataImag)
{
    GDALWarpOptions *psOptions = GDALCreateWarpOptions();
    psOptions->nBandCount = 1;
    psOptions->padfSrcNoDataReal =
        static_cast<double *>(CPLMalloc(sizeof(double)));
    psOptions->padfSrcNoDataImag =
        static_cast<double *>(CPLMalloc(sizeof(double)));
    psOptions->padfSrcNoDataReal[0] = 0.0;
    psOptions->padfSrcNoDataImag[0] = 0.0;
    GDALWarpResolveWorkingDataType(psOptions);
    EXPECT_EQ(psOptions->eWorkingDataType, GDT_Byte);

    psOptions->eWorkingDataType = GDT_Unknown;
    psOptions->padfSrcNoDataReal[0] = 0.0;
    psOptions->padfSrcNoDataImag[0] = 1.0;
    GDALWarpResolveWorkingDataType(psOptions);
    // Could probably be CInt16
    EXPECT_EQ(psOptions->eWorkingDataType, GDT_CInt32);

    psOptions->eWorkingDataType = GDT_Unknown;
    psOptions->padfSrcNoDataReal[0] = 0.0;
    psOptions->padfSrcNoDataImag[0] = 1.5;
    GDALWarpResolveWorkingDataType(psOptions);
    EXPECT_EQ(psOptions->eWorkingDataType, GDT_CFloat32);

    psOptions->eWorkingDataType = GDT_Unknown;
    psOptions->padfSrcNoDataReal[0] = 0.0;
    psOptions->padfSrcNoDataImag[0] = 2.12345678;
    GDALWarpResolveWorkingDataType(psOptions);
    EXPECT_EQ(psOptions->eWorkingDataType, GDT_CFloat64);

    GDALDestroyWarpOptions(psOptions);
}

// GDALWarpResolveWorkingDataType: effect of padfDstNoDataReal
TEST_F(test_alg, GDALWarpResolveWorkingDataType_padfDstNoDataReal)
{
    GDALWarpOptions *psOptions = GDALCreateWarpOptions();
    psOptions->nBandCount = 1;
    psOptions->padfDstNoDataReal =
        static_cast<double *>(CPLMalloc(sizeof(double)));
    psOptions->padfDstNoDataReal[0] = 0.0;
    GDALWarpResolveWorkingDataType(psOptions);
    EXPECT_EQ(psOptions->eWorkingDataType, GDT_Byte);

    psOptions->eWorkingDataType = GDT_Unknown;
    psOptions->padfDstNoDataReal[0] = -1.0;
    GDALWarpResolveWorkingDataType(psOptions);
    EXPECT_EQ(psOptions->eWorkingDataType, GDT_Int16);

    psOptions->eWorkingDataType = GDT_Unknown;
    psOptions->padfDstNoDataReal[0] = 2.0;
    GDALWarpResolveWorkingDataType(psOptions);
    EXPECT_EQ(psOptions->eWorkingDataType, GDT_Byte);

    psOptions->eWorkingDataType = GDT_Unknown;
    psOptions->padfDstNoDataReal[0] = 256.0;
    GDALWarpResolveWorkingDataType(psOptions);
    EXPECT_EQ(psOptions->eWorkingDataType, GDT_UInt16);

    psOptions->eWorkingDataType = GDT_Unknown;
    psOptions->padfDstNoDataReal[0] = 2.5;
    GDALWarpResolveWorkingDataType(psOptions);
    EXPECT_EQ(psOptions->eWorkingDataType, GDT_Float32);

    psOptions->eWorkingDataType = GDT_Unknown;
    psOptions->padfDstNoDataReal[0] = 2.12345678;
    GDALWarpResolveWorkingDataType(psOptions);
    EXPECT_EQ(psOptions->eWorkingDataType, GDT_Float64);

    GDALDestroyWarpOptions(psOptions);
}

// GDALWarpResolveWorkingDataType: effect of padfDstNoDataImag
TEST_F(test_alg, GDALWarpResolveWorkingDataType_padfDstNoDataImag)
{
    GDALWarpOptions *psOptions = GDALCreateWarpOptions();
    psOptions->nBandCount = 1;
    psOptions->padfDstNoDataReal =
        static_cast<double *>(CPLMalloc(sizeof(double)));
    psOptions->padfDstNoDataImag =
        static_cast<double *>(CPLMalloc(sizeof(double)));
    psOptions->padfDstNoDataReal[0] = 0.0;
    psOptions->padfDstNoDataImag[0] = 0.0;
    GDALWarpResolveWorkingDataType(psOptions);
    EXPECT_EQ(psOptions->eWorkingDataType, GDT_Byte);

    psOptions->eWorkingDataType = GDT_Unknown;
    psOptions->padfDstNoDataReal[0] = 0.0;
    psOptions->padfDstNoDataImag[0] = 1.0;
    GDALWarpResolveWorkingDataType(psOptions);
    // Could probably be CInt16
    EXPECT_EQ(psOptions->eWorkingDataType, GDT_CInt32);

    psOptions->eWorkingDataType = GDT_Unknown;
    psOptions->padfDstNoDataImag[0] = 0.0;
    psOptions->padfDstNoDataImag[0] = 1.5;
    GDALWarpResolveWorkingDataType(psOptions);
    EXPECT_EQ(psOptions->eWorkingDataType, GDT_CFloat32);

    psOptions->eWorkingDataType = GDT_Unknown;
    psOptions->padfDstNoDataImag[0] = 0.0;
    psOptions->padfDstNoDataImag[0] = 2.12345678;
    GDALWarpResolveWorkingDataType(psOptions);
    EXPECT_EQ(psOptions->eWorkingDataType, GDT_CFloat64);

    GDALDestroyWarpOptions(psOptions);
}

// Test GDALAutoCreateWarpedVRT() with creation of an alpha band
TEST_F(test_alg, GDALAutoCreateWarpedVRT_alpha_band)
{
    GDALDatasetUniquePtr poDS(GDALDriver::FromHandle(GDALGetDriverByName("MEM"))
                                  ->Create("", 1, 1, 1, GDT_Byte, nullptr));
    poDS->SetProjection(SRS_WKT_WGS84_LAT_LONG);
    double adfGeoTransform[6] = {10, 1, 0, 20, 0, -1};
    poDS->SetGeoTransform(adfGeoTransform);
    GDALWarpOptions *psOptions = GDALCreateWarpOptions();
    psOptions->nDstAlphaBand = 2;
    GDALDatasetH hWarpedVRT =
        GDALAutoCreateWarpedVRT(GDALDataset::ToHandle(poDS.get()), nullptr,
                                nullptr, GRA_NearestNeighbour, 0.0, psOptions);
    ASSERT_TRUE(hWarpedVRT != nullptr);
    ASSERT_EQ(GDALGetRasterCount(hWarpedVRT), 2);
    EXPECT_EQ(
        GDALGetRasterColorInterpretation(GDALGetRasterBand(hWarpedVRT, 2)),
        GCI_AlphaBand);
    GDALDestroyWarpOptions(psOptions);
    GDALClose(hWarpedVRT);
}

// Test GDALIsLineOfSightVisible() with single point dataset
TEST_F(test_alg, GDALIsLineOfSightVisible_single_point_dataset)
{
    auto const sizeX = 1;
    auto const sizeY = 1;
    auto const numBands = 1;
    GDALDatasetUniquePtr poDS(
        GDALDriver::FromHandle(GDALGetDriverByName("MEM"))
            ->Create("", sizeX, sizeY, numBands, GDT_Int8, nullptr));
    ASSERT_TRUE(poDS != nullptr);

    int8_t val = 42;
    auto pBand = poDS->GetRasterBand(1);
    ASSERT_TRUE(pBand != nullptr);
    ASSERT_TRUE(poDS->RasterIO(GF_Write, 0, 0, 1, 1, &val, 1, 1, GDT_Int8, 1,
                               nullptr, 0, 0, 0, nullptr) == CE_None);
    // Both points below terrain
    EXPECT_FALSE(
        GDALIsLineOfSightVisible(pBand, 0, 0, 0.0, 0, 0, 0.0, nullptr));
    // One point below terrain
    EXPECT_FALSE(
        GDALIsLineOfSightVisible(pBand, 0, 0, 0.0, 0, 0, 43.0, nullptr));
    // Both points above terrain
    EXPECT_TRUE(
        GDALIsLineOfSightVisible(pBand, 0, 0, 44.0, 0, 0, 43.0, nullptr));
}

// Test GDALIsLineOfSightVisible() with 10x10 default dataset
TEST_F(test_alg, GDALIsLineOfSightVisible_default_square_dataset)
{
    auto const sizeX = 10;
    auto const sizeY = 10;
    auto const numBands = 1;
    GDALDatasetUniquePtr poDS(
        GDALDriver::FromHandle(GDALGetDriverByName("MEM"))
            ->Create("", sizeX, sizeY, numBands, GDT_Int8, nullptr));
    ASSERT_TRUE(poDS != nullptr);

    auto pBand = poDS->GetRasterBand(1);
    ASSERT_TRUE(pBand != nullptr);

    const int x1 = 1;
    const int y1 = 1;
    const int x2 = 2;
    const int y2 = 2;

    // Both points are above terrain.
    EXPECT_TRUE(
        GDALIsLineOfSightVisible(pBand, x1, y1, 1.0, x2, y2, 1.0, nullptr));
    // Flip the order, same result.
    EXPECT_TRUE(
        GDALIsLineOfSightVisible(pBand, x2, y2, 1.0, x1, y1, 1.0, nullptr));

    // One point is below terrain.
    EXPECT_FALSE(
        GDALIsLineOfSightVisible(pBand, x1, y1, -1.0, x2, y2, 1.0, nullptr));
    // Flip the order, same result.
    EXPECT_FALSE(
        GDALIsLineOfSightVisible(pBand, x2, y2, -1.0, x1, y1, 1.0, nullptr));

    // Both points are below terrain.
    EXPECT_FALSE(
        GDALIsLineOfSightVisible(pBand, x1, y1, -1.0, x2, y2, -1.0, nullptr));
    // Flip the order, same result.
    EXPECT_FALSE(
        GDALIsLineOfSightVisible(pBand, x2, y2, -1.0, x1, y1, -1.0, nullptr));
}

// Test GDALIsLineOfSightVisible() through a mountain (not a unit test)
TEST_F(test_alg, GDALIsLineOfSightVisible_through_mountain)
{
    GDALAllRegister();
    const std::string path = "/vsizip/vsicurl/https://terrain.ardupilot.org/"
                             "SRTM1/ap_srtm1.zip/ap_srtm1.vrt";
    const auto poDS = GDALDatasetUniquePtr(
        GDALDataset::FromHandle(GDALOpen(path.c_str(), GA_ReadOnly)));
    if (!poDS)
    {
        GTEST_SKIP() << "Cannot open " << path;
    }

    auto pBand = poDS->GetRasterBand(1);
    ASSERT_TRUE(pBand != nullptr);
    std::array<double, 6> geoFwdTransform;
    ASSERT_TRUE(poDS->GetGeoTransform(geoFwdTransform.data()) == CE_None);
    std::array<double, 6> geoInvTransform;
    ASSERT_TRUE(
        GDALInvGeoTransform(geoFwdTransform.data(), geoInvTransform.data()));

    // Check both sides of the continental divide in Colorado at Eisenhower tunnel.
    const double eisLatE = 39.679000;
    const double eisLngE = -105.903062;
    const double eisLatW = 39.678664;
    const double eisLngW = -105.935403;

    double eisEx, eisEy, eisWx, eisWy;
    GDALApplyGeoTransform(geoInvTransform.data(), eisLngE, eisLatE, &eisEx,
                          &eisEy);
    GDALApplyGeoTransform(geoInvTransform.data(), eisLngW, eisLatW, &eisWx,
                          &eisWy);
    const int iEisEx = static_cast<int>(eisEx);
    const int iEisEy = static_cast<int>(eisEy);
    const int iEisWx = static_cast<int>(eisWx);
    const int iEisWy = static_cast<int>(eisWy);

    // Both points are just above terrain, with terrain between.
    EXPECT_FALSE(GDALIsLineOfSightVisible(pBand, iEisEx, iEisEy, 3380.0, iEisWx,
                                          iEisWy, 3450.0, nullptr));
    // Flip the order, same result.
    EXPECT_FALSE(GDALIsLineOfSightVisible(pBand, iEisWx, iEisWy, 3450.0, iEisEx,
                                          iEisEy, 3380.0, nullptr));

    // Both points above terrain.
    EXPECT_TRUE(GDALIsLineOfSightVisible(pBand, iEisEx, iEisEy, 3900.0, iEisWx,
                                         iEisWy, 3900.0, nullptr));
}

}  // namespace