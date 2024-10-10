/******************************************************************************
 * Project:  GDAL Point abstraction
 * Purpose:  Tests for the RawPoint class
 * Author:   Javier Jimenez Shaw
 *
 ******************************************************************************
 * Copyright (c) 2024, Javier Jimenez Shaw
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

#include "gdal_unit_test.h"

#include "gdal_point_templ.h"

#include <limits>

#include "gtest_include.h"

namespace
{

// Common fixture with test data
struct test_point_templ : public ::testing::Test
{
};

// Dummy test
TEST_F(test_point_templ, dummy)
{
    gdal::RawPoint2i a;
    EXPECT_EQ(0, a.x());
    EXPECT_EQ(0, a.y());

    gdal::RawPoint2i p2(2, 3);
    EXPECT_EQ(2, p2.x());
    EXPECT_EQ(3, p2.y());

    gdal::RawPoint3i p3(12, 13, 14);
    EXPECT_EQ(12, p3.x());
    EXPECT_EQ(13, p3.y());
    EXPECT_EQ(14, p3.z());

    gdal::RawPoint<int, 1> p1{2};
    EXPECT_EQ(2, p1.x());
}
}  // namespace
