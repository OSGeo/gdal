///////////////////////////////////////////////////////////////////////////////
//
// Project:  C++ Test Suite for GDAL/OGR
// Purpose:  Test ogr_wkb.h
// Author:   Even Rouault <even.rouault at spatialys.com>
//
///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2023, Even Rouault <even.rouault at spatialys.com>
/*
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

#include "ogr_geometry.h"
#include "ogr_wkb.h"

#include "gtest_include.h"

#include <limits>

namespace
{

struct test_ogr_wkb : public ::testing::Test
{
};

class OGRWKBGetEnvelopeFixture
    : public test_ogr_wkb,
      public ::testing::WithParamInterface<std::tuple<
          const char *, double, double, double, double, const char *>>
{
    static constexpr double INF = std::numeric_limits<double>::infinity();

  public:
    static std::vector<
        std::tuple<const char *, double, double, double, double, const char *>>
    GetTupleValues()
    {
        return {
            std::make_tuple("POINT(1 2)", 1, 2, 1, 2, "POINT"),
            std::make_tuple("POINT EMPTY", INF, INF, -INF, -INF, "POINT_EMPTY"),
            std::make_tuple("POINT Z (1 2 3)", 1, 2, 1, 2, "POINT_3D"),
            std::make_tuple("LINESTRING(3 4,1 2)", 1, 2, 3, 4, "LINESTRING"),
            std::make_tuple("LINESTRING EMPTY", INF, INF, -INF, -INF,
                            "LINESTRING_EMPTY"),
            std::make_tuple("LINESTRING Z (3 4 5,1 2 6)", 1, 2, 3, 4,
                            "LINESTRING_3D"),
            std::make_tuple("POLYGON((0 1,0 2,3 2,0 1))", 0, 1, 3, 2,
                            "POLYGON"),
            std::make_tuple("POLYGON EMPTY", INF, INF, -INF, -INF,
                            "POLYGON_EMPTY"),
            std::make_tuple("POLYGON Z ((0 1 10,0 2 20,3 2 20,0 1 10))", 0, 1,
                            3, 2, "POLYGON_3D"),
            std::make_tuple("MULTIPOINT((1 2),(3 4))", 1, 2, 3, 4,
                            "MULTIPOINT"),
            std::make_tuple("MULTIPOINT EMPTY", INF, INF, -INF, -INF,
                            "MULTIPOINT_EMPTY"),
            std::make_tuple("MULTIPOINT Z ((1 2 10),(3 4 20))", 1, 2, 3, 4,
                            "MULTIPOINT_3D"),
            std::make_tuple("MULTILINESTRING((3 4,1 2),(5 6,7 8))", 1, 2, 7, 8,
                            "MULTILINESTRING"),
            std::make_tuple("MULTILINESTRING EMPTY", INF, INF, -INF, -INF,
                            "MULTILINESTRING_EMPTY"),
            std::make_tuple(
                "MULTILINESTRING Z ((3 4 10,1 2 20),(5 6 10,7 8 20))", 1, 2, 7,
                8, "MULTILINESTRING_3D"),
            std::make_tuple(
                "MULTIPOLYGON(((0 1,0 2,3 2,0 1)),((0 -1,0 -2,-3 -2,0 -1)))",
                -3, -2, 3, 2, "MULTIPOLYGON"),
            std::make_tuple("MULTIPOLYGON EMPTY", INF, INF, -INF, -INF,
                            "MULTIPOLYGON_EMPTY"),
            std::make_tuple("MULTIPOLYGON Z (((0 1 10,0 2 20,3 2 20,0 1 "
                            "10)),((0 -1 -10,0 -2 -20,-3 -2 -20,0 -1 -10)))",
                            -3, -2, 3, 2, "MULTIPOLYGON_3D"),
            std::make_tuple("GEOMETRYCOLLECTION(POINT(1 2),POINT(3 4))", 1, 2,
                            3, 4, "GEOMETRYCOLLECTION"),
            std::make_tuple("CIRCULARSTRING(0 10,1 11,2 10)", 0, 10, 2, 11,
                            "CIRCULARSTRING"),
            std::make_tuple("COMPOUNDCURVE((3 4,1 2))", 1, 2, 3, 4,
                            "COMPOUNDCURVE"),
            std::make_tuple("CURVEPOLYGON((0 1,0 2,3 2,0 1))", 0, 1, 3, 2,
                            "CURVEPOLYGON"),
            std::make_tuple("MULTICURVE((3 4,1 2),(5 6,7 8))", 1, 2, 7, 8,
                            "MULTICURVE"),
            std::make_tuple(
                "MULTISURFACE(((0 1,0 2,3 2,0 1)),((0 -1,0 -2,-3 -2,0 -1)))",
                -3, -2, 3, 2, "MULTISURFACE"),
            std::make_tuple("TRIANGLE((0 1,0 2,3 2,0 1))", 0, 1, 3, 2,
                            "TRIANGLE"),
            std::make_tuple("POLYHEDRALSURFACE(((0 1,0 2,3 2,0 1)))", 0, 1, 3,
                            2, "POLYHEDRALSURFACE"),
            std::make_tuple("TIN(((0 1,0 2,3 2,0 1)))", 0, 1, 3, 2, "TIN"),
        };
    }
};

TEST_P(OGRWKBGetEnvelopeFixture, test)
{
    const char *pszInput = std::get<0>(GetParam());
    const double dfExpectedMinX = std::get<1>(GetParam());
    const double dfExpectedMinY = std::get<2>(GetParam());
    const double dfExpectedMaxX = std::get<3>(GetParam());
    const double dfExpectedMaxY = std::get<4>(GetParam());

    OGRGeometry *poGeom = nullptr;
    OGRGeometryFactory::createFromWkt(pszInput, nullptr, &poGeom);
    ASSERT_TRUE(poGeom != nullptr);
    std::vector<GByte> abyWkb(poGeom->WkbSize());
    poGeom->exportToWkb(wkbNDR, abyWkb.data(), wkbVariantIso);
    delete poGeom;
    OGREnvelope sEnvelope;
    OGRWKBGetBoundingBox(abyWkb.data(), abyWkb.size(), sEnvelope);
    EXPECT_EQ(sEnvelope.MinX, dfExpectedMinX);
    EXPECT_EQ(sEnvelope.MinY, dfExpectedMinY);
    EXPECT_EQ(sEnvelope.MaxX, dfExpectedMaxX);
    EXPECT_EQ(sEnvelope.MaxY, dfExpectedMaxY);
}

INSTANTIATE_TEST_SUITE_P(
    test_ogr_wkb, OGRWKBGetEnvelopeFixture,
    ::testing::ValuesIn(OGRWKBGetEnvelopeFixture::GetTupleValues()),
    [](const ::testing::TestParamInfo<OGRWKBGetEnvelopeFixture::ParamType>
           &l_info) { return std::get<5>(l_info.param); });

class OGRWKBGetEnvelope3DFixture
    : public test_ogr_wkb,
      public ::testing::WithParamInterface<
          std::tuple<const char *, double, double, double, double, double,
                     double, const char *>>
{
    static constexpr double INF = std::numeric_limits<double>::infinity();

  public:
    static std::vector<std::tuple<const char *, double, double, double, double,
                                  double, double, const char *>>
    GetTupleValues()
    {
        return {
            std::make_tuple("POINT(1 2)", 1, 2, INF, 1, 2, -INF, "POINT"),
            std::make_tuple("POINT EMPTY", INF, INF, INF, -INF, -INF, -INF,
                            "POINT_EMPTY"),
            std::make_tuple("POINT Z (1 2 3)", 1, 2, 3, 1, 2, 3, "POINT_3D"),
            std::make_tuple("LINESTRING(3 4,1 2)", 1, 2, INF, 3, 4, -INF,
                            "LINESTRING"),
            std::make_tuple("LINESTRING EMPTY", INF, INF, INF, -INF, -INF, -INF,
                            "LINESTRING_EMPTY"),
            std::make_tuple("LINESTRING Z (3 4 5,1 2 6)", 1, 2, 5, 3, 4, 6,
                            "LINESTRING_3D"),
            std::make_tuple("POLYGON((0 1,0 2,3 2,0 1))", 0, 1, INF, 3, 2, -INF,
                            "POLYGON"),
            std::make_tuple("POLYGON EMPTY", INF, INF, INF, -INF, -INF, -INF,
                            "POLYGON_EMPTY"),
            std::make_tuple("POLYGON Z ((0 1 10,0 2 20,3 2 20,0 1 10))", 0, 1,
                            10, 3, 2, 20, "POLYGON_3D"),
            std::make_tuple("MULTIPOINT((1 2),(3 4))", 1, 2, INF, 3, 4, -INF,
                            "MULTIPOINT"),
            std::make_tuple("MULTIPOINT EMPTY", INF, INF, INF, -INF, -INF, -INF,
                            "MULTIPOINT_EMPTY"),
            std::make_tuple("MULTIPOINT Z ((1 2 10),(3 4 20))", 1, 2, 10, 3, 4,
                            20, "MULTIPOINT_3D"),
            std::make_tuple("MULTILINESTRING((3 4,1 2),(5 6,7 8))", 1, 2, INF,
                            7, 8, -INF, "MULTILINESTRING"),
            std::make_tuple("MULTILINESTRING EMPTY", INF, INF, INF, -INF, -INF,
                            -INF, "MULTILINESTRING_EMPTY"),
            std::make_tuple(
                "MULTILINESTRING Z ((3 4 10,1 2 20),(5 6 10,7 8 20))", 1, 2, 10,
                7, 8, 20, "MULTILINESTRING_3D"),
            std::make_tuple(
                "MULTIPOLYGON(((0 1,0 2,3 2,0 1)),((0 -1,0 -2,-3 -2,0 -1)))",
                -3, -2, INF, 3, 2, -INF, "MULTIPOLYGON"),
            std::make_tuple("MULTIPOLYGON EMPTY", INF, INF, INF, -INF, -INF,
                            -INF, "MULTIPOLYGON_EMPTY"),
            std::make_tuple("MULTIPOLYGON Z (((0 1 10,0 2 20,3 2 20,0 1 "
                            "10)),((0 -1 -10,0 -2 -20,-3 -2 -20,0 -1 -10)))",
                            -3, -2, -20, 3, 2, 20, "MULTIPOLYGON_3D"),
        };
    }
};

TEST_P(OGRWKBGetEnvelope3DFixture, test)
{
    const char *pszInput = std::get<0>(GetParam());
    const double dfExpectedMinX = std::get<1>(GetParam());
    const double dfExpectedMinY = std::get<2>(GetParam());
    const double dfExpectedMinZ = std::get<3>(GetParam());
    const double dfExpectedMaxX = std::get<4>(GetParam());
    const double dfExpectedMaxY = std::get<5>(GetParam());
    const double dfExpectedMaxZ = std::get<6>(GetParam());

    OGRGeometry *poGeom = nullptr;
    OGRGeometryFactory::createFromWkt(pszInput, nullptr, &poGeom);
    ASSERT_TRUE(poGeom != nullptr);
    std::vector<GByte> abyWkb(poGeom->WkbSize());
    poGeom->exportToWkb(wkbNDR, abyWkb.data(), wkbVariantIso);
    delete poGeom;
    OGREnvelope3D sEnvelope;
    OGRWKBGetBoundingBox(abyWkb.data(), abyWkb.size(), sEnvelope);
    EXPECT_EQ(sEnvelope.MinX, dfExpectedMinX);
    EXPECT_EQ(sEnvelope.MinY, dfExpectedMinY);
    EXPECT_EQ(sEnvelope.MinZ, dfExpectedMinZ);
    EXPECT_EQ(sEnvelope.MaxX, dfExpectedMaxX);
    EXPECT_EQ(sEnvelope.MaxY, dfExpectedMaxY);
    EXPECT_EQ(sEnvelope.MaxZ, dfExpectedMaxZ);
}

INSTANTIATE_TEST_SUITE_P(
    test_ogr_wkb, OGRWKBGetEnvelope3DFixture,
    ::testing::ValuesIn(OGRWKBGetEnvelope3DFixture::GetTupleValues()),
    [](const ::testing::TestParamInfo<OGRWKBGetEnvelope3DFixture::ParamType>
           &l_info) { return std::get<7>(l_info.param); });

class OGRWKBFixupCounterClockWiseExternalRingFixture
    : public test_ogr_wkb,
      public ::testing::WithParamInterface<
          std::tuple<const char *, const char *, const char *>>
{
  public:
    static std::vector<std::tuple<const char *, const char *, const char *>>
    GetTupleValues()
    {
        return {
            std::make_tuple("MULTIPOLYGON (((0 1,0 0,1 1,0 1),(0.2 0.3,0.2 "
                            "0.8,0.7 0.8,0.2 0.3)))",
                            "MULTIPOLYGON (((0 1,0 0,1 1,0 1),(0.2 0.3,0.2 "
                            "0.8,0.7 0.8,0.2 0.3)))",
                            "MULTIPOLYGON_CCW"),
            std::make_tuple("MULTIPOLYGON (((1 1,0 0,0 1,1 1),(0.2 0.3,0.7 "
                            "0.8,0.2 0.8,0.2 0.3)))",
                            "MULTIPOLYGON (((1 1,0 1,0 0,1 1),(0.2 0.3,0.2 "
                            "0.8,0.7 0.8,0.2 0.3)))",
                            "MULTIPOLYGON_CW"),
            std::make_tuple(
                "MULTIPOLYGON Z (((0 0 10,0 1 10,1 1 10,0 0 10),(0.2 0.3 "
                "10,0.7 0.8 10,0.2 0.8 10,0.2 0.3 10)))",
                "MULTIPOLYGON Z (((0 0 10,1 1 10,0 1 10,0 0 10),(0.2 0.3 "
                "10,0.2 0.8 10,0.7 0.8 10,0.2 0.3 10)))",
                "MULTIPOLYGON_CW_3D"),
            std::make_tuple("MULTIPOLYGON (((0 0,0 0,1 1,1 1,0 1,0 1,0 0)))",
                            "MULTIPOLYGON (((0 0,0 0,1 1,1 1,0 1,0 1,0 0)))",
                            "MULTIPOLYGON_CCW_REPEATED_POINTS"),
            std::make_tuple("MULTIPOLYGON (((0 0,0 0,0 1,0 1,1 1,1 1,0 0)))",
                            "MULTIPOLYGON (((0 0,1 1,1 1,0 1,0 1,0 0,0 0)))",
                            "MULTIPOLYGON_CW_REPEATED_POINTS"),
            std::make_tuple("MULTIPOLYGON EMPTY", "MULTIPOLYGON EMPTY",
                            "MULTIPOLYGON_EMPTY"),
            std::make_tuple("POINT (1 2)", "POINT (1 2)", "POINT"),
        };
    }
};

TEST_P(OGRWKBFixupCounterClockWiseExternalRingFixture, test)
{
    const char *pszInput = std::get<0>(GetParam());
    const char *pszExpected = std::get<1>(GetParam());

    OGRGeometry *poGeom = nullptr;
    OGRGeometryFactory::createFromWkt(pszInput, nullptr, &poGeom);
    ASSERT_TRUE(poGeom != nullptr);
    std::vector<GByte> abyWkb(poGeom->WkbSize());
    poGeom->exportToWkb(wkbNDR, abyWkb.data());
    OGRWKBFixupCounterClockWiseExternalRing(abyWkb.data(), abyWkb.size());
    delete poGeom;
    poGeom = nullptr;
    OGRGeometryFactory::createFromWkb(abyWkb.data(), nullptr, &poGeom);
    ASSERT_TRUE(poGeom != nullptr);
    char *pszWKT = nullptr;
    poGeom->exportToWkt(&pszWKT, wkbVariantIso);
    EXPECT_STREQ(pszWKT, pszExpected);
    CPLFree(pszWKT);
    delete poGeom;
}

INSTANTIATE_TEST_SUITE_P(
    test_ogr_wkb, OGRWKBFixupCounterClockWiseExternalRingFixture,
    ::testing::ValuesIn(
        OGRWKBFixupCounterClockWiseExternalRingFixture::GetTupleValues()),
    [](const ::testing::TestParamInfo<
        OGRWKBFixupCounterClockWiseExternalRingFixture::ParamType> &l_info)
    { return std::get<2>(l_info.param); });

}  // namespace
