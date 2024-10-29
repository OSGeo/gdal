///////////////////////////////////////////////////////////////////////////////
//
// Project:  C++ Test Suite for GDAL/OGR
// Purpose:  Test ogr_wkb.h
// Author:   Even Rouault <even.rouault at spatialys.com>
//
///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2023, Even Rouault <even.rouault at spatialys.com>
/*
 * SPDX-License-Identifier: MIT
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
    ASSERT_EQ(OGRGeometryFactory::createFromWkt(pszInput, nullptr, &poGeom),
              OGRERR_NONE);
    ASSERT_TRUE(poGeom != nullptr);
    std::vector<GByte> abyWkb(poGeom->WkbSize());
    poGeom->exportToWkb(wkbNDR, abyWkb.data(), wkbVariantIso);
    delete poGeom;
    OGREnvelope sEnvelope;
    EXPECT_EQ(OGRWKBGetBoundingBox(abyWkb.data(), abyWkb.size(), sEnvelope),
              true);
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
    ASSERT_EQ(OGRGeometryFactory::createFromWkt(pszInput, nullptr, &poGeom),
              OGRERR_NONE);
    ASSERT_TRUE(poGeom != nullptr);
    std::vector<GByte> abyWkb(poGeom->WkbSize());
    poGeom->exportToWkb(wkbNDR, abyWkb.data(), wkbVariantIso);
    delete poGeom;
    OGREnvelope3D sEnvelope;
    EXPECT_EQ(OGRWKBGetBoundingBox(abyWkb.data(), abyWkb.size(), sEnvelope),
              true);
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
    ASSERT_EQ(OGRGeometryFactory::createFromWkt(pszInput, nullptr, &poGeom),
              OGRERR_NONE);
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

class OGRWKBIntersectsPessimisticFixture
    : public test_ogr_wkb,
      public ::testing::WithParamInterface<std::tuple<
          const char *, double, double, double, double, bool, const char *>>
{
  public:
    static std::vector<std::tuple<const char *, double, double, double, double,
                                  bool, const char *>>
    GetTupleValues()
    {
        return {
            std::make_tuple("POINT(1 2)", 0.9, 1.9, 1.1, 2.1, true, "POINT_IN"),
            std::make_tuple("POINT(1 2)", 1.05, 1.9, 1.1, 2.1, false,
                            "POINT_OUT1"),
            std::make_tuple("POINT(1 2)", 0.9, 2.05, 1.1, 2.1, false,
                            "POINT_OUT2"),
            std::make_tuple("POINT(1 2)", 0.9, 1.9, 0.95, 2.1, false,
                            "POINT_OUT3"),
            std::make_tuple("POINT(1 2)", 0.9, 1.9, 1.1, 1.95, false,
                            "POINT_OUT4"),
            std::make_tuple("POINT Z (1 2 3)", 0.9, 1.9, 1.1, 2.1, true,
                            "POINTZ_IN"),
            std::make_tuple("POINT Z (1 2 3)", 1.05, 1.9, 1.1, 2.1, false,
                            "POINTZ_OUT"),
            std::make_tuple("POINT EMPTY", 0.9, 1.9, 1.1, 2.1, false,
                            "POINT_EMPTY"),
            std::make_tuple("LINESTRING(1 2, 3 4)", 0.9, 1.9, 1.1, 2.1, true,
                            "LINESTRING_IN"),
            std::make_tuple("LINESTRING(1 2, 3 4)", 0.9, 1.9, 0.95, 2.1, false,
                            "LINESTRING_OUT"),
            std::make_tuple("LINESTRING EMPTY", 0.9, 1.9, 1.1, 2.1, false,
                            "LINESTRING_EMPTY"),
            std::make_tuple("LINESTRING Z (1 2 10, 3 4 10)", 0.9, 1.9, 1.1, 2.1,
                            true, "LINESTRINGZ_IN"),
            std::make_tuple("LINESTRING Z (1 2 10, 3 4 10)", 0.9, 1.9, 0.95,
                            2.1, false, "LINESTRINGZ_OUT"),
            std::make_tuple("POLYGON((1 2,1 3,10 3,1 2))", 0.9, 1.9, 1.1, 2.1,
                            true, "POLYGON_IN"),
            std::make_tuple("POLYGON((1 2,1 3,10 3,1 2))", 0.9, 1.9, 0.95, 2.1,
                            false, "POLYGON_OUT"),
            std::make_tuple("POLYGON EMPTY", 0.9, 1.9, 1.1, 2.1, false,
                            "POLYGON_EMPTY"),
            std::make_tuple("POLYGON Z ((1 2 -10,1 3 -10,10 3 -10,1 2 -10))",
                            0.9, 1.9, 1.1, 2.1, true, "POLYGONZ_IN"),
            std::make_tuple("POLYGON Z ((1 2 -10,1 3 -10,10 3 -10,1 2 -10))",
                            0.9, 1.9, 0.95, 2.1, false, "POLYGONZ_OUT"),
            std::make_tuple("MULTIPOINT((1 2))", 0.9, 1.9, 1.1, 2.1, true,
                            "MULTIPOINT_IN"),
            std::make_tuple("MULTIPOINT((1 2))", 1.05, 1.9, 1.1, 2.1, false,
                            "MULTIPOINT_OUT1"),
            std::make_tuple("MULTIPOINT((1 2))", 0.9, 2.05, 1.1, 2.1, false,
                            "MULTIPOINT_OUT2"),
            std::make_tuple("MULTIPOINT((1 2))", 0.9, 1.9, 0.95, 2.1, false,
                            "MULTIPOINT_OUT3"),
            std::make_tuple("MULTIPOINT((1 2))", 0.9, 1.9, 1.1, 1.95, false,
                            "MULTIPOINT_OUT4"),
            std::make_tuple("MULTIPOINT Z ((1 2 3))", 0.9, 1.9, 1.1, 2.1, true,
                            "MULTIPOINTZ_IN"),
            std::make_tuple("MULTIPOINT Z ((1 2 3))", 1.05, 1.9, 1.1, 2.1,
                            false, "MULTIPOINTZ_OUT"),
            std::make_tuple("MULTIPOINT EMPTY", 0.9, 1.9, 1.1, 2.1, false,
                            "MULTIPOINT_EMPTY"),
            std::make_tuple("MULTILINESTRING((1 2, 3 4))", 0.9, 1.9, 1.1, 2.1,
                            true, "MULTILINESTRING_IN"),
            std::make_tuple("MULTILINESTRING((1 2, 3 4))", 0.9, 1.9, 0.95, 2.1,
                            false, "MULTILINESTRING_OUT"),
            std::make_tuple("MULTILINESTRING EMPTY", 0.9, 1.9, 1.1, 2.1, false,
                            "MULTILINESTRING_EMPTY"),
            std::make_tuple("MULTILINESTRING Z ((1 2 10, 3 4 10))", 0.9, 1.9,
                            1.1, 2.1, true, "MULTILINESTRINGZ_IN"),
            std::make_tuple("MULTILINESTRING Z ((1 2 10, 3 4 10))", 0.9, 1.9,
                            0.95, 2.1, false, "MULTILINESTRINGZ_OUT"),
            std::make_tuple("MULTIPOLYGON(((1 2,1 3,10 3,1 2)))", 0.9, 1.9, 1.1,
                            2.1, true, "MULTIPOLYGON_IN"),
            std::make_tuple("MULTIPOLYGON(((1 2,1 3,10 3,1 2)))", 0.9, 1.9,
                            0.95, 2.1, false, "MULTIPOLYGON_OUT"),
            std::make_tuple("MULTIPOLYGON EMPTY", 0.9, 1.9, 1.1, 2.1, false,
                            "MULTIPOLYGON_EMPTY"),
            std::make_tuple(
                "MULTIPOLYGON Z (((1 2 -10,1 3 -10,10 3 -10,1 2 -10)))", 0.9,
                1.9, 1.1, 2.1, true, "MULTIPOLYGONZ_IN"),
            std::make_tuple(
                "MULTIPOLYGON Z (((1 2 -10,1 3 -10,10 3 -10,1 2 -10)))", 0.9,
                1.9, 0.95, 2.1, false, "MULTIPOLYGONZ_OUT"),
            std::make_tuple("GEOMETRYCOLLECTION(POINT(1 2))", 0.9, 1.9, 1.1,
                            2.1, true, "GEOMETRYCOLLECTION_POINT_IN"),
            std::make_tuple("CIRCULARSTRING(0 10,1 11,2 10)", -0.1, 9.9, 0.1,
                            10.1, true, "CIRCULARSTRING_IN"),
            std::make_tuple("CIRCULARSTRING(0 10,1 11,2 10)", -0.1, 9.9, -0.05,
                            10.1, false, "CIRCULARSTRING_OUT"),
            std::make_tuple("CIRCULARSTRING EMPTY", -0.1, 9.9, 0.1, 10.1, false,
                            "CIRCULARSTRING_EMPTY"),
            std::make_tuple("TRIANGLE((1 2,1 3,10 3,1 2))", 0.9, 1.9, 1.1, 2.1,
                            true, "TRIANGLE_IN"),
            std::make_tuple("TRIANGLE((1 2,1 3,10 3,1 2))", 0.9, 1.9, 0.95, 2.1,
                            false, "TRIANGLE_OUT"),
            std::make_tuple("TRIANGLE EMPTY", 0.9, 1.9, 1.1, 2.1, false,
                            "TRIANGLE_EMPTY"),
            std::make_tuple("TRIANGLE Z ((1 2 -10,1 3 -10,10 3 -10,1 2 -10))",
                            0.9, 1.9, 1.1, 2.1, true, "TRIANGLEZ_IN"),
            std::make_tuple("TRIANGLE Z ((1 2 -10,1 3 -10,10 3 -10,1 2 -10))",
                            0.9, 1.9, 0.95, 2.1, false, "TRIANGLEZ_OUT"),
            std::make_tuple("COMPOUNDCURVE((1 2, 3 4))", 0.9, 1.9, 1.1, 2.1,
                            true, "COMPOUNDCURVE_IN"),
            std::make_tuple("COMPOUNDCURVE((1 2, 3 4))", 0.9, 1.9, 0.95, 2.1,
                            false, "COMPOUNDCURVE_OUT"),
            std::make_tuple("COMPOUNDCURVE EMPTY", 0.9, 1.9, 1.1, 2.1, false,
                            "COMPOUNDCURVE_EMPTY"),
            std::make_tuple("COMPOUNDCURVE Z ((1 2 10, 3 4 10))", 0.9, 1.9, 1.1,
                            2.1, true, "COMPOUNDCURVEZ_IN"),
            std::make_tuple("COMPOUNDCURVE Z ((1 2 10, 3 4 10))", 0.9, 1.9,
                            0.95, 2.1, false, "COMPOUNDCURVEZ_OUT"),
            std::make_tuple("CURVEPOLYGON((1 2,1 3,10 3,1 2))", 0.9, 1.9, 1.1,
                            2.1, true, "CURVEPOLYGON_IN"),
            std::make_tuple("CURVEPOLYGON((1 2,1 3,10 3,1 2))", 0.9, 1.9, 0.95,
                            2.1, false, "CURVEPOLYGON_OUT"),
            std::make_tuple("CURVEPOLYGON EMPTY", 0.9, 1.9, 1.1, 2.1, false,
                            "CURVEPOLYGON_EMPTY"),
            std::make_tuple(
                "CURVEPOLYGON Z ((1 2 -10,1 3 -10,10 3 -10,1 2 -10))", 0.9, 1.9,
                1.1, 2.1, true, "CURVEPOLYGONZ_IN"),
            std::make_tuple(
                "CURVEPOLYGON Z ((1 2 -10,1 3 -10,10 3 -10,1 2 -10))", 0.9, 1.9,
                0.95, 2.1, false, "CURVEPOLYGONZ_OUT"),
            std::make_tuple("MULTICURVE((1 2, 3 4))", 0.9, 1.9, 1.1, 2.1, true,
                            "MULTICURVE_IN"),
            std::make_tuple("MULTICURVE((1 2, 3 4))", 0.9, 1.9, 0.95, 2.1,
                            false, "MULTICURVE_OUT"),
            std::make_tuple("MULTICURVE EMPTY", 0.9, 1.9, 1.1, 2.1, false,
                            "MULTICURVE_EMPTY"),
            std::make_tuple("MULTICURVE Z ((1 2 10, 3 4 10))", 0.9, 1.9, 1.1,
                            2.1, true, "MULTICURVEZ_IN"),
            std::make_tuple("MULTICURVE Z ((1 2 10, 3 4 10))", 0.9, 1.9, 0.95,
                            2.1, false, "MULTICURVEZ_OUT"),
            std::make_tuple("MULTISURFACE(((1 2,1 3,10 3,1 2)))", 0.9, 1.9, 1.1,
                            2.1, true, "MULTISURFACE_IN"),
            std::make_tuple("MULTISURFACE(((1 2,1 3,10 3,1 2)))", 0.9, 1.9,
                            0.95, 2.1, false, "MULTISURFACE_OUT"),
            std::make_tuple("MULTISURFACE EMPTY", 0.9, 1.9, 1.1, 2.1, false,
                            "MULTISURFACE_EMPTY"),
            std::make_tuple(
                "MULTISURFACE Z (((1 2 -10,1 3 -10,10 3 -10,1 2 -10)))", 0.9,
                1.9, 1.1, 2.1, true, "MULTISURFACEZ_IN"),
            std::make_tuple(
                "MULTISURFACE Z (((1 2 -10,1 3 -10,10 3 -10,1 2 -10)))", 0.9,
                1.9, 0.95, 2.1, false, "MULTISURFACEZ_OUT"),
            std::make_tuple("POLYHEDRALSURFACE(((1 2,1 3,10 3,1 2)))", 0.9, 1.9,
                            1.1, 2.1, true, "POLYHEDRALSURFACE_IN"),
            std::make_tuple("POLYHEDRALSURFACE(((1 2,1 3,10 3,1 2)))", 0.9, 1.9,
                            0.95, 2.1, false, "POLYHEDRALSURFACE_OUT"),
            std::make_tuple("POLYHEDRALSURFACE EMPTY", 0.9, 1.9, 1.1, 2.1,
                            false, "POLYHEDRALSURFACE_EMPTY"),
            std::make_tuple(
                "POLYHEDRALSURFACE Z (((1 2 -10,1 3 -10,10 3 -10,1 2 -10)))",
                0.9, 1.9, 1.1, 2.1, true, "POLYHEDRALSURFACEZ_IN"),
            std::make_tuple(
                "POLYHEDRALSURFACE Z (((1 2 -10,1 3 -10,10 3 -10,1 2 -10)))",
                0.9, 1.9, 0.95, 2.1, false, "POLYHEDRALSURFACEZ_OUT"),
            std::make_tuple("TIN(((1 2,1 3,10 3,1 2)))", 0.9, 1.9, 1.1, 2.1,
                            true, "TIN_IN"),
            std::make_tuple("TIN(((1 2,1 3,10 3,1 2)))", 0.9, 1.9, 0.95, 2.1,
                            false, "TIN_OUT"),
            std::make_tuple("TIN EMPTY", 0.9, 1.9, 1.1, 2.1, false,
                            "TIN_EMPTY"),
            std::make_tuple("TIN Z (((1 2 -10,1 3 -10,10 3 -10,1 2 -10)))", 0.9,
                            1.9, 1.1, 2.1, true, "TINZ_IN"),
            std::make_tuple("TIN Z (((1 2 -10,1 3 -10,10 3 -10,1 2 -10)))", 0.9,
                            1.9, 0.95, 2.1, false, "TINZ_OUT"),
        };
    }
};

TEST_P(OGRWKBIntersectsPessimisticFixture, test)
{
    const char *pszInput = std::get<0>(GetParam());
    const double dfMinX = std::get<1>(GetParam());
    const double dfMinY = std::get<2>(GetParam());
    const double dfMaxX = std::get<3>(GetParam());
    const double dfMaxY = std::get<4>(GetParam());
    const bool bIntersects = std::get<5>(GetParam());

    OGRGeometry *poGeom = nullptr;
    EXPECT_EQ(OGRGeometryFactory::createFromWkt(pszInput, nullptr, &poGeom),
              OGRERR_NONE);
    ASSERT_TRUE(poGeom != nullptr);
    std::vector<GByte> abyWkb(poGeom->WkbSize());
    poGeom->exportToWkb(wkbNDR, abyWkb.data(), wkbVariantIso);
    delete poGeom;
    OGREnvelope sEnvelope;
    sEnvelope.MinX = dfMinX;
    sEnvelope.MinY = dfMinY;
    sEnvelope.MaxX = dfMaxX;
    sEnvelope.MaxY = dfMaxY;
    EXPECT_EQ(
        OGRWKBIntersectsPessimistic(abyWkb.data(), abyWkb.size(), sEnvelope),
        bIntersects);

    if (abyWkb.size() > 9)
    {
        EXPECT_EQ(OGRWKBIntersectsPessimistic(abyWkb.data(), 9, sEnvelope),
                  false);

        if (!STARTS_WITH(pszInput, "POINT"))
        {
            // Corrupt number of sub-geometries
            abyWkb[5] = 0xff;
            abyWkb[6] = 0xff;
            abyWkb[7] = 0xff;
            abyWkb[8] = 0xff;
            EXPECT_EQ(OGRWKBIntersectsPessimistic(abyWkb.data(), abyWkb.size(),
                                                  sEnvelope),
                      false);
        }
    }
}

INSTANTIATE_TEST_SUITE_P(
    test_ogr_wkb, OGRWKBIntersectsPessimisticFixture,
    ::testing::ValuesIn(OGRWKBIntersectsPessimisticFixture::GetTupleValues()),
    [](const ::testing::TestParamInfo<
        OGRWKBIntersectsPessimisticFixture::ParamType> &l_info)
    { return std::get<6>(l_info.param); });

class OGRWKBTransformFixture
    : public test_ogr_wkb,
      public ::testing::WithParamInterface<
          std::tuple<const char *, OGRwkbByteOrder, const char *, const char *>>
{
  public:
    static std::vector<
        std::tuple<const char *, OGRwkbByteOrder, const char *, const char *>>
    GetTupleValues()
    {
        return {
            std::make_tuple("POINT EMPTY", wkbNDR, "POINT EMPTY",
                            "POINT_EMPTY_NDR"),
            std::make_tuple("POINT EMPTY", wkbXDR, "POINT EMPTY",
                            "POINT_EMPTY_XDR"),
            std::make_tuple("POINT (1 2)", wkbNDR, "POINT (2 4)", "POINT_NDR"),
            std::make_tuple("POINT (1 2)", wkbXDR, "POINT (2 4)", "POINT_XDR"),
            std::make_tuple("POINT Z EMPTY", wkbNDR, "POINT Z EMPTY",
                            "POINT_Z_EMPTY_NDR"),
            std::make_tuple("POINT Z EMPTY", wkbXDR, "POINT Z EMPTY",
                            "POINT_Z_EMPTY_XDR"),
            std::make_tuple("POINT Z (1 2 3)", wkbNDR, "POINT Z (2 4 6)",
                            "POINT_Z_NDR"),
            std::make_tuple("POINT Z (1 2 3)", wkbXDR, "POINT Z (2 4 6)",
                            "POINT_Z_XDR"),
            std::make_tuple("POINT M EMPTY", wkbNDR, "POINT M EMPTY",
                            "POINT_M_EMPTY_NDR"),
            std::make_tuple("POINT M EMPTY", wkbXDR, "POINT M EMPTY",
                            "POINT_M_EMPTY_XDR"),
            std::make_tuple("POINT M (1 2 -10)", wkbNDR, "POINT M (2 4 -10)",
                            "POINT_M_NDR"),
            std::make_tuple("POINT M (1 2 -10)", wkbXDR, "POINT M (2 4 -10)",
                            "POINT_M_XDR"),
            std::make_tuple("POINT ZM EMPTY", wkbNDR, "POINT ZM EMPTY",
                            "POINT_ZM_EMPTY_NDR"),
            std::make_tuple("POINT ZM EMPTY", wkbXDR, "POINT ZM EMPTY",
                            "POINT_ZM_EMPTY_XDR"),
            std::make_tuple("POINT ZM (1 2 3 10)", wkbNDR,
                            "POINT ZM (2 4 6 10)", "POINT_ZM_NDR"),
            std::make_tuple("POINT ZM (1 2 3 10)", wkbXDR,
                            "POINT ZM (2 4 6 10)", "POINT_ZM_XDR"),

            std::make_tuple("LINESTRING EMPTY", wkbNDR, "LINESTRING EMPTY",
                            "LINESTRING_EMPTY"),
            std::make_tuple("LINESTRING (1 2,11 12)", wkbNDR,
                            "LINESTRING (2 4,12 14)", "LINESTRING_NDR"),
            std::make_tuple("LINESTRING (1 2,11 12)", wkbXDR,
                            "LINESTRING (2 4,12 14)", "LINESTRING_XDR"),
            std::make_tuple("LINESTRING Z EMPTY", wkbNDR, "LINESTRING Z EMPTY",
                            "LINESTRING_Z_EMPTY"),
            std::make_tuple("LINESTRING Z (1 2 3,11 12 13)", wkbNDR,
                            "LINESTRING Z (2 4 6,12 14 16)",
                            "LINESTRING_Z_NDR"),
            std::make_tuple("LINESTRING Z (1 2 3,11 12 13)", wkbXDR,
                            "LINESTRING Z (2 4 6,12 14 16)",
                            "LINESTRING_Z_XDR"),
            std::make_tuple("LINESTRING M EMPTY", wkbNDR, "LINESTRING M EMPTY",
                            "LINESTRING_M_EMPTY"),
            std::make_tuple("LINESTRING M (1 2 -10,11 12 -20)", wkbNDR,
                            "LINESTRING M (2 4 -10,12 14 -20)",
                            "LINESTRING_M_NDR"),
            std::make_tuple("LINESTRING M (1 2 -10,11 12 -20)", wkbXDR,
                            "LINESTRING M (2 4 -10,12 14 -20)",
                            "LINESTRING_M_XDR"),
            std::make_tuple("LINESTRING ZM EMPTY", wkbNDR,
                            "LINESTRING ZM EMPTY", "LINESTRING_ZM_EMPTY"),
            std::make_tuple("LINESTRING ZM (1 2 3 -10,11 12 13 -20)", wkbNDR,
                            "LINESTRING ZM (2 4 6 -10,12 14 16 -20)",
                            "LINESTRING_ZM_NDR"),
            std::make_tuple("LINESTRING ZM (1 2 3 -10,11 12 13 -20)", wkbXDR,
                            "LINESTRING ZM (2 4 6 -10,12 14 16 -20)",
                            "LINESTRING_ZM_XDR"),

            // I know the polygon is invalid, but this is enough for our purposes
            std::make_tuple("POLYGON EMPTY", wkbNDR, "POLYGON EMPTY",
                            "POLYGON_EMPTY"),
            std::make_tuple("POLYGON ((1 2,11 12))", wkbNDR,
                            "POLYGON ((2 4,12 14))", "POLYGON_NDR"),
            std::make_tuple("POLYGON ((1 2,11 12))", wkbXDR,
                            "POLYGON ((2 4,12 14))", "POLYGON_XDR"),
            std::make_tuple("POLYGON ((1 2,11 12),(21 22,31 32))", wkbNDR,
                            "POLYGON ((2 4,12 14),(22 24,32 34))",
                            "POLYGON_TWO_RINGS"),
            std::make_tuple("POLYGON Z EMPTY", wkbNDR, "POLYGON Z EMPTY",
                            "POLYGON_Z_EMPTY"),
            std::make_tuple("POLYGON Z ((1 2 3,11 12 13))", wkbNDR,
                            "POLYGON Z ((2 4 6,12 14 16))", "POLYGON_Z_NDR"),
            std::make_tuple("POLYGON Z ((1 2 3,11 12 13))", wkbXDR,
                            "POLYGON Z ((2 4 6,12 14 16))", "POLYGON_Z_XDR"),
            std::make_tuple("POLYGON M EMPTY", wkbNDR, "POLYGON M EMPTY",
                            "POLYGON_M_EMPTY"),
            std::make_tuple("POLYGON M ((1 2 -10,11 12 -20))", wkbNDR,
                            "POLYGON M ((2 4 -10,12 14 -20))", "POLYGON_M_NDR"),
            std::make_tuple("POLYGON M ((1 2 -10,11 12 -20))", wkbXDR,
                            "POLYGON M ((2 4 -10,12 14 -20))", "POLYGON_M_XDR"),
            std::make_tuple("POLYGON ZM EMPTY", wkbNDR, "POLYGON ZM EMPTY",
                            "POLYGON_ZM_EMPTY"),
            std::make_tuple("POLYGON ZM ((1 2 3 -10,11 12 13 -20))", wkbNDR,
                            "POLYGON ZM ((2 4 6 -10,12 14 16 -20))",
                            "POLYGON_ZM_NDR"),
            std::make_tuple("POLYGON ZM ((1 2 3 -10,11 12 13 -20))", wkbXDR,
                            "POLYGON ZM ((2 4 6 -10,12 14 16 -20))",
                            "POLYGON_ZM_XDR"),

            std::make_tuple("MULTIPOINT EMPTY", wkbNDR, "MULTIPOINT EMPTY",
                            "MULTIPOINT_EMPTY_NDR"),
            std::make_tuple("MULTIPOINT ((1 2),(11 12))", wkbNDR,
                            "MULTIPOINT ((2 4),(12 14))", "MULTIPOINT_NDR"),
            std::make_tuple("MULTIPOINT Z ((1 2 3),(11 12 13))", wkbXDR,
                            "MULTIPOINT Z ((2 4 6),(12 14 16))",
                            "MULTIPOINT_Z_XDR"),

            std::make_tuple("MULTILINESTRING ((1 2,11 12))", wkbNDR,
                            "MULTILINESTRING ((2 4,12 14))",
                            "MULTILINESTRING_NDR"),

            std::make_tuple("MULTIPOLYGON (((1 2,11 12)))", wkbNDR,
                            "MULTIPOLYGON (((2 4,12 14)))", "MULTIPOLYGON_NDR"),

            std::make_tuple("GEOMETRYCOLLECTION (POLYGON ((1 2,11 12)))",
                            wkbNDR,
                            "GEOMETRYCOLLECTION (POLYGON ((2 4,12 14)))",
                            "GEOMETRYCOLLECTION_NDR"),

            std::make_tuple("CIRCULARSTRING (1 2,11 12,21 22)", wkbNDR,
                            "CIRCULARSTRING (2 4,12 14,22 24)",
                            "CIRCULARSTRING_NDR"),

            std::make_tuple("COMPOUNDCURVE ((1 2,11 12))", wkbNDR,
                            "COMPOUNDCURVE ((2 4,12 14))", "COMPOUNDCURVE_NDR"),

            std::make_tuple("CURVEPOLYGON ((1 2,11 12,21 22,1 2))", wkbNDR,
                            "CURVEPOLYGON ((2 4,12 14,22 24,2 4))",
                            "CURVEPOLYGON_NDR"),

            std::make_tuple("MULTICURVE ((1 2,11 12))", wkbNDR,
                            "MULTICURVE ((2 4,12 14))", "MULTICURVE_NDR"),

            std::make_tuple("MULTISURFACE (((1 2,11 12)))", wkbNDR,
                            "MULTISURFACE (((2 4,12 14)))", "MULTISURFACE_NDR"),

            std::make_tuple("TRIANGLE ((1 2,11 12,21 22,1 2))", wkbNDR,
                            "TRIANGLE ((2 4,12 14,22 24,2 4))", "TRIANGLE_NDR"),

            std::make_tuple("POLYHEDRALSURFACE (((1 2,11 12,21 22,1 2)))",
                            wkbNDR,
                            "POLYHEDRALSURFACE (((2 4,12 14,22 24,2 4)))",
                            "POLYHEDRALSURFACE_NDR"),

            std::make_tuple("TIN (((1 2,11 12,21 22,1 2)))", wkbNDR,
                            "TIN (((2 4,12 14,22 24,2 4)))", "TIN_NDR"),
        };
    }
};

struct MyCT final : public OGRCoordinateTransformation
{
    const bool m_bSuccess;

    explicit MyCT(bool bSuccess = true) : m_bSuccess(bSuccess)
    {
    }

    const OGRSpatialReference *GetSourceCS() const override
    {
        return nullptr;
    }

    const OGRSpatialReference *GetTargetCS() const override
    {
        return nullptr;
    }

    int Transform(size_t nCount, double *x, double *y, double *z, double *,
                  int *pabSuccess) override
    {
        for (size_t i = 0; i < nCount; ++i)
        {
            x[i] += 1;
            y[i] += 2;
            if (z)
                z[i] += 3;
            if (pabSuccess)
                pabSuccess[i] = m_bSuccess;
        }
        return true;
    }

    OGRCoordinateTransformation *Clone() const override
    {
        return new MyCT();
    }

    OGRCoordinateTransformation *GetInverse() const override
    {
        return nullptr;
    }  // unused
};

TEST_P(OGRWKBTransformFixture, test)
{
    const char *pszInput = std::get<0>(GetParam());
    OGRwkbByteOrder eByteOrder = std::get<1>(GetParam());
    const char *pszOutput = std::get<2>(GetParam());

    MyCT oCT;
    oCT.GetSourceCS();        // just for code coverage purpose
    oCT.GetTargetCS();        // just for code coverage purpose
    delete oCT.Clone();       // just for code coverage purpose
    delete oCT.GetInverse();  // just for code coverage purpose

    OGRGeometry *poGeom = nullptr;
    EXPECT_EQ(OGRGeometryFactory::createFromWkt(pszInput, nullptr, &poGeom),
              OGRERR_NONE);
    ASSERT_TRUE(poGeom != nullptr);
    std::vector<GByte> abyWkb(poGeom->WkbSize());
    poGeom->exportToWkb(eByteOrder, abyWkb.data(), wkbVariantIso);
    delete poGeom;

    OGRWKBTransformCache oCache;
    OGREnvelope3D sEnv;
    EXPECT_TRUE(
        OGRWKBTransform(abyWkb.data(), abyWkb.size(), &oCT, oCache, sEnv));
    const auto abyWkbOri = abyWkb;

    poGeom = nullptr;
    OGRGeometryFactory::createFromWkb(abyWkb.data(), nullptr, &poGeom,
                                      abyWkb.size());
    ASSERT_TRUE(poGeom != nullptr);
    char *pszWKT = nullptr;
    poGeom->exportToWkt(&pszWKT, wkbVariantIso);
    delete poGeom;
    EXPECT_STREQ(pszWKT, pszOutput);
    CPLFree(pszWKT);

    {
        CPLErrorHandlerPusher oErrorHandler(CPLQuietErrorHandler);

        // Truncated geometry
        for (size_t i = 0; i < abyWkb.size(); ++i)
        {
            abyWkb = abyWkbOri;
            EXPECT_FALSE(OGRWKBTransform(abyWkb.data(), i, &oCT, oCache, sEnv));
        }

        // Check altering all bytes
        for (size_t i = 0; i < abyWkb.size(); ++i)
        {
            abyWkb = abyWkbOri;
            abyWkb[i] = 0xff;
            CPL_IGNORE_RET_VAL(OGRWKBTransform(abyWkb.data(), abyWkb.size(),
                                               &oCT, oCache, sEnv));
        }

        if (abyWkb.size() > 9)
        {
            abyWkb = abyWkbOri;
            if (!STARTS_WITH(pszInput, "POINT"))
            {
                // Corrupt number of sub-geometries
                abyWkb[5] = 0xff;
                abyWkb[6] = 0xff;
                abyWkb[7] = 0xff;
                abyWkb[8] = 0xff;
                EXPECT_FALSE(OGRWKBTransform(abyWkb.data(), abyWkb.size(), &oCT,
                                             oCache, sEnv));
            }
        }
    }
}

INSTANTIATE_TEST_SUITE_P(
    test_ogr_wkb, OGRWKBTransformFixture,
    ::testing::ValuesIn(OGRWKBTransformFixture::GetTupleValues()),
    [](const ::testing::TestParamInfo<OGRWKBTransformFixture::ParamType>
           &l_info) { return std::get<3>(l_info.param); });

TEST_F(test_ogr_wkb, OGRWKBTransformFixture_rec_collection)
{
    std::vector<GByte> abyWkb;
    constexpr int BEYOND_ALLOWED_RECURSION_LEVEL = 128;
    for (int i = 0; i < BEYOND_ALLOWED_RECURSION_LEVEL; ++i)
    {
        abyWkb.push_back(wkbNDR);
        abyWkb.push_back(wkbGeometryCollection);
        abyWkb.push_back(0);
        abyWkb.push_back(0);
        abyWkb.push_back(0);
        abyWkb.push_back(1);
        abyWkb.push_back(0);
        abyWkb.push_back(0);
        abyWkb.push_back(0);
    }
    {
        abyWkb.push_back(wkbNDR);
        abyWkb.push_back(wkbGeometryCollection);
        abyWkb.push_back(0);
        abyWkb.push_back(0);
        abyWkb.push_back(0);
        abyWkb.push_back(0);
        abyWkb.push_back(0);
        abyWkb.push_back(0);
        abyWkb.push_back(0);
    }

    MyCT oCT;
    OGRWKBTransformCache oCache;
    OGREnvelope3D sEnv;
    EXPECT_FALSE(
        OGRWKBTransform(abyWkb.data(), abyWkb.size(), &oCT, oCache, sEnv));
}

TEST_F(test_ogr_wkb, OGRWKBTransformFixture_ct_failure)
{
    MyCT oCT(/* bSuccess = */ false);
    OGRWKBTransformCache oCache;
    OGREnvelope3D sEnv;
    {
        OGRPoint p(1, 2);
        std::vector<GByte> abyWkb(p.WkbSize());
        static_cast<OGRGeometry &>(p).exportToWkb(wkbNDR, abyWkb.data(),
                                                  wkbVariantIso);

        EXPECT_FALSE(
            OGRWKBTransform(abyWkb.data(), abyWkb.size(), &oCT, oCache, sEnv));
    }
    {
        OGRLineString ls;
        ls.addPoint(1, 2);
        std::vector<GByte> abyWkb(ls.WkbSize());
        static_cast<OGRGeometry &>(ls).exportToWkb(wkbNDR, abyWkb.data(),
                                                   wkbVariantIso);

        EXPECT_FALSE(
            OGRWKBTransform(abyWkb.data(), abyWkb.size(), &oCT, oCache, sEnv));
    }
    {
        OGRPolygon p;
        auto poLR = std::make_unique<OGRLinearRing>();
        poLR->addPoint(1, 2);
        p.addRing(std::move(poLR));
        std::vector<GByte> abyWkb(p.WkbSize());
        static_cast<OGRGeometry &>(p).exportToWkb(wkbNDR, abyWkb.data(),
                                                  wkbVariantIso);

        EXPECT_FALSE(
            OGRWKBTransform(abyWkb.data(), abyWkb.size(), &oCT, oCache, sEnv));
    }
}

}  // namespace
