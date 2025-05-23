///////////////////////////////////////////////////////////////////////////////
//
// Project:  Internal C++ Test Suite for GDAL/OGR
// Purpose:  Test viewshed algorithm
// Author:   Andrew Bell
//
///////////////////////////////////////////////////////////////////////////////
/*
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include <cmath>
#include <utility>
#include <iostream>

#include "gtest_include.h"

#include "viewshed/viewshed.h"
#include "viewshed/viewshed_types.h"
#include "viewshed/util.h"

namespace gdal
{
namespace viewshed
{

TEST(ViewshedInternal, angle)
{
    EXPECT_DOUBLE_EQ(M_PI / 2, normalizeAngle(0));
    EXPECT_DOUBLE_EQ(M_PI / 4, normalizeAngle(45));
    EXPECT_DOUBLE_EQ(0.0, normalizeAngle(90));
    EXPECT_DOUBLE_EQ(7 * M_PI / 4, normalizeAngle(135));
    EXPECT_DOUBLE_EQ(3 * M_PI / 2, normalizeAngle(180));
    EXPECT_DOUBLE_EQ(M_PI, normalizeAngle(270));
}

TEST(ViewshedInternal, between)
{
    EXPECT_TRUE(rayBetween(M_PI, 0, M_PI / 2));
    EXPECT_FALSE(rayBetween(M_PI, 0, 3 * M_PI / 2));
    EXPECT_TRUE(rayBetween(0, 3 * M_PI / 2, 7 * M_PI / 4));
    EXPECT_TRUE(rayBetween(M_PI / 4, 7 * M_PI / 4, 0));
    EXPECT_FALSE(rayBetween(M_PI / 4, 7 * M_PI / 4, M_PI));
}

TEST(ViewshedInternal, intersect)
{
    // Top side
    EXPECT_TRUE(std::isnan(horizontalIntersect(0, 0, 0, -2)));
    EXPECT_TRUE(std::isnan(horizontalIntersect(M_PI, 0, 0, -2)));
    EXPECT_DOUBLE_EQ(horizontalIntersect(M_PI / 2, 0, 0, -2), 0);
    EXPECT_TRUE(std::isnan(horizontalIntersect(3 * M_PI / 2, 0, 0, -2)));
    EXPECT_DOUBLE_EQ(horizontalIntersect(M_PI / 4, 0, 0, -2), 2);
    EXPECT_DOUBLE_EQ(horizontalIntersect(3 * M_PI / 4, 0, 0, -2), -2);
    EXPECT_TRUE(std::isnan(horizontalIntersect(5 * M_PI / 4, 0, 0, -2)));
    EXPECT_DOUBLE_EQ(horizontalIntersect(M_PI / 6, 0, 0, -2), 2 * std::sqrt(3));

    // Bottom side
    EXPECT_TRUE(std::isnan(horizontalIntersect(0, 0, 0, 2)));
    EXPECT_TRUE(std::isnan(horizontalIntersect(M_PI, 0, 0, 2)));
    EXPECT_TRUE(std::isnan(horizontalIntersect(M_PI / 2, 0, 0, 2)));
    EXPECT_DOUBLE_EQ(horizontalIntersect(3 * M_PI / 2, 0, 0, 2), 0);

    EXPECT_DOUBLE_EQ(horizontalIntersect(5 * M_PI / 4, 0, 0, 2), -2);
    EXPECT_DOUBLE_EQ(horizontalIntersect(7 * M_PI / 4, 0, 0, 2), 2);
    EXPECT_TRUE(std::isnan(horizontalIntersect(3 * M_PI / 4, 0, 0, 2)));
    EXPECT_NEAR(horizontalIntersect(7 * M_PI / 6, 0, 0, 2), -2 * std::sqrt(3),
                1e-10);

    // Right side
    EXPECT_DOUBLE_EQ(verticalIntersect(0, 0, 0, 2), 0);
    EXPECT_TRUE(std::isnan(verticalIntersect(M_PI, 0, 0, 2)));
    EXPECT_TRUE(std::isnan(verticalIntersect(M_PI / 2, 0, 0, 2)));
    EXPECT_TRUE(std::isnan(verticalIntersect(3 * M_PI / 2, 0, 0, 2)));
    EXPECT_TRUE(std::isnan(verticalIntersect(5 * M_PI / 4, 0, 0, 2)));
    EXPECT_DOUBLE_EQ(verticalIntersect(M_PI / 4, 0, 0, 2), -2);
    EXPECT_DOUBLE_EQ(verticalIntersect(7 * M_PI / 4, 0, 0, 2), 2);
    EXPECT_DOUBLE_EQ(verticalIntersect(M_PI / 6, 0, 0, 2), -2 / std::sqrt(3));

    // Left side
    EXPECT_DOUBLE_EQ(verticalIntersect(M_PI, 0, 0, -2), 0);
    EXPECT_TRUE(std::isnan(verticalIntersect(0, 0, 0, -2)));
    EXPECT_TRUE(std::isnan(verticalIntersect(M_PI / 2, 0, 0, -2)));
    EXPECT_TRUE(std::isnan(verticalIntersect(3 * M_PI / 2, 0, 0, -2)));
    EXPECT_TRUE(std::isnan(verticalIntersect(3 * M_PI / 4, 0, 0, 2)));
    EXPECT_DOUBLE_EQ(verticalIntersect(3 * M_PI / 4, 0, 0, -2), -2);
    EXPECT_DOUBLE_EQ(verticalIntersect(5 * M_PI / 4, 0, 0, -2), 2);
    EXPECT_DOUBLE_EQ(verticalIntersect(5 * M_PI / 6, 0, 0, -2),
                     -2 / std::sqrt(3));
}

void testShrinkWindowForAngles(Window &w, int, int, double, double);

TEST(ViewshedInternal, shrinkbox)
{
    auto testExtent = [](double start, double stop, Window expected)
    {
        Window extent{-3, 3, -2, 2};
        testShrinkWindowForAngles(extent, 0, 0, start, stop);
        EXPECT_EQ(extent, expected);
    };

    // Angles are standard (0 right going counter-clockwise
    // We go clockwise from start to stop.
    testExtent(3 * M_PI / 4, M_PI / 4, {-2, 3, -2, 1});
    testExtent(M_PI / 4, 3 * M_PI / 4, {-3, 3, -2, 2});
    testExtent(0.321750554, 2 * M_PI - 0.321750554,
               {0, 3, -1, 2});  // <2, 1>, <2, -1>
    testExtent(2 * M_PI - 0.321750554, 0.321750554,
               {-3, 3, -2, 2});  // <2, -1>, <2, 1>
    testExtent(7 * M_PI / 4, 5 * M_PI / 4, {-2, 3, 0, 2});
    testExtent(5 * M_PI / 4, 7 * M_PI / 4, {-3, 3, -2, 2});
    testExtent(M_PI + 0.321750554, M_PI - 0.321750554,
               {-3, 1, -1, 2});  // <-2, -1>, <-2, 1>
    testExtent(M_PI - 0.321750554, M_PI + 0.321750554,
               {-3, 3, -2, 2});                         // <-2, 1>, <-2, -1>
    testExtent(M_PI / 4, 0.321750554, {0, 3, -2, 1});   // <2, 2>, <2, 1>
    testExtent(0.321750554, M_PI / 4, {-3, 3, -2, 2});  // <2, 1>, <2, 2>
    testExtent(M_PI / 4, 7 * M_PI / 4, {0, 3, -2, 2});
    testExtent(M_PI / 4, M_PI + 0.321750554,
               {-3, 3, -2, 2});  // <2, 2>, <-2, -1>
    testExtent(M_PI + 0.321750554, M_PI / 4,
               {-3, 3, -2, 2});  // <-2, -1>, <2, 2>
}

}  // namespace viewshed
}  // namespace gdal
