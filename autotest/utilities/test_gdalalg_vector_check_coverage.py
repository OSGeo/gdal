#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal vector check-coverage' testing
# Author:   Daniel Baston
#
###############################################################################
# Copyright (c) 2025, ISciences LLC
#
# SPDX-License-Identifier: MIT
###############################################################################

import pytest

from osgeo import gdal, ogr

pytestmark = pytest.mark.require_geos(3, 12)

import gdaltest


@pytest.fixture()
def alg():
    return gdal.GetGlobalAlgorithmRegistry()["vector"]["check-coverage"]


@pytest.fixture()
def three_rectangles():

    return gdaltest.wkt_ds(
        [
            "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))",
            "POLYGON ((9.9 0, 20 0, 20 10, 9.9 10, 9.9 0))",
            "POLYGON ((20 0, 30 0, 30 10, 20 10, 20 0))",
        ],
        epsg=32145,
    )


@pytest.mark.parametrize("include_valid", (True, False))
def test_gdalalg_vector_check_coverage(alg, include_valid, three_rectangles):

    alg["input"] = three_rectangles
    alg["output"] = ""
    alg["output-format"] = "stream"

    if include_valid:
        alg["include-valid"] = True

    assert alg.Run()

    dst_ds = alg["output"].GetDataset()
    dst_lyr = dst_ds.GetLayer(0)
    assert dst_lyr.GetName() == "invalid_edge"

    assert dst_lyr.GetSpatialRef().IsSame(three_rectangles.GetLayer(0).GetSpatialRef())
    assert dst_lyr.GetFeatureCount() == 3 if include_valid else 2

    features = [f for f in dst_lyr]
    assert len(features) == dst_lyr.GetFeatureCount()

    assert not features[0].GetGeometryRef().IsEmpty()
    assert not features[1].GetGeometryRef().IsEmpty()

    if include_valid:
        assert features[2].GetGeometryRef().IsEmpty()

    assert alg.Finalize()


def test_gdalalg_vector_check_coverage_invalid_layer(alg, three_rectangles):

    alg["input"] = three_rectangles
    alg["input-layer"] = "does_not_exist"
    alg["output"] = ""
    alg["output-format"] = "stream"

    with pytest.raises(RuntimeError, match="Cannot find source layer"):
        assert alg.Run()


@pytest.mark.parametrize("input_layers", (1, 2))
def test_gdalalg_vector_check_coverage_two_layers(alg, three_rectangles, input_layers):

    poly_ds = gdal.OpenEx("../ogr/data/poly.shp", gdal.OF_VECTOR)

    ds = gdal.GetDriverByName("MEM").CreateVector("")
    ds.CopyLayer(poly_ds.GetLayer(0), "poly1")
    ds.CopyLayer(three_rectangles.GetLayer(0), "poly2")

    alg["input"] = ds
    alg["output"] = ""
    alg["output-format"] = "stream"
    if input_layers == 1:
        alg["input-layer"] = "poly2"
    else:
        alg["input-layer"] = ["poly2", "poly1"]

    assert alg.Run()

    dst_ds = alg["output"].GetDataset()
    assert dst_ds.GetLayerCount() == input_layers

    for i in range(input_layers):
        dst_lyr = dst_ds.GetLayer(i)

        errors = 2 if i == 0 else 0

        if input_layers == 1:
            assert dst_lyr.GetName() == "invalid_edge"
        else:
            assert dst_lyr.GetName() == f"invalid_edge_{alg['input-layer'][i]}"

        assert dst_lyr.GetFeatureCount() == errors

        for f in dst_lyr:
            assert f.GetGeometryRef().GetGeometryType() == ogr.wkbMultiLineString

    assert alg.Finalize()


def test_gdalalg_vector_check_coverage_invalid_geometry_name(alg, three_rectangles):

    alg["input"] = three_rectangles
    alg["geometry-field"] = "does_not_exist"
    alg["output"] = ""
    alg["output-format"] = "stream"

    with pytest.raises(
        RuntimeError, match="Specified geometry field .* does not exist"
    ):
        assert alg.Run()


def test_gdalalg_vector_check_coverage_multiple_geometry_fields(alg):

    poly_ds = gdal.OpenEx("../ogr/data/poly.shp", gdal.OF_VECTOR)

    ds = gdal.GetDriverByName("MEM").CreateVector("")
    lyr = ds.CreateLayer("source", geom_type=ogr.wkbPolygon)
    lyr.CreateGeomField(ogr.GeomFieldDefn("bufgeom", ogr.wkbPolygon))

    for f in poly_ds.GetLayer(0):
        f_out = ogr.Feature(lyr.GetLayerDefn())
        f_out.SetGeomField(0, f.GetGeometryRef())
        f_out.SetGeomField(1, f.GetGeometryRef().Buffer(0.1))
        lyr.CreateFeature(f_out)

    alg["input"] = ds
    alg["geometry-field"] = "bufgeom"
    alg["output"] = ""
    alg["output-format"] = "stream"

    assert alg.Run()

    dst_ds = alg["output"].GetDataset()
    dst_lyr = dst_ds.GetLayer(0)

    assert dst_lyr.GetFeatureCount() == 10

    assert alg.Finalize()


def test_gdalalg_vector_check_coverage_multiple_layers(alg):

    ds = gdal.GetDriverByName("MEM").CreateVector("")

    ds.CreateLayer("source1")
    ds.CreateLayer("source2")

    alg["input"] = ds
    alg["output"] = ""
    alg["output-format"] = "stream"

    assert alg.Run()

    dst_ds = alg["output"].GetDataset()
    assert dst_ds.GetLayer(0).GetName() == "invalid_edge_source1"
    assert dst_ds.GetLayer(1).GetName() == "invalid_edge_source2"


def test_gdalalg_vector_check_coverage_no_geometry_field(alg):

    ds = gdal.GetDriverByName("MEM").CreateVector("")
    ds.CreateLayer("source", geom_type=ogr.wkbNone)

    alg["input"] = ds
    alg["output"] = ""
    alg["output-format"] = "stream"

    assert alg.Run()

    dst_ds = alg["output"].GetDataset()
    assert dst_ds.GetLayerCount() == 0

    alg["input"] = ds
    alg["output"] = ""
    alg["output-format"] = "stream"
    alg["input-layer"] = "source"

    with pytest.raises(
        Exception, match="Specified layer 'source' has no geometry field"
    ):
        alg.Run()
