#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal raster footprint' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import pytest

from osgeo import gdal


def get_alg():
    return gdal.GetGlobalAlgorithmRegistry()["raster"]["footprint"]


def test_gdalalg_raster_footprint():

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
    lyr = ds.GetLayerByName("footprint")
    assert lyr.GetSpatialRef().GetAuthorityCode(None) == "26711"
    assert lyr.GetFeatureCount() == 1
    f = lyr.GetNextFeature()
    assert f["location"] == "../gcore/data/byte.tif"
    assert (
        f.GetGeometryRef().ExportToWkt()
        == "MULTIPOLYGON (((440720 3751320,440720 3750120,441920 3750120,441920 3751320,440720 3751320)))"
    )


def test_gdalalg_raster_footprint_existing_output(tmp_vsimem):

    out_filename = tmp_vsimem / "out.shp"

    alg = get_alg()
    alg["input"] = "../gcore/data/byte.tif"
    alg["output"] = out_filename
    assert alg.Run()
    assert alg.Finalize()

    alg = get_alg()
    alg["input"] = "../gcore/data/byte.tif"
    alg["output"] = out_filename
    with pytest.raises(
        Exception,
        match="already exists",
    ):
        alg.Run()

    alg = get_alg()
    alg["input"] = "../gcore/data/byte.tif"
    alg["output"] = out_filename
    alg["append"] = True
    assert alg.Run()
    ds = alg["output"].GetDataset()
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 2
    assert alg.Finalize()
    ds.Close()

    alg = get_alg()
    alg["input"] = "../gcore/data/byte.tif"
    out_ds = gdal.OpenEx(out_filename, gdal.OF_UPDATE)
    alg["output"] = out_ds
    alg["append"] = True
    assert alg.Run()
    lyr = out_ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 3
    assert alg.Finalize()
    out_ds.Close()

    alg = get_alg()
    alg["input"] = "../gcore/data/byte.tif"
    alg["output"] = out_filename
    alg["overwrite"] = True
    assert alg.Run()
    ds = alg["output"].GetDataset()
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 1


def test_gdalalg_raster_footprint_output_layer():

    alg = get_alg()
    alg["input"] = "../gcore/data/byte.tif"
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["output-layer"] = "foo"
    assert alg.Run()
    ds = alg["output"].GetDataset()
    assert ds.GetLayerByName("foo")


@pytest.mark.require_driver("GPKG")
def test_gdalalg_raster_footprint_creation_options(tmp_vsimem):

    out_filename = tmp_vsimem / "out.gpkg"

    alg = get_alg()
    alg["input"] = "../gcore/data/byte.tif"
    alg["output"] = out_filename
    alg["creation-option"] = {"METADATA_TABLES": "YES"}
    alg["layer-creation-option"] = {"DESCRIPTION": "my_description"}
    assert alg.Run()
    assert alg.Finalize()
    with gdal.OpenEx(out_filename, gdal.OF_VECTOR) as ds:
        assert ds.GetLayer(0).GetMetadata() == {"DESCRIPTION": "my_description"}
        with ds.ExecuteSQL(
            "SELECT * FROM sqlite_master WHERE name LIKE '%metadata%'"
        ) as sql_lyr:
            assert sql_lyr.GetFeatureCount() == 2


def test_gdalalg_raster_footprint_band():

    src_ds = gdal.GetDriverByName("MEM").Create("", 4, 1, 3)
    src_ds.GetRasterBand(1).SetNoDataValue(0)
    src_ds.GetRasterBand(2).WriteRaster(0, 0, 2, 1, b"\x01\x01")
    src_ds.GetRasterBand(2).SetNoDataValue(0)
    src_ds.GetRasterBand(3).WriteRaster(1, 0, 2, 1, b"\x01\x01")
    src_ds.GetRasterBand(3).SetNoDataValue(0)

    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["output-layer"] = "foo"
    alg["band"] = 1
    assert alg.Run()
    ds = alg["output"].GetDataset()
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f is None

    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["output-layer"] = "foo"
    alg["band"] = 2
    assert alg.Run()
    ds = alg["output"].GetDataset()
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f.GetGeometryRef().ExportToWkt() == "MULTIPOLYGON (((0 0,0 1,2 1,2 0,0 0)))"

    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["output-layer"] = "foo"
    alg["band"] = [2, 3]
    assert alg.Run()
    ds = alg["output"].GetDataset()
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f.GetGeometryRef().ExportToWkt() == "MULTIPOLYGON (((0 0,0 1,3 1,3 0,0 0)))"

    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["output-layer"] = "foo"
    alg["band"] = [2, 3]
    alg["combine-bands"] = "intersection"
    assert alg.Run()
    ds = alg["output"].GetDataset()
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f.GetGeometryRef().ExportToWkt() == "MULTIPOLYGON (((1 0,1 1,2 1,2 0,1 0)))"


def test_gdalalg_raster_footprint_overview():

    src_ds = gdal.GetDriverByName("MEM").Create("", 2, 2)
    src_ds.GetRasterBand(1).Fill(0)
    src_ds.GetRasterBand(1).SetNoDataValue(0)

    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["overview"] = 0
    with pytest.raises(
        Exception,
        match="Source dataset has no overviews. Argument 'overview' should not be specified",
    ):
        alg.Run()

    src_ds.BuildOverviews("NONE", [2])
    src_ds.GetRasterBand(1).GetOverview(0).Fill(255)

    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["overview"] = 0
    assert alg.Run()
    ds = alg["output"].GetDataset()
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f.GetGeometryRef().ExportToWkt() == "MULTIPOLYGON (((0 0,0 2,2 2,2 0,0 0)))"

    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["overview"] = 1
    with pytest.raises(
        Exception,
        match="Source dataset has only 1 overview levels. 'overview' value should be strictly lower than this number",
    ):
        alg.Run()


def test_gdalalg_raster_footprint_srcnodata():

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    src_ds.GetRasterBand(1).Fill(255)

    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["src-nodata"] = 255
    assert alg.Run()
    ds = alg["output"].GetDataset()
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f is None


@pytest.mark.parametrize("use_setnodatavalue", [True, False])
def test_gdalalg_raster_footprint_srcnodata_several(use_setnodatavalue):

    src_ds = gdal.GetDriverByName("MEM").Create("", 2, 1, 2)
    src_ds.GetRasterBand(1).WriteRaster(0, 0, 1, 1, b"\x01")
    if use_setnodatavalue:
        src_ds.GetRasterBand(1).SetNoDataValue(0)
    src_ds.GetRasterBand(2).WriteRaster(0, 0, 1, 1, b"\x01")
    if use_setnodatavalue:
        src_ds.GetRasterBand(2).SetNoDataValue(1)

    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "MEM"
    if not use_setnodatavalue:
        alg["src-nodata"] = [0, 1]
    assert alg.Run()
    ds = alg["output"].GetDataset()
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f.GetGeometryRef().ExportToWkt() == "MULTIPOLYGON (((0 0,0 1,2 1,2 0,0 0)))"


def test_gdalalg_raster_footprint_coordinate_system():

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    src_ds.SetGeoTransform([2, 1, 0, 49, 0, -1])
    src_ds.GetRasterBand(1).Fill(255)

    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "MEM"
    assert alg.Run()
    ds = alg["output"].GetDataset()
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert (
        f.GetGeometryRef().ExportToWkt()
        == "MULTIPOLYGON (((2 49,2 48,3 48,3 49,2 49)))"
    )

    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["coordinate-system"] = "pixel"
    assert alg.Run()
    ds = alg["output"].GetDataset()
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f.GetGeometryRef().ExportToWkt() == "MULTIPOLYGON (((0 0,0 1,1 1,1 0,0 0)))"

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    src_ds.GetRasterBand(1).Fill(255)
    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["coordinate-system"] = "georeferenced"
    with pytest.raises(
        Exception,
        match="Georeferenced coordinates requested, but input dataset has no geotransform",
    ):
        alg.Run()


def test_gdalalg_raster_dst_crs():

    alg = get_alg()
    alg["input"] = "../gcore/data/byte.tif"
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["dst-crs"] = "EPSG:4267"
    assert alg.Run()
    ds = alg["output"].GetDataset()
    lyr = ds.GetLayer(0)
    assert lyr.GetSpatialRef().GetAuthorityCode(None) == "4267"
    f = lyr.GetNextFeature()
    assert f.GetGeometryRef().ExportToWkt().startswith("MULTIPOLYGON (((-117.6411")


def test_gdalalg_raster_footprint_split_multipolygons():

    src_ds = gdal.GetDriverByName("MEM").Create("", 2, 2)
    src_ds.GetRasterBand(1).WriteRaster(0, 0, 2, 2, b"\x01\x00\x00\x01")
    src_ds.GetRasterBand(1).SetNoDataValue(0)

    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "MEM"
    assert alg.Run()
    ds = alg["output"].GetDataset()
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert (
        f.GetGeometryRef().ExportToWkt()
        == "MULTIPOLYGON (((0 0,0 1,1 1,1 0,0 0)),((1 1,1 2,2 2,2 1,1 1)))"
    )

    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["split-multipolygons"] = True
    assert alg.Run()
    ds = alg["output"].GetDataset()
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f.GetGeometryRef().ExportToWkt() == "POLYGON ((0 0,0 1,1 1,1 0,0 0))"
    f = lyr.GetNextFeature()
    assert f.GetGeometryRef().ExportToWkt() == "POLYGON ((1 1,1 2,2 2,2 1,1 1))"


@pytest.mark.require_geos()
def test_gdalalg_raster_footprint_convex_hull():

    src_ds = gdal.GetDriverByName("MEM").Create("", 2, 2)
    src_ds.GetRasterBand(1).WriteRaster(0, 0, 2, 2, b"\x01\x00\x00\x01")
    src_ds.GetRasterBand(1).SetNoDataValue(0)

    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["convex-hull"] = True
    assert alg.Run()
    ds = alg["output"].GetDataset()
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert (
        f.GetGeometryRef().ExportToWkt()
        == "MULTIPOLYGON (((0 0,0 1,1 2,2 2,2 1,1 0,0 0)))"
    )


def test_gdalalg_raster_footprint_densify():

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    src_ds.GetRasterBand(1).Fill(1)

    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["densify-distance"] = 0.5
    assert alg.Run()
    ds = alg["output"].GetDataset()
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert (
        f.GetGeometryRef().ExportToWkt()
        == "MULTIPOLYGON (((0 0,0.0 0.5,0 1,0.5 1.0,1 1,1.0 0.5,1 0,0.5 0.0,0 0)))"
    )


@pytest.mark.require_geos()
def test_gdalalg_raster_footprint_simplify():

    src_ds = gdal.GetDriverByName("MEM").Create("", 2, 2)
    src_ds.GetRasterBand(1).WriteRaster(0, 0, 2, 2, b"\x01\x01\x00\x01")
    src_ds.GetRasterBand(1).SetNoDataValue(0)

    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["simplify-tolerance"] = 1
    assert alg.Run()
    ds = alg["output"].GetDataset()
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f.GetGeometryRef().ExportToWkt() == "MULTIPOLYGON (((0 0,2 2,2 0,0 0)))"


def test_gdalalg_raster_footprint_min_ring_area():

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    src_ds.GetRasterBand(1).Fill(1)

    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["min-ring-area"] = 1.1
    assert alg.Run()
    ds = alg["output"].GetDataset()
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f is None

    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["min-ring-area"] = 0.9
    assert alg.Run()
    ds = alg["output"].GetDataset()
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f.GetGeometryRef().ExportToWkt() == "MULTIPOLYGON (((0 0,0 1,1 1,1 0,0 0)))"


@pytest.mark.require_geos()
def test_gdalalg_raster_footprint_max_points():

    src_ds = gdal.GetDriverByName("MEM").Create("", 10, 9)
    src_ds.GetRasterBand(1).SetNoDataValue(0)
    for i in range(9):
        src_ds.GetRasterBand(1).WriteRaster(i, i, 2, 1, b"\x01\x01")

    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["max-points"] = 30
    assert alg.Run()
    ds = alg["output"].GetDataset()
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f.GetGeometryRef().ExportToWkt() == "MULTIPOLYGON (((0 0,8 9,10 9,2 0,0 0)))"

    alg = get_alg()
    with pytest.raises(
        Exception,
        match="Value of 'max-points' should be a positive integer greater or equal to 4, or 'unlimited'",
    ):
        alg["max-points"] = "illegal"


def test_gdalalg_raster_footprint_location_field():

    src_ds = gdal.GetDriverByName("MEM").Create("bar", 1, 1)
    src_ds.GetRasterBand(1).Fill(1)

    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["location-field"] = "foo"
    assert alg.Run()
    ds = alg["output"].GetDataset()
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f["foo"] == "bar"


def test_gdalalg_raster_footprint_no_location_field():

    src_ds = gdal.GetDriverByName("MEM").Create("bar", 1, 1)
    src_ds.GetRasterBand(1).Fill(1)

    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["no-location-field"] = True
    assert alg.Run()
    ds = alg["output"].GetDataset()
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetFieldCount() == 0


def test_gdalalg_raster_footprint_absolute_path():

    alg = get_alg()
    alg["input"] = "../gcore/data/byte.tif"
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["absolute-path"] = True
    assert alg.Run()
    ds = alg["output"].GetDataset()
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert "byte.tif" in f["location"]
    assert f["location"] != "../gcore/data/byte.tif"
