#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal raster polygonize' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import pytest

from osgeo import gdal, ogr


def get_alg():
    return gdal.GetGlobalAlgorithmRegistry()["raster"]["polygonize"]


def test_gdalalg_raster_polygonize():

    last_pct = [0]

    def my_progress(pct, msg, user_data):
        last_pct[0] = pct
        return True

    alg = get_alg()
    alg["input"] = "../gcore/data/byte.tif"
    alg["output"] = ""
    alg["output-format"] = "MEM"
    assert alg.Run(my_progress)
    assert last_pct[0] == 1.0
    ds = alg["output"].GetDataset()
    lyr = ds.GetLayerByName("polygonize")
    assert lyr.GetSpatialRef().GetAuthorityCode(None) == "26711"
    assert lyr.GetFeatureCount() == 281
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetName() == "DN"
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetType() == ogr.OFTInteger
    f = lyr.GetNextFeature()
    assert f["DN"] == 107
    assert (
        f.GetGeometryRef().ExportToWkt()
        == "POLYGON ((440720 3751320,440720 3751260,440780 3751260,440780 3751320,440720 3751320))"
    )


def test_gdalalg_raster_polygonize_invalid_driver():

    alg = get_alg()
    alg["input"] = "../gcore/data/byte.tif"
    alg["output"] = ""
    with pytest.raises(
        Exception,
        match="Invalid value for argument 'output-format'. Driver 'invalid' does not exist",
    ):
        alg["output-format"] = "invalid"


def test_gdalalg_raster_polygonize_cannot_guess_driver():

    alg = get_alg()
    alg["input"] = "../gcore/data/byte.tif"
    alg["output"] = "/vsimem/invalid.invalid"
    with pytest.raises(
        Exception, match="Cannot guess driver for /vsimem/invalid.invalid"
    ):
        alg.Run()


@pytest.mark.require_driver("GPKG")
def test_gdalalg_raster_polygonize_cannot_create_dataset(tmp_path):

    alg = get_alg()
    alg["input"] = "../gcore/data/byte.tif"
    alg["output"] = tmp_path / "i_do" / "not" / "exist.gpkg"
    with pytest.raises(Exception, match="unable to open database file"):
        alg.Run()


def test_gdalalg_raster_polygonize_cannot_create_layer(tmp_path):

    alg = get_alg()
    alg["input"] = "../gcore/data/byte.tif"
    alg["output"] = tmp_path / "i_do" / "not" / "exist.shp"
    with pytest.raises(Exception, match="Cannot create layer 'exist'"):
        alg.Run()


def test_gdalalg_raster_polygonize_overwrite(tmp_vsimem):

    out_filename = tmp_vsimem / "out.shp"

    alg = get_alg()
    alg["input"] = "../gcore/data/byte.tif"
    alg["output"] = out_filename
    assert alg.Run()
    ds = alg["output"].GetDataset()
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 281
    assert alg.Finalize()
    ds.Close()

    with ogr.Open(out_filename) as ds:
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 281

    alg = get_alg()
    alg["input"] = "../gcore/data/byte.tif"
    alg["output"] = out_filename
    with pytest.raises(
        Exception,
        match="already exists",
    ):
        alg.Run()
    with ogr.Open(out_filename) as ds:
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 281

    alg = get_alg()
    alg["input"] = "../gcore/data/byte.tif"
    alg["output"] = out_filename
    alg["update"] = True
    with pytest.raises(
        Exception,
        match="Layer 'out' already exists. Specify the --overwrite-layer option to overwrite it, or --append to append to it",
    ):
        alg.Run()
    with ogr.Open(out_filename) as ds:
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 281

    alg = get_alg()
    alg["input"] = "../gcore/data/byte.tif"
    alg["output"] = out_filename
    alg["append"] = True
    assert alg.Run()
    ds = alg["output"].GetDataset()
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 281 * 2
    assert alg.Finalize()
    ds.Close()

    alg = get_alg()
    alg["input"] = "../gcore/data/byte.tif"
    alg["output"] = out_filename
    alg["overwrite-layer"] = True
    assert alg.Run()
    ds = alg["output"].GetDataset()
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 281
    assert alg.Finalize()
    ds.Close()


def test_gdalalg_raster_polygonize_cannot_find_layer():

    out_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)

    alg = get_alg()
    alg["input"] = "../gcore/data/byte.tif"
    alg["output"] = out_ds
    alg["append"] = True
    with pytest.raises(Exception, match="Cannot find layer 'polygonize'"):
        alg.Run()


def test_gdalalg_raster_polygonize_cannot_find_field():

    out_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    out_ds.CreateLayer("polygonize")

    alg = get_alg()
    alg["input"] = "../gcore/data/byte.tif"
    alg["output"] = out_ds
    alg["append"] = True
    with pytest.raises(Exception, match="Cannot find field 'DN' in layer 'polygonize'"):
        alg.Run()


def test_gdalalg_raster_polygonize_layer_and_field_name():

    alg = get_alg()
    alg["input"] = "../gcore/data/byte.tif"
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["output-layer"] = "foo"
    alg["attribute-name"] = "bar"
    assert alg.Run()
    ds = alg["output"].GetDataset()
    lyr = ds.GetLayerByName("foo")
    assert lyr.GetFeatureCount() == 281
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetName() == "bar"


def test_gdalalg_raster_polygonize_float32():

    alg = get_alg()
    alg["input"] = "../gcore/data/float32.tif"
    alg["output"] = ""
    alg["output-format"] = "MEM"
    assert alg.Run()
    ds = alg["output"].GetDataset()
    lyr = ds.GetLayerByName("polygonize")
    assert lyr.GetFeatureCount() == 281
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetName() == "DN"
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetType() == ogr.OFTReal


def test_gdalalg_raster_polygonize_int64():

    alg = get_alg()
    alg["input"] = "../gcore/data/int64.tif"
    alg["output"] = ""
    alg["output-format"] = "MEM"
    assert alg.Run()
    ds = alg["output"].GetDataset()
    lyr = ds.GetLayerByName("polygonize")
    assert lyr.GetFeatureCount() == 281
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetName() == "DN"
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetType() == ogr.OFTInteger64


def test_gdalalg_raster_polygonize_connect_diagonal_pixels():

    alg = get_alg()
    alg["input"] = "../gcore/data/byte.tif"
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["connect-diagonal-pixels"] = True
    assert alg.Run()
    ds = alg["output"].GetDataset()
    lyr = ds.GetLayerByName("polygonize")
    assert lyr.GetFeatureCount() == 229


@pytest.mark.require_driver("GPKG")
def test_gdalalg_raster_polygonize_creation_options(tmp_vsimem):

    out_filename = tmp_vsimem / "out.gpkg"

    alg = get_alg()
    alg["input"] = "../gcore/data/byte.tif"
    alg["output"] = out_filename
    alg["creation-option"] = {"METADATA_TABLES": "YES"}
    alg["layer-creation-option"] = {"DESCRIPTION": "my_desc"}
    assert alg.Run()
    assert alg.Finalize()
    with ogr.Open(out_filename) as ds:
        with ds.ExecuteSQL(
            "SELECT * FROM sqlite_master WHERE name LIKE '%metadata%'"
        ) as sql_lyr:
            assert sql_lyr.GetFeatureCount() == 2
        assert ds.GetLayer(0).GetMetadata_Dict() == {"DESCRIPTION": "my_desc"}
        assert ds.GetLayer(0).GetFeatureCount() == 281


@pytest.mark.require_driver("GPKG")
@pytest.mark.parametrize("commit_interval", [-1, 0, 10])
def test_gdalalg_raster_polygonize_commit_interval(tmp_vsimem, commit_interval):

    out_filename = tmp_vsimem / "out.gpkg"

    alg = get_alg()
    alg["input"] = "../gcore/data/byte.tif"
    alg["output"] = out_filename
    alg["commit-interval"] = commit_interval
    assert alg.Run()
    assert alg.Finalize()
    with ogr.Open(out_filename) as ds:
        assert ds.GetLayer(0).GetFeatureCount() == 281


@pytest.mark.require_driver("GPKG")
def test_gdalalg_raster_polygonize_creation_options_in_pipeline(tmp_vsimem):

    out_filename = tmp_vsimem / "out.gpkg"

    gdal.Run(
        "pipeline",
        pipeline=f"read ../gcore/data/byte.tif ! polygonize ! write --creation-option METADATA_TABLES=YES --layer-creation-option DESCRIPTION=my_desc {out_filename}",
    )

    with ogr.Open(out_filename) as ds:
        with ds.ExecuteSQL(
            "SELECT * FROM sqlite_master WHERE name LIKE '%metadata%'"
        ) as sql_lyr:
            assert sql_lyr.GetFeatureCount() == 2
        assert ds.GetLayer(0).GetMetadata_Dict() == {"DESCRIPTION": "my_desc"}
        assert ds.GetLayer(0).GetFeatureCount() == 281


def test_gdalalg_raster_polygonize_no_next_usable_step_in_pipeline(tmp_vsimem):

    out_filename = tmp_vsimem / "out.shp"

    gdal.Run(
        "pipeline",
        pipeline=f"read ../gcore/data/byte.tif ! polygonize ! edit ! write {out_filename}",
    )

    with ogr.Open(out_filename) as ds:
        assert ds.GetLayer(0).GetFeatureCount() == 281


def test_gdalalg_raster_polygonize_pipeline_output_layer(tmp_vsimem):

    with gdal.alg.pipeline(
        pipeline="read ../gcore/data/byte.tif ! polygonize --output-layer foo"
    ) as alg:
        ds = alg.Output()
        assert ds.GetLayer(0).GetName() == "foo"
