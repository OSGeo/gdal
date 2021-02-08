#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test Misc. OGRGeometry operations.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2008-2014, Even Rouault <even dot rouault at spatialys.com>
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

import copy
import math
import pickle
import random


import gdaltest
import ogrtest
from osgeo import ogr
from osgeo import osr
from osgeo import gdal
import pytest

###############################################################################
# Test Area calculation for a MultiPolygon (which exercises lower level
# get_Area() methods as well).


def test_ogr_geom_area():

    geom_wkt = 'MULTIPOLYGON( ((0 0,1 1,1 0,0 0)),((0 0,10 0, 10 10, 0 10),(1 1,1 2,2 2,2 1)) )'
    geom = ogr.CreateGeometryFromWkt(geom_wkt)

    area = geom.GetArea()
    assert area == pytest.approx(99.5, abs=0.00000000001), ('GetArea() result wrong, got %g.' % area)

    # OGR >= 1.8.0
    area = geom.Area()
    assert area == pytest.approx(99.5, abs=0.00000000001), ('Area() result wrong, got %g.' % area)

###############################################################################
# Test Area calculation for a LinearRing (which exercises special case of
# getGeometryType value).


def test_ogr_geom_area_linearring():

    geom = ogr.Geometry(type=ogr.wkbLinearRing)
    geom.AddPoint_2D(0, 0)
    geom.AddPoint_2D(10, 0)
    geom.AddPoint_2D(10, 10)
    geom.AddPoint_2D(0, 10)
    geom.AddPoint_2D(0, 0)

    area = geom.GetArea()
    assert area == pytest.approx(100.0, abs=0.00000000001), ('Area result wrong, got %g.' % area)

###############################################################################
# Test Area calculation for a GeometryCollection


def test_ogr_geom_area_geometrycollection():

    # OGR >= 1.8.0
    geom_wkt = 'GEOMETRYCOLLECTION( POLYGON((0 0,1 1,1 0,0 0)), MULTIPOLYGON(((0 0,1 1,1 0,0 0))), LINESTRING(0 0,1 1), POINT(0 0), GEOMETRYCOLLECTION EMPTY )'
    geom = ogr.CreateGeometryFromWkt(geom_wkt)

    area = geom.Area()
    assert area == pytest.approx(1, abs=0.00000000001), ('Area() result wrong, got %g.' % area)

###############################################################################
# Test Area calculation for a LinearRing whose coordinates are shifted by a
# huge value With algorithm prior to #3556, this would return 0.


def test_ogr_geom_area_linearring_big_offset():

    geom = ogr.Geometry(type=ogr.wkbLinearRing)
    BIGOFFSET = 1.0e11
    geom.AddPoint_2D(BIGOFFSET + 0, BIGOFFSET + 0)
    geom.AddPoint_2D(BIGOFFSET + 10, BIGOFFSET + 0)
    geom.AddPoint_2D(BIGOFFSET + 10, BIGOFFSET + 10)
    geom.AddPoint_2D(BIGOFFSET + 0, BIGOFFSET + 10)
    geom.AddPoint_2D(BIGOFFSET + 0, BIGOFFSET + 0)

    area = geom.GetArea()
    assert area == pytest.approx(100.0, abs=0.00000000001), ('Area result wrong, got %g.' % area)

###############################################################################
# Test Area calculation for a Triangle


def test_ogr_geom_area_triangle():

    geom_wkt = 'TRIANGLE((0 0,100 0,0 100,0 0))'
    geom = ogr.CreateGeometryFromWkt(geom_wkt)

    area = geom.GetArea()
    assert area != pytest.approx(4999.5, abs=0.00000000001), \
        ('GetArea() result wrong, got %g.' % area)

    # OGR >= 1.8.0
    area = geom.Area()
    assert area != pytest.approx(4999.5, abs=0.00000000001), ('Area() result wrong, got %g.' % area)


def test_ogr_geom_is_empty():

    geom_wkt = 'LINESTRING EMPTY'
    geom = ogr.CreateGeometryFromWkt(geom_wkt)

    assert geom.IsEmpty(), "IsEmpty returning false for an empty geometry"

    geom_wkt = 'POINT( 1 2 )'
    geom = ogr.CreateGeometryFromWkt(geom_wkt)
    assert geom, ("A geometry could not be created from wkt: %s" % geom_wkt)

    assert not geom.IsEmpty(), "IsEmpty returning true for a non-empty geometry"

###############################################################################
# Test if a Triangle is Empty


def test_ogr_geom_is_empty_triangle():

    geom_wkt = 'TRIANGLE EMPTY'
    geom = ogr.CreateGeometryFromWkt(geom_wkt)

    assert geom.IsEmpty(), "IsEmpty returning false for an empty geometry"

    geom = ogr.CreateGeometryFromWkb(geom.ExportToWkb())

    assert geom.IsEmpty(), "IsEmpty returning false for an empty geometry"

    geom_wkt = 'TRIANGLE((0 0,100 0,0 100,0 0))'

    geom = ogr.CreateGeometryFromWkt(geom_wkt)
    assert geom, ("A geometry could not be created from wkt: %s" % geom_wkt)

    assert not geom.IsEmpty(), "IsEmpty returning true for a non-empty geometry"


def test_ogr_geom_pickle():
    geom_wkt = 'MULTIPOLYGON( ((0 0,1 1,1 0,0 0)),((0 0,10 0, 10 10, 0 10),(1 1,1 2,2 2,2 1)) )'
    geom = ogr.CreateGeometryFromWkt(geom_wkt)
    p = pickle.dumps(geom)
    with gdaltest.error_handler():
        g = pickle.loads(p)

    assert geom.Equal(g), "pickled geometries were not equal"

###############################################################################
# Test suite for PolyhedralSurface


def test_ogr_geom_polyhedral_surface():

    wkt_original = 'POLYHEDRALSURFACE Z (((0 0 0,0 0 1,0 1 1,0 1 0,0 0 0)),\
((0 0 0,0 1 0,1 1 0,1 0 0,0 0 0)),\
((0 0 0,1 0 0,1 0 1,0 0 1,0 0 0)),\
((1 1 0,1 1 1,1 0 1,1 0 0,1 1 0)),\
((0 1 0,0 1 1,1 1 1,1 1 0,0 1 0)),\
((0 0 1,1 0 1,1 1 1,0 1 1,0 0 1)))'
    ps = ogr.CreateGeometryFromWkt(wkt_original)

    wkb_string = ps.ExportToWkb(ogr.wkbXDR)
    geom = ogr.CreateGeometryFromWkb(wkb_string)
    wkt_string = geom.ExportToWkt()
    assert wkt_string == wkt_original, "Failure in Wkb methods of PolyhedralSurface"

    wkt_string = geom.Clone().ExportToWkt()
    assert wkt_string == wkt_original, "Failure in Clone()"

    polygon_wkt = ogr.ForceTo(geom.Clone(), ogr.wkbPolygon).ExportToWkt()
    assert polygon_wkt == wkt_original

    polygon_wkt = ogr.ForceTo(geom.Clone(), ogr.wkbMultiPolygon).ExportToWkt()
    assert polygon_wkt == 'MULTIPOLYGON (((0 0 0,0 0 1,0 1 1,0 1 0,0 0 0)),((0 0 0,0 1 0,1 1 0,1 0 0,0 0 0)),((0 0 0,1 0 0,1 0 1,0 0 1,0 0 0)),((1 1 0,1 1 1,1 0 1,1 0 0,1 1 0)),((0 1 0,0 1 1,1 1 1,1 1 0,0 1 0)),((0 0 1,1 0 1,1 1 1,0 1 1,0 0 1)))'

    if ogrtest.have_sfcgal():
        area = ps.Area()
        assert area == 6.0, "Wrong area of PolyhedralSurface"

    size = ps.WkbSize()
    assert size == 807, "Wrong WkbSize() of PolyhedralSurface"

    # if ogrtest.have_sfcgal():
    #    geom = ps.DelaunayTriangulation(0.0,True)
    #    wkt_geom_dt = 'MULTILINESTRING ((0 1 0,1 1 0),(0 0 0,0 1 0),(0 0 0,1 0 0),(1 0 0,1 1 0),(0 1 0,1 0 0))'
    #    wkt_geom = geom.ExportToWkt()
    #    if wkt_geom != wkt_geom_dt:
    #        gdaltest.post_reason ("Failure in DelaunayTriangulation() of PolyhedralSurface")
    #        print(wkt_geom)
    #        return 'fail'

    if ogrtest.have_geos() or ogrtest.have_sfcgal():
        geom = ogr.CreateGeometryFromWkb(wkb_string)
        assert ps.Contains(geom), "Failure in Contains() of PolyhedralSurface"

    assert not ps.IsEmpty(), "Failure in IsEmpty() of PolyhedralSurface"

    ps.Empty()
    wkt_string = ps.ExportToWkt()
    assert wkt_string == 'POLYHEDRALSURFACE Z EMPTY', \
        "Failure in Empty() of PolyhedralSurface"

    g = ogr.CreateGeometryFromWkt('POLYHEDRALSURFACE (((0 0 0,0 0 1,0 1 1,0 1 0,0 0 0)))')
    assert g.Equals(g) != 0

    for wkt in ['MULTIPOLYGON (((0 0 0,0 0 1,0 1 1,0 1 0,0 0 0)))',
                'POLYHEDRALSURFACE (((0 0 0,0 0 1,0 1 1,0 0 0)))',
                'POLYHEDRALSURFACE (((0 0 0,0 0 1,0 1 1,0 0 0)),((0 0 0,0 0 1,0 1 1,0 0 0)))',
                'POLYHEDRALSURFACE EMPTY']:
        g2 = ogr.CreateGeometryFromWkt(wkt)
        if g.Equals(g2):
            print(wkt)
            pytest.fail("Unexpected true Equals() return")

    # Error
    assert g.AddGeometry(ogr.CreateGeometryFromWkt('POINT (0 0)')) != 0

    # Error
    assert g.AddGeometryDirectly(ogr.CreateGeometryFromWkt('POINT (0 0)')) != 0

    # Test dimension promotion
    g = ogr.CreateGeometryFromWkt('POLYHEDRALSURFACE EMPTY')
    g.AddGeometryDirectly(ogr.CreateGeometryFromWkt('POLYGON ZM ((0 0 1 2,0 1 1 2,1 1 1 2,0 0 1 2))'))
    g.AddGeometryDirectly(ogr.CreateGeometryFromWkt('POLYGON ((10 10,10 11,11 11,10 10))'))
    wkt = g.ExportToIsoWkt()
    assert wkt == 'POLYHEDRALSURFACE ZM (((0 0 1 2,0 1 1 2,1 1 1 2,0 0 1 2)),((10 10 0 0,10 11 0 0,11 11 0 0,10 10 0 0)))'

###############################################################################
# Test suite for TIN


def test_ogr_geom_tin():
    poly1 = ogr.CreateGeometryFromWkt("TRIANGLE ((0 0 0,0 0 1,0 1 0,0 0 0))")
    poly2 = ogr.CreateGeometryFromWkt("TRIANGLE ((0 0 0,0 1 0,1 1 0,0 0 0))")
    tin = ogr.Geometry(ogr.wkbTIN)
    tin.AddGeometry(poly1)
    tin.AddGeometry(poly2)
    polygon_wkt = [poly1.ExportToWkt(), poly2.ExportToWkt()]
    for i in range(0, tin.GetGeometryCount()):
        geom = tin.GetGeometryRef(i)
        wkt_geom = geom.ExportToWkt()
        assert polygon_wkt[i] == wkt_geom, "Failure in getting geometries of TIN"

    wkt_original = 'TIN Z (((0 0 0,0 0 1,0 1 0,0 0 0)),((0 0 0,0 1 0,1 1 0,0 0 0)))'
    tin = ogr.CreateGeometryFromWkt(wkt_original)

    wkb_string = tin.ExportToWkb(ogr.wkbXDR)
    geom = ogr.CreateGeometryFromWkb(wkb_string)
    wkt_string = geom.ExportToWkt()
    assert wkt_string == wkt_original, "Failure in Wkb methods of TIN"

    wkt_string = geom.Clone().ExportToWkt()
    assert wkt_string == wkt_original, "Failure in Clone()"

    if ogrtest.have_sfcgal():
        area = 12.3 * tin.Area()
        assert area == 12.3, "Wrong area of TIN"

    size = tin.WkbSize()
    assert size == 227, "Wrong WkbSize() of TIN"

    # geom = tin.DelaunayTriangulation(0.0,True)
    # wkt_geom_dt = 'MULTILINESTRING ((0 1 0,1 1 0),(0 0 0,0 1 0),(0 0 0,1 1 0))'
    # wkt_geom = geom.ExportToWkt()
    # if wkt_geom != wkt_geom_dt:
    #    gdaltest.post_reason ("Failure in DelaunayTriangulation() of TIN")
    #    print(wkt_geom)
    #    return 'fail'

    # geom = ogr.CreateGeometryFromWkb(wkb_string)
    # if tin.Contains(geom) != True:
    #    gdaltest.post_reason ("Failure in Contains() of TIN")
    #    return 'fail'

    assert not tin.IsEmpty(), "Failure in IsEmpty() of TIN"

    tin.Empty()
    wkt_string = tin.ExportToWkt()
    assert wkt_string == 'TIN Z EMPTY', "Failure in Empty() of TIN"

    wrong_polygon = ogr.CreateGeometryFromWkt('POLYGON ((0 0 0,0 1 0,1 1 0,0 0 1))')
    geom_count = tin.GetGeometryCount()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    x = tin.AddGeometry(wrong_polygon)
    gdal.PopErrorHandler()
    assert tin.GetGeometryCount() == geom_count, \
        ("Added wrong geometry in TIN, error has code " + str(x))

    if ogrtest.have_geos() or ogrtest.have_sfcgal():
        point = tin.PointOnSurface()
        point_wkt = point.ExportToWkt()
        point_correct_wkt = 'POINT EMPTY'
        assert point_wkt == point_correct_wkt, \
            "Wrong Point Obtained for PointOnSurface() in TIN"

    tin = ogr.CreateGeometryFromWkt(wkt_original)
    # point = tin.PointOnSurface()
    # point_wkt = point.ExportToWkt()
    # point_correct_wkt = 'POINT (0.25 0.5)'
    # if point_wkt != point_correct_wkt:
    #    gdaltest.post_reason ("Wrong Point Obtained for PointOnSurface() in TIN")
    #    print(point_wkt)
    #    return 'fail'

    tin.FlattenTo2D()
    assert not tin.IsValid(), "Problem with IsValid() in TIN"

    # 4 points
    invalid_wkt = 'TIN (((0 0,0 1,1 1,1 0,0 0)))'
    with gdaltest.error_handler():
        g = ogr.CreateGeometryFromWkt(invalid_wkt)
    assert g is None

    # hole
    invalid_wkt = 'TIN(((0 0,0 1,1 1,0 0),(0.1 0.1,0.1 0.2,0.2 0.2,0.1 0.1)))'
    with gdaltest.error_handler():
        g = ogr.CreateGeometryFromWkt(invalid_wkt)
    assert g is None

    invalid_wkt = 'TIN (POLYGON((0 0,0 1,1 1,0 0)))'
    with gdaltest.error_handler():
        g = ogr.CreateGeometryFromWkt(invalid_wkt)
    assert g is None

    # Add a POLYGON that can be cast as a TRIANGLE
    g = ogr.Geometry(ogr.wkbTIN)
    assert g.AddGeometry(ogr.CreateGeometryFromWkt('POLYGON ((0 0,0 1,1 1,0 0))')) == 0
    assert g.ExportToIsoWkt() == 'TIN (((0 0,0 1,1 1,0 0)))'

###############################################################################
# Test OGRGeometry::getBoundary() result for point.


def test_ogr_geom_boundary_point():

    if not ogrtest.have_geos():
        pytest.skip()

    geom_wkt = 'POINT(1 1)'
    geom = ogr.CreateGeometryFromWkt(geom_wkt)

    bnd = geom.GetBoundary()
    assert bnd.GetGeometryType() == ogr.wkbGeometryCollection, \
        'GetBoundary not reported as GEOMETRYCOLLECTION EMPTY'

    bnd = geom.Boundary()
    assert bnd.GetGeometryType() == ogr.wkbGeometryCollection, \
        'Boundary not reported as GEOMETRYCOLLECTION EMPTY'

###############################################################################
# Test OGRGeometry::getBoundary() result for multipoint.


def test_ogr_geom_boundary_multipoint():

    if not ogrtest.have_geos():
        pytest.skip()

    geom_wkt = 'MULTIPOINT((0 0),(1 1))'
    geom = ogr.CreateGeometryFromWkt(geom_wkt)

    bnd = geom.GetBoundary()
    assert bnd.GetGeometryType() == ogr.wkbGeometryCollection, \
        'Boundary not reported as GEOMETRYCOLLECTION EMPTY'

###############################################################################
# Test OGRGeometry::getBoundary() result for linestring.


def test_ogr_geom_boundary_linestring():

    if not ogrtest.have_geos():
        pytest.skip()

    geom_wkt = 'LINESTRING(0 0, 1 1, 2 2, 3 2, 4 2)'
    geom = ogr.CreateGeometryFromWkt(geom_wkt)

    bnd = geom.GetBoundary()
    assert bnd.GetGeometryType() == ogr.wkbMultiPoint, \
        'Boundary not reported as MULTIPOINT'

    assert bnd.GetGeometryCount() == 2, \
        'Boundary not reported as MULTIPOINT consisting of 2 points'

    geom_wkt = 'LINESTRING(0 0, 1 0, 1 1, 0 1, 0 0)'
    geom = ogr.CreateGeometryFromWkt(geom_wkt)

    bnd = geom.GetBoundary()
    assert bnd.GetGeometryType() == ogr.wkbMultiPoint, \
        'Boundary not reported as MULTIPOINT'

    assert bnd.GetGeometryCount() == 0, 'Boundary not reported as MULTIPOINT EMPTY'

###############################################################################
# Test OGRGeometry::getBoundary() result for polygon.


def test_ogr_geom_boundary_polygon():

    if not ogrtest.have_geos():
        pytest.skip()

    geom_wkt = 'POLYGON((0 0,1 1,1 0,0 0))'
    geom = ogr.CreateGeometryFromWkt(geom_wkt)

    bnd = geom.GetBoundary()
    assert bnd.GetGeometryType() == ogr.wkbLineString, \
        'Boundary not reported as non-empty LINESTRING'

###############################################################################
# Test OGRBuildPolygonFromEdges() on a geometry collection of line strings


def test_ogr_geom_build_from_edges_1():

    if not ogrtest.have_geos():
        pytest.skip()

    link_coll = ogr.Geometry(type=ogr.wkbGeometryCollection)

    wkt_array = [
        'LINESTRING (-87.601595 30.999522,-87.599623 31.000059,-87.599219 31.00017)',
        'LINESTRING (-87.601595 30.999522,-87.604349 30.999493,-87.606935 30.99952)',
        'LINESTRING (-87.59966 31.000756,-87.599851 31.000805,-87.599992 31.000805,-87.600215 31.000761,-87.600279 31.000723,-87.600586 31.000624,-87.601256 31.000508,-87.602501 31.000447,-87.602801 31.000469,-87.603108 31.000579,-87.603331 31.000716,-87.603523 31.000909,-87.603766 31.001233,-87.603913 31.00136)',
        'LINESTRING (-87.606134 31.000182,-87.605885 31.000325,-87.605343 31.000716,-87.60466 31.001117,-87.604468 31.0012,-87.603913 31.00136)',
        'LINESTRING (-87.599219 31.00017,-87.599289 31.0003,-87.599398 31.000426,-87.599564 31.000547,-87.599609 31.000701,-87.59966 31.000756)',
        'LINESTRING (-87.606935 30.99952,-87.606713 30.999799,-87.6064 30.999981,-87.606134 31.000182)']

    for wkt in wkt_array:
        geom = ogr.CreateGeometryFromWkt(wkt)
        # print "geom is",geom
        link_coll.AddGeometry(geom)

    try:
        poly = ogr.BuildPolygonFromEdges(link_coll)
        assert poly is not None
    except:
        pytest.fail()

    
###############################################################################
# Test OGRBuildPolygonFromEdges() on a multilinestring


def test_ogr_geom_build_from_edges_2():

    if not ogrtest.have_geos():
        pytest.skip()

    link_coll = ogr.Geometry(type=ogr.wkbMultiLineString)

    wkt_array = [
        'LINESTRING (-87.601595 30.999522,-87.599623 31.000059,-87.599219 31.00017)',
        'LINESTRING (-87.601595 30.999522,-87.604349 30.999493,-87.606935 30.99952)',
        'LINESTRING (-87.59966 31.000756,-87.599851 31.000805,-87.599992 31.000805,-87.600215 31.000761,-87.600279 31.000723,-87.600586 31.000624,-87.601256 31.000508,-87.602501 31.000447,-87.602801 31.000469,-87.603108 31.000579,-87.603331 31.000716,-87.603523 31.000909,-87.603766 31.001233,-87.603913 31.00136)',
        'LINESTRING (-87.606134 31.000182,-87.605885 31.000325,-87.605343 31.000716,-87.60466 31.001117,-87.604468 31.0012,-87.603913 31.00136)',
        'LINESTRING (-87.599219 31.00017,-87.599289 31.0003,-87.599398 31.000426,-87.599564 31.000547,-87.599609 31.000701,-87.59966 31.000756)',
        'LINESTRING (-87.606935 30.99952,-87.606713 30.999799,-87.6064 30.999981,-87.606134 31.000182)']

    for wkt in wkt_array:
        geom = ogr.CreateGeometryFromWkt(wkt)
        link_coll.AddGeometry(geom)

    try:
        poly = ogr.BuildPolygonFromEdges(link_coll)
        assert poly is not None
    except:
        pytest.fail()

    
###############################################################################
# Test OGRBuildPolygonFromEdges() on invalid geometries


def test_ogr_geom_build_from_edges_3():

    if not ogrtest.have_geos():
        pytest.skip()

    src_geom = ogr.CreateGeometryFromWkt('POINT (0 1)')
    try:
        gdal.PushErrorHandler('CPLQuietErrorHandler')
        poly = ogr.BuildPolygonFromEdges(src_geom)
        gdal.PopErrorHandler()
        assert poly is None
    except:
        pass

    src_geom = ogr.CreateGeometryFromWkt('GEOMETRYCOLLECTION (LINESTRING(0 1,2 3),POINT(0 1),LINESTRING(0 1,-2 3),LINESTRING(-2 3,2 3))')
    try:
        gdal.PushErrorHandler('CPLQuietErrorHandler')
        poly = ogr.BuildPolygonFromEdges(src_geom)
        gdal.PopErrorHandler()
        assert poly is None
    except:
        pass

    
###############################################################################
# Test OGRBuildPolygonFromEdges() and identify exterior ring (#3610)


def test_ogr_geom_build_from_edges_4():

    if int(gdal.VersionInfo('VERSION_NUM')) < 1900:
        pytest.skip('would crash')

    if not ogrtest.have_geos():
        pytest.skip()

    link_coll = ogr.Geometry(type=ogr.wkbGeometryCollection)

    wkt_array = [
        'LINESTRING EMPTY',
        'LINESTRING (1 1,1 2)',
        'LINESTRING EMPTY',
        'LINESTRING (1 2,2 2)',
        'LINESTRING (2 2,2 1)',
        'LINESTRING (2 1,1 1)',
        'LINESTRING (0 0,0 10)',
        'LINESTRING (0 10,10 10)',
        'LINESTRING (10 10,10 0)',
        'LINESTRING (10 0,0 0)'
    ]

    for wkt in wkt_array:
        geom = ogr.CreateGeometryFromWkt(wkt)
        # print "geom is",geom
        link_coll.AddGeometry(geom)

    try:
        poly = ogr.BuildPolygonFromEdges(link_coll)
        assert poly is not None
        wkt = poly.ExportToWkt()
        assert wkt == 'POLYGON ((0 0,0 10,10 10,10 0,0 0),(1 1,1 2,2 2,2 1,1 1))'
    except:
        pytest.fail()

    
###############################################################################
# Test GetArea() on empty linear ring (#2792)


def test_ogr_geom_area_empty_linearring():

    geom = ogr.Geometry(type=ogr.wkbLinearRing)

    area = geom.GetArea()
    assert area == 0

###############################################################################
# Test TransformTo()


def test_ogr_geom_transform_to():

    # Somewhere in Paris suburbs...
    geom = ogr.CreateGeometryFromWkt('POINT(2 49)')

    # Input SRS is EPSG:4326
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4326)
    sr.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)
    geom.AssignSpatialReference(sr)

    # Output SRS is EPSG:32631
    sr2 = osr.SpatialReference()
    sr2.ImportFromEPSG(32631)
    ret = geom.TransformTo(sr2)

    assert ret == 0 and geom.GetX() == pytest.approx(426857, abs=1) and geom.GetY() == pytest.approx(5427937, abs=1), \
        geom.ExportToWkt()

    # Geometry without SRS
    geom = ogr.CreateGeometryFromWkt('POINT(2 49)')
    gdal.ErrorReset()
    with gdaltest.error_handler():
        ret = geom.TransformTo(sr2)
    assert not (ret == 0 or gdal.GetLastErrorMsg() == '')

###############################################################################
# Test Transform()


def test_ogr_geom_transform():

    # Somewhere in Paris suburbs...
    geom = ogr.CreateGeometryFromWkt('POINT(2 49)')

    # Input SRS is EPSG:4326
    sr = osr.SpatialReference()
    sr.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)
    sr.ImportFromEPSG(4326)

    # Output SRS is EPSG:32631
    sr2 = osr.SpatialReference()
    sr2.ImportFromEPSG(32631)

    ct = osr.CoordinateTransformation(sr, sr2)

    geom.Transform(ct)

    assert geom.GetX() == pytest.approx(426857, abs=1) and geom.GetY() == pytest.approx(5427937, abs=1), \
        geom.ExportToWkt()

###############################################################################
# Test ogr.GeomTransformer()


def test_ogr_geomtransfomer_default():

    if not ogrtest.have_geos():
        pytest.skip()

    src = osr.SpatialReference()
    src.ImportFromEPSG(32660)

    dst = osr.SpatialReference()
    dst.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)
    dst.ImportFromEPSG(4326)

    ct = osr.CoordinateTransformation(src, dst)
    geom = ogr.CreateGeometryFromWkt('LINESTRING(832864.275023695 0,835092.849076364 0)')
    transformer = ogr.GeomTransformer(ct)

    geom_dst = transformer.Transform(geom)
    assert geom_dst.ExportToWkt() == 'MULTILINESTRING ((179.99 0.0,180 0),(-180 0,-179.99 0.0))'

###############################################################################
# Test ogr.GeomTransformer()


def test_ogr_geomtransfomer_wrapdateline_with_ct():

    if not ogrtest.have_geos():
        pytest.skip()

    src = osr.SpatialReference()
    src.ImportFromEPSG(3857)

    dst = osr.SpatialReference()
    dst.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)
    dst.ImportFromEPSG(4326)

    ct = osr.CoordinateTransformation(src, dst)
    geom = ogr.CreateGeometryFromWkt('LINESTRING(20026376.3937099 0,-20026376.3937099 0)')
    transformer = ogr.GeomTransformer(ct, ['WRAPDATELINE=YES'])

    geom_dst = geom.Transform(transformer)
    assert ogrtest.check_feature_geometry(geom_dst, 'MULTILINESTRING ((179.9 0.0,180 0),(-180 0,-179.9 0.0))') == 0

###############################################################################
# Test ogr.GeomTransformer()


def test_ogr_geomtransfomer_wrapdateline_no_ct():

    if not ogrtest.have_geos():
        pytest.skip()
    geom = ogr.CreateGeometryFromWkt('LINESTRING(-179 0,179 0)')
    transformer = ogr.GeomTransformer(None, ['WRAPDATELINE=YES'])

    geom_dst = geom.Transform(transformer)
    assert geom_dst.ExportToWkt() == 'MULTILINESTRING ((-179 0,-180 0),(180 0,179 0))'


###############################################################################
# Test CloseRings()


def test_ogr_geom_closerings():

    geom = ogr.CreateGeometryFromWkt('POLYGON((0 0,0 1,1 1,1 0))')
    geom.CloseRings()

    assert geom.ExportToWkt() == 'POLYGON ((0 0,0 1,1 1,1 0,0 0))'

    geom.CloseRings()
    assert geom.ExportToWkt() == 'POLYGON ((0 0,0 1,1 1,1 0,0 0))'

###############################################################################
# Test Segmentize()


def test_ogr_geom_segmentize():

    # 2D
    geom = ogr.CreateGeometryFromWkt('LINESTRING(0 0,0 10)')
    geom.Segmentize(1.00001)

    assert geom.ExportToWkt() == 'LINESTRING (0 0,0 1,0 2,0 3,0 4,0 5,0 6,0 7,0 8,0 9,0 10)'

    # 2D + Z
    geom = ogr.CreateGeometryFromWkt('LINESTRING(0 0 1,0 10 1)')
    geom.Segmentize(1.00001)

    assert geom.ExportToWkt() == 'LINESTRING (0 0 1,0 1 1,0 2 1,0 3 1,0 4 1,0 5 1,0 6 1,0 7 1,0 8 1,0 9 1,0 10 1)'

    # 2D + M
    geom = ogr.CreateGeometryFromWkt('LINESTRING M(0 0 1,0 10 2)')
    geom.Segmentize(5)
    assert geom.ExportToIsoWkt() == 'LINESTRING M (0 0 1,0 5 2,0 10 2)'

    # 2D + ZM
    geom = ogr.CreateGeometryFromWkt('LINESTRING ZM(0 0 1 2,0 10 1 2)')
    geom.Segmentize(5)
    assert geom.ExportToIsoWkt() == 'LINESTRING ZM (0 0 1 2,0 5 1 2,0 10 1 2)'

    # Test distance between points <<<< segmentization threshold
    # https://github.com/OSGeo/gdal/issues/1414
    geom = ogr.CreateGeometryFromWkt('LINESTRING(0 0,0 1)')
    geom.Segmentize(10000)
    assert geom.ExportToWkt() == 'LINESTRING (0 0,0 1)'

    # Check segmentize symmetry : do exact binary comparison.
    in_wkt = 'LINESTRING (0 0,1.2 1,2 0)'
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    g1.Segmentize(0.25)
    in_wkt = 'LINESTRING (2 0,1.2 1,0 0)'
    g2 = ogr.CreateGeometryFromWkt(in_wkt)
    g2.Segmentize(0.25)
    for i in range(g1.GetPointCount()):
        if g1.GetPoint(i) != g2.GetPoint(g1.GetPointCount() - 1 - i):
            print('%.18g' % (g1.GetPoint(i)[0] - g2.GetPoint(g1.GetPointCount() - 1 - i)[0]))
            pytest.fail('%.18g' % (g1.GetPoint(i)[1] - g2.GetPoint(g1.GetPointCount() - 1 - i)[1]))


def test_ogr_geom_segmentize_issue_1341():

    expected_geom = 'LINESTRING (0 0,0.4 0.0,0.8 0.0,1.2 0.0,1.6 0.0,2 0,2.4 0.0,2.8 0.0,3.2 0.0,3.6 0.0,4 0,4.4 0.0,4.8 0.0,5.2 0.0,5.6 0.0,6 0,6.4 0.0,6.8 0.0,7.2 0.0,7.6 0.0,8 0,8.4 0.0,8.8 0.0,9.2 0.0,9.6 0.0,10 0)'

    geom = ogr.CreateGeometryFromWkt('LINESTRING(0 0,10 0)')
    geom.Segmentize(0.399999999999)
    assert geom.ExportToWkt() == expected_geom
    geom.Segmentize(0.399999999999)
    assert geom.ExportToWkt() == expected_geom

    geom = ogr.CreateGeometryFromWkt('LINESTRING(0 0,10 0)')
    geom.Segmentize(0.4)
    assert geom.ExportToWkt() == expected_geom
    geom.Segmentize(0.4)
    assert geom.ExportToWkt() == expected_geom

    geom = ogr.CreateGeometryFromWkt('LINESTRING(0 0,10 0)')
    geom.Segmentize(0.400000000001)
    assert geom.ExportToWkt() == expected_geom
    geom.Segmentize(0.400000000001)
    assert geom.ExportToWkt() == expected_geom

###############################################################################
# Test Value()


def test_ogr_geom_value():

    geom = ogr.CreateGeometryFromWkt('LINESTRING(2 3,5 3,5 0)')

    p = geom.Value(-1e-3)
    expected_p = ogr.CreateGeometryFromWkt('POINT (2 3)')
    assert ogrtest.check_feature_geometry(p, expected_p) == 0

    p = geom.Value(geom.Length() / 4)
    expected_p = ogr.CreateGeometryFromWkt('POINT (3.5 3)')
    assert ogrtest.check_feature_geometry(p, expected_p) == 0

    p = geom.Value(geom.Length() / 2)
    expected_p = ogr.CreateGeometryFromWkt('POINT (5 3)')
    assert ogrtest.check_feature_geometry(p, expected_p) == 0

    p = geom.Value(3 * geom.Length() / 4)
    expected_p = ogr.CreateGeometryFromWkt('POINT (5 1.5)')
    assert ogrtest.check_feature_geometry(p, expected_p) == 0

    p = geom.Value(geom.Length() + 1e-3)
    expected_p = ogr.CreateGeometryFromWkt('POINT (5 0)')
    assert ogrtest.check_feature_geometry(p, expected_p) == 0

###############################################################################
# Test FlattenTo2D(), GetDimension() and GetCoordinateDimension()


def test_ogr_geom_flattenTo2D():

    geom = ogr.CreateGeometryFromWkt('POINT (1 2 3)')

    # Point is 0 dimension, LineString 1, ...
    assert geom.GetDimension() == 0

    assert geom.GetCoordinateDimension() == 3

    geom.FlattenTo2D()
    assert geom.GetCoordinateDimension() == 2

    assert geom.ExportToWkt() == 'POINT (1 2)'

###############################################################################
# Test FlattenTo2D(), GetDimension() and GetCoordinateDimension() for Triangle


def test_ogr_geom_flattenTo2D_triangle():

    geom = ogr.CreateGeometryFromWkt('TRIANGLE ((0 0 0,100 0 100,0 100 100,0 0 0))')

    # Point is 0 dimension, LineString 1, ...
    assert geom.GetDimension() == 2

    assert geom.GetCoordinateDimension() == 3

    geom.FlattenTo2D()
    assert geom.GetCoordinateDimension() == 2

    assert geom.ExportToWkt() == 'TRIANGLE ((0 0,100 0,0 100,0 0))'

###############################################################################


def test_ogr_geom_linestring_limits():

    geom = ogr.CreateGeometryFromWkt('LINESTRING EMPTY')
    assert geom.Length() == 0

    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    geom.GetPoint(-1)
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorType() != 0

    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    geom.GetPoint(0)
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorType() != 0

    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    geom.GetPoint_2D(-1)
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorType() != 0

    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    geom.GetPoint_2D(0)
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorType() != 0

    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    geom.SetPoint(-1, 5, 6, 7)
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorType() != 0

    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    geom.SetPoint_2D(-1, 5, 6)
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorType() != 0

    gdal.ErrorReset()
    with gdaltest.error_handler():
        geom.SetPoint(2147000000, 5, 6, 7)
    assert gdal.GetLastErrorType() != 0

    gdal.ErrorReset()
    with gdaltest.error_handler():
        geom.SetPoint_2D(2147000000, 5, 6)
    assert gdal.GetLastErrorType() != 0

    geom = ogr.CreateGeometryFromWkt('LINESTRING(0 0)')
    assert geom.Length() == 0
    geom = ogr.CreateGeometryFromWkt('LINESTRING(0 0, 1 0)')
    assert geom.Length() == 1

###############################################################################


def test_ogr_geom_coord_round():

    geom = ogr.CreateGeometryFromWkt('POINT(370441.860 5591000.590)')
    wkt = geom.ExportToWkt()
    assert wkt == 'POINT (370441.86 5591000.59)', 'did not get expected WKT'

###############################################################################


def test_ogr_geom_coord_round_2():

    geom = ogr.CreateGeometryFromWkt('POINT(1.0 169.600374)')
    wkt = geom.ExportToWkt()
    assert wkt == 'POINT (1.0 169.600374)', 'did not get expected WKT'

###############################################################################
# Test Area calculation for a Point


def test_ogr_geom_area_point():

    geom_wkt = 'POINT(0 0)'
    geom = ogr.CreateGeometryFromWkt(geom_wkt)

    with gdaltest.error_handler():
        area = geom.Area()
    assert area == 0, ('Area() result wrong, got %g.' % area)

###############################################################################
# Test Length calculation for a Point


def test_ogr_geom_length_point():

    # OGR >= 1.8.0
    geom_wkt = 'POINT(0 0)'
    geom = ogr.CreateGeometryFromWkt(geom_wkt)

    with gdaltest.error_handler():
        length = geom.Length()
    assert length == 0, ('Length() result wrong, got %g.' % length)

###############################################################################
# Test Length calculation for a MultiLineString


def test_ogr_geom_length_multilinestring():

    # OGR >= 1.8.0
    geom_wkt = 'MULTILINESTRING((0 0,0 1),(0 0,0 1))'
    geom = ogr.CreateGeometryFromWkt(geom_wkt)

    length = geom.Length()
    assert length == pytest.approx(2, abs=0.00000000001), ('Length() result wrong, got %g.' % length)

###############################################################################
# Test Length calculation for a GeometryCollection


def test_ogr_geom_length_geometrycollection():

    # OGR >= 1.8.0
    geom_wkt = 'GEOMETRYCOLLECTION( POLYGON((0 0,0 1,1 1,1 0,0 0)), MULTILINESTRING((0 0,0 1),(0 0,0 1)), LINESTRING(0 0,0 1), LINESTRING(0 0,0 1), POINT(0 0), GEOMETRYCOLLECTION EMPTY )'
    geom = ogr.CreateGeometryFromWkt(geom_wkt)

    length = geom.Length()
    assert length == pytest.approx(4, abs=0.00000000001), ('Length() result wrong, got %g.' % length)

###############################################################################
# Test Geometry.GetPoints() (#4016)


def test_ogr_geom_getpoints():

    geom = ogr.CreateGeometryFromWkt('LINESTRING(0 1,2 3)')
    points = geom.GetPoints()
    assert points == [(0.0, 1.0), (2.0, 3.0)], 'did not get expected points (1)'
    points = geom.GetPoints(nCoordDimension=3)
    assert points == [(0.0, 1.0, 0.0), (2.0, 3.0, 0.0)], \
        'did not get expected points (2)'

    geom = ogr.CreateGeometryFromWkt('LINESTRING(0 1 2,3 4 5)')
    points = geom.GetPoints()
    assert points == [(0.0, 1.0, 2.0), (3.0, 4.0, 5.0)], \
        'did not get expected points (3)'
    points = geom.GetPoints(nCoordDimension=2)
    assert points == [(0.0, 1.0), (3.0, 4.0)], 'did not get expected points (4)'

    geom = ogr.CreateGeometryFromWkt('POINT(0 1)')
    points = geom.GetPoints()
    assert points == [(0.0, 1.0)], 'did not get expected points (5)'
    points = geom.GetPoints(nCoordDimension=3)
    assert points == [(0.0, 1.0, 0.0)], 'did not get expected points (6)'

    geom = ogr.CreateGeometryFromWkt('POINT(0 1 2)')
    points = geom.GetPoints()
    assert points == [(0.0, 1.0, 2.0)], 'did not get expected points (7)'
    points = geom.GetPoints(nCoordDimension=2)
    assert points == [(0.0, 1.0)], 'did not get expected points (8)'

###############################################################################
# Test OGRGeometry::empty()


def test_ogr_geom_empty():

    g1 = ogr.CreateGeometryFromWkt('POLYGON((0 0,1 1,1 2,1 1,0 0))')
    g1.Empty()
    wkt = g1.ExportToWkt()

    assert wkt == 'POLYGON EMPTY'

###############################################################################
# Test OGRGeometry::empty() for Triangle


def test_ogr_geom_empty_triangle():

    g1 = ogr.CreateGeometryFromWkt('TRIANGLE ((0 0,100 0,0 100,0 0))')
    g1.Empty()
    wkt = g1.ExportToWkt()

    assert wkt == 'TRIANGLE EMPTY'

###############################################################################
# Test parsing WKT made of 2D and 3D parts


def test_ogr_geom_mixed_coordinate_dimension():

    # first part is 3D, second part is 2D of same length
    wkt = 'MULTIPOLYGON (((1 2 -4,1 3 -3,2 3 -3,2 2 -3,1 2 -6)),((-1 -2,-1 -3,-2 -3,-2 -2,-1 -2,50 60)))'
    expected_wkt = 'MULTIPOLYGON (((1 2 -4,1 3 -3,2 3 -3,2 2 -3,1 2 -6)),((-1 -2 0,-1 -3 0,-2 -3 0,-2 -2 0,-1 -2 0,50 60 0)))'
    g = ogr.CreateGeometryFromWkt(wkt)
    got_wkt = g.ExportToWkt()
    assert got_wkt == expected_wkt, \
        ('for %s: got %s, expected %s' % (wkt, got_wkt, expected_wkt))

    # first part is 3D, second part is 2D of same length, except the last point which is 3D
    wkt = 'MULTIPOLYGON (((1 2 -4,1 3 -3,2 3 -3,2 2 -3,1 2 -6)),((-1 -2,-1 -3,-2 -3,-2 -2,-1 -2,50 60 7)))'
    expected_wkt = 'MULTIPOLYGON (((1 2 -4,1 3 -3,2 3 -3,2 2 -3,1 2 -6)),((-1 -2 0,-1 -3 0,-2 -3 0,-2 -2 0,-1 -2 0,50 60 7)))'
    g = ogr.CreateGeometryFromWkt(wkt)
    got_wkt = g.ExportToWkt()
    assert got_wkt == expected_wkt, \
        ('for %s: got %s, expected %s' % (wkt, got_wkt, expected_wkt))

###############################################################################
# Test GetEnvelope3D()


def test_ogr_geom_getenvelope3d():

    g = ogr.CreateGeometryFromWkt('POINT EMPTY')
    envelope = g.GetEnvelope3D()
    expected_envelope = (0.0, 0.0, 0.0, 0.0, 0.0, 0.0)
    assert envelope == expected_envelope, 'did not get expected envelope (1)'

    g = ogr.CreateGeometryFromWkt('POINT(1 2)')
    envelope = g.GetEnvelope3D()
    expected_envelope = (1.0, 1.0, 2.0, 2.0, 0.0, 0.0)
    assert envelope == expected_envelope, 'did not get expected envelope (2)'

    g = ogr.CreateGeometryFromWkt('POINT(1 2 3)')
    envelope = g.GetEnvelope3D()
    expected_envelope = (1.0, 1.0, 2.0, 2.0, 3.0, 3.0)
    assert envelope == expected_envelope, 'did not get expected envelope (3)'

    g = ogr.CreateGeometryFromWkt('POLYGON EMPTY')
    envelope = g.GetEnvelope3D()
    expected_envelope = (0.0, 0.0, 0.0, 0.0, 0.0, 0.0)
    assert envelope == expected_envelope, 'did not get expected envelope (4)'

    g = ogr.CreateGeometryFromWkt('LINESTRING(1 2,3 4)')
    envelope = g.GetEnvelope3D()
    expected_envelope = (1, 3, 2, 4, 0, 0)
    assert envelope == expected_envelope, 'did not get expected envelope (5)'

    g = ogr.CreateGeometryFromWkt('MULTIPOLYGON(((1 2 3,-1 2 3,-1 -2 3,-1 2 3,1 2 3),(0.1 0.2 0.3,-0.1 0.2 0.3,-0.1 -0.2 0.3,-0.1 0.2 0.3,0.1 0.2 0.3)),((10 20 -30,-10 20 -30,-10 -20 -30,-10 20 -30,10 20 -30)))')
    envelope = g.GetEnvelope3D()
    expected_envelope = (-10, 10, -20, 20, -30, 3.0)
    assert envelope == expected_envelope, 'did not get expected envelope (6)'

    g = ogr.CreateGeometryFromWkt('MULTIPOLYGON EMPTY')
    envelope = g.GetEnvelope3D()
    expected_envelope = (0.0, 0.0, 0.0, 0.0, 0.0, 0.0)
    assert envelope == expected_envelope, 'did not get expected envelope (7)'

    g = ogr.CreateGeometryFromWkt('TRIANGLE ((0 0 0,100 0 100,0 100 100,0 0 0))')
    envelope = g.GetEnvelope3D()
    expected_envelope = (0.0, 100.0, 0.0, 100.0, 0.0, 100.0)
    assert envelope == expected_envelope, 'did not get expected envelope (8)'

###############################################################################
# Test importing/exporting XXX Z EMPTY


def test_ogr_geom_z_empty():

    for geom in ['POINT', 'LINESTRING', 'POLYGON', 'MULTIPOINT', 'MULTILINESTRING',
                 'MULTIPOLYGON', 'GEOMETRYCOLLECTION', 'CIRCULARSTRING', 'COMPOUNDCURVE',
                 'CURVEPOLYGON', 'MULTICURVE', 'MULTISURFACE']:
        in_wkt = geom + ' Z EMPTY'
        g1 = ogr.CreateGeometryFromWkt(in_wkt)
        out_wkt = g1.ExportToIsoWkt()
        assert in_wkt == out_wkt

        g2 = ogr.CreateGeometryFromWkb(g1.ExportToIsoWkb())
        out_wkt = g2.ExportToIsoWkt()
        assert in_wkt == out_wkt

    
###############################################################################
# Test HasCurveGeometry and GetLinearGeometry


def test_ogr_geom_getlineargeometry():

    for geom in ['POINT', 'LINESTRING', 'POLYGON', 'MULTIPOINT', 'MULTILINESTRING',
                 'MULTIPOLYGON', 'GEOMETRYCOLLECTION',
                 ('CIRCULARSTRING', 'LINESTRING'),
                 ('COMPOUNDCURVE', 'LINESTRING'),
                 ('CURVEPOLYGON', 'POLYGON'),
                 ('MULTICURVE', 'MULTILINESTRING'),
                 ('MULTISURFACE', 'MULTIPOLYGON')]:
        try:
            (geom_in, geom_out) = geom
        except:
            geom_in = geom_out = geom
        in_wkt = geom_in + ' EMPTY'
        g = ogr.CreateGeometryFromWkt(in_wkt)
        g2 = g.GetLinearGeometry()
        assert geom_in == geom_out or g.HasCurveGeometry()
        assert g2.ExportToWkt() == geom_out + ' EMPTY', g

    g = ogr.CreateGeometryFromWkt('GEOMETRYCOLLECTION(POINT (0 1))')
    assert not g.HasCurveGeometry()
    assert not g.HasCurveGeometry(True)

    g = ogr.CreateGeometryFromWkt('GEOMETRYCOLLECTION(CIRCULARSTRING (0 0,0 1,0 0))')
    assert g.HasCurveGeometry()
    assert g.HasCurveGeometry(True)

###############################################################################
# Test GetDimension()


def test_ogr_geom_getdimension():
    for (geom, dim) in [('POINT EMPTY', 0),
                        ('LINESTRING EMPTY', 1),
                        ('POLYGON EMPTY', 2),
                        ('MULTIPOINT EMPTY', 0),
                        ('MULTILINESTRING EMPTY', 1),
                        ('MULTIPOLYGON EMPTY', 2),
                        ('GEOMETRYCOLLECTION EMPTY', 0),
                        ('CIRCULARSTRING EMPTY', 1),
                        ('COMPOUNDCURVE EMPTY', 1),
                        ('CURVEPOLYGON EMPTY', 2),
                        ('MULTICURVE EMPTY', 1),
                        ('TRIANGLE EMPTY', 2),
                        ('MULTISURFACE EMPTY', 2),
                        ('POLYHEDRALSURFACE EMPTY', 2),
                        ('TIN EMPTY', 2)]:
        g = ogr.CreateGeometryFromWkt(geom)
        assert g.GetDimension() == dim

    g = ogr.CreateGeometryFromWkt('GEOMETRYCOLLECTION(LINESTRING EMPTY, POINT (0 1))')
    assert g.GetDimension() == 1

    g = ogr.CreateGeometryFromWkt('GEOMETRYCOLLECTION(POINT (0 1), LINESTRING EMPTY, POLYGON EMPTY)')
    assert g.GetDimension() == 2

###############################################################################
# Test triangle


def test_ogr_geom_triangle():

    wkt_original = 'TRIANGLE ((0 0,0 1,1 1,0 0))'
    geom = ogr.CreateGeometryFromWkt(wkt_original)

    wkb_string = geom.ExportToWkb(ogr.wkbXDR)
    geom = ogr.CreateGeometryFromWkb(wkb_string)
    wkt_string = geom.ExportToWkt()
    assert wkt_string == wkt_original, "Failure in Wkb methods of Triangle"

    wkt_string = geom.Clone().ExportToWkt()
    assert wkt_string == wkt_original, "Failure in Clone()"

    polygon_wkt = ogr.ForceTo(geom.Clone(), ogr.wkbPolygon).ExportToWkt()
    assert polygon_wkt == 'POLYGON ((0 0,0 1,1 1,0 0))'

    polygon_wkt = ogr.ForceTo(geom.Clone(), ogr.wkbMultiPolygon).ExportToWkt()
    assert polygon_wkt == 'MULTIPOLYGON (((0 0,0 1,1 1,0 0)))'

###############################################################################
# Test importing invalid triangle WKT


def test_ogr_geom_triangle_invalid_wkt():

    geom_wkt = 'TRIANGLE (0 0)'
    with gdaltest.error_handler():
        geom = ogr.CreateGeometryFromWkt(geom_wkt)
    assert geom is None

    geom_wkt = 'TRIANGLE ((0 0))'
    with gdaltest.error_handler():
        geom = ogr.CreateGeometryFromWkt(geom_wkt)
    assert geom is None

    geom_wkt = 'TRIANGLE ((0 0,0 1,1 1,1 0))'
    with gdaltest.error_handler():
        geom = ogr.CreateGeometryFromWkt(geom_wkt)
    assert geom is None

    geom_wkt = 'TRIANGLE ((0 0,0 1,1 1,1 0,0 0))'
    with gdaltest.error_handler():
        geom = ogr.CreateGeometryFromWkt(geom_wkt)
    assert geom is None

    geom_wkt = 'TRIANGLE ((0 0,0 1,1 1,0 0),(0 0,0 1,1 1,0 0))'
    with gdaltest.error_handler():
        geom = ogr.CreateGeometryFromWkt(geom_wkt)
    assert geom is None

###############################################################################
# Test OGRTriangle. Tests if the GEOS/SFCGAL methods are working


def test_ogr_geom_triangle_sfcgal():

    if not ogrtest.have_sfcgal():
        pytest.skip()

    g1 = ogr.CreateGeometryFromWkt('TRIANGLE ((0 0,100 0 100,0 100 100,0 0))')
    g2 = ogr.CreateGeometryFromWkt('TRIANGLE ((-1 -1,100 0 100,0 100 100,-1 -1))')
    assert g2.Intersects(g1)

    g1 = ogr.CreateGeometryFromWkt('TRIANGLE ((0 0,1 0,0 1,0 0))')
    g2 = ogr.CreateGeometryFromWkt('TRIANGLE ((0 0,1 0,1 1,0 0))')
    g3 = g1.Intersection(g2)
    g4 = ogr.CreateGeometryFromWkt('TRIANGLE ((0.5 0.5 0,0 0 0,1 0 0,0.5 0.5 0))')
    assert g4.Equals(g3)

###############################################################################
# Test OGRCircularString


def test_ogr_geom_circularstring():

    in_wkt = 'CIRCULARSTRING (0 0,1 1,1 -1)'

    # Test export/import Wkt
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    assert g1.GetGeometryType() == ogr.wkbCircularString
    out_wkt = g1.ExportToWkt()
    assert in_wkt == out_wkt

    if ogrtest.have_geos():
        assert g1.IsValid()

    # Test GetEnvelope()
    env = g1.GetEnvelope()
    expected_env = (0.0, 2.0, -1.0, 1.0)
    for i in range(4):
        assert env[i] == pytest.approx(expected_env[i], abs=1e-8)

    # Test Length()
    length = g1.Length()
    expected_length = 1.5 * math.pi
    assert length == pytest.approx(expected_length, abs=1e-8)

    # Test Value()
    p = g1.Value(-1)
    expected_p = ogr.CreateGeometryFromWkt('POINT (0 0)')
    assert ogrtest.check_feature_geometry(p, expected_p) == 0

    p = g1.Value(0)
    expected_p = ogr.CreateGeometryFromWkt('POINT (0 0)')
    assert ogrtest.check_feature_geometry(p, expected_p) == 0

    p = g1.Value(length / 6.0)
    expected_p = ogr.CreateGeometryFromWkt('POINT (0.292893218813453 0.707106781186548)')
    if ogrtest.check_feature_geometry(p, expected_p) != 0:
        print(p)

    p = g1.Value(length / 3.0)
    expected_p = ogr.CreateGeometryFromWkt('POINT (1 1)')
    assert ogrtest.check_feature_geometry(p, expected_p) == 0

    p = g1.Value(length / 2.0)
    expected_p = ogr.CreateGeometryFromWkt('POINT (1.707106781186547 0.707106781186547)')
    assert ogrtest.check_feature_geometry(p, expected_p) == 0

    p = g1.Value(2 * length / 3.0)
    expected_p = ogr.CreateGeometryFromWkt('POINT (2 0)')
    assert ogrtest.check_feature_geometry(p, expected_p) == 0

    p = g1.Value(length)
    expected_p = ogr.CreateGeometryFromWkt('POINT (1 -1)')
    assert ogrtest.check_feature_geometry(p, expected_p) == 0

    p = g1.Value(length + 1)
    expected_p = ogr.CreateGeometryFromWkt('POINT (1 -1)')
    assert ogrtest.check_feature_geometry(p, expected_p) == 0

    # Test export/import Wkb
    g2 = ogr.CreateGeometryFromWkb(g1.ExportToWkb())
    out_wkt = g2.ExportToWkt()
    assert in_wkt == out_wkt

    # With Z
    in_wkt = 'CIRCULARSTRING Z (0 0 10,1 1 20,2 0 30)'
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    assert g1.GetGeometryType() == ogr.wkbCircularStringZ
    out_wkt = g1.ExportToWkt()
    assert in_wkt == out_wkt

    wkb = g1.ExportToWkb()
    isowkb = g1.ExportToIsoWkb()
    assert wkb == isowkb
    g2 = ogr.CreateGeometryFromWkb(wkb)
    out_wkt = g2.ExportToWkt()
    assert in_wkt == out_wkt

    # Test stroking
    gdal.SetConfigOption('OGR_STROKE_CURVE', 'TRUE')
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', '45')
    in_wkt = 'CIRCULARSTRING (0 0,1 1,1 -1)'
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    gdal.SetConfigOption('OGR_STROKE_CURVE', 'FALSE')
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', None)

    expected_g = ogr.CreateGeometryFromWkt('LINESTRING (0 0,0.218168517531969 0.623489801858729,0.777479066043687 0.974927912181831,1.433883739117561 0.900968867902435,1.900968867902463 0.433883739117562,1.974927912181821 -0.222520933956316,1.623489801858719 -0.78183148246804,1 -1)')
    assert ogrtest.check_feature_geometry(g1, expected_g) == 0

    in_wkt = 'CIRCULARSTRING (0 0,1 1,1 -1)'
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    in_wkb = g1.ExportToWkb()

    gdal.SetConfigOption('OGR_STROKE_CURVE', 'TRUE')
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', '45')
    g2 = ogr.CreateGeometryFromWkb(in_wkb)
    gdal.SetConfigOption('OGR_STROKE_CURVE', 'FALSE')
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', None)

    assert ogrtest.check_feature_geometry(g2, expected_g) == 0

    # Test HasCurveGeometry
    assert g1.HasCurveGeometry()
    assert g1.HasCurveGeometry(True)

    # Test GetLinearGeometry
    g2 = g1.GetLinearGeometry(45)
    assert ogrtest.check_feature_geometry(g2, expected_g) == 0

    # Test ForceToLineString
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', '45')
    g2 = ogr.ForceToLineString(g1)
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', None)
    assert ogrtest.check_feature_geometry(g2, expected_g) == 0

    # Test ForceToMultiLineString
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', '45')
    expected_g2 = ogr.CreateGeometryFromWkt('MULTILINESTRING ((0 0,0.218168517531969 0.623489801858729,0.777479066043687 0.974927912181831,1.433883739117561 0.900968867902435,1.900968867902463 0.433883739117562,1.974927912181821 -0.222520933956316,1.623489801858719 -0.78183148246804,1 -1))')
    g2 = ogr.ForceToMultiLineString(g1)
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', None)
    assert ogrtest.check_feature_geometry(g2, expected_g2) == 0

    # Test GEOS operations
    if ogrtest.have_geos():
        g2 = g1.Intersection(g1)
        assert g2 is not None

    # Test CIRCULARSTRING wrapped in a GEOMETRYCOLLECTION
    in_wkt = 'GEOMETRYCOLLECTION(CIRCULARSTRING (0 0,1 1,1 -1))'
    g1 = ogr.CreateGeometryFromWkt(in_wkt)

    # Test GEOS operations
    if ogrtest.have_geos():
        g2 = g1.Intersection(g1)
        assert g2 is not None

    # Test ForceToLineString
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', '45')
    g2 = ogr.ForceToLineString(g1)
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', None)
    assert ogrtest.check_feature_geometry(g2, expected_g) == 0

    # Test ForceToMultiLineString
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', '45')
    g2 = ogr.ForceToMultiLineString(g1)
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', None)
    assert ogrtest.check_feature_geometry(g2, expected_g2) == 0

    # Test stroking of full circle with 3 points. ISO draft
    # mentions that this should define a full circle, but there's an
    # ambiguity on the winding order. We choose counter-clock-wise order like
    # PostGIS. On the contrary, Microsoft for example forbids
    # such a possibility : http://msdn.microsoft.com/en-us/library/ff929141.aspx
    in_wkt = 'CIRCULARSTRING (0 0,1 0,0 0)'
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    expected_g = ogr.CreateGeometryFromWkt('LINESTRING (0 0,0.116977778440514 -0.321393804843282,0.413175911166547 -0.49240387650611,0.75 -0.433012701892224,0.969846310392967 -0.171010071662835,0.969846310392967 0.171010071662835,0.75 0.433012701892224,0.413175911166547 0.49240387650611,0.116977778440514 0.321393804843282,0 0)')
    g2 = g1.GetLinearGeometry(45)
    assert ogrtest.check_feature_geometry(g2, expected_g) == 0

    length = g1.Length()
    expected_length = 2 * math.pi * 0.5
    assert length == pytest.approx(expected_length, abs=1e-8)

    # Test stroking of full circle (well defined)
    in_wkt = 'CIRCULARSTRING (0 0,0.5 0.5,1.0 0.0,0.5 -0.5,0.0 0.0)'
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    expected_g = ogr.CreateGeometryFromWkt('LINESTRING (0 0,0.049515566048791 0.216941869558781,0.188255099070638 0.390915741234018,0.388739533021848 0.48746395609092,0.611260466978166 0.48746395609092,0.811744900929369 0.390915741234018,0.950484433951232 0.216941869558781,1 0,0.950484433951232 -0.216941869558781,0.811744900929369 -0.390915741234018,0.611260466978166 -0.48746395609092,0.388739533021848 -0.48746395609092,0.188255099070638 -0.390915741234018,0.049515566048791 -0.216941869558781,0 0)')
    g2 = g1.GetLinearGeometry(45)
    assert ogrtest.check_feature_geometry(g2, expected_g) == 0

    length = g1.Length()
    expected_length = 2 * math.pi * 0.5
    assert length == pytest.approx(expected_length, abs=1e-8)

    # Check segmentize
    g1.Segmentize(0.5)
    expected_g = ogr.CreateGeometryFromWkt('CIRCULARSTRING (0 0,0.146446609406726 0.353553390593274,0.5 0.5,0.853553390593274 0.353553390593274,1 0,0.853553390593274 -0.353553390593274,0.5 -0.5,0.146446609406726 -0.353553390593274,0 0)')
    assert ogrtest.check_feature_geometry(g1, expected_g) == 0

    # Sanity check: the length must remain the same
    length = g1.Length()
    expected_length = 2 * math.pi * 0.5
    assert length == pytest.approx(expected_length, abs=1e-8)

    # Check segmentize symmetry : do exact binary comparison
    in_wkt = 'CIRCULARSTRING (0 0,1.2 1,2 0)'
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    g1.Segmentize(0.25)
    in_wkt = 'CIRCULARSTRING (2 0,1.2 1,0 0)'
    g2 = ogr.CreateGeometryFromWkt(in_wkt)
    g2.Segmentize(0.25)
    for i in range(g1.GetPointCount()):
        if g1.GetPoint(i) != g2.GetPoint(g1.GetPointCount() - 1 - i):
            print('%.18g' % (g1.GetPoint(i)[0] - g2.GetPoint(g1.GetPointCount() - 1 - i)[0]))
            pytest.fail('%.18g' % (g1.GetPoint(i)[1] - g2.GetPoint(g1.GetPointCount() - 1 - i)[1]))

    # Test stroking of full circle with Z
    in_wkt = 'CIRCULARSTRING (0 0 1,1 0 2,0 0 1)'
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    expected_g = ogr.CreateGeometryFromWkt('LINESTRING (0 0 1,0.116977778440514 -0.321393804843282 1,0.413175911166547 -0.49240387650611 1,0.75 -0.433012701892224 1,0.969846310392967 -0.171010071662835 1,0.969846310392967 0.171010071662835 1,0.75 0.433012701892224 1,0.413175911166547 0.49240387650611 1,0.116977778440514 0.321393804843282 1,0 0 1)')
    g2 = g1.GetLinearGeometry(45)
    assert ogrtest.check_feature_geometry(g2, expected_g) == 0

    # Check segmentize
    g1.Segmentize(0.5)
    expected_g = ogr.CreateGeometryFromWkt('CIRCULARSTRING Z (0 0 1,0.146446609406726 -0.353553390593274 1.25,0.5 -0.5 1.5,0.853553390593274 -0.353553390593274 1.75,1 0 2,0.853553390593274 0.353553390593274 1.75,0.5 0.5 1.5,0.146446609406727 0.353553390593274 1.25,0 0 1)')
    assert ogrtest.check_feature_geometry(g1, expected_g) == 0

    # Same as above but reverse order
    in_wkt = 'CIRCULARSTRING (0 0,0.5 -0.5,1.0 0.0,0.5 0.5,0.0 0.0)'
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    expected_g = ogr.CreateGeometryFromWkt('LINESTRING (0 0,0.049515566048791 -0.216941869558781,0.188255099070638 -0.390915741234018,0.388739533021848 -0.48746395609092,0.611260466978166 -0.48746395609092,0.811744900929369 -0.390915741234018,0.950484433951232 -0.216941869558781,1 0,0.950484433951232 0.216941869558781,0.811744900929369 0.390915741234018,0.611260466978166 0.48746395609092,0.388739533021848 0.48746395609092,0.188255099070638 0.390915741234018,0.049515566048791 0.216941869558781,0 0)')
    g2 = g1.GetLinearGeometry(45)
    assert ogrtest.check_feature_geometry(g2, expected_g) == 0

    length = g1.Length()
    expected_length = 2 * math.pi * 0.5
    assert length == pytest.approx(expected_length, abs=1e-8)

    # Test stroking of a circular string with 3 colinear points
    in_wkt = 'CIRCULARSTRING (0 0,1 1,2 2)'
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    expected_g = ogr.CreateGeometryFromWkt('LINESTRING (0 0,1 1,2 2)')
    g2 = g1.GetLinearGeometry(45)
    assert ogrtest.check_feature_geometry(g2, expected_g) == 0

    length = g1.Length()
    expected_length = 2 * math.sqrt(2)
    assert length == pytest.approx(expected_length, abs=1e-8)

    # Test Value()
    p = g1.Value(length / 4.0)
    expected_p = ogr.CreateGeometryFromWkt('POINT (0.5 0.5)')
    assert ogrtest.check_feature_geometry(p, expected_p) == 0
    p = g1.Value(3.0 * length / 4.0)
    expected_p = ogr.CreateGeometryFromWkt('POINT (1.5 1.5)')
    assert ogrtest.check_feature_geometry(p, expected_p) == 0

    # Check segmentize
    g1.Segmentize(0.5)
    expected_g = ogr.CreateGeometryFromWkt('CIRCULARSTRING (0 0,0.166666666666667 0.166666666666667,0.333333333333333 0.333333333333333,0.5 0.5,0.666666666666667 0.666666666666667,0.833333333333333 0.833333333333333,1 1,1.166666666666667 1.166666666666667,1.333333333333333 1.333333333333333,1.5 1.5,1.666666666666667 1.666666666666667,1.833333333333333 1.833333333333333,2 2)')
    assert ogrtest.check_feature_geometry(g1, expected_g) == 0

    # Sanity check: the length must remain the same
    length = g1.Length()
    expected_length = 2 * math.sqrt(2)
    assert length == pytest.approx(expected_length, abs=1e-8)

    # Same with Z
    in_wkt = 'CIRCULARSTRING (0 0 1,1 1 2,2 2 1)'
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    expected_g = ogr.CreateGeometryFromWkt('LINESTRING (0 0 1,1 1 2,2 2 1)')
    g2 = g1.GetLinearGeometry(45)
    assert ogrtest.check_feature_geometry(g2, expected_g) == 0

    # Check segmentize
    g1.Segmentize(0.5)
    expected_g = ogr.CreateGeometryFromWkt('CIRCULARSTRING Z (0 0 1,0.166666666666667 0.166666666666667 1.166666666666667,0.333333333333333 0.333333333333333 1.333333333333333,0.5 0.5 1.5,0.666666666666667 0.666666666666667 1.666666666666667,0.833333333333333 0.833333333333333 1.833333333333333,1 1 2,1.166666666666667 1.166666666666667 1.833333333333333,1.333333333333333 1.333333333333333 1.666666666666667,1.5 1.5 1.5,1.666666666666667 1.666666666666667 1.333333333333333,1.833333333333333 1.833333333333333 1.166666666666667,2 2 1)')
    assert ogrtest.check_feature_geometry(g1, expected_g) == 0

    # Test Value()
    p = g1.Value(length / 4.0)
    expected_p = ogr.CreateGeometryFromWkt('POINT (0.5 0.5 1.5)')
    assert ogrtest.check_feature_geometry(p, expected_p) == 0
    p = g1.Value(3.0 * length / 4.0)
    expected_p = ogr.CreateGeometryFromWkt('POINT (1.5 1.5 1.5)')
    assert ogrtest.check_feature_geometry(p, expected_p) == 0
    in_wkt = 'CIRCULARSTRING (0 0,1 1,1 -1)'

    # Test GetEnvelope() in various cases
    cx = 1
    cy = 2
    r = 3

    # In quadrant 0
    a0 = math.pi / 3
    a1 = math.pi / 4
    a2 = math.pi / 6
    in_wkt = 'CIRCULARSTRING(%.16g %.16g,%.16g %.16g,%.16g %.16g)' % (
        cx + r * math.cos(a0), cy + r * math.sin(a0),
        cx + r * math.cos(a1), cy + r * math.sin(a1),
        cx + r * math.cos(a2), cy + r * math.sin(a2))
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    env = g1.GetEnvelope()
    expected_env = (cx + r * math.cos(a0), cx + r * math.cos(a2), cy + r * math.sin(a2), cy + r * math.sin(a0))
    for i in range(4):
        assert env[i] == pytest.approx(expected_env[i], abs=1e-8)

    # From quadrant 0 to quadrant -1
    a0 = math.pi / 3
    a1 = math.pi / 6
    a2 = -math.pi / 6
    in_wkt = 'CIRCULARSTRING(%.16g %.16g,%.16g %.16g,%.16g %.16g)' % (
        cx + r * math.cos(a0), cy + r * math.sin(a0),
        cx + r * math.cos(a1), cy + r * math.sin(a1),
        cx + r * math.cos(a2), cy + r * math.sin(a2))
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    env = g1.GetEnvelope()
    expected_env = (cx + r * math.cos(a0), cx + r, cy + r * math.sin(a2), cy + r * math.sin(a0))
    for i in range(4):
        assert env[i] == pytest.approx(expected_env[i], abs=1e-8)

    # From quadrant 0 to quadrant 3
    a0 = math.pi / 3
    a1 = math.pi - math.pi / 3
    a2 = -math.pi / 6
    in_wkt = 'CIRCULARSTRING(%.16g %.16g,%.16g %.16g,%.16g %.16g)' % (
        cx + r * math.cos(a0), cy + r * math.sin(a0),
        cx + r * math.cos(a1), cy + r * math.sin(a1),
        cx + r * math.cos(a2), cy + r * math.sin(a2))
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    env = g1.GetEnvelope()
    expected_env = (cx - r, cx + r * math.cos(a2), cy - r, cy + r)
    for i in range(4):
        assert env[i] == pytest.approx(expected_env[i], abs=1e-8)

    # From quadrant 0 to quadrant 4
    a0 = math.pi / 3
    a1 = math.pi - math.pi / 3
    a2 = math.pi / 6
    in_wkt = 'CIRCULARSTRING(%.16g %.16g,%.16g %.16g,%.16g %.16g)' % (
        cx + r * math.cos(a0), cy + r * math.sin(a0),
        cx + r * math.cos(a1), cy + r * math.sin(a1),
        cx + r * math.cos(a2), cy + r * math.sin(a2))
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    env = g1.GetEnvelope()
    expected_env = (cx - r, cx + r, cy - r, cy + r)
    for i in range(4):
        assert env[i] == pytest.approx(expected_env[i], abs=1e-8)

    # Full circle
    a0 = math.pi / 3
    a1 = math.pi + math.pi / 3
    a2 = math.pi / 3
    in_wkt = 'CIRCULARSTRING(%.16g %.16g,%.16g %.16g,%.16g %.16g)' % (
        cx + r * math.cos(a0), cy + r * math.sin(a0),
        cx + r * math.cos(a1), cy + r * math.sin(a1),
        cx + r * math.cos(a2), cy + r * math.sin(a2))
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    env = g1.GetEnvelope()
    expected_env = (cx - r, cx + r, cy - r, cy + r)
    for i in range(4):
        assert env[i] == pytest.approx(expected_env[i], abs=1e-8)

    # Error case : not enough points
    in_wkt = 'CIRCULARSTRING (0 0)'
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    g = ogr.CreateGeometryFromWkt(in_wkt)
    gdal.PopErrorHandler()
    assert g is None

    # EMPTY
    for in_wkt in ['CIRCULARSTRING EMPTY', 'CIRCULARSTRING Z EMPTY']:
        g1 = ogr.CreateGeometryFromWkt(in_wkt)
        out_wkt = g1.ExportToWkt()
        assert in_wkt == out_wkt
        assert g1.Length() == 0.0, in_wkt

        g2 = ogr.CreateGeometryFromWkb(g1.ExportToWkb())
        out_wkt = g2.ExportToWkt()
        assert in_wkt == out_wkt

    
###############################################################################
# Test OGRCompoundCurve


def test_ogr_geom_compoundcurve():

    in_wkt = 'COMPOUNDCURVE (CIRCULARSTRING (0 0,1 1,1 -1))'
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    assert g1.GetGeometryType() == ogr.wkbCompoundCurve
    out_wkt = g1.ExportToWkt()
    assert in_wkt == out_wkt

    assert g1.GetGeometryCount() == 1
    assert g1.GetGeometryRef(0).ExportToWkt() == 'CIRCULARSTRING (0 0,1 1,1 -1)'

    env = g1.GetEnvelope()
    expected_env = (0.0, 2.0, -1.0, 1.0)
    for i in range(4):
        assert env[i] == pytest.approx(expected_env[i], abs=1e-8)

    length = g1.Length()
    expected_length = 1.5 * math.pi
    assert length == pytest.approx(expected_length, abs=1e-8)

    g2 = ogr.CreateGeometryFromWkb(g1.ExportToWkb())
    out_wkt = g2.ExportToWkt()
    assert in_wkt == out_wkt

    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    assert g1.HasCurveGeometry()
    assert g1.HasCurveGeometry(True)

    # CreateGeometryFromWkt of LINESTRING
    in_wkt = 'COMPOUNDCURVE ((0 0,0 10))'
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    assert g1.HasCurveGeometry()
    assert not g1.HasCurveGeometry(True)

    # With Z
    in_wkt = 'COMPOUNDCURVE Z (CIRCULARSTRING Z (0 0 10,1 1 20,2 0 30),(2 0 30,0 0 10))'
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    assert g1.GetGeometryType() == ogr.wkbCompoundCurveZ
    out_wkt = g1.ExportToWkt()
    assert in_wkt == out_wkt

    env = g1.GetEnvelope()
    expected_env = (0.0, 2.0, 0.0, 1.0)
    for i in range(4):
        assert env[i] == pytest.approx(expected_env[i], abs=1e-8)

    env = g1.GetEnvelope3D()
    expected_env = (0.0, 2.0, 0.0, 1.0, 10, 30)
    for i in range(6):
        assert env[i] == pytest.approx(expected_env[i], abs=1e-8)

    # Test Value()
    p = g1.Value(-1e-3)
    expected_p = ogr.CreateGeometryFromWkt('POINT (0 0 10)')
    assert ogrtest.check_feature_geometry(p, expected_p) == 0

    p = g1.Value(math.pi / 2.0)
    expected_p = ogr.CreateGeometryFromWkt('POINT (1 1 20)')
    assert ogrtest.check_feature_geometry(p, expected_p) == 0

    p = g1.Value(math.pi)
    expected_p = ogr.CreateGeometryFromWkt('POINT (2 0 30)')
    assert ogrtest.check_feature_geometry(p, expected_p) == 0

    p = g1.Value(math.pi + 1)
    expected_p = ogr.CreateGeometryFromWkt('POINT (1 0 20)')
    assert ogrtest.check_feature_geometry(p, expected_p) == 0

    p = g1.Value(math.pi + 2 + 1e-3)
    expected_p = ogr.CreateGeometryFromWkt('POINT (0 0 10)')
    assert ogrtest.check_feature_geometry(p, expected_p) == 0

    wkb = g1.ExportToWkb()
    isowkb = g1.ExportToIsoWkb()
    assert wkb == isowkb
    g2 = ogr.CreateGeometryFromWkb(wkb)
    out_wkt = g2.ExportToWkt()
    assert in_wkt == out_wkt

    # Several parts
    in_wkt = 'COMPOUNDCURVE (CIRCULARSTRING (0 0,1 1,1 -1),(1 -1,0 0))'
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    assert g1.GetGeometryType() == ogr.wkbCompoundCurve
    out_wkt = g1.ExportToWkt()
    assert in_wkt == out_wkt

    assert g1.Equals(g1)

    assert g1.Equals(g1.Clone())

    assert not g1.Equals(ogr.CreateGeometryFromWkt('POINT(0 0)'))

    assert not g1.Equals(ogr.CreateGeometryFromWkt('COMPOUNDCURVE (CIRCULARSTRING (0 0,1 1,1 -1))'))

    assert not g1.Equals(ogr.CreateGeometryFromWkt('COMPOUNDCURVE (CIRCULARSTRING (0 0,1 1,1 -1),(1 -1,0 1))'))

    length = g1.Length()
    expected_length = 1.5 * math.pi + math.sqrt(2)
    assert length == pytest.approx(expected_length, abs=1e-8)

    g2 = ogr.CreateGeometryFromWkb(g1.ExportToWkb())
    out_wkt = g2.ExportToWkt()
    assert in_wkt == out_wkt

    # Test stroking
    gdal.SetConfigOption('OGR_STROKE_CURVE', 'TRUE')
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', '45')
    in_wkt = 'COMPOUNDCURVE (CIRCULARSTRING (0 0,1 1,1 -1))'
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    gdal.SetConfigOption('OGR_STROKE_CURVE', 'FALSE')
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', None)

    expected_g = ogr.CreateGeometryFromWkt('LINESTRING (0 0,0.218168517531969 0.623489801858729,0.777479066043687 0.974927912181831,1.433883739117561 0.900968867902435,1.900968867902463 0.433883739117562,1.974927912181821 -0.222520933956316,1.623489801858719 -0.78183148246804,1 -1)')
    assert ogrtest.check_feature_geometry(g1, expected_g) == 0

    in_wkt = 'COMPOUNDCURVE (CIRCULARSTRING (0 0,1 1,1 -1))'
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    in_wkb = g1.ExportToWkb()

    gdal.SetConfigOption('OGR_STROKE_CURVE', 'TRUE')
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', '45')
    g2 = ogr.CreateGeometryFromWkb(in_wkb)
    gdal.SetConfigOption('OGR_STROKE_CURVE', 'FALSE')
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', None)

    expected_g = ogr.CreateGeometryFromWkt('LINESTRING (0 0,0.218168517531969 0.623489801858729,0.777479066043687 0.974927912181831,1.433883739117561 0.900968867902435,1.900968867902463 0.433883739117562,1.974927912181821 -0.222520933956316,1.623489801858719 -0.78183148246804,1 -1)')
    assert ogrtest.check_feature_geometry(g2, expected_g) == 0

    g2 = g1.GetLinearGeometry(45)
    assert ogrtest.check_feature_geometry(g2, expected_g) == 0

    assert g1.HasCurveGeometry()

    assert g1.HasCurveGeometry(True)

    g2 = g1.GetLinearGeometry(45)
    assert ogrtest.check_feature_geometry(g2, expected_g) == 0

    gdal.SetConfigOption('OGR_ARC_STEPSIZE', '45')
    g2 = ogr.ForceToLineString(g1)
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', None)
    assert ogrtest.check_feature_geometry(g2, expected_g) == 0

    gdal.SetConfigOption('OGR_ARC_STEPSIZE', '45')
    expected_g = ogr.CreateGeometryFromWkt('MULTILINESTRING ((0 0,0.218168517531969 0.623489801858729,0.777479066043687 0.974927912181831,1.433883739117561 0.900968867902435,1.900968867902463 0.433883739117562,1.974927912181821 -0.222520933956316,1.623489801858719 -0.78183148246804,1 -1))')
    g2 = ogr.ForceToMultiLineString(g1)
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', None)
    assert ogrtest.check_feature_geometry(g2, expected_g) == 0

    # Check segmentize
    g1.Segmentize(0.5)
    expected_g = ogr.CreateGeometryFromWkt('COMPOUNDCURVE (CIRCULARSTRING (0 0,0.076120467488713 0.38268343236509,0.292893218813453 0.707106781186548,0.61731656763491 0.923879532511287,1 1,1.38268343236509 0.923879532511287,1.707106781186547 0.707106781186547,1.923879532511287 0.38268343236509,2 0,1.923879532511287 -0.38268343236509,1.707106781186547 -0.707106781186547,1.38268343236509 -0.923879532511287,1 -1))')
    assert ogrtest.check_feature_geometry(g1, expected_g) == 0

    # Error case : not enough points
    in_wkt = 'COMPOUNDCURVE ((0 0))'
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    g = ogr.CreateGeometryFromWkt(in_wkt)
    gdal.PopErrorHandler()
    assert g is None

    # Error case : invalid curve
    in_wkt = 'COMPOUNDCURVE (COMPOUNDCURVE EMPTY)'
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    g = ogr.CreateGeometryFromWkt(in_wkt)
    gdal.PopErrorHandler()
    assert g is None

    # Error case : non contiguous curves
    in_wkt = 'COMPOUNDCURVE ((0 0,1 1),(2 2,3 3))'
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    g = ogr.CreateGeometryFromWkt(in_wkt)
    gdal.PopErrorHandler()
    assert g is None

    # Error case : non contiguous curves
    in_wkt = 'COMPOUNDCURVE (EMPTY,(2 2,3 3))'
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    g = ogr.CreateGeometryFromWkt(in_wkt)
    gdal.PopErrorHandler()
    assert g is None

    # Error case : non contiguous curves
    in_wkt = 'COMPOUNDCURVE ((2 2,3 3), EMPTY)'
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    g = ogr.CreateGeometryFromWkt(in_wkt)
    gdal.PopErrorHandler()
    assert g is None

    g = ogr.Geometry(ogr.wkbCompoundCurve)
    g.AddGeometry(ogr.CreateGeometryFromWkt('LINESTRING(0 0,1 1)'))
    assert g.ExportToWkt() == 'COMPOUNDCURVE ((0 0,1 1))'

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    g.AddGeometry(ogr.CreateGeometryFromWkt('LINESTRING(0 0,1 1)'))
    gdal.PopErrorHandler()
    assert g.ExportToWkt() == 'COMPOUNDCURVE ((0 0,1 1),(1 1,0 0))'

    g = ogr.Geometry(ogr.wkbCompoundCurve)
    g.AddGeometryDirectly(ogr.CreateGeometryFromWkt('LINESTRING(0 0,1 1)'))
    assert g.ExportToWkt() == 'COMPOUNDCURVE ((0 0,1 1))'

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    g.AddGeometryDirectly(ogr.CreateGeometryFromWkt('LINESTRING(0 0,1 1)'))
    gdal.PopErrorHandler()
    assert g.ExportToWkt() == 'COMPOUNDCURVE ((0 0,1 1),(1 1,0 0))'

    # Cannot add compoundcurve in compoundcurve
    g = ogr.Geometry(ogr.wkbCompoundCurve)
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    g.AddGeometryDirectly(ogr.CreateGeometryFromWkt('COMPOUNDCURVE((1 1,2 2))'))
    gdal.PopErrorHandler()
    assert g.ExportToWkt() == 'COMPOUNDCURVE EMPTY'

    # Check that discretization is not sensitive to winding order
    g1 = ogr.CreateGeometryFromWkt('COMPOUNDCURVE((-1 0,0 1),CIRCULARSTRING (0 1,0.25 0,0.1 -0.5),(0.1 -0.5,-1 0))')
    g2 = g1.GetLinearGeometry(1.5)
    g1 = ogr.CreateGeometryFromWkt('COMPOUNDCURVE((-1 0,0.1 -0.5),CIRCULARSTRING (0.1 -0.5,0.25 0,0 1),(0 1,-1 0))')
    g3 = g1.GetLinearGeometry(1.5)
    p_count = g2.GetPointCount()
    for i in range(p_count):
        # yes we do strict (binary) comparison. This is really intended.
        # The curves must be exactly the same, despite our stealth mode
        if g2.GetX(i) != g3.GetX(p_count - 1 - i) or \
           g2.GetY(i) != g3.GetY(p_count - 1 - i):
            print(abs(g2.GetX(i) - g3.GetX(p_count - 1 - i)))
            pytest.fail(abs(g2.GetY(i) - g3.GetY(p_count - 1 - i)))

    # Test Transform
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4326)
    g.AssignSpatialReference(sr)
    g.TransformTo(sr)

    # Invalid wkb
    wkb_list = ['\x01\x09\x00\x00\x00\x01\x00\x00\x00',  # subgeometry declared but not present
                '\x01\x09\x00\x00\x00\xff\xff\xff\x7f',  # 2 billion subgeometries declared !
                '\x01\x09\x00\x00\x00\x01\x00\x00\x00\x01\xff\x00\x00\x00\x00\x00\x00\x00',  # subgeometry invalid: unknown type
                '\x01\x09\x00\x00\x00\x01\x00\x00\x00\x01\x02\x00\x00\x00\x01\x00\x00\x00',  # subgeometry invalid: linestring truncated
                '\x01\x09\x00\x00\x00\x01\x00\x00\x00\x01\x02\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00',  # subgeometry invalid: linestring with one point
                '\x01\x09\x00\x00\x00\x01\x00\x00\x00\x01\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00',  # subgeometry invalid: point
                '\x01\x09\x00\x00\x00\x01\x00\x00\x00\x01\x09\x00\x00\x00\x00\x00\x00\x00',  # subgeometry invalid: compoundcurve
               ]
    for wkb in wkb_list:
        gdal.PushErrorHandler('CPLQuietErrorHandler')
        g = ogr.CreateGeometryFromWkb(wkb)
        gdal.PopErrorHandler()
        assert g is None, wkb

    # EMPTY
    for in_wkt in ['COMPOUNDCURVE EMPTY', 'COMPOUNDCURVE Z EMPTY']:
        g1 = ogr.CreateGeometryFromWkt(in_wkt)
        out_wkt = g1.ExportToWkt()
        assert in_wkt == out_wkt
        assert g1.Length() == 0.0, in_wkt

        g2 = ogr.CreateGeometryFromWkb(g1.ExportToWkb())
        out_wkt = g2.ExportToWkt()
        assert in_wkt == out_wkt

    
###############################################################################
# Test OGRCurvePolygon


def test_ogr_geom_curvepolygon():

    in_wkt = 'CURVEPOLYGON (CIRCULARSTRING (0 0,1 0,0 0))'
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    assert g1.GetGeometryType() == ogr.wkbCurvePolygon
    out_wkt = g1.ExportToWkt()
    assert in_wkt == out_wkt

    if ogrtest.have_geos():
        p1 = g1.PointOnSurface()
        assert (p1.GetX() - 0.5) * (p1.GetX() - 0.5) + p1.GetY() * p1.GetY() <= 0.5 * 0.5

    env = g1.GetEnvelope()
    expected_env = (0.0, 1.0, -0.5, 0.5)
    for i in range(4):
        assert env[i] == pytest.approx(expected_env[i], abs=1e-8)

    area = g1.Area()
    expected_area = math.pi * 0.5 * 0.5
    assert area == pytest.approx(expected_area, abs=1e-8)

    g2 = ogr.CreateGeometryFromWkb(g1.ExportToWkb())
    out_wkt = g2.ExportToWkt()
    assert in_wkt == out_wkt

    # CURVEPOLYGON of LINESTRING
    in_wkt = 'CURVEPOLYGON ((0 0,0 10,10 10,10 0,0 0))'
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    assert g1.HasCurveGeometry()
    assert not g1.HasCurveGeometry(True)

    # With Z
    in_wkt = 'CURVEPOLYGON Z (CIRCULARSTRING Z (0 0 10,1 0 10,0 0 10))'
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    assert g1.GetGeometryType() == ogr.wkbCurvePolygonZ
    out_wkt = g1.ExportToWkt()
    assert in_wkt == out_wkt

    wkb = g1.ExportToWkb()
    isowkb = g1.ExportToIsoWkb()
    assert wkb == isowkb
    g2 = ogr.CreateGeometryFromWkb(wkb)
    out_wkt = g2.ExportToWkt()
    assert in_wkt == out_wkt

    # Test stroking
    gdal.SetConfigOption('OGR_STROKE_CURVE', 'TRUE')
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', '45')
    in_wkt = 'CURVEPOLYGON (CIRCULARSTRING (0 0,1 0,0 0))'
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    gdal.SetConfigOption('OGR_STROKE_CURVE', 'FALSE')
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', None)

    expected_g = ogr.CreateGeometryFromWkt('POLYGON ((0 0,0.116977778440514 -0.321393804843282,0.413175911166547 -0.49240387650611,0.75 -0.433012701892224,0.969846310392967 -0.171010071662835,0.969846310392967 0.171010071662835,0.75 0.433012701892224,0.413175911166547 0.49240387650611,0.116977778440514 0.321393804843282,0 0))')
    assert ogrtest.check_feature_geometry(g1, expected_g) == 0

    in_wkt = 'CURVEPOLYGON (CIRCULARSTRING (0 0,1 0,0 0))'
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    in_wkb = g1.ExportToWkb()

    gdal.SetConfigOption('OGR_STROKE_CURVE', 'TRUE')
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', '45')
    g2 = ogr.CreateGeometryFromWkb(in_wkb)
    gdal.SetConfigOption('OGR_STROKE_CURVE', 'FALSE')
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', None)

    assert ogrtest.check_feature_geometry(g2, expected_g) == 0

    # Test HasCurveGeometry
    assert g1.HasCurveGeometry()
    assert g1.HasCurveGeometry(True)

    # Test GetLinearGeometry
    g2 = g1.GetLinearGeometry(45)
    assert ogrtest.check_feature_geometry(g2, expected_g) == 0

    # Test ForceToPolygon
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', '45')
    g2 = ogr.ForceToPolygon(g1)
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', None)
    assert ogrtest.check_feature_geometry(g2, expected_g) == 0

    # Test ForceToMultiPolygon
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', '45')
    expected_g2 = ogr.CreateGeometryFromWkt('MULTIPOLYGON (((0 0,0.116977778440514 -0.321393804843282,0.413175911166547 -0.49240387650611,0.75 -0.433012701892224,0.969846310392967 -0.171010071662835,0.969846310392967 0.171010071662835,0.75 0.433012701892224,0.413175911166547 0.49240387650611,0.116977778440514 0.321393804843282,0 0)))')
    g2 = ogr.ForceToMultiPolygon(g1)
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', None)
    assert ogrtest.check_feature_geometry(g2, expected_g2) == 0

    # Test ForceToMultiLineString
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', '45')
    g2 = ogr.ForceToMultiLineString(g1)
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', None)
    expected_g3 = ogr.CreateGeometryFromWkt('MULTILINESTRING ((0 0,0.116977778440514 -0.321393804843282,0.413175911166547 -0.49240387650611,0.75 -0.433012701892224,0.969846310392967 -0.171010071662835,0.969846310392967 0.171010071662835,0.75 0.433012701892224,0.413175911166547 0.49240387650611,0.116977778440514 0.321393804843282,0 0))')
    assert ogrtest.check_feature_geometry(g2, expected_g3) == 0

    # Test GEOS operations
    if ogrtest.have_geos():
        g2 = g1.Intersection(g1)
        assert g2 is not None

    # Test CURVEPOLYGON and COMPOUNDCURVE, CIRCULARSTRING, LINESTRING
    in_wkt = 'CURVEPOLYGON (COMPOUNDCURVE (CIRCULARSTRING (0 0,1 1,2 0),(2 0,0 0)),(0.1 0.1,0.1 0.2,0.2 0.2,0.2 0.1,0.1 0.1),CIRCULARSTRING (0.25 0.25,0.75 0.25,0.25 0.25))'
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    g2 = ogr.CreateGeometryFromWkb(g1.ExportToWkb())
    assert g2.ExportToWkt() == in_wkt

    # Test CURVEPOLYGON wrapped in a GEOMETRYCOLLECTION
    in_wkt = 'GEOMETRYCOLLECTION(CURVEPOLYGON(CIRCULARSTRING (0 0,1 0,0 0)))'
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    g2 = ogr.CreateGeometryFromWkb(g1.ExportToWkb())
    assert g1.Equals(g2)

    # Test GEOS operations
    if ogrtest.have_geos():
        g2 = g1.Intersection(g1)
        assert g2 is not None

    # Test ForceToPolygon
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', '45')
    g2 = ogr.ForceToPolygon(g1)
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', None)
    assert ogrtest.check_feature_geometry(g2, expected_g) == 0

    # Test ForceToMultiPolygon
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', '45')
    g2 = ogr.ForceToMultiPolygon(g1)
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', None)
    assert ogrtest.check_feature_geometry(g2, expected_g2) == 0

    # Error case : not enough points
    in_wkt = 'CURVEPOLYGON ((0 0,0 1,0 0))'
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    g = ogr.CreateGeometryFromWkt(in_wkt)
    gdal.PopErrorHandler()
    assert g is None

    # Error case : wrong sub-geometry type
    in_wkt = 'CURVEPOLYGON (POINT EMPTY)'
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    g = ogr.CreateGeometryFromWkt(in_wkt)
    gdal.PopErrorHandler()
    assert g is None

    # Error case: non closed ring
    in_wkt = 'CURVEPOLYGON ((0 0,0 1,1 1,1 0))'
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    g = ogr.CreateGeometryFromWkt(in_wkt)
    gdal.PopErrorHandler()
    assert g is None

    # Area
    g = ogr.CreateGeometryFromWkt('CURVEPOLYGON ((0 0,1 1,1 0,0 0))')
    assert g.Area() == 0.5

    g = ogr.CreateGeometryFromWkt('CURVEPOLYGON (COMPOUNDCURVE((0 0,1 1,1 0,0 0)))')
    assert g.Area() == 0.5

    g = ogr.CreateGeometryFromWkt('CURVEPOLYGON (COMPOUNDCURVE((0 0,1 1),(1 1,1 0,0 0)))')
    assert g.Area() == 0.5

    g = ogr.CreateGeometryFromWkt('CURVEPOLYGON ((100000000 100000000,100000001 100000001,100000001 100000000,100000000 100000000))')
    assert g.Area() == 0.5

    # Equals
    g1 = ogr.CreateGeometryFromWkt('CURVEPOLYGON ((0 0,1 1,1 0,0 0),CIRCULARSTRING(0.75 0.5,0.85 0.5,0.75 0.5))')
    assert g1.Equals(g1)

    assert g1.Equals(g1.Clone())

    assert not g1.Equals(ogr.CreateGeometryFromWkt('POINT(0 0)'))

    assert not g1.Equals(ogr.CreateGeometryFromWkt('CURVEPOLYGON ((0 0,1 1,1 0,0 0))'))

    # Intersects optimizations on a circle
    g1 = ogr.CreateGeometryFromWkt('CURVEPOLYGON (CIRCULARSTRING (0 0,2 0,0 0))')
    # Point slightly within circle
    p1 = ogr.CreateGeometryFromWkt('POINT (%.16g %.16g)' % (1 + math.cos(math.pi / 6) - 1e-4, math.sin(math.pi / 6)))
    # To prove that we don't use discretization
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', '45')
    res = g1.Intersects(p1)
    res = res & p1.Intersects(g1)
    res = res & g1.Contains(p1)
    res = res & p1.Within(g1)
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', None)
    assert res

    # Test point slightly outside circle
    p2 = ogr.CreateGeometryFromWkt('POINT (%.16g %.16g)' % (1 + math.cos(math.pi / 6) + 1e-4, math.sin(math.pi / 6)))
    assert not p2.Within(g1)

    # Full circle defined by 2 arcs
    g1 = ogr.CreateGeometryFromWkt('CURVEPOLYGON (CIRCULARSTRING (0 0,1 1,2 0,1 -1,0 0))')
    assert p1.Within(g1)

    # Same but in reverse order
    g1 = ogr.CreateGeometryFromWkt('CURVEPOLYGON (CIRCULARSTRING (0 0,1 -1,2 0,1 1,0 0))')
    assert p1.Within(g1)

    # This is not a circle
    p2 = ogr.CreateGeometryFromWkt('POINT (%.16g %.16g)' % (1 + math.cos(math.pi / 6) - 1e-2, math.sin(math.pi / 6)))
    g1 = ogr.CreateGeometryFromWkt('CURVEPOLYGON (CIRCULARSTRING (0 0,1 1,2 0,1 1,0 0))')
    assert not p2.Within(g1)

    # Test area on circle in 2 pieces
    g1 = ogr.CreateGeometryFromWkt('CURVEPOLYGON (CIRCULARSTRING (0 0,1 1,2 0,1 -1,0 0))')
    area = g1.Area()
    expected_area = math.pi
    assert area == pytest.approx(expected_area, abs=1e-10)

    # Test area on hippodrome
    g1 = ogr.CreateGeometryFromWkt('CURVEPOLYGON (CIRCULARSTRING (0 0,1 1,2 0,2 -1,2 -2,1 -3,0 -2,0 -1,0 0))')
    area = g1.Area()
    expected_area = math.pi + 2 * 2
    assert area == pytest.approx(expected_area, abs=1e-10)

    # Same hippodrome but with different WKT
    g1 = ogr.CreateGeometryFromWkt('CURVEPOLYGON (COMPOUNDCURVE(CIRCULARSTRING (0 0,1 1,2 0),(2 0,2 -2),CIRCULARSTRING(2 -2,1 -3,0 -2),(0 -2,0 0)))')
    area = g1.Area()
    expected_area = math.pi + 2 * 2
    assert area == pytest.approx(expected_area, abs=1e-10)

    # Similar, but with concave part (does not trigger optimization)
    g1 = ogr.CreateGeometryFromWkt('CURVEPOLYGON (COMPOUNDCURVE(CIRCULARSTRING (0 0,1 1,2 0),(2 0,2 -2),CIRCULARSTRING(2 -2,1 -1,0 -2),(0 -2,0 0)))')
    area = g1.Area()
    expected_area = 2 * 2
    assert area == pytest.approx(expected_area, abs=1e-10)

    # EMPTY
    for in_wkt in ['CURVEPOLYGON EMPTY', 'CURVEPOLYGON Z EMPTY']:
        g1 = ogr.CreateGeometryFromWkt(in_wkt)
        out_wkt = g1.ExportToWkt()
        assert in_wkt == out_wkt
        assert g1.Area() == 0.0, in_wkt

        g2 = ogr.CreateGeometryFromWkb(g1.ExportToWkb())
        out_wkt = g2.ExportToWkt()
        assert in_wkt == out_wkt

    # Empty CircularString
    g = ogr.Geometry(ogr.wkbCurvePolygon)
    g.AddGeometry(ogr.Geometry(ogr.wkbCircularString))
    assert g.Area() == 0

    # Non-convex CircularString
    g = ogr.CreateGeometryFromWkt('CURVEPOLYGON( COMPOUNDCURVE( CIRCULARSTRING(0 0,0.5 -0.5,1 0,1.5 0.5,2 0), (2 0,2 1,0 1,0 0) ) )')
    assert g.Area() == pytest.approx(2.0, abs=1e-10)

###############################################################################
# Test OGRMultiCurve


def test_ogr_geom_multicurve():

    # Simple test
    in_wkt = 'MULTICURVE (CIRCULARSTRING (0 0,1 0,0 0))'
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    assert g1.GetGeometryType() == ogr.wkbMultiCurve
    out_wkt = g1.ExportToWkt()
    assert in_wkt == out_wkt

    assert g1.GetDimension() == 1

    env = g1.GetEnvelope()
    expected_env = (0.0, 1.0, -0.5, 0.5)
    for i in range(4):
        assert env[i] == pytest.approx(expected_env[i], abs=1e-8)

    length = g1.Length()
    expected_length = 2 * math.pi * 0.5
    assert length == pytest.approx(expected_length, abs=1e-8)

    g2 = ogr.CreateGeometryFromWkb(g1.ExportToWkb())
    out_wkt = g2.ExportToWkt()
    assert in_wkt == out_wkt

    assert g1.HasCurveGeometry()
    assert g1.HasCurveGeometry(True)

    # MULTICURVE of LINESTRING
    in_wkt = 'MULTICURVE ((0 0,1 0))'
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    assert g1.HasCurveGeometry()
    assert not g1.HasCurveGeometry(True)

    # Z
    in_wkt = 'MULTICURVE Z (CIRCULARSTRING Z (0 0 10,1 0 10,0 0 10))'
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    assert g1.GetGeometryType() == ogr.wkbMultiCurveZ
    out_wkt = g1.ExportToWkt()
    assert in_wkt == out_wkt

    g2 = ogr.CreateGeometryFromWkb(g1.ExportToWkb())
    out_wkt = g2.ExportToWkt()
    assert in_wkt == out_wkt

    # WKT with all possible sub geometries
    in_wkt = 'MULTICURVE (CIRCULARSTRING (0 0,1 0,0 0),(0 0,1 1),COMPOUNDCURVE ((0 0,1 1),CIRCULARSTRING (1 1,2 2,3 3)))'
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    out_wkt = g1.ExportToWkt()
    assert in_wkt == out_wkt

    g2 = ogr.CreateGeometryFromWkb(g1.ExportToWkb())
    out_wkt = g2.ExportToWkt()
    assert in_wkt == out_wkt

    # Test ForceToMultiLineString
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', '45')
    g2 = ogr.ForceToMultiLineString(g1)
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', None)
    expected_g = 'MULTILINESTRING ((0 0,0.116977778440514 -0.321393804843282,0.413175911166547 -0.49240387650611,0.75 -0.433012701892224,0.969846310392967 -0.171010071662835,0.969846310392967 0.171010071662835,0.75 0.433012701892224,0.413175911166547 0.49240387650611,0.116977778440514 0.321393804843282,0 0),(0 0,1 1),(0 0,1 1,2 2,3 3))'
    assert ogrtest.check_feature_geometry(g2, expected_g) == 0

    # Test GetLinearGeometry
    g2 = g1.GetLinearGeometry(45)
    assert ogrtest.check_feature_geometry(g2, expected_g) == 0

    # Error case : wrong sub-geometry type
    in_wkt = 'MULTILINESTRING (POINT EMPTY)'
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    g = ogr.CreateGeometryFromWkt(in_wkt)
    gdal.PopErrorHandler()
    assert g is None

###############################################################################
# Test OGRMultiSurface


def test_ogr_geom_multisurface():

    # Simple test
    in_wkt = 'MULTISURFACE (CURVEPOLYGON (CIRCULARSTRING (0 0,1 0,0 0)))'
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    assert g1.GetGeometryType() == ogr.wkbMultiSurface
    out_wkt = g1.ExportToWkt()
    assert in_wkt == out_wkt

    assert g1.GetDimension() == 2

    env = g1.GetEnvelope()
    expected_env = (0.0, 1.0, -0.5, 0.5)
    for i in range(4):
        assert env[i] == pytest.approx(expected_env[i], abs=1e-8)

    area = g1.Area()
    expected_area = math.pi * 0.5 * 0.5
    assert area == pytest.approx(expected_area, abs=1e-8)

    g2 = ogr.CreateGeometryFromWkb(g1.ExportToWkb())
    out_wkt = g2.ExportToWkt()
    assert in_wkt == out_wkt

    assert g1.HasCurveGeometry()
    assert g1.HasCurveGeometry(True)

    # Z
    in_wkt = 'MULTISURFACE Z (CURVEPOLYGON Z (CIRCULARSTRING Z (0 0 10,1 0 10,0 0 10)))'
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    assert g1.GetGeometryType() == ogr.wkbMultiSurfaceZ
    out_wkt = g1.ExportToWkt()
    assert in_wkt == out_wkt

    g2 = ogr.CreateGeometryFromWkb(g1.ExportToWkb())
    out_wkt = g2.ExportToWkt()
    assert in_wkt == out_wkt

    # MULTISURFACE of POLYGON
    in_wkt = 'MULTISURFACE (((0 0,0 10,10 10,10 0,0 0)))'
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    assert g1.HasCurveGeometry()
    assert not g1.HasCurveGeometry(True)

    # WKT with all possible sub geometries
    in_wkt = 'MULTISURFACE (((0 0,0 10,10 10,10 0,0 0)),CURVEPOLYGON (CIRCULARSTRING (0 0,1 0,0 0)))'
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    out_wkt = g1.ExportToWkt()
    assert in_wkt == out_wkt

    g2 = ogr.CreateGeometryFromWkb(g1.ExportToWkb())
    out_wkt = g2.ExportToWkt()
    assert in_wkt == out_wkt

    # Test ForceToMultiPolygon
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', '45')
    g2 = ogr.ForceToMultiPolygon(g1)
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', None)
    expected_g = 'MULTIPOLYGON (((0 0,0 10,10 10,10 0,0 0)),((0 0,0.116977778440514 -0.321393804843282,0.413175911166547 -0.49240387650611,0.75 -0.433012701892224,0.969846310392967 -0.171010071662835,0.969846310392967 0.171010071662835,0.75 0.433012701892224,0.413175911166547 0.49240387650611,0.116977778440514 0.321393804843282,0 0)))'
    assert ogrtest.check_feature_geometry(g2, expected_g) == 0

    # Test GetLinearGeometry
    g2 = g1.GetLinearGeometry(45)
    assert ogrtest.check_feature_geometry(g2, expected_g) == 0
    # Check that GetLinearGeometry() is idem-potent on MULTIPOLYGON
    g3 = g2.GetLinearGeometry(45)
    assert g3.Equals(g2)

    # PointOnSurface
    if ogrtest.have_geos():
        in_wkt = 'MULTISURFACE (((0 0,0 10,10 10,10 0,0 0),(1 1,1 9,9 9,9 1,1 1)),((10 0,10 10,20 10,20 0,10 0),(11 1,11 9,19 9,19 1,11 1)))'
        g1 = ogr.CreateGeometryFromWkt(in_wkt)
        p1 = g1.PointOnSurface()
        assert p1.ExportToWkt() == 'POINT (0.5 5.0)'

    # Error case : wrong sub-geometry type
    in_wkt = 'MULTIPOLYGON (POINT EMPTY)'
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    g = ogr.CreateGeometryFromWkt(in_wkt)
    gdal.PopErrorHandler()
    assert g is None

###############################################################################
# Test GetCurveGeometry


def test_ogr_geom_getcurvegeometry():

    for geom in ['POINT', 'LINESTRING', 'POLYGON', 'MULTIPOINT', 'MULTILINESTRING',
                 'MULTIPOLYGON', 'GEOMETRYCOLLECTION',
                 'CIRCULARSTRING',
                 'COMPOUNDCURVE',
                 'CURVEPOLYGON',
                 'MULTICURVE',
                 'MULTISURFACE']:
        in_wkt = geom + ' EMPTY'
        g = ogr.CreateGeometryFromWkt(in_wkt)
        g2 = g.GetCurveGeometry()
        assert g2.ExportToWkt() == in_wkt, g

    g = ogr.CreateGeometryFromWkt('GEOMETRYCOLLECTION(POINT (0 1))')
    g2 = g.GetCurveGeometry()
    assert g.Equals(g2)

    g = ogr.CreateGeometryFromWkt('GEOMETRYCOLLECTION(LINESTRING (0 0,0 1,0 0))')
    g2 = g.GetCurveGeometry()
    assert g.Equals(g2)

    g = ogr.CreateGeometryFromWkt('POLYGON((0 0,0 1,1 1,1 0,0 0))')
    g2 = g.GetCurveGeometry()
    assert g.Equals(g2)

    g = ogr.CreateGeometryFromWkt('POLYGON Z ((0 0 10,0 1 10,1 1 10,1 0 10,0 0 10))')
    g2 = g.GetCurveGeometry()
    assert g.Equals(g2)

    # CircularString with large step
    g1 = ogr.CreateGeometryFromWkt('CIRCULARSTRING (0 0,1 1,2 0)')
    g2 = g1.GetLinearGeometry(15)
    g3 = g2.GetCurveGeometry()
    assert (g3.GetGeometryType() == ogr.wkbCircularString and \
       g1.GetPoint(0) == g3.GetPoint(0) and g1.GetPoint(2) == g3.GetPoint(2) and \
       abs((g3.GetX(1) - 1) * (g3.GetX(1) - 1) + g3.GetY(1) * g3.GetY(1) - 1) <= 1e-8)

    # CurvePolygon with large step
    g1 = ogr.CreateGeometryFromWkt('CURVEPOLYGON( CIRCULARSTRING (0 0,1 1,0 0))')
    g2 = g1.GetLinearGeometry(15)
    g3 = g2.GetCurveGeometry()
    assert g3.Equals(g1)

    # This is a straight line
    g1 = ogr.CreateGeometryFromWkt('CIRCULARSTRING (1 2,3 4,5 6)')
    g2 = g1.GetLinearGeometry()
    g3 = g2.GetCurveGeometry()
    assert g3.ExportToWkt() == 'LINESTRING (1 2,3 4,5 6)'

    # "Random" arcs
    for wkt in ['CIRCULARSTRING (1 2,3 1,5 6)',
                'CIRCULARSTRING (1 -2,3 -1,5 -6)',
                'CIRCULARSTRING (-1 2,-3 1,-5 6)',
                'CIRCULARSTRING (5 6,3 1,1 2)',
                'CIRCULARSTRING (-5 6,-3 1,-1 2)',
                'CIRCULARSTRING (5 -6,3 -1,1 -2)',
                'CIRCULARSTRING (215725 -977513,872751 872597,560240 -7500)',
                'CIRCULARSTRING (-492367 816163,537838 -421954,745494 -65479)',
                'CIRCULARSTRING (543208 -865295,582257 635396,563925 -68156)',
                'CIRCULARSTRING (-481 -193,1 329,-692 -421)',
                'CIRCULARSTRING (525407 781005,710737 463833,-674365 340022)',
                'CIRCULARSTRING (743949 709309,743952 709307,743964 709298)',
                'CIRCULARSTRING (283167 -48388,536492 -197399,-449301 382451)']:
        g1 = ogr.CreateGeometryFromWkt(wkt)
        g2 = g1.GetLinearGeometry()
        g3 = g2.GetCurveGeometry()
        assert g3.Equals(g1)

    # Really random arcs with coordinates in the [-1000,1000] range
    for i in range(1000):
        v = [random.randint(-1000, 1000) for i in range(6)]
        if v[0] != v[4] or v[1] != v[5]:
            wkt = 'CIRCULARSTRING (%d %d,%d %d,%d %d)' % (v[0], v[1], v[2], v[3], v[4], v[5])
            g1 = ogr.CreateGeometryFromWkt(wkt)
            g2 = g1.GetLinearGeometry()
            if g2.GetPointCount() != 3:
                g3 = g2.GetCurveGeometry()
                if not g3.Equals(g1):
                    # FIXME sometime... but avoid failing. for now. This randomly fails, but this is not
                    # the end of the world...
                    # gdaltest.post_reason('fail')
                    print('Difference found :')
                    print(g1)
                    print(g3)
                    # return 'fail'

    # Really random arcs in random displacements, but with small radius
    for i in range(1000):
        x = random.randint(-1000000, 1000000)
        y = random.randint(-1000000, 1000000)
        v = [random.randint(-10, 10) for i in range(6)]
        if v[0] != v[4] or v[1] != v[5]:
            wkt = 'CIRCULARSTRING (%d %d,%d %d,%d %d)' % (x + v[0], y + v[1], x + v[2], y + v[3], x + v[4], y + v[5])
            g1 = ogr.CreateGeometryFromWkt(wkt)
            g2 = g1.GetLinearGeometry()
            if g2.GetPointCount() != 3:
                g3 = g2.GetCurveGeometry()
                if not g3.Equals(g1):
                    # FIXME sometime... but avoid failing. for now. This randomly fails, but this is not
                    # the end of the world...
                    # gdaltest.post_reason('fail')
                    print('Difference found :')
                    print(g1)
                    print(g3)
                    # return 'fail'

    # Really random arcs with coordinates in the [-1000000,1000000] range
    for i in range(1000):
        v = [random.randint(-1000000, 1000000) for i in range(6)]
        if v[0] != v[4] or v[1] != v[5]:
            wkt = 'CIRCULARSTRING (%d %d,%d %d,%d %d)' % (v[0], v[1], v[2], v[3], v[4], v[5])
            g1 = ogr.CreateGeometryFromWkt(wkt)
            g2 = g1.GetLinearGeometry()
            if g2.GetPointCount() != 3:
                g3 = g2.GetCurveGeometry()
                if not g3.Equals(g1):
                    # FIXME sometime... but avoid failing. for now. This randomly fails, but this is not
                    # the end of the world...
                    # gdaltest.post_reason('fail')
                    print('Difference found :')
                    print(g1)
                    print(g3)
                    # return 'fail'

    # 5 points full circle
    g1 = ogr.CreateGeometryFromWkt('CIRCULARSTRING (0 0,0.5 -0.5,1 0,0.5 0.5,0 0)')
    g2 = g1.GetLinearGeometry()
    g3 = g2.GetCurveGeometry()
    assert g3.Equals(g1)

    # 3 points full circle
    g1 = ogr.CreateGeometryFromWkt('CIRCULARSTRING (0 0,1 0,0 0)')
    g2 = g1.GetLinearGeometry()
    g3 = g2.GetCurveGeometry()
    assert (g3.GetGeometryType() == ogr.wkbCircularString and \
       g1.GetPoint(0) == g3.GetPoint(0) and \
       g1.GetPoint(1) == g3.GetPoint(2) and \
       g1.GetPoint(2) == g3.GetPoint(4) and \
       abs((g3.GetX(1) - 0.5) * (g3.GetX(1) - 0.5) + g3.GetY(1) * g3.GetY(1) - 0.5 * 0.5) <= 1e-12 and \
       abs((g3.GetX(3) - 0.5) * (g3.GetX(3) - 0.5) + g3.GetY(3) * g3.GetY(3) - 0.5 * 0.5) <= 1e-12)

    # 3 points full circle in a CurvePolygon
    for wkt in ['CURVEPOLYGON( CIRCULARSTRING (0 0,1 0,0 0))',
                'CURVEPOLYGON( CIRCULARSTRING (0 0,0 1,0 0))',
                'CURVEPOLYGON( CIRCULARSTRING (0 0,-1 0,0 0))',
                'CURVEPOLYGON( CIRCULARSTRING (0 0,0 -1,0 0))']:
        g1 = ogr.CreateGeometryFromWkt(wkt)
        g2 = g1.GetLinearGeometry()
        g3 = g2.GetCurveGeometry()
        assert g3.Equals(g1)

    # 2 curves in the CircularString
    g1 = ogr.CreateGeometryFromWkt('CIRCULARSTRING (0 0,1 1,2 0,3 -1,4 0)')
    g2 = g1.GetLinearGeometry()
    g3 = g2.GetCurveGeometry()
    assert g3.Equals(g1)

    # 3 curves in the CircularString
    g1 = ogr.CreateGeometryFromWkt('CIRCULARSTRING (0 0,1 1,2 0,3 -1,4 0,5 1,6 0)')
    g2 = g1.GetLinearGeometry()
    g3 = g2.GetCurveGeometry()
    assert g3.Equals(g1)

    # CircularString, LineString, CircularString
    g1 = ogr.CreateGeometryFromWkt('COMPOUNDCURVE (CIRCULARSTRING (0 0,1 1,2 0),(2 0,3 0,4 0),CIRCULARSTRING (4 0,5 1,6 0))')
    g2 = g1.GetLinearGeometry()
    g3 = g2.GetCurveGeometry()
    assert g3.Equals(g1)

    # LineString, CircularString, LineString, CircularString, LineString
    g1 = ogr.CreateGeometryFromWkt('COMPOUNDCURVE ((-1 0,-0.5 0.5,0 0),CIRCULARSTRING (0 0,1 1,2 0),(2 0,3 0,4 0),CIRCULARSTRING (4 0,5 1,6 0),(6 0,7 0))')
    g2 = g1.GetLinearGeometry()
    g3 = g2.GetCurveGeometry()
    assert g3.Equals(g1)

    # Check with default discretization method
    g1 = ogr.CreateGeometryFromWkt('CIRCULARSTRING (0 0,1.2 1.0,2 0)')
    g2 = ogr.CreateGeometryFromWkt('LINESTRING (0 0,0.000997093138961 0.068920262501281,0.006738548124866 0.137608197923873,0.017197098230267 0.205737600590167,0.032323074784779 0.272984917348872,0.052044643056421 0.339030784160355,0.076268143401735 0.403561542787492,0.104878536064531 0.466270730389102,0.137739947510866 0.526860534941749,0.174696315705653 0.585043209577972,0.215572131266425 0.640542439124111,0.260173270974444 0.693094652347904,0.30828791968472 0.742450273683876,0.359687576256715 0.788374908491942,0.414128138728451 0.830650456220319,0.471351063580441 0.869076146186235,0.531084593583979 0.903469491055406,0.593045048402615 0.933667153492107,0.656938171817632 0.959525721864054,0.722460529179411 0.980922391318168,0.789300948448056 0.99775554699276,0.857141997979641 1.009945246596371,0.925661494039921 1.017433600061489,0.994534030886182 1.020185044470095,1.063432526150724 1.01818651294542,1.132029774186796 1.01144749670781,1.2 1.0,1.267547127648721 0.983752320094182,1.333803428245673 0.962854851656094,1.398449236893265 0.937408418110045,1.461172658788815 0.907535790143504,1.521671074014578 0.873381093378847,1.579652597579492 0.835109113014538,1.634837487668428 0.792904498790658,1.686959495304612 0.746970874114522,1.73576714891352 0.697529853644598,1.781024967590663 0.644819974072535,1.822514597219645 0.589095543261942,1.860035863959079 0.53062541329644,1.89340774001566 0.469691683356621,1.922469217043826 0.406588338684124,1.94708008295817 0.341619832199361,1.967121598410718 0.27509961561613,1.982497069669296 0.207348627140011,1.993132315133044 0.138693743046887,1.998976023234287 0.069466200612231,2 0)')
    g3 = g2.GetCurveGeometry()
    assert g3.Equals(g1)

    # Check with alternate discretization method : ROUND_ANGLE_METHOD
    g1 = ogr.CreateGeometryFromWkt('CIRCULARSTRING (0 0,1.2 1.0,2 0)')
    g2 = ogr.CreateGeometryFromWkt('LINESTRING (0 0,-0.000199980003999 0.02,0.002236456877416 0.089770423644023,0.009533897446083 0.159200932797352,0.021656789305088 0.227953268998521,0.038546070943884 0.295692477776518,0.060119459480029 0.362088540515299,0.086271851533127 0.426817982271309,0.116875835277923 0.489565447710894,0.151782311181914 0.550025237489786,0.190821218403283 0.607902797589577,0.233802363310224 0.662916154355295,0.280516346085201 0.7147972882427,0.330735580899807 0.763293439582565,0.384215404690045 0.80816834000038,0.440695269130182 0.849203363492117,0.499900009998002 0.886198591548151,0.561541187747236 0.918973787136141,0.62531849275604 0.947369272797732,0.690921208405283 0.971246708581086,0.758029724858684 0.990489766019218,0.826317096169807 1.005004694870594,0.895450633129846 1.014720779860886,0.965093524096011 1.019590685200679,1.034906475903993 1.019590685200679,1.104549366870158 1.014720779860886,1.173682903830197 1.005004694870593,1.2 1.0,1.241970275141317 0.990489766019217,1.309078791594718 0.971246708581085,1.374681507243961 0.947369272797731,1.438458812252765 0.91897378713614,1.500099990002 0.886198591548151,1.559304730869819 0.849203363492116,1.615784595309956 0.808168340000379,1.669264419100194 0.763293439582565,1.7194836539148 0.714797288242699,1.766197636689777 0.662916154355294,1.809178781596718 0.607902797589576,1.848217688818087 0.550025237489785,1.883124164722078 0.489565447710893,1.913728148466874 0.426817982271308,1.939880540519971 0.362088540515298,1.961453929056117 0.295692477776516,1.978343210694912 0.227953268998519,1.990466102553917 0.15920093279735,1.997763543122584 0.089770423644022,2.000199980003999 0.02,2 0)')
    g3 = g2.GetCurveGeometry()
    assert g3.Equals(g1)

    # Check with PostgreSQL output of SELECT ST_CurveToLine(ST_GeomFromText('CIRCULARSTRING (0 0,1.2 1.0,2 0)'))
    g1 = ogr.CreateGeometryFromWkt('CIRCULARSTRING (0 0,1.2 1.0,2 0)')
    g2 = ogr.CreateGeometryFromWkt('LINESTRING (0 0,0.000223190308279 0.049091765203314,0.002854930521212 0.098113445796116,0.007888880546112 0.146946944256066,0.015312913156447 0.195474616408063,0.025109143207391 0.243579554839372,0.037253970722702 0.291145870539817,0.051718137749134 0.338058972088558,0.068466798841411 0.384205841714863,0.08745960500795 0.429475307567812,0.108650800915124 0.473758311539029,0.131989335115863 0.516948171993215,0.157418983037062 0.558940840773549,0.184878482429505 0.599635153862819,0.214301680953989 0.638933075096389,0.245617695548099 0.676739932339917,0.27875108318972 0.712964645562815,0.31362202264588 0.747519946258017,0.350146506769097 0.780322587679462,0.388236544877951 0.811293545390794,0.427800374734344 0.840358207642151,0.46874268360677 0.867446555116406,0.510964837887032 0.892493329611833,0.554365120707246 0.915438191254836,0.598838976984681 0.936225863863983,0.644279265304116 0.954806268115175,0.690576516030889 0.971134642187118,0.737619195032841 0.985171649596477,0.785293972375802 0.996883473962907,0.833485995345338 1.006241900475673,0.88207916513699 1.013224383865605,0.930956416548473 1.017814102718623,0.98 1.02,1.029091765203309 1.019776809691721,1.078113445796111 1.017145069478788,1.12694694425606 1.012111119453889,1.175474616408058 1.004687086843554,1.223579554839367 0.994890856792611,1.271145870539812 0.9827460292773,1.318058972088553 0.968281862250867,1.364205841714858 0.951533201158591,1.409475307567807 0.932540394992052,1.453758311539024 0.911349199084878,1.49694817199321 0.88801066488414,1.538940840773545 0.862581016962941,1.579635153862814 0.835121517570498,1.618933075096384 0.805698319046015,1.656739932339913 0.774382304451905,1.692964645562811 0.741248916810284,1.727519946258013 0.706377977354124,1.760322587679458 0.669853493230907,1.791293545390791 0.631763455122053,1.820358207642148 0.59219962526566,1.847446555116403 0.551257316393235,1.872493329611831 0.509035162112973,1.895438191254833 0.465634879292759,1.916225863863981 0.421161023015324,1.934806268115173 0.37572073469589,1.951134642187117 0.329423483969116,1.965171649596476 0.282380804967164,1.976883473962906 0.234706027624203,1.986241900475672 0.186514004654668,1.993224383865604 0.137920834863015,1.997814102718623 0.089043583451532,2.0 0.04,2 0)')
    g3 = g2.GetCurveGeometry()
    g1_expected = ogr.CreateGeometryFromWkt('CIRCULARSTRING (0 0,0.98 1.02,2 0)')
    assert g3.Equals(g1_expected)

    # Test default ( implicit option ADD_INTERMEDIATE_POINT=STEALTH )
    g1 = ogr.CreateGeometryFromWkt('CIRCULARSTRING (0 0,1.2 1.0,2 0)')
    g2 = g1.GetLinearGeometry()
    g3 = g2.GetCurveGeometry()
    assert g3.Equals(g1)

    # Test with Z
    g1 = ogr.CreateGeometryFromWkt('CIRCULARSTRING (0 0 1,1 1 2,2 0 3)')
    g2 = g1.GetLinearGeometry()
    g3 = g2.GetCurveGeometry()
    assert g3.Equals(g1)

    g1 = ogr.CreateGeometryFromWkt('CIRCULARSTRING (2 0 3,1 1 2,0 0 1)')
    g2 = g1.GetLinearGeometry()
    g3 = g2.GetCurveGeometry()
    assert g3.Equals(g1)

    # Test option ADD_INTERMEDIATE_POINT=STEALTH
    g1 = ogr.CreateGeometryFromWkt('CIRCULARSTRING (0 0,1.2 1.0,2 0)')
    g2 = g1.GetLinearGeometry(options=['ADD_INTERMEDIATE_POINT=STEALTH'])
    g3 = g2.GetCurveGeometry()
    assert g3.Equals(g1)

    # Test option ADD_INTERMEDIATE_POINT=YES
    g1 = ogr.CreateGeometryFromWkt('CIRCULARSTRING (0 0,1.2 1.0,2 0)')
    g2 = g1.GetLinearGeometry(options=['ADD_INTERMEDIATE_POINT=YES'])
    g3 = g2.GetCurveGeometry()
    assert g3.Equals(g1)

    # Test with big coordinates. The points are (2,49),(3,50),(4,49) reprojected from EPSG:4326 to EPSG:32631
    g1 = ogr.CreateGeometryFromWkt('CIRCULARSTRING (426857.987717275 5427937.52346616,500000.000000001 5538630.70286887,573142.012282726 5427937.52346616)')
    g2 = g1.GetLinearGeometry()
    g3 = g2.GetCurveGeometry()
    assert ogrtest.check_feature_geometry(g3, g1) == 0

    # Same with integer coordinates
    g1 = ogr.CreateGeometryFromWkt('CIRCULARSTRING (426858 5427938,500000 5538632,573142 5427938)')
    g2 = g1.GetLinearGeometry()
    g3 = g2.GetCurveGeometry()
    assert g3.Equals(g1)

    # Test option ADD_INTERMEDIATE_POINT=FALSE
    g1 = ogr.CreateGeometryFromWkt('CIRCULARSTRING (0 0,1.2 1.0,2 0)')
    g2 = g1.GetLinearGeometry(options=['ADD_INTERMEDIATE_POINT=FALSE'])
    g3 = g2.GetCurveGeometry()
    g1_expected = ogr.CreateGeometryFromWkt('CIRCULARSTRING (0 0,1.0 1.020199980003999,2 0)')
    assert ogrtest.check_feature_geometry(g3, g1_expected) == 0

    # Test with unrecognized options
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    g2_new = g1.GetLinearGeometry(options=['bla', 'ADD_INTERMEDIATE_POINT=FALSE', 'foo=bar'])
    gdal.PopErrorHandler()
    assert g2_new.Equals(g2)

    # Add repeated point at end of line
    g2 = g1.GetLinearGeometry()
    g2.AddPoint_2D(2 - 1e-9, 0)
    g3 = g2.GetCurveGeometry()
    assert g3.Equals(g1)

    # Add repeated point at end of line
    g2 = g1.GetLinearGeometry()
    g2_new = ogr.Geometry(ogr.wkbLineString)
    g2_new.AddPoint_2D(0, 1e-9)
    for i in range(g2.GetPointCount()):
        g2_new.AddPoint_2D(g2.GetX(i), g2.GetY(i))
    g3 = g2_new.GetCurveGeometry()
    assert g3.Equals(g1)

    # Test various configurations
    for (wkt, eps) in [('CIRCULARSTRING (0 0,0.5 0.5,0 1,-0.5 0.5,0 0)', 0),
                       ('CIRCULARSTRING (0 0,-0.5 0.5,0 1,0.5 0.5,0 0)', 0),
                       ('CIRCULARSTRING (0 0,0.5 -0.5,0 -1,-0.5 -0.5,0 0)', 0),
                       ('CIRCULARSTRING (0 0,-0.5 -0.5,0 -1,0.5 -0.5,0 0)', 0),
                       ('CIRCULARSTRING (-1 -1,-1 1,1 1,1 -1,-1 -1)', 0),
                       ('CIRCULARSTRING (-1 -1,1 -1,1 1,-1 1,-1 -1)', 0),
                       ('CIRCULARSTRING (0 1,1 0,0 -1,-1 0,0 1)', 0),
                       ('CIRCULARSTRING (0 0.01,0.01 0,0 -0.01,-0.01 0,0 0.01)', 0),
                       ('CIRCULARSTRING (0 0.0001,0.0001 0,0 -0.0001,-0.0001 0,0 0.0001)', 0),
                       ('CIRCULARSTRING (0 1000000,1000000 0,0 -1000000,-1000000 0,0 1000000)', 0),
                       ('CIRCULARSTRING (1234567 8901234,5678901 23456789,0123456 78901234)', 0),
                       ('CIRCULARSTRING (1234567.12 8901234.34,5678901.56 23456789.01,0123456.78 78901234.56)', 1e-1),
                       ('CIRCULARSTRING (1234567 -8901234,-5678901 23456789,0123456 -78901234)', 0),
                       ('CIRCULARSTRING (0 0.000001,0.000001 0,0 -0.000001,-0.000001 0,0 0.000001)', 1e-12),
                       ('CIRCULARSTRING (0 0.00000001,0.00000001 0,0 -0.00000001,-0.00000001 0,0 0.00000001)', 1e-12),
                       ('CIRCULARSTRING (1 0.00000001,1.00000001 0,1 -0.00000001,0.99999999 0,1 0.00000001)', 1e-12),
                       ('CIRCULARSTRING (100000000 1,100000001 0,100000000 -1,99999999 0,100000000 1)', 0),
                       ('CIRCULARSTRING (-100000000 1,-100000001 0,-100000000 -1,-99999999 0,-100000000 1)', 0),
                       ('CIRCULARSTRING (100000000 100000001,100000001 100000000,100000000 99999999,99999999 100000000,100000000 100000001)', 0),
                       ('CIRCULARSTRING (760112.098000001162291 207740.096999999135733,760116.642489952617325 207741.101843414857285,760120.967999998480082 207742.820000000298023,760123.571822694852017 207744.275888498465065,760126.011999998241663 207745.991999998688698,760127.330062366439961 207747.037052432337077,760128.585999999195337 207748.155999999493361)', 1e-5)]:
        g1 = ogr.CreateGeometryFromWkt(wkt)
        g2 = g1.GetLinearGeometry()
        g3 = g2.GetCurveGeometry()
        assert not (eps == 0 and not g3.Equals(g1)) or (eps > 0 and ogrtest.check_feature_geometry(g3, g1, eps) != 0), \
            ''

    # Test with GEOMETRYCOLLECTION container
    g1 = ogr.CreateGeometryFromWkt('GEOMETRYCOLLECTION(CIRCULARSTRING (0 0,0.5 0.5,0 1,-0.5 0.5,0 0))')
    g2 = g1.GetLinearGeometry()
    g3 = g2.GetCurveGeometry()
    assert g3.Equals(g1)

    # Test with MULTICURVE container
    g1 = ogr.CreateGeometryFromWkt('MULTICURVE(CIRCULARSTRING (0 0,0.5 0.5,0 1,-0.5 0.5,0 0))')
    g2 = g1.GetLinearGeometry()
    g3 = g2.GetCurveGeometry()
    assert g3.Equals(g1)

    # Test with MULTISURFACE container
    g1 = ogr.CreateGeometryFromWkt('MULTISURFACE(CURVEPOLYGON(COMPOUNDCURVE(CIRCULARSTRING (0 0,0.5 0.5,1 0),(1 0,0 0))))')
    g2 = g1.GetLinearGeometry()
    g3 = g2.GetCurveGeometry()
    assert g3.Equals(g1)

    if ogrtest.have_geos():
        g1 = ogr.CreateGeometryFromWkt('CURVEPOLYGON(CIRCULARSTRING (0 0,2 0,0 0))')
        g2 = ogr.CreateGeometryFromWkt('CURVEPOLYGON(CIRCULARSTRING (1 0,3 1,1 0))')
        g3 = g1.Intersection(g2)
        assert g3.GetGeometryType() == ogr.wkbCurvePolygon

        g1 = ogr.CreateGeometryFromWkt('CURVEPOLYGON(CIRCULARSTRING (0 0,2 0,0 0))')
        g2 = ogr.CreateGeometryFromWkt('CURVEPOLYGON((1 -1,1 1,3 1,3 -1,1 -1))')
        g3 = g1.Intersection(g2)
        assert g3.GetGeometryType() == ogr.wkbCurvePolygon

        g1 = ogr.CreateGeometryFromWkt('CURVEPOLYGON(CIRCULARSTRING (0 0,2 0,0 0))')
        g2 = ogr.CreateGeometryFromWkt('CURVEPOLYGON(CIRCULARSTRING (3 0,5 0,3 0))')
        g3 = g1.Union(g2)
        assert g3.ExportToWkt() == 'MULTISURFACE (CURVEPOLYGON (CIRCULARSTRING (0 0,2 0,0 0)),CURVEPOLYGON (CIRCULARSTRING (3 0,5 0,3 0)))'

        g1 = ogr.CreateGeometryFromWkt('CIRCULARSTRING (0 0,1 1,2 0))')
        g2 = ogr.CreateGeometryFromWkt('CIRCULARSTRING (2 0,1 -1,0 0))')
        g3 = g1.Union(g2)
        assert g3.ExportToWkt() == 'MULTICURVE (CIRCULARSTRING (0 0,1 1,2 0),CIRCULARSTRING (2 0,1 -1,0 0))' or \
               g3.ExportToWkt() == 'MULTICURVE (CIRCULARSTRING (2 0,1 -1,0 0),CIRCULARSTRING (0 0,1 1,2 0))'  # GEOS OverlayNG

        g1 = ogr.CreateGeometryFromWkt('POINT(1 2)')
        g1 = g1.Buffer(0.5)
        g1 = g1.GetCurveGeometry()
        assert g1.ExportToWkt() == 'CURVEPOLYGON (CIRCULARSTRING (1.5 2.0,0.5 2.0,1.5 2.0))'

    
###############################################################################
# Test OGR_GT_ functions


def test_ogr_geom_gt_functions():

    # GT_HasZ
    tuples = [(ogr.wkbPoint, 0),
              (ogr.wkbPoint25D, 1),
              (ogr.wkbPointM, 0),
              (ogr.wkbPointZM, 1),
              (ogr.wkbCircularString, 0),
              (ogr.wkbCircularStringZ, 1),
              (ogr.wkbCircularStringM, 0),
              (ogr.wkbCircularStringZM, 1)]
    for (gt, res) in tuples:
        assert ogr.GT_HasZ(gt) == res

    # GT_SetZ
    tuples = [(ogr.wkbPoint, ogr.wkbPoint25D),
              (ogr.wkbPoint25D, ogr.wkbPoint25D),
              (ogr.wkbPointM, ogr.wkbPointZM),
              (ogr.wkbPointZM, ogr.wkbPointZM),
              (ogr.wkbCircularString, ogr.wkbCircularStringZ),
              (ogr.wkbCircularStringZ, ogr.wkbCircularStringZ),
              (ogr.wkbCircularStringM, ogr.wkbCircularStringZM),
              (ogr.wkbCircularStringZM, ogr.wkbCircularStringZM)]
    for (gt, res) in tuples:
        assert ogr.GT_SetZ(gt) == res

    # GT_HasM
    tuples = [(ogr.wkbPoint, 0),
              (ogr.wkbPoint25D, 0),
              (ogr.wkbPointM, 1),
              (ogr.wkbPointZM, 1),
              (ogr.wkbCircularString, 0),
              (ogr.wkbCircularStringZ, 0),
              (ogr.wkbCircularStringM, 1),
              (ogr.wkbCircularStringZM, 1)]
    for (gt, res) in tuples:
        assert ogr.GT_HasM(gt) == res

    # GT_SetM
    tuples = [(ogr.wkbPoint, ogr.wkbPointM),
              (ogr.wkbPoint25D, ogr.wkbPointZM),
              (ogr.wkbPointM, ogr.wkbPointM),
              (ogr.wkbPointZM, ogr.wkbPointZM),
              (ogr.wkbCircularString, ogr.wkbCircularStringM),
              (ogr.wkbCircularStringZ, ogr.wkbCircularStringZM),
              (ogr.wkbCircularStringM, ogr.wkbCircularStringM),
              (ogr.wkbCircularStringZM, ogr.wkbCircularStringZM)]
    for (gt, res) in tuples:
        assert ogr.GT_SetM(gt) == res

    # OGR_GT_SetModifier
    tuples = [(ogr.wkbPoint, 0, 0, ogr.wkbPoint),
              (ogr.wkbPoint, 1, 0, ogr.wkbPoint25D),
              (ogr.wkbPoint, 0, 1, ogr.wkbPointM),
              (ogr.wkbPoint, 1, 1, ogr.wkbPointZM),
              (ogr.wkbPoint25D, 0, 0, ogr.wkbPoint),
              (ogr.wkbPoint25D, 1, 0, ogr.wkbPoint25D),
              (ogr.wkbPoint25D, 0, 1, ogr.wkbPointM),
              (ogr.wkbPoint25D, 1, 1, ogr.wkbPointZM),
              (ogr.wkbPointM, 0, 0, ogr.wkbPoint),
              (ogr.wkbPointM, 1, 0, ogr.wkbPoint25D),
              (ogr.wkbPointM, 0, 1, ogr.wkbPointM),
              (ogr.wkbPointM, 1, 1, ogr.wkbPointZM),
              (ogr.wkbCircularString, 1, 0, ogr.wkbCircularStringZ),
              (ogr.wkbCircularStringZ, 1, 0, ogr.wkbCircularStringZ),
              (ogr.wkbPoint, 0, 0, ogr.wkbPoint),
              (ogr.wkbPoint25D, 0, 0, ogr.wkbPoint),
              (ogr.wkbCircularString, 0, 0, ogr.wkbCircularString),
              (ogr.wkbCircularStringZ, 0, 0, ogr.wkbCircularString)]
    for (gt, modZ, modM, res) in tuples:
        assert ogr.GT_SetModifier(gt, modZ, modM) == res

    # GT_Flatten
    tuples = [(ogr.wkbPoint, ogr.wkbPoint),
              (ogr.wkbPoint25D, ogr.wkbPoint),
              (ogr.wkbPointM, ogr.wkbPoint),
              (ogr.wkbPointZM, ogr.wkbPoint),
              (ogr.wkbCircularString, ogr.wkbCircularString),
              (ogr.wkbTriangleZ, ogr.wkbTriangle),
              (ogr.wkbCircularStringZ, ogr.wkbCircularString),
              (ogr.wkbCircularStringM, ogr.wkbCircularString),
              (ogr.wkbCircularStringZM, ogr.wkbCircularString)]
    for (gt, res) in tuples:
        assert ogr.GT_Flatten(gt) == res

    # GT_IsSubClassOf
    tuples = [(ogr.wkbPoint, ogr.wkbPoint, 1),
              (ogr.wkbPoint25D, ogr.wkbPoint, 1),
              (ogr.wkbPoint, ogr.wkbUnknown, 1),
              (ogr.wkbPoint, ogr.wkbLineString, 0),
              (ogr.wkbPolygon, ogr.wkbCurvePolygon, 1),
              (ogr.wkbTriangle, ogr.wkbCurvePolygon, 1),
              (ogr.wkbTriangle, ogr.wkbPolygon, 1),
              (ogr.wkbMultiSurface, ogr.wkbGeometryCollection, 1),
              (ogr.wkbMultiPolygon, ogr.wkbMultiSurface, 1),
              (ogr.wkbMultiLineString, ogr.wkbMultiCurve, 1),
              (ogr.wkbUnknown, ogr.wkbUnknown, 1),
              (ogr.wkbUnknown, ogr.wkbPoint, 0),
              (ogr.wkbTIN, ogr.wkbPolyhedralSurface, 1),
              (ogr.wkbPolyhedralSurface, ogr.wkbTIN, 0),
             ]
    for (gt, gt2, res) in tuples:
        assert ogr.GT_IsSubClassOf(gt, gt2) == res

    # GT_IsCurve
    tuples = [(ogr.wkbPoint, 0),
              (ogr.wkbPoint25D, 0),
              (ogr.wkbPointM, 0),
              (ogr.wkbPointZM, 0),
              (ogr.wkbCircularString, 1),
              (ogr.wkbCircularStringZ, 1),
              (ogr.wkbLineString, 1),
              (ogr.wkbCompoundCurve, 1),
              (ogr.wkbCompoundCurveZ, 1),
              (ogr.wkbCompoundCurveM, 1),
              (ogr.wkbCompoundCurveZM, 1),
              (ogr.wkbCurvePolygon, 0),
              (ogr.wkbTriangle, 0),
              (ogr.wkbPolyhedralSurface, 0),
              (ogr.wkbTIN, 0)]
    for (gt, res) in tuples:
        assert ogr.GT_IsCurve(gt) == res

    # GT_IsSurface
    tuples = [(ogr.wkbPoint, 0),
              (ogr.wkbPoint25D, 0),
              (ogr.wkbPointM, 0),
              (ogr.wkbPointZM, 0),
              (ogr.wkbCircularString, 0),
              (ogr.wkbCurvePolygon, 1),
              (ogr.wkbCurvePolygonZ, 1),
              (ogr.wkbCurvePolygonM, 1),
              (ogr.wkbCurvePolygonZM, 1),
              (ogr.wkbPolygon, 1),
              (ogr.wkbTriangle, 1),
              (ogr.wkbPolyhedralSurface, 1),
              (ogr.wkbTIN, 1)]
    for (gt, res) in tuples:
        assert ogr.GT_IsSurface(gt) == res

    # GT_GetCollection
    tuples = [(ogr.wkbPoint, ogr.wkbMultiPoint),
              (ogr.wkbPoint25D, ogr.wkbMultiPoint25D),
              (ogr.wkbPointM, ogr.wkbMultiPointM),
              (ogr.wkbPointZM, ogr.wkbMultiPointZM),
              (ogr.wkbCircularString, ogr.wkbMultiCurve),
              (ogr.wkbCompoundCurve, ogr.wkbMultiCurve),
              (ogr.wkbCurvePolygon, ogr.wkbMultiSurface),
              (ogr.wkbLineString, ogr.wkbMultiLineString),
              (ogr.wkbPolygon, ogr.wkbMultiPolygon)]
    for (gt, res) in tuples:
        assert ogr.GT_GetCollection(gt) == res

    # GT_IsNonLinear
    tuples = [(ogr.wkbPoint, 0),
              (ogr.wkbPoint25D, 0),
              (ogr.wkbPointM, 0),
              (ogr.wkbPointZM, 0),
              (ogr.wkbCircularString, 1),
              (ogr.wkbCircularStringM, 1),
              (ogr.wkbCircularStringZ, 1),
              (ogr.wkbCircularStringZM, 1),
              (ogr.wkbCompoundCurve, 1),
              (ogr.wkbCurvePolygon, 1),
              (ogr.wkbMultiCurve, 1),
              (ogr.wkbMultiSurface, 1),
              (ogr.wkbLineString, 0),
              (ogr.wkbPolygon, 0),
              (ogr.wkbTriangle, 0)]
    for (gt, res) in tuples:
        assert ogr.GT_IsNonLinear(gt) == res

    # GT_GetCurve
    tuples = [(ogr.wkbPoint, ogr.wkbPoint),
              (ogr.wkbPoint25D, ogr.wkbPoint25D),
              (ogr.wkbPointM, ogr.wkbPointM),
              (ogr.wkbPointZM, ogr.wkbPointZM),
              (ogr.wkbCircularString, ogr.wkbCircularString),
              (ogr.wkbCircularStringZ, ogr.wkbCircularStringZ),
              (ogr.wkbCircularStringM, ogr.wkbCircularStringM),
              (ogr.wkbCircularStringZM, ogr.wkbCircularStringZM),
              (ogr.wkbCompoundCurve, ogr.wkbCompoundCurve),
              (ogr.wkbCurvePolygon, ogr.wkbCurvePolygon),
              (ogr.wkbLineString, ogr.wkbCompoundCurve),
              (ogr.wkbPolygon, ogr.wkbCurvePolygon),
              (ogr.wkbMultiLineString, ogr.wkbMultiCurve),
              (ogr.wkbMultiPolygon, ogr.wkbMultiSurface),
              (ogr.wkbMultiCurve, ogr.wkbMultiCurve),
              (ogr.wkbMultiSurface, ogr.wkbMultiSurface)]
    for (gt, res) in tuples:
        assert ogr.GT_GetCurve(gt) == res

    # GT_GetLinear
    tuples = [(ogr.wkbPoint, ogr.wkbPoint),
              (ogr.wkbPoint25D, ogr.wkbPoint25D),
              (ogr.wkbPointM, ogr.wkbPointM),
              (ogr.wkbPointZM, ogr.wkbPointZM),
              (ogr.wkbCircularString, ogr.wkbLineString),
              (ogr.wkbCircularStringM, ogr.wkbLineStringM),
              (ogr.wkbCircularStringZ, ogr.wkbLineString25D),
              (ogr.wkbCircularStringZM, ogr.wkbLineStringZM),
              (ogr.wkbCompoundCurve, ogr.wkbLineString),
              (ogr.wkbCurvePolygon, ogr.wkbPolygon),
              (ogr.wkbLineString, ogr.wkbLineString),
              (ogr.wkbPolygon, ogr.wkbPolygon),
              (ogr.wkbMultiLineString, ogr.wkbMultiLineString),
              (ogr.wkbMultiPolygon, ogr.wkbMultiPolygon),
              (ogr.wkbMultiCurve, ogr.wkbMultiLineString),
              (ogr.wkbMultiSurface, ogr.wkbMultiPolygon)]
    for (gt, res) in tuples:
        assert ogr.GT_GetLinear(gt) == res

    
###############################################################################
# Limit cases


def test_ogr_geom_api_limit_tests():

    p = ogr.Geometry(ogr.wkbPoint)
    lyr = ogr.Geometry(ogr.wkbLineString)
    poly = ogr.Geometry(ogr.wkbPolygon)

    with gdaltest.error_handler():
        p.GetX(1)
        p.GetY(1)
        p.GetZ(1)

        lyr.GetX(1)
        lyr.GetY(1)
        lyr.GetZ(1)

        poly.GetX()
        poly.GetY()
        poly.GetZ()

        poly.GetPoints()

        p.GetPoint(1)
        lyr.GetPoint(1)
        poly.GetPoint(1)

        p.SetPoint(1, 0, 0)
        lyr.SetPoint(-1, 0, 0)
        poly.SetPoint(0, 0, 0)

        p.SetPoint_2D(1, 0, 0)
        lyr.SetPoint_2D(-1, 0, 0)
        poly.SetPoint_2D(0, 0, 0)

        poly.AddPoint(0, 0)

        poly.AddPoint_2D(0, 0)

        p.GetGeometryRef(1)

        p.AddGeometry(p)

        p.AddGeometryDirectly(p)

    
###############################################################################
# Test Equals


def test_ogr_geom_equals():

    p_empty = ogr.Geometry(ogr.wkbPoint)
    p_0 = ogr.CreateGeometryFromWkt('POINT (0 0)')
    p_1 = ogr.CreateGeometryFromWkt('POINT (1 1)')
    assert p_empty.Equals(p_empty)
    assert p_0.Equals(p_0)
    assert p_0.Equals(p_0.Clone())
    assert not p_empty.Equals(p_0)
    assert not p_0.Equals(p_empty)
    assert not p_0.Equals(p_1)

    l_empty = ogr.Geometry(ogr.wkbLineString)
    l_0_1 = ogr.CreateGeometryFromWkt('LINESTRING (0 0,1 1)')
    l_0_1_2 = ogr.CreateGeometryFromWkt('LINESTRING (0 0,1 1,2 2)')
    assert l_0_1.Equals(l_0_1)
    assert l_0_1.Equals(l_0_1.Clone())
    assert not l_empty.Equals(l_0_1)
    assert not l_0_1.Equals(l_empty)
    assert not l_0_1.Equals(l_0_1_2)

    gc_empty = ogr.Geometry(ogr.wkbGeometryCollection)
    gc_p_0 = ogr.CreateGeometryFromWkt('GEOMETRYCOLLECTION (POINT (0 0))')
    gc_p_1 = ogr.CreateGeometryFromWkt('GEOMETRYCOLLECTION (POINT (1 1))')
    assert gc_empty.Equals(gc_empty)
    assert gc_p_0.Equals(gc_p_0)
    assert gc_p_0.Equals(gc_p_0.Clone())
    assert not gc_empty.Equals(gc_p_0)
    assert not gc_p_0.Equals(gc_empty)
    assert not gc_p_0.Equals(gc_p_1)

###############################################################################
# Test FlattenTo2D(), SetCoordinateDimension(2) and SetCoordinateDimension(3) with Measured geometries


def test_ogr_geom_measured_geometries_to_2D_or_3D():

    list_wkt = [['POINT M (1 2 3)', 'POINT (1 2)', 'POINT Z (1 2 0)'],
                ['POINT ZM (1 2 3 4)', 'POINT (1 2)', 'POINT Z (1 2 3)'],
                ['LINESTRING M (1 2 3)', 'LINESTRING (1 2)', 'LINESTRING Z (1 2 0)'],
                ['LINESTRING ZM (1 2 3 4)', 'LINESTRING (1 2)', 'LINESTRING Z (1 2 3)'],
                ['POLYGON M ((1 2 3))', 'POLYGON ((1 2))', 'POLYGON Z ((1 2 0))'],
                ['POLYGON ZM ((1 2 3 4))', 'POLYGON ((1 2))', 'POLYGON Z ((1 2 3))'],
                ['CIRCULARSTRING M (1 2 3,4 5 6,7 8 9)', 'CIRCULARSTRING (1 2,4 5,7 8)', 'CIRCULARSTRING Z (1 2 0,4 5 0,7 8 0)'],
                ['CIRCULARSTRING ZM (1 2 3 0,4 5 6 0,7 8 9 0)', 'CIRCULARSTRING (1 2,4 5,7 8)', 'CIRCULARSTRING Z (1 2 3,4 5 6,7 8 9)'],
                ['COMPOUNDCURVE M ((1 2 3,4 5 6))', 'COMPOUNDCURVE ((1 2,4 5))', 'COMPOUNDCURVE Z ((1 2 0,4 5 0))'],
                ['COMPOUNDCURVE ZM ((1 2 3 4,5 6 7 8))', 'COMPOUNDCURVE ((1 2,5 6))', 'COMPOUNDCURVE Z ((1 2 3,5 6 7))'],
                ['MULTIPOINT M ((1 2 3))', 'MULTIPOINT ((1 2))', 'MULTIPOINT Z ((1 2 0))'],
                ['MULTIPOINT ZM ((1 2 3 4))', 'MULTIPOINT ((1 2))', 'MULTIPOINT Z ((1 2 3))'],
                ['MULTILINESTRING M ((1 2 3))', 'MULTILINESTRING ((1 2))', 'MULTILINESTRING Z ((1 2 0))'],
                ['MULTILINESTRING ZM ((1 2 3 4))', 'MULTILINESTRING ((1 2))', 'MULTILINESTRING Z ((1 2 3))'],
                ['MULTICURVE M ((1 2 3))', 'MULTICURVE ((1 2))', 'MULTICURVE Z ((1 2 0))'],
                ['MULTICURVE ZM ((1 2 3 4))', 'MULTICURVE ((1 2))', 'MULTICURVE Z ((1 2 3))'],
                ['MULTIPOLYGON M (((1 2 3)))', 'MULTIPOLYGON (((1 2)))', 'MULTIPOLYGON Z (((1 2 0)))'],
                ['MULTIPOLYGON ZM (((1 2 3 4)))', 'MULTIPOLYGON (((1 2)))', 'MULTIPOLYGON Z (((1 2 3)))'],
                ['MULTISURFACE M (((1 2 3)))', 'MULTISURFACE (((1 2)))', 'MULTISURFACE Z (((1 2 0)))'],
                ['MULTISURFACE ZM (((1 2 3 4)))', 'MULTISURFACE (((1 2)))', 'MULTISURFACE Z (((1 2 3)))'],
                ['GEOMETRYCOLLECTION M (POINT M (1 2 3))', 'GEOMETRYCOLLECTION (POINT (1 2))', 'GEOMETRYCOLLECTION Z (POINT Z (1 2 0))'],
                ['GEOMETRYCOLLECTION ZM (POINT ZM (1 2 3 4))', 'GEOMETRYCOLLECTION (POINT (1 2))', 'GEOMETRYCOLLECTION Z (POINT Z (1 2 3))'],
                ['TRIANGLE M ((0 0 3,0 1 3,1 1 3,0 0 3))', 'TRIANGLE ((0 0,0 1,1 1,0 0))', 'TRIANGLE Z ((0 0 0,0 1 0,1 1 0,0 0 0))'],
                ['TRIANGLE ZM ((0 0 3 4,0 1 3 4,1 1 3 4,0 0 3 4))', 'TRIANGLE ((0 0,0 1,1 1,0 0))', 'TRIANGLE Z ((0 0 3,0 1 3,1 1 3,0 0 3))'],
                ['POLYHEDRALSURFACE M (((0 0 3,0 1 3,1 1 3,0 0 3)))', 'POLYHEDRALSURFACE (((0 0,0 1,1 1,0 0)))', 'POLYHEDRALSURFACE Z (((0 0 0,0 1 0,1 1 0,0 0 0)))'],
                ['POLYHEDRALSURFACE ZM (((0 0 3 4,0 1 3 4,1 1 3 4,0 0 3 4)))', 'POLYHEDRALSURFACE (((0 0,0 1,1 1,0 0)))', 'POLYHEDRALSURFACE Z (((0 0 3,0 1 3,1 1 3,0 0 3)))'],
                ['TIN M (((0 0 3,0 1 3,1 1 3,0 0 3)))', 'TIN (((0 0,0 1,1 1,0 0)))', 'TIN Z (((0 0 0,0 1 0,1 1 0,0 0 0)))'],
                ['TIN ZM (((0 0 3 4,0 1 3 4,1 1 3 4,0 0 3 4)))', 'TIN (((0 0,0 1,1 1,0 0)))', 'TIN Z (((0 0 3,0 1 3,1 1 3,0 0 3)))'],
               ]
    for (before, after_2D, after_3D) in list_wkt:

        geom = ogr.CreateGeometryFromWkt(before)
        wkb = geom.ExportToIsoWkb()
        geom2 = ogr.CreateGeometryFromWkb(wkb)
        if not geom.Equals(geom2):
            print(before)
            pytest.fail(geom2.ExportToIsoWkt())

        geom = ogr.CreateGeometryFromWkt(before)
        geom.FlattenTo2D()
        assert geom.ExportToIsoWkt() == after_2D, before

        geom = ogr.CreateGeometryFromWkt(before)
        geom.SetCoordinateDimension(2)
        assert geom.ExportToIsoWkt() == after_2D, before

        geom = ogr.CreateGeometryFromWkt(before)
        geom.SetCoordinateDimension(3)
        assert geom.ExportToIsoWkt() == after_3D, before

        # Test no-op
        geom = ogr.CreateGeometryFromWkt(before)
        geom.Set3D(geom.Is3D())
        geom.SetMeasured(geom.IsMeasured())
        assert geom.ExportToIsoWkt() == before

    assert ogr.CreateGeometryFromWkt('POINT (1 2)').CoordinateDimension() == 2

    assert ogr.CreateGeometryFromWkt('POINT M (1 2 3)').CoordinateDimension() == 3

    assert ogr.CreateGeometryFromWkt('POINT Z (1 2 3)').CoordinateDimension() == 3

    assert ogr.CreateGeometryFromWkt('POINT ZM (1 2 3 4)').CoordinateDimension() == 4

    # Unrelated test. Test old-style unqualified non-bracketted ZM import
    g = ogr.CreateGeometryFromWkt('MULTIPOINT (1 2 3 4)')
    assert g.ExportToIsoWkt() == 'MULTIPOINT ZM ((1 2 3 4))'

###############################################################################
# Test PostGIS EWKT with XYM


def test_ogr_geom_postgis_ewkt_xym():

    list_wkt = [['POINTM(1 2 3)', 'POINT M (1 2 3)'],
                ['GEOMETRYCOLLECTIONM(POINTM(1 2 3))', 'GEOMETRYCOLLECTION M (POINT M (1 2 3))'],
               ]
    for (before, after) in list_wkt:
        geom = ogr.CreateGeometryFromWkt(before)
        assert geom.ExportToIsoWkt() == after, before

    
###############################################################################
# Test ogr.wkbCurve / ogr.wkbSurface


def test_ogr_geom_curve_surface():

    tests = [[ogr.wkbCurve, "Curve"],
             [ogr.wkbCurveZ, "3D Curve"],
             [ogr.wkbCurveM, "Measured Curve"],
             [ogr.wkbCurveZM, "3D Measured Curve"],
             [ogr.wkbSurface, "Surface"],
             [ogr.wkbSurfaceZ, "3D Surface"],
             [ogr.wkbSurfaceM, "Measured Surface"],
             [ogr.wkbSurfaceZM, "3D Measured Surface"]]

    for (wkb_type, name) in tests:
        assert ogr.GeometryTypeToName(wkb_type) == name

    
###############################################################################
# Test importing corrupted WKB


def test_ogr_geom_import_corrupted_wkb():

    list_wkt = ['POINT ZM (1 2 3 4)'
                'LINESTRING ZM (1 2 3 4)'
                'POLYGON ZM ((1 2 3 4))'
                'CIRCULARSTRING ZM (1 2 3 0,4 5 6 0,7 8 9 0)',
                'COMPOUNDCURVE ZM ((1 2 3 4,5 6 7 8))',
                'MULTIPOINT ZM ((1 2 3 4))',
                'MULTILINESTRING ZM ((1 2 3 4))',
                'MULTICURVE ZM ((1 2 3 4))',
                'MULTIPOLYGON ZM (((1 2 3 4)))',
                'MULTISURFACE ZM (((1 2 3 4)))',
                'GEOMETRYCOLLECTION ZM (POINT ZM (1 2 3 4))',
                'TRIANGLE ZM ((0 0 3 4,0 1 3 4,1 1 3 4,0 0 3 4))',
                'POLYHEDRALSURFACE ZM (((0 0 3 4,0 1 3 4,1 1 3 4,0 0 3 4)),((0 0 3 4,0 1 3 4,1 1 3 4,0 0 3 4)))',
                'TIN ZM (((0 0 3 4,0 1 3 4,1 1 3 4,0 0 3 4)),((0 0 3 4,0 1 3 4,1 1 3 4,0 0 3 4)))',
               ]

    for wkt in list_wkt:
        g = ogr.CreateGeometryFromWkt(wkt)
        wkb = bytearray(g.ExportToWkb())

        # Test altering the WKB
        for i, _ in enumerate(wkb):
            for method in range(4 + 4 + 4 + 1):
                init_val = wkb[i]
                if method < 4:
                    wkb[i] = method
                elif method < 8:
                    wkb[i] = 255 - (method - 4)
                elif method < 12:
                    wkb[i] = 127 + 2 - (method - 8)
                else:
                    wkb[i] = 255 - wkb[i]
                with gdaltest.error_handler():
                    g = ogr.CreateGeometryFromWkb(bytes(wkb))
                if g:
                    g2 = ogr.CreateGeometryFromWkb(g.ExportToIsoWkb())
                    assert g.Equals(g2), (wkt, i, method)
                wkb[i] = init_val

        # Test truncation of the WKB
        for i in range(len(wkb)):
            with gdaltest.error_handler():
                g = ogr.CreateGeometryFromWkb(bytes(wkb[0:i]))
            assert g is None, (wkt, i)

    
###############################################################################
# Test conversions from/into triangle, TIN, PS


def test_ogr_geom_triangle_ps_tin_conversion():

    wkts = ["TRIANGLE ((0 0,0 1,1 1,0 0))",
            "POLYGON ((0 0,0 1,1 1,0 0))",
            "CURVEPOLYGON ((0 0,0 1,1 1,0 0))",
            "MULTIPOLYGON (((0 0,0 1,1 1,0 0)))",
            "MULTISURFACE (((0 0,0 1,1 1,0 0)))",
            "TIN (((0 0,0 1,1 1,0 0)))",
            "POLYHEDRALSURFACE (((0 0,0 1,1 1,0 0)))"]

    for wkt in wkts:
        for wkt_target in wkts:
            # print(wkt, wkt_target)
            g = ogr.CreateGeometryFromWkt(wkt)
            g2 = ogr.CreateGeometryFromWkt(wkt_target)
            got_wkt = ogr.ForceTo(g, g2.GetGeometryType()).ExportToWkt()
            if not (g.GetGeometryType() in [ogr.wkbTriangle, ogr.wkbTIN, ogr.wkbPolyhedralSurface] or
                    g2.GetGeometryType() in [ogr.wkbTriangle, ogr.wkbTIN, ogr.wkbPolyhedralSurface]):
                continue
            wkt_expected = wkt_target
            if (g.GetGeometryType() == ogr.wkbTIN or g.GetGeometryType() == ogr.wkbPolyhedralSurface) and g2.GetGeometryType() == ogr.wkbCurvePolygon:
                wkt_expected = wkt
            assert got_wkt == wkt_expected, (wkt, wkt_target, got_wkt, wkt_expected)

    g = ogr.ForceTo(ogr.CreateGeometryFromWkt('POLYGON ((0 0,0 1,1 1,1 0,0 0))'), ogr.wkbTriangle)
    assert g.GetGeometryType() == ogr.wkbPolygon, g.ExportToWkt()

    g = ogr.ForceTo(ogr.CreateGeometryFromWkt('POLYHEDRALSURFACE (((0 0,0 1,1 1,1 0,0 0)))'), ogr.wkbTIN)
    assert g.GetGeometryType() == ogr.wkbPolyhedralSurface, g.ExportToWkt()

    g = ogr.ForceTo(ogr.CreateGeometryFromWkt('GEOMETRYCOLLECTION (POLYHEDRALSURFACE (((0 0,0 1,1 1,1 0,0 0))))'), ogr.wkbMultiPolygon)
    assert g.ExportToWkt() == 'MULTIPOLYGON (((0 0,0 1,1 1,1 0,0 0)))'

    g = ogr.ForceTo(ogr.CreateGeometryFromWkt('GEOMETRYCOLLECTION (TRIANGLE ((0 0,0 1,1 1,0 0)))'), ogr.wkbMultiPolygon)
    assert g.ExportToWkt() == 'MULTIPOLYGON (((0 0,0 1,1 1,0 0)))'

    g = ogr.ForceTo(ogr.CreateGeometryFromWkt('MULTIPOLYGON (((0 0,0 1,1 1,0 0)))'), ogr.wkbGeometryCollection)
    assert g.ExportToWkt() == 'GEOMETRYCOLLECTION (POLYGON ((0 0,0 1,1 1,0 0)))'

    g = ogr.ForceTo(ogr.CreateGeometryFromWkt('TRIANGLE ((0 0,0 1,1 1,0 0))'), ogr.wkbGeometryCollection)
    assert g.ExportToWkt() == 'GEOMETRYCOLLECTION (TRIANGLE ((0 0,0 1,1 1,0 0)))'

    g = ogr.ForceTo(ogr.CreateGeometryFromWkt('TIN (((0 0,0 1,1 1,0 0)))'), ogr.wkbGeometryCollection)
    assert g.ExportToWkt() == 'GEOMETRYCOLLECTION (POLYGON ((0 0,0 1,1 1,0 0)))'

    g = ogr.ForceTo(ogr.CreateGeometryFromWkt('POLYHEDRALSURFACE (((0 0,0 1,1 1,0 0)))'), ogr.wkbGeometryCollection)
    assert g.ExportToWkt() == 'GEOMETRYCOLLECTION (POLYGON ((0 0,0 1,1 1,0 0)))'

###############################################################################


def test_ogr_geom_multipoint_envelope_bug():

    g = ogr.CreateGeometryFromWkt('MULTIPOINT(0 0,1 1)')
    minx, maxx, miny, maxy = g.GetEnvelope()
    assert (minx, maxx, miny, maxy) == (0, 1, 0, 1)

    g = ogr.CreateGeometryFromWkt('MULTIPOINT(0 0 0,1 1 1)')
    minx, maxx, miny, maxy, minz, maxz = g.GetEnvelope3D()
    assert (minx, maxx, miny, maxy, minz, maxz) == (0, 1, 0, 1, 0, 1)

###############################################################################


def test_ogr_geom_polygon_empty_ring():

    g = ogr.Geometry(ogr.wkbPolygon)
    g2 = ogr.Geometry(ogr.wkbLinearRing)
    g.AddGeometryDirectly(g2)
    assert g.IsEmpty()

###############################################################################


def test_ogr_geom_polygon_intersects_point():

    if not ogrtest.have_geos():
        pytest.skip()

    poly = ogr.CreateGeometryFromWkt('POLYGON((0 0,5 5,10 0,0 0))')
    point = ogr.Geometry(ogr.wkbPoint)
    point.AddPoint(10, 0)
    assert poly.Intersects(point) == 1
    assert poly.Contains(point) == 0

###############################################################################
# Test fix for #7128


def test_ogr_geom_geometrycollection():

    wkt_list = ['GEOMETRYCOLLECTION (POINT EMPTY)',
                'GEOMETRYCOLLECTION (LINESTRING EMPTY)',
                'GEOMETRYCOLLECTION (POLYGON EMPTY)',
                'GEOMETRYCOLLECTION (MULTIPOINT EMPTY)',
                'GEOMETRYCOLLECTION (MULTILINESTRING EMPTY)',
                'GEOMETRYCOLLECTION (MULTIPOLYGON EMPTY)',
                'GEOMETRYCOLLECTION (GEOMETRYCOLLECTION EMPTY)',
                'GEOMETRYCOLLECTION (CIRCULARSTRING EMPTY)',
                'GEOMETRYCOLLECTION (COMPOUNDCURVE EMPTY)',
                'GEOMETRYCOLLECTION (CURVEPOLYGON EMPTY)',
                'GEOMETRYCOLLECTION (MULTICURVE EMPTY)',
                'GEOMETRYCOLLECTION (MULTISURFACE EMPTY)',
                'GEOMETRYCOLLECTION (TRIANGLE EMPTY)',
                'GEOMETRYCOLLECTION (POLYHEDRALSURFACE EMPTY)',
                'GEOMETRYCOLLECTION (TIN EMPTY)']
    for wkt in wkt_list:
        g = ogr.CreateGeometryFromWkt(wkt)
        assert g.ExportToWkt() == wkt

    
###############################################################################
# Test fix for #7126


def test_ogr_geom_assignspatialref():

    g = ogr.CreateGeometryFromWkt('GEOMETRYCOLLECTION(POLYGON((0 0,0 1,1 1,0 0)),COMPOUNDCURVE(CIRCULARSTRING(0 0,1 1,2 0)),POLYHEDRALSURFACE(((0 0,0 1,1 1,0 0))))')
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4326)
    g.AssignSpatialReference(sr)
    assert g.GetGeometryRef(0).GetSpatialReference().ExportToWkt() == sr.ExportToWkt()
    assert g.GetGeometryRef(0).GetGeometryRef(0).GetSpatialReference().ExportToWkt() == sr.ExportToWkt()
    assert g.GetGeometryRef(1).GetSpatialReference().ExportToWkt() == sr.ExportToWkt()
    assert g.GetGeometryRef(1).GetGeometryRef(0).GetSpatialReference().ExportToWkt() == sr.ExportToWkt()
    assert g.GetGeometryRef(2).GetSpatialReference().ExportToWkt() == sr.ExportToWkt()
    assert g.GetGeometryRef(2).GetGeometryRef(0).GetSpatialReference().ExportToWkt() == sr.ExportToWkt()

###############################################################################


def test_ogr_geom_swapxy():

    g = ogr.CreateGeometryFromWkt('GEOMETRYCOLLECTION(POINT(1 2),LINESTRING(1 2,2 3),POLYGON((0 0,0 1,1 1,0 0)),COMPOUNDCURVE(CIRCULARSTRING(0 0,1 1,2 0)),POLYHEDRALSURFACE(((0 0,0 1,1 1,0 0))))')
    g.SwapXY()
    assert g.ExportToWkt() == 'GEOMETRYCOLLECTION (POINT (2 1),LINESTRING (2 1,3 2),POLYGON ((0 0,1 0,1 1,0 0)),COMPOUNDCURVE (CIRCULARSTRING (0 0,1 1,0 2)),POLYHEDRALSURFACE (((0 0,1 0,1 1,0 0))))'

###############################################################################


def test_ogr_geom_remove_geometry():

    # With GEOMETRYCOLLECTION
    g = ogr.CreateGeometryFromWkt('GEOMETRYCOLLECTION (POINT (1 2),LINESTRING (1 2,2 3),POINT (3 4))')
    assert not (g.RemoveGeometry(3) == 0 or g.RemoveGeometry(-2) == 0)

    assert g.RemoveGeometry(1) == 0
    assert g.ExportToWkt() == 'GEOMETRYCOLLECTION (POINT (1 2),POINT (3 4))'

    assert g.RemoveGeometry(1) == 0
    assert g.ExportToWkt() == 'GEOMETRYCOLLECTION (POINT (1 2))'

    assert g.RemoveGeometry(0) == 0
    assert g.ExportToWkt() == 'GEOMETRYCOLLECTION EMPTY'

    g = ogr.CreateGeometryFromWkt('GEOMETRYCOLLECTION (POINT (1 2),LINESTRING (1 2,2 3),POINT (3 4))')
    assert g.RemoveGeometry(-1) == 0
    assert g.ExportToWkt() == 'GEOMETRYCOLLECTION EMPTY'

    # With POLYHEDRALSURFACE/TIN
    g = ogr.CreateGeometryFromWkt('TIN (((0 0,0 1,1 1,0 0)),((0 0,1 0,1 1,0 0)))')
    assert not (g.RemoveGeometry(2) == 0 or g.RemoveGeometry(-2) == 0)

    assert g.RemoveGeometry(1) == 0
    assert g.ExportToWkt() == 'TIN (((0 0,0 1,1 1,0 0)))'

    assert g.RemoveGeometry(0) == 0
    assert g.ExportToWkt() == 'TIN EMPTY'

    g = ogr.CreateGeometryFromWkt('TIN (((0 0,0 1,1 1,0 0)),((0 0,1 0,1 1,0 0)))')
    assert g.RemoveGeometry(-1) == 0
    assert g.ExportToWkt() == 'TIN EMPTY'

    # With POLYGON
    g = ogr.CreateGeometryFromWkt('POLYGON ((0 0,0 10,10 10,10 0,0 0),(1 1,1 9,9 9,1 1))')
    assert not (g.RemoveGeometry(2) == 0 or g.RemoveGeometry(-2) == 0)

    assert g.RemoveGeometry(1) == 0
    assert g.ExportToWkt() == 'POLYGON ((0 0,0 10,10 10,10 0,0 0))'

    assert g.RemoveGeometry(0) == 0
    assert g.ExportToWkt() == 'POLYGON EMPTY'

    g = ogr.CreateGeometryFromWkt('POLYGON ((0 0,0 10,10 10,10 0,0 0),(1 1,1 9,9 9,1 1))')
    assert g.RemoveGeometry(-1) == 0
    assert g.ExportToWkt() == 'POLYGON EMPTY'

    # Unsupported type
    g = ogr.CreateGeometryFromWkt('POINT (0 0)')
    assert g.RemoveGeometry(0) != 0

###############################################################################


def test_ogr_geom_sfcgal():

    if not ogrtest.have_sfcgal():
        pytest.skip()

    g1 = ogr.CreateGeometryFromWkt('TIN EMPTY')

    g2_poly = ogr.CreateGeometryFromWkt('POLYGON((0 0,0 1,1 1,0 0))')
    g2 = g2_poly.GetGeometryRef(0)
    g1.Distance(g2)

    g2 = ogr.CreateGeometryFromWkt('CIRCULARSTRING EMPTY')
    g1.Distance(g2)

    g2 = ogr.CreateGeometryFromWkt('CURVEPOLYGON EMPTY')
    g1.Distance(g2)

###############################################################################


def test_ogr_geom_cannot_add_triangle_to_multisurface():

    g = ogr.Geometry(ogr.wkbMultiSurface)
    assert g.AddGeometry(ogr.Geometry(ogr.wkbTriangle)) != 0

###############################################################################


def test_ogr_geom_force_polygonzm_to_linestring():
    g = ogr.CreateGeometryFromWkt('POLYGON ZM ((0 0 10 20,0 1 30 40,1 1 50 60,0 0 10 70))')
    wkt = ogr.ForceToLineString(g).ExportToIsoWkt()
    assert wkt == 'LINESTRING ZM (0 0 10 20,0 1 30 40,1 1 50 60,0 0 10 70)'

###############################################################################


def test_ogr_geom_force_polygonzm_to_multilinestring():
    g = ogr.CreateGeometryFromWkt('POLYGON ZM ((0 0 10 20,0 1 30 40,1 1 50 60,0 0 10 70))')
    wkt = ogr.ForceToMultiLineString(g).ExportToIsoWkt()
    assert wkt == 'MULTILINESTRING ZM ((0 0 10 20,0 1 30 40,1 1 50 60,0 0 10 70))'

###############################################################################


def test_ogr_geom_create_from_wkt_polyhedrasurface():
    g = ogr.CreateGeometryFromWkt('POLYHEDRALSURFACE (((0 0 0,0 0 1,0 1 1,0 1 0,0 0 0)))')
    assert g.ExportToWkt() == 'POLYHEDRALSURFACE Z (((0 0 0,0 0 1,0 1 1,0 1 0,0 0 0)))'

###############################################################################


def test_ogr_geom_makevalid():

    if not ogrtest.have_geos():
        pytest.skip()

    g = ogr.CreateGeometryFromWkt('POINT (0 0)')
    g = g.MakeValid()
    assert g is None or g.ExportToWkt() == 'POINT (0 0)'

    g = ogr.CreateGeometryFromWkt('POINT EMPTY')
    g = g.MakeValid()
    assert g is None or g.ExportToWkt() == 'POINT EMPTY'

    g = ogr.CreateGeometryFromWkt('LINESTRING (0 0)')
    with gdaltest.error_handler():
        g = g.MakeValid()
    assert not g

    g = ogr.CreateGeometryFromWkt('CURVEPOLYGON ((0 0,0 1,1 0,0 0))')
    g = g.MakeValid()
    assert g is None or g.ExportToWkt() == 'CURVEPOLYGON ((0 0,0 1,1 0,0 0))'

    # Invalid
    g = ogr.CreateGeometryFromWkt('POLYGON ((0 0,10 10,0 10,10 0,0 0))')
    g = g.MakeValid()
    assert g is None or ogrtest.check_feature_geometry(g, 'MULTIPOLYGON (((0 0,5 5,10 0,0 0)),((5 5,0 10,10 10,5 5)))') == 0, g.ExportToWkt()

    # Invalid
    g = ogr.CreateGeometryFromWkt('CURVEPOLYGON ((0 0,10 10,0 10,10 0,0 0))')
    g = g.MakeValid()
    assert g is None or ogrtest.check_feature_geometry(g, 'MULTIPOLYGON (((0 0,5 5,10 0,0 0)),((5 5,0 10,10 10,5 5)))') == 0, g.ExportToWkt()

    return 'success'

###############################################################################


def test_ogr_geom_force_multipolygon_z_to_compound_curve():

    g = ogr.CreateGeometryFromWkt('MULTIPOLYGON Z (((0 0 0,0 1 0,1 1 0,0 0 0)))')
    g = ogr.ForceTo(g, ogr.wkbCompoundCurve)
    assert g.ExportToIsoWkt() == 'COMPOUNDCURVE Z ((0 0 0,0 1 0,1 1 0,0 0 0))'

###############################################################################

@pytest.mark.parametrize(
    'input_wkt,expected_wkt',
    [
        ('POINT EMPTY', 'POINT EMPTY'),
        ('POINT (1 2)', 'POINT (1 2)'),
        ('POINT Z (1 2 3)', 'POINT Z (1 2 3)'),
        ('LINESTRING (1 2,3 4)', 'LINESTRING (1 2,3 4)'),
        ('POLYGON ((0 0,0 1,1 1,0 0))', 'POLYGON ((0 0,0 1,1 1,0 0))'),
        ('GEOMETRYCOLLECTION EMPTY', 'GEOMETRYCOLLECTION EMPTY'),
        ('GEOMETRYCOLLECTION Z (POINT Z (1 2 3))', 'POINT Z (1 2 3)'),
        ('GEOMETRYCOLLECTION (LINESTRING (1 2,3 4))', 'LINESTRING (1 2,3 4)'),
        ('GEOMETRYCOLLECTION (POLYGON ((0 0,0 1,1 1,0 0)))', 'POLYGON ((0 0,0 1,1 1,0 0))'),
        ('GEOMETRYCOLLECTION (LINESTRING (1 2,3 4),POINT (1 2))', 'LINESTRING (1 2,3 4)'),
        ('GEOMETRYCOLLECTION (POINT (1 2),LINESTRING (1 2,3 4))', 'LINESTRING (1 2,3 4)'),
        ('GEOMETRYCOLLECTION (LINESTRING (1 2,3 4),LINESTRING (5 6,7 8))', 'MULTILINESTRING ((1 2,3 4),(5 6,7 8))'),
        ('GEOMETRYCOLLECTION (POINT (1 2),POLYGON ((0 0,0 1,1 1,0 0)))', 'POLYGON ((0 0,0 1,1 1,0 0))'),
        ('GEOMETRYCOLLECTION (POLYGON ((0 0,0 1,1 1,0 0)),POLYGON ((10 0,10 1,11 1,10 0)))', 'MULTIPOLYGON (((0 0,0 1,1 1,0 0)),((10 0,10 1,11 1,10 0)))'),
        ('GEOMETRYCOLLECTION (POLYGON ((0 0,0 1,1 1,0 0)),MULTIPOLYGON (((10 0,10 1,11 1,10 0))))', 'MULTIPOLYGON (((0 0,0 1,1 1,0 0)),((10 0,10 1,11 1,10 0)))'),
        ('GEOMETRYCOLLECTION (CIRCULARSTRING (0 0,1 1,2 0),LINESTRING(3 4,5 6))', 'MULTICURVE (CIRCULARSTRING (0 0,1 1,2 0),(3 4,5 6))'),
        ('GEOMETRYCOLLECTION (POLYGON ((0 0,0 1,1 1,0 0)),CURVEPOLYGON ((10 0,10 1,11 1,10 0)))', 'MULTISURFACE (((0 0,0 1,1 1,0 0)),CURVEPOLYGON ((10 0,10 1,11 1,10 0)))'),
        ('GEOMETRYCOLLECTION (MULTIPOLYGON (((0 0,0 1,1 1,0 0)),((10 0,10 1,11 1,10 0))),POINT (1 2))', 'MULTIPOLYGON (((0 0,0 1,1 1,0 0)),((10 0,10 1,11 1,10 0)))'),
    ]
)
def test_ogr_geom_removeLowerDimensionSubGeoms(input_wkt, expected_wkt):

    g = ogr.CreateGeometryFromWkt(input_wkt)
    g = g.RemoveLowerDimensionSubGeoms()
    assert g.ExportToIsoWkt() == expected_wkt


###############################################################################

def test_ogr_geom_copy():

    g = ogr.CreateGeometryFromWkt('POINT (0 1)')
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4326)
    g.AssignSpatialReference(sr)

    g2 = copy.copy(g)
    assert g2.ExportToWkt() == g.ExportToWkt()
    assert g2.GetSpatialReference().IsSame(sr)
    sr.ImportFromEPSG(32631)
    assert g2.GetSpatialReference().IsSame(sr)


###############################################################################

def test_ogr_geom_deepcopy():

    g = ogr.CreateGeometryFromWkt('POINT (0 1)')
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4326)
    g.AssignSpatialReference(sr)

    g2 = copy.deepcopy(g)
    assert g2.ExportToWkt() == g.ExportToWkt()
    assert g2.GetSpatialReference().IsSame(sr)
    sr.ImportFromEPSG(32631)
    assert not g2.GetSpatialReference().IsSame(sr)
