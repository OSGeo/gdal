/******************************************************************************
 * $Id$
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

#include "marching_squares/point.h"
#include "marching_squares/level_generator.h"
#include "marching_squares/contour_generator.h"
#include <limits>
#include <map>
#include <fstream>

#include "gtest_include.h"

namespace test_marching_squares_tile
{
using namespace marching_squares;

struct Writer
{
    typedef std::pair<Point, Point> Segment;

    static bool coordEquals(double a, double b)
    {
        return (a - b) * (a - b) < 0.001;
    }

    void addSegment(int levelIdx, const Point &first, const Point &second)
    {
        contours[levelIdx].push_back(Segment(first, second));
    }

    void addBorderSegment(int levelIdx, const Point &first, const Point &second)
    {
        borders[levelIdx].push_back(Segment(first, second));
    }

    // check if a segment is in a set of borders
    bool segmentInBorders(int levelIdx, const Segment &segmentToTest) const
    {
        const auto iter = borders.find(levelIdx);
        if (iter == borders.end())
        {
            CPLAssert(false);
            return false;
        }
        const auto &segments = iter->second;
        for (const Segment &s : segments)
        {
            // (A,B) == (A,B) || (A,B) == (B,A)
            if (((coordEquals(s.first.x, segmentToTest.first.x)) &&
                 (coordEquals(s.first.y, segmentToTest.first.y)) &&
                 (coordEquals(s.second.x, segmentToTest.second.x)) &&
                 (coordEquals(s.second.y, segmentToTest.second.y))) ||
                ((coordEquals(s.second.x, segmentToTest.first.x)) &&
                 (coordEquals(s.second.y, segmentToTest.first.y)) &&
                 (coordEquals(s.first.x, segmentToTest.second.x)) &&
                 (coordEquals(s.first.y, segmentToTest.second.y))))
                return true;
        }
        return false;
    }

    // check if a segment is in a set of contours
    bool segmentInContours(int levelIdx, const Segment &segmentToTest) const
    {
        const auto iter = contours.find(levelIdx);
        if (iter == contours.end())
        {
            CPLAssert(false);
            return false;
        }
        const auto &segments = iter->second;
        for (const Segment &s : segments)
        {
            // (A,B) == (A,B) || (A,B) == (B,A)
            if (((coordEquals(s.first.x, segmentToTest.first.x)) &&
                 (coordEquals(s.first.y, segmentToTest.first.y)) &&
                 (coordEquals(s.second.x, segmentToTest.second.x)) &&
                 (coordEquals(s.second.y, segmentToTest.second.y))) ||
                ((coordEquals(s.second.x, segmentToTest.first.x)) &&
                 (coordEquals(s.second.y, segmentToTest.first.y)) &&
                 (coordEquals(s.first.x, segmentToTest.second.x)) &&
                 (coordEquals(s.first.y, segmentToTest.second.y))))
                return true;
        }
        return false;
    }

    void beginningOfLine()
    {
    }

    void endOfLine()
    {
    }

    std::map<int, std::vector<Segment>> contours;
    std::map<int, std::vector<Segment>> borders;
    const bool polygonize = true;
};

// Common fixture with test data
struct test_ms_tile : public ::testing::Test
{
};

// Dummy test
TEST_F(test_ms_tile, dummy)
{

    // only one pixel of value 2.0
    // levels = 0, 10
    std::vector<double> data = {2.0};
    IntervalLevelRangeIterator levels(0.0, 10.0,
                                      -std::numeric_limits<double>::infinity());
    Writer writer;

    ContourGenerator<Writer, IntervalLevelRangeIterator> cg(
        1, 1, /* hasNoData */ false, NaN, writer, levels);
    cg.feedLine(&data[0]);

    EXPECT_EQ(writer.borders.size(), size_t(1));
    EXPECT_EQ(writer.borders[1].size(), size_t(8));
    EXPECT_TRUE(writer.segmentInBorders(
        1, std::make_pair(Point(0.0, 0.0), Point(0.5, 0.0))));
    EXPECT_TRUE(writer.segmentInBorders(
        1, std::make_pair(Point(0.5, 0.0), Point(1.0, 0.0))));
    EXPECT_TRUE(writer.segmentInBorders(
        1, std::make_pair(Point(1.0, 0.0), Point(1.0, 0.5))));
    EXPECT_TRUE(writer.segmentInBorders(
        1, std::make_pair(Point(1.0, 0.5), Point(1.0, 1.0))));
    EXPECT_TRUE(writer.segmentInBorders(
        1, std::make_pair(Point(1.0, 1.0), Point(0.5, 1.0))));
    EXPECT_TRUE(writer.segmentInBorders(
        1, std::make_pair(Point(0.5, 1.0), Point(0.0, 1.0))));
    EXPECT_TRUE(writer.segmentInBorders(
        1, std::make_pair(Point(0.0, 1.0), Point(0.0, 0.5))));
    EXPECT_TRUE(writer.segmentInBorders(
        1, std::make_pair(Point(0.0, 0.5), Point(0.0, 0.0))));
}

TEST_F(test_ms_tile, tile_one_pixel)
{
    // Tile with one pixel, value below
    // only one pixel of value 2.0
    // levels = 0, 10
    std::vector<double> data = {2.0};
    const double levels[] = {0.0};
    FixedLevelRangeIterator levelGenerator(
        levels, 1, -std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::infinity());
    Writer writer;

    ContourGenerator<Writer, FixedLevelRangeIterator> cg(
        1, 1, /* hasNoData */ false, NaN, writer, levelGenerator);
    cg.feedLine(&data[0]);

    EXPECT_EQ(writer.borders.size(), size_t(1));
    EXPECT_TRUE(levelGenerator.level(1) == Inf);
    EXPECT_EQ(writer.borders[1].size(), size_t(8));
    EXPECT_TRUE(writer.segmentInBorders(
        1, std::make_pair(Point(0.0, 0.0), Point(0.5, 0.0))));
    EXPECT_TRUE(writer.segmentInBorders(
        1, std::make_pair(Point(0.5, 0.0), Point(1.0, 0.0))));
    EXPECT_TRUE(writer.segmentInBorders(
        1, std::make_pair(Point(1.0, 0.0), Point(1.0, 0.5))));
    EXPECT_TRUE(writer.segmentInBorders(
        1, std::make_pair(Point(1.0, 0.5), Point(1.0, 1.0))));
    EXPECT_TRUE(writer.segmentInBorders(
        1, std::make_pair(Point(1.0, 1.0), Point(0.5, 1.0))));
    EXPECT_TRUE(writer.segmentInBorders(
        1, std::make_pair(Point(0.5, 1.0), Point(0.0, 1.0))));
    EXPECT_TRUE(writer.segmentInBorders(
        1, std::make_pair(Point(0.0, 1.0), Point(0.0, 0.5))));
    EXPECT_TRUE(writer.segmentInBorders(
        1, std::make_pair(Point(0.0, 0.5), Point(0.0, 0.0))));
}

TEST_F(test_ms_tile, tile_one_pixel_two)
{
    // Tile with one pixel (2)
    // only one pixel of value 2.0
    // levels = 2, 10
    std::vector<double> data = {2.0};
    IntervalLevelRangeIterator levels(2.0, 10.0,
                                      -std::numeric_limits<double>::infinity());
    Writer writer;

    ContourGenerator<Writer, IntervalLevelRangeIterator> cg(
        1, 1, /* hasNoData */ false, NaN, writer, levels);
    cg.feedLine(&data[0]);

    EXPECT_EQ(writer.borders.size(), size_t(1));
    EXPECT_EQ(writer.borders[1].size(), size_t(8));
    EXPECT_TRUE(writer.segmentInBorders(
        1, std::make_pair(Point(0.0, 0.0), Point(0.5, 0.0))));
    EXPECT_TRUE(writer.segmentInBorders(
        1, std::make_pair(Point(0.5, 0.0), Point(1.0, 0.0))));
    EXPECT_TRUE(writer.segmentInBorders(
        1, std::make_pair(Point(1.0, 0.0), Point(1.0, 0.5))));
    EXPECT_TRUE(writer.segmentInBorders(
        1, std::make_pair(Point(1.0, 0.5), Point(1.0, 1.0))));
    EXPECT_TRUE(writer.segmentInBorders(
        1, std::make_pair(Point(1.0, 1.0), Point(0.5, 1.0))));
    EXPECT_TRUE(writer.segmentInBorders(
        1, std::make_pair(Point(0.5, 1.0), Point(0.0, 1.0))));
    EXPECT_TRUE(writer.segmentInBorders(
        1, std::make_pair(Point(0.0, 1.0), Point(0.0, 0.5))));
    EXPECT_TRUE(writer.segmentInBorders(
        1, std::make_pair(Point(0.0, 0.5), Point(0.0, 0.0))));
}

TEST_F(test_ms_tile, tile_two_pixels)
{
    // Tile with two pixels
    // two pixels
    // 10  7
    // levels = 8
    //
    // pixels
    // +-----+-----+-----+-----+
    // |     |     |     |     |
    // | NaN | NaN | NaN | NaN |
    // |     |     |     |     |
    // +-----+-----+-----+-----+
    // |     |     |     |     |
    // | NaN | 10  |  7  | NaN |
    // |     |     |     |     |
    // +-----+-----+-----+-----+
    // |     |     |     |     |
    // | NaN | NaN | NaN | NaN |
    // |     |     |     |     |
    // +-----+-----+-----+-----+
    //
    // squares
    // +-----+-----+-----+-----+
    // |NaN  | NaN | NaN | NaN |
    // |  +.....+.....+.....+  |
    // |  :  |  :  |  :  |  :  |
    // +--:--+--:--+--:--+--:--+
    // |  :  |10:  | 7:  |NaN  |
    // NaN+.....+.....+.....+  |
    // |  :  |  :  |  :  |  :  |
    // +--:--+--:--+--:--+--:--+
    // |  :  |  :  |  :  |  :  |
    // |  +.....+.....+.....+  |
    // | NaN | NaN | NaN | NaN |
    // +-----+-----+-----+-----+
    //
    // subsquares
    // legend:
    //  :   contour
    //  =   border (level 8)
    //  #   border (level 18)
    //
    //   NaN                NaN                NaN
    //    +------------------+------------------+------------------+
    //    |                  |                  |                  |
    //    |    (0,0)         |      (1,0)       |      (2,0)       |
    //    |      10        10|      8.5        7|        7         |
    //    |        +#########+########+###o=====+========+         |
    //    |        #         |        |   :     |        ||        |
    //    |        #         |        |   :     |        ||        |
    //    |        #         |        |   :     |        ||        |
    //    +--------+---------+--------+---o-----+--------+|--------+
    //    |NaN   10#       10|      8.5   :    7|      7 ||     NaN|
    //    |        #         |        |   :     |        ||        |
    //    |        #         |        |   :     |        ||        |
    //    |        +#########+########+###o=====+========+         |
    //    |       10       10|      8.5        7|        7         |
    //    |     (0,1)        |       (1,1)      |       (2,1)      |
    //    |                  |                  |                  |
    //    +------------------+------------------+------------------+
    //  NaN                 NaN                NaN                NaN

    std::vector<double> data = {10.0, 7.0};
    {
        IntervalLevelRangeIterator levels(
            8.0, 10.0, -std::numeric_limits<double>::infinity());
        Writer writer;
        ContourGenerator<Writer, IntervalLevelRangeIterator> cg(
            2, 1, /* hasNoData */ false, NaN, writer, levels);
        cg.feedLine(&data[0]);

        // check borders
        EXPECT_EQ(writer.borders.size(), size_t(2));
        EXPECT_EQ(writer.borders[0].size(), size_t(6));
        EXPECT_EQ(writer.borders[1].size(), size_t(8));

        EXPECT_TRUE(writer.segmentInBorders(
            0, std::make_pair(Point(1.166, 0.0), Point(1.5, 0.0))));
        EXPECT_TRUE(writer.segmentInBorders(
            0, std::make_pair(Point(1.5, 0.0), Point(2.0, 0.0))));
        EXPECT_TRUE(writer.segmentInBorders(
            0, std::make_pair(Point(2.0, 0.0), Point(2.0, 0.5))));
        EXPECT_TRUE(writer.segmentInBorders(
            0, std::make_pair(Point(2.0, 0.5), Point(2.0, 1.0))));
        EXPECT_TRUE(writer.segmentInBorders(
            0, std::make_pair(Point(2.0, 1.0), Point(1.5, 1.0))));
        EXPECT_TRUE(writer.segmentInBorders(
            0, std::make_pair(Point(1.5, 1.0), Point(1.166, 1.0))));

        EXPECT_TRUE(writer.segmentInBorders(
            1, std::make_pair(Point(1.166, 0.0), Point(1.0, 0.0))));
        EXPECT_TRUE(writer.segmentInBorders(
            1, std::make_pair(Point(1.0, 0.0), Point(0.5, 0.0))));
        EXPECT_TRUE(writer.segmentInBorders(
            1, std::make_pair(Point(0.5, 0.0), Point(0.0, 0.0))));
        EXPECT_TRUE(writer.segmentInBorders(
            1, std::make_pair(Point(0.0, 0.0), Point(0.0, 0.5))));
        EXPECT_TRUE(writer.segmentInBorders(
            1, std::make_pair(Point(0.0, 0.5), Point(0.0, 1.0))));
        EXPECT_TRUE(writer.segmentInBorders(
            1, std::make_pair(Point(0.0, 1.0), Point(0.5, 1.0))));
        EXPECT_TRUE(writer.segmentInBorders(
            1, std::make_pair(Point(0.5, 1.0), Point(1.0, 1.0))));
        EXPECT_TRUE(writer.segmentInBorders(
            1, std::make_pair(Point(1.0, 1.0), Point(1.166, 1.0))));
    }
}

TEST_F(test_ms_tile, tile_four_pixels)
{
    // four pixels
    // 10  7
    //  4  5
    // levels = 8
    // pixels
    // +-----+-----+-----+-----+
    // |     |     |     |     |
    // | NaN | NaN | NaN | NaN |
    // |     |     |     |     |
    // +-----+-----+-----+-----+
    // |     |     |     |     |
    // | NaN | 10  |  7  | NaN |
    // |     |     |     |     |
    // +-----+-----+-----+-----+
    // |     |     |     |     |
    // | NaN |  4  |  5  | NaN |
    // |     |     |     |     |
    // +-----+-----+-----+-----+
    // |     |     |     |     |
    // | NaN | NaN | NaN | NaN |
    // |     |     |     |     |
    // +-----+-----+-----+-----+
    //
    // squares
    // +-----+-----+-----+-----+
    // |NaN  | NaN | NaN | NaN |
    // |  +.....+.....+.....+  |
    // |  :  |  :  |  :  |  :  |
    // +--:--+--:--+--:--+--:--+
    // |  :  |10:  | 7:  |NaN  |
    // NaN+.....+.....+.....+  |
    // |  :  |  :  |  :  |  :  |
    // +--:--+--:--+--:--+--:--+
    // |  :  | 4:  | 5:  |NaN  |
    // NaN+.....+.....+.....+  |
    // |  :  |  :  |  :  |  :  |
    // +--:--+--:--+--:--+--:--+
    // |  :  |  :  |  :  |  :  |
    // |  +.....+.....+.....+  |
    // | NaN | NaN | NaN | NaN |
    // +-----+-----+-----+-----+
    //
    // subsquares
    // legend:
    //  :   contour
    //  =   border (level 8)
    //  #   border (level 18)
    //
    //   NaN                NaN                NaN
    //    +------------------+------------------+------------------+
    //    |                  |                  |                  |
    //    |    (0,0)         |      (1,0)       |      (2,0)       |
    //    |      10        10|      8.5        7|        7         |
    //    |        +#########+########+###o=====+========++        |
    //    |        #         |        |   :     |        ||        |
    //    |        #         |        |   :     |        ||        |
    //    |        #         |        |   :     |        ||        |
    //    +--------+---------+--------+---o-----+--------++--------+
    //    |NaN   10#       10|   ........:     7|      7 ||     NaN|
    //    |        o.........o..:               |        ||        |
    //    |       ||         |                  |        ||        |
    //    |      7++---------+ 7              6 +--------++        |
    //    |       ||         |                  |        ||        |
    //    |       ||         |                  |        ||        |
    //    |       ||         |       4.5        |        ||        |
    //    +-------++---------+--------+---------+--------++--------+
    //    |NaN   4||       4 |        |        5|      5 ||     NaN|
    //    |       ||         |        |         |        ||        |
    //    |       ||         |        |         |        ||        |
    //    |       ++=========+========+=========+========++        |
    //    |        4       4 |      4.5        5|        5         |
    //    |     (0,2)        |       (1,2)      |       (2,2)      |
    //    |                  |                  |                  |
    //    +------------------+------------------+------------------+
    //  NaN                 NaN                NaN                NaN
    std::vector<double> data = {10.0, 7.0, 4.0, 5.0};
    {
        IntervalLevelRangeIterator levels(
            8.0, 10.0, -std::numeric_limits<double>::infinity());
        Writer writer;
        ContourGenerator<Writer, IntervalLevelRangeIterator> cg(
            2, 2, /* hasNoData */ false, NaN, writer, levels);
        cg.feedLine(&data[0]);
        cg.feedLine(&data[2]);

        // check borders
        EXPECT_EQ(writer.borders.size(), size_t(2));
        EXPECT_EQ(writer.borders[0].size(), size_t(13));
        EXPECT_EQ(writer.borders[1].size(), size_t(5));

        EXPECT_TRUE(writer.segmentInBorders(
            1, std::make_pair(Point(1.166, 0.0), Point(1.0, 0.0))));
        EXPECT_TRUE(writer.segmentInBorders(
            1, std::make_pair(Point(1.0, 0.0), Point(0.5, 0.0))));
        EXPECT_TRUE(writer.segmentInBorders(
            1, std::make_pair(Point(0.5, 0.0), Point(0.0, 0.0))));
        EXPECT_TRUE(writer.segmentInBorders(
            1, std::make_pair(Point(0.0, 0.0), Point(0.0, 0.5))));
        EXPECT_TRUE(writer.segmentInBorders(
            1, std::make_pair(Point(0.0, 0.5), Point(0.0, 0.833))));

        // check contour
        EXPECT_EQ(writer.contours.size(), size_t(2));
        EXPECT_EQ(writer.contours[0].size(), size_t(3));
        EXPECT_TRUE(writer.segmentInContours(
            0, std::make_pair(Point(1.166, 0.0), Point(1.166, 0.5))));
        EXPECT_TRUE(writer.segmentInContours(
            0, std::make_pair(Point(1.166, 0.5), Point(0.5, 0.833))));
        EXPECT_TRUE(writer.segmentInContours(
            0, std::make_pair(Point(0.5, 0.833), Point(0.0, 0.833))));
    }
}

TEST_F(test_ms_tile, tile_four_pixels_2)
{
    // four pixels
    // 155    155.01
    // 154.99 155
    // levels = 155

    //   NaN                NaN                NaN
    //    +------------------+------------------+------------------+
    //    |                  |                  |                  |
    //    |    (0,0)         |      (1,0)       |      (2,0)       |
    //    |      155         |     155.005      |      155.01      |
    //    |        +---------+--------+---------+---------+        |
    //    |        |       155        |      155.01       |        |
    //    |        |         |        |         |         |        |
    //    |        |         |     155.005      |         |        |
    //    +--------+---------+--------+---------+---------+--------+
    //    |NaN   155       155               155.01    155.01   NaN|
    //    |        |         |                  |         |        |
    //    |    154.995       |                  |      155.005     |
    //    |        +-------154.995           155.005------+        |
    //    |        |         |                  |         |        |
    //    |        |         |                  |         |        |
    //    |        |         |                  |         |        |
    //    +--------+---------+--------+---------+---------+--------+
    //    |NaN  154.99    154.99   154.995    155       155     NaN|
    //    |        |         |        |         |         |        |
    //    |        |         |        |         |         |        |
    //    |        +---------+--------+---------+---------+        |
    //    |     154.99    154.99   154.995    155       155        |
    //    |     (0,2)        |       (1,2)      |       (2,2)      |
    //    |                  |                  |                  |
    //    +------------------+------------------+------------------+
    //  NaN                 NaN                NaN                NaN

    std::vector<double> data = {155.0, 155.01, 154.99, 155.0};
    {
        const double levels[] = {155.0};
        FixedLevelRangeIterator levelGenerator(
            levels, 1, -std::numeric_limits<double>::infinity(),
            std::numeric_limits<double>::infinity());
        Writer writer;
        ContourGenerator<Writer, FixedLevelRangeIterator> cg(
            2, 2, /* hasNoData */ false, NaN, writer, levelGenerator);
        cg.feedLine(&data[0]);
        cg.feedLine(&data[2]);

        // check borders
        EXPECT_EQ(writer.borders.size(), size_t(2));
        EXPECT_EQ(levelGenerator.level(0), 155.0);
        EXPECT_TRUE(levelGenerator.level(1) == Inf);
        EXPECT_EQ(writer.borders[0].size(), size_t(6));
        EXPECT_EQ(writer.borders[1].size(), size_t(12));
    }
}
}  // namespace test_marching_squares_tile
