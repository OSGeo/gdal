#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal vector sort' testing
# Author:   Daniel Baston
#
###############################################################################
# Copyright (c) 2025, ISciences LLC
#
# SPDX-License-Identifier: MIT
###############################################################################

import random

import ogrtest
import pytest

from osgeo import gdal, ogr


@pytest.fixture()
def alg():
    return gdal.GetGlobalAlgorithmRegistry()["vector"]["sort"]


@pytest.fixture(params=("hilbert", "strtree"))
def method(request):

    if request.param == "strtree" and not ogrtest.have_geos():
        pytest.skip("GEOS not available")

    yield request.param


@pytest.fixture(params=(True, False))
def use_tempfile(request):

    yield request.param


def test_gdalalg_vector_sort(alg, method, use_tempfile):

    src_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src_lyr = src_ds.CreateLayer("points", geom_type=ogr.wkbPoint)
    src_lyr.CreateField(ogr.FieldDefn("id", ogr.OFTInteger))

    nx = 8
    ny = 8

    points = []
    i = 0

    for y in range(ny):
        for x in range(nx):
            points.append((i, x, y))
            i += 1

    random.Random(802).shuffle(points)

    for i, x, y in points:
        feature = ogr.Feature(src_lyr.GetLayerDefn())
        feature["id"] = i

        geom = ogr.Geometry(ogr.wkbPoint)
        geom.AddPoint_2D(x, y)

        feature.SetGeometryDirectly(geom)
        src_lyr.CreateFeature(feature)

    alg["input"] = src_ds
    alg["method"] = method
    alg["output-format"] = "stream"
    alg["use-tempfile"] = use_tempfile

    assert alg.Run()

    dst_ds = alg.Output()
    dst_lyr = dst_ds.GetLayer(0)

    assert dst_lyr.GetFeatureCount() == len(points)
    points_sorted = [f.GetGeometryRef().GetPoint_2D() for f in dst_lyr]

    if method == "hilbert":
        assert points_sorted[0] == (0, 0)
        assert points_sorted[1] == (0, 1)
        assert points_sorted[2] == (1, 1)
        assert points_sorted[3] == (1, 0)
        assert points_sorted[4] == (2, 0)
        assert points_sorted[5] == (3, 0)
        assert points_sorted[6] == (3, 1)
    elif method == "strtree":
        for i in range(20):
            assert points_sorted[i][1] <= 20


def test_gdalalg_vector_sort_no_geometry_field(alg, method, use_tempfile):

    n_features = 20

    src_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src_lyr = src_ds.CreateLayer("points", geom_type=ogr.wkbNone)
    src_lyr.CreateField(ogr.FieldDefn("id", ogr.OFTInteger))

    for i in range(n_features):
        feature = ogr.Feature(src_lyr.GetLayerDefn())
        feature["id"] = i
        src_lyr.CreateFeature(feature)

    alg["input"] = src_ds
    alg["method"] = method
    alg["output-format"] = "stream"
    alg["use-tempfile"] = use_tempfile

    assert alg.Run()

    dst_ds = alg.Output()

    assert dst_ds.GetLayerCount() == 1

    dst_lyr = dst_ds.GetLayer(0)

    assert dst_lyr.GetFeatureCount() == n_features

    for i, feature in enumerate(dst_lyr):
        assert feature["id"] == i


@pytest.mark.parametrize("geom_type", ("null", "empty"))
def test_gdalalg_vector_sort_null_empty_geometry(alg, method, geom_type, use_tempfile):

    nx, ny = 8, 8
    n_empty = 4

    src_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src_lyr = src_ds.CreateLayer("points", geom_type=ogr.wkbPoint)
    src_lyr.CreateField(ogr.FieldDefn("id", ogr.OFTInteger))

    points = []
    i = 0

    for y in range(ny):
        for x in range(nx):
            points.append((i, x, y))
            i += 1

    for i in range(n_empty):
        points.append((i, None, None))
        i += 1

    random.Random(802).shuffle(points)

    for i, x, y in points:
        feature = ogr.Feature(src_lyr.GetLayerDefn())
        feature["id"] = i

        geom = ogr.Geometry(ogr.wkbPoint)
        if x is not None:
            geom.AddPoint_2D(x, y)

        if x is not None or geom_type != "null":
            feature.SetGeometryDirectly(geom)

        src_lyr.CreateFeature(feature)

    alg["input"] = src_ds
    alg["method"] = method
    alg["output-format"] = "stream"
    alg["use-tempfile"] = use_tempfile

    tab_pct = [0]

    def my_progress(pct, msg, user_data):
        assert pct >= tab_pct[0]
        tab_pct[0] = pct
        return True

    assert alg.Run(my_progress)

    assert tab_pct[0] == 1.0

    dst_ds = alg.Output()
    dst_lyr = dst_ds.GetLayer(0)

    assert dst_lyr.GetFeatureCount() == len(points)
    features = [f for f in dst_lyr]

    for feature in features[-n_empty:]:
        if geom_type == "empty":
            assert feature.GetGeometryRef().IsEmpty()
        else:
            assert feature.GetGeometryRef() is None


def test_gdalalg_vector_sort_invalid_method(alg):

    with pytest.raises(Exception, match="Invalid value .* argument 'method'"):
        alg["method"] = "does_not_exist"


@pytest.mark.parametrize("geometry_field", ("", "swapped_geom"))
def test_gdalalg_vector_sort_multiple_geom_fields(alg, geometry_field):

    poly_ds = gdal.OpenEx("../ogr/data/poly.shp", gdal.OF_VECTOR)

    ds = gdal.GetDriverByName("MEM").CreateVector("")
    lyr = ds.CreateLayer("source", geom_type=ogr.wkbPolygon)
    lyr.CreateField(ogr.FieldDefn("EAS_ID", ogr.OFTInteger64))
    lyr.CreateGeomField(ogr.GeomFieldDefn("swapped_geom", ogr.wkbPolygon))

    for f in poly_ds.GetLayer(0):
        f_out = ogr.Feature(lyr.GetLayerDefn())
        f_out["EAS_ID"] = f["EAS_ID"]
        f_out.SetGeomField(0, f.GetGeometryRef())

        g1 = f.GetGeometryRef().Clone()
        g1.SwapXY()
        f_out.SetGeomField(1, g1)

        lyr.CreateFeature(f_out)

    alg["input"] = ds
    alg["geometry-field"] = "does_not_exist"
    alg["output"] = ""
    alg["output-format"] = "stream"

    with pytest.raises(Exception, match="Specified geometry field .* does not exist"):
        alg.Run()

    alg["geometry-field"] = geometry_field

    assert alg.Run()

    dst_ds = alg.Output()
    dst_lyr = dst_ds.GetLayer(0)

    f = dst_lyr.GetNextFeature()

    if geometry_field == "swapped_geom":
        assert f["EAS_ID"] == 158
    else:
        assert f["EAS_ID"] == 173
