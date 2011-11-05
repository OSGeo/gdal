#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test Misc. OGRGeometry operations.
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

sys.path.append( '../pymod' )

import gdaltest
import ogrtest
import ogr
import osr
import gdal

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


def ogr_geom_empty():
    try:
        ogr.Geometry.IsEmpty
    except:
        return 'skip'

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
        gdaltest.post_reason ("A geometry could not be created from wkt: %s"%wkt)
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
    try:
        geom.Segmentize
    except:
        return 'skip'

    geom.Segmentize(1.00001)

    if geom.ExportToWkt() != 'LINESTRING (0 0,0 1,0 2,0 3,0 4,0 5,0 6,0 7,0 8,0 9,0 10)':
        print(geom.ExportToWkt())
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
    if abs(length-2) > 0.00000000001:
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
# cleanup

def ogr_geom_cleanup():
    return 'success'

gdaltest_list = [ 
    ogr_geom_area,
    ogr_geom_area_linearring,
    ogr_geom_area_linearring_big_offset,
    ogr_geom_area_geometrycollection,
    ogr_geom_empty,
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
    ogr_geom_flattenTo2D,
    ogr_geom_linestring_limits,
    ogr_geom_coord_round,
    ogr_geom_area_point,
    ogr_geom_length_point,
    ogr_geom_length_multilinestring,
    ogr_geom_length_geometrycollection,
    ogr_geom_empty,
    ogr_geom_getpoints,
    ogr_geom_mixed_coordinate_dimension,
    ogr_geom_getenvelope3d,
    ogr_geom_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_geom' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

