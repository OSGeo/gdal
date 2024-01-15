/**********************************************************************
 *
 * Project:  GDAL
 * Purpose:  Test OGRGeometryFactory::organizePolygons
 * Author:   Daniel Baston <dbaston at gmail.com>
 *
 **********************************************************************
 * Copyright (c) 2023, ISciences LLC
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "gdal_unit_test.h"
#include "cpl_string.h"
#include "ogr_geometry.h"
#include "gtest_include.h"

#include <vector>

static std::unique_ptr<OGRGeometry>
organizePolygons(std::vector<OGRGeometry *> &polygons,
                 const std::string &method)
{
    CPLStringList options;
    options.AddNameValue("METHOD", method.c_str());

    return std::unique_ptr<OGRGeometry>(OGRGeometryFactory::organizePolygons(
        polygons.data(), static_cast<int>(polygons.size()), nullptr,
        (const char **)options.List()));
}

static OGRGeometry *readWKT(const std::string &wkt)
{
    OGRGeometry *g;
    auto err = OGRGeometryFactory::createFromWkt(wkt.c_str(), nullptr, &g);

    if (err != OGRERR_NONE)
    {
        throw std::runtime_error("Failed to parse WKT");
    }

    return g;
}

class OrganizePolygonsTest : public testing::TestWithParam<std::string>
{
};

INSTANTIATE_TEST_SUITE_P(
    test_gdal, OrganizePolygonsTest,
    ::testing::Values("DEFAULT", "ONLY_CCW", "SKIP"),
    [](const ::testing::TestParamInfo<std::string> &param_info)
    { return param_info.param; });

TEST_P(OrganizePolygonsTest, EmptyInputVector)
{
    std::vector<OGRGeometry *> polygons;

    const auto &method = GetParam();
    auto result = organizePolygons(polygons, method);

    ASSERT_NE(result, nullptr);
    ASSERT_EQ(result->getGeometryType(), wkbPolygon);
    ASSERT_TRUE(result->IsEmpty());
}

TEST_P(OrganizePolygonsTest, SinglePolygonInput)
{
    std::vector<OGRGeometry *> polygons;
    polygons.push_back(readWKT("POLYGON ((0 0, 1 0, 1 1, 0 0))"));

    std::unique_ptr<OGRGeometry> expected(polygons.front()->clone());

    const auto &method = GetParam();
    auto result = organizePolygons(polygons, method);

    ASSERT_NE(result, nullptr);
    ASSERT_EQ(result->getGeometryType(), wkbPolygon);
    ASSERT_TRUE(result->Equals(expected.get()));
}

TEST_P(OrganizePolygonsTest, SingleCurvePolygonInput)
{
    std::vector<OGRGeometry *> polygons;
    polygons.push_back(readWKT("CURVEPOLYGON ((0 0, 1 0, 1 1, 0 0))"));

    std::unique_ptr<OGRGeometry> expected(polygons.front()->clone());

    const auto &method = GetParam();
    auto result = organizePolygons(polygons, method);

    ASSERT_NE(result, nullptr);
    ASSERT_EQ(result->getGeometryType(), wkbCurvePolygon);
    ASSERT_TRUE(result->Equals(expected.get()));
}

TEST_P(OrganizePolygonsTest, SinglePointInput)
{
    std::vector<OGRGeometry *> polygons;
    polygons.push_back(readWKT("POINT (0 0)"));

    const auto &method = GetParam();
    auto result = organizePolygons(polygons, method);

    ASSERT_NE(result, nullptr);
    ASSERT_EQ(result->getGeometryType(), wkbPolygon);
    ASSERT_TRUE(result->IsEmpty());
}

TEST_P(OrganizePolygonsTest, MixedPolygonCurvePolygonInput)
{
    std::vector<OGRGeometry *> polygons;
    polygons.push_back(
        readWKT("POLYGON ((10 10, 20 10, 20 20, 20 10, 10 10))"));
    polygons.push_back(readWKT("CURVEPOLYGON ((0 0, 1 0, 1 1, 0 0))"));

    const auto &method = GetParam();
    auto result = organizePolygons(polygons, method);

    ASSERT_NE(result, nullptr);
    ASSERT_EQ(result->getGeometryType(), wkbMultiSurface);

    std::unique_ptr<OGRGeometry> expected(
        readWKT("MULTISURFACE ("
                "POLYGON ((10 10, 20 10, 20 20, 20 10, 10 10)),"
                "CURVEPOLYGON ((0 0, 1 0, 1 1, 0 0)))"));

    ASSERT_TRUE(result->Equals(expected.get()));
}

TEST_P(OrganizePolygonsTest, MixedPolygonPointInput)
{
    std::vector<OGRGeometry *> polygons;
    polygons.push_back(readWKT("POLYGON ((0 0, 1 0, 1 1, 0 0))"));
    polygons.push_back(readWKT("POINT (2 2)"));

    std::unique_ptr<OGRGeometry> expected(polygons[0]->clone());

    const auto &method = GetParam();
    auto result = organizePolygons(polygons, method);

    ASSERT_NE(result, nullptr);
    ASSERT_TRUE(result->Equals(expected.get()));
}

TEST_P(OrganizePolygonsTest, CWPolygonCCWHole)
{
    std::vector<OGRGeometry *> polygons;
    polygons.push_back(readWKT("POLYGON ((0 0, 0 10, 10 10, 10 0, 0 0))"));
    polygons.push_back(readWKT("POLYGON ((1 1, 2 1, 2 2, 1 2, 1 1))"));

    const auto &method = GetParam();
    auto result = organizePolygons(polygons, method);

    ASSERT_NE(result, nullptr);

    std::unique_ptr<OGRGeometry> expected;
    if (method == "SKIP")
    {
        expected.reset(readWKT("MULTIPOLYGON (((0 0, 0 10, 10 10, 10 0, 0 0)), "
                               "((1 1, 2 1, 2 2, 1 2, 1 1)))"));
    }
    else
    {
        expected.reset(readWKT("POLYGON ((0 0, 0 10, 10 10, 10 0, 0 0), (1 1, "
                               "2 1, 2 2, 1 2, 1 1))"));
    }

    ASSERT_TRUE(result->Equals(expected.get()));
}

TEST_P(OrganizePolygonsTest, CWPolygonCCWLakeCWIslandInLake)
{
    std::vector<OGRGeometry *> polygons;
    polygons.push_back(
        readWKT("POLYGON ((0 0, 0 100, 100 100, 100 0, 0 0))"));  // CW
    polygons.push_back(
        readWKT("POLYGON ((10 10, 20 10, 20 20, 10 20, 10 10))"));  // CCW
    polygons.push_back(
        readWKT("POLYGON ((15 15, 15 16, 16 16, 16 15, 15 15))"));  // CW

    const auto &method = GetParam();
    auto result = organizePolygons(polygons, method);

    ASSERT_NE(result, nullptr);

    if (method != "SKIP")
    {
        std::unique_ptr<OGRGeometry> expected(
            readWKT("MULTIPOLYGON ("
                    "((0 0, 0 100, 100 100, 100 0, 0 0), (10 10, 20 10, 20 20, "
                    "10 20, 10 10)),"
                    "((15 15, 15 16, 16 16, 16 15, 15 15)))"));
        ASSERT_TRUE(result->Equals(expected.get()));
    }
}

TEST_P(OrganizePolygonsTest, AdjacentCCWPolygons)
{
    std::vector<OGRGeometry *> polygons;
    polygons.push_back(readWKT("POLYGON ((0 0, 1 0, 1 1, 0 1, 0 0))"));  // CCW
    polygons.push_back(readWKT("POLYGON ((1 0, 2 0, 2 1, 1 1, 1 0))"));  // CCW

    const auto &method = GetParam();
    auto result = organizePolygons(polygons, method);

    ASSERT_NE(result, nullptr);

    std::unique_ptr<OGRGeometry> expected(
        readWKT("MULTIPOLYGON("
                "((0 0, 1 0, 1 1, 0 1, 0 0)), "
                "((1 0, 2 0, 2 1, 1 1, 1 0)))"));
    ASSERT_TRUE(result->Equals(expected.get()));
}

TEST_P(OrganizePolygonsTest, HoleAlongEdge)
{
    std::vector<OGRGeometry *> polygons;
    polygons.push_back(
        readWKT("POLYGON ((0 0, 0 10, 10 10, 10 0, 0 0))"));             // CW
    polygons.push_back(readWKT("POLYGON ((0 2, 1 2, 1 3, 0 3, 0 2))"));  // CCW

    const auto &method = GetParam();
    auto result = organizePolygons(polygons, method);

    ASSERT_NE(result, nullptr);

    if (method != "SKIP")
    {
        std::unique_ptr<OGRGeometry> expected(
            readWKT("POLYGON("
                    "(0 0, 0 10, 10 10, 10 0, 0 0), "
                    "(0 2, 1 2, 1 3, 0 3, 0 2))"));
        ASSERT_TRUE(result->Equals(expected.get()));
    }
}

TEST_P(OrganizePolygonsTest, CrossingCCWPolygons)
{
    std::vector<OGRGeometry *> polygons;
    polygons.push_back(readWKT("POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))"));
    polygons.push_back(readWKT("POLYGON ((5 5, 15 5, 15 15, 5 15, 5 5))"));

    const auto &method = GetParam();
    auto result = organizePolygons(polygons, method);

    ASSERT_NE(result, nullptr);

    std::unique_ptr<OGRGeometry> expected(
        readWKT("MULTIPOLYGON("
                "((0 0, 10 0, 10 10, 0 10, 0 0)), "
                "((5 5, 15 5, 15 15, 5 15, 5 5)))"));
    ASSERT_TRUE(result->Equals(expected.get()));
}
