#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal raster index' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import os

import gdaltest
import pytest

from osgeo import gdal, ogr


def get_alg():
    return gdal.GetGlobalAlgorithmRegistry()["raster"]["index"]


def test_gdalalg_raster_index_layer_must_be_specified():

    alg = get_alg()
    alg["input"] = "../gcore/data/byte.tif"
    alg["output"] = ""
    alg["output-format"] = "MEM"
    with pytest.raises(Exception, match="Argument 'layer' must be specified"):
        alg.Run()


def test_gdalalg_raster_index():

    last_pct = [0]

    def my_progress(pct, msg, user_data):
        last_pct[0] = pct
        return True

    alg = get_alg()
    alg["input"] = "../gcore/data/byte.tif"
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["output-layer"] = "my_layer"
    assert alg.Run(my_progress)
    assert last_pct[0] == 1.0
    ds = alg["output"].GetDataset()
    lyr = ds.GetLayerByName("my_layer")
    assert lyr.GetSpatialRef().GetAuthorityCode(None) == "26711"
    assert lyr.GetLayerDefn().GetFieldCount() == 1
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetName() == "location"
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetType() == ogr.OFTString
    f = lyr.GetNextFeature()
    assert f["location"] == "../gcore/data/byte.tif"
    assert (
        f.GetGeometryRef().ExportToWkt()
        == "POLYGON ((440720 3751320,441920 3751320,441920 3750120,440720 3750120,440720 3751320))"
    )


def test_gdalalg_raster_index_source_by_ref():

    alg = get_alg()
    alg["input"] = gdal.GetDriverByName("MEM").Create("", 1, 1)
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["output-layer"] = "my_layer"
    with pytest.raises(
        Exception, match="Input datasets must be provided by name, not as object"
    ):
        alg.Run()


def test_gdalalg_raster_index_overwrite(tmp_vsimem):

    out_filename = tmp_vsimem / "out.shp"

    alg = get_alg()
    alg["input"] = "../gcore/data/byte.tif"
    alg["output"] = out_filename
    assert alg.Run()
    ds = alg["output"].GetDataset()
    lyr = ds.GetLayer(0)
    assert lyr.GetSpatialRef().GetAuthorityCode(None) == "26711"
    assert lyr.GetFeatureCount() == 1
    assert alg.Finalize()
    ds.Close()

    alg = get_alg()
    alg["input"] = "../gcore/data/uint16.tif"
    alg["output"] = out_filename
    with pytest.raises(
        Exception,
        match="already exists",
    ):
        alg.Run()

    alg = get_alg()
    alg["input"] = "../gcore/data/uint16.tif"
    alg["output"] = out_filename
    alg["update"] = True
    with pytest.raises(
        Exception,
        match="Layer 'out' already exists. Specify the --overwrite-layer option to overwrite it, or --append to append to it.",
    ):
        alg.Run()

    alg = get_alg()
    alg["input"] = "../gcore/data/uint16.tif"
    alg["output"] = out_filename
    alg["append"] = True
    assert alg.Run()
    ds = alg["output"].GetDataset()
    lyr = ds.GetLayer(0)
    assert lyr.GetSpatialRef().GetAuthorityCode(None) == "26711"
    assert lyr.GetFeatureCount() == 2
    assert alg.Finalize()
    ds.Close()

    alg = get_alg()
    alg["input"] = "../gcore/data/byte.tif"
    alg["output"] = out_filename
    alg["overwrite-layer"] = True
    assert alg.Run()
    ds = alg["output"].GetDataset()
    lyr = ds.GetLayer(0)
    assert lyr.GetSpatialRef().GetAuthorityCode(None) == "26711"
    assert lyr.GetFeatureCount() == 1
    assert alg.Finalize()
    ds.Close()

    alg = get_alg()
    alg["input"] = "../gcore/data/uint16.tif"
    alg["output"] = out_filename
    alg["append"] = True
    assert alg.Run()
    ds = alg["output"].GetDataset()
    lyr = ds.GetLayer(0)
    assert lyr.GetSpatialRef().GetAuthorityCode(None) == "26711"
    assert lyr.GetFeatureCount() == 2
    assert alg.Finalize()
    ds.Close()

    alg = get_alg()
    alg["input"] = "../gcore/data/byte.tif"
    alg["output"] = out_filename
    alg["overwrite"] = True
    assert alg.Run()
    ds = alg["output"].GetDataset()
    lyr = ds.GetLayer(0)
    assert lyr.GetSpatialRef().GetAuthorityCode(None) == "26711"
    assert lyr.GetFeatureCount() == 1
    assert alg.Finalize()
    ds.Close()


def test_gdalalg_raster_index_recursive_filter_absolute_path_location_name():

    alg = get_alg()
    alg["input"] = "../gcore/data"
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["output-layer"] = "out"
    alg["recursive"] = True
    alg["filename-filter"] = "byt?.tif"
    alg["absolute-path"] = True
    alg["location-name"] = "path"
    assert alg.Run()
    ds = alg["output"].GetDataset()
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetFieldCount() == 1
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetName() == "path"
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetType() == ogr.OFTString
    f = lyr.GetNextFeature()
    assert "byte.tif" in f["path"]
    assert not f["path"].startswith("../gcore")
    assert os.path.exists(f["path"])


def test_gdalalg_raster_index_metadata():

    alg = get_alg()
    alg["input"] = "../gcore/data/byte.tif"
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["output-layer"] = "out"
    alg["metadata"] = {"foo": "bar"}
    alg["filename-filter"] = "byte.tif"
    assert alg.Run()
    ds = alg["output"].GetDataset()
    lyr = ds.GetLayer(0)
    assert lyr.GetMetadataItem("foo") == "bar"


@pytest.mark.parametrize("min_pixel_size,expected_count", [(61, 0), (59, 1)])
def test_gdalalg_raster_index_min_pixel_size(min_pixel_size, expected_count):

    alg = get_alg()
    alg["input"] = "../gcore/data/byte.tif"
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["output-layer"] = "out"
    alg["min-pixel-size"] = min_pixel_size
    with gdaltest.error_raised(
        gdal.CE_Warning if min_pixel_size == 61 else gdal.CE_None
    ):
        assert alg.Run()
    ds = alg["output"].GetDataset()
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == expected_count


def test_gdalalg_raster_index_crs():

    alg = get_alg()
    alg["input"] = "../gcore/data/byte.tif"
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["output-layer"] = "out"
    alg["dst-crs"] = "EPSG:4267"
    alg["source-crs-field-name"] = "source_crs"
    assert alg.Run()
    ds = alg["output"].GetDataset()
    lyr = ds.GetLayer(0)
    assert lyr.GetSpatialRef().GetAuthorityCode(None) == "4267"
    f = lyr.GetNextFeature()
    assert f["source_crs"] == "EPSG:26711"
    assert (
        f.GetGeometryRef().ExportToWkt()
        == "POLYGON ((-117.641168620797 33.9023526904272,-117.628190189534 33.9024195619211,-117.628110837847 33.8915970129623,-117.641087629972 33.8915301685907,-117.641168620797 33.9023526904272))"
    )


def test_gdalalg_raster_error():

    alg = get_alg()
    alg["input"] = "/i/do/not/exist"
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["output-layer"] = "out"
    alg["dst-crs"] = "EPSG:4267"
    with pytest.raises(Exception, match="Unable to open /i/do/not/exist"):
        alg.Run()


def test_gdalalg_raster_skip_errors_with_crs():

    alg = get_alg()
    alg["input"] = "/i/do/not/exist"
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["output-layer"] = "out"
    alg["dst-crs"] = "EPSG:4267"
    alg["skip-errors"] = True
    with gdaltest.error_raised(gdal.CE_Warning):
        assert alg.Run()
    ds = alg["output"].GetDataset()
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 0


def test_gdalalg_raster_skip_errors_without_crs():

    alg = get_alg()
    alg["input"] = "/i/do/not/exist"
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["output-layer"] = "out"
    alg["skip-errors"] = True
    with gdaltest.error_raised(gdal.CE_Warning):
        assert alg.Run()
    ds = alg["output"].GetDataset()
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 0
