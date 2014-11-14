#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test some geometry factory methods, like arc stroking.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2008, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2010-2012, Even Rouault <even dot rouault at mines-paris dot org>
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
from osgeo import gdal
from osgeo import ogr

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
        print(dst_geom.ExportToWkt())
        return 'fail'

    src_wkt = 'MULTISURFACE (((0 0,100 0,100 100,0 0)))'
    exp_wkt = 'POLYGON((0 0,100 0,100 100,0 0))'

    src_geom = ogr.CreateGeometryFromWkt( src_wkt )
    dst_geom = ogr.ForceToPolygon( src_geom )

    if ogrtest.check_feature_geometry( dst_geom, exp_wkt ):
        print(dst_geom.ExportToWkt())
        return 'fail'

    src_wkt = 'CURVEPOLYGON ((0 0,100 0,100 100,0 0))'
    exp_wkt = 'POLYGON((0 0,100 0,100 100,0 0))'

    src_geom = ogr.CreateGeometryFromWkt( src_wkt )
    dst_geom = ogr.ForceToPolygon( src_geom )

    if ogrtest.check_feature_geometry( dst_geom, exp_wkt ):
        print(dst_geom.ExportToWkt())
        return 'fail'

    src_wkt = 'CURVEPOLYGON (CIRCULARSTRING(0 0,0 1,0 2,1 2,2 2,2 1,2 0,1 0,0 0))'
    exp_wkt = 'POLYGON ((0 0,0 1,0 2,1 2,2 2,2 1,2 0,1 0,0 0))'

    src_geom = ogr.CreateGeometryFromWkt( src_wkt )
    dst_geom = ogr.ForceToPolygon( src_geom )

    if ogrtest.check_feature_geometry( dst_geom, exp_wkt ):
        print(dst_geom.ExportToWkt())
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
        print(dst_geom.ExportToWkt())
        return 'fail'
    
    src_wkt = 'GEOMETRYCOLLECTION(POLYGON((0 0,100 0,100 100,0 0)))'
    exp_wkt = 'MULTIPOLYGON (((0 0,100 0,100 100,0 0)))'

    src_geom = ogr.CreateGeometryFromWkt( src_wkt )
    dst_geom = ogr.ForceToMultiPolygon( src_geom )

    if ogrtest.check_feature_geometry( dst_geom, exp_wkt ):
        print(dst_geom.ExportToWkt())
        return 'fail'

    src_wkt = 'CURVEPOLYGON ((0 0,100 0,100 100,0 0))'
    exp_wkt = 'MULTIPOLYGON (((0 0,100 0,100 100,0 0)))'

    src_geom = ogr.CreateGeometryFromWkt( src_wkt )
    dst_geom = ogr.ForceToMultiPolygon( src_geom )

    if ogrtest.check_feature_geometry( dst_geom, exp_wkt ):
        print(dst_geom.ExportToWkt())
        return 'fail'

    src_wkt = 'MULTISURFACE (((0 0,100 0,100 100,0 0)))'
    exp_wkt = 'MULTIPOLYGON (((0 0,100 0,100 100,0 0)))'

    src_geom = ogr.CreateGeometryFromWkt( src_wkt )
    dst_geom = ogr.ForceToMultiPolygon( src_geom )

    if ogrtest.check_feature_geometry( dst_geom, exp_wkt ):
        print(dst_geom.ExportToWkt())
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
        print(dst_geom.ExportToWkt())
        return 'fail'

    src_wkt = 'GEOMETRYCOLLECTION(POINT(2 5 3),POINT(4 5 5))'
    exp_wkt = 'MULTIPOINT(2 5 3,4 5 5)'

    src_geom = ogr.CreateGeometryFromWkt( src_wkt )
    dst_geom = ogr.ForceToMultiPoint( src_geom )

    if ogrtest.check_feature_geometry( dst_geom, exp_wkt ):
        print(dst_geom.ExportToWkt())
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
        print(dst_geom.ExportToWkt())
        return 'fail'
    
    src_wkt = 'GEOMETRYCOLLECTION(LINESTRING(2 5,10 20),LINESTRING(0 0,10 10))'
    exp_wkt = 'MULTILINESTRING((2 5,10 20),(0 0,10 10))'

    src_geom = ogr.CreateGeometryFromWkt( src_wkt )
    dst_geom = ogr.ForceToMultiLineString( src_geom )

    if ogrtest.check_feature_geometry( dst_geom, exp_wkt ):
        print(dst_geom.ExportToWkt())
        return 'fail'
    
    src_wkt = 'POLYGON((2 5,10 20),(0 0,10 10))'
    exp_wkt = 'MULTILINESTRING((2 5,10 20),(0 0,10 10))'

    src_geom = ogr.CreateGeometryFromWkt( src_wkt )
    dst_geom = ogr.ForceToMultiLineString( src_geom )

    if ogrtest.check_feature_geometry( dst_geom, exp_wkt ):
        print(dst_geom.ExportToWkt())
        return 'fail'
    
    src_wkt = 'MULTIPOLYGON(((2 5,10 20),(0 0,10 10)),((2 5,10 20)))'
    exp_wkt = 'MULTILINESTRING((2 5,10 20),(0 0,10 10),(2 5,10 20))'

    src_geom = ogr.CreateGeometryFromWkt( src_wkt )
    dst_geom = ogr.ForceToMultiLineString( src_geom )

    if ogrtest.check_feature_geometry( dst_geom, exp_wkt ):
        print(dst_geom.ExportToWkt())
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
                     'CURVEPOLYGON EMPTY',
                     'CURVEPOLYGON ((0 0,0 1,1 1,1 0,0 0))',
                     'CURVEPOLYGON (CIRCULARSTRING(0 0,1 0,0 0))',
                     'COMPOUNDCURVE EMPTY',
                     'COMPOUNDCURVE ((0 0,0 1,1 1,1 0,0 0))',
                     'COMPOUNDCURVE (CIRCULARSTRING(0 0,1 0,0 0))',
                     'CIRCULARSTRING EMPTY',
                     'CIRCULARSTRING (0 0,1 0,0 0)',
                     'MULTISURFACE EMPTY',
                     'MULTISURFACE (((0 0,0 1,1 1,1 0,0 0)))',
                     'MULTISURFACE (CURVEPOLYGON((0 0,0 1,1 1,1 0,0 0)))',
                     'MULTICURVE EMPTY',
                     'MULTICURVE ((0 0,0 1))',
                     'MULTICURVE (COMPOUNDCURVE((0 0,0 1)))',
                     'MULTICURVE (CIRCULARSTRING (0 0,1 0,0 0))',
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
        dst_geom5 = ogr.ForceToLineString( src_geom )
        for target_type in range(ogr.wkbMultiSurface):
            gdal.PushErrorHandler('CPLQuietErrorHandler')
            dst_geom6 = ogr.ForceTo( src_geom, 1 +target_type )
            gdal.PopErrorHandler()
        #print(src_geom.ExportToWkt(), dst_geom1.ExportToWkt(), dst_geom2.ExportToWkt(), dst_geom3.ExportToWkt(), dst_geom4.ExportToWkt())

    return 'success'

###############################################################################
# Test forceToLineString()

def ogr_factory_7():

    src_wkt = 'LINESTRING(2 5,10 20)'
    exp_wkt = 'LINESTRING(2 5,10 20)'

    src_geom = ogr.CreateGeometryFromWkt( src_wkt )
    dst_geom = ogr.ForceToLineString( src_geom )

    if ogrtest.check_feature_geometry( dst_geom, exp_wkt ):
        print(dst_geom.ExportToWkt())
        return 'fail'

    src_wkt = 'MULTILINESTRING((2 5,10 20))'
    exp_wkt = 'LINESTRING(2 5,10 20)'

    src_geom = ogr.CreateGeometryFromWkt( src_wkt )
    dst_geom = ogr.ForceToLineString( src_geom )

    if ogrtest.check_feature_geometry( dst_geom, exp_wkt ):
        print(dst_geom.ExportToWkt())
        return 'fail'

    src_wkt = 'MULTICURVE((2 5,10 20))'
    exp_wkt = 'LINESTRING(2 5,10 20)'

    src_geom = ogr.CreateGeometryFromWkt( src_wkt )
    dst_geom = ogr.ForceToLineString( src_geom )

    if ogrtest.check_feature_geometry( dst_geom, exp_wkt ):
        print(dst_geom.ExportToWkt())
        return 'fail'

    src_wkt = 'MULTICURVE(COMPOUNDCURVE((2 5,10 20)))'
    exp_wkt = 'LINESTRING(2 5,10 20)'

    src_geom = ogr.CreateGeometryFromWkt( src_wkt )
    dst_geom = ogr.ForceToLineString( src_geom )

    if ogrtest.check_feature_geometry( dst_geom, exp_wkt ):
        print(dst_geom.ExportToWkt())
        return 'fail'

    src_wkt = 'MULTILINESTRING((2 5,10 20),(3 4,30 40))'
    exp_wkt = 'MULTILINESTRING((2 5,10 20),(3 4,30 40))'

    src_geom = ogr.CreateGeometryFromWkt( src_wkt )
    dst_geom = ogr.ForceToLineString( src_geom )

    if ogrtest.check_feature_geometry( dst_geom, exp_wkt ):
        print(dst_geom.ExportToWkt())
        return 'fail'

    src_wkt = 'MULTILINESTRING((2 5,10 20),(10 20,30 40))'
    exp_wkt = 'LINESTRING (2 5,10 20,30 40)'

    src_geom = ogr.CreateGeometryFromWkt( src_wkt )
    dst_geom = ogr.ForceToLineString( src_geom )

    if ogrtest.check_feature_geometry( dst_geom, exp_wkt ):
        print(dst_geom.ExportToWkt())
        return 'fail'

    src_wkt = 'GEOMETRYCOLLECTION(LINESTRING(2 5,10 20),LINESTRING(10 20,30 40))'
    exp_wkt = 'LINESTRING (2 5,10 20,30 40)'

    src_geom = ogr.CreateGeometryFromWkt( src_wkt )
    dst_geom = ogr.ForceToLineString( src_geom )

    if ogrtest.check_feature_geometry( dst_geom, exp_wkt ):
        print(dst_geom.ExportToWkt())
        return 'fail'

    src_wkt = 'MULTILINESTRING((2 5,10 20),(10 20))'
    exp_wkt = 'MULTILINESTRING((2 5,10 20),(10 20))'

    src_geom = ogr.CreateGeometryFromWkt( src_wkt )
    dst_geom = ogr.ForceToLineString( src_geom )

    if ogrtest.check_feature_geometry( dst_geom, exp_wkt ):
        print(dst_geom.ExportToWkt())
        return 'fail'

    src_wkt = 'MULTILINESTRING((2 5,10 20),(10 20,30 40),(30 40,50 60))'
    exp_wkt = 'LINESTRING (2 5,10 20,30 40,50 60)'

    src_geom = ogr.CreateGeometryFromWkt( src_wkt )
    dst_geom = ogr.ForceToLineString( src_geom )

    if ogrtest.check_feature_geometry( dst_geom, exp_wkt ):
        print(dst_geom.ExportToWkt())
        return 'fail'

    src_wkt = 'POLYGON ((0 0,0 1,1 1,1 0,0 0))'
    exp_wkt = 'LINESTRING (0 0,0 1,1 1,1 0,0 0)'

    src_geom = ogr.CreateGeometryFromWkt( src_wkt )
    dst_geom = ogr.ForceToLineString( src_geom )

    if ogrtest.check_feature_geometry( dst_geom, exp_wkt ):
        print(dst_geom.ExportToWkt())
        return 'fail'

    src_wkt = 'CURVEPOLYGON ((0 0,0 1,1 1,1 0,0 0))'
    exp_wkt = 'LINESTRING (0 0,0 1,1 1,1 0,0 0)'

    src_geom = ogr.CreateGeometryFromWkt( src_wkt )
    dst_geom = ogr.ForceToLineString( src_geom )

    if ogrtest.check_feature_geometry( dst_geom, exp_wkt ):
        print(dst_geom.ExportToWkt())
        return 'fail'

    src_wkt = 'CURVEPOLYGON (COMPOUNDCURVE((0 0,0 1,1 1,1 0,0 0)))'
    exp_wkt = 'LINESTRING (0 0,0 1,1 1,1 0,0 0)'

    src_geom = ogr.CreateGeometryFromWkt( src_wkt )
    dst_geom = ogr.ForceToLineString( src_geom )

    if ogrtest.check_feature_geometry( dst_geom, exp_wkt ):
        print(dst_geom.ExportToWkt())
        return 'fail'

    return 'success'

###############################################################################
# Test forceTo()

def ogr_factory_8():
    
    tests = [ ('POINT(2 5)', 'MULTIPOINT (2 5)', ogr.wkbMultiPoint ),

              ('LINESTRING(2 5,10 20)', 'LINESTRING(2 5,10 20)', ogr.wkbLineString ),
              ('LINESTRING(2 5,10 20)', 'COMPOUNDCURVE ((2 5,10 20))', ogr.wkbCompoundCurve ),
              ('LINESTRING(2 5,10 20)', 'MULTILINESTRING ((2 5,10 20))', ogr.wkbMultiLineString ),
              ('LINESTRING(2 5,10 20)', 'MULTICURVE ((2 5,10 20))', ogr.wkbMultiCurve ),
              ('LINESTRING(2 5,10 20)', None, ogr.wkbPolygon ),
              ('LINESTRING(2 5,10 20)', None, ogr.wkbCurvePolygon ),
              ('LINESTRING(2 5,10 20)', None, ogr.wkbMultiSurface ),
              ('LINESTRING(2 5,10 20)', None, ogr.wkbMultiPolygon ),

              ('LINESTRING(0 0,0 1,1 1,0 0)', 'POLYGON ((0 0,0 1,1 1,0 0))', ogr.wkbPolygon ),
              ('LINESTRING(0 0,0 1,1 1,0 0)', 'CURVEPOLYGON ((0 0,0 1,1 1,0 0))', ogr.wkbCurvePolygon ),
              ('LINESTRING(0 0,0 1,1 1,0 0)', 'MULTIPOLYGON (((0 0,0 1,1 1,0 0)))', ogr.wkbMultiPolygon ),
              ('LINESTRING(0 0,0 1,1 1,0 0)', 'MULTISURFACE (((0 0,0 1,1 1,0 0)))', ogr.wkbMultiSurface ),

              ('LINESTRING EMPTY', 'COMPOUNDCURVE EMPTY', ogr.wkbCompoundCurve ),
              ('LINESTRING EMPTY', 'MULTILINESTRING EMPTY', ogr.wkbMultiLineString ),
              ('LINESTRING EMPTY', 'MULTICURVE EMPTY', ogr.wkbMultiCurve ),

              ('MULTILINESTRING ((2 5,10 20))', 'LINESTRING(2 5,10 20)', ogr.wkbLineString ),
              ('MULTILINESTRING ((2 5,10 20))', 'COMPOUNDCURVE ((2 5,10 20))', ogr.wkbCompoundCurve ),
              ('MULTILINESTRING ((2 5,10 20))', 'MULTICURVE ((2 5,10 20))', ogr.wkbMultiCurve ),
              ('MULTILINESTRING ((2 5,10 20))', None, ogr.wkbPolygon ),
              ('MULTILINESTRING ((2 5,10 20))', None, ogr.wkbCurvePolygon ),
              ('MULTILINESTRING ((2 5,10 20))', None, ogr.wkbMultiPolygon ),
              ('MULTILINESTRING ((2 5,10 20))', None, ogr.wkbMultiSurface ),

              ('MULTILINESTRING ((0 0,0 1,1 1,0 0))', 'POLYGON ((0 0,0 1,1 1,0 0))', ogr.wkbPolygon ),
              ('MULTILINESTRING ((0 0,0 1,1 1,0 0))', 'CURVEPOLYGON ((0 0,0 1,1 1,0 0))', ogr.wkbCurvePolygon ),
              ('MULTILINESTRING ((0 0,0 1,1 1,0 0))', 'MULTIPOLYGON (((0 0,0 1,1 1,0 0)))', ogr.wkbMultiPolygon ),
              ('MULTILINESTRING ((0 0,0 1,1 1,0 0))', 'MULTISURFACE (((0 0,0 1,1 1,0 0)))', ogr.wkbMultiSurface ),

              ('MULTILINESTRING EMPTY', 'LINESTRING EMPTY', ogr.wkbLineString ),
              ('MULTILINESTRING EMPTY', 'COMPOUNDCURVE EMPTY', ogr.wkbCompoundCurve ),
              ('MULTILINESTRING EMPTY', 'MULTICURVE EMPTY', ogr.wkbMultiCurve ),

              ('CIRCULARSTRING(0 0,1 0,0 0)', 'COMPOUNDCURVE (CIRCULARSTRING (0 0,1 0,0 0))', ogr.wkbCompoundCurve),
              ('CIRCULARSTRING(0 0,1 0,0 0)', 'MULTICURVE (CIRCULARSTRING (0 0,1 0,0 0))', ogr.wkbMultiCurve),
              ('CIRCULARSTRING(0 0,1 0,0 0)', 'CURVEPOLYGON (CIRCULARSTRING (0 0,1 0,0 0))', ogr.wkbCurvePolygon),
              ('CIRCULARSTRING(0 0,1 0,0 0)', 'POLYGON ((0 0,0.116977778440514 -0.321393804843282,0.413175911166547 -0.49240387650611,0.75 -0.433012701892224,0.969846310392967 -0.171010071662835,0.969846310392967 0.171010071662835,0.75 0.433012701892224,0.413175911166547 0.49240387650611,0.116977778440514 0.321393804843282,0 0))', ogr.wkbPolygon),
              ('CIRCULARSTRING(0 0,1 0,0 0)', 'MULTIPOLYGON (((0 0,0.116977778440514 -0.321393804843282,0.413175911166547 -0.49240387650611,0.75 -0.433012701892224,0.969846310392967 -0.171010071662835,0.969846310392967 0.171010071662835,0.75 0.433012701892224,0.413175911166547 0.49240387650611,0.116977778440514 0.321393804843282,0 0)))', ogr.wkbMultiPolygon),
              ('CIRCULARSTRING(0 0,1 0,0 0)', 'MULTISURFACE (CURVEPOLYGON (CIRCULARSTRING (0 0,1 0,0 0)))', ogr.wkbMultiSurface),
              ('CIRCULARSTRING(0 0,1 0,0 0)', 'LINESTRING (0 0,0.116977778440514 -0.321393804843282,0.413175911166547 -0.49240387650611,0.75 -0.433012701892224,0.969846310392967 -0.171010071662835,0.969846310392967 0.171010071662835,0.75 0.433012701892224,0.413175911166547 0.49240387650611,0.116977778440514 0.321393804843282,0 0)', ogr.wkbLineString),

              ('CIRCULARSTRING(0 0,1 1,2 2)', 'LINESTRING (0 0,1 1,2 2)', ogr.wkbLineString),
              ('CIRCULARSTRING(0 0,1 1,2 2)', 'MULTILINESTRING ((0 0,1 1,2 2))', ogr.wkbMultiLineString),
              ('CIRCULARSTRING(0 0,1 1,2 2)', None, ogr.wkbPolygon),
              ('CIRCULARSTRING(0 0,1 1,2 2)', None, ogr.wkbCurvePolygon),
              ('CIRCULARSTRING(0 0,1 1,2 2)', None, ogr.wkbMultiSurface),
              ('CIRCULARSTRING(0 0,1 1,2 2)', None, ogr.wkbMultiPolygon),

              ('COMPOUNDCURVE ((2 5,10 20))', 'LINESTRING(2 5,10 20)', ogr.wkbLineString ),
              ('COMPOUNDCURVE (CIRCULARSTRING(0 0,1 1,2 2))', 'LINESTRING (0 0,1 1,2 2)', ogr.wkbLineString ),
              ('COMPOUNDCURVE ((2 5,10 20),(10 20,30 40))', 'LINESTRING(2 5,10 20,30 40)', ogr.wkbLineString ),
              ('COMPOUNDCURVE ((2 5,10 20),(10 20,30 40))', 'MULTILINESTRING((2 5,10 20,30 40))', ogr.wkbMultiLineString ),
              ('COMPOUNDCURVE ((2 5,10 20),(10 20,30 40))', 'MULTICURVE (COMPOUNDCURVE ((2 5,10 20),(10 20,30 40)))', ogr.wkbMultiCurve ),

              ('COMPOUNDCURVE (CIRCULARSTRING(0 0,1 0,0 0))', 'CURVEPOLYGON (COMPOUNDCURVE (CIRCULARSTRING (0 0,1 0,0 0)))', ogr.wkbCurvePolygon),
              ('COMPOUNDCURVE (CIRCULARSTRING(0 0,1 0,0 0))', 'POLYGON ((0 0,0.116977778440514 -0.321393804843282,0.413175911166547 -0.49240387650611,0.75 -0.433012701892224,0.969846310392967 -0.171010071662835,0.969846310392967 0.171010071662835,0.75 0.433012701892224,0.413175911166547 0.49240387650611,0.116977778440514 0.321393804843282,0 0))', ogr.wkbPolygon),
              ('COMPOUNDCURVE (CIRCULARSTRING(0 0,1 0,0 0))', 'MULTISURFACE (CURVEPOLYGON (COMPOUNDCURVE (CIRCULARSTRING (0 0,1 0,0 0))))', ogr.wkbMultiSurface),
              ('COMPOUNDCURVE (CIRCULARSTRING(0 0,1 0,0 0))', 'MULTIPOLYGON (((0 0,0.116977778440514 -0.321393804843282,0.413175911166547 -0.49240387650611,0.75 -0.433012701892224,0.969846310392967 -0.171010071662835,0.969846310392967 0.171010071662835,0.75 0.433012701892224,0.413175911166547 0.49240387650611,0.116977778440514 0.321393804843282,0 0)))', ogr.wkbMultiPolygon),
              ('COMPOUNDCURVE (CIRCULARSTRING(0 0,1 0,0 0))', 'LINESTRING (0 0,0.116977778440514 -0.321393804843282,0.413175911166547 -0.49240387650611,0.75 -0.433012701892224,0.969846310392967 -0.171010071662835,0.969846310392967 0.171010071662835,0.75 0.433012701892224,0.413175911166547 0.49240387650611,0.116977778440514 0.321393804843282,0 0)', ogr.wkbLineString),

              ('COMPOUNDCURVE((0 0,0 1,1 1,0 0))', 'POLYGON ((0 0,0 1,1 1,0 0))', ogr.wkbPolygon ),
              ('COMPOUNDCURVE((0 0,0 1,1 1,0 0))', 'MULTIPOLYGON (((0 0,0 1,1 1,0 0)))', ogr.wkbMultiPolygon ),
              ('COMPOUNDCURVE((0 0,0 1,1 1,0 0))', 'MULTISURFACE (CURVEPOLYGON (COMPOUNDCURVE ((0 0,0 1,1 1,0 0))))', ogr.wkbMultiSurface ),
              ('COMPOUNDCURVE((0 0,0 1,1 1,0 0))', 'CURVEPOLYGON (COMPOUNDCURVE((0 0,0 1,1 1,0 0)))', ogr.wkbCurvePolygon ),

              ('POLYGON ((0 0,0 1,1 1,0 0))', 'MULTIPOLYGON (((0 0,0 1,1 1,0 0)))', ogr.wkbMultiPolygon ),
              ('POLYGON ((0 0,0 1,1 1,0 0))', 'MULTISURFACE (((0 0,0 1,1 1,0 0)))', ogr.wkbMultiSurface ),
              ('POLYGON ((0 0,0 1,1 1,0 0))', 'CURVEPOLYGON ((0 0,0 1,1 1,0 0))', ogr.wkbCurvePolygon ),
              ('POLYGON ((0 0,0 1,1 1,0 0),(0.25 0.25,0.25 0.75,0.75 0.75,0.25 0.25))', 'CURVEPOLYGON ((0 0,0 1,1 1,0 0),(0.25 0.25,0.25 0.75,0.75 0.75,0.25 0.25))', ogr.wkbCurvePolygon ),
              ('POLYGON ((0 0,0 1,1 1,0 0))', 'LINESTRING (0 0,0 1,1 1,0 0)', ogr.wkbLineString ),
              ('POLYGON ((0 0,0 1,1 1,0 0))', 'COMPOUNDCURVE ((0 0,0 1,1 1,0 0))', ogr.wkbCompoundCurve ),

              ('CURVEPOLYGON ((0 0,0 1,1 1,0 0))', 'POLYGON ((0 0,0 1,1 1,0 0))', ogr.wkbPolygon ),
              ('CURVEPOLYGON ((0 0,0 1,1 1,0 0))', 'MULTIPOLYGON (((0 0,0 1,1 1,0 0)))', ogr.wkbMultiPolygon ),
              ('CURVEPOLYGON ((0 0,0 1,1 1,0 0))', 'MULTISURFACE (CURVEPOLYGON((0 0,0 1,1 1,0 0)))', ogr.wkbMultiSurface ),
              ('CURVEPOLYGON ((0 0,0 1,1 1,0 0))', 'CURVEPOLYGON ((0 0,0 1,1 1,0 0))', ogr.wkbCurvePolygon ),
              ('CURVEPOLYGON ((0 0,0 1,1 1,0 0))', 'LINESTRING (0 0,0 1,1 1,0 0)', ogr.wkbLineString ),
              ('CURVEPOLYGON ((0 0,0 1,1 1,0 0))', 'COMPOUNDCURVE ((0 0,0 1,1 1,0 0))', ogr.wkbCompoundCurve ),
              ('CURVEPOLYGON ((0 0,0 1,1 1,0 0))', 'MULTILINESTRING ((0 0,0 1,1 1,0 0))', ogr.wkbMultiLineString ),
              ('CURVEPOLYGON ((0 0,0 1,1 1,0 0))', 'MULTICURVE ((0 0,0 1,1 1,0 0))', ogr.wkbMultiCurve ),
              ('CURVEPOLYGON (COMPOUNDCURVE((0 0,0 1,1 1,0 0)))', 'POLYGON ((0 0,0 1,1 1,0 0))', ogr.wkbPolygon ),
              ('CURVEPOLYGON (COMPOUNDCURVE((0 0,0 1,1 1,0 0)))', 'MULTIPOLYGON (((0 0,0 1,1 1,0 0)))', ogr.wkbMultiPolygon ),
              ('CURVEPOLYGON (COMPOUNDCURVE((0 0,0 1,1 1,0 0)))', 'MULTISURFACE (CURVEPOLYGON (COMPOUNDCURVE ((0 0,0 1,1 1,0 0))))', ogr.wkbMultiSurface ),
              ('CURVEPOLYGON (COMPOUNDCURVE((0 0,0 1,1 1,0 0)))', 'CURVEPOLYGON (COMPOUNDCURVE((0 0,0 1,1 1,0 0)))', ogr.wkbCurvePolygon ),
              ('CURVEPOLYGON (COMPOUNDCURVE((0 0,0 1,1 1,0 0)))', 'LINESTRING (0 0,0 1,1 1,0 0)', ogr.wkbLineString ),
              ('CURVEPOLYGON (COMPOUNDCURVE((0 0,0 1,1 1,0 0)))', 'COMPOUNDCURVE ((0 0,0 1,1 1,0 0))', ogr.wkbCompoundCurve ),
              ('CURVEPOLYGON (COMPOUNDCURVE((0 0,0 1),(0 1,1 1,0 0)))', 'POLYGON ((0 0,0 1,1 1,0 0))', ogr.wkbPolygon ),

              ('CURVEPOLYGON (CIRCULARSTRING(0 0,1 0,0 0))', 'POLYGON ((0 0,0.116977778440514 -0.321393804843282,0.413175911166547 -0.49240387650611,0.75 -0.433012701892224,0.969846310392967 -0.171010071662835,0.969846310392967 0.171010071662835,0.75 0.433012701892224,0.413175911166547 0.49240387650611,0.116977778440514 0.321393804843282,0 0))', ogr.wkbPolygon),
              ('CURVEPOLYGON (CIRCULARSTRING(0 0,1 0,0 0))', 'MULTISURFACE (CURVEPOLYGON ( CIRCULARSTRING (0 0,1 0,0 0)))', ogr.wkbMultiSurface),
              ('CURVEPOLYGON (CIRCULARSTRING(0 0,1 0,0 0))', 'MULTIPOLYGON (((0 0,0.116977778440514 -0.321393804843282,0.413175911166547 -0.49240387650611,0.75 -0.433012701892224,0.969846310392967 -0.171010071662835,0.969846310392967 0.171010071662835,0.75 0.433012701892224,0.413175911166547 0.49240387650611,0.116977778440514 0.321393804843282,0 0)))', ogr.wkbMultiPolygon),
              ('CURVEPOLYGON (CIRCULARSTRING(0 0,1 0,0 0))', 'COMPOUNDCURVE (CIRCULARSTRING (0 0,1 0,0 0))', ogr.wkbCompoundCurve),
              ('CURVEPOLYGON (CIRCULARSTRING(0 0,1 0,0 0))', 'MULTICURVE (CIRCULARSTRING (0 0,1 0,0 0))', ogr.wkbMultiCurve),
              ('CURVEPOLYGON (CIRCULARSTRING(0 0,1 0,0 0))', 'MULTILINESTRING ((0 0,0.116977778440514 -0.321393804843282,0.413175911166547 -0.49240387650611,0.75 -0.433012701892224,0.969846310392967 -0.171010071662835,0.969846310392967 0.171010071662835,0.75 0.433012701892224,0.413175911166547 0.49240387650611,0.116977778440514 0.321393804843282,0 0))', ogr.wkbMultiLineString),

              ('MULTICURVE ((2 5,10 20))', 'LINESTRING(2 5,10 20)', ogr.wkbLineString ),
              ('MULTICURVE ((2 5,10 20))', 'COMPOUNDCURVE ((2 5,10 20))', ogr.wkbCompoundCurve ),
              ('MULTICURVE ((2 5,10 20))', 'MULTILINESTRING ((2 5,10 20))', ogr.wkbMultiLineString ),
              ('MULTICURVE (COMPOUNDCURVE((2 5,10 20)))', 'LINESTRING(2 5,10 20)', ogr.wkbLineString ),
              ('MULTICURVE (COMPOUNDCURVE((2 5,10 20)))', 'COMPOUNDCURVE ((2 5,10 20))', ogr.wkbCompoundCurve ),
              ('MULTICURVE (COMPOUNDCURVE((2 5,10 20)))', 'MULTILINESTRING ((2 5,10 20))', ogr.wkbMultiLineString ),

              ('MULTICURVE ((0 0,0 1,1 1,0 0))', 'POLYGON ((0 0,0 1,1 1,0 0))', ogr.wkbPolygon ),
              ('MULTICURVE ((0 0,0 1,1 1,0 0))', 'CURVEPOLYGON ((0 0,0 1,1 1,0 0))', ogr.wkbCurvePolygon ),
              ('MULTICURVE ((0 0,0 1,1 1,0 0))', 'MULTIPOLYGON (((0 0,0 1,1 1,0 0)))', ogr.wkbMultiPolygon ),
              ('MULTICURVE ((0 0,0 1,1 1,0 0))', 'MULTISURFACE (((0 0,0 1,1 1,0 0)))', ogr.wkbMultiSurface ),
              ('MULTICURVE (COMPOUNDCURVE((0 0,0 1,1 1,0 0)))', 'MULTIPOLYGON (((0 0,0 1,1 1,0 0)))', ogr.wkbMultiPolygon ),
              ('MULTICURVE (COMPOUNDCURVE((0 0,0 1,1 1,0 0)))', 'MULTISURFACE (CURVEPOLYGON (COMPOUNDCURVE ((0 0,0 1,1 1,0 0))))', ogr.wkbMultiSurface ),

              ('MULTIPOLYGON (((0 0,0 1,1 1,0 0)))', 'MULTIPOLYGON (((0 0,0 1,1 1,0 0)))', ogr.wkbUnknown ),

              ('MULTIPOLYGON (((0 0,0 1,1 1,0 0)))', 'MULTISURFACE (((0 0,0 1,1 1,0 0)))', ogr.wkbMultiSurface ),
              ('MULTIPOLYGON (((0 0,0 1,1 1,0 0)))', 'CURVEPOLYGON ((0 0,0 1,1 1,0 0))', ogr.wkbCurvePolygon ),
              ('MULTIPOLYGON (((0 0,0 1,1 1,0 0),(0.25 0.25,0.25 0.75,0.75 0.75,0.25 0.25)))', 'CURVEPOLYGON ((0 0,0 1,1 1,0 0),(0.25 0.25,0.25 0.75,0.75 0.75,0.25 0.25))', ogr.wkbCurvePolygon ),
              ('MULTIPOLYGON (((0 0,0 1,1 1,0 0)))', 'LINESTRING (0 0,0 1,1 1,0 0)', ogr.wkbLineString ),
              ('MULTIPOLYGON (((0 0,0 1,1 1,0 0)))', 'COMPOUNDCURVE ((0 0,0 1,1 1,0 0))', ogr.wkbCompoundCurve ),
              ('MULTIPOLYGON (((0 0,0 1,1 1,0 0)))', 'MULTILINESTRING ((0 0,0 1,1 1,0 0))', ogr.wkbMultiLineString ),
              ('MULTIPOLYGON (((0 0,0 1,1 1,0 0)))', 'MULTICURVE ((0 0,0 1,1 1,0 0))', ogr.wkbMultiCurve ),

              ('MULTISURFACE (((0 0,0 1,1 1,0 0)))', 'MULTIPOLYGON (((0 0,0 1,1 1,0 0)))', ogr.wkbMultiPolygon ),
              ('MULTISURFACE (((0 0,0 1,1 1,0 0)))', 'CURVEPOLYGON ((0 0,0 1,1 1,0 0))', ogr.wkbCurvePolygon ),
              ('MULTISURFACE (((0 0,0 1,1 1,0 0),(0.25 0.25,0.25 0.75,0.75 0.75,0.25 0.25)))', 'CURVEPOLYGON ((0 0,0 1,1 1,0 0),(0.25 0.25,0.25 0.75,0.75 0.75,0.25 0.25))', ogr.wkbCurvePolygon ),
              ('MULTISURFACE (((0 0,0 1,1 1,0 0)))', 'LINESTRING (0 0,0 1,1 1,0 0)', ogr.wkbLineString ),
              ('MULTISURFACE (((0 0,0 1,1 1,0 0)))', 'COMPOUNDCURVE ((0 0,0 1,1 1,0 0))', ogr.wkbCompoundCurve ),
              ('MULTISURFACE (((0 0,0 1,1 1,0 0)))', 'MULTILINESTRING ((0 0,0 1,1 1,0 0))', ogr.wkbMultiLineString ),
              ('MULTISURFACE (((0 0,0 1,1 1,0 0)))', 'MULTICURVE ((0 0,0 1,1 1,0 0))', ogr.wkbMultiCurve ),
              ('MULTISURFACE (CURVEPOLYGON((0 0,0 1,1 1,0 0)))', 'MULTIPOLYGON (((0 0,0 1,1 1,0 0)))', ogr.wkbMultiPolygon ),
              ('MULTISURFACE (CURVEPOLYGON((0 0,0 1,1 1,0 0)))', 'CURVEPOLYGON ((0 0,0 1,1 1,0 0))', ogr.wkbCurvePolygon ),
              ('MULTISURFACE (CURVEPOLYGON((0 0,0 1,1 1,0 0),(0.25 0.25,0.25 0.75,0.75 0.75,0.25 0.25)))', 'CURVEPOLYGON ((0 0,0 1,1 1,0 0),(0.25 0.25,0.25 0.75,0.75 0.75,0.25 0.25))', ogr.wkbCurvePolygon ),
              ('MULTISURFACE (CURVEPOLYGON((0 0,0 1,1 1,0 0)))', 'LINESTRING (0 0,0 1,1 1,0 0)', ogr.wkbLineString ),
              ('MULTISURFACE (CURVEPOLYGON((0 0,0 1,1 1,0 0)))', 'COMPOUNDCURVE ((0 0,0 1,1 1,0 0))', ogr.wkbCompoundCurve ),
              ('MULTISURFACE (CURVEPOLYGON(CIRCULARSTRING(0 0,1 0,0 0)))', 'COMPOUNDCURVE (CIRCULARSTRING (0 0,1 0,0 0))', ogr.wkbCompoundCurve ),
              ('MULTISURFACE (CURVEPOLYGON((0 0,0 1,1 1,0 0)))', 'MULTILINESTRING ((0 0,0 1,1 1,0 0))', ogr.wkbMultiLineString ),
              ('MULTISURFACE (CURVEPOLYGON((0 0,0 1,1 1,0 0)))', 'MULTICURVE ((0 0,0 1,1 1,0 0))', ogr.wkbMultiCurve ),
              ('MULTISURFACE (CURVEPOLYGON(CIRCULARSTRING(0 0,1 0,0 0)))', 'MULTICURVE (CIRCULARSTRING (0 0,1 0,0 0))', ogr.wkbMultiCurve ),

              ('MULTIPOINT (2 5)', 'POINT(2 5)', ogr.wkbPoint ),
            ]
    for (src_wkt, exp_wkt, target_type) in tests:

        src_geom = ogr.CreateGeometryFromWkt( src_wkt )
        gdal.SetConfigOption('OGR_ARC_STEPSIZE', '45')
        dst_geom = ogr.ForceTo( src_geom, target_type )
        gdal.SetConfigOption('OGR_ARC_STEPSIZE', None)

        if exp_wkt is None:
            exp_wkt = src_wkt
        elif target_type != ogr.wkbUnknown and dst_geom.GetGeometryType() != target_type:
            gdaltest.post_reason('fail')
            print(src_wkt)
            print(target_type)
            print(dst_geom.ExportToWkt())
            return 'fail'

        if ogrtest.check_feature_geometry( dst_geom, exp_wkt ):
            gdaltest.post_reason('fail')
            print(src_wkt)
            print(target_type)
            print(dst_geom.ExportToWkt())
            return 'fail'

    return 'success'

gdaltest_list = [ 
    ogr_factory_1,
    ogr_factory_2,
    ogr_factory_3,
    ogr_factory_4,
    ogr_factory_5,
    ogr_factory_6,
    ogr_factory_7,
    ogr_factory_8,
    ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_factory' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

