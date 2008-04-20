#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test support for the various "EMPTY" WKT geometry representations.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
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

import os
import sys
import string

sys.path.append( '../pymod' )

import gdaltest
import ogrtest
import ogr
import gdal

class TestWktEmpty:
    def __init__( self, inString, expectedOutString ):
        self.inString = inString
        self.expectedOutString = expectedOutString

    def isEmpty(self, geom):
        try:
            ogr.Geometry.IsEmpty
        except:
            return 'skip'

        if (geom.IsEmpty() == False):
            geom.Destroy()
            gdaltest.post_reason ("IsEmpty returning false for an empty geometry")
            return 'fail'

        return 'success'

    def CheckIsEmpty(self):
        geom = ogr.CreateGeometryFromWkt( self.inString )
        wkt = geom.ExportToWkt()

        if self.expectedOutString != 'POINT EMPTY':
            if ogr.CreateGeometryFromWkb(geom.ExportToWkb()).ExportToWkt() != wkt:
                return 'fail'

        if wkt == self.expectedOutString:
            if self.isEmpty(geom) == 'fail':
                return 'fail'
            else:
                return 'success'
        else:
            gdaltest.post_reason( 'WKT is wrong: ' + wkt + '. Expected value is: ' + self.expectedOutString )
            return 'fail'

empty_wkt_list = [ \
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


def ogr_wktempty_test_partial_empty_geoms():

    # Multipoint with a valid point and an empty point
    wkt = 'MULTIPOINT (1 1)'
    geom = ogr.CreateGeometryFromWkt(wkt)
    geom.AddGeometry(ogr.Geometry( type = ogr.wkbPoint ))
    if geom.ExportToWkt() != wkt:
        gdaltest.post_reason( 'WKT is wrong: ' + geom.ExportToWkt() + '. Expected value is: ' + wkt )
        return 'fail'

    # Multipoint with an empty point and a valid point
    geom = ogr.CreateGeometryFromWkt('MULTIPOINT EMPTY')
    geom.AddGeometry(ogr.Geometry( type = ogr.wkbPoint ))
    geom.AddGeometry(ogr.CreateGeometryFromWkt('POINT (1 1)'))
    wkt = 'MULTIPOINT (1 1)'
    if geom.ExportToWkt() != wkt:
        gdaltest.post_reason( 'WKT is wrong: ' + geom.ExportToWkt() + '. Expected value is: ' + wkt )
        return 'fail'

    # Multilinestring with a valid string and an empty linestring
    wkt = 'MULTILINESTRING ((0 1,2 3,4 5,0 1))'
    geom = ogr.CreateGeometryFromWkt(wkt)
    geom.AddGeometry(ogr.Geometry( type = ogr.wkbLineString ))
    if geom.ExportToWkt() != wkt:
        gdaltest.post_reason( 'WKT is wrong: ' + geom.ExportToWkt() + '. Expected value is: ' + wkt )
        return 'fail'

    # Multilinestring with an empty linestring and a valid linestring
    geom = ogr.CreateGeometryFromWkt('MULTILINESTRING EMPTY')
    geom.AddGeometry(ogr.Geometry( type = ogr.wkbLineString ))
    geom.AddGeometry(ogr.CreateGeometryFromWkt('LINESTRING (0 1,2 3,4 5,0 1)'))
    wkt = 'MULTILINESTRING ((0 1,2 3,4 5,0 1))'
    if geom.ExportToWkt() != wkt:
        gdaltest.post_reason( 'WKT is wrong: ' + geom.ExportToWkt() + '. Expected value is: ' + wkt )
        return 'fail'

    # Polygon with a valid external ring and an empty internal ring
    wkt = 'POLYGON ((100 0,100 10,110 10,100 0))'
    geom = ogr.CreateGeometryFromWkt(wkt)
    geom.AddGeometry(ogr.Geometry( type = ogr.wkbLinearRing ))
    if geom.ExportToWkt() != wkt:
        gdaltest.post_reason( 'WKT is wrong: ' + geom.ExportToWkt() + '. Expected value is: ' + wkt )
        return 'fail'

    # Polygon with an empty external ring and a valid internal ring
    wkt = 'POLYGON EMPTY'
    geom = ogr.CreateGeometryFromWkt(wkt)
    geom.AddGeometry(ogr.Geometry( type = ogr.wkbLinearRing ))
    ring = ogr.Geometry( type = ogr.wkbLinearRing )
    ring.AddPoint_2D( 0, 0)
    ring.AddPoint_2D( 10, 0)
    ring.AddPoint_2D( 10, 10)
    ring.AddPoint_2D( 0, 10)
    ring.AddPoint_2D( 0, 0)
    geom.AddGeometry(ring)
    if geom.ExportToWkt() != wkt:
        gdaltest.post_reason( 'WKT is wrong: ' + geom.ExportToWkt() + '. Expected value is: ' + wkt )
        return 'fail'

    # Multipolygon with a valid polygon and an empty polygon
    wkt = 'MULTIPOLYGON (((0 0,0 10,10 10,0 0)))'
    geom = ogr.CreateGeometryFromWkt(wkt)
    geom.AddGeometry(ogr.Geometry( type = ogr.wkbPolygon ))
    if geom.ExportToWkt() != wkt:
        gdaltest.post_reason( 'WKT is wrong: ' + geom.ExportToWkt() + '. Expected value is: ' + wkt )
        return 'fail'

    # Multipolygon with an empty polygon and a valid polygon
    geom = ogr.CreateGeometryFromWkt('MULTIPOLYGON EMPTY')
    geom.AddGeometry(ogr.Geometry( type = ogr.wkbPolygon ))
    geom.AddGeometry(ogr.CreateGeometryFromWkt('POLYGON ((100 0,100 10,110 10,100 0))'))
    wkt = 'MULTIPOLYGON (((100 0,100 10,110 10,100 0)))'
    if geom.ExportToWkt() != wkt:
        gdaltest.post_reason( 'WKT is wrong: ' + geom.ExportToWkt() + '. Expected value is: ' + wkt )
        return 'fail'

    return 'success'


gdaltest_list = []

for item in empty_wkt_list:
    ut = TestWktEmpty( item[0], item[1] )
    gdaltest_list.append( (ut.CheckIsEmpty, item[0]) )
gdaltest_list.append( ogr_wktempty_test_partial_empty_geoms )

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_wktempty' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

