#!/usr/bin/env python
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

def overlay_1():

    try:
        gdaltest.have_ng = 1
    except:
        gdaltest.have_ng = 0
        return 'skip'

    # Create three memory layers for intersection.

    ds = ogr.GetDriverByName('Memory').CreateDataSource( 'wrk' )
    A = ds.CreateLayer( 'poly' )
    B = ds.CreateLayer( 'poly' )
    C = ds.CreateLayer( 'poly' )

    # Add polygons.
    
    a1 = 'POLYGON((1 2, 1 3, 3 3, 3 2, 1 2))'
    a2 = 'POLYGON((5 2, 5 3, 7 3, 7 2, 5 2))'
    b1 = 'POLYGON((2 1, 2 4, 6 4, 6 1, 2 1))'
    
    feat = ogr.Feature( A.GetLayerDefn() )
    feat.SetGeometryDirectly( ogr.Geometry(wkt = a1) )
    A.CreateFeature( feat )

    feat = ogr.Feature( A.GetLayerDefn() )
    feat.SetGeometryDirectly( ogr.Geometry(wkt = a2) )
    A.CreateFeature( feat )

    feat = ogr.Feature( B.GetLayerDefn() )
    feat.SetGeometryDirectly( ogr.Geometry(wkt = b1) )
    B.CreateFeature( feat )

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

    # Erase; this should return 2 polygons

    err = A.Erase( B, C )

    if ogrtest.have_geos():
        if err != 0:
            gdaltest.post_reason( 'got non-zero result code '+str(err)+' from Layer.Erase' )
            return 'fail'

        if C.GetFeatureCount() != 2:
            gdaltest.post_reason( 'Layer.Erase returned '+str(C.GetFeatureCount())+' features' )
            return 'fail'

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
