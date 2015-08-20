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
# Copyright (c) 2012-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

import sys

sys.path.append( '../pymod' )

import gdaltest
import ogrtest

from osgeo import ogr

###############################################################################
# Common usage tests.

ds = None
A = None
B = None
C = None
pointInB = None
D1 = None
D2 = None

def recreate_layer_C():
    global C
    
    ds.DeleteLayer( 'C' )
    C = ds.CreateLayer('C')

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

def algebra_setup():

    global ds, A, B, C, pointInB, D1, D2
    
    if not ogrtest.have_geos():
        return 'skip'
    
    # Create three memory layers for intersection.

    ds = ogr.GetDriverByName('Memory').CreateDataSource( 'wrk' )

    A = ds.CreateLayer( 'A' )
    A.CreateField( ogr.FieldDefn("A", ogr.OFTInteger) )

    B = ds.CreateLayer( 'B' )
    B.CreateField( ogr.FieldDefn("B", ogr.OFTString) )

    pointInB = ds.CreateLayer( 'pointInB' )

    C = ds.CreateLayer( 'C' )

    # Add polygons.
    
    a1 = 'POLYGON((1 2, 1 3, 3 3, 3 2, 1 2))'
    a2 = 'POLYGON((5 2, 5 3, 7 3, 7 2, 5 2))'
    b1 = 'POLYGON((2 1, 2 4, 6 4, 6 1, 2 1))'
    pointInB1 = 'POINT(3 3)'
    
    feat = ogr.Feature( A.GetLayerDefn() )
    feat.SetField('A',1)
    feat.SetGeometryDirectly( ogr.Geometry(wkt = a1) )
    A.CreateFeature( feat )

    feat = ogr.Feature( A.GetLayerDefn() )
    feat.SetField('A',2)
    feat.SetGeometryDirectly( ogr.Geometry(wkt = a2) )
    A.CreateFeature( feat )

    feat = ogr.Feature( B.GetLayerDefn() )
    feat.SetField('B','first')
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

    return 'success'


def algebra_intersection():
    if not ogrtest.have_geos():
        return 'skip'
    
    recreate_layer_C()
    
    # Intersection; this should return two rectangles

    err = A.Intersection( B, C )

    if err != 0:
        gdaltest.post_reason( 'got non-zero result code '+str(err)+' from Layer.Intersection' )
        return 'fail'

    C_defn = C.GetLayerDefn()
    if C_defn.GetFieldCount() != 2 \
       or C_defn.GetFieldDefn(0).GetName() != 'A' \
       or C_defn.GetFieldDefn(0).GetType() != ogr.OFTInteger \
       or C_defn.GetFieldDefn(1).GetName() != 'B' \
       or C_defn.GetFieldDefn(1).GetType() != ogr.OFTString:
        gdaltest.post_reason( 'Did not get expected output schema.' )
        return 'fail'
        
    if C.GetFeatureCount() != 2:
        gdaltest.post_reason( 'Layer.Intersection returned '+str(C.GetFeatureCount())+' features' )
        return 'fail'

    f1 = (ogr.Geometry(wkt = 'POLYGON ((2 3,3 3,3 2,2 2,2 3))'),1,'first')
    f2 = (ogr.Geometry(wkt = 'POLYGON ((5 2,5 3,6 3,6 2,5 2))'),2,'first')

    C.ResetReading();
    while 1:
        feat = C.GetNextFeature()
        if not feat: break

        g = feat.GetGeometryRef()
        if g.Equals(f1[0]):
            if feat.GetField('A') != f1[1] or feat.GetField('B') != f1[2]:
                gdaltest.post_reason('Did not get expected field values.')
                return 'fail'
        elif g.Equals(f2[0]):
            if feat.GetField('A') != f2[1] or feat.GetField('B') != f2[2]:
                gdaltest.post_reason('Did not get expected field values.')
                return 'fail'
        else:
            gdaltest.post_reason( 'Layer.Intersection returned wrong geometry: '+g.ExportToWkt() )
            return 'fail'

    # This time we test with PROMOTE_TO_MULTI and pre-created output fields.
    recreate_layer_C()
    C.CreateField( ogr.FieldDefn("A", ogr.OFTInteger) )
    C.CreateField( ogr.FieldDefn("B", ogr.OFTString) )

    err = A.Intersection( B, C, options = ['PROMOTE_TO_MULTI=YES'] )

    if err != 0:
        gdaltest.post_reason( 'got non-zero result code '+str(err)+' from Layer.Intersection' )
        return 'fail'

    if C.GetFeatureCount() != 2:
        gdaltest.post_reason( 'Layer.Intersection returned '+str(C.GetFeatureCount())+' features' )
        return 'fail'

    f1 = (ogr.Geometry(wkt='MULTIPOLYGON (((2 3,3 3,3 2,2 2,2 3)))'),1,'first')
    f2 = (ogr.Geometry(wkt='MULTIPOLYGON (((5 2,5 3,6 3,6 2,5 2)))'),2,'first')

    C.ResetReading();
    while 1:
        feat = C.GetNextFeature()
        if not feat: break

        g = feat.GetGeometryRef()
        if g.Equals(f1[0]):
            if feat.GetField('A') != f1[1] or feat.GetField('B') != f1[2]:
                gdaltest.post_reason('Did not get expected field values. (1)')
                print(feat.GetField('A'))
                print(feat.GetField('B'))
                return 'fail'
        elif g.Equals(f2[0]):
            if feat.GetField('A') != f2[1] or feat.GetField('B') != f2[2]:
                gdaltest.post_reason('Did not get expected field values. (2)')
                print(feat.GetField('A'))
                print(feat.GetField('B'))
                print(feat.GetField('B'))
                return 'fail'
        else:
            gdaltest.post_reason( 'Layer.Intersection returned wrong geometry: '+g.ExportToWkt() )
            return 'fail'

    recreate_layer_C()

    # Intersection with self ; this should return 2 polygons

    err = D1.Intersection( D2, C )

    if err != 0:
        gdaltest.post_reason( 'got non-zero result code '+str(err)+' from Layer.Intersection' )
        return 'fail'
 
    if not is_same(D1, C):
        gdaltest.post_reason( 'D1 != C' )
        return 'fail'

    return 'success'


def algebra_union():
    if not ogrtest.have_geos():
        return 'skip'
    
    recreate_layer_C()

    # Union; this should return 5 polygons

    err = A.Union( B, C )

    if ogrtest.have_geos():
        if err != 0:
            gdaltest.post_reason( 'got non-zero result code '+str(err)+' from Layer.Union' )
            return 'fail'

        if C.GetFeatureCount() != 5:
            gdaltest.post_reason( 'Layer.Union returned '+str(C.GetFeatureCount())+' features' )
            return 'fail'

    recreate_layer_C()

    err = A.Union( B, C, options = ['PROMOTE_TO_MULTI=YES'] )

    if ogrtest.have_geos():
        if err != 0:
            gdaltest.post_reason( 'got non-zero result code '+str(err)+' from Layer.Union' )
            return 'fail'

        if C.GetFeatureCount() != 5:
            gdaltest.post_reason( 'Layer.Union returned '+str(C.GetFeatureCount())+' features' )
            return 'fail'

    recreate_layer_C()

    # Union with self ; this should return 2 polygons

    err = D1.Union( D2, C )

    if ogrtest.have_geos():
        if err != 0:
            gdaltest.post_reason( 'got non-zero result code '+str(err)+' from Layer.Union' )
            return 'fail'
 
        if not is_same(D1, C):
            gdaltest.post_reason( 'D1 != C' )
            return 'fail'

    recreate_layer_C()
    
    # Union of a polygon and a point within : should return the point and the polygon (#4772)

    err = B.Union( pointInB, C )

    if ogrtest.have_geos():
        if err != 0:
            gdaltest.post_reason( 'got non-zero result code '+str(err)+' from Layer.Union' )
            return 'fail'
 
        if C.GetFeatureCount() != 2:
            gdaltest.post_reason( 'Layer.Union returned '+str(C.GetFeatureCount())+' features' )
            return 'fail'

    return 'success'


def algebra_symdifference():
    if not ogrtest.have_geos():
        return 'skip'
    
    recreate_layer_C()

    # SymDifference; this should return 3 polygons

    err = A.SymDifference( B, C )

    if ogrtest.have_geos():
        if err != 0:
            gdaltest.post_reason( 'got non-zero result code '+str(err)+' from Layer.SymDifference' )
            return 'fail'

        if C.GetFeatureCount() != 3:
            gdaltest.post_reason( 'Layer.SymDifference returned '+str(C.GetFeatureCount())+' features' )
            return 'fail'

    recreate_layer_C()

    err = A.SymDifference( B, C, options = ['PROMOTE_TO_MULTI=YES'] )

    if ogrtest.have_geos():
        if err != 0:
            gdaltest.post_reason( 'got non-zero result code '+str(err)+' from Layer.SymDifference' )
            return 'fail'

        if C.GetFeatureCount() != 3:
            gdaltest.post_reason( 'Layer.SymDifference returned '+str(C.GetFeatureCount())+' features' )
            return 'fail'

    recreate_layer_C()

    # SymDifference with self ; this should return 0 features

    err = D1.SymDifference( D2, C )

    if ogrtest.have_geos():
        if err != 0:
            gdaltest.post_reason( 'got non-zero result code '+str(err)+' from Layer.SymDifference' )
            return 'fail'
 
    if C.GetFeatureCount() != 0:
            gdaltest.post_reason( 'Layer.SymDifference returned '+str(C.GetFeatureCount())+' features' )
            return 'fail'

    return 'success'


def algebra_identify():
    if not ogrtest.have_geos():
        return 'skip'
    
    recreate_layer_C()

    # Identity; this should return 4 polygons

    err = A.Identity( B, C )

    if ogrtest.have_geos():
        if err != 0:
            gdaltest.post_reason( 'got non-zero result code '+str(err)+' from Layer.Identity' )
            return 'fail'

        if C.GetFeatureCount() != 4:
            gdaltest.post_reason( 'Layer.Identity returned '+str(C.GetFeatureCount())+' features' )
            return 'fail'

    recreate_layer_C()

    err = A.Identity( B, C, options = ['PROMOTE_TO_MULTI=YES'] )

    if ogrtest.have_geos():
        if err != 0:
            gdaltest.post_reason( 'got non-zero result code '+str(err)+' from Layer.Identity' )
            return 'fail'

        if C.GetFeatureCount() != 4:
            gdaltest.post_reason( 'Layer.Identity returned '+str(C.GetFeatureCount())+' features' )
            return 'fail'

    recreate_layer_C()

    # Identity with self ; this should return 2 polygons

    err = D1.Identity( D2, C )

    if ogrtest.have_geos():
        if err != 0:
            gdaltest.post_reason( 'got non-zero result code '+str(err)+' from Layer.Identity' )
            return 'fail'
 
        if not is_same(D1, C):
            gdaltest.post_reason( 'D1 != C' )
            return 'fail'

    return 'success'

def algebra_update():
    if not ogrtest.have_geos():
        return 'skip'

    recreate_layer_C()

    # Update; this should return 3 polygons

    err = A.Update( B, C )

    if ogrtest.have_geos():
        if err != 0:
            gdaltest.post_reason( 'got non-zero result code '+str(err)+' from Layer.Update' )
            return 'fail'

        if C.GetFeatureCount() != 3:
            gdaltest.post_reason( 'Layer.Update returned '+str(C.GetFeatureCount())+' features' )
            return 'fail'

    recreate_layer_C()

    err = A.Update( B, C, options = ['PROMOTE_TO_MULTI=YES'] )

    if ogrtest.have_geos():
        if err != 0:
            gdaltest.post_reason( 'got non-zero result code '+str(err)+' from Layer.Update' )
            return 'fail'

        if C.GetFeatureCount() != 3:
            gdaltest.post_reason( 'Layer.Update returned '+str(C.GetFeatureCount())+' features' )
            return 'fail'

    recreate_layer_C()

    # Update with self ; this should return 2 polygons

    err = D1.Update( D2, C )

    if ogrtest.have_geos():
        if err != 0:
            gdaltest.post_reason( 'got non-zero result code '+str(err)+' from Layer.Update' )
            return 'fail'
 
        if not is_same(D1, C):
            gdaltest.post_reason( 'D1 != C' )
            return 'fail'

    return 'success'


def algebra_clip():
    if not ogrtest.have_geos():
        return 'skip'

    recreate_layer_C()

    # Clip; this should return 2 polygons

    err = A.Clip( B, C )

    if ogrtest.have_geos():
        if err != 0:
            gdaltest.post_reason( 'got non-zero result code '+str(err)+' from Layer.Clip' )
            return 'fail'

        if C.GetFeatureCount() != 2:
            gdaltest.post_reason( 'Layer.Clip returned '+str(C.GetFeatureCount())+' features' )
            return 'fail'

    recreate_layer_C()

    err = A.Clip( B, C, options = ['PROMOTE_TO_MULTI=YES'] )

    if ogrtest.have_geos():
        if err != 0:
            gdaltest.post_reason( 'got non-zero result code '+str(err)+' from Layer.Clip' )
            return 'fail'

        if C.GetFeatureCount() != 2:
            gdaltest.post_reason( 'Layer.Clip returned '+str(C.GetFeatureCount())+' features' )
            return 'fail'

    recreate_layer_C()

    # Clip with self ; this should return 2 polygons

    err = D1.Update( D2, C )

    if ogrtest.have_geos():
        if err != 0:
            gdaltest.post_reason( 'got non-zero result code '+str(err)+' from Layer.Clip' )
            return 'fail'
 
        if not is_same(D1, C):
            gdaltest.post_reason( 'D1 != C' )
            return 'fail'

    return 'success'


def algebra_erase():
    if not ogrtest.have_geos():
        return 'skip'

    recreate_layer_C()

    # Erase; this should return 2 polygons

    err = A.Erase( B, C )

    if ogrtest.have_geos():
        if err != 0:
            gdaltest.post_reason( 'got non-zero result code '+str(err)+' from Layer.Erase' )
            return 'fail'

        if C.GetFeatureCount() != 2:
            gdaltest.post_reason( 'Layer.Erase returned '+str(C.GetFeatureCount())+' features' )
            return 'fail'

    recreate_layer_C()

    err = A.Erase( B, C, options = ['PROMOTE_TO_MULTI=YES'] )

    if ogrtest.have_geos():
        if err != 0:
            gdaltest.post_reason( 'got non-zero result code '+str(err)+' from Layer.Erase' )
            return 'fail'

        if C.GetFeatureCount() != 2:
            gdaltest.post_reason( 'Layer.Erase returned '+str(C.GetFeatureCount())+' features' )
            return 'fail'

    recreate_layer_C()

    # Erase with self ; this should return 0 features

    err = D1.Erase( D2, C )

    if ogrtest.have_geos():
        if err != 0:
            gdaltest.post_reason( 'got non-zero result code '+str(err)+' from Layer.Erase' )
            return 'fail'
 
    if C.GetFeatureCount() != 0:
            gdaltest.post_reason( 'Layer.Erase returned '+str(C.GetFeatureCount())+' features' )
            return 'fail'

    recreate_layer_C()

    return 'success'

def algebra_cleanup():
    if not ogrtest.have_geos():
        return 'skip'

    global ds, A, B, C, pointInB, D1, D2

    D2 = None
    D1 = None
    pointInB = None
    C = None
    B = None
    A = None
    ds = None

    return 'success'


gdaltest_list = [
    algebra_setup,
    algebra_intersection,
    algebra_union,
    algebra_symdifference,
    algebra_identify,
    algebra_update,
    algebra_clip,
    algebra_erase,
    algebra_cleanup,
    ]

if __name__ == '__main__':

    gdaltest.setup_run( 'algebra' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()
