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
import pytest

from osgeo import ogr

pytestmark = pytest.mark.require_geos


###############################################################################
# Common usage tests.


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


@pytest.fixture()
def mem_ds(request):

    ds = ogr.GetDriverByName("Memory").CreateDataSource(request.node.name)

    return ds


@pytest.fixture()
def A(mem_ds):

    lyr = mem_ds.CreateLayer("A")
    lyr.CreateField(ogr.FieldDefn("A", ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn("same_in_both_layers", ogr.OFTInteger))

    a1 = "POLYGON((1 2, 1 3, 3 3, 3 2, 1 2))"
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField("A", 1)
    feat.SetGeometryDirectly(ogr.Geometry(wkt=a1))
    lyr.CreateFeature(feat)

    a2 = "POLYGON((5 2, 5 3, 7 3, 7 2, 5 2))"
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField("A", 2)
    feat.SetGeometryDirectly(ogr.Geometry(wkt=a2))
    lyr.CreateFeature(feat)

    return lyr


@pytest.fixture()
def B(mem_ds):

    lyr = mem_ds.CreateLayer("B")
    lyr.CreateField(ogr.FieldDefn("B", ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn("same_in_both_layers", ogr.OFTInteger))

    b1 = "POLYGON((2 1, 2 4, 6 4, 6 1, 2 1))"
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField("B", "first")
    feat.SetGeometryDirectly(ogr.Geometry(wkt=b1))
    lyr.CreateFeature(feat)

    return lyr


@pytest.fixture()
def pointInB(mem_ds):

    lyr = mem_ds.CreateLayer("pointInB")

    pointInB1 = "POINT(3 3)"
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.Geometry(wkt=pointInB1))
    lyr.CreateFeature(feat)

    return lyr


@pytest.fixture()
def D1(mem_ds):

    lyr = mem_ds.CreateLayer("D1")

    d1 = "POLYGON((1 2, 1 3, 3 3, 3 2, 1 2))"
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.Geometry(wkt=d1))
    lyr.CreateFeature(feat)

    d2 = "POLYGON((3 2, 3 3, 4 3, 4 2, 3 2))"
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.Geometry(wkt=d2))
    lyr.CreateFeature(feat)

    return lyr


@pytest.fixture()
def D2(mem_ds):

    lyr = mem_ds.CreateLayer("D1")

    d1 = "POLYGON((1 2, 1 3, 3 3, 3 2, 1 2))"
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.Geometry(wkt=d1))
    lyr.CreateFeature(feat)

    d2 = "POLYGON((3 2, 3 3, 4 3, 4 2, 3 2))"
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.Geometry(wkt=d2))
    lyr.CreateFeature(feat)

    return lyr


@pytest.fixture()
def C(mem_ds):

    return mem_ds.CreateLayer("C")


@pytest.fixture()
def empty(mem_ds):

    return mem_ds.CreateLayer("empty")


def test_algebra_intersection_1(A, B, C):

    # Intersection; this should return two rectangles

    err = A.Intersection(B, C)

    assert err == 0, "got non-zero result code " + str(err) + " from Layer.Intersection"

    C_defn = C.GetLayerDefn()
    assert (
        C_defn.GetFieldCount() == 4
        and C_defn.GetFieldDefn(0).GetName() == "A"
        and C_defn.GetFieldDefn(0).GetType() == ogr.OFTInteger
        and C_defn.GetFieldDefn(1).GetName() == "input_same_in_both_layers"
        and C_defn.GetFieldDefn(1).GetType() == ogr.OFTInteger
        and C_defn.GetFieldDefn(2).GetName() == "B"
        and C_defn.GetFieldDefn(2).GetType() == ogr.OFTString
        and C_defn.GetFieldDefn(3).GetName() == "method_same_in_both_layers"
        and C_defn.GetFieldDefn(3).GetType() == ogr.OFTInteger
    ), "Did not get expected output schema."

    assert C.GetFeatureCount() == 2, (
        "Layer.Intersection returned " + str(C.GetFeatureCount()) + " features"
    )

    f1 = (ogr.Geometry(wkt="POLYGON ((2 3,3 3,3 2,2 2,2 3))"), 1, "first")
    f2 = (ogr.Geometry(wkt="POLYGON ((5 2,5 3,6 3,6 2,5 2))"), 2, "first")

    C.ResetReading()
    while 1:
        feat = C.GetNextFeature()
        if not feat:
            break

        g = feat.GetGeometryRef()

        try:
            ogrtest.check_feature_geometry(g, f1[0])
            assert (
                feat.GetField("A") == f1[1] and feat.GetField("B") == f1[2]
            ), "Did not get expected field values."
        except AssertionError:
            ogrtest.check_feature_geometry(g, f2[0])
            assert (
                feat.GetField("A") == f2[1] and feat.GetField("B") == f2[2]
            ), "Did not get expected field values."


def test_algebra_intersection_2(A, B, C):

    # This time we test with PROMOTE_TO_MULTI and pre-created output fields.
    C.CreateField(ogr.FieldDefn("A", ogr.OFTInteger))
    C.CreateField(ogr.FieldDefn("B", ogr.OFTString))

    err = A.Intersection(B, C, options=["PROMOTE_TO_MULTI=YES"])

    assert err == 0, "got non-zero result code " + str(err) + " from Layer.Intersection"

    assert C.GetFeatureCount() == 2, (
        "Layer.Intersection returned " + str(C.GetFeatureCount()) + " features"
    )

    f1 = (ogr.Geometry(wkt="MULTIPOLYGON (((2 3,3 3,3 2,2 2,2 3)))"), 1, "first")
    f2 = (ogr.Geometry(wkt="MULTIPOLYGON (((5 2,5 3,6 3,6 2,5 2)))"), 2, "first")

    C.ResetReading()
    while 1:
        feat = C.GetNextFeature()
        if not feat:
            break

        g = feat.GetGeometryRef()
        try:
            ogrtest.check_feature_geometry(g, f1[0])
            assert (
                feat.GetField("A") == f1[1] and feat.GetField("B") == f1[2]
            ), "Did not get expected field values. (1)"
        except AssertionError:
            ogrtest.check_feature_geometry(g, f2[0])
            assert (
                feat.GetField("A") == f2[1] and feat.GetField("B") == f2[2]
            ), "Did not get expected field values. (2)"


def test_algebra_intersection_3(D1, D2, C):

    # Intersection with self ; this should return 2 polygons

    err = D1.Intersection(D2, C, ["KEEP_LOWER_DIMENSION_GEOMETRIES=NO"])

    assert err == 0, "got non-zero result code " + str(err) + " from Layer.Intersection"

    assert is_same(D1, C), "D1 != C"


def test_algebra_intersection_multipoint():

    driver = ogr.GetDriverByName("MEMORY")
    ds = driver.CreateDataSource("ds")
    layer1 = ds.CreateLayer("layer1")
    layer2 = ds.CreateLayer("layer2")

    g1 = "LINESTRING (0 0, 1 1)"
    geom1 = ogr.CreateGeometryFromWkt(g1)
    feat1 = ogr.Feature(layer1.GetLayerDefn())
    feat1.SetGeometry(geom1)
    layer1.CreateFeature(feat1)

    g2 = "LINESTRING (0 1, 1 0)"
    geom2 = ogr.CreateGeometryFromWkt(g2)
    feat2 = ogr.Feature(layer2.GetLayerDefn())
    feat2.SetGeometry(geom2)
    layer2.CreateFeature(feat2)

    layer3 = ds.CreateLayer("layer3")
    layer1.Intersection(layer2, layer3, ["PROMOTE_TO_MULTI=YES"])
    f = layer3.GetNextFeature()
    assert f.GetGeometryRef().ExportToIsoWkt() == "MULTIPOINT ((0.5 0.5))"


def test_algebra_KEEP_LOWER_DIMENSION_GEOMETRIES():

    driver = ogr.GetDriverByName("MEMORY")
    ds = driver.CreateDataSource("ds")
    layer1 = ds.CreateLayer("layer1")
    layer2 = ds.CreateLayer("layer2")

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

    layer3 = ds.CreateLayer("layer3a")
    layer1.Intersection(layer2, layer3, ["KEEP_LOWER_DIMENSION_GEOMETRIES=NO"])
    assert (
        layer3.GetFeatureCount() == 0
    ), "Lower dimension geometries not removed in intersection"

    layer3 = ds.CreateLayer("layer3b")
    layer1.Intersection(layer2, layer3, ["KEEP_LOWER_DIMENSION_GEOMETRIES=YES"])
    assert (
        layer3.GetFeatureCount() == 2
    ), "Lower dimension geometries not kept in intersection"

    layer3 = ds.CreateLayer("layer3c")
    layer1.Union(layer2, layer3, ["KEEP_LOWER_DIMENSION_GEOMETRIES=NO"])
    assert (
        layer3.GetFeatureCount() == 4
    ), "Lower dimension geometries not removed in union"

    layer3 = ds.CreateLayer("layer3d")
    layer1.Union(layer2, layer3, ["KEEP_LOWER_DIMENSION_GEOMETRIES=YES"])
    assert layer3.GetFeatureCount() == 6, "Lower dimension geometries not kept in union"

    layer3 = ds.CreateLayer("layer3e")
    layer1.Identity(layer2, layer3, ["KEEP_LOWER_DIMENSION_GEOMETRIES=NO"])
    assert (
        layer3.GetFeatureCount() == 2
    ), "Lower dimension geometries not removed in identity"

    layer3 = ds.CreateLayer("layer3f")
    layer1.Identity(layer2, layer3, ["KEEP_LOWER_DIMENSION_GEOMETRIES=YES"])
    assert (
        layer3.GetFeatureCount() == 4
    ), "Lower dimension geometries not kept in identity"


def test_algebra_union_1(A, B, C):

    err = A.Union(B, C)

    assert err == 0, "got non-zero result code " + str(err) + " from Layer.Union"

    assert C.GetFeatureCount() == 5, (
        "Layer.Union returned " + str(C.GetFeatureCount()) + " features"
    )


def test_algebra_union_2(A, B, C):

    err = A.Union(B, C, options=["PROMOTE_TO_MULTI=YES"])

    assert err == 0, "got non-zero result code " + str(err) + " from Layer.Union"

    assert C.GetFeatureCount() == 5, (
        "Layer.Union returned " + str(C.GetFeatureCount()) + " features"
    )


def test_algebra_union_3(D1, D2, C):

    # Union with self ; this should return 2 polygons

    err = D1.Union(D2, C, ["KEEP_LOWER_DIMENSION_GEOMETRIES=NO"])

    assert err == 0, "got non-zero result code " + str(err) + " from Layer.Union"

    assert is_same(D1, C), "D1 != C"


def test_algebra_union_4(B, pointInB, C):

    # Union of a polygon and a point within : should return the point and the polygon (#4772)

    err = B.Union(pointInB, C)

    assert err == 0, "got non-zero result code " + str(err) + " from Layer.Union"

    assert C.GetFeatureCount() == 2, (
        "Layer.Union returned " + str(C.GetFeatureCount()) + " features"
    )


def test_algebra_symdifference_1(A, B, C):

    # SymDifference; this should return 3 polygons

    err = A.SymDifference(B, C)

    assert err == 0, (
        "got non-zero result code " + str(err) + " from Layer.SymDifference"
    )

    assert C.GetFeatureCount() == 3, (
        "Layer.SymDifference returned " + str(C.GetFeatureCount()) + " features"
    )


def test_algebra_symdifference_2(A, B, C):

    err = A.SymDifference(B, C, options=["PROMOTE_TO_MULTI=YES"])

    assert err == 0, (
        "got non-zero result code " + str(err) + " from Layer.SymDifference"
    )

    assert C.GetFeatureCount() == 3, (
        "Layer.SymDifference returned " + str(C.GetFeatureCount()) + " features"
    )


def test_algebra_symdifference_3(D1, D2, C):

    # SymDifference with self ; this should return 0 features

    err = D1.SymDifference(D2, C)

    assert err == 0, (
        "got non-zero result code " + str(err) + " from Layer.SymDifference"
    )

    assert C.GetFeatureCount() == 0, (
        "Layer.SymDifference returned " + str(C.GetFeatureCount()) + " features"
    )


def test_algebra_identity_1(A, B, C):

    # Identity; this should return 4 polygons

    err = A.Identity(B, C)

    assert err == 0, "got non-zero result code " + str(err) + " from Layer.Identity"

    assert C.GetFeatureCount() == 4, (
        "Layer.Identity returned " + str(C.GetFeatureCount()) + " features"
    )


def test_algebra_identity_2(A, B, C):

    err = A.Identity(B, C, options=["PROMOTE_TO_MULTI=YES"])

    assert err == 0, "got non-zero result code " + str(err) + " from Layer.Identity"

    assert C.GetFeatureCount() == 4, (
        "Layer.Identity returned " + str(C.GetFeatureCount()) + " features"
    )


def test_algebra_identity_3(D1, D2, C):

    # Identity with self ; this should return 2 polygons

    err = D1.Identity(D2, C, ["KEEP_LOWER_DIMENSION_GEOMETRIES=NO"])

    assert err == 0, "got non-zero result code " + str(err) + " from Layer.Identity"

    assert is_same(D1, C), "D1 != C"


def test_algebra_update_1(A, B, C):

    # Update; this should return 3 polygons

    err = A.Update(B, C)

    assert err == 0, "got non-zero result code " + str(err) + " from Layer.Update"

    assert C.GetFeatureCount() == 3, (
        "Layer.Update returned " + str(C.GetFeatureCount()) + " features"
    )


def test_algebra_update_2(A, B, C):

    err = A.Update(B, C, options=["PROMOTE_TO_MULTI=YES"])

    assert err == 0, "got non-zero result code " + str(err) + " from Layer.Update"

    assert C.GetFeatureCount() == 3, (
        "Layer.Update returned " + str(C.GetFeatureCount()) + " features"
    )


def test_algebra_update_3(D1, D2, C):

    # Update with self ; this should return 2 polygons

    err = D1.Update(D2, C)

    assert err == 0, "got non-zero result code " + str(err) + " from Layer.Update"

    assert is_same(D1, C), "D1 != C"


def test_algebra_clip_1(A, B, C):

    # Clip; this should return 2 polygons

    err = A.Clip(B, C)

    assert err == 0, "got non-zero result code " + str(err) + " from Layer.Clip"

    assert C.GetFeatureCount() == 2, (
        "Layer.Clip returned " + str(C.GetFeatureCount()) + " features"
    )


def test_algebra_clip_2(A, B, C):

    err = A.Clip(B, C, options=["PROMOTE_TO_MULTI=YES"])

    assert err == 0, "got non-zero result code " + str(err) + " from Layer.Clip"

    assert C.GetFeatureCount() == 2, (
        "Layer.Clip returned " + str(C.GetFeatureCount()) + " features"
    )


def test_algebra_clip_3(D1, D2, C):

    # Clip with self ; this should return 2 polygons

    err = D1.Update(D2, C)

    assert err == 0, "got non-zero result code " + str(err) + " from Layer.Clip"

    assert is_same(D1, C), "D1 != C"


def test_algebra_erase_1(A, B, C):

    # Erase; this should return 2 polygons

    err = A.Erase(B, C)

    assert err == 0, "got non-zero result code " + str(err) + " from Layer.Erase"

    assert C.GetFeatureCount() == 2, (
        "Layer.Erase returned " + str(C.GetFeatureCount()) + " features"
    )


def test_algebra_erase_2(A, B, C):

    err = A.Erase(B, C, options=["PROMOTE_TO_MULTI=YES"])

    assert err == 0, "got non-zero result code " + str(err) + " from Layer.Erase"

    assert C.GetFeatureCount() == 2, (
        "Layer.Erase returned " + str(C.GetFeatureCount()) + " features"
    )


def test_algebra_erase_3(D1, D2, C):

    # Erase with self ; this should return 0 features

    err = D1.Erase(D2, C)

    assert err == 0, "got non-zero result code " + str(err) + " from Layer.Erase"

    assert C.GetFeatureCount() == 0, (
        "Layer.Erase returned " + str(C.GetFeatureCount()) + " features"
    )


def test_algebra_erase_4(A, empty, C):

    # Erase with empty layer (or no intersection)

    A.Erase(empty, C)

    assert C.GetFeatureCount() == A.GetFeatureCount(), (
        "Layer.Erase returned " + str(C.GetFeatureCount()) + " features"
    )

    A.ResetReading()
    feat_a = A.GetNextFeature()
    feat_c = C.GetNextFeature()
    if feat_a.Equal(feat_c) != 0:
        feat_a.DumpReadable()
        feat_c.DumpReadable()
        pytest.fail("features not identical")


def test_algebra_erase_5(A, empty, C):

    A.Erase(empty, C, options=["PROMOTE_TO_MULTI=YES"])

    assert C.GetFeatureCount() == A.GetFeatureCount(), (
        "Layer.Erase returned " + str(C.GetFeatureCount()) + " features"
    )
