/******************************************************************************
 *
 * Project:  GDAL algorithms
 * Purpose:  Test Delaunay triangulation
 * Author:   Even Rouault, even.rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2015, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_conv.h"
#include "gdal_alg.h"

#include "gtest_include.h"

namespace
{

// Common fixture with test data
struct test_triangulation : public ::testing::Test
{
    GDALTriangulation *psDT = nullptr;

    void SetUp() override
    {
        if (!GDALHasTriangulation())
        {
            GTEST_SKIP() << "qhull support missing";
        }
    }

    void TearDown() override
    {
        GDALTriangulationFree(psDT);
        psDT = nullptr;
    }
};

TEST_F(test_triangulation, error_case_1)
{

    double adfX[] = {0, -5, -5, 5, 5};
    double adfY[] = {0, -5, 5, -5, 5};
    CPLPushErrorHandler(CPLQuietErrorHandler);
    CPLSetConfigOption("QHULL_LOG_TO_TEMP_FILE", "YES");
    psDT = GDALTriangulationCreateDelaunay(2, adfX, adfY);
    CPLSetConfigOption("QHULL_LOG_TO_TEMP_FILE", nullptr);
    CPLPopErrorHandler();
    ASSERT_TRUE(psDT == nullptr);
}

TEST_F(test_triangulation, error_case_2)
{
    double adfX[] = {0, 1, 2, 3};
    double adfY[] = {0, 1, 2, 3};
    CPLPushErrorHandler(CPLQuietErrorHandler);
    CPLSetConfigOption("QHULL_LOG_TO_TEMP_FILE", "YES");
    psDT = GDALTriangulationCreateDelaunay(4, adfX, adfY);
    CPLSetConfigOption("QHULL_LOG_TO_TEMP_FILE", nullptr);
    CPLPopErrorHandler();
    ASSERT_TRUE(psDT == nullptr);
}

TEST_F(test_triangulation, nominal)
{
    {
        double adfX[] = {0, -5, -5, 5, 5};
        double adfY[] = {0, -5, 5, -5, 5};
        int i, j;
        psDT = GDALTriangulationCreateDelaunay(5, adfX, adfY);
        ASSERT_TRUE(psDT != nullptr);
        ASSERT_EQ(psDT->nFacets, 4);
        for (i = 0; i < psDT->nFacets; i++)
        {
            for (j = 0; j < 3; j++)
            {
                ASSERT_TRUE(psDT->pasFacets[i].anVertexIdx[j] >= 0);
                ASSERT_TRUE(psDT->pasFacets[i].anVertexIdx[j] <= 4);
                ASSERT_TRUE(psDT->pasFacets[i].anNeighborIdx[j] >= -1);
                ASSERT_TRUE(psDT->pasFacets[i].anNeighborIdx[j] <= 4);
            }
        }
        int face;
        CPLPushErrorHandler(CPLQuietErrorHandler);
        ASSERT_EQ(GDALTriangulationFindFacetDirected(psDT, 0, 0, 0, &face),
                  FALSE);
        ASSERT_EQ(GDALTriangulationFindFacetBruteForce(psDT, 0, 0, &face),
                  FALSE);
        double l1, l2, l3;
        ASSERT_EQ(GDALTriangulationComputeBarycentricCoordinates(psDT, 0, 0, 0,
                                                                 &l1, &l2, &l3),
                  FALSE);
        CPLPopErrorHandler();
        ASSERT_EQ(
            GDALTriangulationComputeBarycentricCoefficients(psDT, adfX, adfY),
            TRUE);
        ASSERT_EQ(
            GDALTriangulationComputeBarycentricCoefficients(psDT, adfX, adfY),
            TRUE);
    }

    // Points inside
    {
        double adfX[] = {0.1, 0.9, 0.499, -0.9};
        double adfY[] = {0.9, 0.1, -0.5, 0.1};
        for (int i = 0; i < 4; i++)
        {
            double x = adfX[i];
            double y = adfY[i];
            int new_face;
            int face;
            ASSERT_EQ(GDALTriangulationFindFacetDirected(psDT, 0, x, y, &face),
                      TRUE);
            ASSERT_TRUE(face >= 0 && face < 4);
            ASSERT_EQ(
                GDALTriangulationFindFacetDirected(psDT, 1, x, y, &new_face),
                TRUE);
            ASSERT_EQ(face, new_face);
            ASSERT_EQ(
                GDALTriangulationFindFacetDirected(psDT, 2, x, y, &new_face),
                TRUE);
            ASSERT_EQ(face, new_face);
            ASSERT_EQ(
                GDALTriangulationFindFacetDirected(psDT, 3, x, y, &new_face),
                TRUE);
            ASSERT_EQ(face, new_face);
            ASSERT_EQ(
                GDALTriangulationFindFacetBruteForce(psDT, x, y, &new_face),
                TRUE);
            ASSERT_EQ(face, new_face);

            double l1, l2, l3;
            GDALTriangulationComputeBarycentricCoordinates(psDT, face, x, y,
                                                           &l1, &l2, &l3);
            ASSERT_TRUE(l1 >= 0 && l1 <= 1);
            ASSERT_TRUE(l2 >= 0 && l2 <= 1);
            ASSERT_TRUE(l3 >= 0 && l3 <= 1);
            ASSERT_NEAR(l3, 1.0 - l1 - l2, 1e-10);
        }
    }

    // Points outside
    {
        double adfX[] = {0, 10, 0, -10};
        double adfY[] = {10, 0, -10, 0};
        for (int i = 0; i < 4; i++)
        {
            double x = adfX[i];
            double y = adfY[i];
            int new_face;
            int face;
            ASSERT_EQ(GDALTriangulationFindFacetDirected(psDT, 0, x, y, &face),
                      FALSE);
            ASSERT_TRUE(face < 0 || (face >= 0 && face < 4));
            ASSERT_EQ(
                GDALTriangulationFindFacetDirected(psDT, 1, x, y, &new_face),
                FALSE);
            ASSERT_EQ(face, new_face);
            ASSERT_EQ(
                GDALTriangulationFindFacetDirected(psDT, 2, x, y, &new_face),
                FALSE);
            ASSERT_EQ(face, new_face);
            ASSERT_EQ(
                GDALTriangulationFindFacetDirected(psDT, 3, x, y, &new_face),
                FALSE);
            ASSERT_EQ(face, new_face);
            ASSERT_EQ(
                GDALTriangulationFindFacetBruteForce(psDT, x, y, &new_face),
                FALSE);
            ASSERT_EQ(face, new_face);

            double l1, l2, l3;
            if (face < 0)
                face = 0;
            GDALTriangulationComputeBarycentricCoordinates(psDT, face, x, y,
                                                           &l1, &l2, &l3);
            ASSERT_TRUE(!((l1 >= 0 && l1 <= 1) && (l2 >= 0 && l2 <= 1) &&
                          (l3 >= 0 && l3 <= 1)))
                << "outside";
            ASSERT_NEAR(l3, 1.0 - l1 - l2, 1e-10);
        }
    }
}
}  // namespace
