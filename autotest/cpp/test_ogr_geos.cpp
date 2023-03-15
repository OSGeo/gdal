///////////////////////////////////////////////////////////////////////////////
//
// Project:  C++ Test Suite for GDAL/OGR
// Purpose:  Test GEOS integration in OGR - geometric operations.
//           Ported from ogr/ogr_geos.py.
// Author:   Mateusz Loskot <mateusz@loskot.net>
//
///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2006, Mateusz Loskot <mateusz@loskot.net>
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

#include "ogr_api.h"
#include "ogrsf_frmts.h"

#ifdef HAVE_GEOS
#include <geos_c.h>
#endif

#include <string>

#include "gtest_include.h"

namespace
{
using namespace tut;  // for CheckEqualGeometries

// Common fixture with test data
struct test_ogr_geos : public ::testing::Test
{
    OGRErr err_ = OGRERR_NONE;
    OGRGeometryH g1_ = nullptr;
    OGRGeometryH g2_ = nullptr;
    OGRGeometryH g3_ = nullptr;

    void SetUp() override
    {
#ifndef HAVE_GEOS
        GTEST_SKIP() << "GEOS support is not available";
#endif
    }

    void TearDown() override
    {
        OGR_G_DestroyGeometry(g1_);
        g1_ = nullptr;
        OGR_G_DestroyGeometry(g2_);
        g2_ = nullptr;
        OGR_G_DestroyGeometry(g3_);
        g3_ = nullptr;
    }
};

// Test export OGR geometry to GEOS using GDAL C++ API
TEST_F(test_ogr_geos, exportToGEOS)
{
#ifdef HAVE_GEOS
    const char *wkt = "POLYGON((0 0,4 0,4 4,0 4,0 0),(1 1, 2 1, 2 2, 1 2,1 1))";
    OGRPolygon geom;
    err_ = geom.importFromWkt(&wkt);
    ASSERT_EQ(OGRERR_NONE, err_);

    GEOSContextHandle_t ctxt = OGRGeometry::createGEOSContext();
    GEOSGeom geosGeom = geom.exportToGEOS(ctxt);
    OGRGeometry::freeGEOSContext(ctxt);
    ASSERT_TRUE(nullptr != geosGeom);

    GEOSGeom_destroy_r(ctxt, geosGeom);
#endif
}

// Test OGR_G_Contains function
TEST_F(test_ogr_geos, OGR_G_Contains)
{
    char *wktOuter =
        const_cast<char *>("POLYGON((-90 -90, -90 90, 190 -90, -90 -90))");
    err_ = OGR_G_CreateFromWkt(&wktOuter, nullptr, &g1_);
    ASSERT_EQ(OGRERR_NONE, err_);
    ASSERT_TRUE(nullptr != g1_);

    char *wktInner = const_cast<char *>("POLYGON((0 0, 10 10, 10 0, 0 0))");
    err_ = OGR_G_CreateFromWkt(&wktInner, nullptr, &g2_);
    ASSERT_EQ(OGRERR_NONE, err_);
    ASSERT_TRUE(nullptr != g2_);

    ASSERT_EQ(OGR_G_Contains(g1_, g2_), TRUE);
    ASSERT_EQ(OGR_G_Contains(g2_, g1_), FALSE);
}

// Test OGR_G_Crosses function
TEST_F(test_ogr_geos, OGR_G_Crosses)
{
    char *wkt1 = const_cast<char *>("LINESTRING(0 0, 10 10)");
    err_ = OGR_G_CreateFromWkt(&wkt1, nullptr, &g1_);
    ASSERT_EQ(OGRERR_NONE, err_);
    ASSERT_TRUE(nullptr != g1_);

    char *wkt2 = const_cast<char *>("LINESTRING(10 0, 0 10)");
    err_ = OGR_G_CreateFromWkt(&wkt2, nullptr, &g2_);
    ASSERT_EQ(OGRERR_NONE, err_);
    ASSERT_TRUE(nullptr != g2_);

    ASSERT_EQ(OGR_G_Crosses(g1_, g2_), TRUE);

    char *wkt3 = const_cast<char *>("LINESTRING(0 0, 0 10)");
    err_ = OGR_G_CreateFromWkt(&wkt3, nullptr, &g3_);
    ASSERT_EQ(OGRERR_NONE, err_);
    ASSERT_TRUE(nullptr != g3_);

    ASSERT_EQ(OGR_G_Crosses(g1_, g3_), FALSE);
}

// Test OGR_G_Disjoint function
TEST_F(test_ogr_geos, OGR_G_Disjoint)
{
    char *wkt1 = const_cast<char *>("LINESTRING(0 0, 10 10)");
    err_ = OGR_G_CreateFromWkt(&wkt1, nullptr, &g1_);
    ASSERT_EQ(OGRERR_NONE, err_);
    ASSERT_TRUE(nullptr != g1_);

    char *wkt2 = const_cast<char *>("LINESTRING(10 0, 0 10)");
    err_ = OGR_G_CreateFromWkt(&wkt2, nullptr, &g2_);
    ASSERT_EQ(OGRERR_NONE, err_);
    ASSERT_TRUE(nullptr != g2_);

    ASSERT_EQ(OGR_G_Disjoint(g1_, g2_), FALSE);

    char *wkt3 = const_cast<char *>("POLYGON((20 20, 20 30, 30 20, 20 20))");
    err_ = OGR_G_CreateFromWkt(&wkt3, nullptr, &g3_);
    ASSERT_EQ(OGRERR_NONE, err_);
    ASSERT_TRUE(nullptr != g3_);

    ASSERT_EQ(OGR_G_Disjoint(g1_, g3_), TRUE);
}

// Test OGR_G_Equals function
TEST_F(test_ogr_geos, OGR_G_Equals)
{
    char *wkt1 = const_cast<char *>("LINESTRING(0 0, 10 10)");
    err_ = OGR_G_CreateFromWkt(&wkt1, nullptr, &g1_);
    ASSERT_EQ(OGRERR_NONE, err_);
    ASSERT_TRUE(nullptr != g1_);

    char *wkt2 = const_cast<char *>("LINESTRING(0 0, 10 10)");
    err_ = OGR_G_CreateFromWkt(&wkt2, nullptr, &g2_);
    ASSERT_EQ(OGRERR_NONE, err_);
    ASSERT_TRUE(nullptr != g2_);

    ASSERT_EQ(OGR_G_Equals(g1_, g2_), TRUE);

    char *wkt3 = const_cast<char *>("POLYGON((20 20, 20 30, 30 20, 20 20))");
    err_ = OGR_G_CreateFromWkt(&wkt3, nullptr, &g3_);
    ASSERT_EQ(OGRERR_NONE, err_);
    ASSERT_TRUE(nullptr != g3_);

    ASSERT_EQ(OGR_G_Equals(g1_, g3_), FALSE);
}

// Test OGR_G_Intersects function
TEST_F(test_ogr_geos, OGR_G_Intersects)
{
    char *wkt1 = const_cast<char *>("POLYGON((0 0, 10 10, 10 0, 0 0))");
    err_ = OGR_G_CreateFromWkt(&wkt1, nullptr, &g1_);
    ASSERT_EQ(OGRERR_NONE, err_);
    ASSERT_TRUE(nullptr != g1_);

    char *wkt2 = const_cast<char *>("POLYGON((0 0, 0 10, 10 0, 0 0))");
    err_ = OGR_G_CreateFromWkt(&wkt2, nullptr, &g2_);
    ASSERT_EQ(OGRERR_NONE, err_);
    ASSERT_TRUE(nullptr != g2_);

    ASSERT_EQ(OGR_G_Intersects(g1_, g2_), TRUE);

    char *wkt3 = const_cast<char *>("POLYGON((20 20, 40 20, 40 40, 20 20))");
    err_ = OGR_G_CreateFromWkt(&wkt3, nullptr, &g3_);
    ASSERT_EQ(OGRERR_NONE, err_);
    ASSERT_TRUE(nullptr != g3_);

    ASSERT_EQ(OGR_G_Intersects(g1_, g3_), FALSE);
}

// Test OGR_G_Overlaps function
TEST_F(test_ogr_geos, OGR_G_Overlaps)
{
    char *wkt1 = const_cast<char *>("POLYGON((0 0, 10 10, 10 0, 0 0))");
    err_ = OGR_G_CreateFromWkt(&wkt1, nullptr, &g1_);
    ASSERT_EQ(OGRERR_NONE, err_);
    ASSERT_TRUE(nullptr != g1_);

    char *wkt2 =
        const_cast<char *>("POLYGON((-90 -90, -90 90, 190 -90, -90 -90))");
    err_ = OGR_G_CreateFromWkt(&wkt2, nullptr, &g2_);
    ASSERT_EQ(OGRERR_NONE, err_);
    ASSERT_TRUE(nullptr != g2_);

    ASSERT_EQ(OGR_G_Overlaps(g1_, g2_), FALSE);
}

// Test OGR_G_Touches function
TEST_F(test_ogr_geos, OGR_G_Touches)
{
    char *wkt1 = const_cast<char *>("LINESTRING(0 0, 10 10)");
    err_ = OGR_G_CreateFromWkt(&wkt1, nullptr, &g1_);
    ASSERT_EQ(OGRERR_NONE, err_);
    ASSERT_TRUE(nullptr != g1_);

    char *wkt2 = const_cast<char *>("LINESTRING(0 0, 0 10)");
    err_ = OGR_G_CreateFromWkt(&wkt2, nullptr, &g2_);
    ASSERT_EQ(OGRERR_NONE, err_);
    ASSERT_TRUE(nullptr != g2_);

    ASSERT_EQ(OGR_G_Touches(g1_, g2_), TRUE);

    char *wkt3 = const_cast<char *>("POLYGON((20 20, 20 30, 30 20, 20 20))");
    err_ = OGR_G_CreateFromWkt(&wkt3, nullptr, &g3_);
    ASSERT_EQ(OGRERR_NONE, err_);
    ASSERT_TRUE(nullptr != g3_);

    ASSERT_EQ(OGR_G_Touches(g1_, g3_), FALSE);
}

// Test OGR_G_Within function
TEST_F(test_ogr_geos, OGR_G_Within)
{
    char *wkt1 = const_cast<char *>("POLYGON((0 0, 10 10, 10 0, 0 0))");
    err_ = OGR_G_CreateFromWkt(&wkt1, nullptr, &g1_);
    ASSERT_EQ(OGRERR_NONE, err_);
    ASSERT_TRUE(nullptr != g1_);

    char *wkt2 =
        const_cast<char *>("POLYGON((-90 -90, -90 90, 190 -90, -90 -90))");
    err_ = OGR_G_CreateFromWkt(&wkt2, nullptr, &g2_);
    ASSERT_EQ(OGRERR_NONE, err_);
    ASSERT_TRUE(nullptr != g2_);

    ASSERT_EQ(OGR_G_Within(g1_, g2_), TRUE);

    ASSERT_EQ(OGR_G_Within(g2_, g1_), FALSE);
}

// Test OGR_G_Union function
TEST_F(test_ogr_geos, OGR_G_Union)
{
    char *wkt1 = const_cast<char *>("POINT(10 20)");
    err_ = OGR_G_CreateFromWkt(&wkt1, nullptr, &g1_);
    ASSERT_EQ(OGRERR_NONE, err_);
    ASSERT_TRUE(nullptr != g1_);

    char *wkt2 = const_cast<char *>("POINT(30 20)");
    err_ = OGR_G_CreateFromWkt(&wkt2, nullptr, &g2_);
    ASSERT_EQ(OGRERR_NONE, err_);
    ASSERT_TRUE(nullptr != g2_);

    g3_ = OGR_G_Union(g1_, g2_);
    ASSERT_TRUE(nullptr != g3_);

    OGRGeometryH expect = nullptr;
    char *wktExpect = const_cast<char *>("MULTIPOINT (10 20,30 20)");
    err_ = OGR_G_CreateFromWkt(&wktExpect, nullptr, &expect);
    ASSERT_EQ(OGRERR_NONE, err_);
    ASSERT_TRUE(nullptr != expect);

    // Compare operation result against expected geometry
    EXPECT_TRUE(CheckEqualGeometries(g3_, expect, 0.0001));

    OGR_G_DestroyGeometry(expect);
}

// Test OGR_G_UnaryUnion function
TEST_F(test_ogr_geos, OGR_G_UnaryUnion)
{
    char *wkt = const_cast<char *>("GEOMETRYCOLLECTION(POINT(0.5 0.5),"
                                   "POLYGON((0 0,0 1,1 1,1 0,0 0)),"
                                   "POLYGON((1 0,1 1,2 1,2 0,1 0)))");
    err_ = OGR_G_CreateFromWkt(&wkt, nullptr, &g1_);

    g3_ = OGR_G_UnaryUnion(g1_);
    ASSERT_TRUE(nullptr != g3_);

    OGRGeometryH expect = nullptr;
    char *wktExpect =
        const_cast<char *>("POLYGON ((0 1,1 1,2 1,2 0,1 0,0 0,0 1))");
    err_ = OGR_G_CreateFromWkt(&wktExpect, nullptr, &expect);
    ASSERT_EQ(OGRERR_NONE, err_);
    ASSERT_TRUE(nullptr != expect);

    OGREnvelope sEnvelopeIn;
    OGREnvelope sEnvelopeOut;
    OGR_G_GetEnvelope(g1_, &sEnvelopeIn);
    OGR_G_GetEnvelope(g3_, &sEnvelopeOut);

    // CheckEqualGeometries() doesn't work with GEOS 3.6 with the above
    // expected polygon, because of the order of the nodes, and for some
    // reason OGR_G_Normalize() in CheckEqualGeometries() doesn't fix this,
    // so just fallback to bounding box and area comparison
    EXPECT_EQ(sEnvelopeIn.MinX, sEnvelopeOut.MinX);
    EXPECT_EQ(sEnvelopeIn.MinY, sEnvelopeOut.MinY);
    EXPECT_EQ(sEnvelopeIn.MaxX, sEnvelopeOut.MaxX);
    EXPECT_EQ(sEnvelopeIn.MaxY, sEnvelopeOut.MaxY);
    EXPECT_EQ(OGR_G_Area(g1_), OGR_G_Area(g3_));

    OGR_G_DestroyGeometry(expect);
}

// Test OGR_G_Intersection function
TEST_F(test_ogr_geos, OGR_G_Intersection)
{
    char *wkt1 = const_cast<char *>("POLYGON((0 0, 10 10, 10 0, 0 0))");
    err_ = OGR_G_CreateFromWkt(&wkt1, nullptr, &g1_);
    ASSERT_EQ(OGRERR_NONE, err_);
    ASSERT_TRUE(nullptr != g1_);

    char *wkt2 = const_cast<char *>("POLYGON((0 0, 0 10, 10 0, 0 0))");
    err_ = OGR_G_CreateFromWkt(&wkt2, nullptr, &g2_);
    ASSERT_EQ(OGRERR_NONE, err_);
    ASSERT_TRUE(nullptr != g2_);

    g3_ = OGR_G_Intersection(g1_, g2_);
    ASSERT_TRUE(nullptr != g3_);

    OGRGeometryH expect = nullptr;
    char *wktExpect = const_cast<char *>("POLYGON ((0 0,5 5,10 0,0 0))");
    err_ = OGR_G_CreateFromWkt(&wktExpect, nullptr, &expect);
    ASSERT_EQ(OGRERR_NONE, err_);
    ASSERT_TRUE(nullptr != expect);

    // Compare operation result against expected geometry
    EXPECT_TRUE(CheckEqualGeometries(g3_, expect, 0.0001));

    OGR_G_DestroyGeometry(expect);
}

// Test OGR_G_Difference function
TEST_F(test_ogr_geos, OGR_G_Difference)
{
    char *wkt1 = const_cast<char *>("POLYGON((0 0, 10 10, 10 0, 0 0))");
    err_ = OGR_G_CreateFromWkt(&wkt1, nullptr, &g1_);
    ASSERT_EQ(OGRERR_NONE, err_);
    ASSERT_TRUE(nullptr != g1_);

    char *wkt2 = const_cast<char *>("POLYGON((0 0, 0 10, 10 0, 0 0))");
    err_ = OGR_G_CreateFromWkt(&wkt2, nullptr, &g2_);
    ASSERT_EQ(OGRERR_NONE, err_);
    ASSERT_TRUE(nullptr != g2_);

    g3_ = OGR_G_Difference(g1_, g2_);
    ASSERT_TRUE(nullptr != g3_);

    OGRGeometryH expect = nullptr;
    char *wktExpect = const_cast<char *>("POLYGON ((5 5,10 10,10 0,5 5))");
    err_ = OGR_G_CreateFromWkt(&wktExpect, nullptr, &expect);
    ASSERT_EQ(OGRERR_NONE, err_);
    ASSERT_TRUE(nullptr != expect);

    // Compare operation result against expected geometry
    EXPECT_TRUE(CheckEqualGeometries(g3_, expect, 0.0001));

    OGR_G_DestroyGeometry(expect);
}

// Test OGR_G_SymDifference function
TEST_F(test_ogr_geos, OGR_G_SymDifference)
{
    char *wkt1 = const_cast<char *>("POLYGON((0 0, 10 10, 10 0, 0 0))");
    err_ = OGR_G_CreateFromWkt(&wkt1, nullptr, &g1_);
    ASSERT_EQ(OGRERR_NONE, err_);
    ASSERT_TRUE(nullptr != g1_);

    char *wkt2 = const_cast<char *>("POLYGON((0 0, 0 10, 10 0, 0 0))");
    err_ = OGR_G_CreateFromWkt(&wkt2, nullptr, &g2_);
    ASSERT_EQ(OGRERR_NONE, err_);
    ASSERT_TRUE(nullptr != g2_);

    g3_ = OGR_G_SymDifference(g1_, g2_);
    ASSERT_TRUE(nullptr != g3_);

    OGRGeometryH expect = nullptr;
    char *wktExpect = const_cast<char *>(
        "MULTIPOLYGON (((5 5,0 0,0 10,5 5)),((5 5,10 10,10 0,5 5)))");
    err_ = OGR_G_CreateFromWkt(&wktExpect, nullptr, &expect);
    ASSERT_EQ(OGRERR_NONE, err_);
    ASSERT_TRUE(nullptr != expect);

    // Compare operation result against expected geometry
    EXPECT_TRUE(CheckEqualGeometries(g3_, expect, 0.0001));

    OGR_G_DestroyGeometry(expect);
}
}  // namespace
