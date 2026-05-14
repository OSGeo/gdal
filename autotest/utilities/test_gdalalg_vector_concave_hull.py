#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal vector concave-hull' testing
# Author:   Daniel Baston
#
###############################################################################
# Copyright (c) 2026, ISciences LLC
#
# SPDX-License-Identifier: MIT
###############################################################################

import gdaltest
import ogrtest
import pytest

from osgeo import gdal, ogr

pytestmark = pytest.mark.require_geos(3, 11)


@pytest.fixture()
def alg():
    return gdal.GetGlobalAlgorithmRegistry()["vector"]["concave-hull"]


@pytest.mark.parametrize(
    "input_wkt,ratio,output_wkt",
    [
        pytest.param("POINT (0 0)", 1, "POINT (0 0)", id="single point"),
        pytest.param(
            "MULTIPOINT (0 1, 3 4)", 1, "LINESTRING (0 1, 3 4)", id="two points"
        ),
        pytest.param(
            "MULTIPOINT (0 1, 8 2, 3 4, 3 2)",
            1,
            "POLYGON ((0 1, 3 4, 8 2, 0 1))",
            id="four points, convex",
        ),
        pytest.param(
            "MULTIPOINT (0 1, 8 2, 3 4, 3 2)",
            0,
            "POLYGON ((3 2, 0 1, 3 4, 8 2, 3 2))",
            id="four points, concave",
        ),
    ],
)
@pytest.mark.require_geos()
def test_gdalalg_vector_concave_hull_basic(alg, input_wkt, ratio, output_wkt):

    src_ds = gdaltest.wkt_ds([input_wkt], epsg=32631)

    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "stream"
    alg["ratio"] = ratio

    assert alg.Run()

    out_ds = alg["output"].GetDataset()
    out_lyr = out_ds.GetLayer(0)
    out_f = out_lyr.GetNextFeature()
    assert out_lyr.GetLayerDefn().IsSame(out_f.GetDefnRef())
    assert out_lyr.GetGeomType() == ogr.wkbUnknown

    ogrtest.check_feature_geometry(out_f, output_wkt)
    assert out_f.GetGeometryRef().GetSpatialReference().GetAuthorityCode() == "32631"
    out_f = out_lyr.GetNextFeature()
    assert out_f is None


@pytest.mark.parametrize("allow_holes", (True, False))
def test_gdalalg_vector_concave_hull_holes(alg, allow_holes):

    # points arranged in a donut
    src_ds = gdaltest.wkt_ds(
        [
            "MULTIPOINT ((90 20), (80 10), (45 5), (10 20), (20 10), (21 30), (40 20), (11 60), (20 70), (20 90), (40 80), (70 80), (80 60), (90 70), (80 90), (56 95), (95 45), (80 40), (70 20), (15 45), (5 40), (40 96), (60 15))"
        ]
    )

    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "stream"
    alg["ratio"] = 0.4
    alg["allow-holes"] = allow_holes

    assert alg.Run()

    out_ds = alg["output"].GetDataset()
    out_lyr = out_ds.GetLayer(0)
    out_f = out_lyr.GetNextFeature()
    g = out_f.GetGeometryRef()

    assert g.GetGeometryType() == ogr.wkbPolygon

    if allow_holes:
        assert g.GetGeometryCount() == 2  # exterior ring and interior ring
    else:
        assert g.GetGeometryCount() == 1  # exterior ring only


@pytest.mark.parametrize("ratio", (-1, 1.1))
def test_gdalalg_vector_concave_hull_invalid_ratio(alg, ratio):

    with pytest.raises(Exception, match="Value of argument 'ratio"):
        alg["ratio"] = ratio


@pytest.mark.parametrize("allow_holes", (True, False))
@pytest.mark.parametrize("geom_type", (ogr.wkbUnknown, ogr.wkbMultiPolygon))
def test_gdalalg_vector_concave_hull_polygon_mode_allow_holes(
    alg, allow_holes, geom_type
):

    # Two Pacman's facing each other
    src_ds = gdaltest.wkt_ds(
        [
            "MULTIPOLYGON(((0 0,0 1,1 1,1 0.9,0.1 0.9,0.1 0.1,1 0.1,1 0,0 0)),((1.1 1,2 1,2 0,1.1 0,1.1 0.1,1.9 0.1,1.9 0.9, 1.1 0.9,1.1 1)))"
        ],
        geom_type=geom_type,
    )

    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "stream"
    alg["ratio"] = 0.5
    alg["allow-holes"] = allow_holes

    assert alg.Run()

    out_ds = alg["output"].GetDataset()
    out_lyr = out_ds.GetLayer(0)
    assert out_lyr.GetGeomType() == geom_type
    out_f = out_lyr.GetNextFeature()
    g = out_f.GetGeometryRef()

    assert (
        g.GetGeometryType() == ogr.wkbMultiPolygon
        if geom_type == ogr.wkbMultiPolygon
        else ogr.wkbPolygon
    )

    if g.GetGeometryType() == ogr.wkbMultiPolygon:
        assert g.GetGeometryCount() == 1
        g = g.GetGeometryRef(0)

    if allow_holes:
        assert g.GetGeometryCount() == 2  # exterior ring and interior ring
    else:
        assert g.GetGeometryCount() == 1  # exterior ring only


@pytest.mark.parametrize("tight", (True, False))
def test_gdalalg_vector_concave_hull_polygon_mode_tight(alg, tight):

    # Hourglass
    src_ds = gdaltest.wkt_ds(
        ["POLYGON((0 0,0.4 0.5,0 1,1 1,0.6 0.5,1 0,0 0))"],
        geom_type=ogr.wkbMultiPolygon,
    )

    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "stream"
    alg["ratio"] = 1
    alg["tight"] = tight

    assert alg.Run()

    out_ds = alg["output"].GetDataset()
    out_lyr = out_ds.GetLayer(0)
    assert out_lyr.GetGeomType() == ogr.wkbMultiPolygon
    out_f = out_lyr.GetNextFeature()
    g = out_f.GetGeometryRef()

    assert g.GetGeometryType() == ogr.wkbMultiPolygon
    assert g.GetGeometryCount() == 1

    if tight:
        assert g.GetGeometryRef(0).Equals(
            src_ds.GetLayer(0).GetFeature(1).GetGeometryRef()
        )
    else:
        assert g.Area() == 1.0
