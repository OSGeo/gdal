#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test topological overlay methods in Layer class.
# Author:   Ari Jolma <ari.jolma@aalto.fi>
#
###############################################################################
# Copyright (c) 2012, Ari Jolma <ari.jolma@aalto.fi>
# Copyright (c) 2012-2013, Even Rouault <even dot rouault at spatialys.com>
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



import ogrtest

from osgeo import ogr
import pytest

###############################################################################
@pytest.fixture(autouse=True, scope='module')
def startup_and_cleanup():

    if not ogrtest.have_geos():
        pytest.skip()

###############################################################################
# Common usage tests.

ds = None
A = None
B = None
C = None
pointInB = None
D1 = None
D2 = None
empty = None


def recreate_layer_C():
    global C

    ds.DeleteLayer('C')
    C = ds.CreateLayer('C')


def print_layer(A):
    A.ResetReading()
    while True:
        f = A.GetNextFeature()
        if f is None:
            return
        g = f.GetGeometryRef()
        print(g.ExportToWkt())


def is_same(A, B):

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


def test_algebra_setup():

    global ds, A, B, C, pointInB, D1, D2, empty

    # Create three memory layers for intersection.

    ds = ogr.GetDriverByName('Memory').CreateDataSource('wrk')

    A = ds.CreateLayer('A')
    A.CreateField(ogr.FieldDefn("A", ogr.OFTInteger))

    B = ds.CreateLayer('B')
    B.CreateField(ogr.FieldDefn("B", ogr.OFTString))

    pointInB = ds.CreateLayer('pointInB')

    C = ds.CreateLayer('C')

    # Add polygons.

    a1 = 'POLYGON((1 2, 1 3, 3 3, 3 2, 1 2))'
    a2 = 'POLYGON((5 2, 5 3, 7 3, 7 2, 5 2))'
    b1 = 'POLYGON((2 1, 2 4, 6 4, 6 1, 2 1))'
    pointInB1 = 'POINT(3 3)'

    feat = ogr.Feature(A.GetLayerDefn())
    feat.SetField('A', 1)
    feat.SetGeometryDirectly(ogr.Geometry(wkt=a1))
    A.CreateFeature(feat)

    feat = ogr.Feature(A.GetLayerDefn())
    feat.SetField('A', 2)
    feat.SetGeometryDirectly(ogr.Geometry(wkt=a2))
    A.CreateFeature(feat)

    feat = ogr.Feature(B.GetLayerDefn())
    feat.SetField('B', 'first')
    feat.SetGeometryDirectly(ogr.Geometry(wkt=b1))
    B.CreateFeature(feat)

    feat = ogr.Feature(pointInB.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.Geometry(wkt=pointInB1))
    pointInB.CreateFeature(feat)

    d1 = 'POLYGON((1 2, 1 3, 3 3, 3 2, 1 2))'
    d2 = 'POLYGON((3 2, 3 3, 4 3, 4 2, 3 2))'

    D1 = ds.CreateLayer('D1')

    feat = ogr.Feature(D1.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.Geometry(wkt=d1))
    D1.CreateFeature(feat)

    feat = ogr.Feature(D1.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.Geometry(wkt=d2))
    D1.CreateFeature(feat)

    D2 = ds.CreateLayer('D2')

    feat = ogr.Feature(D2.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.Geometry(wkt=d1))
    D2.CreateFeature(feat)

    feat = ogr.Feature(D2.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.Geometry(wkt=d2))
    D2.CreateFeature(feat)

    empty = ds.CreateLayer('empty')


def test_algebra_intersection():

    recreate_layer_C()

    # Intersection; this should return two rectangles

    err = A.Intersection(B, C)

    assert err == 0, \
        ('got non-zero result code ' + str(err) + ' from Layer.Intersection')

    C_defn = C.GetLayerDefn()
    assert C_defn.GetFieldCount() == 2 and C_defn.GetFieldDefn(0).GetName() == 'A' and C_defn.GetFieldDefn(0).GetType() == ogr.OFTInteger and C_defn.GetFieldDefn(1).GetName() == 'B' and C_defn.GetFieldDefn(1).GetType() == ogr.OFTString, \
        'Did not get expected output schema.'

    assert C.GetFeatureCount() == 2, \
        ('Layer.Intersection returned ' + str(C.GetFeatureCount()) + ' features')

    f1 = (ogr.Geometry(wkt='POLYGON ((2 3,3 3,3 2,2 2,2 3))'), 1, 'first')
    f2 = (ogr.Geometry(wkt='POLYGON ((5 2,5 3,6 3,6 2,5 2))'), 2, 'first')

    C.ResetReading()
    while 1:
        feat = C.GetNextFeature()
        if not feat:
            break

        g = feat.GetGeometryRef()
        if ogrtest.check_feature_geometry(g, f1[0]) == 0:
            assert feat.GetField('A') == f1[1] and feat.GetField('B') == f1[2], \
                'Did not get expected field values.'
        elif ogrtest.check_feature_geometry(g, f2[0]) == 0:
            assert feat.GetField('A') == f2[1] and feat.GetField('B') == f2[2], \
                'Did not get expected field values.'
        else:
            pytest.fail('Layer.Intersection returned wrong geometry: ' + g.ExportToWkt())

    # This time we test with PROMOTE_TO_MULTI and pre-created output fields.
    recreate_layer_C()
    C.CreateField(ogr.FieldDefn("A", ogr.OFTInteger))
    C.CreateField(ogr.FieldDefn("B", ogr.OFTString))

    err = A.Intersection(B, C, options=['PROMOTE_TO_MULTI=YES'])

    assert err == 0, \
        ('got non-zero result code ' + str(err) + ' from Layer.Intersection')

    assert C.GetFeatureCount() == 2, \
        ('Layer.Intersection returned ' + str(C.GetFeatureCount()) + ' features')

    f1 = (ogr.Geometry(wkt='MULTIPOLYGON (((2 3,3 3,3 2,2 2,2 3)))'), 1, 'first')
    f2 = (ogr.Geometry(wkt='MULTIPOLYGON (((5 2,5 3,6 3,6 2,5 2)))'), 2, 'first')

    C.ResetReading()
    while 1:
        feat = C.GetNextFeature()
        if not feat:
            break

        g = feat.GetGeometryRef()
        if ogrtest.check_feature_geometry(g, f1[0]) == 0:
            assert feat.GetField('A') == f1[1] and feat.GetField('B') == f1[2], \
                'Did not get expected field values. (1)'
        elif ogrtest.check_feature_geometry(g, f2[0]) == 0:
            assert feat.GetField('A') == f2[1] and feat.GetField('B') == f2[2], \
                'Did not get expected field values. (2)'
        else:
            pytest.fail('Layer.Intersection returned wrong geometry: ' + g.ExportToWkt())

    recreate_layer_C()

    # Intersection with self ; this should return 2 polygons

    err = D1.Intersection(D2, C, ['KEEP_LOWER_DIMENSION_GEOMETRIES=NO'])

    assert err == 0, \
        ('got non-zero result code ' + str(err) + ' from Layer.Intersection')

    assert is_same(D1, C), 'D1 != C'


def test_algebra_KEEP_LOWER_DIMENSION_GEOMETRIES():

    driver = ogr.GetDriverByName('MEMORY')
    ds = driver.CreateDataSource('ds')
    layer1 = ds.CreateLayer('layer1')
    layer2 = ds.CreateLayer('layer2')

    g1 = "POLYGON (( 140 360, 140 480, 220 480, 220 360, 140 360 ))"
    geom1 = ogr.CreateGeometryFromWkt(g1)
    feat1 = ogr.Feature(layer1.GetLayerDefn())
    feat1.SetGeometry(geom1)
    layer1.CreateFeature(feat1)

    g2 = "POLYGON (( 220 260, 220 360, 300 360, 300 260, 220 260 ))"
    geom2 = ogr.CreateGeometryFromWkt(g2)
    feat2 = ogr.Feature(layer2.GetLayerDefn())
    feat2.SetGeometry(geom2)
    layer2.CreateFeature(feat2)

    g1 = "LINESTRING (0 0, 1 0)"
    geom1 = ogr.CreateGeometryFromWkt(g1)
    feat1 = ogr.Feature(layer1.GetLayerDefn())
    feat1.SetGeometry(geom1)
    layer1.CreateFeature(feat1)

    g2 = "LINESTRING (1 0, 2 0)"
    geom2 = ogr.CreateGeometryFromWkt(g2)
    feat2 = ogr.Feature(layer2.GetLayerDefn())
    feat2.SetGeometry(geom2)
    layer2.CreateFeature(feat2)

    layer3 = ds.CreateLayer('layer3a')
    layer1.Intersection(layer2, layer3, ['KEEP_LOWER_DIMENSION_GEOMETRIES=NO'])
    assert layer3.GetFeatureCount() == 0, \
        'Lower dimension geometries not removed in intersection'

    layer3 = ds.CreateLayer('layer3b')
    layer1.Intersection(layer2, layer3, ['KEEP_LOWER_DIMENSION_GEOMETRIES=YES'])
    assert layer3.GetFeatureCount() == 2, \
        'Lower dimension geometries not kept in intersection'

    layer3 = ds.CreateLayer('layer3c')
    layer1.Union(layer2, layer3, ['KEEP_LOWER_DIMENSION_GEOMETRIES=NO'])
    assert layer3.GetFeatureCount() == 4, \
        'Lower dimension geometries not removed in union'

    layer3 = ds.CreateLayer('layer3d')
    layer1.Union(layer2, layer3, ['KEEP_LOWER_DIMENSION_GEOMETRIES=YES'])
    assert layer3.GetFeatureCount() == 6, 'Lower dimension geometries not kept in union'

    layer3 = ds.CreateLayer('layer3e')
    layer1.Identity(layer2, layer3, ['KEEP_LOWER_DIMENSION_GEOMETRIES=NO'])
    assert layer3.GetFeatureCount() == 2, \
        'Lower dimension geometries not removed in identity'

    layer3 = ds.CreateLayer('layer3f')
    layer1.Identity(layer2, layer3, ['KEEP_LOWER_DIMENSION_GEOMETRIES=YES'])
    assert layer3.GetFeatureCount() == 4, \
        'Lower dimension geometries not kept in identity'


def test_algebra_union():

    recreate_layer_C()

    # Union; this should return 5 polygons

    err = A.Union(B, C)

    assert err == 0, ('got non-zero result code ' + str(err) + ' from Layer.Union')

    assert C.GetFeatureCount() == 5, \
        ('Layer.Union returned ' + str(C.GetFeatureCount()) + ' features')

    recreate_layer_C()

    err = A.Union(B, C, options=['PROMOTE_TO_MULTI=YES'])

    assert err == 0, ('got non-zero result code ' + str(err) + ' from Layer.Union')

    assert C.GetFeatureCount() == 5, \
        ('Layer.Union returned ' + str(C.GetFeatureCount()) + ' features')

    recreate_layer_C()

    # Union with self ; this should return 2 polygons

    err = D1.Union(D2, C, ['KEEP_LOWER_DIMENSION_GEOMETRIES=NO'])

    assert err == 0, ('got non-zero result code ' + str(err) + ' from Layer.Union')

    assert is_same(D1, C), 'D1 != C'

    recreate_layer_C()

    # Union of a polygon and a point within : should return the point and the polygon (#4772)

    err = B.Union(pointInB, C)

    assert err == 0, ('got non-zero result code ' + str(err) + ' from Layer.Union')

    assert C.GetFeatureCount() == 2, \
        ('Layer.Union returned ' + str(C.GetFeatureCount()) + ' features')


def test_algebra_symdifference():

    recreate_layer_C()

    # SymDifference; this should return 3 polygons

    err = A.SymDifference(B, C)

    assert err == 0, ('got non-zero result code ' + str(err) + ' from Layer.SymDifference')

    assert C.GetFeatureCount() == 3, \
        ('Layer.SymDifference returned ' + str(C.GetFeatureCount()) + ' features')

    recreate_layer_C()

    err = A.SymDifference(B, C, options=['PROMOTE_TO_MULTI=YES'])

    assert err == 0, ('got non-zero result code ' + str(err) + ' from Layer.SymDifference')

    assert C.GetFeatureCount() == 3, \
        ('Layer.SymDifference returned ' + str(C.GetFeatureCount()) + ' features')

    recreate_layer_C()

    # SymDifference with self ; this should return 0 features

    err = D1.SymDifference(D2, C)

    assert err == 0, ('got non-zero result code ' + str(err) + ' from Layer.SymDifference')

    assert C.GetFeatureCount() == 0, \
        ('Layer.SymDifference returned ' + str(C.GetFeatureCount()) + ' features')


def test_algebra_identify():

    recreate_layer_C()

    # Identity; this should return 4 polygons

    err = A.Identity(B, C)

    assert err == 0, ('got non-zero result code ' + str(err) + ' from Layer.Identity')

    assert C.GetFeatureCount() == 4, \
        ('Layer.Identity returned ' + str(C.GetFeatureCount()) + ' features')

    recreate_layer_C()

    err = A.Identity(B, C, options=['PROMOTE_TO_MULTI=YES'])

    assert err == 0, ('got non-zero result code ' + str(err) + ' from Layer.Identity')

    assert C.GetFeatureCount() == 4, \
        ('Layer.Identity returned ' + str(C.GetFeatureCount()) + ' features')

    recreate_layer_C()

    # Identity with self ; this should return 2 polygons

    err = D1.Identity(D2, C, ['KEEP_LOWER_DIMENSION_GEOMETRIES=NO'])

    assert err == 0, ('got non-zero result code ' + str(err) + ' from Layer.Identity')

    assert is_same(D1, C), 'D1 != C'


def test_algebra_update():

    recreate_layer_C()

    # Update; this should return 3 polygons

    err = A.Update(B, C)

    assert err == 0, ('got non-zero result code ' + str(err) + ' from Layer.Update')

    assert C.GetFeatureCount() == 3, \
        ('Layer.Update returned ' + str(C.GetFeatureCount()) + ' features')

    recreate_layer_C()

    err = A.Update(B, C, options=['PROMOTE_TO_MULTI=YES'])

    assert err == 0, ('got non-zero result code ' + str(err) + ' from Layer.Update')

    assert C.GetFeatureCount() == 3, \
        ('Layer.Update returned ' + str(C.GetFeatureCount()) + ' features')

    recreate_layer_C()

    # Update with self ; this should return 2 polygons

    err = D1.Update(D2, C)

    assert err == 0, ('got non-zero result code ' + str(err) + ' from Layer.Update')

    assert is_same(D1, C), 'D1 != C'


def test_algebra_clip():

    recreate_layer_C()

    # Clip; this should return 2 polygons

    err = A.Clip(B, C)

    assert err == 0, ('got non-zero result code ' + str(err) + ' from Layer.Clip')

    assert C.GetFeatureCount() == 2, \
        ('Layer.Clip returned ' + str(C.GetFeatureCount()) + ' features')

    recreate_layer_C()

    err = A.Clip(B, C, options=['PROMOTE_TO_MULTI=YES'])

    assert err == 0, ('got non-zero result code ' + str(err) + ' from Layer.Clip')

    assert C.GetFeatureCount() == 2, \
        ('Layer.Clip returned ' + str(C.GetFeatureCount()) + ' features')

    recreate_layer_C()

    # Clip with self ; this should return 2 polygons

    err = D1.Update(D2, C)

    assert err == 0, ('got non-zero result code ' + str(err) + ' from Layer.Clip')

    assert is_same(D1, C), 'D1 != C'


def test_algebra_erase():

    recreate_layer_C()

    # Erase; this should return 2 polygons

    err = A.Erase(B, C)

    assert err == 0, ('got non-zero result code ' + str(err) + ' from Layer.Erase')

    assert C.GetFeatureCount() == 2, \
        ('Layer.Erase returned ' + str(C.GetFeatureCount()) + ' features')

    recreate_layer_C()

    err = A.Erase(B, C, options=['PROMOTE_TO_MULTI=YES'])

    assert err == 0, ('got non-zero result code ' + str(err) + ' from Layer.Erase')

    assert C.GetFeatureCount() == 2, \
        ('Layer.Erase returned ' + str(C.GetFeatureCount()) + ' features')

    recreate_layer_C()

    # Erase with self ; this should return 0 features

    err = D1.Erase(D2, C)

    assert err == 0, ('got non-zero result code ' + str(err) + ' from Layer.Erase')

    assert C.GetFeatureCount() == 0, \
        ('Layer.Erase returned ' + str(C.GetFeatureCount()) + ' features')

    recreate_layer_C()

    # Erase with empty layer (or no intersection)

    A.Erase(empty, C)

    assert C.GetFeatureCount() == A.GetFeatureCount(), \
        ('Layer.Erase returned ' + str(C.GetFeatureCount()) + ' features')

    A.ResetReading()
    feat_a = A.GetNextFeature()
    feat_c = C.GetNextFeature()
    if feat_a.Equal(feat_c) != 0:
        feat_a.DumpReadable()
        feat_c.DumpReadable()
        pytest.fail('features not identical')

    recreate_layer_C()

    A.Erase(empty, C, options=['PROMOTE_TO_MULTI=YES'])

    assert C.GetFeatureCount() == A.GetFeatureCount(), \
        ('Layer.Erase returned ' + str(C.GetFeatureCount()) + ' features')

    recreate_layer_C()


def test_algebra_cleanup():

    global ds, A, B, C, pointInB, D1, D2, empty

    D2 = None
    D1 = None
    pointInB = None
    C = None
    B = None
    A = None
    empty = None
    ds = None



