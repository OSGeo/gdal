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

namespace tut
{

    // Common fixture with test data
    struct test_geos_data
    {
        OGRErr err_;
        OGRGeometryH g1_;
        OGRGeometryH g2_;
        OGRGeometryH g3_;

        test_geos_data()
            : err_(OGRERR_NONE), g1_(nullptr), g2_(nullptr), g3_(nullptr)
        {}

        ~test_geos_data()
        {
            OGR_G_DestroyGeometry(g1_);
            OGR_G_DestroyGeometry(g2_);
            OGR_G_DestroyGeometry(g3_);
        }
    };

    // Register test group
    typedef test_group<test_geos_data> group;
    typedef group::object object;
    group test_geos_group("OGR::GEOS");

#ifdef OGR_ENABLED
#ifdef HAVE_GEOS

    // Test GEOS support enabled
    template<>
    template<>
    void object::test<1>()
    {
        // HAVE_GEOS definition promises GEOS support is enabled
        ensure(true);
    }

    // Test export OGR geometry to GEOS using GDAL C++ API
    template<>
    template<>
    void object::test<2>()
    {
        char* wkt = "POLYGON((0 0,4 0,4 4,0 4,0 0),(1 1, 2 1, 2 2, 1 2,1 1))";
        OGRPolygon geom;
        err_ = geom.importFromWkt(&wkt);
        ensure_equals("Can't import geometry from WKT", OGRERR_NONE, err_);

        GEOSGeom geosGeom = geom.exportToGEOS();
        ensure("Can't export geometry to GEOS", NULL != geosGeom);

        GEOSGeom_destroy(geosGeom);
    }

    // Test OGR_G_Contains function
    template<>
    template<>
    void object::test<3>()
    {
        char* wktOuter = "POLYGON((-90 -90, -90 90, 190 -90, -90 -90))";
        err_ = OGR_G_CreateFromWkt(&wktOuter, NULL, &g1_);
        ensure_equals("Can't import geometry from WKT", OGRERR_NONE, err_);
        ensure("Can't create geometry", NULL != g1_);

        char* wktInner = "POLYGON((0 0, 10 10, 10 0, 0 0))";
        err_ = OGR_G_CreateFromWkt(&wktInner, NULL, &g2_);
        ensure_equals("Can't import geometry from WKT", OGRERR_NONE, err_);
        ensure("Can't create geometry", NULL != g2_);

        ensure_equals("OGR_G_Contains() failed with FALSE",
            OGR_G_Contains(g1_, g2_), TRUE);
        ensure_equals("OGR_G_Contains() failed with TRUE",
            OGR_G_Contains(g2_, g1_), FALSE);
    }

    // Test OGR_G_Crosses function
    template<>
    template<>
    void object::test<4>()
    {
        char* wkt1 = "LINESTRING(0 0, 10 10)";
        err_ = OGR_G_CreateFromWkt(&wkt1, NULL, &g1_);
        ensure_equals("Can't import geometry from WKT", OGRERR_NONE, err_);
        ensure("Can't create geometry", NULL != g1_);

        char* wkt2 = "LINESTRING(10 0, 0 10)";
        err_ = OGR_G_CreateFromWkt(&wkt2, NULL, &g2_);
        ensure_equals("Can't import geometry from WKT", OGRERR_NONE, err_);
        ensure("Can't create geometry", NULL != g2_);

        ensure_equals("OGR_G_Crosses() failed with FALSE",
            OGR_G_Crosses(g1_, g2_), TRUE);

        char* wkt3 = "LINESTRING(0 0, 0 10)";
        err_ = OGR_G_CreateFromWkt(&wkt3, NULL, &g3_);
        ensure_equals("Can't import geometry from WKT", OGRERR_NONE, err_);
        ensure("Can't create geometry", NULL != g3_);

        ensure_equals("OGR_G_Crosses() failed with TRUE",
            OGR_G_Crosses(g1_, g3_), FALSE);
    }

    // Test OGR_G_Disjoint function
    template<>
    template<>
    void object::test<5>()
    {
        char* wkt1 = "LINESTRING(0 0, 10 10)";
        err_ = OGR_G_CreateFromWkt(&wkt1, NULL, &g1_);
        ensure_equals("Can't import geometry from WKT", OGRERR_NONE, err_);
        ensure("Can't create geometry", NULL != g1_);

        char* wkt2 = "LINESTRING(10 0, 0 10)";
        err_ = OGR_G_CreateFromWkt(&wkt2, NULL, &g2_);
        ensure_equals("Can't import geometry from WKT", OGRERR_NONE, err_);
        ensure("Can't create geometry", NULL != g2_);

        ensure_equals("OGR_G_Disjoint() failed with TRUE",
            OGR_G_Disjoint(g1_, g2_), FALSE);

        char* wkt3 = "POLYGON((20 20, 20 30, 30 20, 20 20))";
        err_ = OGR_G_CreateFromWkt(&wkt3, NULL, &g3_);
        ensure_equals("Can't import geometry from WKT", OGRERR_NONE, err_);
        ensure("Can't create geometry", NULL != g3_);

        ensure_equals("OGR_G_Disjoint() failed with FALSE",
            OGR_G_Disjoint(g1_, g3_), TRUE);
    }

    // Test OGR_G_Equals function
    template<>
    template<>
    void object::test<6>()
    {
        char* wkt1 = "LINESTRING(0 0, 10 10)";
        err_ = OGR_G_CreateFromWkt(&wkt1, NULL, &g1_);
        ensure_equals("Can't import geometry from WKT", OGRERR_NONE, err_);
        ensure("Can't create geometry", NULL != g1_);

        char* wkt2 = "LINESTRING(0 0, 10 10)";
        err_ = OGR_G_CreateFromWkt(&wkt2, NULL, &g2_);
        ensure_equals("Can't import geometry from WKT", OGRERR_NONE, err_);
        ensure("Can't create geometry", NULL != g2_);

        ensure_equals("OGR_G_Equals() failed with FALSE",
            OGR_G_Equals(g1_, g2_), TRUE);

        char* wkt3 = "POLYGON((20 20, 20 30, 30 20, 20 20))";
        err_ = OGR_G_CreateFromWkt(&wkt3, NULL, &g3_);
        ensure_equals("Can't import geometry from WKT", OGRERR_NONE, err_);
        ensure("Can't create geometry", NULL != g3_);

        ensure_equals("OGR_G_Equals() failed with TRUE",
            OGR_G_Equals(g1_, g3_), FALSE);
    }

    // Test OGR_G_Intersects function
    template<>
    template<>
    void object::test<7>()
    {
        char* wkt1 = "POLYGON((0 0, 10 10, 10 0, 0 0))";
        err_ = OGR_G_CreateFromWkt(&wkt1, NULL, &g1_);
        ensure_equals("Can't import geometry from WKT", OGRERR_NONE, err_);
        ensure("Can't create geometry", NULL != g1_);

        char* wkt2 = "POLYGON((0 0, 0 10, 10 0, 0 0))";
        err_ = OGR_G_CreateFromWkt(&wkt2, NULL, &g2_);
        ensure_equals("Can't import geometry from WKT", OGRERR_NONE, err_);
        ensure("Can't create geometry", NULL != g2_);

        ensure_equals("OGR_G_Intersects() failed with FALSE",
            OGR_G_Intersects(g1_, g2_), TRUE);

        char* wkt3 = "POLYGON((20 20, 40 20, 40 40, 20 20))";
        err_ = OGR_G_CreateFromWkt(&wkt3, NULL, &g3_);
        ensure_equals("Can't import geometry from WKT", OGRERR_NONE, err_);
        ensure("Can't create geometry", NULL != g3_);

        ensure_equals("OGR_G_Intersects() failed with TRUE",
            OGR_G_Intersects(g1_, g3_), FALSE);
    }

    // Test OGR_G_Overlaps function
    template<>
    template<>
    void object::test<8>()
    {
        char* wkt1 = "POLYGON((0 0, 10 10, 10 0, 0 0))";
        err_ = OGR_G_CreateFromWkt(&wkt1, NULL, &g1_);
        ensure_equals("Can't import geometry from WKT", OGRERR_NONE, err_);
        ensure("Can't create geometry", NULL != g1_);

        char* wkt2 = "POLYGON((-90 -90, -90 90, 190 -90, -90 -90))";
        err_ = OGR_G_CreateFromWkt(&wkt2, NULL, &g2_);
        ensure_equals("Can't import geometry from WKT", OGRERR_NONE, err_);
        ensure("Can't create geometry", NULL != g2_);

        ensure_equals("OGR_G_Overlaps() failed with TRUE",
            OGR_G_Overlaps(g1_, g2_), FALSE);
    }

    // Test OGR_G_Touches function
    template<>
    template<>
    void object::test<9>()
    {
        char* wkt1 = "LINESTRING(0 0, 10 10)";
        err_ = OGR_G_CreateFromWkt(&wkt1, NULL, &g1_);
        ensure_equals("Can't import geometry from WKT", OGRERR_NONE, err_);
        ensure("Can't create geometry", NULL != g1_);

        char* wkt2 = "LINESTRING(0 0, 0 10)";
        err_ = OGR_G_CreateFromWkt(&wkt2, NULL, &g2_);
        ensure_equals("Can't import geometry from WKT", OGRERR_NONE, err_);
        ensure("Can't create geometry", NULL != g2_);

        ensure_equals("OGR_G_Touches() failed with FALSE",
            OGR_G_Touches(g1_, g2_), TRUE);

        char* wkt3 = "POLYGON((20 20, 20 30, 30 20, 20 20))";
        err_ = OGR_G_CreateFromWkt(&wkt3, NULL, &g3_);
        ensure_equals("Can't import geometry from WKT", OGRERR_NONE, err_);
        ensure("Can't create geometry", NULL != g3_);

        ensure_equals("OGR_G_Touches() failed with TRUE",
            OGR_G_Touches(g1_, g3_), FALSE);
    }

    // Test OGR_G_Within function
    template<>
    template<>
    void object::test<10>()
    {
        char* wkt1 = "POLYGON((0 0, 10 10, 10 0, 0 0))";
        err_ = OGR_G_CreateFromWkt(&wkt1, NULL, &g1_);
        ensure_equals("Can't import geometry from WKT", OGRERR_NONE, err_);
        ensure("Can't create geometry", NULL != g1_);

        char* wkt2 = "POLYGON((-90 -90, -90 90, 190 -90, -90 -90))";
        err_ = OGR_G_CreateFromWkt(&wkt2, NULL, &g2_);
        ensure_equals("Can't import geometry from WKT", OGRERR_NONE, err_);
        ensure("Can't create geometry", NULL != g2_);

        ensure_equals("OGR_G_Within() failed with FALSE",
            OGR_G_Within(g1_, g2_), TRUE);

        ensure_equals("OGR_G_Within() failed with TRUE",
            OGR_G_Within(g2_, g1_), FALSE);
    }

    // Test OGR_G_Union function
    template<>
    template<>
    void object::test<11>()
    {
        char* wkt1 = "POINT(10 20)";
        err_ = OGR_G_CreateFromWkt(&wkt1, NULL, &g1_);
        ensure_equals("Can't import geometry from WKT", OGRERR_NONE, err_);
        ensure("Can't create geometry", NULL != g1_);

        char* wkt2 = "POINT(30 20)";
        err_ = OGR_G_CreateFromWkt(&wkt2, NULL, &g2_);
        ensure_equals("Can't import geometry from WKT", OGRERR_NONE, err_);
        ensure("Can't create geometry", NULL != g2_);

        g3_ = OGR_G_Union(g1_, g2_);
        ensure("OGR_G_Union failed with NULL", NULL != g3_);

        OGRGeometryH expect = NULL;
        char* wktExpect = "MULTIPOINT (10 20,30 20)";
        err_ = OGR_G_CreateFromWkt(&wktExpect, NULL, &expect);
        ensure_equals("Can't import geometry from WKT", OGRERR_NONE, err_);
        ensure("Can't create geometry", NULL != expect);

        // Compare operation result against expected geometry
        ensure_equal_geometries(g3_, expect, 0.0001);

        OGR_G_DestroyGeometry(expect);
    }

    // Test OGR_G_Intersection function
    template<>
    template<>
    void object::test<12>()
    {
        char* wkt1 = "POLYGON((0 0, 10 10, 10 0, 0 0))";
        err_ = OGR_G_CreateFromWkt(&wkt1, NULL, &g1_);
        ensure_equals("Can't import geometry from WKT", OGRERR_NONE, err_);
        ensure("Can't create geometry", NULL != g1_);

        char* wkt2 = "POLYGON((0 0, 0 10, 10 0, 0 0))";
        err_ = OGR_G_CreateFromWkt(&wkt2, NULL, &g2_);
        ensure_equals("Can't import geometry from WKT", OGRERR_NONE, err_);
        ensure("Can't create geometry", NULL != g2_);

        g3_ = OGR_G_Intersection(g1_, g2_);
        ensure("OGR_G_Intersection failed with NULL", NULL != g3_);

        OGRGeometryH expect = NULL;
        char* wktExpect = "POLYGON ((0 0,5 5,10 0,0 0))";
        err_ = OGR_G_CreateFromWkt(&wktExpect, NULL, &expect);
        ensure_equals("Can't import geometry from WKT", OGRERR_NONE, err_);
        ensure("Can't create geometry", NULL != expect);

        // Compare operation result against expected geometry
        ensure_equal_geometries(g3_, expect, 0.0001);

        OGR_G_DestroyGeometry(expect);
    }

    // Test OGR_G_Difference function
    template<>
    template<>
    void object::test<13>()
    {
        char* wkt1 = "POLYGON((0 0, 10 10, 10 0, 0 0))";
        err_ = OGR_G_CreateFromWkt(&wkt1, NULL, &g1_);
        ensure_equals("Can't import geometry from WKT", OGRERR_NONE, err_);
        ensure("Can't create geometry", NULL != g1_);

        char* wkt2 = "POLYGON((0 0, 0 10, 10 0, 0 0))";
        err_ = OGR_G_CreateFromWkt(&wkt2, NULL, &g2_);
        ensure_equals("Can't import geometry from WKT", OGRERR_NONE, err_);
        ensure("Can't create geometry", NULL != g2_);

        g3_ = OGR_G_Difference(g1_, g2_);
        ensure("OGR_G_Difference failed with NULL", NULL != g3_);

        OGRGeometryH expect = NULL;
        char* wktExpect = "POLYGON ((5 5,10 10,10 0,5 5))";
        err_ = OGR_G_CreateFromWkt(&wktExpect, NULL, &expect);
        ensure_equals("Can't import geometry from WKT", OGRERR_NONE, err_);
        ensure("Can't create geometry", NULL != expect);

        // Compare operation result against expected geometry
        ensure_equal_geometries(g3_, expect, 0.0001);

        OGR_G_DestroyGeometry(expect);
    }

    // Test OGR_G_SymmetricDifference function
    template<>
    template<>
    void object::test<14>()
    {
        char* wkt1 = "POLYGON((0 0, 10 10, 10 0, 0 0))";
        err_ = OGR_G_CreateFromWkt(&wkt1, NULL, &g1_);
        ensure_equals("Can't import geometry from WKT", OGRERR_NONE, err_);
        ensure("Can't create geometry", NULL != g1_);

        char* wkt2 = "POLYGON((0 0, 0 10, 10 0, 0 0))";
        err_ = OGR_G_CreateFromWkt(&wkt2, NULL, &g2_);
        ensure_equals("Can't import geometry from WKT", OGRERR_NONE, err_);
        ensure("Can't create geometry", NULL != g2_);

        g3_ = OGR_G_SymmetricDifference(g1_, g2_);
        ensure("OGR_G_SymmetricDifference failed with NULL", NULL != g3_);

        OGRGeometryH expect = NULL;
        char* wktExpect = "MULTIPOLYGON (((5 5,0 0,0 10,5 5)),((5 5,10 10,10 0,5 5)))";
        err_ = OGR_G_CreateFromWkt(&wktExpect, NULL, &expect);
        ensure_equals("Can't import geometry from WKT", OGRERR_NONE, err_);
        ensure("Can't create geometry", NULL != expect);

        // Compare operation result against expected geometry
        ensure_equal_geometries(g3_, expect, 0.0001);

        OGR_G_DestroyGeometry(expect);
    }

#else // HAVE_GEOS

    // Test GEOS support is disabled and shout about it
    template<>
    template<>
    void object::test<1>()
    {
        CPLDebug( "TEST", "GEOS support is not available" );
    }

#endif // ndef HAVE_GEOS
#endif // OGR_ENABLED

} // namespace tut
