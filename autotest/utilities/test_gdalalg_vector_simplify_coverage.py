#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal vector simplify-coverage' testing
# Author:   Daniel Baston
#
###############################################################################
# Copyright (c) 2025, ISciences LLC
#
# SPDX-License-Identifier: MIT
###############################################################################

import gdaltest
import pytest

from osgeo import gdal, ogr

pytestmark = pytest.mark.require_geos(3, 12)


@pytest.fixture()
def alg():
    return gdal.GetGlobalAlgorithmRegistry()["vector"]["simplify-coverage"]


def count_points(g):
    if type(g) is ogr.Layer:
        return sum(count_points(f) for f in g)

    if type(g) is ogr.Feature:
        return count_points(g.GetGeometryRef())

    if g.GetGeometryCount():
        return sum(count_points(subg) for subg in g)
    else:
        return g.GetPointCount()


def test_gdalalg_vector_simplify_coverage(alg):

    src_ds = gdal.OpenEx("../ogr/data/poly.shp", gdal.OF_VECTOR)
    src_lyr = src_ds.GetLayer(0)

    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "stream"
    alg["tolerance"] = 2

    assert alg.Run()

    dst_ds = alg["output"].GetDataset()
    dst_lyr = dst_ds.GetLayer(0)

    assert dst_lyr.GetFeatureCount() == src_lyr.GetFeatureCount()
    assert dst_lyr.GetSpatialRef().IsSame(src_lyr.GetSpatialRef())

    assert count_points(dst_lyr) < count_points(src_lyr)

    assert alg.Finalize()


def test_gdalalg_vector_simplify_coverage_with_progress(alg):

    src_ds = gdal.OpenEx("../ogr/data/poly.shp", gdal.OF_VECTOR)
    src_lyr = src_ds.GetLayer(0)

    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "stream"
    alg["tolerance"] = 2

    tab_pct = [0]

    def my_progress(pct, msg, user_data):
        assert pct >= tab_pct[0]
        tab_pct[0] = pct
        return True

    assert alg.Run(my_progress)

    assert tab_pct[0] == 1.0

    dst_ds = alg["output"].GetDataset()
    dst_lyr = dst_ds.GetLayer(0)

    assert dst_lyr.GetFeatureCount() == src_lyr.GetFeatureCount()
    assert dst_lyr.GetSpatialRef().IsSame(src_lyr.GetSpatialRef())

    assert count_points(dst_lyr) < count_points(src_lyr)

    assert alg.Finalize()


def test_gdalalg_vector_simplify_coverage_with_progress_but_src_has_no_fast_feature_count():

    tab_pct = [0]

    def my_progress(pct, msg, user_data):
        assert pct >= tab_pct[0]
        tab_pct[0] = pct
        return True

    assert gdal.Run(
        "vector",
        "pipeline",
        pipeline="read ../ogr/data/poly.shp ! explode-collections ! simplify-coverage --tolerance 2 ! write streamed_dataset --of=stream",
        progress=my_progress,
    )

    assert tab_pct[0] == 1.0


@pytest.mark.parametrize("stop_ratio", [0, 0.02, 0.1, 0.5, 0.9, 0.98, 1.0])
def test_gdalalg_vector_simplify_coverage_with_progress_interrupted(alg, stop_ratio):

    src_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src_lyr = src_ds.CreateLayer("test")
    for i in range(1000):
        f = ogr.Feature(src_lyr.GetLayerDefn())
        f.SetGeometryDirectly(
            ogr.CreateGeometryFromWkt(
                f"POLYGON (({i} 0,{i + 1} 0,{i + 1} 1,{i} 1,{i} 0))"
            )
        )
        src_lyr.CreateFeature(f)

    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "stream"
    alg["tolerance"] = 2

    tab_pct = [0]

    def my_progress(pct, msg, user_data):
        assert pct >= tab_pct[0]
        tab_pct[0] = pct
        return pct < stop_ratio

    with pytest.raises(Exception, match="Processing interrupted by user"):
        alg.Run(my_progress)


def test_gdalalg_vector_simplify_coverage_active_layer(alg):

    src_ds = gdal.GetDriverByName("MEM").CreateVector("")

    with gdal.OpenEx("../ogr/data/poly.shp", gdal.OF_VECTOR) as poly:
        src_ds.CopyLayer(poly.GetLayer(0), "poly1", options=["ADVERTIZE_UTF8=YES"])
        src_ds.CopyLayer(poly.GetLayer(0), "poly2", options=["ADVERTIZE_UTF8=YES"])

    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "stream"
    alg["tolerance"] = 2
    alg["active-layer"] = "bad"

    with pytest.raises(RuntimeError, match="layer .* was not found"):
        alg.Run()

    alg["active-layer"] = "poly2"
    assert alg.Run()

    dst_ds = alg["output"].GetDataset()

    assert dst_ds.GetLayerCount() == 2

    assert (
        dst_ds.GetLayerByName("poly1").GetFeatureCount()
        == dst_ds.GetLayerByName("poly2").GetFeatureCount()
    )

    assert count_points(dst_ds.GetLayerByName("poly2")) < count_points(
        dst_ds.GetLayerByName("poly1")
    )

    assert dst_ds.GetLayerByName("poly1").TestCapability(ogr.OLCStringsAsUTF8)

    assert alg.Finalize()


@pytest.mark.parametrize(
    "geom",
    (
        ogr.CreateGeometryFromWkt("POINT (3 8)"),
        ogr.CreateGeometryFromWkt("TIN (((0 0,0 1,1 1,0 0)))"),
        None,
    ),
)
def test_gdalalg_vector_simplify_coverage_non_polygonal_inputs(alg, geom, tmp_vsimem):

    src_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src_lyr = src_ds.CreateLayer("layer")
    src_lyr.CreateField(ogr.FieldDefn("a"))

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f["a"] = 1
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT (3 8)"))
    src_lyr.CreateFeature(f)

    alg["input"] = src_ds
    alg["output"] = tmp_vsimem / "out.shp"
    alg["tolerance"] = 2

    with pytest.raises(RuntimeError, match="can only be performed on polygonal"):
        assert alg.Run()


def test_gdalalg_vector_simplify_coverage_no_tolerance(alg):

    alg["input"] = "../ogr/data/poly.shp"
    alg["output"] = ""
    alg["output-format"] = "stream"

    with pytest.raises(RuntimeError, match="tolerance. has not been specified"):
        assert alg.Run()


@pytest.mark.parametrize("tol", (-5, float("nan")))
def test_gdalalg_vector_simplify_coverage_bad_tolerance(alg, tol):

    with pytest.raises(RuntimeError, match="should be >= 0"):
        alg["tolerance"] = tol


@pytest.mark.require_driver("GDALG")
def test_gdalalg_vector_simplify_coverage_test_ogrsf(tmp_path):

    import test_cli_utilities

    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    gdalg_filename = tmp_path / "tmp.gdalg.json"
    open(gdalg_filename, "wb").write(
        b'{"type": "gdal_streamed_alg","command_line": "gdal vector simplify-coverage --tolerance 10 ../ogr/data/poly.shp --output-format=stream dummy_dataset_name","relative_paths_relative_to_this_file":false}'
    )

    ret = gdaltest.runexternal(
        test_cli_utilities.get_test_ogrsf_path() + f" -ro {gdalg_filename}"
    )

    assert "INFO" in ret
    assert "ERROR" not in ret
    assert "FAILURE" not in ret
