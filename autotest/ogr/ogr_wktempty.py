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

gdaltest_list = []

for item in empty_wkt_list:
    ut = TestWktEmpty( item[0], item[1] )
    gdaltest_list.append( (ut.CheckIsEmpty, item[0]) )

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_wktempty' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

