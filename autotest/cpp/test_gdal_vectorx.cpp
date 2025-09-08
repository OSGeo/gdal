/******************************************************************************
 * Project:  GDAL Vector abstraction
 * Purpose:  Tests for the VectorX class
 * Author:   Javier Jimenez Shaw
 *
 ******************************************************************************
 * Copyright (c) 2024, Javier Jimenez Shaw
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdal_unit_test.h"

#include "gdal_vectorx.h"

#include <array>
#include <cmath>
#include <complex>
#include <limits>
#include <type_traits>

#include "gtest_include.h"

namespace
{

// Common fixture with test data
struct test_vectorx : public ::testing::Test
{
};

// Dummy test
TEST_F(test_vectorx, simple_int)
{
    gdal::Vector2i a;
    EXPECT_EQ(0, a.x());
    EXPECT_EQ(0, a.y());
    static_assert(std::is_same_v<decltype(a)::value_type, int> == true);

    gdal::Vector2i p2(2, 3);
    EXPECT_EQ(2, p2.x());
    EXPECT_EQ(3, p2.y());

    gdal::Vector3i p3(12, 13, 14);
    EXPECT_EQ(12, p3.x());
    EXPECT_EQ(13, p3.y());
    EXPECT_EQ(14, p3.z());

    gdal::VectorX<int, 1> p1{2};
    EXPECT_EQ(2, p1.x());

    gdal::VectorX<int, 4> p4(12, 13, -14, 150);
    EXPECT_EQ(12, p4.x());
    EXPECT_EQ(13, p4.y());
    EXPECT_EQ(-14, p4.z());
    EXPECT_EQ(150, p4[3]);
}

TEST_F(test_vectorx, simple_double)
{
    gdal::Vector2d a;
    EXPECT_EQ(0.0, a.x());
    EXPECT_EQ(0.0, a.y());
    EXPECT_EQ(2U, a.size());
    static_assert(std::is_same_v<decltype(a)::value_type, double> == true);

    gdal::Vector2d p2(2.1, 3.6);
    EXPECT_EQ(2.1, p2.x());
    EXPECT_EQ(3.6, p2.y());
    EXPECT_EQ(2U, p2.size());

    gdal::Vector3d p3(12e-2, -13.0, 14e3);
    EXPECT_EQ(12e-2, p3.x());
    EXPECT_EQ(-13.0, p3.y());
    EXPECT_EQ(14e3, p3.z());
    EXPECT_EQ(3U, p3.size());

    gdal::VectorX<double, 1> p1{2.1};
    EXPECT_EQ(2.1, p1.x());
    EXPECT_EQ(1U, p1.size());

    gdal::VectorX<double, 4> p4(12.0, 13.1, -14.2, 150.0);
    EXPECT_EQ(12.0, p4.x());
    EXPECT_EQ(13.1, p4.y());
    EXPECT_EQ(-14.2, p4.z());
    EXPECT_EQ(150.0, p4[3]);
    EXPECT_EQ(4U, p4.size());
}

TEST_F(test_vectorx, simple_float)
{
    gdal::VectorX<float, 1> p1{2.1f};
    EXPECT_EQ(2.1f, p1.x());
    static_assert(std::is_same_v<decltype(p1)::value_type, float> == true);

    gdal::VectorX<float, 4> p4(12.0f, 13.1f, -14.2f, 150.0f);
    EXPECT_EQ(12.0f, p4.x());
    EXPECT_EQ(13.1f, p4.y());
    EXPECT_EQ(-14.2f, p4.z());
    EXPECT_EQ(150.0f, p4[3]);
}

TEST_F(test_vectorx, simple_complex)
{
    using namespace std::complex_literals;
    gdal::VectorX<std::complex<double>, 2> p2{2.1 + 3.0i, -9.0 + -7.0i};
    EXPECT_EQ(2.1 + 3.0i, p2.x());
    EXPECT_EQ(-9.0 + -7.0i, p2.y());
    static_assert(
        std::is_same_v<decltype(p2)::value_type, std::complex<double>> == true);
}

TEST_F(test_vectorx, array)
{
    gdal::Vector2d p2(2.1, 3.6);
    const std::array<double, 2> arr = p2.array();
    EXPECT_EQ(2.1, arr[0]);
    EXPECT_EQ(3.6, arr[1]);
}

TEST_F(test_vectorx, fill)
{
    const gdal::Vector3d a = gdal::Vector3d().fill(42.0);
    EXPECT_EQ(3U, a.size());
    EXPECT_EQ(42.0, a[0]);
    EXPECT_EQ(42.0, a[1]);
    EXPECT_EQ(42.0, a[2]);
}

TEST_F(test_vectorx, fill_nan)
{
    const gdal::Vector3d a =
        gdal::Vector3d().fill(std::numeric_limits<double>::quiet_NaN());
    EXPECT_EQ(3U, a.size());
    EXPECT_TRUE(std::isnan(a[0]));
    EXPECT_TRUE(std::isnan(a[1]));
    EXPECT_TRUE(std::isnan(a[2]));
}

TEST_F(test_vectorx, change)
{
    gdal::Vector2d p2(2.1, 3.6);
    p2[0] = 7;
    EXPECT_EQ(7, p2.x());
    p2[1] = 10.5;
    EXPECT_EQ(10.5, p2.y());

    gdal::Vector3d p3(12.1, 13.6, -9.0);
    p3.x() = 79;
    EXPECT_EQ(79, p3[0]);
    p3.y() = 10.4;
    EXPECT_EQ(10.4, p3[1]);
    p3.z() = 1.5;
    EXPECT_EQ(1.5, p3[2]);
}

TEST_F(test_vectorx, scalar_prod)
{
    gdal::Vector2d a(2.1, 3.6);
    gdal::Vector2d b(-2.0, 10.0);
    EXPECT_NEAR(2.1 * -2.0 + 3.6 * 10.0, a.scalarProd(b), 1e-10);
}

TEST_F(test_vectorx, norm2)
{
    gdal::Vector2d a(2.1, 3.6);
    EXPECT_NEAR(2.1 * 2.1 + 3.6 * 3.6, a.norm2(), 1e-10);
}

TEST_F(test_vectorx, cast)
{
    gdal::Vector2d a(2.1, -3.6);
    auto b = a.cast<int>();
    static_assert(std::is_same_v<decltype(b)::value_type, int> == true);
    EXPECT_EQ(2, b.x());
    EXPECT_EQ(-3, b.y());

    gdal::Vector2d c = b.cast<double>();
    static_assert(std::is_same_v<decltype(c)::value_type, double> == true);
    EXPECT_EQ(2.0, c.x());
    EXPECT_EQ(-3.0, c.y());
}

TEST_F(test_vectorx, floor)
{
    const gdal::Vector2d a(2.1, -3.6);
    const gdal::Vector2d d = a.floor();
    EXPECT_EQ(2.0, d.x());
    EXPECT_EQ(-4.0, d.y());

    // just to show how to use the template keyword.
    const gdal::Vector2i i = a.floor().template cast<int>();
    EXPECT_EQ(2, i.x());
    EXPECT_EQ(-4, i.y());
}

TEST_F(test_vectorx, ceil)
{
    const gdal::Vector2d a(2.1, -3.6);
    const gdal::Vector2d d = a.ceil();
    EXPECT_EQ(3.0, d.x());
    EXPECT_EQ(-3.0, d.y());
}

TEST_F(test_vectorx, apply)
{
    const gdal::Vector2d a(2.1, -3.6);
    const gdal::Vector2d d =
        a.apply([](const gdal::Vector2d::value_type v) { return v + 1.0; });
    EXPECT_NEAR(3.1, d.x(), 1e-10);
    EXPECT_NEAR(-2.6, d.y(), 1e-10);
}

TEST_F(test_vectorx, sum)
{
    const gdal::Vector2d a(2.1, -3.6);
    const gdal::Vector2d b = a + 2.2;
    EXPECT_NEAR(4.3, b.x(), 1e-10);
    EXPECT_NEAR(-1.4, b.y(), 1e-10);
}

TEST_F(test_vectorx, sum_eq)
{
    gdal::Vector2d a(2.1, -3.6);
    a += 2.0;
    EXPECT_NEAR(4.1, a.x(), 1e-10);
    EXPECT_NEAR(-1.6, a.y(), 1e-10);
}

TEST_F(test_vectorx, sum_eq_int)
{
    gdal::Vector2i a(2, -3);
    a += 1;
    EXPECT_EQ(3, a.x());
    EXPECT_EQ(-2, a.y());
}

TEST_F(test_vectorx, minus)
{
    const gdal::Vector2d a(2.1, -3.6);
    const gdal::Vector2d b = a - 2.2;
    EXPECT_NEAR(-0.1, b.x(), 1e-10);
    EXPECT_NEAR(-5.8, b.y(), 1e-10);
}

TEST_F(test_vectorx, minus_eq)
{
    gdal::Vector2d a(2.1, -3.6);
    a -= 2.0;
    EXPECT_NEAR(0.1, a.x(), 1e-10);
    EXPECT_NEAR(-5.6, a.y(), 1e-10);
}

TEST_F(test_vectorx, minus_eq_int)
{
    gdal::Vector2i a(2, -3);
    a -= 1;
    EXPECT_EQ(1, a.x());
    EXPECT_EQ(-4, a.y());
}

TEST_F(test_vectorx, minus_op)
{
    gdal::Vector2d a(2.1, -3.6);
    const auto b = -a;
    EXPECT_NEAR(-2.1, b.x(), 1e-10);
    EXPECT_NEAR(3.6, b.y(), 1e-10);
}

TEST_F(test_vectorx, multiply_int_double)
{
    gdal::Vector2i a(2, -3);
    const auto b = a * 2.6;
    static_assert(std::is_same_v<decltype(b)::value_type, int> == true);
    EXPECT_EQ(5, b.x());
    EXPECT_EQ(-7, b.y());
}

TEST_F(test_vectorx, multiply_double)
{
    gdal::Vector2d a(2.1, -3.2);
    const auto b = a * 2.6;
    EXPECT_NEAR(5.46, b.x(), 1e-10);
    EXPECT_NEAR(-8.32, b.y(), 1e-10);
}

TEST_F(test_vectorx, divide_int_double)
{
    gdal::Vector2i a(4, -3);
    const auto b = a / 2.2;
    static_assert(std::is_same_v<decltype(b)::value_type, int> == true);
    EXPECT_EQ(1, b.x());
    EXPECT_EQ(-1, b.y());
}

TEST_F(test_vectorx, divide_double)
{
    gdal::Vector2d a(2.1, -3.2);
    const auto b = a / 2.5;
    EXPECT_NEAR(0.84, b.x(), 1e-10);
    EXPECT_NEAR(-1.28, b.y(), 1e-10);
}

TEST_F(test_vectorx, plus_vectorx)
{
    const gdal::Vector2d a(2.1, -3.6);
    const gdal::Vector2d b(10.0, 1.1);
    const auto c = a + b;
    EXPECT_NEAR(12.1, c.x(), 1e-10);
    EXPECT_NEAR(-2.5, c.y(), 1e-10);
}

TEST_F(test_vectorx, minus_vectorx)
{
    const gdal::Vector2d a(2.1, -3.6);
    const gdal::Vector2d b(10.0, 1.1);
    const auto c = a - b;
    EXPECT_NEAR(-7.9, c.x(), 1e-10);
    EXPECT_NEAR(-4.7, c.y(), 1e-10);
}

TEST_F(test_vectorx, plus_scalar_vectorx)
{
    const gdal::Vector2d a(2.1, -3.6);
    const auto b = 2.5 + a;
    EXPECT_NEAR(4.6, b.x(), 1e-10);
    EXPECT_NEAR(-1.1, b.y(), 1e-10);
}

TEST_F(test_vectorx, minus_scalar_vectorx)
{
    const gdal::Vector2d a(2.1, -3.6);
    const auto b = 2.5 - a;
    EXPECT_NEAR(0.4, b.x(), 1e-10);
    EXPECT_NEAR(6.1, b.y(), 1e-10);
}

}  // namespace
