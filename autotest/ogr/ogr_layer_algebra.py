#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test topological overlay methods in Layer class.
# Author:   Ari Jolma <ari.jolma@aalto.fi>
# 
###############################################################################
# Copyright (c) 2012, Ari Jolma <ari.jolma@aalto.fi>
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

import gdaltest
import ogrtest

try:
    from osgeo import gdal, gdalconst, ogr, osr
except:
    import gdal
    import gdalconst
    import ogr
    import osr

###############################################################################
# Common usage tests.

def empty_layer(C):
    C.ResetReading();
    while 1:
        feat = C.GetNextFeature()
        if not feat: break
        C.DeleteFeature(feat.GetFID())

def is_same(A,B):
    A.ResetReading()
    B.ResetReading()
    while True:
        fA = A.GetNextFeature()
        fB = B.GetNextFeature()
        if fA is None:
            if fB is not None:
                return False
            return True
        if fB is None:
            return False
        if fA.Equal(fB) != 0:
            return False

def overlay_1():

    try:
        gdaltest.have_ng = 1
    except:
        gdaltest.have_ng = 0
        return 'skip'

    # Create three memory layers for intersection.

    ds = ogr.GetDriverByName('Memory').CreateDataSource( 'wrk' )
    A = ds.CreateLayer( 'A' )
    B = ds.CreateLayer( 'B' )
    pointInB = ds.CreateLayer( 'pointInB' )
    C = ds.CreateLayer( 'C' )

    # Add polygons.
    
    a1 = 'POLYGON((1 2, 1 3, 3 3, 3 2, 1 2))'
    a2 = 'POLYGON((5 2, 5 3, 7 3, 7 2, 5 2))'
    b1 = 'POLYGON((2 1, 2 4, 6 4, 6 1, 2 1))'
    pointInB1 = 'POINT(3 3)'
    
    feat = ogr.Feature( A.GetLayerDefn() )
    feat.SetGeometryDirectly( ogr.Geometry(wkt = a1) )
    A.CreateFeature( feat )

    feat = ogr.Feature( A.GetLayerDefn() )
    feat.SetGeometryDirectly( ogr.Geometry(wkt = a2) )
    A.CreateFeature( feat )

    feat = ogr.Feature( B.GetLayerDefn() )
    feat.SetGeometryDirectly( ogr.Geometry(wkt = b1) )
    B.CreateFeature( feat )

    feat = ogr.Feature( pointInB.GetLayerDefn() )
    feat.SetGeometryDirectly( ogr.Geometry(wkt = pointInB1) )
    pointInB.CreateFeature( feat )

    d1 = 'POLYGON((1 2, 1 3, 3 3, 3 2, 1 2))'
    d2 = 'POLYGON((3 2, 3 3, 4 3, 4 2, 3 2))'

    D1 = ds.CreateLayer( 'D1' )

    feat = ogr.Feature( D1.GetLayerDefn() )
    feat.SetGeometryDirectly( ogr.Geometry(wkt = d1) )
    D1.CreateFeature( feat )

    feat = ogr.Feature( D1.GetLayerDefn() )
    feat.SetGeometryDirectly( ogr.Geometry(wkt = d2) )
    D1.CreateFeature( feat )
    
    D2 = ds.CreateLayer( 'D2' )
    
    feat = ogr.Feature( D2.GetLayerDefn() )
    feat.SetGeometryDirectly( ogr.Geometry(wkt = d1) )
    D2.CreateFeature( feat )

    feat = ogr.Feature( D2.GetLayerDefn() )
    feat.SetGeometryDirectly( ogr.Geometry(wkt = d2) )
    D2.CreateFeature( feat )
    
    # Run the methods and check results.

    # Intersection; this should return two rectangles

    err = A.Intersection( B, C )

    if ogrtest.have_geos():
        if err != 0:
            gdaltest.post_reason( 'got non-zero result code '+str(err)+' from Layer.Intersection' )
            return 'fail'

        if C.GetFeatureCount() != 2:
            gdaltest.post_reason( 'Layer.Intersection returned '+str(C.GetFeatureCount())+' features' )
            return 'fail'

        i1 = ogr.Geometry(wkt = 'POLYGON ((2 3,3 3,3 2,2 2,2 3))')
        i2 = ogr.Geometry(wkt = 'POLYGON ((5 2,5 3,6 3,6 2,5 2))')

        C.ResetReading();
        while 1:
            feat = C.GetNextFeature()
            if not feat: break
            g = feat.GetGeometryRef()
            if not(g.Equals(i1) or g.Equals(i2)):
                gdaltest.post_reason( 'Layer.Intersection returned wrong geometry: '+g.ExportToWkt() )
                return 'fail'

    empty_layer(C)

    err = A.Intersection( B, C, options = ['PROMOTE_TO_MULTI=YES'] )

    if ogrtest.have_geos():
        if err != 0:
            gdaltest.post_reason( 'got non-zero result code '+str(err)+' from Layer.Intersection' )
            return 'fail'

        if C.GetFeatureCount() != 2:
            gdaltest.post_reason( 'Layer.Intersection returned '+str(C.GetFeatureCount())+' features' )
            return 'fail'

        i1 = ogr.Geometry(wkt = 'MULTIPOLYGON (((2 3,3 3,3 2,2 2,2 3)))')
        i2 = ogr.Geometry(wkt = 'MULTIPOLYGON (((5 2,5 3,6 3,6 2,5 2)))')

        C.ResetReading();
        while 1:
            feat = C.GetNextFeature()
            if not feat: break
            g = feat.GetGeometryRef()
            if not(g.Equals(i1) or g.Equals(i2)):
                gdaltest.post_reason( 'Layer.Intersection returned wrong geometry: '+g.ExportToWkt() )
                return 'fail'

    empty_layer(C)

    # Intersection with self ; this should return 2 polygons

    err = D1.Intersection( D2, C )

    if ogrtest.have_geos():
        if err != 0:
            gdaltest.post_reason( 'got non-zero result code '+str(err)+' from Layer.Intersection' )
            return 'fail'
 
        if not is_same(D1, C):
            gdaltest.post_reason( 'D1 != C' )
            return 'fail'

    empty_layer(C)

    # Union; this should return 5 polygons

    err = A.Union( B, C )

    if ogrtest.have_geos():
        if err != 0:
            gdaltest.post_reason( 'got non-zero result code '+str(err)+' from Layer.Union' )
            return 'fail'

        if C.GetFeatureCount() != 5:
            gdaltest.post_reason( 'Layer.Union returned '+str(C.GetFeatureCount())+' features' )
            return 'fail'

    empty_layer(C)

    err = A.Union( B, C, options = ['PROMOTE_TO_MULTI=YES'] )

    if ogrtest.have_geos():
        if err != 0:
            gdaltest.post_reason( 'got non-zero result code '+str(err)+' from Layer.Union' )
            return 'fail'

        if C.GetFeatureCount() != 5:
            gdaltest.post_reason( 'Layer.Union returned '+str(C.GetFeatureCount())+' features' )
            return 'fail'

    empty_layer(C)

    # Union with self ; this should return 2 polygons

    err = D1.Union( D2, C )

    if ogrtest.have_geos():
        if err != 0:
            gdaltest.post_reason( 'got non-zero result code '+str(err)+' from Layer.Union' )
            return 'fail'
 
        if not is_same(D1, C):
            gdaltest.post_reason( 'D1 != C' )
            return 'fail'

    empty_layer(C)
    
    # Union of a polygon and a point within : should return the point and the polygon (#4772)

    err = B.Union( pointInB, C )

    if ogrtest.have_geos():
        if err != 0:
            gdaltest.post_reason( 'got non-zero result code '+str(err)+' from Layer.Union' )
            return 'fail'
 
        if C.GetFeatureCount() != 2:
            gdaltest.post_reason( 'Layer.Union returned '+str(C.GetFeatureCount())+' features' )
            return 'fail'

    empty_layer(C)

    # SymDifference; this should return 3 polygons

    err = A.SymDifference( B, C )

    if ogrtest.have_geos():
        if err != 0:
            gdaltest.post_reason( 'got non-zero result code '+str(err)+' from Layer.SymDifference' )
            return 'fail'

        if C.GetFeatureCount() != 3:
            gdaltest.post_reason( 'Layer.SymDifference returned '+str(C.GetFeatureCount())+' features' )
            return 'fail'

    empty_layer(C)

    err = A.SymDifference( B, C, options = ['PROMOTE_TO_MULTI=YES'] )

    if ogrtest.have_geos():
        if err != 0:
            gdaltest.post_reason( 'got non-zero result code '+str(err)+' from Layer.SymDifference' )
            return 'fail'

        if C.GetFeatureCount() != 3:
            gdaltest.post_reason( 'Layer.SymDifference returned '+str(C.GetFeatureCount())+' features' )
            return 'fail'

    empty_layer(C)

    # SymDifference with self ; this should return 0 features

    err = D1.SymDifference( D2, C )

    if ogrtest.have_geos():
        if err != 0:
            gdaltest.post_reason( 'got non-zero result code '+str(err)+' from Layer.SymDifference' )
            return 'fail'
 
    if C.GetFeatureCount() != 0:
            gdaltest.post_reason( 'Layer.SymDifference returned '+str(C.GetFeatureCount())+' features' )
            return 'fail'

    empty_layer(C)

    # Identity; this should return 4 polygons

    err = A.Identity( B, C )

    if ogrtest.have_geos():
        if err != 0:
            gdaltest.post_reason( 'got non-zero result code '+str(err)+' from Layer.Identity' )
            return 'fail'

        if C.GetFeatureCount() != 4:
            gdaltest.post_reason( 'Layer.Identity returned '+str(C.GetFeatureCount())+' features' )
            return 'fail'

    empty_layer(C)

    err = A.Identity( B, C, options = ['PROMOTE_TO_MULTI=YES'] )

    if ogrtest.have_geos():
        if err != 0:
            gdaltest.post_reason( 'got non-zero result code '+str(err)+' from Layer.Identity' )
            return 'fail'

        if C.GetFeatureCount() != 4:
            gdaltest.post_reason( 'Layer.Identity returned '+str(C.GetFeatureCount())+' features' )
            return 'fail'

    empty_layer(C)

    # Identity with self ; this should return 2 polygons

    err = D1.Identity( D2, C )

    if ogrtest.have_geos():
        if err != 0:
            gdaltest.post_reason( 'got non-zero result code '+str(err)+' from Layer.Identity' )
            return 'fail'
 
        if not is_same(D1, C):
            gdaltest.post_reason( 'D1 != C' )
            return 'fail'

    empty_layer(C)

    # Update; this should return 3 polygons

    err = A.Update( B, C )

    if ogrtest.have_geos():
        if err != 0:
            gdaltest.post_reason( 'got non-zero result code '+str(err)+' from Layer.Update' )
            return 'fail'

        if C.GetFeatureCount() != 3:
            gdaltest.post_reason( 'Layer.Update returned '+str(C.GetFeatureCount())+' features' )
            return 'fail'

    empty_layer(C)

    err = A.Update( B, C, options = ['PROMOTE_TO_MULTI=YES'] )

    if ogrtest.have_geos():
        if err != 0:
            gdaltest.post_reason( 'got non-zero result code '+str(err)+' from Layer.Update' )
            return 'fail'

        if C.GetFeatureCount() != 3:
            gdaltest.post_reason( 'Layer.Update returned '+str(C.GetFeatureCount())+' features' )
            return 'fail'

    empty_layer(C)

    # Update with self ; this should return 2 polygons

    err = D1.Update( D2, C )

    if ogrtest.have_geos():
        if err != 0:
            gdaltest.post_reason( 'got non-zero result code '+str(err)+' from Layer.Update' )
            return 'fail'
 
        if not is_same(D1, C):
            gdaltest.post_reason( 'D1 != C' )
            return 'fail'

    empty_layer(C)

    # Clip; this should return 2 polygons

    err = A.Clip( B, C )

    if ogrtest.have_geos():
        if err != 0:
            gdaltest.post_reason( 'got non-zero result code '+str(err)+' from Layer.Clip' )
            return 'fail'

        if C.GetFeatureCount() != 2:
            gdaltest.post_reason( 'Layer.Clip returned '+str(C.GetFeatureCount())+' features' )
            return 'fail'

    empty_layer(C)

    err = A.Clip( B, C, options = ['PROMOTE_TO_MULTI=YES'] )

    if ogrtest.have_geos():
        if err != 0:
            gdaltest.post_reason( 'got non-zero result code '+str(err)+' from Layer.Clip' )
            return 'fail'

        if C.GetFeatureCount() != 2:
            gdaltest.post_reason( 'Layer.Clip returned '+str(C.GetFeatureCount())+' features' )
            return 'fail'

    empty_layer(C)

    # Clip with self ; this should return 2 polygons

    err = D1.Update( D2, C )

    if ogrtest.have_geos():
        if err != 0:
            gdaltest.post_reason( 'got non-zero result code '+str(err)+' from Layer.Clip' )
            return 'fail'
 
        if not is_same(D1, C):
            gdaltest.post_reason( 'D1 != C' )
            return 'fail'

    empty_layer(C)

    # Erase; this should return 2 polygons

    err = A.Erase( B, C )

    if ogrtest.have_geos():
        if err != 0:
            gdaltest.post_reason( 'got non-zero result code '+str(err)+' from Layer.Erase' )
            return 'fail'

        if C.GetFeatureCount() != 2:
            gdaltest.post_reason( 'Layer.Erase returned '+str(C.GetFeatureCount())+' features' )
            return 'fail'

    empty_layer(C)

    err = A.Erase( B, C, options = ['PROMOTE_TO_MULTI=YES'] )

    if ogrtest.have_geos():
        if err != 0:
            gdaltest.post_reason( 'got non-zero result code '+str(err)+' from Layer.Erase' )
            return 'fail'

        if C.GetFeatureCount() != 2:
            gdaltest.post_reason( 'Layer.Erase returned '+str(C.GetFeatureCount())+' features' )
            return 'fail'

    empty_layer(C)

    # Erase with self ; this should return 0 features

    err = D1.Erase( D2, C )

    if ogrtest.have_geos():
        if err != 0:
            gdaltest.post_reason( 'got non-zero result code '+str(err)+' from Layer.Erase' )
            return 'fail'
 
    if C.GetFeatureCount() != 0:
            gdaltest.post_reason( 'Layer.Erase returned '+str(C.GetFeatureCount())+' features' )
            return 'fail'

    empty_layer(C)

    return 'success'


gdaltest_list = [
    overlay_1
    ]

if __name__ == '__main__':

    gdaltest.setup_run( 'overlay' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

    #C.ResetReading();
    #while 1:
    #    feat = C.GetNextFeature()
    #    if not feat: break
    #    g = feat.GetGeometryRef()
    #    print(g.ExportToWkt())
