#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal raster as-features' testing
# Author:   Daniel Baston
#
###############################################################################
# Copyright (c) 2025, ISciences LLC
#
# SPDX-License-Identifier: MIT
###############################################################################

import pytest

from osgeo import gdal, ogr


@pytest.fixture()
def alg():
    reg = gdal.GetGlobalAlgorithmRegistry()
    raster = reg.InstantiateAlg("raster")
    return raster.InstantiateSubAlgorithm("as-features")


@pytest.mark.require_driver("GPKG")
def test_gdalalg_raster_as_features_multiple_bands(alg, tmp_vsimem):

    src_fname = "../gcore/data/rgbsmall.tif"

    alg["input"] = src_fname
    alg["output"] = tmp_vsimem / "out.gpkg"
    alg["geometry-type"] = "point"

    tab_pct = [0]

    def my_progress(pct, msg, user_data):
        assert pct >= tab_pct[0]
        tab_pct[0] = pct
        return True

    assert alg.Run(my_progress)
    assert alg.Finalize()
    assert tab_pct[0] == 1

    with gdal.OpenEx(tmp_vsimem / "out.gpkg", gdal.OF_VECTOR) as ds:
        assert ds.GetLayerCount() == 1
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 2500

        f = lyr.GetFeature(2317)

        assert f["BAND_1"] == 106
        assert f["BAND_2"] == 156
        assert f["BAND_3"] == 43

        with gdal.Open(src_fname) as src_ds:
            assert src_ds.GetSpatialRef().IsSame(lyr.GetSpatialRef())


@pytest.mark.require_driver("GPKG")
def test_gdalalg_raster_as_features_specified_bands(alg, tmp_vsimem):

    alg["input"] = "../gcore/data/rgbsmall.tif"
    alg["output"] = tmp_vsimem / "out.gpkg"
    alg["band"] = [3, 2]

    assert alg.Run()
    assert alg.Finalize()

    with gdal.OpenEx(tmp_vsimem / "out.gpkg", gdal.OF_VECTOR) as ds:
        lyr = ds.GetLayer(0)

        f = lyr.GetFeature(2317)

        assert "BAND_1" not in f
        assert f["BAND_2"] == 156
        assert f["BAND_3"] == 43


@pytest.mark.require_driver("GPKG")
def test_gdalalg_raster_as_features_skip_nodata(alg, tmp_vsimem):

    alg["input"] = "../gcore/data/nodata_byte.tif"
    alg["output"] = tmp_vsimem / "out.gpkg"
    alg["skip-nodata"] = True

    assert alg.Run()
    assert alg.Finalize()

    with gdal.OpenEx(tmp_vsimem / "out.gpkg", gdal.OF_VECTOR) as ds:
        assert ds.GetLayerCount() == 1
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 380


@pytest.mark.require_driver("GPKG")
@pytest.mark.parametrize("geom_type", ("Point", "Polygon", "None"))
def test_gdalalg_raster_as_features_geom_type(alg, tmp_path, geom_type):

    alg["input"] = "../gcore/data/byte.tif"
    alg["output"] = tmp_path / "out.gpkg"
    alg["geometry-type"] = geom_type

    assert alg.Run()
    assert alg.Finalize()

    with gdal.OpenEx(tmp_path / "out.gpkg", gdal.OF_VECTOR) as ds:
        assert ds.GetLayerCount() == 1
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 400
        assert ogr.GeometryTypeToName(lyr.GetGeomType()) == geom_type

        f = lyr.GetFeature(239)
        if geom_type == "Point":
            assert f.GetGeometryRef().ExportToWkt() == "POINT (441830 3750630)"
        elif geom_type == "Polygon":
            assert f.GetGeometryRef().Equals(
                ogr.CreateGeometryFromWkt(
                    "POLYGON ((441800 3750660,441800 3750600,441860 3750600,441860 3750660,441800 3750660))"
                )
            )
        else:
            assert f.GetGeometryRef() is None

        assert f["BAND_1"] == 123


@pytest.mark.parametrize("include_xy", (True, False))
def test_gdalalg_raster_as_features_include_xy(alg, tmp_vsimem, include_xy):

    alg["input"] = "../gcore/data/byte.tif"
    alg["output"] = tmp_vsimem / "out.shp"
    alg["include-xy"] = include_xy

    assert alg.Run()

    assert alg["output"]
    ds = alg["output"].GetDataset()
    lyr = ds.GetLayer(0)

    f = lyr.GetFeature(239)
    if include_xy:
        assert f["CENTER_X"] == 441890
        assert f["CENTER_Y"] == 3750630
    else:
        assert "CENTER_X" not in f
        assert "CENTER_Y" not in f


@pytest.mark.parametrize("include_row_col", (True, False))
def test_gdalalg_raster_as_features_include_row_col(alg, tmp_vsimem, include_row_col):

    alg["input"] = "../gcore/data/byte.tif"
    alg["output"] = tmp_vsimem / "out.shp"
    alg["include-row-col"] = include_row_col

    assert alg.Run()

    assert alg["output"]
    ds = alg["output"].GetDataset()
    lyr = ds.GetLayer(0)

    f = lyr.GetFeature(239)
    if include_row_col:
        assert f["ROW"] == 11
        assert f["COL"] == 19
    else:
        assert "ROW" not in f
        assert "COL" not in f


def test_gdalalg_raster_as_features_geom_type_invalid(alg):

    with pytest.raises(Exception, match="Invalid value .* 'geometry-type'"):
        alg["geometry-type"] = "LineString"


def test_gdalalg_raster_as_features_layer_name(alg, tmp_vsimem):

    alg["input"] = "../gcore/data/byte.tif"
    alg["output-format"] = "stream"
    alg["output-layer"] = "layer123"

    assert alg.Run()

    assert alg["output"]
    ds = alg["output"].GetDataset()
    lyr = ds.GetLayer("layer123")
    assert lyr is not None
    assert lyr.GetLayerDefn().GetName() == "layer123"


def test_gdalalg_raster_as_features_layer_name_pipeline(alg, tmp_vsimem):

    with gdal.alg.pipeline(
        pipeline="read ../gcore/data/byte.tif ! as-features --output-layer layer123"
    ) as alg:
        ds = alg["output"].GetDataset()
        lyr = ds.GetLayer("layer123")
        assert lyr is not None
        assert lyr.GetLayerDefn().GetName() == "layer123"


def test_gdalalg_raster_as_features_zero_bands(alg, tmp_vsimem):

    src_ds = gdal.GetDriverByName("MEM").Create("", 2, 2, 0)

    alg["input"] = src_ds
    alg["output"] = tmp_vsimem / "out.shp"
    alg["include-row-col"] = True

    assert alg.Run()

    assert alg["output"]
    ds = alg["output"].GetDataset()
    lyr = ds.GetLayer(0)

    assert lyr.GetFeatureCount() == 4

    features = [f for f in lyr]

    assert (features[0]["ROW"], features[0]["COL"]) == (0, 0)
    assert (features[1]["ROW"], features[1]["COL"]) == (0, 1)
    assert (features[2]["ROW"], features[2]["COL"]) == (1, 0)
    assert (features[3]["ROW"], features[3]["COL"]) == (1, 1)


def test_gdalalg_raster_as_features_zero_height(alg, tmp_vsimem):

    src_ds = gdal.GetDriverByName("MEM").Create("", 2, 0, 1)

    alg["input"] = src_ds
    alg["output"] = tmp_vsimem / "out.shp"
    alg["include-row-col"] = True

    assert alg.Run()

    assert alg["output"]
    ds = alg["output"].GetDataset()
    lyr = ds.GetLayer(0)

    assert lyr.GetFeatureCount() == 0


def test_gdalalg_raster_as_features_zero_width(alg, tmp_vsimem):

    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 2, 1)

    alg["input"] = src_ds
    alg["output"] = tmp_vsimem / "out.shp"
    alg["include-row-col"] = True

    assert alg.Run()

    assert alg["output"]
    ds = alg["output"].GetDataset()
    lyr = ds.GetLayer(0)

    assert lyr.GetFeatureCount() == 0
