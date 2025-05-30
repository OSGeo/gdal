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

#include "marching_squares/level_generator.h"
#include "marching_squares/segment_merger.h"
#include "marching_squares/contour_generator.h"

#include <limits>

#include "gtest_include.h"

namespace marching_squares
{
class TestRingAppender
{
  public:
    struct Point
    {
        Point(double xx, double yy) : x(xx), y(yy)
        {
        }

        double x;
        double y;

        bool operator<(const Point &b) const
        {
            return x == b.x ? y < b.y : x < b.x;
        }

        bool operator==(const Point &b) const
        {
            return std::fabs(x - b.x) < 0.001 && std::fabs(y - b.y) < 0.001;
        }

        bool operator!=(const Point &b) const
        {
            return !(*this == b);
        }
    };

    void addLine(double level, LineString &ls, bool /* closed */)
    {
        auto &v = points_[level];
        std::vector<Point> ring;
        for (const auto &pt : ls)
        {
            ring.push_back(Point(pt.x, pt.y));
        }
        v.push_back(std::move(ring));
    }

    bool hasRing(double level, const std::vector<Point> &other) const
    {
        auto it = points_.find(level);
        if (it == points_.end())
        {
            return false;
        }

        const auto &rings = it->second;
        for (const auto &ring : rings)
        {
            if (ringEquals_(ring, other))
            {
                return true;
            }
            else
            {
                // test also the reverse ring
                auto rev = other;
                std::reverse(rev.begin(), rev.end());
                if (ringEquals_(ring, rev))
                {
                    return true;
                }
            }
        }
        return false;
    }

    void out(std::ostream &o, double level)
    {
        for (const auto &p : points_[level])
        {
            out_(o, p);
        }
    }

  private:
    // level -> vector of rings
    std::map<double, std::vector<std::vector<Point>>> points_;

    bool ringEquals_(const std::vector<Point> &aRing,
                     const std::vector<Point> &bRing) const
    {
        if (aRing.size() - 1 != bRing.size())
        {
            return false;
        }

        // rings do not really have a "first" point, but since
        // we represent them with a vector, we need to find a common "first"
        // point
        Point pfirst = aRing[0];
        size_t offset = 0;
        while (offset < bRing.size() && pfirst != bRing[offset])
            offset++;
        if (offset >= bRing.size())
        {
            // can't find a common point
            return false;
        }
        // now compare each point of the two rings
        for (size_t i = 0; i < aRing.size(); i++)
        {
            const Point &p2 = bRing[(i + offset) % bRing.size()];
            if (aRing[i] != p2)
            {
                return false;
            }
        }
        return true;
    }

    void out_(std::ostream &o, const std::vector<Point> &points) const
    {
        o << "{ ";
        for (const auto &pt : points)
        {
            o << "{" << pt.x << "," << pt.y << "}, ";
        }
        o << "}, ";
    }
};
}  // namespace marching_squares

namespace
{
using namespace marching_squares;

// Common fixture with test data
struct test_ms_contour : public ::testing::Test
{
};

// Dummy test
TEST_F(test_ms_contour, dummy)
{
    // one pixel
    std::vector<double> data = {2.0};
    TestRingAppender w;
    {
        IntervalLevelRangeIterator levels(
            0.0, 10.0, -std::numeric_limits<double>::infinity());
        SegmentMerger<TestRingAppender, IntervalLevelRangeIterator> writer(
            w, levels, /* polygonize */ true);
        ContourGenerator<decltype(writer), IntervalLevelRangeIterator> cg(
            1, 1, /* hasNoData */ false, NaN, writer, levels);
        cg.feedLine(&data[0]);

        // "Polygon ring"
        EXPECT_TRUE(w.hasRing(10.0, {{0.0, 0.0},
                                     {0.5, 0.0},
                                     {1.0, 0.0},
                                     {1.0, 0.5},
                                     {1.0, 1.0},
                                     {0.5, 1.0},
                                     {0.0, 1.0},
                                     {0.0, 0.5}}));
    }
}

TEST_F(test_ms_contour, two_pixels)
{
    // two pixels
    // 10  7
    // levels = 8
    std::vector<double> data = {10.0, 7.0};
    TestRingAppender w;

    {
        IntervalLevelRangeIterator levels(
            8.0, 10.0, -std::numeric_limits<double>::infinity());
        SegmentMerger<TestRingAppender, IntervalLevelRangeIterator> writer(
            w, levels, /* polygonize */ true);
        ContourGenerator<decltype(writer), IntervalLevelRangeIterator> cg(
            2, 1, /* hasNoData */ false, NaN, writer, levels);
        cg.feedLine(&data[0]);

        // "Polygon #0"
        EXPECT_TRUE(w.hasRing(8.0, {{1.166, 0.0},
                                    {1.5, 0.0},
                                    {2.0, 0.0},
                                    {2.0, 0.5},
                                    {2.0, 1.0},
                                    {1.5, 1.0},
                                    {1.166, 1.0},
                                    {1.166, 0.5}}));
        // "Polygon #1"
        EXPECT_TRUE(w.hasRing(18.0, {{1.166, 0.0},
                                     {1.0, 0.0},
                                     {0.5, 0.0},
                                     {0.0, 0.0},
                                     {0.0, 0.5},
                                     {0.0, 1.0},
                                     {0.5, 1.0},
                                     {1.0, 1.0},
                                     {1.166, 1.0},
                                     {1.166, 0.5}}));
    }
}

TEST_F(test_ms_contour, four_pixels)
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
    TestRingAppender w;

    {
        IntervalLevelRangeIterator levels(
            8.0, 10.0, -std::numeric_limits<double>::infinity());
        SegmentMerger<TestRingAppender, IntervalLevelRangeIterator> writer(
            w, levels, /* polygonize */ true);
        ContourGenerator<decltype(writer), IntervalLevelRangeIterator> cg(
            2, 2, /* hasNoData */ false, NaN, writer, levels);
        cg.feedLine(&data[0]);
        cg.feedLine(&data[2]);

        // "Polygon #0"
        EXPECT_TRUE(w.hasRing(8.0, {{2.0, 0.0},
                                    {2.0, 0.5},
                                    {2.0, 1.0},
                                    {2.0, 1.5},
                                    {2.0, 2.0},
                                    {1.5, 2.0},
                                    {1.0, 2.0},
                                    {0.5, 2.0},
                                    {0.0, 2.0},
                                    {0.0, 1.5},
                                    {0.0, 1.0},
                                    {0.0, 0.833},
                                    {0.5, 0.833},
                                    {1.167, 0.5},
                                    {1.167, 0.0},
                                    {1.5, 0.0}}));
        // "Polygon #1"
        EXPECT_TRUE(w.hasRing(18.0, {{0.0, 0.0},
                                     {0.5, 0.0},
                                     {1.0, 0.0},
                                     {1.167, 0.0},
                                     {1.167, 0.5},
                                     {0.5, 0.833},
                                     {0, 0.833},
                                     {0.0, 0.5}}));
    }
}

TEST_F(test_ms_contour, saddle_point)
{
    // four pixels
    // two rings
    // with a saddle point
    // 5  10
    // 10  5
    // levels = 8
    // pixels
    // +-----+-----+-----+-----+
    // |     |     |     |     |
    // | NaN | NaN | NaN | NaN |
    // |     |     |     |     |
    // +-----+-----+-----+-----+
    // |     |     |     |     |
    // | NaN |  5  |  10 | NaN |
    // |     |     |     |     |
    // +-----+-----+-----+-----+
    // |     |     |     |     |
    // | NaN | 10  |  5  | NaN |
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
    // |  :  | 5:  |10:  |NaN  |
    // NaN+.....+.....+.....+  |
    // |  :  |  :  |  :  |  :  |
    // +--:--+--:--+--:--+--:--+
    // |  :  |10:  | 5:  |NaN  |
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
    //  #   border (level 8)
    //  =   border (level 18)
    //
    //   NaN                NaN                NaN
    //    +------------------+------------------+------------------+
    //    |                  |                  |                  |
    //    |    (0,0)         |      (1,0)       |      (2,0)       |
    //    |       5         5|      7.5       10|        10        |
    //    |        +#########+########+###o=====+========++        |
    //    |        #         |        |   :     |        ||        |
    //    |        #         |        |   :     |        ||        |
    //    |        #         |        |   :     |        ||        |
    //    +--------+---------+--------+---o-----+--------++--------+
    //    |NaN   5 #        5|             \  10|      10||     NaN|
    //    |        #         |              \___o........o         |
    //    |        #         |                  |        #         |
    //    |    7.5++---------+7.5            7.5+--------+         |
    //    |        #         |                  |        #         |
    //    |        o.........o\_                |        #         |
    //    |       ||         |  \_    7.5       |        #         |
    //    +-------++---------+----\o--+---------+--------+---------+
    //    |NaN  10||       10|     :  |        5|      5 #      NaN|
    //    |       ||         |     :  |         |        #         |
    //    |       ||         |     :  |         |        #         |
    //    |       ++=========+=====o##+#########+########+         |
    //    |      10        10|      7.5        5|        5         |
    //    |     (0,2)        |       (1,2)      |       (2,2)      |
    //    |                  |                  |                  |
    //    +------------------+------------------+------------------+
    //  NaN                 NaN                NaN                NaN

    std::vector<double> data = {5.0, 10.0, 10.0, 5.0};
    TestRingAppender w;

    {
        IntervalLevelRangeIterator levels(
            8.0, 10.0, -std::numeric_limits<double>::infinity());
        SegmentMerger<TestRingAppender, IntervalLevelRangeIterator> writer(
            w, levels, /* polygonize */ true);
        ContourGenerator<decltype(writer), IntervalLevelRangeIterator> cg(
            2, 2, /* hasNoData */ false, NaN, writer, levels);
        cg.feedLine(&data[0]);
        cg.feedLine(&data[2]);

        // "Polygon #0"
        EXPECT_TRUE(w.hasRing(8.0, {{1.5, 2},
                                    {2, 2},
                                    {2, 1.5},
                                    {2, 1},
                                    {2, 0.9},
                                    {1.5, 0.9},
                                    {1.1, 0.5},
                                    {1.1, 0},
                                    {1, 0},
                                    {0.5, 0},
                                    {0, 0},
                                    {0, 0.5},
                                    {0, 1},
                                    {0, 1.1},
                                    {0.5, 1.1},
                                    {0.9, 1.5},
                                    {0.9, 2},
                                    {1, 2}}));
        // "Polygon #1, Ring #0"
        EXPECT_TRUE(w.hasRing(18.0, {{2, 0.9},
                                     {2, 0.5},
                                     {2, 0},
                                     {1.5, 0},
                                     {1.1, 0},
                                     {1.1, 0.5},
                                     {1.5, 0.9}}));
        // "Polygon #1, Ring #1"
        EXPECT_TRUE(w.hasRing(18.0, {{0.9, 1.5},
                                     {0.5, 1.1},
                                     {0, 1.1},
                                     {0, 1.5},
                                     {0, 2},
                                     {0.5, 2},
                                     {0.9, 2}}));
    }
}
}  // namespace
