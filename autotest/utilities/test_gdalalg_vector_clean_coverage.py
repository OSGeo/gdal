#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal vector clean-coverage' testing
# Author:   Daniel Baston
#
###############################################################################
# Copyright (c) 2025, ISciences LLC
#
# SPDX-License-Identifier: MIT
###############################################################################

import json
import string

import gdaltest
import pytest

from osgeo import gdal, ogr, osr

pytestmark = pytest.mark.require_geos(3, 14)


@pytest.fixture()
def alg():
    return gdal.GetGlobalAlgorithmRegistry()["vector"]["clean-coverage"]


@pytest.fixture()
def circles():

    ds = gdal.GetDriverByName("MEM").CreateVector("")
    lyr = ds.CreateLayer(
        "circles", osr.SpatialReference(epsg=32145), geom_type=ogr.wkbPolygon
    )
    lyr.CreateField(ogr.FieldDefn("a", ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn("b", ogr.OFTReal))

    geoms = []

    g = ogr.Geometry(ogr.wkbPoint)
    g.AddPoint(5, 5)
    geoms.append(g.Buffer(5))

    g = ogr.Geometry(ogr.wkbPoint)
    g.AddPoint(10, 15)
    geoms.append(g.Buffer(6.5))

    g = ogr.Geometry(ogr.wkbPoint)
    g.AddPoint(15, 5)
    geoms.append(g.Buffer(5.1))

    for i, geom in enumerate(geoms):
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometry(geom)
        f["a"] = "V" + string.ascii_lowercase[i]
        f["b"] = i + 0.2
        lyr.CreateFeature(f)

    return ds


def test_gdalalg_vector_clean_coverage(alg, circles):

    src_lyr = circles.GetLayer(0)

    alg["input"] = circles
    alg["output"] = ""
    alg["output-format"] = "stream"

    assert alg.Run()

    dst_ds = alg["output"].GetDataset()
    dst_lyr = dst_ds.GetLayer(0)

    assert dst_lyr.GetFeatureCount() == src_lyr.GetFeatureCount()
    assert dst_lyr.GetSpatialRef().IsSame(src_lyr.GetSpatialRef())

    areas = [f.GetGeometryRef().GetArea() for f in dst_lyr]
    assert areas == pytest.approx([77.85, 132.67, 80.82], abs=0.01)

    for src_feature, dst_feature in zip(src_lyr, dst_lyr):
        src_attrs = json.loads(src_feature.ExportToJson())["properties"]
        dst_attrs = json.loads(dst_feature.ExportToJson())["properties"]
        assert src_attrs == dst_attrs

    assert alg.Finalize()


@pytest.mark.parametrize(
    "strategy", (None, "longest-border", "min-index", "max-area", "min-area")
)
def test_gdalalg_vector_clean_coverage_merge_strategy(alg, circles, strategy):

    alg["input"] = circles
    alg["output"] = ""
    alg["output-format"] = "stream"

    if strategy is not None:
        alg["merge-strategy"] = strategy

    assert alg.Run()

    dst_ds = alg["output"].GetDataset()
    dst_lyr = dst_ds.GetLayer(0)

    areas = [f.GetGeometryRef().GetArea() for f in dst_lyr]

    if strategy in {"max-area", None, "longest-border"}:
        assert areas == pytest.approx([77.85, 132.67, 80.82], abs=0.01)
    elif strategy == "min-area":
        assert areas == pytest.approx([78.50, 131.26, 81.58], abs=0.01)
    elif strategy == "min-index":
        assert areas == pytest.approx([78.50, 132.11, 80.73], abs=0.01)
    else:
        pytest.fail("Unhandled strategy")

    assert alg.Finalize()


def test_gdalalg_vector_clean_coverage_maximum_gap_width(alg, circles):

    alg["input"] = circles
    alg["output"] = ""
    alg["output-format"] = "stream"
    alg["maximum-gap-width"] = 2

    assert alg.Run()

    dst_ds = alg["output"].GetDataset()
    dst_lyr = dst_ds.GetLayer(0)

    areas = [f.GetGeometryRef().GetArea() for f in dst_lyr]

    assert areas == pytest.approx([80.80, 132.67, 80.82], abs=0.01)


@pytest.mark.parametrize("tol", (-5, float("nan")))
def test_gdalalg_vector_clean_coverage_bad_maximum_gap_width(alg, tol):

    with pytest.raises(RuntimeError, match="should be >= 0"):
        alg["maximum-gap-width"] = tol


@pytest.mark.parametrize("tol", (-5, float("nan")))
def test_gdalalg_vector_clean_coverage_bad_snapping_distance(alg, tol):

    with pytest.raises(RuntimeError, match="should be >= 0"):
        alg["snapping-distance"] = tol


def test_gdalalg_vector_clean_coverage_active_layer(alg, circles):

    src_ds = gdal.GetDriverByName("MEM").CreateVector("")

    src_ds.CopyLayer(circles.GetLayer(0), "poly1")
    src_ds.CopyLayer(circles.GetLayer(0), "poly2")

    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "stream"
    alg["active-layer"] = "bad"

    with pytest.raises(RuntimeError, match="layer .* was not found"):
        alg.Run()

    alg["active-layer"] = "poly2"
    assert alg.Run()

    dst_ds = alg["output"].GetDataset()

    assert dst_ds.GetLayerCount() == 2

    dst_lyr1 = dst_ds.GetLayer(0)
    dst_lyr2 = dst_ds.GetLayer(1)
    areas1 = [f.GetGeometryRef().GetArea() for f in dst_lyr1]
    areas2 = [f.GetGeometryRef().GetArea() for f in dst_lyr2]

    # overlaps removed in layer 2, so total area is less
    assert sum(areas1) > sum(areas2)

    assert alg.Finalize()


@pytest.mark.parametrize(
    "geom",
    (
        ogr.CreateGeometryFromWkt("POINT (3 8)"),
        ogr.CreateGeometryFromWkt("TIN (((0 0,0 1,1 1,0 0)))"),
        None,
    ),
)
def test_gdalalg_vector_clean_coverage_non_polygonal_inputs(alg, geom, tmp_vsimem):

    src_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src_lyr = src_ds.CreateLayer("layer")
    src_lyr.CreateField(ogr.FieldDefn("a"))

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f["a"] = 1
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT (3 8)"))
    src_lyr.CreateFeature(f)

    alg["input"] = src_ds
    alg["output"] = tmp_vsimem / "out.shp"

    with pytest.raises(RuntimeError, match="can only be performed on polygonal"):
        assert alg.Run()


@pytest.mark.require_driver("GDALG")
def test_gdalalg_vector_clean_coverage_test_ogrsf(tmp_path):

    import test_cli_utilities

    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    gdalg_filename = tmp_path / "tmp.gdalg.json"
    open(gdalg_filename, "wb").write(
        b'{"type": "gdal_streamed_alg","command_line": "gdal vector clean-coverage ../ogr/data/poly.shp --output-format=stream dummy_dataset_name","relative_paths_relative_to_this_file":false}'
    )

    ret = gdaltest.runexternal(
        test_cli_utilities.get_test_ogrsf_path() + f" -ro {gdalg_filename}"
    )

    assert "INFO" in ret
    assert "ERROR" not in ret
    assert "FAILURE" not in ret
