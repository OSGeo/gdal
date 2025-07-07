/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Core
 * Purpose:  Test GFloat16.
 * Author:   Erik Schnetter <eschnetter at perimeterinstitute.ca>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_float.h"

#include "gtest_include.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{

TEST(TestFloat16, conversions)
{
    for (int i = -2048; i <= +2048; ++i)
    {
        EXPECT_EQ(GFloat16(i), static_cast<double>(i));
        if (i >= -128 && i <= 127)
        {
            EXPECT_EQ(GFloat16(static_cast<signed char>(i)),
                      static_cast<double>(i));
        }
        EXPECT_EQ(GFloat16(static_cast<short>(i)), static_cast<double>(i));
        EXPECT_EQ(GFloat16(static_cast<int>(i)), static_cast<double>(i));
        EXPECT_EQ(GFloat16(static_cast<long>(i)), static_cast<double>(i));
        EXPECT_EQ(GFloat16(static_cast<long long>(i)), static_cast<double>(i));
        if (i >= 0)
        {
            if (i <= 255)
            {
                EXPECT_EQ(GFloat16(static_cast<unsigned char>(i)),
                          static_cast<double>(i));
            }
            EXPECT_EQ(GFloat16(static_cast<unsigned short>(i)),
                      static_cast<double>(i));
            EXPECT_EQ(GFloat16(static_cast<unsigned>(i)),
                      static_cast<double>(i));
            EXPECT_EQ(GFloat16(static_cast<unsigned long>(i)),
                      static_cast<double>(i));
            EXPECT_EQ(GFloat16(static_cast<unsigned long long>(i)),
                      static_cast<double>(i));
        }
        EXPECT_EQ(GFloat16(i), GFloat16(i));
        EXPECT_EQ(GFloat16(i), static_cast<double>(i));
    }

    EXPECT_EQ(GFloat16(65504), 65504.0);
    EXPECT_EQ(GFloat16(-65504), -65504.0);
    // Work around the Windows compiler reporting "error C2124: divide
    // or mod by zero". See also
    // <https://stackoverflow.com/questions/3082508/msvc-erroring-on-a-divide-by-0-that-will-never-happen-fix>.
    volatile double zero = 0.0;
    EXPECT_EQ(GFloat16(1.0 / zero), 1.0 / zero);
    EXPECT_EQ(GFloat16(-1.0 / zero), -1.0 / zero);
    EXPECT_EQ(GFloat16(0.0), -0.0);
    EXPECT_EQ(GFloat16(-0.0), 0.0);
}

TEST(TestFloat16, arithmetic)
{
    for (int i = -100; i <= +100; ++i)
    {
        double x = i;

        EXPECT_EQ(+GFloat16(x), +x);
        EXPECT_EQ(-GFloat16(x), -x);
    }

    for (int i = -100; i <= +100; ++i)
    {
        for (int j = -100; j <= +100; ++j)
        {
            double x = i;
            double y = j;

            EXPECT_EQ(GFloat16(x) + GFloat16(y), x + y);
            EXPECT_EQ(GFloat16(x) - GFloat16(y), x - y);
            using std::fabs;
            EXPECT_NEAR(GFloat16(x) * GFloat16(y), x * y, fabs(x * y / 1024));
            if (j != 0)
            {
                EXPECT_NEAR(GFloat16(x) / GFloat16(y), x / y,
                            fabs(x / y / 1024));
            }
        }
    }
}

TEST(TestFloat16, comparisons)
{
    for (int i = -100; i <= +100; ++i)
    {
        for (int j = -100; j <= +100; ++j)
        {
            double x = i;
            double y = j;

            EXPECT_EQ(GFloat16(x) == GFloat16(y), x == y);
            EXPECT_EQ(GFloat16(x) != GFloat16(y), x != y);
            EXPECT_EQ(GFloat16(x) < GFloat16(y), x < y);
            EXPECT_EQ(GFloat16(x) > GFloat16(y), x > y);
            EXPECT_EQ(GFloat16(x) <= GFloat16(y), x <= y);
            EXPECT_EQ(GFloat16(x) >= GFloat16(y), x >= y);
        }
    }
}

TEST(TestFloat16, math)
{
    for (int i = -100; i <= +100; ++i)
    {
        const double x = i;

        using std::isfinite;
        EXPECT_EQ(isfinite(GFloat16(x)), isfinite(x));
        using std::isinf;
        EXPECT_EQ(isinf(GFloat16(x)), isinf(x));
        using std::isnan;
        EXPECT_EQ(isnan(GFloat16(x)), isnan(x));
        using std::abs;
        EXPECT_EQ(abs(GFloat16(x)), abs(x));
        using std::cbrt;
        using std::fabs;
        EXPECT_NEAR(cbrt(GFloat16(x)), cbrt(x), fabs(cbrt(x) / 1024));
        using std::ceil;
        EXPECT_EQ(ceil(GFloat16(x)), ceil(x));
        using std::fabs;
        EXPECT_EQ(fabs(GFloat16(x)), fabs(x));
        using std::floor;
        EXPECT_EQ(floor(GFloat16(x)), floor(x));
        using std::round;
        EXPECT_EQ(round(GFloat16(x)), round(x));
    }
    for (int i = 0; i <= 100; ++i)
    {
        const double x = i;
        using std::sqrt;
        EXPECT_NEAR(sqrt(GFloat16(x)), sqrt(x), fabs(sqrt(x) / 1024));
    }

    // To avoid Coverity Scan false positive about first value not positive...
    const auto myPow = [](int a, int b)
    {
        double res = 1.0;
        for (int k = 0; k < std::abs(b); ++k)
            res *= a;
        if (b >= 0)
            return res;
        else if (a == 0)
            return std::numeric_limits<double>::infinity();
        else
            return 1.0 / res;
    };

    for (int i = -100; i <= +100; ++i)
    {
        for (int j = -100; j <= +100; ++j)
        {
            const double x = i;
            const double y = j;

            using std::fmax;
            EXPECT_EQ(fmax(GFloat16(x), GFloat16(y)), GFloat16(fmax(x, y)));
            using std::fmin;
            EXPECT_EQ(fmin(GFloat16(x), GFloat16(y)), GFloat16(fmin(x, y)));
            using std::hypot;
            EXPECT_EQ(hypot(GFloat16(x), GFloat16(y)), GFloat16(hypot(x, y)));
            using std::max;
            EXPECT_EQ(max(GFloat16(x), GFloat16(y)), GFloat16(max(x, y)));
            using std::min;
            EXPECT_EQ(min(GFloat16(x), GFloat16(y)), GFloat16(min(x, y)));
            using std::pow;
            EXPECT_EQ(pow(GFloat16(x), GFloat16(y)), GFloat16(myPow(i, j)))
                << "i=" << i << ", j=" << j;
            using std::fabs;
            using std::isfinite;
            GFloat16 r1 = GFloat16(pow(GFloat16(x), j));
            GFloat16 r2 = GFloat16(myPow(i, j));
            if (!isfinite(r1))
            {
                EXPECT_EQ(r1, r2);
            }
            else
            {
                GFloat16 tol = (1 + fabs(r2)) / 1024;
                EXPECT_NEAR(r1, r2, tol);
            }
        }
    }
}

}  // namespace
