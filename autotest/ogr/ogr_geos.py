#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test GEOS integration in OGR - geometric operations.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2009-2012, Even Rouault <even dot rouault at spatialys.com>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the
# Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.
###############################################################################



import gdaltest
import ogrtest
from osgeo import ogr
from osgeo import gdal
import pytest

###############################################################################
@pytest.fixture(autouse=True, scope='module')
def startup_and_cleanup():

    if not ogrtest.have_geos():
        pytest.skip()

###############################################################################
# Establish whether we have GEOS support integrated, testing simple Union.


def test_ogr_geos_union():

    pnt1 = ogr.CreateGeometryFromWkt('POINT(10 20)')
    pnt2 = ogr.CreateGeometryFromWkt('POINT(30 20)')

    result = pnt1.Union(pnt2)

    assert not ogrtest.check_feature_geometry(result, 'MULTIPOINT (10 20,30 20)')

###############################################################################
# Test polygon intersection.


def test_ogr_geos_intersection():

    g1 = ogr.CreateGeometryFromWkt('POLYGON((0 0, 10 10, 10 0, 0 0))')
    g2 = ogr.CreateGeometryFromWkt('POLYGON((0 0, 0 10, 10 0, 0 0))')

    result = g1.Intersection(g2)

    assert not ogrtest.check_feature_geometry(result, 'POLYGON ((0 0,5 5,10 0,0 0))'), \
        ('Got: %s' % result.ExportToWkt())

###############################################################################
# Test polygon difference.


def test_ogr_geos_difference():

    g1 = ogr.CreateGeometryFromWkt('POLYGON((0 0, 10 10, 10 0, 0 0))')
    g2 = ogr.CreateGeometryFromWkt('POLYGON((0 0, 0 10, 10 0, 0 0))')

    result = g1.Difference(g2)

    assert (not ogrtest.check_feature_geometry(result,
                                      'POLYGON ((5 5,10 10,10 0,5 5))')), \
        ('Got: %s' % result.ExportToWkt())

###############################################################################
# Test polygon symmetric difference.


def test_ogr_geos_symmetric_difference():

    g1 = ogr.CreateGeometryFromWkt('POLYGON((0 0, 10 10, 10 0, 0 0))')
    g2 = ogr.CreateGeometryFromWkt('POLYGON((0 0, 0 10, 10 0, 0 0))')

    result = g1.SymmetricDifference(g2)

    assert (not ogrtest.check_feature_geometry(result,
                                      'MULTIPOLYGON (((5 5,0 0,0 10,5 5)),((5 5,10 10,10 0,5 5)))')), \
        ('Got: %s' % result.ExportToWkt())

###############################################################################
# Test polygon symmetric difference.


def test_ogr_geos_sym_difference():

    g1 = ogr.CreateGeometryFromWkt('POLYGON((0 0, 10 10, 10 0, 0 0))')
    g2 = ogr.CreateGeometryFromWkt('POLYGON((0 0, 0 10, 10 0, 0 0))')

    result = g1.SymDifference(g2)

    assert (not ogrtest.check_feature_geometry(result,
                                      'MULTIPOLYGON (((5 5,0 0,0 10,5 5)),((5 5,10 10,10 0,5 5)))')), \
        ('Got: %s' % result.ExportToWkt())

###############################################################################
# Test Intersect().


def test_ogr_geos_intersect():

    g1 = ogr.CreateGeometryFromWkt('LINESTRING(0 0, 10 10)')
    g2 = ogr.CreateGeometryFromWkt('LINESTRING(10 0, 0 10)')

    result = g1.Intersect(g2)

    assert result != 0, 'wrong result (got false)'

    g1 = ogr.CreateGeometryFromWkt('LINESTRING(0 0, 10 10)')
    g2 = ogr.CreateGeometryFromWkt('POLYGON((20 20, 20 30, 30 20, 20 20))')

    result = g1.Intersect(g2)

    assert result == 0, 'wrong result (got true)'

###############################################################################
# Test disjoint().


def test_ogr_geos_disjoint():

    g1 = ogr.CreateGeometryFromWkt('LINESTRING(0 0, 10 10)')
    g2 = ogr.CreateGeometryFromWkt('LINESTRING(10 0, 0 10)')

    result = g1.Disjoint(g2)

    assert result == 0, 'wrong result (got true)'

    g1 = ogr.CreateGeometryFromWkt('LINESTRING(0 0, 10 10)')
    g2 = ogr.CreateGeometryFromWkt('POLYGON((20 20, 20 30, 30 20, 20 20))')

    result = g1.Disjoint(g2)

    assert result != 0, 'wrong result (got false)'

###############################################################################
# Test touches.


def test_ogr_geos_touches():

    g1 = ogr.CreateGeometryFromWkt('LINESTRING(0 0, 10 10)')
    g2 = ogr.CreateGeometryFromWkt('LINESTRING(0 0, 0 10)')

    result = g1.Touches(g2)

    assert result != 0, 'wrong result (got false)'

    g1 = ogr.CreateGeometryFromWkt('LINESTRING(0 0, 10 10)')
    g2 = ogr.CreateGeometryFromWkt('POLYGON((20 20, 20 30, 30 20, 20 20))')

    result = g1.Touches(g2)

    assert result == 0, 'wrong result (got true)'

###############################################################################
# Test crosses.


def test_ogr_geos_crosses():

    g1 = ogr.CreateGeometryFromWkt('LINESTRING(0 0, 10 10)')
    g2 = ogr.CreateGeometryFromWkt('LINESTRING(10 0, 0 10)')

    result = g1.Crosses(g2)

    assert result != 0, 'wrong result (got false)'

    g1 = ogr.CreateGeometryFromWkt('LINESTRING(0 0, 10 10)')
    g2 = ogr.CreateGeometryFromWkt('LINESTRING(0 0, 0 10)')

    result = g1.Crosses(g2)

    assert result == 0, 'wrong result (got true)'

###############################################################################


def test_ogr_geos_within():

    g1 = ogr.CreateGeometryFromWkt('POLYGON((0 0, 10 10, 10 0, 0 0))')
    g2 = ogr.CreateGeometryFromWkt('POLYGON((-90 -90, -90 90, 190 -90, -90 -90))')

    result = g1.Within(g2)

    assert result != 0, 'wrong result (got false)'

    result = g2.Within(g1)

    assert result == 0, 'wrong result (got true)'

###############################################################################


def test_ogr_geos_contains():

    g1 = ogr.CreateGeometryFromWkt('POLYGON((0 0, 10 10, 10 0, 0 0))')
    g2 = ogr.CreateGeometryFromWkt('POLYGON((-90 -90, -90 90, 190 -90, -90 -90))')

    result = g2.Contains(g1)

    assert result != 0, 'wrong result (got false)'

    result = g1.Contains(g2)

    assert result == 0, 'wrong result (got true)'

###############################################################################


def test_ogr_geos_overlaps():

    g1 = ogr.CreateGeometryFromWkt('POLYGON((0 0, 10 10, 10 0, 0 0))')
    g2 = ogr.CreateGeometryFromWkt('POLYGON((-90 -90, -90 90, 190 -90, -90 -90))')

    result = g2.Overlaps(g1)

    # g1 and g2 intersect, but their intersection is equal to g1
    assert result == 0, 'wrong result (got true)'

    g1 = ogr.CreateGeometryFromWkt('POLYGON((0 0, 10 10, 10 0, 0 0))')
    g2 = ogr.CreateGeometryFromWkt('POLYGON((0 -5,10 5,10 -5,0 -5))')

    result = g2.Overlaps(g1)

    assert result != 0, 'wrong result (got false)'

###############################################################################


def test_ogr_geos_buffer():

    g1 = ogr.CreateGeometryFromWkt('POLYGON((0 0, 10 10, 10 0, 0 0))')

    result = g1.Buffer(1.0, 3)

    assert (ogrtest.check_feature_geometry(result,
                                      'POLYGON ((0 -1,-0.555570233019607 -0.831469612302542,-0.923879532511288 -0.382683432365087,-0.98078528040323 0.19509032201613,-0.707106781186547 0.707106781186547,9.292893218813452 10.707106781186548,9.690983005625053 10.951056516295154,10.156434465040231 10.987688340595138,10.587785252292473 10.809016994374947,10.891006524188368 10.453990499739547,11 10,11 0,10.866025403784439 -0.5,10.5 -0.866025403784439,10 -1,0 -1))') == 0), \
        ('Got: %s' % result.ExportToWkt())

###############################################################################


def test_ogr_geos_centroid():

    g1 = ogr.CreateGeometryFromWkt('POLYGON((0 0, 10 10, 10 0, 0 0))')

    centroid = g1.Centroid()

    assert (ogrtest.check_feature_geometry(centroid,
                                      'POINT(6.666666667 3.333333333)') == 0), \
        ('Got: %s' % centroid.ExportToWkt())

# Test with a self intersecting polygon too.
# This particular polygon has two triangles. The right triangle is larger.
    g2 = ogr.CreateGeometryFromWkt('POLYGON((0 0, 0 2, 2 -0.1, 2 2.1, 0 0))')
    centroid2 = g2.Centroid()

    assert ogrtest.check_feature_geometry(centroid2, 'POINT (8.0 1.0)') == 0, \
        ('Got: %s' % centroid2.ExportToWkt())

###############################################################################


def test_ogr_geos_centroid_multipolygon():

    g1 = ogr.CreateGeometryFromWkt('MULTIPOLYGON(((0 0,0 1,1 1,1 0,0 0)),((2 0,2 1,3 1,3 0,2 0)))')

    centroid = g1.Centroid()

    assert (ogrtest.check_feature_geometry(centroid,
                                      'POINT (1.5 0.5)') == 0), \
        ('Got: %s' % centroid.ExportToWkt())

###############################################################################


def test_ogr_geos_centroid_point_empty():

    g1 = ogr.CreateGeometryFromWkt('POINT EMPTY')

    centroid = g1.Centroid()

    assert centroid.ExportToWkt() == 'POINT EMPTY', ('Got: %s' % centroid.ExportToWkt())

###############################################################################


def test_ogr_geos_simplify_linestring():

    g1 = ogr.CreateGeometryFromWkt('LINESTRING(0 0,1 0,10 0)')

    gdal.ErrorReset()
    simplify = g1.Simplify(5)

    assert simplify.ExportToWkt() == 'LINESTRING (0 0,10 0)', \
        ('Got: %s' % simplify.ExportToWkt())

###############################################################################


def test_ogr_geos_simplifypreservetopology_linestring():

    g1 = ogr.CreateGeometryFromWkt('LINESTRING(0 0,1 0,10 0)')

    gdal.ErrorReset()
    simplify = g1.SimplifyPreserveTopology(5)

    assert simplify.ExportToWkt() == 'LINESTRING (0 0,10 0)', \
        ('Got: %s' % simplify.ExportToWkt())

###############################################################################


def test_ogr_geos_unioncascaded():

    g1 = ogr.CreateGeometryFromWkt('MULTIPOLYGON(((0 0,0 1,1 1,1 0,0 0)),((0.5 0.5,0.5 1.5,1.5 1.5,1.5 0.5,0.5 0.5)))')

    gdal.ErrorReset()
    cascadedunion = g1.UnionCascaded()

    assert ogrtest.check_feature_geometry(cascadedunion, 'POLYGON ((0 0,0 1,0.5 1.0,0.5 1.5,1.5 1.5,1.5 0.5,1.0 0.5,1 0,0 0))') == 0, \
        ('Got: %s' % cascadedunion.ExportToWkt())

###############################################################################


def test_ogr_geos_convexhull():

    g1 = ogr.CreateGeometryFromWkt('GEOMETRYCOLLECTION(POINT(0 1), POINT(0 0), POINT(1 0), POINT(1 1))')

    convexhull = g1.ConvexHull()

    assert convexhull.ExportToWkt() == 'POLYGON ((0 0,0 1,1 1,1 0,0 0))', \
        ('Got: %s' % convexhull.ExportToWkt())

###############################################################################


def test_ogr_geos_distance():

    g1 = ogr.CreateGeometryFromWkt('POINT(0 0)')
    g2 = ogr.CreateGeometryFromWkt('POINT(1 0)')

    distance = g1.Distance(g2)

    assert distance == pytest.approx(1, abs=0.00000000001), \
        ('Distance() result wrong, got %g.' % distance)

###############################################################################


def test_ogr_geos_isring():

    g1 = ogr.CreateGeometryFromWkt('LINESTRING(0 0,0 1,1 1,0 0)')

    isring = g1.IsRing()

    assert isring == 1

###############################################################################


def test_ogr_geos_issimple_true():

    g1 = ogr.CreateGeometryFromWkt('POLYGON ((0 0,0 1,1 1,1 0,0 0))')

    isring = g1.IsSimple()

    assert isring == 1

###############################################################################


def test_ogr_geos_issimple_false():

    g1 = ogr.CreateGeometryFromWkt('LINESTRING(1 1,2 2,2 3.5,1 3,1 2,2 1)')

    isring = g1.IsSimple()

    assert isring == 0

###############################################################################


def test_ogr_geos_isvalid_true():

    g1 = ogr.CreateGeometryFromWkt('LINESTRING(0 0, 1 1)')

    isring = g1.IsValid()

    assert isring == 1

###############################################################################


def test_ogr_geos_isvalid_true_linestringM():

    g1 = ogr.CreateGeometryFromWkt('LINESTRING M(0 0 10, 1 1 20)')

    isring = g1.IsValid()

    assert isring == 1

###############################################################################


def test_ogr_geos_isvalid_true_circularStringM():

    g1 = ogr.CreateGeometryFromWkt('CIRCULARSTRING M(0 0 10, 1 1 20,2 0 30)')

    isring = g1.IsValid()

    assert isring == 1

###############################################################################


def test_ogr_geos_isvalid_true_triangle():

    g1 = ogr.CreateGeometryFromWkt('TRIANGLE ((0 0,0 1,1 1,0 0))')

    isring = g1.IsValid()

    assert isring == 1

###############################################################################


def test_ogr_geos_isvalid_false():

    g1 = ogr.CreateGeometryFromWkt('POLYGON((0 0,1 1,1 2,1 1,0 0))')

    with gdaltest.error_handler():
        isring = g1.IsValid()

    assert isring == 0

###############################################################################


def test_ogr_geos_pointonsurface():

    g1 = ogr.CreateGeometryFromWkt('POLYGON((0 0, 10 10, 10 0, 0 0))')

    pointonsurface = g1.PointOnSurface()

    assert pointonsurface.Within(g1) == 1

###############################################################################


def test_ogr_geos_DelaunayTriangulation():

    g1 = ogr.CreateGeometryFromWkt('MULTIPOINT(0 0,0 1,1 1,1 0)')

    gdal.ErrorReset()
    triangulation = g1.DelaunayTriangulation()
    if triangulation is None:
        assert gdal.GetLastErrorMsg() != ''
        pytest.skip()

    assert triangulation.ExportToWkt() == 'GEOMETRYCOLLECTION (POLYGON ((0 1,0 0,1 0,0 1)),POLYGON ((0 1,1 0,1 1,0 1)))', \
        ('Got: %s' % triangulation.ExportToWkt())

###############################################################################


def test_ogr_geos_polygonize():

    g = ogr.CreateGeometryFromWkt('MULTILINESTRING((0 0,0 1,1 1),(1 1,0 0))')
    got = g.Polygonize()
    assert got.ExportToWkt() == 'GEOMETRYCOLLECTION (POLYGON ((0 0,0 1,1 1,0 0)))', \
        ('Got: %s' % got.ExportToWkt())

    g = ogr.CreateGeometryFromWkt('POINT EMPTY')
    got = g.Polygonize()
    assert got is None, ('Got: %s' % got.ExportToWkt())

    g = ogr.CreateGeometryFromWkt('GEOMETRYCOLLECTION(POINT EMPTY)')
    got = g.Polygonize()
    assert got is None, ('Got: %s' % got.ExportToWkt())

###############################################################################


def test_ogr_geos_prepared_geom():

    g = ogr.CreateGeometryFromWkt('POLYGON((0 0,0 1,1 1,1 0,0 0))')
    pg = g.CreatePreparedGeometry()

    assert pg.Contains(ogr.CreateGeometryFromWkt('POINT(0.5 0.5)'))
    assert not pg.Contains(ogr.CreateGeometryFromWkt('POINT(-0.5 0.5)'))

    g2 = ogr.CreateGeometryFromWkt('POLYGON((0.5 0,0.5 1,1.5 1,1.5 0,0.5 0))')
    assert pg.Intersects(g2)
    assert not pg.Intersects(ogr.CreateGeometryFromWkt('POINT(-0.5 0.5)'))

    # Test workaround for https://github.com/libgeos/geos/pull/423
    assert not pg.Intersects(ogr.CreateGeometryFromWkt('POINT EMPTY'))
    assert not pg.Contains(ogr.CreateGeometryFromWkt('POINT EMPTY'))
