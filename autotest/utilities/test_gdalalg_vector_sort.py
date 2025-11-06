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

import pytest

from osgeo import gdal, ogr


@pytest.fixture()
def alg():
    return gdal.GetGlobalAlgorithmRegistry()["vector"]["sort"]


pytestmark = pytest.mark.require_geos


@pytest.mark.parametrize("method", ("hilbert", "strtree"))
def test_gdalalg_vector_sort(alg, method):

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

    # TODO: remove/disable code below this point
    # with gdal.GetDriverByName("ESRI Shapefile").CreateVector(f"/tmp/out_{method}.shp") as ds:
    #    lyr = ds.CreateLayer("line", geom_type=ogr.wkbLineString)

    #    feature = ogr.Feature(lyr.GetLayerDefn())

    #    geom = ogr.Geometry(ogr.wkbLineString)

    #    for f in dst_lyr:
    #        g = f.GetGeometryRef()
    #        geom.AddPoint_2D(*g.GetPoint_2D())

    #    feature.SetGeometryDirectly(geom)

    #    lyr.CreateFeature(feature)
