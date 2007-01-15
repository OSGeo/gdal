#!/usr/bin/env python
###############################################################################
# $Id: ogr_wktempty.py,v 1.2 2005/07/12 17:33:17 fwarmerdam Exp $
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
# 
#  $Log: ogr_wktempty.py,v $
#  Revision 1.2  2005/07/12 17:33:17  fwarmerdam
#  Updated to proper empty syntax.
#
#  Revision 1.1  2004/02/25 15:18:55  warmerda
#  New
#
#

import os
import sys
import string

sys.path.append( '../pymod' )

import gdaltest
import ogrtest
import ogr
import gdal

def ogr_wktempty_1():

    geom = ogr.CreateGeometryFromWkt( 'GEOMETRYCOLLECTION(EMPTY)' )
    wkt = geom.ExportToWkt()

    if wkt == 'GEOMETRYCOLLECTION EMPTY':
        return 'success'
    else:
        gdaltest.post_reason( 'WKT is wrong: ' + wkt )
        return 'fail'

def ogr_wktempty_2():

    geom = ogr.CreateGeometryFromWkt( 'MULTIPOLYGON( EMPTY)' )
    wkt = geom.ExportToWkt()

    if wkt == 'MULTIPOLYGON EMPTY':
        return 'success'
    else:
        gdaltest.post_reason( 'WKT is wrong: ' + wkt )
        return 'fail'

def ogr_wktempty_3():

    geom = ogr.CreateGeometryFromWkt( ' MULTILINESTRING(EMPTY)' )
    wkt = geom.ExportToWkt()

    if wkt == 'MULTILINESTRING EMPTY':
        return 'success'
    else:
        gdaltest.post_reason( 'WKT is wrong: ' + wkt )
        return 'fail'

def ogr_wktempty_4():

    geom = ogr.CreateGeometryFromWkt( 'MULTIPOINT(EMPTY)' )
    wkt = geom.ExportToWkt()

    if wkt == 'MULTIPOINT EMPTY':
        return 'success'
    else:
        gdaltest.post_reason( 'WKT is wrong: ' + wkt )
        return 'fail'

def ogr_wktempty_5():

    geom = ogr.CreateGeometryFromWkt( 'POINT ( EMPTY )' )
    wkt = geom.ExportToWkt()

    if wkt == 'POINT (0 0)':
        return 'success'
    else:
        gdaltest.post_reason( 'WKT is wrong: ' + wkt )
        return 'fail'

def ogr_wktempty_6():

    geom = ogr.CreateGeometryFromWkt( 'LINESTRING(EMPTY)' )
    wkt = geom.ExportToWkt()

    if wkt == 'LINESTRING EMPTY':
        return 'success'
    else:
        gdaltest.post_reason( 'WKT is wrong: ' + wkt )
        return 'fail'

def ogr_wktempty_7():

    geom = ogr.CreateGeometryFromWkt( 'POLYGON ( EMPTY ) ' )
    wkt = geom.ExportToWkt()

    if wkt == 'POLYGON EMPTY':
        return 'success'
    else:
        gdaltest.post_reason( 'WKT is wrong: ' + wkt )
        return 'fail'

def ogr_wktempty_8():

    geom = ogr.CreateGeometryFromWkt( 'GEOMETRYCOLLECTION EMPTY' )
    wkt = geom.ExportToWkt()

    if wkt == 'GEOMETRYCOLLECTION EMPTY':
        return 'success'
    else:
        gdaltest.post_reason( 'WKT is wrong: ' + wkt )
        return 'fail'

def ogr_wktempty_9():

    geom = ogr.CreateGeometryFromWkt( 'MULTIPOLYGON EMPTY' )
    wkt = geom.ExportToWkt()

    if wkt == 'MULTIPOLYGON EMPTY':
        return 'success'
    else:
        gdaltest.post_reason( 'WKT is wrong: ' + wkt )
        return 'fail'

def ogr_wktempty_10():

    geom = ogr.CreateGeometryFromWkt( ' MULTILINESTRING EMPTY' )
    wkt = geom.ExportToWkt()

    if wkt == 'MULTILINESTRING EMPTY':
        return 'success'
    else:
        gdaltest.post_reason( 'WKT is wrong: ' + wkt )
        return 'fail'

def ogr_wktempty_11():

    geom = ogr.CreateGeometryFromWkt( 'MULTIPOINT EMPTY' )
    wkt = geom.ExportToWkt()

    if wkt == 'MULTIPOINT EMPTY':
        return 'success'
    else:
        gdaltest.post_reason( 'WKT is wrong: ' + wkt )
        return 'fail'

def ogr_wktempty_12():

    geom = ogr.CreateGeometryFromWkt( 'POINT EMPTY ' )
    wkt = geom.ExportToWkt()

    if wkt == 'POINT (0 0)':
        return 'success'
    else:
        gdaltest.post_reason( 'WKT is wrong: ' + wkt )
        return 'fail'

def ogr_wktempty_13():

    geom = ogr.CreateGeometryFromWkt( 'LINESTRING EMPTY' )
    wkt = geom.ExportToWkt()

    if wkt == 'LINESTRING EMPTY':
        return 'success'
    else:
        gdaltest.post_reason( 'WKT is wrong: ' + wkt )
        return 'fail'

def ogr_wktempty_14():

    geom = ogr.CreateGeometryFromWkt( 'POLYGON  EMPTY  ' )
    wkt = geom.ExportToWkt()

    if wkt == 'POLYGON EMPTY':
        return 'success'
    else:
        gdaltest.post_reason( 'WKT is wrong: ' + wkt )
        return 'fail'
    
gdaltest_list = [ 
    ogr_wktempty_1,
    ogr_wktempty_2,
    ogr_wktempty_3,
    ogr_wktempty_4,
    ogr_wktempty_5,
    ogr_wktempty_6,
    ogr_wktempty_7,
    ogr_wktempty_8,
    ogr_wktempty_9,
    ogr_wktempty_10,
    ogr_wktempty_11,
    ogr_wktempty_12,
    ogr_wktempty_13,
    ogr_wktempty_14,
    ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_wktgeom' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

