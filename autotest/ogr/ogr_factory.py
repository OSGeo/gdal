#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test some geometry factory methods, like arc stroking.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2008, Frank Warmerdam <warmerdam@pobox.com>
# 
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.
###############################################################################

import os
import sys

sys.path.append( '../pymod' )

import ogrtest
import gdaltest
import ogr

def save_as_csv( geom, filename ):
    csv = 'ID,WKT\n0,"%s"\n' % geom.ExportToWkt()
    open('/home/warmerda/'+filename,'w').write(csv)

###############################################################################
# 30 degree rotated ellipse, just one quarter.

def ogr_factory_1():

    geom = ogr.ApproximateArcAngles( 20, 30, 40, 7, 3.5, 30.0, 270.0, 360.0, 6.0 )

    expected_geom = 'LINESTRING (21.75 33.031088913245533 40,22.374083449152831 32.648634669593925 40,22.972155943227843 32.237161430239802 40,23.537664874825239 31.801177382099848 40,24.064414409750082 31.345459257641004 40,24.546633369868303 30.875 40,24.979038463342047 30.394954059253475 40,25.356892169480634 29.910580919184319 40,25.676054644008637 29.427187473276717 40,25.933029076066084 28.95006988128063 40,26.125 28.484455543377237 40,26.249864142195264 28.035445827688662 40,26.306253464980482 27.607960178621322 40,26.293550155134998 27.206682218403525 40,26.211893392779814 26.836008432340218 40,26.062177826491073 26.5 40)'

    if ogrtest.check_feature_geometry( geom, expected_geom ):
        return 'fail'
    else:
        return 'success'

###############################################################################
# Test forceToPolygon()

def ogr_factory_2():

    src_wkt = 'MULTIPOLYGON (((0 0,100 0,100 100,0 0)))'
    exp_wkt = 'POLYGON((0 0,100 0,100 100,0 0))'

    src_geom = ogr.CreateGeometryFromWkt( src_wkt )
    dst_geom = ogr.ForceToPolygon( src_geom )

    if ogrtest.check_feature_geometry( dst_geom, exp_wkt ):
        print dst_geom.ExportToWkt()
        return 'fail'
    
    return 'success'

###############################################################################
# Test forceToMultiPolygon()

def ogr_factory_3():

    src_wkt = 'POLYGON((0 0,100 0,100 100,0 0))'
    exp_wkt = 'MULTIPOLYGON (((0 0,100 0,100 100,0 0)))'

    src_geom = ogr.CreateGeometryFromWkt( src_wkt )
    dst_geom = ogr.ForceToMultiPolygon( src_geom )

    if ogrtest.check_feature_geometry( dst_geom, exp_wkt ):
        print dst_geom.ExportToWkt()
        return 'fail'
    
    src_wkt = 'GEOMETRYCOLLECTION(POLYGON((0 0,100 0,100 100,0 0)))'
    exp_wkt = 'MULTIPOLYGON (((0 0,100 0,100 100,0 0)))'

    src_geom = ogr.CreateGeometryFromWkt( src_wkt )
    dst_geom = ogr.ForceToMultiPolygon( src_geom )

    if ogrtest.check_feature_geometry( dst_geom, exp_wkt ):
        print dst_geom.ExportToWkt()
        return 'fail'
    
    return 'success'

###############################################################################
# Test forceToMultiPoint()

def ogr_factory_4():

    src_wkt = 'POINT(2 5 3)'
    exp_wkt = 'MULTIPOINT(2 5 3)'

    src_geom = ogr.CreateGeometryFromWkt( src_wkt )
    dst_geom = ogr.ForceToMultiPoint( src_geom )

    if ogrtest.check_feature_geometry( dst_geom, exp_wkt ):
        print dst_geom.ExportToWkt()
        return 'fail'

    src_wkt = 'GEOMETRYCOLLECTION(POINT(2 5 3),POINT(4 5 5))'
    exp_wkt = 'MULTIPOINT(2 5 3,4 5 5)'

    src_geom = ogr.CreateGeometryFromWkt( src_wkt )
    dst_geom = ogr.ForceToMultiPoint( src_geom )

    if ogrtest.check_feature_geometry( dst_geom, exp_wkt ):
        print dst_geom.ExportToWkt()
        return 'fail'
    
    return 'success'

###############################################################################
# Test forceToMultiLineString()

def ogr_factory_5():

    src_wkt = 'LINESTRING(2 5,10 20)'
    exp_wkt = 'MULTILINESTRING((2 5,10 20))'

    src_geom = ogr.CreateGeometryFromWkt( src_wkt )
    dst_geom = ogr.ForceToMultiLineString( src_geom )

    if ogrtest.check_feature_geometry( dst_geom, exp_wkt ):
        print dst_geom.ExportToWkt()
        return 'fail'
    
    src_wkt = 'GEOMETRYCOLLECTION(LINESTRING(2 5,10 20),LINESTRING(0 0,10 10))'
    exp_wkt = 'MULTILINESTRING((2 5,10 20),(0 0,10 10))'

    src_geom = ogr.CreateGeometryFromWkt( src_wkt )
    dst_geom = ogr.ForceToMultiLineString( src_geom )

    if ogrtest.check_feature_geometry( dst_geom, exp_wkt ):
        print dst_geom.ExportToWkt()
        return 'fail'
    
    src_wkt = 'POLYGON((2 5,10 20),(0 0,10 10))'
    exp_wkt = 'MULTILINESTRING((2 5,10 20),(0 0,10 10))'

    src_geom = ogr.CreateGeometryFromWkt( src_wkt )
    dst_geom = ogr.ForceToMultiLineString( src_geom )

    if ogrtest.check_feature_geometry( dst_geom, exp_wkt ):
        print dst_geom.ExportToWkt()
        return 'fail'
    
    src_wkt = 'MULTIPOLYGON(((2 5,10 20),(0 0,10 10)),((2 5,10 20)))'
    exp_wkt = 'MULTILINESTRING((2 5,10 20),(0 0,10 10),(2 5,10 20))'

    src_geom = ogr.CreateGeometryFromWkt( src_wkt )
    dst_geom = ogr.ForceToMultiLineString( src_geom )

    if ogrtest.check_feature_geometry( dst_geom, exp_wkt ):
        print dst_geom.ExportToWkt()
        return 'fail'
    return 'success'


###############################################################################
# Test robustness of forceToXXX() primitives with various inputs (#3504)

def ogr_factory_6():

    src_wkt_list = [ None,
                     'POINT EMPTY',
                     'LINESTRING EMPTY',
                     'POLYGON EMPTY',
                     'MULTIPOINT EMPTY',
                     'MULTILINESTRING EMPTY',
                     'MULTIPOLYGON EMPTY',
                     'GEOMETRYCOLLECTION EMPTY',
                     'POINT(0 0)',
                     'LINESTRING(0 0)',
                     'POLYGON((0 0))',
                     'POLYGON(EMPTY,(0 0),EMPTY,(1 1))',
                     'MULTIPOINT(EMPTY,(0 0),EMPTY,(1 1))',
                     'MULTILINESTRING(EMPTY,(0 0),EMPTY,(1 1))',
                     'MULTIPOLYGON(((0 0),EMPTY,(1 1)),EMPTY,((2 2)))',
                     'GEOMETRYCOLLECTION(POINT EMPTY)',
                     'GEOMETRYCOLLECTION(LINESTRING EMPTY)',
                     'GEOMETRYCOLLECTION(POLYGON EMPTY)',
                     'GEOMETRYCOLLECTION(MULTIPOINT EMPTY)',
                     'GEOMETRYCOLLECTION(MULTILINESTRING EMPTY)',
                     'GEOMETRYCOLLECTION(MULTIPOLYGON EMPTY)',
                     'GEOMETRYCOLLECTION(GEOMETRYCOLLECTION EMPTY)',
                     'GEOMETRYCOLLECTION(POINT(0 0))',
                     'GEOMETRYCOLLECTION(LINESTRING(0 0),LINESTRING(1 1))',
                     'GEOMETRYCOLLECTION(POLYGON((0 0),EMPTY,(2 2)), POLYGON((1 1)))',
                      ]

    for src_wkt in src_wkt_list:
        if src_wkt is None:
            src_geom = None
        else:
            src_geom = ogr.CreateGeometryFromWkt( src_wkt )
        dst_geom1 = ogr.ForceToPolygon( src_geom )
        dst_geom2 = ogr.ForceToMultiPolygon( src_geom )
        dst_geom3 = ogr.ForceToMultiPoint( src_geom )
        dst_geom4 = ogr.ForceToMultiLineString( src_geom )
        #print(src_geom.ExportToWkt(), dst_geom1.ExportToWkt(), dst_geom2.ExportToWkt(), dst_geom3.ExportToWkt(), dst_geom4.ExportToWkt())

    return 'success'

gdaltest_list = [ 
    ogr_factory_1,
    ogr_factory_2,
    ogr_factory_3,
    ogr_factory_4,
    ogr_factory_5,
    ogr_factory_6,
    ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_factory' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

