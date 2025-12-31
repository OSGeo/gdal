/******************************************************************************
 *
 * Project:  GDAL algorithms
 * Purpose:  Tests for the marching squares algorithm
 * Author:   Hugo Mercier, <hugo dot mercier at oslandia dot com>
 *
 ******************************************************************************
 * Copyright (c) 2018, Hugo Mercier, <hugo dot mercier at oslandia dot com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdal_unit_test.h"

#include "gdal_alg.h"

#include "marching_squares/square.h"
#include "marching_squares/level_generator.h"
#include <vector>
#include <limits>
#include <map>
#include <fstream>

#include "gtest_include.h"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wweak-vtables"
#endif

namespace test_marching_squares_square
{
using namespace marching_squares;

struct Writer
{
    typedef std::pair<Point, Point> Segment;

    void addSegment(int levelIdx, const Point &first, const Point &second)
    {
        contours[levelIdx].push_back(Segment(first, second));
    }

    void addBorderSegment(int levelIdx, const Point &first, const Point &second)
    {
        borders[levelIdx].push_back(Segment(first, second));
    }

    std::map<int, std::vector<Segment>> contours;
    std::map<int, std::vector<Segment>> borders;
    const bool polygonize = true;
};

// Common fixture with test data
struct test_ms_square : public ::testing::Test
{
};

// Dummy test
TEST_F(test_ms_square, dummy)
{
    {
        const double levels[] = {0, 4};
        FixedLevelRangeIterator levelGenerator(
            levels, 2, -std::numeric_limits<double>::infinity(),
            std::numeric_limits<double>::infinity());
        auto r = levelGenerator.range(0, 5.0);
        auto b = r.begin();
        EXPECT_EQ((*b).first, 1);
        EXPECT_EQ((*b).second, 4.0);
        auto e = r.end();
        EXPECT_EQ((*e).first, 2);
        EXPECT_EQ((*e).second, Inf);
    }
    {
        IntervalLevelRangeIterator levelGenerator(
            0, 4, -std::numeric_limits<double>::infinity());
        auto r = levelGenerator.range(0, 5.0);
        auto b = r.begin();
        EXPECT_EQ((*b).first, 1);
        EXPECT_EQ((*b).second, 4.0);
        auto e = r.end();
        EXPECT_EQ((*e).first, 2);
        EXPECT_EQ((*e).second, 8.0);
    }
    {
        IntervalLevelRangeIterator levelGenerator(
            0, 10, -std::numeric_limits<double>::infinity());
        auto r = levelGenerator.range(-18, 5.0);
        auto b = r.begin();
        EXPECT_EQ((*b).first, -1);
        EXPECT_EQ((*b).second, -10.0);
        auto e = r.end();
        EXPECT_EQ((*e).first, 1);
        EXPECT_EQ((*e).second, 10.0);
    }
    {
        ExponentialLevelRangeIterator levelGenerator(
            2, -std::numeric_limits<double>::infinity());
        auto r = levelGenerator.range(0, 5.0);
        auto b = r.begin();
        EXPECT_EQ((*b).first, 1);
        EXPECT_EQ((*b).second, 1.0);
        ++b;
        EXPECT_EQ((*b).first, 2);
        EXPECT_EQ((*b).second, 2.0);
        ++b;
        EXPECT_EQ((*b).first, 3);
        EXPECT_EQ((*b).second, 4.0);
        auto e = r.end();
        EXPECT_EQ((*e).first, 4);
        EXPECT_EQ((*e).second, 8.0);
    }
}

TEST_F(test_ms_square, only_zero)
{
    // Square with only 0, level = 0.1
    Square square(ValuedPoint(0, 1, 0), ValuedPoint(1, 1, 0),
                  ValuedPoint(0, 0, 0), ValuedPoint(1, 0, 0));
    Square::Segments segments(square.segments(.1));
    //
    //   0                    0
    //    +------------------+
    //    |                  |
    //    |                  |
    //    |                  |
    //    |                  |
    //    |                  |
    //    |                  |
    //    |                  |
    //    +------------------+
    //   0                    0
    EXPECT_EQ(segments.size(), size_t(0));
}

TEST_F(test_ms_square, only_one)
{
    // Square with only 1, level = 0.1
    Square square(ValuedPoint(0, 1, 1), ValuedPoint(1, 1, 1),
                  ValuedPoint(0, 0, 1), ValuedPoint(1, 0, 1));
    //
    //   1                    1
    //    +------------------+
    //    |                  |
    //    |                  |
    //    |                  |
    //    |                  |
    //    |                  |
    //    |                  |
    //    |                  |
    //    +------------------+
    //   1                    1
    Square::Segments segments(square.segments(.1));
    EXPECT_EQ(segments.size(), size_t(0));
}

TEST_F(test_ms_square, only_zero_level_1)
{
    // Square with only 1, level = 1.0
    Square square(ValuedPoint(0, 1, 1), ValuedPoint(1, 1, 1),
                  ValuedPoint(0, 0, 1), ValuedPoint(1, 0, 1));
    //
    //   1                    1
    //    +------------------+
    //    |                  |
    //    |                  |
    //    |                  |
    //    |                  |
    //    |                  |
    //    |                  |
    //    |                  |
    //    +------------------+
    //   1                    1
    Square::Segments segments(square.segments(1.0));
    EXPECT_EQ(segments.size(), size_t(0));
}

TEST_F(test_ms_square, one_segment)
{
    // Square with one segment, level = 0.1
    Square square(ValuedPoint(0, 1, 1), ValuedPoint(1, 1, 0),
                  ValuedPoint(0, 0, 0), ValuedPoint(1, 0, 0));
    //
    //   0                    0
    //    +------------------+
    //    |                  |
    //    |                  |
    //    |                  |
    //    |                  |
    //    |                  |
    //    o                  |
    //    | \                |
    //    +---o--------------+
    //   1                    0
    Square::Segments segments(square.segments(.1));
    EXPECT_EQ(segments.size(), size_t(1));
    EXPECT_TRUE(segments[0].first == Point(.9, 1));
    EXPECT_TRUE(segments[0].second == Point(0, .1));
}

TEST_F(test_ms_square, fudge_test_1)
{
    // Fudge test 1
    Square square(ValuedPoint(0, 1, 0), ValuedPoint(1, 1, 1),
                  ValuedPoint(0, 0, 1), ValuedPoint(1, 0, 1));
    //
    //   0                    1
    //    +------------------o
    //    |               __/|
    //    |            __/   |
    //    |         __/      |
    //    |       _/         |
    //    |    __/           |
    //    | __/              |
    //    |/                 |
    //    o------------------+
    //   1                    1
    //  (0,0)
    {
        Square::Segments segments(square.segments(0.0));
        EXPECT_EQ(segments.size(), size_t(0));
    }
    {
        Square::Segments segments(square.segments(1.0));
        EXPECT_EQ(segments.size(), size_t(1));
        EXPECT_NEAR(segments[0].first.x, 0.0, 0.001);
        EXPECT_NEAR(segments[0].first.y, 0.0, 0.001);
        EXPECT_NEAR(segments[0].second.x, 1.0, 0.001);
        EXPECT_NEAR(segments[0].second.y, 1.0, 0.001);
    }
}

TEST_F(test_ms_square, fudge_test_2)
{
    // Fudge test 2
    Square square(ValuedPoint(0, 1, 1), ValuedPoint(1, 1, 0),
                  ValuedPoint(0, 0, 0), ValuedPoint(1, 0, 0));
    //
    //   1                    0
    //    +o-----------------+
    //    o+                 |
    //    |                  |
    //    |                  |
    //    |                  |
    //    |                  |
    //    |                  |
    //    |                  |
    //    +------------------+
    //   0                    0
    // (0,0)
    {
        Square::Segments segments(square.segments(1.0));
        EXPECT_EQ(segments.size(), 1U);
        EXPECT_NEAR(segments[0].first.x, 0.0, 0.001);
        EXPECT_NEAR(segments[0].first.y, 1.0, 0.001);
        EXPECT_NEAR(segments[0].second.x, 0.0, 0.001);
        EXPECT_NEAR(segments[0].second.y, 1.0, 0.001);
    }
    {
        Square::Segments segments(square.segments(0.0));
        EXPECT_EQ(segments.size(), 0U);
    }
}

TEST_F(test_ms_square, nan)
{
    // A square with NaN
    const Square square(ValuedPoint(2.500000, 1.500000, 224.990005),
                        ValuedPoint(3.500000, 1.500000, NaN),
                        ValuedPoint(2.500000, 2.500000, 225.029999),
                        ValuedPoint(3.500000, 2.500000, 224.770004));

    //
    // 224.990005            NaN
    //    +------------------+
    //    |                  |
    //    |                  |
    //    |                  |
    //    |                  |
    //    |                  |
    //    |                  |
    //    |                  |
    //    +------------------+
    // 225.029999     224.770004

    const Square ul(square.upperLeftSquare());
    const Square ll(square.lowerLeftSquare());

    // upper left and lower left squares
    //
    // 224.990005 224.990005 NaN
    //    +--------+---------+
    //    |        |         |
    //    |        |         |
    //    |        |         |
    //    +--------+  224.930002
    // 225.010002  |         |
    //    |        |         |
    //    |    224.900001    |
    //    +--------+---------+
    // 225.029999     224.770004

    EXPECT_NEAR(ul.lowerLeft.value, 225.010002, 0.000001);
    EXPECT_NEAR(ul.lowerRight.value, 224.930002, 0.000001);
    EXPECT_NEAR(ul.upperRight.value, 224.990005, 0.000001);
    EXPECT_NEAR(ll.lowerRight.value, 224.900001, 0.000001);

    EXPECT_EQ(ul.lowerLeft.x, ll.upperLeft.x);
    EXPECT_EQ(ul.lowerLeft.y, ll.upperLeft.y);
    EXPECT_EQ(ul.lowerLeft.value, ll.upperLeft.value);

    EXPECT_EQ(ul.lowerRight.x, ll.upperRight.x);
    EXPECT_EQ(ul.lowerRight.y, ll.upperRight.y);
    EXPECT_EQ(ul.lowerRight.value, ll.upperRight.value);

    const Square::Segments segments_up(ul.segments(225));
    const Square::Segments segments_down(ll.segments(225));

    // segments on 225
    //
    // 224.990005 224.990005 NaN
    //    <--------<---------+
    //    |        |         |
    //    o_       |         |
    //    | \      |         |
    //    >--o-----<  224.930002
    // 225.01|002  |         |
    //    |  \     |         |
    //    |   |224.900001    |
    //    >---o----<---------+
    // 225.029999     224.770004

    EXPECT_EQ(segments_up.size(), 1);
    EXPECT_EQ(segments_down.size(), 1);

    // the two segments have a point in common
    EXPECT_EQ(segments_up[0].second, segments_down[0].first);
}

TEST_F(test_ms_square, border_test_1)
{
    // Border test 1
    const Square square(ValuedPoint(0.5, 0.5, NaN), ValuedPoint(1.5, 0.5, NaN),
                        ValuedPoint(0.5, 1.5, 272.87),
                        ValuedPoint(1.5, 1.5, 272.93));
    //
    //   NaN                NaN
    //    +------------------+
    //    |                  |
    //    |                  |
    //    |                  |
    //    |                  |
    //    |                  |
    //    |                  |
    //    |                  |
    //    +------------------+
    // 272.87             272.93
    const Square ll(square.lowerLeftSquare());
    const Square lr(square.lowerRightSquare());

    //
    //   NaN                NaN
    //    +------------------+
    //    |                  |
    //    |                  |
    // 272.87   272.90000 272.93
    //    +--------+---------+
    //    |        |         |
    //    |        |         |
    //    |        |         |
    //    +--------+---------+
    // 272.87   272.90000 272.93

    Square::Segments segments_l(ll.segments(272.9));
    Square::Segments segments_r(lr.segments(272.9));

    // the level falls exactly on corners
    // thanks to the fudge, each corner should be shifted away a bit

    //
    //   NaN                NaN
    //    +------------------+
    //    |                  |
    //    |                  |
    // 272.87   272.90000 272.93
    //    <-------o>--------->
    //    |       :|         |
    //    |       :|         |
    //    |       :|         |
    //    <-------o>--------->
    // 272.87   272.90000 272.93

    EXPECT_EQ(segments_l.size(), size_t(1));
    EXPECT_EQ(segments_r.size(), size_t(0));
}

TEST_F(test_ms_square, multiple_levels)
{
    // Multiple levels
    const Square square(
        ValuedPoint(0.5, 1.5, 272.99), ValuedPoint(1.5, 1.5, NaN),
        ValuedPoint(0.5, 0.5, 273.03), ValuedPoint(1.5, 0.5, 272.9));
    //
    // 272.99               NaN
    //    +------------------+
    //    |                  |
    //    |                  |
    //    |                  |
    //    |                  |
    //    |                  |
    //    |                  |
    //    |                  |
    //    +------------------+
    // 273.03             272.90

    const Square ul(square.upperLeftSquare());

    //
    // 272.99   272.99      NaN
    //    +---------+--------+
    //    |         |        |
    //    |         |        |
    //    |         |        |
    //    +---------+        |
    // 273.01    272.97      |
    //    |                  |
    //    |                  |
    //    +------------------+
    // 273.03             272.90
    EXPECT_TRUE((std::fabs(ul.lowerLeft.value - 273.01) < 0.01));
    EXPECT_TRUE((std::fabs(ul.lowerRight.value - 272.97) < 0.01));
    EXPECT_TRUE((std::fabs(ul.upperRight.value - 272.99) < 0.01));

    // We have a NaN value on the right, we should then have a right border
    EXPECT_TRUE((ul.borders == Square::RIGHT_BORDER));

    Writer writer;
    // levels starting at min and increasing by 0.1
    IntervalLevelRangeIterator levelGenerator(
        0, .1, -std::numeric_limits<double>::infinity());

    ul.process(levelGenerator, writer);

    // we only have a contour when level = 273.0
    // (0.5, 1.5)                  (1.5, 1.5)
    //      272.99   272.99      NaN
    //         +---------+--------+
    //         |         ||       |
    //         o         ||       |
    //         |\        ||       |
    //         +-o-------+        |
    //      273.01    272.97      |
    //         |                  |
    //         |                  |
    //         +------------------+
    //      273.03             272.90
    // (0.5, 0.5)                  (1.5, 0.5)

    EXPECT_TRUE((writer.contours.size() == 2));
    EXPECT_TRUE((writer.borders.size() == 1));
    EXPECT_TRUE((writer.contours.find(2730) != writer.contours.end()));
    EXPECT_TRUE((writer.contours.find(2731) != writer.contours.end()));
    EXPECT_TRUE((writer.borders.find(2730) != writer.borders.end()));
    // we have one segment border on the right
    EXPECT_TRUE((writer.borders[2730].size() == 1));
    EXPECT_TRUE((writer.contours[2730].size() == 1));
    EXPECT_TRUE((writer.contours[2731].size() == 1));
}

TEST_F(test_ms_square, border_test_3)
{
    // Border test 3
    Square square(ValuedPoint(0, 0, 10), ValuedPoint(1, 0, 5),
                  ValuedPoint(0, 1, NaN), ValuedPoint(1, 1, 4));
    // level value = 7
    //   10        7.5        5
    //    +---------+--------+
    //    |         |        |
    //    |        _o        |
    //    |      _/ |        |
    // 10 +====o====+ 6.33   |
    //    |                  |
    //    |                  |
    //    |                  |
    //    +------------------+
    //   NaN                  4
    const Square ul(square.upperLeftSquare());
    // "Lower left value",
    EXPECT_TRUE((std::fabs(ul.lowerLeft.value - 10.00) < 0.01));
    // "Lower right value",
    EXPECT_TRUE((std::fabs(ul.lowerRight.value - 6.33) < 0.01));
    // "Upper right value",
    EXPECT_TRUE((std::fabs(ul.upperRight.value - 7.50) < 0.01));

    // We have a NaN value on the right, we should then have a right border
    // "We have the lower border",
    EXPECT_TRUE(ul.borders == Square::LOWER_BORDER);

    {
        // ... with a level interval
        Writer writer;
        IntervalLevelRangeIterator levelGenerator(
            7, 5, -std::numeric_limits<double>::infinity());
        ul.process(levelGenerator, writer);

        // we have one contour at 7 and 12
        // and two borders: one, at 7 and the second at >7 (12)
        // "We have 2 borders",
        EXPECT_EQ(writer.borders.size(), size_t(2));
        // "We have 2 contours",
        EXPECT_EQ(writer.contours.size(), size_t(2));

        // "Border at 0",
        EXPECT_TRUE(writer.borders.find(0) != writer.borders.end());
        // "Border at 1",
        EXPECT_TRUE(writer.borders.find(1) != writer.borders.end());
        // "No contour at 0",
        EXPECT_TRUE(writer.contours.find(0) != writer.contours.end());
        // and we have one contour and 2 borders
        // "1 contour at 0",
        EXPECT_EQ(writer.contours[0].size(), size_t(1));
        // "1 border at 0",
        EXPECT_EQ(writer.borders[0].size(), size_t(1));
        // "1 border at 1",
        EXPECT_EQ(writer.borders[1].size(), size_t(1));
        // the border at 7.0 is around 0.5, 0.5
        // "Border at 1 is around 0.5, 0.5",
        EXPECT_TRUE((writer.borders[0][0].first.x == 0.5 &&
                     writer.borders[0][0].first.y == 0.5) ||
                    (writer.borders[0][0].second.x == 0.5 &&
                     writer.borders[0][0].second.y == 0.5));
        // the border at 12.0 is around 0, 0.5
        // "Border at 1 is around 0, 0.5",
        EXPECT_TRUE((writer.borders[1][0].first.x == 0.0 &&
                     writer.borders[1][0].first.y == 0.5) ||
                    (writer.borders[1][0].second.x == 0.0 &&
                     writer.borders[1][0].second.y == 0.5));
    }

    // test with a fixed set of levels
    {
        Writer writer;
        std::vector<double> levels = {7.0};
        FixedLevelRangeIterator levelGenerator(
            &levels[0], 1, -std::numeric_limits<double>::infinity(),
            std::numeric_limits<double>::infinity());
        ul.process(levelGenerator, writer);

        // we have one contour at 7 and 12
        // and two borders: one, at 7 and the second at >7 (12)
        // "We have 2 borders",
        EXPECT_EQ(writer.borders.size(), size_t(2));
        // "We have 2 contours",
        EXPECT_EQ(writer.contours.size(), size_t(2));

        // "Border at 0",
        EXPECT_TRUE(writer.borders.find(0) != writer.borders.end());
        // "Border at 1",
        EXPECT_TRUE(writer.borders.find(1) != writer.borders.end());
        // "No contour at 0",
        EXPECT_TRUE(writer.contours.find(0) != writer.contours.end());
        // and we have one contour and 2 borders
        // "1 contour at 0",
        EXPECT_EQ(writer.contours[0].size(), size_t(1));
        // "1 border at 0",
        EXPECT_EQ(writer.borders[0].size(), size_t(1));
        // "1 border at 1",
        EXPECT_EQ(writer.borders[1].size(), size_t(1));
        // the border at 7.0 is around 0.5, 0.5
        // "Border at 1 is around 0.5, 0.5",
        EXPECT_TRUE((writer.borders[0][0].first.x == 0.5 &&
                     writer.borders[0][0].first.y == 0.5) ||
                    (writer.borders[0][0].second.x == 0.5 &&
                     writer.borders[0][0].second.y == 0.5));
        // the border at 12.0 is around 0, 0.5
        // "Border at 1 is around 0, 0.5",
        EXPECT_TRUE((writer.borders[1][0].first.x == 0.0 &&
                     writer.borders[1][0].first.y == 0.5) ||
                    (writer.borders[1][0].second.x == 0.0 &&
                     writer.borders[1][0].second.y == 0.5));
    }
}

TEST_F(test_ms_square, level_value_below_square_values)
{
    // Test level value below square values
    Square square(ValuedPoint(0, 0, 10), ValuedPoint(1, 0, 5),
                  ValuedPoint(0, 1, 8), ValuedPoint(1, 1, 4));
    // level value = 2
    //   10                   5
    //    +------------------+
    //    |                  |
    //    |                  |
    //    |                  |
    //    |                  |
    //    |                  |
    //    |                  |
    //    |                  |
    //    +------------------+
    //    8                   4
    {
        Writer writer;
        std::vector<double> levels = {2.0};
        FixedLevelRangeIterator levelGenerator(
            &levels[0], 1, -std::numeric_limits<double>::infinity(),
            std::numeric_limits<double>::infinity());
        square.process(levelGenerator, writer);
        EXPECT_TRUE((writer.borders.size() == 0));
        EXPECT_TRUE((writer.contours.size() == 0));
    }
}

TEST_F(test_ms_square, full_border_test_1)
{
    // Full border test 1
    Square square(ValuedPoint(-0.5, -0.5, NaN), ValuedPoint(0.5, -0.5, NaN),
                  ValuedPoint(-0.5, 0.5, NaN), ValuedPoint(0.5, 0.5, 5));
    // level value = 0, 10
    //   NaN                NaN
    //    +------------------+
    //    |                  |
    //    |                  |
    //    |                  |
    //    |                  |
    //    |                  |
    //    |                  |
    //    |                  |
    //    +------------------+
    //   NaN                 5
    {
        Writer writer;
        IntervalLevelRangeIterator levelGenerator(
            0, 10.0, -std::numeric_limits<double>::infinity());
        square.process(levelGenerator, writer);
        EXPECT_TRUE((writer.borders.size() == 1));
        EXPECT_TRUE((writer.borders[1].size() == 2));
        EXPECT_TRUE(((writer.borders[1][0].first.x == 0.0 &&
                      writer.borders[1][0].first.y == 0.0) ||
                     (writer.borders[1][0].second.x == 0.0 &&
                      writer.borders[1][0].second.y == 0.0)));
        EXPECT_TRUE(((writer.borders[1][0].first.x == 0.5 &&
                      writer.borders[1][0].first.y == 0.0) ||
                     (writer.borders[1][0].second.x == 0.5 &&
                      writer.borders[1][0].second.y == 0.0)));
        EXPECT_TRUE(((writer.borders[1][1].first.x == 0.0 &&
                      writer.borders[1][1].first.y == 0.0) ||
                     (writer.borders[1][1].second.x == 0.0 &&
                      writer.borders[1][1].second.y == 0.0)));
        EXPECT_TRUE(((writer.borders[1][1].first.x == 0.0 &&
                      writer.borders[1][1].first.y == 0.0) ||
                     (writer.borders[1][1].second.x == 0.0 &&
                      writer.borders[1][1].second.y == 0.5)));
    }
}

TEST_F(test_ms_square, full_border_test_2)
{
    // Full border test 2
    Square square(ValuedPoint(-0.5, -0.5, NaN), ValuedPoint(0.5, -0.5, NaN),
                  ValuedPoint(-0.5, 0.5, NaN), ValuedPoint(0.5, 0.5, 5));
    // level value = 5.0, 10.0
    //   NaN                NaN
    //    +------------------+
    //    |                  |
    //    |                  |
    //    |                  |
    //    |                  |
    //    |                  |
    //    |                  |
    //    |                  |
    //    +------------------+
    //   NaN                 5
    {
        Writer writer;
        IntervalLevelRangeIterator levelGenerator(
            5.0, 5.0, -std::numeric_limits<double>::infinity());
        square.process(levelGenerator, writer);
        EXPECT_TRUE((writer.borders.size() == 1));
        EXPECT_TRUE((writer.borders[1].size() == 2));
        EXPECT_TRUE(((writer.borders[1][0].first.x == 0.0 &&
                      writer.borders[1][0].first.y == 0.0) ||
                     (writer.borders[1][0].second.x == 0.0 &&
                      writer.borders[1][0].second.y == 0.0)));
        EXPECT_TRUE(((writer.borders[1][0].first.x == 0.5 &&
                      writer.borders[1][0].first.y == 0.0) ||
                     (writer.borders[1][0].second.x == 0.5 &&
                      writer.borders[1][0].second.y == 0.0)));
        EXPECT_TRUE(((writer.borders[1][1].first.x == 0.0 &&
                      writer.borders[1][1].first.y == 0.0) ||
                     (writer.borders[1][1].second.x == 0.0 &&
                      writer.borders[1][1].second.y == 0.0)));
        EXPECT_TRUE(((writer.borders[1][1].first.x == 0.0 &&
                      writer.borders[1][1].first.y == 0.0) ||
                     (writer.borders[1][1].second.x == 0.0 &&
                      writer.borders[1][1].second.y == 0.5)));
    }
    {
        Writer writer;
        std::vector<double> levels = {5.0};
        FixedLevelRangeIterator levelGenerator(
            &levels[0], 1, -std::numeric_limits<double>::infinity(),
            std::numeric_limits<double>::infinity());
        square.process(levelGenerator, writer);
        EXPECT_TRUE((writer.borders.size() == 1));
        EXPECT_TRUE((writer.borders[1].size() == 2));
        EXPECT_TRUE(((writer.borders[1][0].first.x == 0.0 &&
                      writer.borders[1][0].first.y == 0.0) ||
                     (writer.borders[1][0].second.x == 0.0 &&
                      writer.borders[1][0].second.y == 0.0)));
        EXPECT_TRUE(((writer.borders[1][0].first.x == 0.5 &&
                      writer.borders[1][0].first.y == 0.0) ||
                     (writer.borders[1][0].second.x == 0.5 &&
                      writer.borders[1][0].second.y == 0.0)));
        EXPECT_TRUE(((writer.borders[1][1].first.x == 0.0 &&
                      writer.borders[1][1].first.y == 0.0) ||
                     (writer.borders[1][1].second.x == 0.0 &&
                      writer.borders[1][1].second.y == 0.0)));
        EXPECT_TRUE(((writer.borders[1][1].first.x == 0.0 &&
                      writer.borders[1][1].first.y == 0.0) ||
                     (writer.borders[1][1].second.x == 0.0 &&
                      writer.borders[1][1].second.y == 0.5)));
    }
}
}  // namespace test_marching_squares_square

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
