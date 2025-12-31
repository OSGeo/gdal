/******************************************************************************
 *
 * Project:  GDAL algorithms
 * Purpose:  Test alg
 * Author:   Even Rouault, even.rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2016, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include <array>
#include <cmath>
#include <limits>

#include "gdal_unit_test.h"

#include "cpl_conv.h"

#include "gdal_alg.h"
#include "gdal_alg_priv.h"
#include "gdalwarper.h"
#include "gdal_priv.h"

#include "gtest_include.h"

#include "test_data.h"

namespace
{
// Common fixture with test data
struct test_alg : public ::testing::Test
{
    std::string data_;

    test_alg()
    {
        // Compose data path for test group
        data_ = tut::common::data_basedir;
    }
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

// GDALWarpResolveWorkingDataType: effect of padfSrcNoDataReal
TEST_F(test_alg, GDALWarpResolveWorkingDataType_padfSrcNoDataReal_with_band)
{
    const auto lambda = [](GDALDataset *poDS)
    {
        std::unique_ptr<GDALWarpOptions, decltype(&GDALDestroyWarpOptions)>
            psOptions(GDALCreateWarpOptions(), GDALDestroyWarpOptions);
        psOptions->hSrcDS = GDALDataset::ToHandle(poDS);
        psOptions->nBandCount = 1;
        psOptions->panSrcBands =
            static_cast<int *>(CPLMalloc(psOptions->nBandCount * sizeof(int)));
        psOptions->panSrcBands[0] = 1;
        psOptions->padfSrcNoDataReal =
            static_cast<double *>(CPLMalloc(sizeof(double)));
        psOptions->padfSrcNoDataReal[0] = 0.0;
        GDALWarpResolveWorkingDataType(psOptions.get());
        EXPECT_EQ(psOptions->eWorkingDataType, GDT_Byte);

        psOptions->padfSrcNoDataReal[0] = -1.0;
        GDALWarpResolveWorkingDataType(psOptions.get());
        EXPECT_EQ(psOptions->eWorkingDataType, GDT_Byte);

        psOptions->eWorkingDataType = GDT_Unknown;
        psOptions->padfSrcNoDataReal[0] = 2.0;
        GDALWarpResolveWorkingDataType(psOptions.get());
        EXPECT_EQ(psOptions->eWorkingDataType, GDT_Byte);
    };

    GDALDatasetUniquePtr poDS(GDALDriver::FromHandle(GDALGetDriverByName("MEM"))
                                  ->Create("", 1, 1, 1, GDT_Byte, nullptr));
    lambda(poDS.get());
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
    EXPECT_FALSE(GDALIsLineOfSightVisible(pBand, 0, 0, 0.0, 0, 0, 0.0, nullptr,
                                          nullptr, nullptr));
    // One point below terrain
    EXPECT_FALSE(GDALIsLineOfSightVisible(pBand, 0, 0, 0.0, 0, 0, 43.0, nullptr,
                                          nullptr, nullptr));
    int xIntersection, yIntersection;
    int *pXIntersection = &xIntersection;
    int *pYIntersection = &yIntersection;
    EXPECT_FALSE(GDALIsLineOfSightVisible(
        pBand, 0, 0, 0.0, 0, 0, 43.0, pXIntersection, pYIntersection, nullptr));
    EXPECT_EQ(*pXIntersection, 0);
    EXPECT_EQ(*pYIntersection, 0);
    // Both points above terrain
    EXPECT_TRUE(GDALIsLineOfSightVisible(pBand, 0, 0, 44.0, 0, 0, 43.0, nullptr,
                                         nullptr, nullptr));
    EXPECT_TRUE(GDALIsLineOfSightVisible(pBand, 0, 0, 44.0, 0, 0, 43.0,
                                         pXIntersection, pYIntersection,
                                         nullptr));
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
    EXPECT_TRUE(GDALIsLineOfSightVisible(pBand, x1, y1, 1.0, x2, y2, 1.0,
                                         nullptr, nullptr, nullptr));
    // Both points are above terrain, supply intersection.
    int xIntersection, yIntersection;
    EXPECT_TRUE(GDALIsLineOfSightVisible(pBand, x1, y1, 1.0, x2, y2, 1.0,
                                         &xIntersection, &yIntersection,
                                         nullptr));
    // Flip the order, same result.
    EXPECT_TRUE(GDALIsLineOfSightVisible(pBand, x2, y2, 1.0, x1, y1, 1.0,
                                         nullptr, nullptr, nullptr));

    // One point is below terrain.
    EXPECT_FALSE(GDALIsLineOfSightVisible(pBand, x1, y1, -1.0, x2, y2, 1.0,
                                          nullptr, nullptr, nullptr));
    int *pXIntersection = &xIntersection;
    int *pYIntersection = &yIntersection;
    EXPECT_FALSE(GDALIsLineOfSightVisible(pBand, x1, y1, -1.0, x2, y2, 1.0,
                                          pXIntersection, pYIntersection,
                                          nullptr));
    EXPECT_EQ(*pXIntersection, 1);
    EXPECT_EQ(*pYIntersection, 1);
    // Flip the order, same result.
    EXPECT_FALSE(GDALIsLineOfSightVisible(pBand, x2, y2, -1.0, x1, y1, 1.0,
                                          pXIntersection, pYIntersection,
                                          nullptr));
    EXPECT_EQ(*pXIntersection, 2);
    EXPECT_EQ(*pYIntersection, 2);

    // Both points are below terrain.
    EXPECT_FALSE(GDALIsLineOfSightVisible(pBand, x1, y1, -1.0, x2, y2, -1.0,
                                          nullptr, nullptr, nullptr));
    // Flip the order, same result.
    EXPECT_FALSE(GDALIsLineOfSightVisible(pBand, x2, y2, -1.0, x1, y1, -1.0,
                                          nullptr, nullptr, nullptr));
}

// Test GDALIsLineOfSightVisible() through a mountain (not a unit test)
TEST_F(test_alg, GDALIsLineOfSightVisible_through_mountain)
{
    GDALAllRegister();

    const std::string path = data_ + SEP + "n43.dt0";
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

    // Check both sides of a mesa (north and south ends).
    // Top mesa at (x=8,y=58,alt=221)
    const double mesaLatTop = 43.5159;
    const double mesaLngTop = -79.9327;

    // Bottom is at (x=12,y=64,alt=199)
    const double mesaLatBottom = 43.4645;
    const double mesaLngBottom = -79.8985;

    // In between the two locations, the mesa reaches a local max altiude of 321.

    double dMesaTopX, dMesaTopY, dMesaBottomX, dMesaBottomY;
    GDALApplyGeoTransform(geoInvTransform.data(), mesaLngTop, mesaLatTop,
                          &dMesaTopX, &dMesaTopY);
    GDALApplyGeoTransform(geoInvTransform.data(), mesaLngBottom, mesaLatBottom,
                          &dMesaBottomX, &dMesaBottomY);
    const int iMesaTopX = static_cast<int>(dMesaTopX);
    const int iMesaTopY = static_cast<int>(dMesaTopY);
    const int iMesaBottomX = static_cast<int>(dMesaBottomX);
    const int iMesaBottomY = static_cast<int>(dMesaBottomY);

    // Both points are just above terrain, with terrain between.
    EXPECT_FALSE(GDALIsLineOfSightVisible(pBand, iMesaTopX, iMesaTopY, 222,
                                          iMesaBottomX, iMesaBottomY, 199,
                                          nullptr, nullptr, nullptr));
    // Flip the order, same result.
    EXPECT_FALSE(GDALIsLineOfSightVisible(pBand, iMesaBottomX, iMesaBottomY,
                                          199, iMesaTopX, iMesaTopY, 222,
                                          nullptr, nullptr, nullptr));

    // Both points above terrain.
    EXPECT_TRUE(GDALIsLineOfSightVisible(pBand, iMesaTopX, iMesaTopY, 322,
                                         iMesaBottomX, iMesaBottomY, 322,
                                         nullptr, nullptr, nullptr));

    // Both points below terrain.
    EXPECT_FALSE(GDALIsLineOfSightVisible(pBand, iMesaTopX, iMesaTopY, 0,
                                          iMesaBottomX, iMesaBottomY, 0,
                                          nullptr, nullptr, nullptr));

    // Test negative slope bresenham diagonals across the whole raster.
    // Both high above terrain.
    EXPECT_TRUE(GDALIsLineOfSightVisible(pBand, 0, 0, 460, 120, 120, 460,
                                         nullptr, nullptr, nullptr));
    // Both heights are 1m above in the corners, but middle terrain violates LOS.
    EXPECT_FALSE(GDALIsLineOfSightVisible(pBand, 0, 0, 295, 120, 120, 183,
                                          nullptr, nullptr, nullptr));

    int xIntersection, yIntersection;
    int *pXIntersection = &xIntersection;
    int *pYIntersection = &yIntersection;
    EXPECT_FALSE(GDALIsLineOfSightVisible(pBand, 0, 0, 295, 120, 120, 183,
                                          pXIntersection, pYIntersection,
                                          nullptr));
    EXPECT_EQ(*pXIntersection, 2);
    EXPECT_EQ(*pYIntersection, 2);

    // Test positive slope bresenham diagnoals across the whole raster.
    // Both high above terrain.
    EXPECT_TRUE(GDALIsLineOfSightVisible(pBand, 0, 120, 460, 120, 0, 460,
                                         nullptr, nullptr, nullptr));
    // Both heights are 1m above in the corners, but middle terrain violates LOS.
    EXPECT_FALSE(GDALIsLineOfSightVisible(pBand, 0, 120, 203, 120, 0, 247,
                                          nullptr, nullptr, nullptr));
    EXPECT_FALSE(GDALIsLineOfSightVisible(pBand, 0, 120, 203, 120, 0, 247,
                                          pXIntersection, pYIntersection,
                                          nullptr));

    EXPECT_EQ(*pXIntersection, 120);
    EXPECT_EQ(*pYIntersection, 0);

    // Vertical line tests with hill between two points, in both directions.
    EXPECT_FALSE(GDALIsLineOfSightVisible(pBand, 83, 111, 154, 83, 117, 198,
                                          nullptr, nullptr, nullptr));
    EXPECT_FALSE(GDALIsLineOfSightVisible(pBand, 83, 117, 198, 83, 111, 154,
                                          nullptr, nullptr, nullptr));
    EXPECT_TRUE(GDALIsLineOfSightVisible(pBand, 83, 111, 460, 83, 117, 460,
                                         nullptr, nullptr, nullptr));
    EXPECT_TRUE(GDALIsLineOfSightVisible(pBand, 83, 117, 460, 83, 111, 460,
                                         nullptr, nullptr, nullptr));

    // Horizontal line tests with hill between two points, in both directions.
    EXPECT_FALSE(GDALIsLineOfSightVisible(pBand, 75, 115, 192, 89, 115, 191,
                                          nullptr, nullptr, nullptr));
    EXPECT_FALSE(GDALIsLineOfSightVisible(pBand, 89, 115, 191, 75, 115, 192,
                                          nullptr, nullptr, nullptr));
    EXPECT_TRUE(GDALIsLineOfSightVisible(pBand, 75, 115, 460, 89, 115, 460,
                                         nullptr, nullptr, nullptr));
    EXPECT_TRUE(GDALIsLineOfSightVisible(pBand, 89, 115, 460, 75, 115, 460,
                                         nullptr, nullptr, nullptr));
}

TEST_F(test_alg, GDALFloatAlmostEquals)
{
    float f = 1.23f;
    EXPECT_TRUE(GDALFloatAlmostEquals(f, f));
    EXPECT_TRUE(GDALFloatAlmostEquals(-f, -f));
    EXPECT_FALSE(GDALFloatAlmostEquals(f, -f));
    EXPECT_FALSE(GDALFloatAlmostEquals(f, 0.0f));
    float f2 = std::nextafter(f, std::numeric_limits<float>::max());
    EXPECT_TRUE(GDALFloatAlmostEquals(f, f2, 1));
    EXPECT_TRUE(GDALFloatAlmostEquals(f2, f, 1));
    EXPECT_TRUE(GDALFloatAlmostEquals(-f, -f2, 1));
    EXPECT_TRUE(GDALFloatAlmostEquals(-f2, -f, 1));
    float f3 = std::nextafter(f2, std::numeric_limits<float>::max());
    EXPECT_FALSE(GDALFloatAlmostEquals(f, f3, 1));
    EXPECT_FALSE(GDALFloatAlmostEquals(f3, f, 1));

    EXPECT_TRUE(GDALFloatAlmostEquals(
        std::nextafter(0.0f, std::numeric_limits<float>::max()),
        std::nextafter(0.0f, -std::numeric_limits<float>::max())));
}

TEST_F(test_alg, GDALDoubleAlmostEquals)
{
    double f = 1.23;
    EXPECT_TRUE(GDALDoubleAlmostEquals(f, f));
    EXPECT_TRUE(GDALDoubleAlmostEquals(-f, -f));
    EXPECT_FALSE(GDALDoubleAlmostEquals(f, -f));
    EXPECT_FALSE(GDALDoubleAlmostEquals(f, 0));
    double f2 = std::nextafter(f, std::numeric_limits<double>::max());
    EXPECT_TRUE(GDALDoubleAlmostEquals(f, f2, 1));
    EXPECT_TRUE(GDALDoubleAlmostEquals(f2, f, 1));
    EXPECT_TRUE(GDALDoubleAlmostEquals(-f, -f2, 1));
    EXPECT_TRUE(GDALDoubleAlmostEquals(-f2, -f, 1));
    double f3 = std::nextafter(f2, std::numeric_limits<double>::max());
    EXPECT_FALSE(GDALDoubleAlmostEquals(f, f3, 1));
    EXPECT_FALSE(GDALDoubleAlmostEquals(f3, f, 1));

    EXPECT_TRUE(GDALDoubleAlmostEquals(
        std::nextafter(0, std::numeric_limits<double>::max()),
        std::nextafter(0, -std::numeric_limits<double>::max())));
}

}  // namespace
