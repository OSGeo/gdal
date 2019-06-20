#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test support for the various "EMPTY" WKT geometry representations.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2008, Even Rouault <even dot rouault at spatialys.com>
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


import pytest

from osgeo import ogr

wkt_list = [
    ('GEOMETRYCOLLECTION(EMPTY)', 'GEOMETRYCOLLECTION EMPTY'),
    ('MULTIPOLYGON( EMPTY )', 'MULTIPOLYGON EMPTY'),
    ('MULTILINESTRING(EMPTY)', 'MULTILINESTRING EMPTY'),
    ('MULTIPOINT(EMPTY)', 'MULTIPOINT EMPTY'),
    ('POINT ( EMPTY )', 'POINT EMPTY'),
    ('LINESTRING(EMPTY)', 'LINESTRING EMPTY'),
    ('POLYGON ( EMPTY )', 'POLYGON EMPTY'),

    ('GEOMETRYCOLLECTION EMPTY', 'GEOMETRYCOLLECTION EMPTY'),
    ('MULTIPOLYGON EMPTY', 'MULTIPOLYGON EMPTY'),
    ('MULTILINESTRING EMPTY', 'MULTILINESTRING EMPTY'),
    ('MULTIPOINT EMPTY', 'MULTIPOINT EMPTY'),
    ('POINT EMPTY', 'POINT EMPTY'),
    ('LINESTRING EMPTY', 'LINESTRING EMPTY'),
    ('POLYGON EMPTY', 'POLYGON EMPTY')
]


@pytest.mark.parametrize(
    "test_input,expected",
    wkt_list,
    ids=[r[0] for r in wkt_list]
)
def test_empty_wkt(test_input, expected):
    geom = ogr.CreateGeometryFromWkt(test_input)
    wkt = geom.ExportToWkt()

    if expected != 'POINT EMPTY':
        assert ogr.CreateGeometryFromWkb(geom.ExportToWkb()).ExportToWkt() == wkt

    assert wkt == expected

    try:
        ogr.Geometry.IsEmpty
    except AttributeError:
        pytest.skip()

    try:
        assert geom.IsEmpty(), "IsEmpty returning false for an empty geometry"
    finally:
        geom.Destroy()


def test_ogr_wktempty_test_partial_empty_geoms():

    # Multipoint with a valid point and an empty point
    wkt = 'MULTIPOINT (1 1)'
    geom = ogr.CreateGeometryFromWkt(wkt)
    geom.AddGeometry(ogr.Geometry(type=ogr.wkbPoint))
    assert geom.ExportToWkt() == wkt, \
        ('WKT is wrong: ' + geom.ExportToWkt() + '. Expected value is: ' + wkt)

    # Multipoint with an empty point and a valid point
    geom = ogr.CreateGeometryFromWkt('MULTIPOINT EMPTY')
    geom.AddGeometry(ogr.Geometry(type=ogr.wkbPoint))
    geom.AddGeometry(ogr.CreateGeometryFromWkt('POINT (1 1)'))
    wkt = 'MULTIPOINT (1 1)'
    assert geom.ExportToWkt() == wkt, \
        ('WKT is wrong: ' + geom.ExportToWkt() + '. Expected value is: ' + wkt)

    # Multilinestring with a valid string and an empty linestring
    wkt = 'MULTILINESTRING ((0 1,2 3,4 5,0 1))'
    geom = ogr.CreateGeometryFromWkt(wkt)
    geom.AddGeometry(ogr.Geometry(type=ogr.wkbLineString))
    assert geom.ExportToWkt() == wkt, \
        ('WKT is wrong: ' + geom.ExportToWkt() + '. Expected value is: ' + wkt)

    # Multilinestring with an empty linestring and a valid linestring
    geom = ogr.CreateGeometryFromWkt('MULTILINESTRING EMPTY')
    geom.AddGeometry(ogr.Geometry(type=ogr.wkbLineString))
    geom.AddGeometry(ogr.CreateGeometryFromWkt('LINESTRING (0 1,2 3,4 5,0 1)'))
    wkt = 'MULTILINESTRING ((0 1,2 3,4 5,0 1))'
    assert geom.ExportToWkt() == wkt, \
        ('WKT is wrong: ' + geom.ExportToWkt() + '. Expected value is: ' + wkt)

    # Polygon with a valid external ring and an empty internal ring
    wkt = 'POLYGON ((100 0,100 10,110 10,100 0))'
    geom = ogr.CreateGeometryFromWkt(wkt)
    geom.AddGeometry(ogr.Geometry(type=ogr.wkbLinearRing))
    assert geom.ExportToWkt() == wkt, \
        ('WKT is wrong: ' + geom.ExportToWkt() + '. Expected value is: ' + wkt)

    # Polygon with an empty external ring and a valid internal ring
    wkt = 'POLYGON EMPTY'
    geom = ogr.CreateGeometryFromWkt(wkt)
    geom.AddGeometry(ogr.Geometry(type=ogr.wkbLinearRing))
    ring = ogr.Geometry(type=ogr.wkbLinearRing)
    ring.AddPoint_2D(0, 0)
    ring.AddPoint_2D(10, 0)
    ring.AddPoint_2D(10, 10)
    ring.AddPoint_2D(0, 10)
    ring.AddPoint_2D(0, 0)
    geom.AddGeometry(ring)
    assert geom.ExportToWkt() == wkt, \
        ('WKT is wrong: ' + geom.ExportToWkt() + '. Expected value is: ' + wkt)

    # Multipolygon with a valid polygon and an empty polygon
    wkt = 'MULTIPOLYGON (((0 0,0 10,10 10,0 0)))'
    geom = ogr.CreateGeometryFromWkt(wkt)
    geom.AddGeometry(ogr.Geometry(type=ogr.wkbPolygon))
    assert geom.ExportToWkt() == wkt, \
        ('WKT is wrong: ' + geom.ExportToWkt() + '. Expected value is: ' + wkt)

    # Multipolygon with an empty polygon and a valid polygon
    geom = ogr.CreateGeometryFromWkt('MULTIPOLYGON EMPTY')
    geom.AddGeometry(ogr.Geometry(type=ogr.wkbPolygon))
    geom.AddGeometry(ogr.CreateGeometryFromWkt('POLYGON ((100 0,100 10,110 10,100 0))'))
    wkt = 'MULTIPOLYGON (((100 0,100 10,110 10,100 0)))'
    assert geom.ExportToWkt() == wkt, \
        ('WKT is wrong: ' + geom.ExportToWkt() + '. Expected value is: ' + wkt)



