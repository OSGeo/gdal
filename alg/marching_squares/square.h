/******************************************************************************
 *
 * Project:  Marching square algorithm
 * Purpose:  Core algorithm implementation for contour line generation.
 * Author:   Oslandia <infos at oslandia dot com>
 *
 ******************************************************************************
 * Copyright (c) 2018, Oslandia <infos at oslandia dot com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/
#ifndef MARCHING_SQUARE_SQUARE_H
#define MARCHING_SQUARE_SQUARE_H

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <math.h>
#include "utility.h"
#include "point.h"

namespace marching_squares
{

struct Square
{
    // Bit flags to determine borders around pixel
    static const uint8_t NO_BORDER = 0;          // 0000 0000
    static const uint8_t LEFT_BORDER = 1 << 0;   // 0000 0001
    static const uint8_t LOWER_BORDER = 1 << 1;  // 0000 0010
    static const uint8_t RIGHT_BORDER = 1 << 2;  // 0000 0100
    static const uint8_t UPPER_BORDER = 1 << 3;  // 0000 1000

    // Bit flags for marching square case
    static const uint8_t ALL_LOW = 0;           // 0000 0000
    static const uint8_t UPPER_LEFT = 1 << 0;   // 0000 0001
    static const uint8_t LOWER_LEFT = 1 << 1;   // 0000 0010
    static const uint8_t LOWER_RIGHT = 1 << 2;  // 0000 0100
    static const uint8_t UPPER_RIGHT = 1 << 3;  // 0000 1000
    static const uint8_t ALL_HIGH =
        UPPER_LEFT | LOWER_LEFT | LOWER_RIGHT | UPPER_RIGHT;    // 0000 1111
    static const uint8_t SADDLE_NW = UPPER_LEFT | LOWER_RIGHT;  // 0000 0101
    static const uint8_t SADDLE_NE = UPPER_RIGHT | LOWER_LEFT;  // 0000 1010

    typedef std::pair<Point, Point> Segment;
    typedef std::pair<ValuedPoint, ValuedPoint> ValuedSegment;

    //
    // An array of segments, at most 3 segments
    struct Segments
    {
        Segments() : sz_(0)
        {
        }

        Segments(const Segment &first) : sz_(1), segs_()
        {
            segs_[0] = first;
        }

        Segments(const Segment &first, const Segment &second) : sz_(2), segs_()
        {
            segs_[0] = first;
            segs_[1] = second;
        }

        Segments(const Segment &first, const Segment &second,
                 const Segment &third)
            : sz_(3), segs_()
        {
            segs_[0] = first;
            segs_[1] = second;
            segs_[2] = third;
        }

        std::size_t size() const
        {
            return sz_;
        }

        const Segment &operator[](std::size_t idx) const
        {
            assert(idx < sz_);
            return segs_[idx];
        }

      private:
        const std::size_t sz_;
        /* const */ Segment segs_[3];
    };

    Square(const ValuedPoint &upperLeft_, const ValuedPoint &upperRight_,
           const ValuedPoint &lowerLeft_, const ValuedPoint &lowerRight_,
           uint8_t borders_ = NO_BORDER, bool split_ = false)
        : upperLeft(upperLeft_), lowerLeft(lowerLeft_), lowerRight(lowerRight_),
          upperRight(upperRight_),
          nanCount((std::isnan(upperLeft.value) ? 1 : 0) +
                   (std::isnan(upperRight.value) ? 1 : 0) +
                   (std::isnan(lowerLeft.value) ? 1 : 0) +
                   (std::isnan(lowerRight.value) ? 1 : 0)),
          borders(borders_), split(split_)
    {
        assert(upperLeft.y == upperRight.y);
        assert(lowerLeft.y == lowerRight.y);
        assert(lowerLeft.x == upperLeft.x);
        assert(lowerRight.x == upperRight.x);
        assert(!split || nanCount == 0);
    }

    Square upperLeftSquare() const
    {
        assert(!std::isnan(upperLeft.value));
        return Square(
            upperLeft, upperCenter(), leftCenter(), center(),
            (std::isnan(upperRight.value) ? RIGHT_BORDER : NO_BORDER) |
                (std::isnan(lowerLeft.value) ? LOWER_BORDER : NO_BORDER),
            true);
    }

    Square lowerLeftSquare() const
    {
        assert(!std::isnan(lowerLeft.value));
        return Square(
            leftCenter(), center(), lowerLeft, lowerCenter(),
            (std::isnan(lowerRight.value) ? RIGHT_BORDER : NO_BORDER) |
                (std::isnan(upperLeft.value) ? UPPER_BORDER : NO_BORDER),
            true);
    }

    Square lowerRightSquare() const
    {
        assert(!std::isnan(lowerRight.value));
        return Square(
            center(), rightCenter(), lowerCenter(), lowerRight,
            (std::isnan(lowerLeft.value) ? LEFT_BORDER : NO_BORDER) |
                (std::isnan(upperRight.value) ? UPPER_BORDER : NO_BORDER),
            true);
    }

    Square upperRightSquare() const
    {
        assert(!std::isnan(upperRight.value));
        return Square(
            upperCenter(), upperRight, center(), rightCenter(),
            (std::isnan(lowerRight.value) ? LOWER_BORDER : NO_BORDER) |
                (std::isnan(upperLeft.value) ? LEFT_BORDER : NO_BORDER),
            true);
    }

    double maxValue() const
    {
        assert(nanCount == 0);
        return std::max(std::max(upperLeft.value, upperRight.value),
                        std::max(lowerLeft.value, lowerRight.value));
    }

    double minValue() const
    {
        assert(nanCount == 0);
        return std::min(std::min(upperLeft.value, upperRight.value),
                        std::min(lowerLeft.value, lowerRight.value));
    }

    ValuedSegment segment(uint8_t border) const
    {
        switch (border)
        {
            case LEFT_BORDER:
                return ValuedSegment(upperLeft, lowerLeft);
            case LOWER_BORDER:
                return ValuedSegment(lowerLeft, lowerRight);
            case RIGHT_BORDER:
                return ValuedSegment(lowerRight, upperRight);
            case UPPER_BORDER:
                return ValuedSegment(upperRight, upperLeft);
        }
        assert(false);
        return ValuedSegment(upperLeft, upperLeft);
    }

    // returns segments of contour
    //
    // segments are oriented:
    //   - they form a vector from their first point to their second point.
    //   - when looking at the vector upward, values greater than the level are
    //   on the right
    //
    //     ^
    //  -  |  +
    Segments segments(double level, double minLevel) const
    {
        switch (marchingCase(level, minLevel))
        {
            case (ALL_LOW):
                // debug("ALL_LOW");
                return Segments();
            case (ALL_HIGH):
                // debug("ALL_HIGH");
                return Segments();
            case (UPPER_LEFT):
                // debug("UPPER_LEFT");
                return Segments(
                    Segment(interpolate(UPPER_BORDER, level, minLevel),
                            interpolate(LEFT_BORDER, level, minLevel)));
            case (LOWER_LEFT):
                // debug("LOWER_LEFT");
                return Segments(
                    Segment(interpolate(LEFT_BORDER, level, minLevel),
                            interpolate(LOWER_BORDER, level, minLevel)));
            case (LOWER_RIGHT):
                // debug("LOWER_RIGHT");
                return Segments(
                    Segment(interpolate(LOWER_BORDER, level, minLevel),
                            interpolate(RIGHT_BORDER, level, minLevel)));
            case (UPPER_RIGHT):
                // debug("UPPER_RIGHT");
                return Segments(
                    Segment(interpolate(RIGHT_BORDER, level, minLevel),
                            interpolate(UPPER_BORDER, level, minLevel)));
            case (UPPER_LEFT | LOWER_LEFT):
                // debug("UPPER_LEFT | LOWER_LEFT");
                return Segments(
                    Segment(interpolate(UPPER_BORDER, level, minLevel),
                            interpolate(LOWER_BORDER, level, minLevel)));
            case (LOWER_LEFT | LOWER_RIGHT):
                // debug("LOWER_LEFT | LOWER_RIGHT");
                return Segments(
                    Segment(interpolate(LEFT_BORDER, level, minLevel),
                            interpolate(RIGHT_BORDER, level, minLevel)));
            case (LOWER_RIGHT | UPPER_RIGHT):
                // debug("LOWER_RIGHT | UPPER_RIGHT");
                return Segments(
                    Segment(interpolate(LOWER_BORDER, level, minLevel),
                            interpolate(UPPER_BORDER, level, minLevel)));
            case (UPPER_RIGHT | UPPER_LEFT):
                // debug("UPPER_RIGHT | UPPER_LEFT");
                return Segments(
                    Segment(interpolate(RIGHT_BORDER, level, minLevel),
                            interpolate(LEFT_BORDER, level, minLevel)));
            case (ALL_HIGH & ~UPPER_LEFT):
                // debug("ALL_HIGH & ~UPPER_LEFT");
                return Segments(
                    Segment(interpolate(LEFT_BORDER, level, minLevel),
                            interpolate(UPPER_BORDER, level, minLevel)));
            case (ALL_HIGH & ~LOWER_LEFT):
                // debug("ALL_HIGH & ~LOWER_LEFT");
                return Segments(
                    Segment(interpolate(LOWER_BORDER, level, minLevel),
                            interpolate(LEFT_BORDER, level, minLevel)));
            case (ALL_HIGH & ~LOWER_RIGHT):
                // debug("ALL_HIGH & ~LOWER_RIGHT");
                return Segments(
                    Segment(interpolate(RIGHT_BORDER, level, minLevel),
                            interpolate(LOWER_BORDER, level, minLevel)));
            case (ALL_HIGH & ~UPPER_RIGHT):
                // debug("ALL_HIGH & ~UPPER_RIGHT");
                return Segments(
                    Segment(interpolate(UPPER_BORDER, level, minLevel),
                            interpolate(RIGHT_BORDER, level, minLevel)));
            case (SADDLE_NE):
            case (SADDLE_NW):
                // From the two possible saddle configurations, we always return
                // the same one.
                //
                // The classical marching square algorithm says the ambiguity
                // should be resolved between the two possible configurations by
                // looking at the value of the center point. But in certain
                // cases, this may lead to line contours from different levels
                // that cross each other and then gives invalid polygons.
                //
                // Arbitrarily choosing one of the two possible configurations
                // is not really that worse than deciding based on the center
                // point.
                return Segments(
                    Segment(interpolate(LEFT_BORDER, level, minLevel),
                            interpolate(LOWER_BORDER, level, minLevel)),
                    Segment(interpolate(RIGHT_BORDER, level, minLevel),
                            interpolate(UPPER_BORDER, level, minLevel)));
        }
        assert(false);
        return Segments();
    }

    template <typename Writer, typename LevelGenerator>
    void process(const LevelGenerator &levelGenerator, Writer &writer) const
    {
        if (nanCount == 4)  // nothing to do
            return;

        if (nanCount)  // split in 4
        {
            if (!std::isnan(upperLeft.value))
                upperLeftSquare().process(levelGenerator, writer);
            if (!std::isnan(upperRight.value))
                upperRightSquare().process(levelGenerator, writer);
            if (!std::isnan(lowerLeft.value))
                lowerLeftSquare().process(levelGenerator, writer);
            if (!std::isnan(lowerRight.value))
                lowerRightSquare().process(levelGenerator, writer);
            return;
        }

        if (writer.polygonize && borders)
        {
            for (uint8_t border :
                 {UPPER_BORDER, LEFT_BORDER, RIGHT_BORDER, LOWER_BORDER})
            {
                // bitwise AND to test which borders we have on the square
                if ((border & borders) == 0)
                    continue;

                // convention: for a level = L, store borders for the previous
                // level up to (and including) L in the border of level "L". For
                // fixed sets of level, this means there is an "Inf" slot for
                // borders of the highest level
                const ValuedSegment s(segment(border));

                Point lastPoint(s.first.x, s.first.y);
                Point endPoint(s.second.x, s.second.y);
                if (s.first.value > s.second.value)
                    std::swap(lastPoint, endPoint);
                bool reverse =
                    (s.first.value > s.second.value) &&
                    ((border == UPPER_BORDER) || (border == LEFT_BORDER));

                auto levelIt =
                    levelGenerator.range(s.first.value, s.second.value);

                auto it = levelIt.begin();  // reused after the for
                for (; it != levelIt.end(); ++it)
                {
                    const int levelIdx = (*it).first;
                    const double level = (*it).second;

                    const Point nextPoint(
                        interpolate(border, level, levelGenerator.minLevel()));
                    if (reverse)
                        writer.addBorderSegment(levelIdx, nextPoint, lastPoint);
                    else
                        writer.addBorderSegment(levelIdx, lastPoint, nextPoint);
                    lastPoint = nextPoint;
                }
                // last level (past the end)
                if (reverse)
                    writer.addBorderSegment((*it).first, endPoint, lastPoint);
                else
                    writer.addBorderSegment((*it).first, lastPoint, endPoint);
            }
        }

        auto range = levelGenerator.range(minValue(), maxValue());
        auto it = range.begin();
        auto itEnd = range.end();
        auto next = range.begin();
        ++next;

        for (; it != itEnd; ++it, ++next)
        {
            const int levelIdx = (*it).first;
            const double level = (*it).second;

            const Segments segments_ =
                segments(level, levelGenerator.minLevel());

            for (std::size_t i = 0; i < segments_.size(); i++)
            {
                const Segment &s = segments_[i];
                writer.addSegment(levelIdx, s.first, s.second);

                if (writer.polygonize)
                {
                    // the contour is used in the polygon of higher level as
                    // well
                    //
                    // TODO: copying the segment to the higher level is easy,
                    // but it involves too much memory. We should reuse segment
                    // contours when constructing polygon rings.
                    writer.addSegment((*next).first, s.first, s.second);
                }
            }
        }
    }

    const ValuedPoint upperLeft;
    const ValuedPoint lowerLeft;
    const ValuedPoint lowerRight;
    const ValuedPoint upperRight;
    const int nanCount;
    const uint8_t borders;
    const bool split;

  private:
    ValuedPoint center() const
    {
        return ValuedPoint(
            .5 * (upperLeft.x + lowerRight.x),
            .5 * (upperLeft.y + lowerRight.y),
            ((std::isnan(lowerLeft.value) ? 0 : lowerLeft.value) +
             (std::isnan(upperLeft.value) ? 0 : upperLeft.value) +
             (std::isnan(lowerRight.value) ? 0 : lowerRight.value) +
             (std::isnan(upperRight.value) ? 0 : upperRight.value)) /
                (4 - nanCount));
    }

    ValuedPoint leftCenter() const
    {
        return ValuedPoint(
            upperLeft.x, .5 * (upperLeft.y + lowerLeft.y),
            std::isnan(upperLeft.value)
                ? lowerLeft.value
                : (std::isnan(lowerLeft.value)
                       ? upperLeft.value
                       : .5 * (upperLeft.value + lowerLeft.value)));
    }

    ValuedPoint lowerCenter() const
    {
        return ValuedPoint(
            .5 * (lowerLeft.x + lowerRight.x), lowerLeft.y,
            std::isnan(lowerRight.value)
                ? lowerLeft.value
                : (std::isnan(lowerLeft.value)
                       ? lowerRight.value
                       : .5 * (lowerRight.value + lowerLeft.value)));
    }

    ValuedPoint rightCenter() const
    {
        return ValuedPoint(
            upperRight.x, .5 * (upperRight.y + lowerRight.y),
            std::isnan(lowerRight.value)
                ? upperRight.value
                : (std::isnan(upperRight.value)
                       ? lowerRight.value
                       : .5 * (lowerRight.value + upperRight.value)));
    }

    ValuedPoint upperCenter() const
    {
        return ValuedPoint(
            .5 * (upperLeft.x + upperRight.x), upperLeft.y,
            std::isnan(upperLeft.value)
                ? upperRight.value
                : (std::isnan(upperRight.value)
                       ? upperLeft.value
                       : .5 * (upperLeft.value + upperRight.value)));
    }

    uint8_t marchingCase(double level, double minLevel) const
    {
        return (level < fudge(upperLeft.value, minLevel, level) ? UPPER_LEFT
                                                                : ALL_LOW) |
               (level < fudge(lowerLeft.value, minLevel, level) ? LOWER_LEFT
                                                                : ALL_LOW) |
               (level < fudge(lowerRight.value, minLevel, level) ? LOWER_RIGHT
                                                                 : ALL_LOW) |
               (level < fudge(upperRight.value, minLevel, level) ? UPPER_RIGHT
                                                                 : ALL_LOW);
    }

    static double interpolate_(double level, double x1, double x2, double y1,
                               double y2, bool need_split, double minLevel)
    {
        if (need_split)
        {
            // The two cases are here to avoid numerical roundup errors, for two
            // points, we always compute the same interpolation. This condition
            // is ensured by the order left->right bottom->top in interpole
            // calls
            //
            // To obtain the same value for border (split) and non-border
            // element, we take the middle value and interpolate from this to
            // the end
            const double xm = .5 * (x1 + x2);
            const double ym = .5 * (y1 + y2);
            const double fy1 = fudge(y1, minLevel, level);
            const double fym = fudge(ym, minLevel, level);
            if ((fy1 < level && level < fym) || (fy1 > level && level > fym))
            {
                x2 = xm;
                y2 = ym;
            }
            else
            {
                x1 = xm;
                y1 = ym;
            }
        }
        const double fy1 = fudge(y1, minLevel, level);
        const double ratio = (level - fy1) / (fudge(y2, minLevel, level) - fy1);
        return x1 * (1. - ratio) + x2 * ratio;
    }

    Point interpolate(uint8_t border, double level, double minLevel) const
    {
        switch (border)
        {
            case LEFT_BORDER:
                return Point(upperLeft.x,
                             interpolate_(level, lowerLeft.y, upperLeft.y,
                                          lowerLeft.value, upperLeft.value,
                                          !split, minLevel));
            case LOWER_BORDER:
                return Point(interpolate_(level, lowerLeft.x, lowerRight.x,
                                          lowerLeft.value, lowerRight.value,
                                          !split, minLevel),
                             lowerLeft.y);
            case RIGHT_BORDER:
                return Point(upperRight.x,
                             interpolate_(level, lowerRight.y, upperRight.y,
                                          lowerRight.value, upperRight.value,
                                          !split, minLevel));
            case UPPER_BORDER:
                return Point(interpolate_(level, upperLeft.x, upperRight.x,
                                          upperLeft.value, upperRight.value,
                                          !split, minLevel),
                             upperLeft.y);
        }
        assert(false);
        return Point();
    }
};
}  // namespace marching_squares
#endif
