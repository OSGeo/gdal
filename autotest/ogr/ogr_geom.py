#!/usr/bin/env python
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
# Copyright (c) 2008-2014, Even Rouault <even dot rouault at mines-paris dot org>
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
import math
import random

sys.path.append( '../pymod' )

import gdaltest
import ogrtest
from osgeo import ogr
from osgeo import osr
from osgeo import gdal

###############################################################################
# Test Area calculation for a MultiPolygon (which excersises lower level
# get_Area() methods as well).

def ogr_geom_area():

    geom_wkt = 'MULTIPOLYGON( ((0 0,1 1,1 0,0 0)),((0 0,10 0, 10 10, 0 10),(1 1,1 2,2 2,2 1)) )'
    geom = ogr.CreateGeometryFromWkt( geom_wkt )

    area = geom.GetArea()
    if abs(area-99.5) > 0.00000000001:
        gdaltest.post_reason( 'GetArea() result wrong, got %g.' % area )
        return 'fail'

    # OGR >= 1.8.0
    area = geom.Area()
    if abs(area-99.5) > 0.00000000001:
        gdaltest.post_reason( 'Area() result wrong, got %g.' % area )
        return 'fail'
        
    geom.Destroy()
    
    return 'success'

###############################################################################
# Test Area calculation for a LinearRing (which excersises special case of
# getGeometryType value).

def ogr_geom_area_linearring():

    geom = ogr.Geometry( type = ogr.wkbLinearRing )
    geom.AddPoint_2D( 0, 0)
    geom.AddPoint_2D( 10, 0)
    geom.AddPoint_2D( 10, 10)
    geom.AddPoint_2D( 0, 10)
    geom.AddPoint_2D( 0, 0)

    area = geom.GetArea()
    if abs(area - 100.0) > 0.00000000001:
        gdaltest.post_reason( 'Area result wrong, got %g.' % area )
        return 'fail'

    geom.Destroy()
    
    return 'success'

###############################################################################
# Test Area calculation for a GeometryCollection

def ogr_geom_area_geometrycollection():

    # OGR >= 1.8.0
    geom_wkt = 'GEOMETRYCOLLECTION( POLYGON((0 0,1 1,1 0,0 0)), MULTIPOLYGON(((0 0,1 1,1 0,0 0))), LINESTRING(0 0,1 1), POINT(0 0), GEOMETRYCOLLECTION EMPTY )'
    geom = ogr.CreateGeometryFromWkt( geom_wkt )

    area = geom.Area()
    if abs(area-1) > 0.00000000001:
        gdaltest.post_reason( 'Area() result wrong, got %g.' % area )
        return 'fail'

    geom.Destroy()

    return 'success'
    
###############################################################################
# Test Area calculation for a LinearRing whose coordinates are shifted by a huge value
# With algorithm prior to #3556, this would return 0.

def ogr_geom_area_linearring_big_offset():

    geom = ogr.Geometry( type = ogr.wkbLinearRing )
    BIGOFFSET = 100000000000.;
    geom.AddPoint_2D( BIGOFFSET + 0, BIGOFFSET + 0)
    geom.AddPoint_2D( BIGOFFSET + 10, BIGOFFSET + 0)
    geom.AddPoint_2D( BIGOFFSET + 10, BIGOFFSET + 10)
    geom.AddPoint_2D( BIGOFFSET + 0, BIGOFFSET + 10)
    geom.AddPoint_2D( BIGOFFSET + 0, BIGOFFSET + 0)

    area = geom.GetArea()
    if abs(area - 100.0) > 0.00000000001:
        gdaltest.post_reason( 'Area result wrong, got %g.' % area )
        return 'fail'

    geom.Destroy()
    
    return 'success'


def ogr_geom_is_empty():

    geom_wkt = 'LINESTRING EMPTY'
    geom = ogr.CreateGeometryFromWkt(geom_wkt)

    if (geom.IsEmpty() == False):
        geom.Destroy()
        gdaltest.post_reason ("IsEmpty returning false for an empty geometry")
        return 'fail'
    geom.Destroy()
    
    geom_wkt = 'POINT( 1 2 )'

    geom = ogr.CreateGeometryFromWkt(geom_wkt)
    if not geom:
        gdaltest.post_reason ("A geometry could not be created from wkt: %s"%geom_wkt)
        return 'fail'

    if (geom.IsEmpty() == True):
        gdaltest.post_reason ("IsEmpty returning true for a non-empty geometry")
        return 'fail'
    geom.Destroy()
    return 'success'

def ogr_geom_pickle():
    try:
        ogr.Geometry.IsEmpty  #IsEmpty is only in the ng bindings
    except:
        return 'skip'
    
    import pickle
    geom_wkt = 'MULTIPOLYGON( ((0 0,1 1,1 0,0 0)),((0 0,10 0, 10 10, 0 10),(1 1,1 2,2 2,2 1)) )'
    geom = ogr.CreateGeometryFromWkt(geom_wkt)
    p = pickle.dumps(geom)
    
    g = pickle.loads(p)
    
    if not geom.Equal(g):
        gdaltest.post_reason ("pickled geometries were not equal")
        return 'fail'
    geom.Destroy()
    return 'success'

###############################################################################
# Test OGRGeometry::getBoundary() result for point.

def ogr_geom_boundary_point():
    
    geom_wkt = 'POINT(1 1)'
    geom = ogr.CreateGeometryFromWkt(geom_wkt)
    tmp = ogr.CreateGeometryFromWkt(geom_wkt)

    # Detect GEOS support
    try:
        result = geom.Union(tmp)
    except:
        result = None

    tmp.Destroy()

    if result is None:
        gdaltest.have_geos = 0
        return 'skip'

    gdaltest.have_geos = 1

    bnd = geom.GetBoundary()
    if bnd.GetGeometryType() is not ogr.wkbGeometryCollection:
        gdaltest.post_reason( 'GetBoundary not reported as GEOMETRYCOLLECTION EMPTY' )
        return 'fail'

    # OGR >= 1.8.0
    bnd = geom.Boundary()
    if bnd.GetGeometryType() is not ogr.wkbGeometryCollection:
        gdaltest.post_reason( 'Boundary not reported as GEOMETRYCOLLECTION EMPTY' )
        return 'fail'

    geom.Destroy()
    
    return 'success'

###############################################################################
# Test OGRGeometry::getBoundary() result for multipoint.

def ogr_geom_boundary_multipoint():
    
    if gdaltest.have_geos == 0:
        return 'skip'

    geom_wkt = 'MULTIPOINT((0 0),(1 1))'
    geom = ogr.CreateGeometryFromWkt(geom_wkt)

    bnd = geom.GetBoundary()
    if bnd.GetGeometryType() is not ogr.wkbGeometryCollection:
        gdaltest.post_reason( 'Boundary not reported as GEOMETRYCOLLECTION EMPTY' )
        return 'fail'

    bnd.Destroy()
    geom.Destroy()
    
    return 'success'

###############################################################################
# Test OGRGeometry::getBoundary() result for linestring.

def ogr_geom_boundary_linestring():
    
    if gdaltest.have_geos == 0:
        return 'skip'

    geom_wkt = 'LINESTRING(0 0, 1 1, 2 2, 3 2, 4 2)'
    geom = ogr.CreateGeometryFromWkt(geom_wkt)

    bnd = geom.GetBoundary()
    if bnd.GetGeometryType() is not ogr.wkbMultiPoint:
        gdaltest.post_reason( 'Boundary not reported as MULTIPOINT' )
        print(bnd)
        return 'fail'

    if bnd.GetGeometryCount() != 2:
        gdaltest.post_reason( 'Boundary not reported as MULTIPOINT consisting of 2 points' )
        return 'fail'

    bnd.Destroy()
    geom.Destroy()
 
    geom_wkt = 'LINESTRING(0 0, 1 0, 1 1, 0 1, 0 0)'
    geom = ogr.CreateGeometryFromWkt(geom_wkt)

    bnd = geom.GetBoundary()
    if bnd.GetGeometryType() is not ogr.wkbMultiPoint:
        gdaltest.post_reason( 'Boundary not reported as MULTIPOINT' )
        return 'fail'

    if bnd.GetGeometryCount() != 0:
        gdaltest.post_reason( 'Boundary not reported as MULTIPOINT EMPTY' )
        return 'fail'

   
    bnd.Destroy()
    geom.Destroy()
 
    return 'success'

###############################################################################
# Test OGRGeometry::getBoundary() result for polygon.

def ogr_geom_boundary_polygon():
    
    if gdaltest.have_geos == 0:
        return 'skip'

    geom_wkt = 'POLYGON((0 0,1 1,1 0,0 0))'
    geom = ogr.CreateGeometryFromWkt(geom_wkt)

    bnd = geom.GetBoundary()
    if bnd.GetGeometryType() is not ogr.wkbLineString:
        gdaltest.post_reason( 'Boundary not reported as non-empty LINESTRING' )
        print(bnd)
        return 'fail'

    bnd.Destroy()
    geom.Destroy()
    
    return 'success'

###############################################################################
# Test OGRBuildPolygonFromEdges() on a geometry collection of line strings

def ogr_geom_build_from_edges_1():

    if gdaltest.have_geos == 0:
        return 'skip'

    link_coll = ogr.Geometry( type = ogr.wkbGeometryCollection )

    wkt_array = [
      'LINESTRING (-87.601595 30.999522,-87.599623 31.000059,-87.599219 31.00017)',
      'LINESTRING (-87.601595 30.999522,-87.604349 30.999493,-87.606935 30.99952)',
      'LINESTRING (-87.59966 31.000756,-87.599851 31.000805,-87.599992 31.000805,-87.600215 31.000761,-87.600279 31.000723,-87.600586 31.000624,-87.601256 31.000508,-87.602501 31.000447,-87.602801 31.000469,-87.603108 31.000579,-87.603331 31.000716,-87.603523 31.000909,-87.603766 31.001233,-87.603913 31.00136)',
      'LINESTRING (-87.606134 31.000182,-87.605885 31.000325,-87.605343 31.000716,-87.60466 31.001117,-87.604468 31.0012,-87.603913 31.00136)',
      'LINESTRING (-87.599219 31.00017,-87.599289 31.0003,-87.599398 31.000426,-87.599564 31.000547,-87.599609 31.000701,-87.59966 31.000756)',
      'LINESTRING (-87.606935 30.99952,-87.606713 30.999799,-87.6064 30.999981,-87.606134 31.000182)' ]

    for wkt in wkt_array:
        geom = ogr.CreateGeometryFromWkt( wkt )
        #print "geom is",geom
        link_coll.AddGeometry( geom )
        geom.Destroy()

    try:
        poly = ogr.BuildPolygonFromEdges( link_coll )
        if poly is None:
            return 'fail'
        poly.Destroy()
    except:
        return 'fail'

    return 'success'

###############################################################################
# Test OGRBuildPolygonFromEdges() on a multilinestring

def ogr_geom_build_from_edges_2():

    if gdaltest.have_geos == 0:
        return 'skip'

    link_coll = ogr.Geometry( type = ogr.wkbMultiLineString )

    wkt_array = [
      'LINESTRING (-87.601595 30.999522,-87.599623 31.000059,-87.599219 31.00017)',
      'LINESTRING (-87.601595 30.999522,-87.604349 30.999493,-87.606935 30.99952)',
      'LINESTRING (-87.59966 31.000756,-87.599851 31.000805,-87.599992 31.000805,-87.600215 31.000761,-87.600279 31.000723,-87.600586 31.000624,-87.601256 31.000508,-87.602501 31.000447,-87.602801 31.000469,-87.603108 31.000579,-87.603331 31.000716,-87.603523 31.000909,-87.603766 31.001233,-87.603913 31.00136)',
      'LINESTRING (-87.606134 31.000182,-87.605885 31.000325,-87.605343 31.000716,-87.60466 31.001117,-87.604468 31.0012,-87.603913 31.00136)',
      'LINESTRING (-87.599219 31.00017,-87.599289 31.0003,-87.599398 31.000426,-87.599564 31.000547,-87.599609 31.000701,-87.59966 31.000756)',
      'LINESTRING (-87.606935 30.99952,-87.606713 30.999799,-87.6064 30.999981,-87.606134 31.000182)' ]

    for wkt in wkt_array:
        geom = ogr.CreateGeometryFromWkt( wkt )
        #print "geom is",geom
        link_coll.AddGeometry( geom )
        geom.Destroy()

    try:
        poly = ogr.BuildPolygonFromEdges( link_coll )
        if poly is None:
            return 'fail'
        poly.Destroy()
    except:
        return 'fail'

    return 'success'

###############################################################################
# Test OGRBuildPolygonFromEdges() on invalid geometries

def ogr_geom_build_from_edges_3():

    if gdaltest.have_geos == 0:
        return 'skip'

    src_geom = ogr.CreateGeometryFromWkt('POINT (0 1)')
    try:
        gdal.PushErrorHandler('CPLQuietErrorHandler')
        poly = ogr.BuildPolygonFromEdges( src_geom )
        gdal.PopErrorHandler()
        if poly is not None:
            return 'fail'
    except:
        pass
    
    src_geom = ogr.CreateGeometryFromWkt('GEOMETRYCOLLECTION (LINESTRING(0 1,2 3),POINT(0 1),LINESTRING(0 1,-2 3),LINESTRING(-2 3,2 3))')
    try:
        gdal.PushErrorHandler('CPLQuietErrorHandler')
        poly = ogr.BuildPolygonFromEdges( src_geom )
        gdal.PopErrorHandler()
        if poly is not None:
            return 'fail'
    except:
        pass

    return 'success'

###############################################################################
# Test OGRBuildPolygonFromEdges() and identify exterior ring (#3610)

def ogr_geom_build_from_edges_4():

    if int(gdal.VersionInfo('VERSION_NUM')) < 1900:
        gdaltest.post_reason('would crash')
        return 'skip'

    if gdaltest.have_geos == 0:
        return 'skip'

    link_coll = ogr.Geometry( type = ogr.wkbGeometryCollection )

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
        geom = ogr.CreateGeometryFromWkt( wkt )
        #print "geom is",geom
        link_coll.AddGeometry( geom )
        geom.Destroy()

    try:
        poly = ogr.BuildPolygonFromEdges( link_coll )
        if poly is None:
            return 'fail'
        wkt = poly.ExportToWkt()
        if wkt != 'POLYGON ((0 0,0 10,10 10,10 0,0 0),(1 1,1 2,2 2,2 1,1 1))':
            print(wkt)
            return 'fail'
        poly.Destroy()
    except:
        return 'fail'

    return 'success'

###############################################################################
# Test GetArea() on empty linear ring (#2792)

def ogr_geom_area_empty_linearring():

    geom = ogr.Geometry( type = ogr.wkbLinearRing )

    area = geom.GetArea()
    if area != 0:
        return 'fail'

    geom.Destroy()

    return 'success'

###############################################################################
# Test TransformTo()

def ogr_geom_transform_to():

    # Somewhere in Paris suburbs...
    geom = ogr.CreateGeometryFromWkt( 'POINT(2 49)')

    # Input SRS is EPSG:4326
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4326)
    geom.AssignSpatialReference(sr)

    # Output SRS is EPSG:32631
    sr2 = osr.SpatialReference()
    sr2.ImportFromEPSG(32631)
    geom.TransformTo(sr2)

    if abs(geom.GetX() - 426857) > 1 or abs(geom.GetY() - 5427937) > 1:
        print(geom.ExportToWkt())
        return 'fail'

    return 'success'

###############################################################################
# Test Transform()

def ogr_geom_transform():

    # Somewhere in Paris suburbs...
    geom = ogr.CreateGeometryFromWkt( 'POINT(2 49)')

    # Input SRS is EPSG:4326
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4326)

    # Output SRS is EPSG:32631
    sr2 = osr.SpatialReference()
    sr2.ImportFromEPSG(32631)

    ct = osr.CoordinateTransformation(sr, sr2)

    geom.Transform(ct)

    if abs(geom.GetX() - 426857) > 1 or abs(geom.GetY() - 5427937) > 1:
        print(geom.ExportToWkt())
        return 'fail'

    return 'success'

###############################################################################
# Test CloseRings()

def ogr_geom_closerings():

    geom = ogr.CreateGeometryFromWkt( 'POLYGON((0 0,0 1,1 1,1 0))' )
    geom.CloseRings()

    if geom.ExportToWkt() != 'POLYGON ((0 0,0 1,1 1,1 0,0 0))':
        print(geom.ExportToWkt())
        return 'fail'

    geom.CloseRings()
    if geom.ExportToWkt() != 'POLYGON ((0 0,0 1,1 1,1 0,0 0))':
        print(geom.ExportToWkt())
        return 'fail'

    return 'success'

###############################################################################
# Test Segmentize()

def ogr_geom_segmentize():

    geom = ogr.CreateGeometryFromWkt( 'LINESTRING(0 0,0 10)' )
    geom.Segmentize(1.00001)

    if geom.ExportToWkt() != 'LINESTRING (0 0,0 1,0 2,0 3,0 4,0 5,0 6,0 7,0 8,0 9,0 10)':
        print(geom.ExportToWkt())
        return 'fail'

    geom = ogr.CreateGeometryFromWkt( 'LINESTRING(0 0 1,0 10 1)' )
    geom.Segmentize(1.00001)

    if geom.ExportToWkt() != 'LINESTRING (0 0 1,0 1 1,0 2 1,0 3 1,0 4 1,0 5 1,0 6 1,0 7 1,0 8 1,0 9 1,0 10 1)':
        print(geom.ExportToWkt())
        return 'fail'

    # Check segmentize symetry : do exact binary comparison
    in_wkt = 'LINESTRING (0 0,1.2 1,2 0)'
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    g1.Segmentize(0.25)
    in_wkt = 'LINESTRING (2 0,1.2 1,0 0)'
    g2 = ogr.CreateGeometryFromWkt(in_wkt)
    g2.Segmentize(0.25)
    for i in range(g1.GetPointCount()):
        if g1.GetPoint(i) != g2.GetPoint(g1.GetPointCount()-1-i):
            gdaltest.post_reason('fail')
            print(i)
            print('%.18g' % (g1.GetPoint(i)[0] - g2.GetPoint(g1.GetPointCount()-1-i)[0]))
            print('%.18g' % (g1.GetPoint(i)[1] - g2.GetPoint(g1.GetPointCount()-1-i)[1]))
            print(g1)
            print(g2)
            return 'fail'
    
    return 'success'

###############################################################################
# Test Value()

def ogr_geom_value():

    geom = ogr.CreateGeometryFromWkt( 'LINESTRING(2 3,5 3,5 0)' )

    p = geom.Value(-1e-3)
    expected_p = ogr.CreateGeometryFromWkt('POINT (2 3)')
    if ogrtest.check_feature_geometry(p, expected_p) != 0:
        gdaltest.post_reason('fail')
        print(p)
        return 'fail'

    p = geom.Value(geom.Length() / 4)
    expected_p = ogr.CreateGeometryFromWkt('POINT (3.5 3)')
    if ogrtest.check_feature_geometry(p, expected_p) != 0:
        gdaltest.post_reason('fail')
        print(p)
        return 'fail'

    p = geom.Value(geom.Length() / 2)
    expected_p = ogr.CreateGeometryFromWkt('POINT (5 3)')
    if ogrtest.check_feature_geometry(p, expected_p) != 0:
        gdaltest.post_reason('fail')
        print(p)
        return 'fail'

    p = geom.Value(3 * geom.Length() / 4)
    expected_p = ogr.CreateGeometryFromWkt('POINT (5 1.5)')
    if ogrtest.check_feature_geometry(p, expected_p) != 0:
        gdaltest.post_reason('fail')
        print(p)
        return 'fail'

    p = geom.Value(geom.Length() + 1e-3)
    expected_p = ogr.CreateGeometryFromWkt('POINT (5 0)')
    if ogrtest.check_feature_geometry(p, expected_p) != 0:
        gdaltest.post_reason('fail')
        print(p)
        return 'fail'

    return 'success'

###############################################################################
# Test FlattenTo2D(), GetDimension() and GetCoordinateDimension()

def ogr_geom_flattenTo2D():

    geom = ogr.CreateGeometryFromWkt( 'POINT (1 2 3)' )

    # Point is 0 dimension, LineString 1, ...
    if geom.GetDimension() != 0:
        print(geom.GetDimension())
        return 'fail'

    if geom.GetCoordinateDimension() != 3:
        print(geom.GetCoordinateDimension())
        return 'fail'

    geom.FlattenTo2D()
    if geom.GetCoordinateDimension() != 2:
        print(geom.GetCoordinateDimension())
        return 'fail'

    if geom.ExportToWkt() != 'POINT (1 2)':
        print(geom.ExportToWkt())
        return 'fail'

    return 'success'

###############################################################################
def ogr_geom_linestring_limits():

    geom = ogr.CreateGeometryFromWkt('LINESTRING EMPTY')
    if geom.Length() != 0:
        return 'fail'

    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    geom.GetPoint(-1)
    gdal.PopErrorHandler()
    if gdal.GetLastErrorType() == 0:
        return 'fail'

    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    geom.GetPoint(0)
    gdal.PopErrorHandler()
    if gdal.GetLastErrorType() == 0:
        return 'fail'

    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    geom.GetPoint_2D(-1)
    gdal.PopErrorHandler()
    if gdal.GetLastErrorType() == 0:
        return 'fail'

    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    geom.GetPoint_2D(0)
    gdal.PopErrorHandler()
    if gdal.GetLastErrorType() == 0:
        return 'fail'

    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    geom.SetPoint(-1, 5,6,7)
    gdal.PopErrorHandler()
    if gdal.GetLastErrorType() == 0:
        return 'fail'

    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    geom.SetPoint_2D(-1, 5,6)
    gdal.PopErrorHandler()
    if gdal.GetLastErrorType() == 0:
        return 'fail'

    if False:
        gdal.ErrorReset()
        gdal.PushErrorHandler('CPLQuietErrorHandler')
        geom.SetPoint(2147000000, 5,6,7)
        gdal.PopErrorHandler()
        if gdal.GetLastErrorType() == 0:
            return 'fail'

        gdal.ErrorReset()
        gdal.PushErrorHandler('CPLQuietErrorHandler')
        geom.SetPoint_2D(2147000000, 5,6)
        gdal.PopErrorHandler()
        if gdal.GetLastErrorType() == 0:
            return 'fail'

    geom = ogr.CreateGeometryFromWkt('LINESTRING(0 0)')
    if geom.Length() != 0:
        return 'fail'
    geom = ogr.CreateGeometryFromWkt('LINESTRING(0 0, 1 0)')
    if geom.Length() != 1:
        return 'fail'

    return 'success'

###############################################################################
def ogr_geom_coord_round():

    geom = ogr.CreateGeometryFromWkt('POINT(370441.860 5591000.590)')
    wkt = geom.ExportToWkt()
    if wkt != 'POINT (370441.86 5591000.59)':
        gdaltest.post_reason('did not get expected WKT')
        print(wkt)
        return 'fail'

    return 'success'

###############################################################################
def ogr_geom_coord_round_2():

    geom = ogr.CreateGeometryFromWkt('POINT(1.0 169.600374)')
    wkt = geom.ExportToWkt()
    if wkt != 'POINT (1.0 169.600374)':
        gdaltest.post_reason('did not get expected WKT')
        print(wkt)
        return 'fail'

    return 'success'

###############################################################################
# Test Area calculation for a Point

def ogr_geom_area_point():

    geom_wkt = 'POINT(0 0)'
    geom = ogr.CreateGeometryFromWkt( geom_wkt )

    area = geom.Area()
    if area != 0:
        gdaltest.post_reason( 'Area() result wrong, got %g.' % area )
        return 'fail'

    geom.Destroy()

    return 'success'
    
###############################################################################
# Test Length calculation for a Point

def ogr_geom_length_point():

    # OGR >= 1.8.0
    geom_wkt = 'POINT(0 0)'
    geom = ogr.CreateGeometryFromWkt( geom_wkt )

    length = geom.Length()
    if length != 0:
        gdaltest.post_reason( 'Length() result wrong, got %g.' % length )
        return 'fail'

    geom.Destroy()

    return 'success'
    
###############################################################################
# Test Length calculation for a MultiLineString

def ogr_geom_length_multilinestring():

    # OGR >= 1.8.0
    geom_wkt = 'MULTILINESTRING((0 0,0 1),(0 0,0 1))'
    geom = ogr.CreateGeometryFromWkt( geom_wkt )

    length = geom.Length()
    if abs(length-2) > 0.00000000001:
        gdaltest.post_reason( 'Length() result wrong, got %g.' % length )
        return 'fail'

    geom.Destroy()

    return 'success'

###############################################################################
# Test Length calculation for a GeometryCollection

def ogr_geom_length_geometrycollection():

    # OGR >= 1.8.0
    geom_wkt = 'GEOMETRYCOLLECTION( POLYGON((0 0,0 1,1 1,1 0,0 0)), MULTILINESTRING((0 0,0 1),(0 0,0 1)), LINESTRING(0 0,0 1), LINESTRING(0 0,0 1), POINT(0 0), GEOMETRYCOLLECTION EMPTY )'
    geom = ogr.CreateGeometryFromWkt( geom_wkt )

    length = geom.Length()
    if abs(length-4) > 0.00000000001:
        gdaltest.post_reason( 'Length() result wrong, got %g.' % length )
        return 'fail'

    geom.Destroy()

    return 'success'

###############################################################################
# Test Geometry.GetPoints() (#4016)

def ogr_geom_getpoints():

    geom = ogr.CreateGeometryFromWkt('LINESTRING(0 1,2 3)')
    points = geom.GetPoints()
    if points != [(0.0, 1.0), (2.0, 3.0)]:
        gdaltest.post_reason('did not get expected points (1)')
        print(points)
        return 'fail'
    points = geom.GetPoints(nCoordDimension = 3)
    if points != [(0.0, 1.0, 0.0), (2.0, 3.0, 0.0)]:
        gdaltest.post_reason('did not get expected points (2)')
        print(points)
        return 'fail'

    geom = ogr.CreateGeometryFromWkt('LINESTRING(0 1 2,3 4 5)')
    points = geom.GetPoints()
    if points != [(0.0, 1.0, 2.0), (3.0, 4.0, 5.0)]:
        gdaltest.post_reason('did not get expected points (3)')
        print(points)
        return 'fail'
    points = geom.GetPoints(nCoordDimension = 2)
    if points != [(0.0, 1.0), (3.0, 4.0)]:
        gdaltest.post_reason('did not get expected points (4)')
        print(points)
        return 'fail'


    geom = ogr.CreateGeometryFromWkt('POINT(0 1)')
    points = geom.GetPoints()
    if points != [(0.0, 1.0)]:
        gdaltest.post_reason('did not get expected points (5)')
        print(points)
        return 'fail'
    points = geom.GetPoints(nCoordDimension = 3)
    if points != [(0.0, 1.0, 0.0)]:
        gdaltest.post_reason('did not get expected points (6)')
        print(points)
        return 'fail'

    geom = ogr.CreateGeometryFromWkt('POINT(0 1 2)')
    points = geom.GetPoints()
    if points != [(0.0, 1.0, 2.0)]:
        gdaltest.post_reason('did not get expected points (7)')
        print(points)
        return 'fail'
    points = geom.GetPoints(nCoordDimension = 2)
    if points != [(0.0, 1.0)]:
        gdaltest.post_reason('did not get expected points (8)')
        print(points)
        return 'fail'
        
    return 'success'

###############################################################################
# Test OGRGeometry::empty()

def ogr_geom_empty():

    g1 = ogr.CreateGeometryFromWkt( 'POLYGON((0 0,1 1,1 2,1 1,0 0))' )
    g1.Empty()
    wkt = g1.ExportToWkt()

    g1.Destroy()

    if wkt != 'POLYGON EMPTY':
        return 'fail'

    return 'success'

###############################################################################
# Test parsing WKT made of 2D and 3D parts

def ogr_geom_mixed_coordinate_dimension():

    # first part is 3D, second part is 2D of same length
    wkt = 'MULTIPOLYGON (((1 2 -4,1 3 -3,2 3 -3,2 2 -3,1 2 -6)),((-1 -2,-1 -3,-2 -3,-2 -2,-1 -2,50 60)))'
    expected_wkt = 'MULTIPOLYGON (((1 2 -4,1 3 -3,2 3 -3,2 2 -3,1 2 -6)),((-1 -2 0,-1 -3 0,-2 -3 0,-2 -2 0,-1 -2 0,50 60 0)))'
    g = ogr.CreateGeometryFromWkt(wkt)
    got_wkt = g.ExportToWkt()
    if got_wkt != expected_wkt:
        gdaltest.post_reason('for %s: got %s, expected %s' % (wkt, got_wkt, expected_wkt))
        return 'fail'

    # first part is 3D, second part is 2D of same length, except the last point which is 3D
    wkt = 'MULTIPOLYGON (((1 2 -4,1 3 -3,2 3 -3,2 2 -3,1 2 -6)),((-1 -2,-1 -3,-2 -3,-2 -2,-1 -2,50 60 7)))'
    expected_wkt = 'MULTIPOLYGON (((1 2 -4,1 3 -3,2 3 -3,2 2 -3,1 2 -6)),((-1 -2 0,-1 -3 0,-2 -3 0,-2 -2 0,-1 -2 0,50 60 7)))'
    g = ogr.CreateGeometryFromWkt(wkt)
    got_wkt = g.ExportToWkt()
    if got_wkt != expected_wkt:
        gdaltest.post_reason('for %s: got %s, expected %s' % (wkt, got_wkt, expected_wkt))
        return 'fail'

    return 'success'
    
   
###############################################################################
# Test GetEnvelope3D()
 
def ogr_geom_getenvelope3d():

    g = ogr.CreateGeometryFromWkt('POINT EMPTY')
    envelope = g.GetEnvelope3D()
    expected_envelope = ( 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 )
    if envelope != expected_envelope:
        gdaltest.post_reason('did not get expected envelope (1)')
        print(envelope)
        print(expected_envelope)
        return 'fail'

    g = ogr.CreateGeometryFromWkt('POINT(1 2)')
    envelope = g.GetEnvelope3D()
    expected_envelope = ( 1.0, 1.0, 2.0, 2.0, 0.0, 0.0 )
    if envelope != expected_envelope:
        gdaltest.post_reason('did not get expected envelope (2)')
        print(envelope)
        print(expected_envelope)
        return 'fail'

    g = ogr.CreateGeometryFromWkt('POINT(1 2 3)')
    envelope = g.GetEnvelope3D()
    expected_envelope = ( 1.0, 1.0, 2.0, 2.0, 3.0, 3.0 )
    if envelope != expected_envelope:
        gdaltest.post_reason('did not get expected envelope (3)')
        print(envelope)
        print(expected_envelope)
        return 'fail'

    g = ogr.CreateGeometryFromWkt('POLYGON EMPTY')
    envelope = g.GetEnvelope3D()
    expected_envelope = ( 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 )
    if envelope != expected_envelope:
        gdaltest.post_reason('did not get expected envelope (4)')
        print(envelope)
        print(expected_envelope)
        return 'fail'

    g = ogr.CreateGeometryFromWkt('LINESTRING(1 2,3 4)')
    envelope = g.GetEnvelope3D()
    expected_envelope = ( 1, 3, 2, 4, 0, 0 )
    if envelope != expected_envelope:
        gdaltest.post_reason('did not get expected envelope (5)')
        print(envelope)
        print(expected_envelope)
        return 'fail'

    g = ogr.CreateGeometryFromWkt('MULTIPOLYGON(((1 2 3,-1 2 3,-1 -2 3,-1 2 3,1 2 3),(0.1 0.2 0.3,-0.1 0.2 0.3,-0.1 -0.2 0.3,-0.1 0.2 0.3,0.1 0.2 0.3)),((10 20 -30,-10 20 -30,-10 -20 -30,-10 20 -30,10 20 -30)))')
    envelope = g.GetEnvelope3D()
    expected_envelope = ( -10, 10, -20, 20, -30, 3.0 )
    if envelope != expected_envelope:
        gdaltest.post_reason('did not get expected envelope (6)')
        print(envelope)
        print(expected_envelope)
        return 'fail'

    g = ogr.CreateGeometryFromWkt('MULTIPOLYGON EMPTY')
    envelope = g.GetEnvelope3D()
    expected_envelope = ( 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 )
    if envelope != expected_envelope:
        gdaltest.post_reason('did not get expected envelope (7)')
        print(envelope)
        print(expected_envelope)
        return 'fail'

    return 'success'

###############################################################################
# Test importing/exporting XXX Z EMPTY

def ogr_geom_z_empty():
    
    for geom in [ 'POINT', 'LINESTRING', 'POLYGON', 'MULTIPOINT', 'MULTILINESTRING', \
                  'MULTIPOLYGON', 'GEOMETRYCOLLECTION', 'CIRCULARSTRING', 'COMPOUNDCURVE', \
                  'CURVEPOLYGON', 'MULTICURVE', 'MULTISURFACE' ]:
        in_wkt = geom + ' Z EMPTY'
        g1 = ogr.CreateGeometryFromWkt(in_wkt)
        out_wkt = g1.ExportToIsoWkt()
        if in_wkt != out_wkt:
            gdaltest.post_reason('fail')
            print(out_wkt)
            return 'fail'

        g2 = ogr.CreateGeometryFromWkb(g1.ExportToIsoWkb())
        out_wkt = g2.ExportToIsoWkt()
        if in_wkt != out_wkt:
            gdaltest.post_reason('fail')
            print(out_wkt)
            return 'fail'

    return 'success'

###############################################################################
# Test HasCurveGeometry and GetLinearGeometry

def ogr_geom_getlineargeometry():
    
    for geom in [ 'POINT', 'LINESTRING', 'POLYGON', 'MULTIPOINT', 'MULTILINESTRING',
                  'MULTIPOLYGON', 'GEOMETRYCOLLECTION',
                  ('CIRCULARSTRING', 'LINESTRING'),
                  ('COMPOUNDCURVE', 'LINESTRING'),
                  ('CURVEPOLYGON', 'POLYGON'),
                  ('MULTICURVE', 'MULTILINESTRING'),
                  ('MULTISURFACE', 'MULTIPOLYGON') ]:
        try:
            (geom_in, geom_out) = geom
        except:
            geom_in = geom_out = geom
        in_wkt = geom_in + ' EMPTY'
        g = ogr.CreateGeometryFromWkt(in_wkt)
        g2 = g.GetLinearGeometry()
        if geom_in != geom_out and not g.HasCurveGeometry():
            gdaltest.post_reason('fail')
            print(g)
            return 'fail'
        if g2.ExportToWkt() != geom_out + ' EMPTY':
            gdaltest.post_reason('fail')
            print(g)
            print(g2)
            return 'fail'

    g = ogr.CreateGeometryFromWkt('GEOMETRYCOLLECTION(POINT (0 1))')
    if g.HasCurveGeometry():
        gdaltest.post_reason('fail')
        return 'fail'
    if g.HasCurveGeometry(True):
        gdaltest.post_reason('fail')
        return 'fail'

    g = ogr.CreateGeometryFromWkt('GEOMETRYCOLLECTION(CIRCULARSTRING (0 0,0 1,0 0))')
    if not g.HasCurveGeometry():
        gdaltest.post_reason('fail')
        return 'fail'
    if not g.HasCurveGeometry(True):
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Test GetDimension()

def ogr_geom_getdimension():
    for (geom, dim) in [ ('POINT EMPTY', 0),
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
                  ('MULTISURFACE EMPTY', 2) ]:
        g = ogr.CreateGeometryFromWkt(geom)
        if g.GetDimension() != dim:
            gdaltest.post_reason('fail')
            print(g)
            print(dim)
            return 'fail'

    g = ogr.CreateGeometryFromWkt('GEOMETRYCOLLECTION(LINESTRING EMPTY, POINT (0 1))')
    if g.GetDimension() != 1:
        gdaltest.post_reason('fail')
        return 'fail'

    g = ogr.CreateGeometryFromWkt('GEOMETRYCOLLECTION(POINT (0 1), LINESTRING EMPTY, POLYGON EMPTY)')
    if g.GetDimension() != 2:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Test OGRCircularString

def ogr_geom_circularstring():

    in_wkt = 'CIRCULARSTRING (0 0,1 1,1 -1)'

    # Test export/import Wkt
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    if g1.GetGeometryType() != ogr.wkbCircularString:
        gdaltest.post_reason('fail')
        return 'fail'
    out_wkt = g1.ExportToWkt()
    if in_wkt != out_wkt:
        gdaltest.post_reason('fail')
        print(out_wkt)
        return 'fail'

    if ogrtest.have_geos():
        if not g1.IsValid():
            gdaltest.post_reason('fail')
            return 'fail'

    # Test GetEnvelope()
    env = g1.GetEnvelope()
    expected_env = (0.0, 2.0, -1.0, 1.0)
    for i in range(4):
        if abs(env[i] - expected_env[i]) > 1e-8:
            gdaltest.post_reason('fail')
            print(env)
            return 'fail'

    # Test Length()
    length = g1.Length()
    expected_length =  1.5 * math.pi
    if abs(length - expected_length) > 1e-8:
        gdaltest.post_reason('fail')
        print(length)
        return 'fail'

    # Test Value()
    p = g1.Value(-1)
    expected_p = ogr.CreateGeometryFromWkt('POINT (0 0)')
    if ogrtest.check_feature_geometry(p, expected_p) != 0:
        gdaltest.post_reason('fail')
        print(p)
        return 'fail'
        
    p = g1.Value(0)
    expected_p = ogr.CreateGeometryFromWkt('POINT (0 0)')
    if ogrtest.check_feature_geometry(p, expected_p) != 0:
        gdaltest.post_reason('fail')
        print(p)
        return 'fail'
        
    p = g1.Value(length / 6.0)
    expected_p = ogr.CreateGeometryFromWkt('POINT (0.292893218813453 0.707106781186548)')
    if ogrtest.check_feature_geometry(p, expected_p) != 0:
        gdaltest.post_reason('fail')
        print(p)
        
    p = g1.Value(length / 3.0)
    expected_p = ogr.CreateGeometryFromWkt('POINT (1 1)')
    if ogrtest.check_feature_geometry(p, expected_p) != 0:
        gdaltest.post_reason('fail')
        print(p)
        return 'fail'
        
    p = g1.Value(length / 2.0)
    expected_p = ogr.CreateGeometryFromWkt('POINT (1.707106781186547 0.707106781186547)')
    if ogrtest.check_feature_geometry(p, expected_p) != 0:
        gdaltest.post_reason('fail')
        print(p)
        return 'fail'
        
    p = g1.Value(2 * length / 3.0)
    expected_p = ogr.CreateGeometryFromWkt('POINT (2 0)')
    if ogrtest.check_feature_geometry(p, expected_p) != 0:
        gdaltest.post_reason('fail')
        print(p)
        return 'fail'

    p = g1.Value(length)
    expected_p = ogr.CreateGeometryFromWkt('POINT (1 -1)')
    if ogrtest.check_feature_geometry(p, expected_p) != 0:
        gdaltest.post_reason('fail')
        print(p)
        return 'fail'
        
    p = g1.Value(length + 1)
    expected_p = ogr.CreateGeometryFromWkt('POINT (1 -1)')
    if ogrtest.check_feature_geometry(p, expected_p) != 0:
        gdaltest.post_reason('fail')
        print(p)
        return 'fail'

    # Test export/import Wkb
    g2 = ogr.CreateGeometryFromWkb(g1.ExportToWkb())
    out_wkt = g2.ExportToWkt()
    if in_wkt != out_wkt:
        gdaltest.post_reason('fail')
        print(out_wkt)
        return 'fail'

    # With Z
    in_wkt = 'CIRCULARSTRING Z (0 0 10,1 1 20,2 0 30)'
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    if g1.GetGeometryType() != ogr.wkbCircularStringZ:
        gdaltest.post_reason('fail')
        return 'fail'
    out_wkt = g1.ExportToWkt()
    if in_wkt != out_wkt:
        gdaltest.post_reason('fail')
        print(out_wkt)
        return 'fail'

    wkb = g1.ExportToWkb()
    isowkb = g1.ExportToIsoWkb()
    if wkb != isowkb:
        gdaltest.post_reason('fail')
        return 'fail'
    g2 = ogr.CreateGeometryFromWkb(wkb)
    out_wkt = g2.ExportToWkt()
    if in_wkt != out_wkt:
        gdaltest.post_reason('fail')
        print(out_wkt)
        return 'fail'

    # Test stroking
    gdal.SetConfigOption('OGR_STROKE_CURVE', 'TRUE')
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', '45')
    in_wkt = 'CIRCULARSTRING (0 0,1 1,1 -1)'
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    gdal.SetConfigOption('OGR_STROKE_CURVE', 'FALSE')
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', None)

    expected_g = ogr.CreateGeometryFromWkt('LINESTRING (0 0,0.218168517531969 0.623489801858729,0.777479066043687 0.974927912181831,1.433883739117561 0.900968867902435,1.900968867902463 0.433883739117562,1.974927912181821 -0.222520933956316,1.623489801858719 -0.78183148246804,1 -1)')
    if ogrtest.check_feature_geometry(g1, expected_g) != 0:
        gdaltest.post_reason('fail')
        print(g1)
        return 'fail'


    in_wkt = 'CIRCULARSTRING (0 0,1 1,1 -1)'
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    in_wkb = g1.ExportToWkb()

    gdal.SetConfigOption('OGR_STROKE_CURVE', 'TRUE')
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', '45')
    g2 = ogr.CreateGeometryFromWkb(in_wkb)
    gdal.SetConfigOption('OGR_STROKE_CURVE', 'FALSE')
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', None)

    if ogrtest.check_feature_geometry(g2, expected_g) != 0:
        gdaltest.post_reason('fail')
        print(g2)
        return 'fail'

    # Test HasCurveGeometry
    if not g1.HasCurveGeometry():
        gdaltest.post_reason('fail')
        return 'fail'
    if not g1.HasCurveGeometry(True):
        gdaltest.post_reason('fail')
        return 'fail'

    # Test GetLinearGeometry
    g2 = g1.GetLinearGeometry(45)
    if ogrtest.check_feature_geometry(g2, expected_g) != 0:
        gdaltest.post_reason('fail')
        print(g2)
        return 'fail'

    # Test ForceToLineString
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', '45')
    g2 = ogr.ForceToLineString(g1)
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', None)
    if ogrtest.check_feature_geometry(g2, expected_g) != 0:
        gdaltest.post_reason('fail')
        print(g2)
        return 'fail'

    # Test ForceToMultiLineString
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', '45')
    expected_g2 = ogr.CreateGeometryFromWkt('MULTILINESTRING ((0 0,0.218168517531969 0.623489801858729,0.777479066043687 0.974927912181831,1.433883739117561 0.900968867902435,1.900968867902463 0.433883739117562,1.974927912181821 -0.222520933956316,1.623489801858719 -0.78183148246804,1 -1))')
    g2 = ogr.ForceToMultiLineString(g1)
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', None)
    if ogrtest.check_feature_geometry(g2, expected_g2) != 0:
        gdaltest.post_reason('fail')
        print(g2)
        return 'fail'

    # Test GEOS operations
    if ogrtest.have_geos():
        g2 = g1.Intersection(g1)
        if g2 is None:
            gdaltest.post_reason('fail')
            return 'fail'

    # Test CIRCULARSTRING wrapped in a GEOMETRYCOLLECTION
    in_wkt = 'GEOMETRYCOLLECTION(CIRCULARSTRING (0 0,1 1,1 -1))'
    g1 = ogr.CreateGeometryFromWkt(in_wkt)

    # Test GEOS operations
    if ogrtest.have_geos():
        g2 = g1.Intersection(g1)
        if g2 is None:
            gdaltest.post_reason('fail')
            return 'fail'

    # Test ForceToLineString
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', '45')
    g2 = ogr.ForceToLineString(g1)
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', None)
    if ogrtest.check_feature_geometry(g2, expected_g) != 0:
        gdaltest.post_reason('fail')
        print(g2)
        return 'fail'

    # Test ForceToMultiLineString
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', '45')
    g2 = ogr.ForceToMultiLineString(g1)
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', None)
    if ogrtest.check_feature_geometry(g2, expected_g2) != 0:
        gdaltest.post_reason('fail')
        print(g2)
        return 'fail'

    # Test stroking of full circle with 3 points. ISO draft
    # mentions that this should define a full circle, but there's an
    # ambiguity on the winding order. We choose counter-clock-wise order like
    # PostGIS. On the contrary, Microsoft for example forbids
    # such a possibility : http://msdn.microsoft.com/en-us/library/ff929141.aspx
    in_wkt = 'CIRCULARSTRING (0 0,1 0,0 0)'
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    expected_g = ogr.CreateGeometryFromWkt('LINESTRING (0 0,0.116977778440514 -0.321393804843282,0.413175911166547 -0.49240387650611,0.75 -0.433012701892224,0.969846310392967 -0.171010071662835,0.969846310392967 0.171010071662835,0.75 0.433012701892224,0.413175911166547 0.49240387650611,0.116977778440514 0.321393804843282,0 0)')
    g2 = g1.GetLinearGeometry(45)
    if ogrtest.check_feature_geometry(g2, expected_g) != 0:
        gdaltest.post_reason('fail')
        print(g2)
        return 'fail'

    length = g1.Length()
    expected_length =  2 * math.pi * 0.5
    if abs(length - expected_length) > 1e-8:
        gdaltest.post_reason('fail')
        print(length)
        return 'fail'

    # Test stroking of full circle (well defined)
    in_wkt = 'CIRCULARSTRING (0 0,0.5 0.5,1.0 0.0,0.5 -0.5,0.0 0.0)'
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    expected_g = ogr.CreateGeometryFromWkt('LINESTRING (0 0,0.049515566048791 0.216941869558781,0.188255099070638 0.390915741234018,0.388739533021848 0.48746395609092,0.611260466978166 0.48746395609092,0.811744900929369 0.390915741234018,0.950484433951232 0.216941869558781,1 0,0.950484433951232 -0.216941869558781,0.811744900929369 -0.390915741234018,0.611260466978166 -0.48746395609092,0.388739533021848 -0.48746395609092,0.188255099070638 -0.390915741234018,0.049515566048791 -0.216941869558781,0 0)')
    g2 = g1.GetLinearGeometry(45)
    if ogrtest.check_feature_geometry(g2, expected_g) != 0:
        gdaltest.post_reason('fail')
        print(g2)
        return 'fail'

    length = g1.Length()
    expected_length =  2 * math.pi * 0.5
    if abs(length - expected_length) > 1e-8:
        gdaltest.post_reason('fail')
        print(length)
        return 'fail'

    # Check segmentize
    g1.Segmentize(0.5)
    expected_g = ogr.CreateGeometryFromWkt('CIRCULARSTRING (0 0,0.146446609406726 0.353553390593274,0.5 0.5,0.853553390593274 0.353553390593274,1 0,0.853553390593274 -0.353553390593274,0.5 -0.5,0.146446609406726 -0.353553390593274,0 0)')
    if ogrtest.check_feature_geometry(g1, expected_g) != 0:
        gdaltest.post_reason('fail')
        print(g1)
        return 'fail'

    # Sanity check: the length must remain the same
    length = g1.Length()
    expected_length =  2 * math.pi * 0.5
    if abs(length - expected_length) > 1e-8:
        gdaltest.post_reason('fail')
        print(length)
        return 'fail'

    # Check segmentize symetry : do exact binary comparison
    in_wkt = 'CIRCULARSTRING (0 0,1.2 1,2 0)'
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    g1.Segmentize(0.25)
    in_wkt = 'CIRCULARSTRING (2 0,1.2 1,0 0)'
    g2 = ogr.CreateGeometryFromWkt(in_wkt)
    g2.Segmentize(0.25)
    for i in range(g1.GetPointCount()):
        if g1.GetPoint(i) != g2.GetPoint(g1.GetPointCount()-1-i):
            gdaltest.post_reason('fail')
            print(i)
            print('%.18g' % (g1.GetPoint(i)[0] - g2.GetPoint(g1.GetPointCount()-1-i)[0]))
            print('%.18g' % (g1.GetPoint(i)[1] - g2.GetPoint(g1.GetPointCount()-1-i)[1]))
            print(g1)
            print(g2)
            return 'fail'
    
    # Test stroking of full circle with Z
    in_wkt = 'CIRCULARSTRING (0 0 1,1 0 2,0 0 1)'
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    expected_g = ogr.CreateGeometryFromWkt('LINESTRING (0 0 1,0.116977778440514 -0.321393804843282 1,0.413175911166547 -0.49240387650611 1,0.75 -0.433012701892224 1,0.969846310392967 -0.171010071662835 1,0.969846310392967 0.171010071662835 1,0.75 0.433012701892224 1,0.413175911166547 0.49240387650611 1,0.116977778440514 0.321393804843282 1,0 0 1)')
    g2 = g1.GetLinearGeometry(45)
    if ogrtest.check_feature_geometry(g2, expected_g) != 0:
        gdaltest.post_reason('fail')
        print(g2)
        return 'fail'

    # Check segmentize
    g1.Segmentize(0.5)
    expected_g = ogr.CreateGeometryFromWkt('CIRCULARSTRING Z (0 0 1,0.146446609406726 -0.353553390593274 1.25,0.5 -0.5 1.5,0.853553390593274 -0.353553390593274 1.75,1 0 2,0.853553390593274 0.353553390593274 1.75,0.5 0.5 1.5,0.146446609406727 0.353553390593274 1.25,0 0 1)')
    if ogrtest.check_feature_geometry(g1, expected_g) != 0:
        gdaltest.post_reason('fail')
        print(g1)
        return 'fail'

    # Same as above but reverse order
    in_wkt = 'CIRCULARSTRING (0 0,0.5 -0.5,1.0 0.0,0.5 0.5,0.0 0.0)'
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    expected_g = ogr.CreateGeometryFromWkt('LINESTRING (0 0,0.049515566048791 -0.216941869558781,0.188255099070638 -0.390915741234018,0.388739533021848 -0.48746395609092,0.611260466978166 -0.48746395609092,0.811744900929369 -0.390915741234018,0.950484433951232 -0.216941869558781,1 0,0.950484433951232 0.216941869558781,0.811744900929369 0.390915741234018,0.611260466978166 0.48746395609092,0.388739533021848 0.48746395609092,0.188255099070638 0.390915741234018,0.049515566048791 0.216941869558781,0 0)')
    g2 = g1.GetLinearGeometry(45)
    if ogrtest.check_feature_geometry(g2, expected_g) != 0:
        gdaltest.post_reason('fail')
        print(g2)
        return 'fail'

    length = g1.Length()
    expected_length =  2 * math.pi * 0.5
    if abs(length - expected_length) > 1e-8:
        gdaltest.post_reason('fail')
        print(length)
        return 'fail'

    # Test stroking of a circular string with 3 colinear points
    in_wkt = 'CIRCULARSTRING (0 0,1 1,2 2)'
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    expected_g = ogr.CreateGeometryFromWkt('LINESTRING (0 0,1 1,2 2)')
    g2 = g1.GetLinearGeometry(45)
    if ogrtest.check_feature_geometry(g2, expected_g) != 0:
        gdaltest.post_reason('fail')
        print(g2)
        return 'fail'

    length = g1.Length()
    expected_length =  2 * math.sqrt(2)
    if abs(length - expected_length) > 1e-8:
        gdaltest.post_reason('fail')
        print(length)
        return 'fail'

    # Test Value()
    p = g1.Value(length / 4.0)
    expected_p = ogr.CreateGeometryFromWkt('POINT (0.5 0.5)')
    if ogrtest.check_feature_geometry(p, expected_p) != 0:
        gdaltest.post_reason('fail')
        print(p)
        return 'fail'
    p = g1.Value(3.0 * length / 4.0)
    expected_p = ogr.CreateGeometryFromWkt('POINT (1.5 1.5)')
    if ogrtest.check_feature_geometry(p, expected_p) != 0:
        gdaltest.post_reason('fail')
        print(p)
        return 'fail'

    # Check segmentize
    g1.Segmentize(0.5)
    expected_g = ogr.CreateGeometryFromWkt('CIRCULARSTRING (0 0,0.166666666666667 0.166666666666667,0.333333333333333 0.333333333333333,0.5 0.5,0.666666666666667 0.666666666666667,0.833333333333333 0.833333333333333,1 1,1.166666666666667 1.166666666666667,1.333333333333333 1.333333333333333,1.5 1.5,1.666666666666667 1.666666666666667,1.833333333333333 1.833333333333333,2 2)')
    if ogrtest.check_feature_geometry(g1, expected_g) != 0:
        gdaltest.post_reason('fail')
        print(g1)
        return 'fail'

    # Sanity check: the length must remain the same
    length = g1.Length()
    expected_length =  2 * math.sqrt(2)
    if abs(length - expected_length) > 1e-8:
        gdaltest.post_reason('fail')
        print(length)
        return 'fail'

    # Same with Z
    in_wkt = 'CIRCULARSTRING (0 0 1,1 1 2,2 2 1)'
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    expected_g = ogr.CreateGeometryFromWkt('LINESTRING (0 0 1,1 1 2,2 2 1)')
    g2 = g1.GetLinearGeometry(45)
    if ogrtest.check_feature_geometry(g2, expected_g) != 0:
        gdaltest.post_reason('fail')
        print(g2)
        return 'fail'

    # Check segmentize
    g1.Segmentize(0.5)
    expected_g = ogr.CreateGeometryFromWkt('CIRCULARSTRING Z (0 0 1,0.166666666666667 0.166666666666667 1.166666666666667,0.333333333333333 0.333333333333333 1.333333333333333,0.5 0.5 1.5,0.666666666666667 0.666666666666667 1.666666666666667,0.833333333333333 0.833333333333333 1.833333333333333,1 1 2,1.166666666666667 1.166666666666667 1.833333333333333,1.333333333333333 1.333333333333333 1.666666666666667,1.5 1.5 1.5,1.666666666666667 1.666666666666667 1.333333333333333,1.833333333333333 1.833333333333333 1.166666666666667,2 2 1)')
    if ogrtest.check_feature_geometry(g1, expected_g) != 0:
        gdaltest.post_reason('fail')
        print(g1)
        return 'fail'

    # Test Value()
    p = g1.Value(length / 4.0)
    expected_p = ogr.CreateGeometryFromWkt('POINT (0.5 0.5 1.5)')
    if ogrtest.check_feature_geometry(p, expected_p) != 0:
        gdaltest.post_reason('fail')
        print(p)
        return 'fail'
    p = g1.Value(3.0 * length / 4.0)
    expected_p = ogr.CreateGeometryFromWkt('POINT (1.5 1.5 1.5)')
    if ogrtest.check_feature_geometry(p, expected_p) != 0:
        gdaltest.post_reason('fail')
        print(p)
        return 'fail'
    in_wkt = 'CIRCULARSTRING (0 0,1 1,1 -1)'

    # Test GetEnvelope() in various cases
    cx = 1
    cy = 2
    r = 3

    # In quadrant 0
    a0 = math.pi / 3
    a1 = math.pi / 4
    a2 = math.pi / 6
    in_wkt = 'CIRCULARSTRING(%.16g %.16g,%.16g %.16g,%.16g %.16g)' % ( \
            cx + r * math.cos(a0), cy + r * math.sin(a0), \
            cx + r * math.cos(a1), cy + r * math.sin(a1), \
            cx + r * math.cos(a2), cy + r * math.sin(a2))
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    env = g1.GetEnvelope()
    expected_env = (cx + r * math.cos(a0), cx + r * math.cos(a2), cy + r * math.sin(a2), cy + r * math.sin(a0))
    for i in range(4):
        if abs(env[i] - expected_env[i]) > 1e-8:
            gdaltest.post_reason('fail')
            print(env)
            return 'fail'

    # From quadrant 0 to quadrant -1
    a0 = math.pi / 3
    a1 = math.pi / 6
    a2 = -math.pi / 6
    in_wkt = 'CIRCULARSTRING(%.16g %.16g,%.16g %.16g,%.16g %.16g)' % ( \
            cx + r * math.cos(a0), cy + r * math.sin(a0), \
            cx + r * math.cos(a1), cy + r * math.sin(a1), \
            cx + r * math.cos(a2), cy + r * math.sin(a2))
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    env = g1.GetEnvelope()
    expected_env = (cx + r * math.cos(a0), cx + r, cy + r * math.sin(a2), cy + r * math.sin(a0))
    for i in range(4):
        if abs(env[i] - expected_env[i]) > 1e-8:
            gdaltest.post_reason('fail')
            print(env)
            return 'fail'

    # From quadrant 0 to quadrant 3
    a0 = math.pi / 3
    a1 = math.pi - math.pi / 3
    a2 = -math.pi / 6
    in_wkt = 'CIRCULARSTRING(%.16g %.16g,%.16g %.16g,%.16g %.16g)' % ( \
            cx + r * math.cos(a0), cy + r * math.sin(a0), \
            cx + r * math.cos(a1), cy + r * math.sin(a1), \
            cx + r * math.cos(a2), cy + r * math.sin(a2))
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    env = g1.GetEnvelope()
    expected_env = (cx - r, cx + r * math.cos(a2), cy - r, cy + r)
    for i in range(4):
        if abs(env[i] - expected_env[i]) > 1e-8:
            gdaltest.post_reason('fail')
            print(env)
            return 'fail'

    # From quadrant 0 to quadrant 4
    a0 = math.pi / 3
    a1 = math.pi - math.pi / 3
    a2 = math.pi / 6
    in_wkt = 'CIRCULARSTRING(%.16g %.16g,%.16g %.16g,%.16g %.16g)' % ( \
            cx + r * math.cos(a0), cy + r * math.sin(a0), \
            cx + r * math.cos(a1), cy + r * math.sin(a1), \
            cx + r * math.cos(a2), cy + r * math.sin(a2))
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    env = g1.GetEnvelope()
    expected_env = (cx - r, cx + r, cy - r, cy + r)
    for i in range(4):
        if abs(env[i] - expected_env[i]) > 1e-8:
            gdaltest.post_reason('fail')
            print(env)
            return 'fail'

    # Full circle
    a0 = math.pi / 3
    a1 = math.pi + math.pi / 3
    a2 = math.pi / 3
    in_wkt = 'CIRCULARSTRING(%.16g %.16g,%.16g %.16g,%.16g %.16g)' % ( \
            cx + r * math.cos(a0), cy + r * math.sin(a0), \
            cx + r * math.cos(a1), cy + r * math.sin(a1), \
            cx + r * math.cos(a2), cy + r * math.sin(a2))
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    env = g1.GetEnvelope()
    expected_env = (cx - r, cx + r, cy - r, cy + r)
    for i in range(4):
        if abs(env[i] - expected_env[i]) > 1e-8:
            gdaltest.post_reason('fail')
            print(env)
            return 'fail'

    # Error case : not enough points
    in_wkt = 'CIRCULARSTRING (0 0)'
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    g = ogr.CreateGeometryFromWkt(in_wkt)
    gdal.PopErrorHandler()
    if g is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # EMPTY
    for in_wkt in [ 'CIRCULARSTRING EMPTY', 'CIRCULARSTRING Z EMPTY' ]:
        g1 = ogr.CreateGeometryFromWkt(in_wkt)
        out_wkt = g1.ExportToWkt()
        if in_wkt != out_wkt:
            gdaltest.post_reason('fail')
            print(in_wkt)
            print(out_wkt)
            return 'fail'
        if g1.Length() != 0.0:
            gdaltest.post_reason('fail')
            print(in_wkt)
            return 'fail'

        g2 = ogr.CreateGeometryFromWkb(g1.ExportToWkb())
        out_wkt = g2.ExportToWkt()
        if in_wkt != out_wkt:
            gdaltest.post_reason('fail')
            print(in_wkt)
            print(out_wkt)
            return 'fail'

    return 'success'

###############################################################################
# Test OGRCompoundCurve

def ogr_geom_compoundcurve():

    in_wkt = 'COMPOUNDCURVE (CIRCULARSTRING (0 0,1 1,1 -1))'
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    if g1.GetGeometryType() != ogr.wkbCompoundCurve:
        gdaltest.post_reason('fail')
        return 'fail'
    out_wkt = g1.ExportToWkt()
    if in_wkt != out_wkt:
        gdaltest.post_reason('fail')
        print(out_wkt)
        return 'fail'

    if g1.GetGeometryCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    if g1.GetGeometryRef(0).ExportToWkt() != 'CIRCULARSTRING (0 0,1 1,1 -1)':
        gdaltest.post_reason('fail')
        return 'fail'

    env = g1.GetEnvelope()
    expected_env = (0.0, 2.0, -1.0, 1.0)
    for i in range(4):
        if abs(env[i] - expected_env[i]) > 1e-8:
            gdaltest.post_reason('fail')
            print(env)
            return 'fail'

    length = g1.Length()
    expected_length =  1.5 * math.pi
    if abs(length - expected_length) > 1e-8:
        gdaltest.post_reason('fail')
        print(length)
        return 'fail'

    g2 = ogr.CreateGeometryFromWkb(g1.ExportToWkb())
    out_wkt = g2.ExportToWkt()
    if in_wkt != out_wkt:
        gdaltest.post_reason('fail')
        print(out_wkt)
        return 'fail'

    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    if not g1.HasCurveGeometry():
        gdaltest.post_reason('fail')
        return 'fail'
    if not g1.HasCurveGeometry(True):
        gdaltest.post_reason('fail')
        return 'fail'

    # CreateGeometryFromWkt of LINESTRING
    in_wkt = 'COMPOUNDCURVE ((0 0,0 10))'
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    if not g1.HasCurveGeometry():
        gdaltest.post_reason('fail')
        return 'fail'
    if g1.HasCurveGeometry(True):
        gdaltest.post_reason('fail')
        return 'fail'

    # With Z
    in_wkt = 'COMPOUNDCURVE Z (CIRCULARSTRING Z (0 0 10,1 1 20,2 0 30),(2 0 30,0 0 10))'
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    if g1.GetGeometryType() != ogr.wkbCompoundCurveZ:
        gdaltest.post_reason('fail')
        return 'fail'
    out_wkt = g1.ExportToWkt()
    if in_wkt != out_wkt:
        gdaltest.post_reason('fail')
        print(out_wkt)
        return 'fail'

    env = g1.GetEnvelope()
    expected_env = (0.0, 2.0, 0.0, 1.0)
    for i in range(4):
        if abs(env[i] - expected_env[i]) > 1e-8:
            gdaltest.post_reason('fail')
            print(env)
            return 'fail'

    env = g1.GetEnvelope3D()
    expected_env = (0.0, 2.0, 0.0, 1.0, 10, 30)
    for i in range(6):
        if abs(env[i] - expected_env[i]) > 1e-8:
            gdaltest.post_reason('fail')
            print(env)
            return 'fail'

    # Test Value()
    p = g1.Value(-1e-3)
    expected_p = ogr.CreateGeometryFromWkt('POINT (0 0 10)')
    if ogrtest.check_feature_geometry(p, expected_p) != 0:
        gdaltest.post_reason('fail')
        print(p)
        return 'fail'
        
    p = g1.Value(math.pi / 2.0)
    expected_p = ogr.CreateGeometryFromWkt('POINT (1 1 20)')
    if ogrtest.check_feature_geometry(p, expected_p) != 0:
        gdaltest.post_reason('fail')
        print(p)
        return 'fail'

    p = g1.Value(math.pi)
    expected_p = ogr.CreateGeometryFromWkt('POINT (2 0 30)')
    if ogrtest.check_feature_geometry(p, expected_p) != 0:
        gdaltest.post_reason('fail')
        print(p)
        return 'fail'

    p = g1.Value(math.pi+1)
    expected_p = ogr.CreateGeometryFromWkt('POINT (1 0 20)')
    if ogrtest.check_feature_geometry(p, expected_p) != 0:
        gdaltest.post_reason('fail')
        print(p)
        return 'fail'

    p = g1.Value(math.pi+2+1e-3)
    expected_p = ogr.CreateGeometryFromWkt('POINT (0 0 10)')
    if ogrtest.check_feature_geometry(p, expected_p) != 0:
        gdaltest.post_reason('fail')
        print(p)
        return 'fail'
        
    wkb = g1.ExportToWkb()
    isowkb = g1.ExportToIsoWkb()
    if wkb != isowkb:
        gdaltest.post_reason('fail')
        return 'fail'
    g2 = ogr.CreateGeometryFromWkb(wkb)
    out_wkt = g2.ExportToWkt()
    if in_wkt != out_wkt:
        gdaltest.post_reason('fail')
        print(out_wkt)
        return 'fail'

    # Several parts
    in_wkt = 'COMPOUNDCURVE (CIRCULARSTRING (0 0,1 1,1 -1),(1 -1,0 0))'
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    if g1.GetGeometryType() != ogr.wkbCompoundCurve:
        gdaltest.post_reason('fail')
        return 'fail'
    out_wkt = g1.ExportToWkt()
    if in_wkt != out_wkt:
        gdaltest.post_reason('fail')
        print(out_wkt)
        return 'fail'

    if not g1.Equals(g1):
        gdaltest.post_reason('fail')
        return 'fail'

    if not g1.Equals(g1.Clone()):
        gdaltest.post_reason('fail')
        return 'fail'

    if g1.Equals(ogr.CreateGeometryFromWkt('POINT(0 0)')):
        gdaltest.post_reason('fail')
        return 'fail'

    if g1.Equals(ogr.CreateGeometryFromWkt('COMPOUNDCURVE (CIRCULARSTRING (0 0,1 1,1 -1))')):
        gdaltest.post_reason('fail')
        return 'fail'

    if g1.Equals(ogr.CreateGeometryFromWkt('COMPOUNDCURVE (CIRCULARSTRING (0 0,1 1,1 -1),(1 -1,0 1))')):
        gdaltest.post_reason('fail')
        return 'fail'

    length = g1.Length()
    expected_length =  1.5 * math.pi + math.sqrt(2)
    if abs(length - expected_length) > 1e-8:
        gdaltest.post_reason('fail')
        print(length)
        return 'fail'

    g2 = ogr.CreateGeometryFromWkb(g1.ExportToWkb())
    out_wkt = g2.ExportToWkt()
    if in_wkt != out_wkt:
        gdaltest.post_reason('fail')
        print(out_wkt)
        return 'fail'

    # Test stroking
    gdal.SetConfigOption('OGR_STROKE_CURVE', 'TRUE')
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', '45')
    in_wkt = 'COMPOUNDCURVE (CIRCULARSTRING (0 0,1 1,1 -1))'
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    gdal.SetConfigOption('OGR_STROKE_CURVE', 'FALSE')
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', None)

    expected_g = ogr.CreateGeometryFromWkt('LINESTRING (0 0,0.218168517531969 0.623489801858729,0.777479066043687 0.974927912181831,1.433883739117561 0.900968867902435,1.900968867902463 0.433883739117562,1.974927912181821 -0.222520933956316,1.623489801858719 -0.78183148246804,1 -1)')
    if ogrtest.check_feature_geometry(g1, expected_g) != 0:
        gdaltest.post_reason('fail')
        print(g1)
        return 'fail'

    in_wkt = 'COMPOUNDCURVE (CIRCULARSTRING (0 0,1 1,1 -1))'
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    in_wkb = g1.ExportToWkb()

    gdal.SetConfigOption('OGR_STROKE_CURVE', 'TRUE')
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', '45')
    g2 = ogr.CreateGeometryFromWkb(in_wkb)
    gdal.SetConfigOption('OGR_STROKE_CURVE', 'FALSE')
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', None)

    expected_g = ogr.CreateGeometryFromWkt('LINESTRING (0 0,0.218168517531969 0.623489801858729,0.777479066043687 0.974927912181831,1.433883739117561 0.900968867902435,1.900968867902463 0.433883739117562,1.974927912181821 -0.222520933956316,1.623489801858719 -0.78183148246804,1 -1)')
    if ogrtest.check_feature_geometry(g2, expected_g) != 0:
        gdaltest.post_reason('fail')
        print(g2)
        return 'fail'

    g2 = g1.GetLinearGeometry(45)
    if ogrtest.check_feature_geometry(g2, expected_g) != 0:
        gdaltest.post_reason('fail')
        print(g2)
        return 'fail'

    if not g1.HasCurveGeometry():
        gdaltest.post_reason('fail')
        return 'fail'

    if not g1.HasCurveGeometry(True):
        gdaltest.post_reason('fail')
        return 'fail'

    g2 = g1.GetLinearGeometry(45)
    if ogrtest.check_feature_geometry(g2, expected_g) != 0:
        gdaltest.post_reason('fail')
        print(g2)
        return 'fail'

    gdal.SetConfigOption('OGR_ARC_STEPSIZE', '45')
    g2 = ogr.ForceToLineString(g1)
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', None)
    if ogrtest.check_feature_geometry(g2, expected_g) != 0:
        gdaltest.post_reason('fail')
        print(g2)
        return 'fail'

    gdal.SetConfigOption('OGR_ARC_STEPSIZE', '45')
    expected_g = ogr.CreateGeometryFromWkt('MULTILINESTRING ((0 0,0.218168517531969 0.623489801858729,0.777479066043687 0.974927912181831,1.433883739117561 0.900968867902435,1.900968867902463 0.433883739117562,1.974927912181821 -0.222520933956316,1.623489801858719 -0.78183148246804,1 -1))')
    g2 = ogr.ForceToMultiLineString(g1)
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', None)
    if ogrtest.check_feature_geometry(g2, expected_g) != 0:
        gdaltest.post_reason('fail')
        print(g2)
        return 'fail'

    # Check segmentize
    g1.Segmentize(0.5)
    expected_g = ogr.CreateGeometryFromWkt('COMPOUNDCURVE (CIRCULARSTRING (0 0,0.076120467488713 0.38268343236509,0.292893218813453 0.707106781186548,0.61731656763491 0.923879532511287,1 1,1.38268343236509 0.923879532511287,1.707106781186547 0.707106781186547,1.923879532511287 0.38268343236509,2 0,1.923879532511287 -0.38268343236509,1.707106781186547 -0.707106781186547,1.38268343236509 -0.923879532511287,1 -1))')
    if ogrtest.check_feature_geometry(g1, expected_g) != 0:
        gdaltest.post_reason('fail')
        print(g1)
        return 'fail'

    # Error case : not enough points
    in_wkt = 'COMPOUNDCURVE ((0 0))'
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    g = ogr.CreateGeometryFromWkt(in_wkt)
    gdal.PopErrorHandler()
    if g is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Error case : invalid curve
    in_wkt = 'COMPOUNDCURVE (COMPOUNDCURVE EMPTY)'
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    g = ogr.CreateGeometryFromWkt(in_wkt)
    gdal.PopErrorHandler()
    if g is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Error case : non contiguous curves
    in_wkt = 'COMPOUNDCURVE ((0 0,1 1),(2 2,3 3))'
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    g = ogr.CreateGeometryFromWkt(in_wkt)
    gdal.PopErrorHandler()
    if g is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Error case : non contiguous curves
    in_wkt = 'COMPOUNDCURVE (EMPTY,(2 2,3 3))'
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    g = ogr.CreateGeometryFromWkt(in_wkt)
    gdal.PopErrorHandler()
    if g is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Error case : non contiguous curves
    in_wkt = 'COMPOUNDCURVE ((2 2,3 3), EMPTY)'
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    g = ogr.CreateGeometryFromWkt(in_wkt)
    gdal.PopErrorHandler()
    if g is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    g = ogr.Geometry(ogr.wkbCompoundCurve)
    g.AddGeometry(ogr.CreateGeometryFromWkt('LINESTRING(0 0,1 1)'))
    if g.ExportToWkt() != 'COMPOUNDCURVE ((0 0,1 1))':
            gdaltest.post_reason('fail')
            return 'fail'

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    g.AddGeometry(ogr.CreateGeometryFromWkt('LINESTRING(0 0,1 1)'))
    gdal.PopErrorHandler()
    if g.ExportToWkt() != 'COMPOUNDCURVE ((0 0,1 1))':
            gdaltest.post_reason('fail')
            return 'fail'

    g = ogr.Geometry(ogr.wkbCompoundCurve)
    g.AddGeometryDirectly(ogr.CreateGeometryFromWkt('LINESTRING(0 0,1 1)'))
    if g.ExportToWkt() != 'COMPOUNDCURVE ((0 0,1 1))':
            gdaltest.post_reason('fail')
            return 'fail'

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    g.AddGeometryDirectly(ogr.CreateGeometryFromWkt('LINESTRING(0 0,1 1)'))
    gdal.PopErrorHandler()
    if g.ExportToWkt() != 'COMPOUNDCURVE ((0 0,1 1))':
            gdaltest.post_reason('fail')
            return 'fail'

    # Cannot add compoundcurve in compoundcurve
    g = ogr.Geometry(ogr.wkbCompoundCurve)
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    g.AddGeometryDirectly(ogr.CreateGeometryFromWkt('COMPOUNDCURVE((1 1,2 2))'))
    gdal.PopErrorHandler()
    if g.ExportToWkt() != 'COMPOUNDCURVE EMPTY':
            gdaltest.post_reason('fail')
            return 'fail'

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
                gdaltest.post_reason('fail')
                print(g2)
                print(g3)
                print(i)
                print(abs(g2.GetX(i) - g3.GetX(p_count - 1 - i)))
                print(abs(g2.GetY(i) - g3.GetY(p_count - 1 - i)))
                return 'fail'

    # Test Transform
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4326)
    g.AssignSpatialReference(sr)
    g.TransformTo(sr)

    # Invalid wkb
    wkb_list = [ '\x01\x09\x00\x00\x00\x01\x00\x00\x00', # subgeometry declared but not present
                 '\x01\x09\x00\x00\x00\xff\xff\xff\x7f', # 2 billion subgeometries declared !
                 '\x01\x09\x00\x00\x00\x01\x00\x00\x00\x01\xff\x00\x00\x00\x00\x00\x00\x00', # subgeometry invalid: unknown type
                 '\x01\x09\x00\x00\x00\x01\x00\x00\x00\x01\x02\x00\x00\x00\x01\x00\x00\x00', # subgeometry invalid: linestring truncated
                 '\x01\x09\x00\x00\x00\x01\x00\x00\x00\x01\x02\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00', # subgeometry invalid: linestring with one point
                 '\x01\x09\x00\x00\x00\x01\x00\x00\x00\x01\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00', # subgeometry invalid: point
                 '\x01\x09\x00\x00\x00\x01\x00\x00\x00\x01\x09\x00\x00\x00\x00\x00\x00\x00', # subgeometry invalid: compoundcurve
               ]
    for wkb in wkb_list:
        gdal.PushErrorHandler('CPLQuietErrorHandler')
        g = ogr.CreateGeometryFromWkb(wkb)
        gdal.PopErrorHandler()
        if g is not None:
            gdaltest.post_reason('fail')
            print(wkb)
            return 'fail'

    # EMPTY
    for in_wkt in [ 'COMPOUNDCURVE EMPTY', 'COMPOUNDCURVE Z EMPTY' ]:
        g1 = ogr.CreateGeometryFromWkt(in_wkt)
        out_wkt = g1.ExportToWkt()
        if in_wkt != out_wkt:
            gdaltest.post_reason('fail')
            print(in_wkt)
            print(out_wkt)
            return 'fail'
        if g1.Length() != 0.0:
            gdaltest.post_reason('fail')
            print(in_wkt)
            return 'fail'

        g2 = ogr.CreateGeometryFromWkb(g1.ExportToWkb())
        out_wkt = g2.ExportToWkt()
        if in_wkt != out_wkt:
            gdaltest.post_reason('fail')
            print(in_wkt)
            print(out_wkt)
            return 'fail'

    return 'success'

###############################################################################
# Test OGRCurvePolygon

def ogr_geom_curvepolygon():

    in_wkt = 'CURVEPOLYGON (CIRCULARSTRING (0 0,1 0,0 0))'
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    if g1.GetGeometryType() != ogr.wkbCurvePolygon:
        gdaltest.post_reason('fail')
        return 'fail'
    out_wkt = g1.ExportToWkt()
    if in_wkt != out_wkt:
        gdaltest.post_reason('fail')
        print(out_wkt)
        return 'fail'

    if ogrtest.have_geos():
        p1 = g1.PointOnSurface()
        if (p1.GetX()-0.5)*(p1.GetX()-0.5)+p1.GetY()*p1.GetY() > 0.5 * 0.5:
            gdaltest.post_reason('fail')
            print(p1)
            return 'fail'

    env = g1.GetEnvelope()
    expected_env = (0.0, 1.0, -0.5, 0.5)
    for i in range(4):
        if abs(env[i] - expected_env[i]) > 1e-8:
            gdaltest.post_reason('fail')
            print(env)
            return 'fail'

    area = g1.Area()
    expected_area = math.pi * 0.5 * 0.5
    if abs(area - expected_area) > 1e-8:
        gdaltest.post_reason('fail')
        print(area)
        print(expected_area)
        return 'fail'

    g2 = ogr.CreateGeometryFromWkb(g1.ExportToWkb())
    out_wkt = g2.ExportToWkt()
    if in_wkt != out_wkt:
        gdaltest.post_reason('fail')
        print(out_wkt)
        return 'fail'

    # CURVEPOLYGON of LINESTRING
    in_wkt = 'CURVEPOLYGON ((0 0,0 10,10 10,10 0,0 0))'
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    if not g1.HasCurveGeometry():
        gdaltest.post_reason('fail')
        return 'fail'
    if g1.HasCurveGeometry(True):
        gdaltest.post_reason('fail')
        return 'fail'

    # With Z
    in_wkt = 'CURVEPOLYGON Z (CIRCULARSTRING Z (0 0 10,1 0 10,0 0 10))'
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    if g1.GetGeometryType() != ogr.wkbCurvePolygonZ:
        gdaltest.post_reason('fail')
        return 'fail'
    out_wkt = g1.ExportToWkt()
    if in_wkt != out_wkt:
        gdaltest.post_reason('fail')
        print(out_wkt)
        return 'fail'

    wkb = g1.ExportToWkb()
    isowkb = g1.ExportToIsoWkb()
    if wkb != isowkb:
        gdaltest.post_reason('fail')
        return 'fail'
    g2 = ogr.CreateGeometryFromWkb(wkb)
    out_wkt = g2.ExportToWkt()
    if in_wkt != out_wkt:
        gdaltest.post_reason('fail')
        print(out_wkt)
        return 'fail'

    # Test stroking
    gdal.SetConfigOption('OGR_STROKE_CURVE', 'TRUE')
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', '45')
    in_wkt = 'CURVEPOLYGON (CIRCULARSTRING (0 0,1 0,0 0))'
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    gdal.SetConfigOption('OGR_STROKE_CURVE', 'FALSE')
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', None)

    expected_g = ogr.CreateGeometryFromWkt('POLYGON ((0 0,0.116977778440514 -0.321393804843282,0.413175911166547 -0.49240387650611,0.75 -0.433012701892224,0.969846310392967 -0.171010071662835,0.969846310392967 0.171010071662835,0.75 0.433012701892224,0.413175911166547 0.49240387650611,0.116977778440514 0.321393804843282,0 0))')
    if ogrtest.check_feature_geometry(g1, expected_g) != 0:
        gdaltest.post_reason('fail')
        print(g1)
        return 'fail'


    in_wkt = 'CURVEPOLYGON (CIRCULARSTRING (0 0,1 0,0 0))'
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    in_wkb = g1.ExportToWkb()

    gdal.SetConfigOption('OGR_STROKE_CURVE', 'TRUE')
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', '45')
    g2 = ogr.CreateGeometryFromWkb(in_wkb)
    gdal.SetConfigOption('OGR_STROKE_CURVE', 'FALSE')
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', None)

    if ogrtest.check_feature_geometry(g2, expected_g) != 0:
        gdaltest.post_reason('fail')
        print(g2)
        return 'fail'

    # Test HasCurveGeometry
    if not g1.HasCurveGeometry():
        gdaltest.post_reason('fail')
        return 'fail'
    if not g1.HasCurveGeometry(True):
        gdaltest.post_reason('fail')
        return 'fail'

    # Test GetLinearGeometry
    g2 = g1.GetLinearGeometry(45)
    if ogrtest.check_feature_geometry(g2, expected_g) != 0:
        gdaltest.post_reason('fail')
        print(g2)
        return 'fail'

    # Test ForceToPolygon
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', '45')
    g2 = ogr.ForceToPolygon(g1)
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', None)
    if ogrtest.check_feature_geometry(g2, expected_g) != 0:
        gdaltest.post_reason('fail')
        print(g2)
        return 'fail'

    # Test ForceToMultiPolygon
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', '45')
    expected_g2 = ogr.CreateGeometryFromWkt('MULTIPOLYGON (((0 0,0.116977778440514 -0.321393804843282,0.413175911166547 -0.49240387650611,0.75 -0.433012701892224,0.969846310392967 -0.171010071662835,0.969846310392967 0.171010071662835,0.75 0.433012701892224,0.413175911166547 0.49240387650611,0.116977778440514 0.321393804843282,0 0)))')
    g2 = ogr.ForceToMultiPolygon(g1)
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', None)
    if ogrtest.check_feature_geometry(g2, expected_g2) != 0:
        gdaltest.post_reason('fail')
        print(g2)
        return 'fail'

    # Test ForceToMultiLineString
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', '45')
    g2 = ogr.ForceToMultiLineString(g1)
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', None)
    expected_g3 = ogr.CreateGeometryFromWkt('MULTILINESTRING ((0 0,0.116977778440514 -0.321393804843282,0.413175911166547 -0.49240387650611,0.75 -0.433012701892224,0.969846310392967 -0.171010071662835,0.969846310392967 0.171010071662835,0.75 0.433012701892224,0.413175911166547 0.49240387650611,0.116977778440514 0.321393804843282,0 0))')
    if ogrtest.check_feature_geometry(g2, expected_g3) != 0:
        gdaltest.post_reason('fail')
        print(g2)
        return 'fail'

    # Test GEOS operations
    if ogrtest.have_geos():
        g2 = g1.Intersection(g1)
        if g2 is None:
            gdaltest.post_reason('fail')
            return 'fail'

    # Test CURVEPOLYGON and COMPOUNDCURVE, CIRCULARSTRING, LINESTRING
    in_wkt = 'CURVEPOLYGON (COMPOUNDCURVE (CIRCULARSTRING (0 0,1 1,2 0),(2 0,0 0)),(0.1 0.1,0.1 0.2,0.2 0.2,0.2 0.1,0.1 0.1),CIRCULARSTRING (0.25 0.25,0.75 0.25,0.25 0.25))'
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    g2 = ogr.CreateGeometryFromWkb(g1.ExportToWkb())
    if g2.ExportToWkt() != in_wkt:
        gdaltest.post_reason('fail')
        print(g2)
        return 'fail'

    # Test CURVEPOLYGON wrapped in a GEOMETRYCOLLECTION
    in_wkt = 'GEOMETRYCOLLECTION(CURVEPOLYGON(CIRCULARSTRING (0 0,1 0,0 0)))'
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    g2 = ogr.CreateGeometryFromWkb(g1.ExportToWkb())
    if not g1.Equals(g2):
        gdaltest.post_reason('fail')
        print(g2)
        return 'fail'

    # Test GEOS operations
    if ogrtest.have_geos():
        g2 = g1.Intersection(g1)
        if g2 is None:
            gdaltest.post_reason('fail')
            return 'fail'

    # Test ForceToPolygon
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', '45')
    g2 = ogr.ForceToPolygon(g1)
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', None)
    if ogrtest.check_feature_geometry(g2, expected_g) != 0:
        gdaltest.post_reason('fail')
        print(g2)
        return 'fail'

    # Test ForceToMultiPolygon
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', '45')
    g2 = ogr.ForceToMultiPolygon(g1)
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', None)
    if ogrtest.check_feature_geometry(g2, expected_g2) != 0:
        gdaltest.post_reason('fail')
        print(g2)
        return 'fail'

    # Error case : not enough points
    in_wkt = 'CURVEPOLYGON ((0 0,0 1,0 0))'
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    g = ogr.CreateGeometryFromWkt(in_wkt)
    gdal.PopErrorHandler()
    if g is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Error case : wrong sub-geometry type
    in_wkt = 'CURVEPOLYGON (POINT EMPTY)'
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    g = ogr.CreateGeometryFromWkt(in_wkt)
    gdal.PopErrorHandler()
    if g is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Error case: non closed ring
    in_wkt = 'CURVEPOLYGON ((0 0,0 1,1 1,1 0))'
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    g = ogr.CreateGeometryFromWkt(in_wkt)
    gdal.PopErrorHandler()
    if g is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Area
    g = ogr.CreateGeometryFromWkt('CURVEPOLYGON ((0 0,1 1,1 0,0 0))')
    if g.Area() != 0.5:
        gdaltest.post_reason('fail')
        return 'fail'

    g = ogr.CreateGeometryFromWkt('CURVEPOLYGON (COMPOUNDCURVE((0 0,1 1,1 0,0 0)))')
    if g.Area() != 0.5:
        gdaltest.post_reason('fail')
        return 'fail'

    g = ogr.CreateGeometryFromWkt('CURVEPOLYGON (COMPOUNDCURVE((0 0,1 1),(1 1,1 0,0 0)))')
    if g.Area() != 0.5:
        gdaltest.post_reason('fail')
        return 'fail'

    g = ogr.CreateGeometryFromWkt('CURVEPOLYGON ((100000000 100000000,100000001 100000001,100000001 100000000,100000000 100000000))')
    if g.Area() != 0.5:
        gdaltest.post_reason('fail')
        return 'fail'

    # Equals
    g1 = ogr.CreateGeometryFromWkt('CURVEPOLYGON ((0 0,1 1,1 0,0 0),CIRCULARSTRING(0.75 0.5,0.85 0.5,0.75 0.5))')
    if not g1.Equals(g1):
        gdaltest.post_reason('fail')
        return 'fail'

    if not g1.Equals(g1.Clone()):
        gdaltest.post_reason('fail')
        return 'fail'

    if g1.Equals(ogr.CreateGeometryFromWkt('POINT(0 0)')):
        gdaltest.post_reason('fail')
        return 'fail'

    if g1.Equals(ogr.CreateGeometryFromWkt('CURVEPOLYGON ((0 0,1 1,1 0,0 0))')):
        gdaltest.post_reason('fail')
        return 'fail'

    # Intersects optimizations on a circle
    g1 = ogr.CreateGeometryFromWkt('CURVEPOLYGON (CIRCULARSTRING (0 0,2 0,0 0))')
    # Point slighly within circle
    p1 = ogr.CreateGeometryFromWkt('POINT (%.16g %.16g)' % (1 + math.cos(math.pi/6)-1e-4,math.sin(math.pi/6)))
    # To prove that we don't use discretization
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', '45')
    res = g1.Intersects(p1)
    res = res & p1.Intersects(g1)
    res = res & g1.Contains(p1)
    res = res & p1.Within(g1)
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', None)
    if not res:
        gdaltest.post_reason('fail')
        return 'fail'

    # Test point slighly outside circle
    p2 = ogr.CreateGeometryFromWkt('POINT (%.16g %.16g)' % (1 + math.cos(math.pi/6)+1e-4,math.sin(math.pi/6)))
    if p2.Within(g1):
        gdaltest.post_reason('fail')
        return 'fail'

    # Full circle defined by 2 arcs
    g1 = ogr.CreateGeometryFromWkt('CURVEPOLYGON (CIRCULARSTRING (0 0,1 1,2 0,1 -1,0 0))')
    if not p1.Within(g1):
        gdaltest.post_reason('fail')
        return 'fail'

    # Same but in reverse order
    g1 = ogr.CreateGeometryFromWkt('CURVEPOLYGON (CIRCULARSTRING (0 0,1 -1,2 0,1 1,0 0))')
    if not p1.Within(g1):
        gdaltest.post_reason('fail')
        return 'fail'

    # This is not a circle
    p2 = ogr.CreateGeometryFromWkt('POINT (%.16g %.16g)' % (1 + math.cos(math.pi/6)-1e-2,math.sin(math.pi/6)))
    g1 = ogr.CreateGeometryFromWkt('CURVEPOLYGON (CIRCULARSTRING (0 0,1 1,2 0,1 1,0 0))')
    if p2.Within(g1):
        gdaltest.post_reason('fail')
        return 'fail'

    # Test area on circle in 2 pieces
    g1 = ogr.CreateGeometryFromWkt('CURVEPOLYGON (CIRCULARSTRING (0 0,1 1,2 0,1 -1,0 0))')
    area = g1.Area()
    expected_area = math.pi
    if abs(area - expected_area) > 1e-10:
        gdaltest.post_reason('fail')
        print(area)
        print(expected_area)
        return 'fail'

    # Test area on hippodrome
    g1 = ogr.CreateGeometryFromWkt('CURVEPOLYGON (CIRCULARSTRING (0 0,1 1,2 0,2 -1,2 -2,1 -3,0 -2,0 -1,0 0))')
    area = g1.Area()
    expected_area = math.pi + 2 * 2
    if abs(area - expected_area) > 1e-10:
        gdaltest.post_reason('fail')
        print(area)
        print(expected_area)
        return 'fail'

    # Same hippodrome but with different WKT
    g1 = ogr.CreateGeometryFromWkt('CURVEPOLYGON (COMPOUNDCURVE(CIRCULARSTRING (0 0,1 1,2 0),(2 0,2 -2),CIRCULARSTRING(2 -2,1 -3,0 -2),(0 -2,0 0)))')
    area = g1.Area()
    expected_area = math.pi + 2 * 2
    if abs(area - expected_area) > 1e-10:
        gdaltest.post_reason('fail')
        print(area)
        print(expected_area)
        return 'fail'

    # Similar, but with concave part (does not trigger optimization)
    g1 = ogr.CreateGeometryFromWkt('CURVEPOLYGON (COMPOUNDCURVE(CIRCULARSTRING (0 0,1 1,2 0),(2 0,2 -2),CIRCULARSTRING(2 -2,1 -1,0 -2),(0 -2,0 0)))')
    area = g1.Area()
    expected_area = 2 * 2
    if abs(area - expected_area) > 1e-10:
        gdaltest.post_reason('fail')
        print(area)
        print(expected_area)
        return 'fail'

    # EMPTY
    for in_wkt in [ 'CURVEPOLYGON EMPTY', 'CURVEPOLYGON Z EMPTY' ]:
        g1 = ogr.CreateGeometryFromWkt(in_wkt)
        out_wkt = g1.ExportToWkt()
        if in_wkt != out_wkt:
            gdaltest.post_reason('fail')
            print(in_wkt)
            print(out_wkt)
            return 'fail'
        if g1.Area() != 0.0:
            gdaltest.post_reason('fail')
            print(in_wkt)
            return 'fail'

        g2 = ogr.CreateGeometryFromWkb(g1.ExportToWkb())
        out_wkt = g2.ExportToWkt()
        if in_wkt != out_wkt:
            gdaltest.post_reason('fail')
            print(in_wkt)
            print(out_wkt)
            return 'fail'

    return 'success'

###############################################################################
# Test OGRMultiCurve

def ogr_geom_multicurve():

    # Simple test
    in_wkt = 'MULTICURVE (CIRCULARSTRING (0 0,1 0,0 0))'
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    if g1.GetGeometryType() != ogr.wkbMultiCurve:
        gdaltest.post_reason('fail')
        return 'fail'
    out_wkt = g1.ExportToWkt()
    if in_wkt != out_wkt:
        gdaltest.post_reason('fail')
        print(out_wkt)
        return 'fail'

    if g1.GetDimension() != 1:
        print(g1.GetDimension())
        return 'fail'

    env = g1.GetEnvelope()
    expected_env = (0.0, 1.0, -0.5, 0.5)
    for i in range(4):
        if abs(env[i] - expected_env[i]) > 1e-8:
            gdaltest.post_reason('fail')
            print(env)
            return 'fail'

    length = g1.Length()
    expected_length =  2 * math.pi * 0.5
    if abs(length - expected_length) > 1e-8:
        gdaltest.post_reason('fail')
        print(length)
        return 'fail'

    g2 = ogr.CreateGeometryFromWkb(g1.ExportToWkb())
    out_wkt = g2.ExportToWkt()
    if in_wkt != out_wkt:
        gdaltest.post_reason('fail')
        print(out_wkt)
        return 'fail'

    if not g1.HasCurveGeometry():
        gdaltest.post_reason('fail')
        return 'fail'
    if not g1.HasCurveGeometry(True):
        gdaltest.post_reason('fail')
        return 'fail'

    # MULTICURVE of LINESTRING
    in_wkt = 'MULTICURVE ((0 0,1 0))'
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    if not g1.HasCurveGeometry():
        gdaltest.post_reason('fail')
        return 'fail'
    if g1.HasCurveGeometry(True):
        gdaltest.post_reason('fail')
        return 'fail'

    # Z
    in_wkt = 'MULTICURVE Z (CIRCULARSTRING Z (0 0 10,1 0 10,0 0 10))'
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    if g1.GetGeometryType() != ogr.wkbMultiCurveZ:
        gdaltest.post_reason('fail')
        return 'fail'
    out_wkt = g1.ExportToWkt()
    if in_wkt != out_wkt:
        gdaltest.post_reason('fail')
        print(out_wkt)
        return 'fail'

    g2 = ogr.CreateGeometryFromWkb(g1.ExportToWkb())
    out_wkt = g2.ExportToWkt()
    if in_wkt != out_wkt:
        gdaltest.post_reason('fail')
        print(out_wkt)
        return 'fail'

    # WKT with all possible sub geometries
    in_wkt = 'MULTICURVE (CIRCULARSTRING (0 0,1 0,0 0),(0 0,1 1),COMPOUNDCURVE ((0 0,1 1),CIRCULARSTRING (1 1,2 2,3 3)))'
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    out_wkt = g1.ExportToWkt()
    if in_wkt != out_wkt:
        gdaltest.post_reason('fail')
        print(out_wkt)
        return 'fail'

    g2 = ogr.CreateGeometryFromWkb(g1.ExportToWkb())
    out_wkt = g2.ExportToWkt()
    if in_wkt != out_wkt:
        gdaltest.post_reason('fail')
        print(out_wkt)
        return 'fail'

    # Test ForceToMultiLineString
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', '45')
    g2 = ogr.ForceToMultiLineString(g1)
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', None)
    expected_g = 'MULTILINESTRING ((0 0,0.116977778440514 -0.321393804843282,0.413175911166547 -0.49240387650611,0.75 -0.433012701892224,0.969846310392967 -0.171010071662835,0.969846310392967 0.171010071662835,0.75 0.433012701892224,0.413175911166547 0.49240387650611,0.116977778440514 0.321393804843282,0 0),(0 0,1 1),(0 0,1 1,2 2,3 3))'
    if ogrtest.check_feature_geometry(g2, expected_g) != 0:
        gdaltest.post_reason('fail')
        print(g2)
        return 'fail'

    # Test GetLinearGeometry
    g2 = g1.GetLinearGeometry(45)
    if ogrtest.check_feature_geometry(g2, expected_g) != 0:
        gdaltest.post_reason('fail')
        print(g2)
        return 'fail'

    # Error case : wrong sub-geometry type
    in_wkt = 'MULTILINESTRING (POINT EMPTY)'
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    g = ogr.CreateGeometryFromWkt(in_wkt)
    gdal.PopErrorHandler()
    if g is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Test OGRMultiSurface

def ogr_geom_multisurface():

    # Simple test
    in_wkt = 'MULTISURFACE (CURVEPOLYGON (CIRCULARSTRING (0 0,1 0,0 0)))'
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    if g1.GetGeometryType() != ogr.wkbMultiSurface:
        gdaltest.post_reason('fail')
        return 'fail'
    out_wkt = g1.ExportToWkt()
    if in_wkt != out_wkt:
        gdaltest.post_reason('fail')
        print(out_wkt)
        return 'fail'

    if g1.GetDimension() != 2:
        print(g1.GetDimension())
        return 'fail'

    env = g1.GetEnvelope()
    expected_env = (0.0, 1.0, -0.5, 0.5)
    for i in range(4):
        if abs(env[i] - expected_env[i]) > 1e-8:
            gdaltest.post_reason('fail')
            print(env)
            return 'fail'

    area = g1.Area()
    expected_area =  math.pi * 0.5 * 0.5
    if abs(area - expected_area) > 1e-8:
        gdaltest.post_reason('fail')
        print(area)
        return 'fail'

    g2 = ogr.CreateGeometryFromWkb(g1.ExportToWkb())
    out_wkt = g2.ExportToWkt()
    if in_wkt != out_wkt:
        gdaltest.post_reason('fail')
        print(out_wkt)
        return 'fail'

    if not g1.HasCurveGeometry():
        gdaltest.post_reason('fail')
        return 'fail'
    if not g1.HasCurveGeometry(True):
        gdaltest.post_reason('fail')
        return 'fail'

    # Z
    in_wkt = 'MULTISURFACE Z (CURVEPOLYGON Z (CIRCULARSTRING Z (0 0 10,1 0 10,0 0 10)))'
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    if g1.GetGeometryType() != ogr.wkbMultiSurfaceZ:
        gdaltest.post_reason('fail')
        return 'fail'
    out_wkt = g1.ExportToWkt()
    if in_wkt != out_wkt:
        gdaltest.post_reason('fail')
        print(out_wkt)
        return 'fail'

    g2 = ogr.CreateGeometryFromWkb(g1.ExportToWkb())
    out_wkt = g2.ExportToWkt()
    if in_wkt != out_wkt:
        gdaltest.post_reason('fail')
        print(out_wkt)
        return 'fail'

    # MULTISURFACE of POLYGON
    in_wkt = 'MULTISURFACE (((0 0,0 10,10 10,10 0,0 0)))'
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    if not g1.HasCurveGeometry():
        gdaltest.post_reason('fail')
        return 'fail'
    if g1.HasCurveGeometry(True):
        gdaltest.post_reason('fail')
        return 'fail'

    # WKT with all possible sub geometries
    in_wkt = 'MULTISURFACE (((0 0,0 10,10 10,10 0,0 0)),CURVEPOLYGON (CIRCULARSTRING (0 0,1 0,0 0)))'
    g1 = ogr.CreateGeometryFromWkt(in_wkt)
    out_wkt = g1.ExportToWkt()
    if in_wkt != out_wkt:
        gdaltest.post_reason('fail')
        print(out_wkt)
        return 'fail'

    g2 = ogr.CreateGeometryFromWkb(g1.ExportToWkb())
    out_wkt = g2.ExportToWkt()
    if in_wkt != out_wkt:
        gdaltest.post_reason('fail')
        print(out_wkt)
        return 'fail'

    # Test ForceToMultiPolygon
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', '45')
    g2 = ogr.ForceToMultiPolygon(g1)
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', None)
    expected_g = 'MULTIPOLYGON (((0 0,0 10,10 10,10 0,0 0)),((0 0,0.116977778440514 -0.321393804843282,0.413175911166547 -0.49240387650611,0.75 -0.433012701892224,0.969846310392967 -0.171010071662835,0.969846310392967 0.171010071662835,0.75 0.433012701892224,0.413175911166547 0.49240387650611,0.116977778440514 0.321393804843282,0 0)))'
    if ogrtest.check_feature_geometry(g2, expected_g) != 0:
        gdaltest.post_reason('fail')
        print(g2)
        return 'fail'

    # Test GetLinearGeometry
    g2 = g1.GetLinearGeometry(45)
    if ogrtest.check_feature_geometry(g2, expected_g) != 0:
        gdaltest.post_reason('fail')
        print(g2)
        return 'fail'
    # Check that GetLinearGeometry() is idem-potent on MULTIPOLYGON
    g3 = g2.GetLinearGeometry(45)
    if not g3.Equals(g2):
        gdaltest.post_reason('fail')
        print(g3)
        return 'fail'

    # PointOnSurface
    if ogrtest.have_geos():
        in_wkt = 'MULTISURFACE (((0 0,0 10,10 10,10 0,0 0),(1 1,1 9,9 9,9 1,1 1)),((10 0,10 10,20 10,20 0,10 0),(11 1,11 9,19 9,19 1,11 1)))'
        g1 = ogr.CreateGeometryFromWkt(in_wkt)
        p1 = g1.PointOnSurface()
        if p1.ExportToWkt() != 'POINT (0.5 5.0)':
            gdaltest.post_reason('fail')
            print(p1)
            return 'fail'

    # Error case : wrong sub-geometry type
    in_wkt = 'MULTIPOLYGON (POINT EMPTY)'
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    g = ogr.CreateGeometryFromWkt(in_wkt)
    gdal.PopErrorHandler()
    if g is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Test GetCurveGeometry

def ogr_geom_getcurvegeometry():
    
    for geom in [ 'POINT', 'LINESTRING', 'POLYGON', 'MULTIPOINT', 'MULTILINESTRING',
                  'MULTIPOLYGON', 'GEOMETRYCOLLECTION',
                  'CIRCULARSTRING',
                  'COMPOUNDCURVE',
                  'CURVEPOLYGON',
                  'MULTICURVE',
                  'MULTISURFACE' ]:
        in_wkt = geom + ' EMPTY'
        g = ogr.CreateGeometryFromWkt(in_wkt)
        g2 = g.GetCurveGeometry()
        if g2.ExportToWkt() != in_wkt:
            gdaltest.post_reason('fail')
            print(g)
            print(g2)
            return 'fail'

    g = ogr.CreateGeometryFromWkt('GEOMETRYCOLLECTION(POINT (0 1))')
    g2 = g.GetCurveGeometry()
    if not g.Equals(g2):
        gdaltest.post_reason('fail')
        return 'fail'

    g = ogr.CreateGeometryFromWkt('GEOMETRYCOLLECTION(LINESTRING (0 0,0 1,0 0))')
    g2 = g.GetCurveGeometry()
    if not g.Equals(g2):
        gdaltest.post_reason('fail')
        return 'fail'

    g = ogr.CreateGeometryFromWkt('POLYGON((0 0,0 1,1 1,1 0,0 0))')
    g2 = g.GetCurveGeometry()
    if not g.Equals(g2):
        gdaltest.post_reason('fail')
        return 'fail'

    g = ogr.CreateGeometryFromWkt('POLYGON Z ((0 0 10,0 1 10,1 1 10,1 0 10,0 0 10))')
    g2 = g.GetCurveGeometry()
    if not g.Equals(g2):
        gdaltest.post_reason('fail')
        return 'fail'

    # CircularString with large step
    g1 = ogr.CreateGeometryFromWkt('CIRCULARSTRING (0 0,1 1,2 0)')
    g2 = g1.GetLinearGeometry(15)
    g3 = g2.GetCurveGeometry()
    if g3.GetGeometryType() != ogr.wkbCircularString or \
       g1.GetPoint(0) != g3.GetPoint(0) or g1.GetPoint(2) != g3.GetPoint(2) or \
       abs((g3.GetX(1) - 1)*(g3.GetX(1) - 1)+g3.GetY(1)*g3.GetY(1) - 1) > 1e-8:
        gdaltest.post_reason('fail')
        print(g3)
        return 'fail'

    # CurvePolygon with large step
    g1 = ogr.CreateGeometryFromWkt('CURVEPOLYGON( CIRCULARSTRING (0 0,1 1,0 0))')
    g2 = g1.GetLinearGeometry(15)
    g3 = g2.GetCurveGeometry()
    if not g3.Equals(g1):
        gdaltest.post_reason('fail')
        print(g3)
        return 'fail'

    # This is a straight line
    g1 = ogr.CreateGeometryFromWkt('CIRCULARSTRING (1 2,3 4,5 6)')
    g2 = g1.GetLinearGeometry()
    g3 = g2.GetCurveGeometry()
    if g3.ExportToWkt() != 'LINESTRING (1 2,3 4,5 6)':
        gdaltest.post_reason('fail')
        print(g3)
        return 'fail'

    # "Random" arcs
    for wkt in [ 'CIRCULARSTRING (1 2,3 1,5 6)',
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
        if not g3.Equals(g1):
            gdaltest.post_reason('fail')
            print(g1)
            print(g3)
            return 'fail'

    # Really random arcs with coordinates in the [-1000,1000] range
    for i in range(1000):
        v = [ random.randint(-1000,1000) for i in range(6) ]
        if v[0] != v[4] or v[1] != v[5]:
            wkt = 'CIRCULARSTRING (%d %d,%d %d,%d %d)' % (v[0],v[1],v[2],v[3],v[4],v[5])
            g1 = ogr.CreateGeometryFromWkt(wkt)
            g2 = g1.GetLinearGeometry()
            if g2.GetPointCount() != 3:
                g3 = g2.GetCurveGeometry()
                if not g3.Equals(g1):
                    gdaltest.post_reason('fail')
                    print(g1)
                    print(g3)
                    return 'fail'

    # Really random arcs in random displacements, but with small radius
    for i in range(1000):
        x = random.randint(-1000000,1000000)
        y = random.randint(-1000000,1000000)
        v = [ random.randint(-10,10) for i in range(6) ]
        if v[0] != v[4] or v[1] != v[5]:
            wkt = 'CIRCULARSTRING (%d %d,%d %d,%d %d)' % (x+v[0],y+v[1],x+v[2],y+v[3],x+v[4],y+v[5])
            g1 = ogr.CreateGeometryFromWkt(wkt)
            g2 = g1.GetLinearGeometry()
            if g2.GetPointCount() != 3:
                g3 = g2.GetCurveGeometry()
                if not g3.Equals(g1):
                    gdaltest.post_reason('fail')
                    print(g1)
                    print(g3)
                    return 'fail'

    # Really random arcs with coordinates in the [-1000000,1000000] range
    for i in range(1000):
        v = [ random.randint(-1000000,1000000) for i in range(6) ]
        if v[0] != v[4] or v[1] != v[5]:
            wkt = 'CIRCULARSTRING (%d %d,%d %d,%d %d)' % (v[0],v[1],v[2],v[3],v[4],v[5])
            g1 = ogr.CreateGeometryFromWkt(wkt)
            g2 = g1.GetLinearGeometry()
            if g2.GetPointCount() != 3:
                g3 = g2.GetCurveGeometry()
                if not g3.Equals(g1):
                    gdaltest.post_reason('fail')
                    print(g1)
                    print(g3)
                    return 'fail'

    # 5 points full circle
    g1 = ogr.CreateGeometryFromWkt('CIRCULARSTRING (0 0,0.5 -0.5,1 0,0.5 0.5,0 0)')
    g2 = g1.GetLinearGeometry()
    g3 = g2.GetCurveGeometry()
    if not g3.Equals(g1):
        gdaltest.post_reason('fail')
        print(g3)
        return 'fail'

    # 3 points full circle
    g1 = ogr.CreateGeometryFromWkt('CIRCULARSTRING (0 0,1 0,0 0)')
    g2 = g1.GetLinearGeometry()
    g3 = g2.GetCurveGeometry()
    if g3.GetGeometryType() != ogr.wkbCircularString or \
       g1.GetPoint(0) != g3.GetPoint(0) or \
       g1.GetPoint(1) != g3.GetPoint(2) or \
       g1.GetPoint(2) != g3.GetPoint(4) or \
       abs((g3.GetX(1) - 0.5)*(g3.GetX(1) - 0.5)+g3.GetY(1)*g3.GetY(1) - 0.5*0.5) > 1e-12 or \
       abs((g3.GetX(3) - 0.5)*(g3.GetX(3) - 0.5)+g3.GetY(3)*g3.GetY(3) - 0.5*0.5) > 1e-12 :
        gdaltest.post_reason('fail')
        print(g3)
        #print(abs((g3.GetX(1) - 0.5)*(g3.GetX(1) - 0.5)+g3.GetY(1)*g3.GetY(1) - 0.5*0.5))
        #print(abs((g3.GetX(3) - 0.5)*(g3.GetX(3) - 0.5)+g3.GetY(3)*g3.GetY(3) - 0.5*0.5))
        return 'fail'

    # 3 points full circle in a CurvePolygon
    for wkt in [ 'CURVEPOLYGON( CIRCULARSTRING (0 0,1 0,0 0))',
                 'CURVEPOLYGON( CIRCULARSTRING (0 0,0 1,0 0))',
                 'CURVEPOLYGON( CIRCULARSTRING (0 0,-1 0,0 0))',
                 'CURVEPOLYGON( CIRCULARSTRING (0 0,0 -1,0 0))']:
        g1 = ogr.CreateGeometryFromWkt(wkt)
        g2 = g1.GetLinearGeometry()
        g3 = g2.GetCurveGeometry()
        if not g3.Equals(g1):
            gdaltest.post_reason('fail')
            print(g1)
            print(g3)
            return 'fail'

    # 2 curves in the CircularString
    g1 = ogr.CreateGeometryFromWkt('CIRCULARSTRING (0 0,1 1,2 0,3 -1,4 0)')
    g2 = g1.GetLinearGeometry()
    g3 = g2.GetCurveGeometry()
    if not g3.Equals(g1):
        gdaltest.post_reason('fail')
        print(g3)
        return 'fail'

    # 3 curves in the CircularString
    g1 = ogr.CreateGeometryFromWkt('CIRCULARSTRING (0 0,1 1,2 0,3 -1,4 0,5 1,6 0)')
    g2 = g1.GetLinearGeometry()
    g3 = g2.GetCurveGeometry()
    if not g3.Equals(g1):
        gdaltest.post_reason('fail')
        print(g3)
        return 'fail'

    # CircularString, LineString, CircularString
    g1 = ogr.CreateGeometryFromWkt('COMPOUNDCURVE (CIRCULARSTRING (0 0,1 1,2 0),(2 0,3 0,4 0),CIRCULARSTRING (4 0,5 1,6 0))')
    g2 = g1.GetLinearGeometry()
    g3 = g2.GetCurveGeometry()
    if not g3.Equals(g1):
        gdaltest.post_reason('fail')
        print(g3)
        return 'fail'

    # LineString, CircularString, LineString, CircularString, LineString
    g1 = ogr.CreateGeometryFromWkt('COMPOUNDCURVE ((-1 0,-0.5 0.5,0 0),CIRCULARSTRING (0 0,1 1,2 0),(2 0,3 0,4 0),CIRCULARSTRING (4 0,5 1,6 0),(6 0,7 0))')
    g2 = g1.GetLinearGeometry()
    g3 = g2.GetCurveGeometry()
    if not g3.Equals(g1):
        gdaltest.post_reason('fail')
        print(g3)
        return 'fail'

    # Check with default discretization method
    g1 = ogr.CreateGeometryFromWkt('CIRCULARSTRING (0 0,1.2 1.0,2 0)')
    g2 = ogr.CreateGeometryFromWkt('LINESTRING (0 0,0.000997093138961 0.068920262501281,0.006738548124866 0.137608197923873,0.017197098230267 0.205737600590167,0.032323074784779 0.272984917348872,0.052044643056421 0.339030784160355,0.076268143401735 0.403561542787492,0.104878536064531 0.466270730389102,0.137739947510866 0.526860534941749,0.174696315705653 0.585043209577972,0.215572131266425 0.640542439124111,0.260173270974444 0.693094652347904,0.30828791968472 0.742450273683876,0.359687576256715 0.788374908491942,0.414128138728451 0.830650456220319,0.471351063580441 0.869076146186235,0.531084593583979 0.903469491055406,0.593045048402615 0.933667153492107,0.656938171817632 0.959525721864054,0.722460529179411 0.980922391318168,0.789300948448056 0.99775554699276,0.857141997979641 1.009945246596371,0.925661494039921 1.017433600061489,0.994534030886182 1.020185044470095,1.063432526150724 1.01818651294542,1.132029774186796 1.01144749670781,1.2 1.0,1.267547127648721 0.983752320094182,1.333803428245673 0.962854851656094,1.398449236893265 0.937408418110045,1.461172658788815 0.907535790143504,1.521671074014578 0.873381093378847,1.579652597579492 0.835109113014538,1.634837487668428 0.792904498790658,1.686959495304612 0.746970874114522,1.73576714891352 0.697529853644598,1.781024967590663 0.644819974072535,1.822514597219645 0.589095543261942,1.860035863959079 0.53062541329644,1.89340774001566 0.469691683356621,1.922469217043826 0.406588338684124,1.94708008295817 0.341619832199361,1.967121598410718 0.27509961561613,1.982497069669296 0.207348627140011,1.993132315133044 0.138693743046887,1.998976023234287 0.069466200612231,2 0)')
    g3 = g2.GetCurveGeometry()
    if not g3.Equals(g1):
        gdaltest.post_reason('fail')
        print(g3)
        return 'fail'

    # Check with alternate discretization method : ROUND_ANGLE_METHOD
    g1 = ogr.CreateGeometryFromWkt('CIRCULARSTRING (0 0,1.2 1.0,2 0)')
    g2 = ogr.CreateGeometryFromWkt('LINESTRING (0 0,-0.000199980003999 0.02,0.002236456877416 0.089770423644023,0.009533897446083 0.159200932797352,0.021656789305088 0.227953268998521,0.038546070943884 0.295692477776518,0.060119459480029 0.362088540515299,0.086271851533127 0.426817982271309,0.116875835277923 0.489565447710894,0.151782311181914 0.550025237489786,0.190821218403283 0.607902797589577,0.233802363310224 0.662916154355295,0.280516346085201 0.7147972882427,0.330735580899807 0.763293439582565,0.384215404690045 0.80816834000038,0.440695269130182 0.849203363492117,0.499900009998002 0.886198591548151,0.561541187747236 0.918973787136141,0.62531849275604 0.947369272797732,0.690921208405283 0.971246708581086,0.758029724858684 0.990489766019218,0.826317096169807 1.005004694870594,0.895450633129846 1.014720779860886,0.965093524096011 1.019590685200679,1.034906475903993 1.019590685200679,1.104549366870158 1.014720779860886,1.173682903830197 1.005004694870593,1.2 1.0,1.241970275141317 0.990489766019217,1.309078791594718 0.971246708581085,1.374681507243961 0.947369272797731,1.438458812252765 0.91897378713614,1.500099990002 0.886198591548151,1.559304730869819 0.849203363492116,1.615784595309956 0.808168340000379,1.669264419100194 0.763293439582565,1.7194836539148 0.714797288242699,1.766197636689777 0.662916154355294,1.809178781596718 0.607902797589576,1.848217688818087 0.550025237489785,1.883124164722078 0.489565447710893,1.913728148466874 0.426817982271308,1.939880540519971 0.362088540515298,1.961453929056117 0.295692477776516,1.978343210694912 0.227953268998519,1.990466102553917 0.15920093279735,1.997763543122584 0.089770423644022,2.000199980003999 0.02,2 0)')
    g3 = g2.GetCurveGeometry()
    if not g3.Equals(g1):
        gdaltest.post_reason('fail')
        print(g3)
        return 'fail'

    # Check with PostgreSQL output of SELECT ST_CurveToLine(ST_GeomFromText('CIRCULARSTRING (0 0,1.2 1.0,2 0)'))
    g1 = ogr.CreateGeometryFromWkt('CIRCULARSTRING (0 0,1.2 1.0,2 0)')
    g2 = ogr.CreateGeometryFromWkt('LINESTRING (0 0,0.000223190308279 0.049091765203314,0.002854930521212 0.098113445796116,0.007888880546112 0.146946944256066,0.015312913156447 0.195474616408063,0.025109143207391 0.243579554839372,0.037253970722702 0.291145870539817,0.051718137749134 0.338058972088558,0.068466798841411 0.384205841714863,0.08745960500795 0.429475307567812,0.108650800915124 0.473758311539029,0.131989335115863 0.516948171993215,0.157418983037062 0.558940840773549,0.184878482429505 0.599635153862819,0.214301680953989 0.638933075096389,0.245617695548099 0.676739932339917,0.27875108318972 0.712964645562815,0.31362202264588 0.747519946258017,0.350146506769097 0.780322587679462,0.388236544877951 0.811293545390794,0.427800374734344 0.840358207642151,0.46874268360677 0.867446555116406,0.510964837887032 0.892493329611833,0.554365120707246 0.915438191254836,0.598838976984681 0.936225863863983,0.644279265304116 0.954806268115175,0.690576516030889 0.971134642187118,0.737619195032841 0.985171649596477,0.785293972375802 0.996883473962907,0.833485995345338 1.006241900475673,0.88207916513699 1.013224383865605,0.930956416548473 1.017814102718623,0.98 1.02,1.029091765203309 1.019776809691721,1.078113445796111 1.017145069478788,1.12694694425606 1.012111119453889,1.175474616408058 1.004687086843554,1.223579554839367 0.994890856792611,1.271145870539812 0.9827460292773,1.318058972088553 0.968281862250867,1.364205841714858 0.951533201158591,1.409475307567807 0.932540394992052,1.453758311539024 0.911349199084878,1.49694817199321 0.88801066488414,1.538940840773545 0.862581016962941,1.579635153862814 0.835121517570498,1.618933075096384 0.805698319046015,1.656739932339913 0.774382304451905,1.692964645562811 0.741248916810284,1.727519946258013 0.706377977354124,1.760322587679458 0.669853493230907,1.791293545390791 0.631763455122053,1.820358207642148 0.59219962526566,1.847446555116403 0.551257316393235,1.872493329611831 0.509035162112973,1.895438191254833 0.465634879292759,1.916225863863981 0.421161023015324,1.934806268115173 0.37572073469589,1.951134642187117 0.329423483969116,1.965171649596476 0.282380804967164,1.976883473962906 0.234706027624203,1.986241900475672 0.186514004654668,1.993224383865604 0.137920834863015,1.997814102718623 0.089043583451532,2.0 0.04,2 0)')
    g3 = g2.GetCurveGeometry()
    g1_expected = ogr.CreateGeometryFromWkt('CIRCULARSTRING (0 0,0.98 1.02,2 0)')
    if not g3.Equals(g1_expected):
        gdaltest.post_reason('fail')
        print(g3)
        return 'fail'
        
    # Test default ( implicit option ADD_INTERMEDIATE_POINT=STEALTH )
    g1 = ogr.CreateGeometryFromWkt('CIRCULARSTRING (0 0,1.2 1.0,2 0)')
    g2 = g1.GetLinearGeometry()
    g3 = g2.GetCurveGeometry()
    if not g3.Equals(g1):
        gdaltest.post_reason('fail')
        print(g3)
        return 'fail'

    # Test with Z
    g1 = ogr.CreateGeometryFromWkt('CIRCULARSTRING (0 0 1,1 1 2,2 0 3)')
    g2 = g1.GetLinearGeometry()
    g3 = g2.GetCurveGeometry()
    if not g3.Equals(g1):
        gdaltest.post_reason('fail')
        print(g3)
        return 'fail'

    g1 = ogr.CreateGeometryFromWkt('CIRCULARSTRING (2 0 3,1 1 2,0 0 1)')
    g2 = g1.GetLinearGeometry()
    g3 = g2.GetCurveGeometry()
    if not g3.Equals(g1):
        gdaltest.post_reason('fail')
        print(g3)
        return 'fail'

    # Test option ADD_INTERMEDIATE_POINT=STEALTH
    g1 = ogr.CreateGeometryFromWkt('CIRCULARSTRING (0 0,1.2 1.0,2 0)')
    g2 = g1.GetLinearGeometry(options = ['ADD_INTERMEDIATE_POINT=STEALTH'])
    g3 = g2.GetCurveGeometry()
    if not g3.Equals(g1):
        gdaltest.post_reason('fail')
        print(g3)
        return 'fail'
        
    # Test option ADD_INTERMEDIATE_POINT=YES
    g1 = ogr.CreateGeometryFromWkt('CIRCULARSTRING (0 0,1.2 1.0,2 0)')
    g2 = g1.GetLinearGeometry(options = ['ADD_INTERMEDIATE_POINT=YES'])
    g3 = g2.GetCurveGeometry()
    if not g3.Equals(g1):
        gdaltest.post_reason('fail')
        print(g3)
        return 'fail'
        
    # Test with big coordinates. The points are (2,49),(3,50),(4,49) reprojected from EPSG:4326 to EPSG:32631
    g1 = ogr.CreateGeometryFromWkt('CIRCULARSTRING (426857.987717275 5427937.52346616,500000.000000001 5538630.70286887,573142.012282726 5427937.52346616)')
    g2 = g1.GetLinearGeometry()
    g3 = g2.GetCurveGeometry()
    if ogrtest.check_feature_geometry(g3, g1) != 0:
        gdaltest.post_reason('fail')
        print(g3)
        return 'fail'

    # Same with integer coordinates
    g1 = ogr.CreateGeometryFromWkt('CIRCULARSTRING (426858 5427938,500000 5538632,573142 5427938)')
    g2 = g1.GetLinearGeometry()
    g3 = g2.GetCurveGeometry()
    if not g3.Equals(g1):
        gdaltest.post_reason('fail')
        print(g3)
        return 'fail'

    # Test option ADD_INTERMEDIATE_POINT=FALSE
    g1 = ogr.CreateGeometryFromWkt('CIRCULARSTRING (0 0,1.2 1.0,2 0)')
    g2 = g1.GetLinearGeometry(options = ['ADD_INTERMEDIATE_POINT=FALSE'])
    g3 = g2.GetCurveGeometry()
    g1_expected = ogr.CreateGeometryFromWkt('CIRCULARSTRING (0 0,1.0 1.020199980003999,2 0)')
    if ogrtest.check_feature_geometry(g3, g1_expected) != 0:
        gdaltest.post_reason('fail')
        print(g3)
        return 'fail'

    # Test with unrecognized options
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    g2_new = g1.GetLinearGeometry(options = ['bla', 'ADD_INTERMEDIATE_POINT=FALSE', 'foo=bar'])
    gdal.PopErrorHandler()
    if not g2_new.Equals(g2):
        gdaltest.post_reason('fail')
        print(g2_new)
        return 'fail'

    # Add repeated point at end of line
    g2 = g1.GetLinearGeometry()
    g2.AddPoint_2D(2-1e-9,0)
    g3 = g2.GetCurveGeometry()
    if not g3.Equals(g1):
        gdaltest.post_reason('fail')
        print(g3)
        return 'fail'

    # Add repeated point at end of line
    g2 = g1.GetLinearGeometry()
    g2_new = ogr.Geometry(ogr.wkbLineString)
    g2_new.AddPoint_2D(0,1e-9)
    for i in range(g2.GetPointCount()):
        g2_new.AddPoint_2D(g2.GetX(i), g2.GetY(i))
    g3 = g2_new.GetCurveGeometry()
    if not g3.Equals(g1):
        gdaltest.post_reason('fail')
        print(g3)
        return 'fail'

    # Test various configurations
    for (wkt,eps) in [  ('CIRCULARSTRING (0 0,0.5 0.5,0 1,-0.5 0.5,0 0)', 0),
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
                        ('CIRCULARSTRING (760112.098000001162291 207740.096999999135733,760116.642489952617325 207741.101843414857285,760120.967999998480082 207742.820000000298023,760123.571822694852017 207744.275888498465065,760126.011999998241663 207745.991999998688698,760127.330062366439961 207747.037052432337077,760128.585999999195337 207748.155999999493361)',1e-5)]:
        g1 = ogr.CreateGeometryFromWkt(wkt)
        g2 = g1.GetLinearGeometry()
        g3 = g2.GetCurveGeometry()
        if (eps == 0 and not g3.Equals(g1)) or (eps > 0 and ogrtest.check_feature_geometry(g3, g1, eps) != 0):
            gdaltest.post_reason('fail')
            print('')
            print(g1)
            print(g3)
            print(ogrtest.check_feature_geometry(g3, g1, 1e-6))
            return 'fail'


    # Test with GEOMETRYCOLLECTION container
    g1 = ogr.CreateGeometryFromWkt('GEOMETRYCOLLECTION(CIRCULARSTRING (0 0,0.5 0.5,0 1,-0.5 0.5,0 0))')
    g2 = g1.GetLinearGeometry()
    g3 = g2.GetCurveGeometry()
    if not g3.Equals(g1):
        gdaltest.post_reason('fail')
        print(g3)
        return 'fail'

    # Test with MULTICURVE container
    g1 = ogr.CreateGeometryFromWkt('MULTICURVE(CIRCULARSTRING (0 0,0.5 0.5,0 1,-0.5 0.5,0 0))')
    g2 = g1.GetLinearGeometry()
    g3 = g2.GetCurveGeometry()
    if not g3.Equals(g1):
        gdaltest.post_reason('fail')
        print(g3)
        return 'fail'

    # Test with MULTISURFACE container
    g1 = ogr.CreateGeometryFromWkt('MULTISURFACE(CURVEPOLYGON(COMPOUNDCURVE(CIRCULARSTRING (0 0,0.5 0.5,1 0),(1 0,0 0))))')
    g2 = g1.GetLinearGeometry()
    g3 = g2.GetCurveGeometry()
    if not g3.Equals(g1):
        gdaltest.post_reason('fail')
        print(g3)
        return 'fail'

    if ogrtest.have_geos():
        g1 = ogr.CreateGeometryFromWkt('CURVEPOLYGON(CIRCULARSTRING (0 0,2 0,0 0))')
        g2 = ogr.CreateGeometryFromWkt('CURVEPOLYGON(CIRCULARSTRING (1 0,3 1,1 0))')
        g3 = g1.Intersection(g2)
        if g3.GetGeometryType() != ogr.wkbCurvePolygon:
            gdaltest.post_reason('fail')
            print(g3)
            return 'fail'

        g1 = ogr.CreateGeometryFromWkt('CURVEPOLYGON(CIRCULARSTRING (0 0,2 0,0 0))')
        g2 = ogr.CreateGeometryFromWkt('CURVEPOLYGON((1 -1,1 1,3 1,3 -1,1 -1))')
        g3 = g1.Intersection(g2)
        if g3.GetGeometryType() != ogr.wkbCurvePolygon:
            gdaltest.post_reason('fail')
            print(g3)
            return 'fail'

        g1 = ogr.CreateGeometryFromWkt('CURVEPOLYGON(CIRCULARSTRING (0 0,2 0,0 0))')
        g2 = ogr.CreateGeometryFromWkt('CURVEPOLYGON(CIRCULARSTRING (3 0,5 0,3 0))')
        g3 = g1.Union(g2)
        if g3.ExportToWkt() != 'MULTISURFACE (CURVEPOLYGON (CIRCULARSTRING (0 0,2 0,0 0)),CURVEPOLYGON (CIRCULARSTRING (3 0,5 0,3 0)))':
            gdaltest.post_reason('fail')
            print(g3)
            return 'fail'

        g1 = ogr.CreateGeometryFromWkt('CIRCULARSTRING (0 0,1 1,2 0))')
        g2 = ogr.CreateGeometryFromWkt('CIRCULARSTRING (2 0,1 -1,0 0))')
        g3 = g1.Union(g2)
        if g3.ExportToWkt() != 'MULTICURVE (CIRCULARSTRING (0 0,1 1,2 0),CIRCULARSTRING (2 0,1 -1,0 0))':
            gdaltest.post_reason('fail')
            print(g3)
            return 'fail'

        g1 = ogr.CreateGeometryFromWkt('POINT(1 2)')
        g1 = g1.Buffer(0.5)
        g1 = g1.GetCurveGeometry()
        if g1.ExportToWkt() != 'CURVEPOLYGON (CIRCULARSTRING (1.5 2.0,0.5 2.0,1.5 2.0))':
            gdaltest.post_reason('fail')
            print(g1)
            return 'fail'

    return 'success'

###############################################################################
# Test OGR_GT_ functions

def ogr_geom_gt_functions():

    # GT_HasZ
    tuples = [ (ogr.wkbPoint, 0),
               (ogr.wkbPoint25D, 1),
               (ogr.wkbCircularString, 0),
               (ogr.wkbCircularStringZ, 1) ]
    for (gt, res) in tuples:
        if ogr.GT_HasZ(gt) != res:
            gdaltest.post_reason('fail')
            print(gt)
            return 'fail'

    # GT_SetZ
    tuples = [ (ogr.wkbPoint, ogr.wkbPoint25D),
               (ogr.wkbPoint25D, ogr.wkbPoint25D),
               (ogr.wkbCircularString, ogr.wkbCircularStringZ),
               (ogr.wkbCircularStringZ, ogr.wkbCircularStringZ) ]
    for (gt, res) in tuples:
        if ogr.GT_SetZ(gt) != res:
            gdaltest.post_reason('fail')
            print(gt)
            return 'fail'

    # OGR_GT_SetModifier
    tuples = [ (ogr.wkbPoint, 1, ogr.wkbPoint25D),
               (ogr.wkbPoint25D, 1, ogr.wkbPoint25D),
               (ogr.wkbCircularString, 1, ogr.wkbCircularStringZ),
               (ogr.wkbCircularStringZ, 1, ogr.wkbCircularStringZ),
               (ogr.wkbPoint, 0, ogr.wkbPoint),
               (ogr.wkbPoint25D, 0, ogr.wkbPoint),
               (ogr.wkbCircularString, 0, ogr.wkbCircularString),
               (ogr.wkbCircularStringZ, 0, ogr.wkbCircularString)]
    for (gt, mod, res) in tuples:
        if ogr.GT_SetModifier(gt, mod) != res:
            gdaltest.post_reason('fail')
            print(gt)
            return 'fail'

    # GT_Flatten
    tuples = [ (ogr.wkbPoint, ogr.wkbPoint),
               (ogr.wkbPoint25D, ogr.wkbPoint),
               (ogr.wkbCircularString, ogr.wkbCircularString),
               (ogr.wkbCircularStringZ, ogr.wkbCircularString)]
    for (gt, res) in tuples:
        if ogr.GT_Flatten(gt) != res:
            gdaltest.post_reason('fail')
            print(gt)
            return 'fail'

    # GT_IsSubClassOf
    tuples = [ (ogr.wkbPoint, ogr.wkbPoint, 1),
               (ogr.wkbPoint25D, ogr.wkbPoint, 1),
               (ogr.wkbPoint, ogr.wkbUnknown, 1),
               (ogr.wkbPoint, ogr.wkbLineString, 0),
               (ogr.wkbPolygon, ogr.wkbCurvePolygon, 1),
               (ogr.wkbMultiSurface, ogr.wkbGeometryCollection, 1),
               (ogr.wkbMultiPolygon, ogr.wkbMultiSurface, 1),
               (ogr.wkbMultiLineString, ogr.wkbMultiCurve, 1),
               (ogr.wkbUnknown, ogr.wkbUnknown, 1),
               (ogr.wkbUnknown, ogr.wkbPoint, 0),
               ]
    for (gt, gt2, res) in tuples:
        if ogr.GT_IsSubClassOf(gt, gt2) != res:
            gdaltest.post_reason('fail')
            print(gt)
            print(gt2)
            return 'fail'

    # GT_IsCurve
    tuples = [ (ogr.wkbPoint, 0),
               (ogr.wkbCircularString, 1),
               (ogr.wkbCircularStringZ, 1),
               (ogr.wkbLineString, 1),
               (ogr.wkbCompoundCurve, 1),
               (ogr.wkbCurvePolygon, 0) ]
    for (gt, res) in tuples:
        if ogr.GT_IsCurve(gt) != res:
            gdaltest.post_reason('fail')
            print(gt)
            return 'fail'

    # GT_IsSurface
    tuples = [ (ogr.wkbPoint, 0),
               (ogr.wkbCircularString, 0),
               (ogr.wkbCurvePolygon, 1),
               (ogr.wkbPolygon, 1) ]
    for (gt, res) in tuples:
        if ogr.GT_IsSurface(gt) != res:
            gdaltest.post_reason('fail')
            print(gt)
            return 'fail'

    # GT_GetCollection
    tuples = [ (ogr.wkbPoint, ogr.wkbMultiPoint),
               (ogr.wkbCircularString, ogr.wkbMultiCurve),
               (ogr.wkbCompoundCurve, ogr.wkbMultiCurve),
               (ogr.wkbCurvePolygon, ogr.wkbMultiSurface),
               (ogr.wkbLineString, ogr.wkbMultiLineString),
               (ogr.wkbPolygon, ogr.wkbMultiPolygon) ]
    for (gt, res) in tuples:
        if ogr.GT_GetCollection(gt) != res:
            gdaltest.post_reason('fail')
            print(gt)
            return 'fail'

    # GT_IsNonLinear
    tuples = [ (ogr.wkbPoint, 0),
               (ogr.wkbCircularString, 1),
               (ogr.wkbCompoundCurve, 1),
               (ogr.wkbCurvePolygon, 1),
               (ogr.wkbMultiCurve, 1),
               (ogr.wkbMultiSurface, 1),
               (ogr.wkbLineString, 0),
               (ogr.wkbPolygon, 0) ]
    for (gt, res) in tuples:
        if ogr.GT_IsNonLinear(gt) != res:
            gdaltest.post_reason('fail')
            print(gt)
            return 'fail'

    # GT_GetCurve
    tuples = [ (ogr.wkbPoint, ogr.wkbPoint),
               (ogr.wkbCircularString, ogr.wkbCircularString),
               (ogr.wkbCompoundCurve, ogr.wkbCompoundCurve),
               (ogr.wkbCurvePolygon, ogr.wkbCurvePolygon),
               (ogr.wkbLineString, ogr.wkbCompoundCurve),
               (ogr.wkbPolygon, ogr.wkbCurvePolygon),
               (ogr.wkbMultiLineString, ogr.wkbMultiCurve),
               (ogr.wkbMultiPolygon, ogr.wkbMultiSurface),
               (ogr.wkbMultiCurve, ogr.wkbMultiCurve),
               (ogr.wkbMultiSurface, ogr.wkbMultiSurface) ]
    for (gt, res) in tuples:
        if ogr.GT_GetCurve(gt) != res:
            gdaltest.post_reason('fail')
            print(gt)
            return 'fail'

    # GT_GetLinear
    tuples = [ (ogr.wkbPoint, ogr.wkbPoint),
               (ogr.wkbCircularString, ogr.wkbLineString),
               (ogr.wkbCompoundCurve, ogr.wkbLineString),
               (ogr.wkbCurvePolygon, ogr.wkbPolygon),
               (ogr.wkbLineString, ogr.wkbLineString),
               (ogr.wkbPolygon, ogr.wkbPolygon),
               (ogr.wkbMultiLineString, ogr.wkbMultiLineString),
               (ogr.wkbMultiPolygon, ogr.wkbMultiPolygon),
               (ogr.wkbMultiCurve, ogr.wkbMultiLineString),
               (ogr.wkbMultiSurface, ogr.wkbMultiPolygon) ]
    for (gt, res) in tuples:
        if ogr.GT_GetLinear(gt) != res:
            gdaltest.post_reason('fail')
            print(gt)
            return 'fail'

    return 'success'

###############################################################################
# cleanup

def ogr_geom_cleanup():
    return 'success'

gdaltest_list = [ 
    ogr_geom_area,
    ogr_geom_area_linearring,
    ogr_geom_area_linearring_big_offset,
    ogr_geom_area_geometrycollection,
    ogr_geom_is_empty,
    ogr_geom_pickle,
    ogr_geom_boundary_point,
    ogr_geom_boundary_multipoint,
    ogr_geom_boundary_linestring,
    ogr_geom_boundary_polygon,
    ogr_geom_build_from_edges_1,
    ogr_geom_build_from_edges_2,
    ogr_geom_build_from_edges_3,
    ogr_geom_build_from_edges_4,
    ogr_geom_area_empty_linearring,
    ogr_geom_transform_to,
    ogr_geom_transform,
    ogr_geom_closerings,
    ogr_geom_segmentize,
    ogr_geom_value,
    ogr_geom_flattenTo2D,
    ogr_geom_linestring_limits,
    ogr_geom_coord_round,
    ogr_geom_coord_round_2,
    ogr_geom_area_point,
    ogr_geom_length_point,
    ogr_geom_length_multilinestring,
    ogr_geom_length_geometrycollection,
    ogr_geom_empty,
    ogr_geom_getpoints,
    ogr_geom_mixed_coordinate_dimension,
    ogr_geom_getenvelope3d,
    ogr_geom_z_empty,
    ogr_geom_getlineargeometry,
    ogr_geom_getdimension,
    ogr_geom_circularstring,
    ogr_geom_compoundcurve,
    ogr_geom_curvepolygon,
    ogr_geom_multicurve,
    ogr_geom_multisurface,
    ogr_geom_getcurvegeometry,
    ogr_geom_gt_functions,
    ogr_geom_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_geom' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

