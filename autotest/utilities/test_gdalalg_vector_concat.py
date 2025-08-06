#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal vector concat' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import gdaltest
import ogrtest
import pytest

from osgeo import gdal, ogr, osr


def get_alg():
    return gdal.GetGlobalAlgorithmRegistry()["vector"]["concat"]


def get_pipeline_alg():
    return gdal.GetGlobalAlgorithmRegistry()["vector"]["pipeline"]


@pytest.mark.parametrize("GDAL_VECTOR_CONCAT_MAX_OPENED_DATASETS", ["1", None])
def test_gdalalg_vector_concat(GDAL_VECTOR_CONCAT_MAX_OPENED_DATASETS):

    last_pct = [0]

    def my_progress(pct, msg, user_data):
        last_pct[0] = pct
        return True

    alg = get_alg()
    alg["input"] = ["../ogr/data/poly.shp", "../ogr/data/poly.shp"]
    alg["output"] = ""
    alg["output-format"] = "MEM"
    with gdal.config_option(
        "GDAL_VECTOR_CONCAT_MAX_OPENED_DATASETS", GDAL_VECTOR_CONCAT_MAX_OPENED_DATASETS
    ):
        assert alg.Run(my_progress)
    assert last_pct[0] == 1.0
    ds = alg["output"].GetDataset()
    assert ds.GetLayerCount() == 1
    assert ds.GetLayer(-1) is None
    assert ds.GetLayer(1) is None
    lyr = ds.GetLayerByName("poly")
    assert lyr.GetSpatialRef().GetAuthorityCode(None) == "27700"
    assert len(lyr) == 20
    f = lyr.GetNextFeature()
    assert lyr.GetLayerDefn().GetFieldCount() == 3
    assert f["AREA"] == 215229.266
    assert f["EAS_ID"] == 168
    assert f["PRFEDEA"] == "35043411"
    assert (
        f.GetGeometryRef().ExportToWkt()
        == "POLYGON ((479819.84375 4765180.5,479690.1875 4765259.5,479647.0 4765369.5,479730.375 4765400.5,480039.03125 4765539.5,480035.34375 4765558.5,480159.78125 4765610.5,480202.28125 4765482.0,480365.0 4765015.5,480389.6875 4764950.0,480133.96875 4764856.5,480080.28125 4764979.5,480082.96875 4765049.5,480088.8125 4765139.5,480059.90625 4765239.5,480019.71875 4765319.5,479980.21875 4765409.5,479909.875 4765370.0,479859.875 4765270.0,479819.84375 4765180.5))"
    )


def test_gdalalg_vector_concat_pipeline():

    last_pct = [0]

    def my_progress(pct, msg, user_data):
        last_pct[0] = pct
        return True

    alg = get_pipeline_alg()
    alg["input"] = ["../ogr/data/poly.shp", "../ogr/data/poly.shp"]
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["pipeline"] = "concat ! write"
    assert alg.Run(my_progress)
    assert last_pct[0] == 1.0
    ds = alg["output"].GetDataset()
    assert ds.GetLayerCount() == 1
    lyr = ds.GetLayerByName("poly")
    assert len(lyr) == 20


def test_gdalalg_vector_concat_dst_crs():

    srs_4326 = osr.SpatialReference()
    srs_4326.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)
    srs_4326.ImportFromEPSG(4326)

    srs_32631 = osr.SpatialReference()
    srs_32631.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)
    srs_32631.ImportFromEPSG(32631)

    ds1 = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    lyr = ds1.CreateLayer("test", srs=srs_4326)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (2 49)"))
    lyr.CreateFeature(f)

    ds2 = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    lyr = ds2.CreateLayer("test", srs=srs_32631)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (500000 0)"))
    lyr.CreateFeature(f)

    alg = get_alg()
    alg["input"] = [ds1, ds2]
    alg["output"] = ""
    alg["output-format"] = "MEM"
    assert alg.Run()
    ds = alg["output"].GetDataset()
    lyr = ds.GetLayerByName("test")
    assert lyr.GetSpatialRef().GetAuthorityCode(None) == "4326"
    f = lyr.GetNextFeature()
    assert f.GetGeometryRef().ExportToWkt() == "POINT (2 49)"
    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(f, "POINT (3 0)")

    alg = get_alg()
    alg["input"] = [ds1, ds2]
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["dst-crs"] = "EPSG:4326"
    assert alg.Run()
    ds = alg["output"].GetDataset()
    lyr = ds.GetLayerByName("test")
    assert lyr.GetSpatialRef().GetAuthorityCode(None) == "4326"
    f = lyr.GetNextFeature()
    assert f.GetGeometryRef().ExportToWkt() == "POINT (2 49)"
    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(f, "POINT (3 0)")

    alg = get_alg()
    alg["input"] = [ds1, ds2]
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["dst-crs"] = "EPSG:32631"
    assert alg.Run()
    ds = alg["output"].GetDataset()
    lyr = ds.GetLayerByName("test")
    assert lyr.GetSpatialRef().GetAuthorityCode(None) == "32631"
    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(f, "POINT (426857.9877172817 5427937.523464922)")
    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(f, "POINT (500000 0)")

    ds3 = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    lyr = ds3.CreateLayer("test")
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (500000 0)"))
    lyr.CreateFeature(f)

    alg = get_alg()
    alg["input"] = [ds3]
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["dst-crs"] = "EPSG:4326"
    with pytest.raises(
        Exception, match="concat: Layer 'test' of '' has no spatial reference system"
    ):
        alg.Run()

    alg = get_alg()
    alg["input"] = [ds3]
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["src-crs"] = "EPSG:32631"
    alg["dst-crs"] = "EPSG:4326"
    assert alg.Run()
    ds = alg["output"].GetDataset()
    lyr = ds.GetLayerByName("test")
    assert lyr.GetSpatialRef().GetAuthorityCode(None) == "4326"
    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(f, "POINT (3 0)")


def test_gdalalg_vector_concat_input_layer_name():

    alg = get_alg()
    alg["input"] = ["../ogr/data/poly.shp", "../ogr/data/idlink.dbf"]
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["input-layer"] = "idlink"
    assert alg.Run()
    ds = alg["output"].GetDataset()
    assert ds.GetLayerCount() == 1
    assert ds.GetLayer(0).GetName() == "idlink"


@pytest.mark.parametrize(
    "strategy,expected_fields",
    [(None, ["a", "b", "c"]), ("union", ["a", "b", "c"]), ("intersection", ["c"])],
)
def test_gdalalg_vector_concat_field_strategy(strategy, expected_fields):

    ds1 = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    lyr = ds1.CreateLayer("test")
    lyr.CreateField(ogr.FieldDefn("a"))
    lyr.CreateField(ogr.FieldDefn("c"))

    ds2 = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    lyr = ds2.CreateLayer("test")
    lyr.CreateField(ogr.FieldDefn("b"))
    lyr.CreateField(ogr.FieldDefn("c"))

    alg = get_alg()
    alg["input"] = [ds1, ds2]
    alg["output"] = ""
    alg["output-format"] = "MEM"
    if strategy:
        alg["field-strategy"] = strategy
    assert alg.Run()
    ds = alg["output"].GetDataset()
    assert ds.GetLayerCount() == 1
    lyr = ds.GetLayerByName("test")
    got_fields = [
        lyr.GetLayerDefn().GetFieldDefn(i).GetName()
        for i in range(lyr.GetLayerDefn().GetFieldCount())
    ]
    assert len(got_fields) == len(expected_fields)
    assert set(got_fields) == set(expected_fields)


def test_gdalalg_vector_concat_single():

    ds1 = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    lyr = ds1.CreateLayer("test")
    lyr.CreateField(ogr.FieldDefn("a"))
    lyr.CreateField(ogr.FieldDefn("c"))

    ds2 = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    lyr = ds2.CreateLayer("test2")
    lyr.CreateField(ogr.FieldDefn("b"))
    lyr.CreateField(ogr.FieldDefn("c"))

    alg = get_alg()
    alg["input"] = [ds1, ds2]
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["mode"] = "single"
    assert alg.Run()
    ds = alg["output"].GetDataset()
    assert ds.GetLayerCount() == 1
    lyr = ds.GetLayerByName("merged")
    got_fields = [
        lyr.GetLayerDefn().GetFieldDefn(i).GetName()
        for i in range(lyr.GetLayerDefn().GetFieldCount())
    ]
    assert set(got_fields) == set(["a", "b", "c"])


def test_gdalalg_vector_concat_mode_default():

    ds1 = gdal.GetDriverByName("MEM").Create("ds1", 0, 0, 0, gdal.GDT_Unknown)
    lyr = ds1.CreateLayer("my_lyr_name_1")
    lyr.CreateFeature(ogr.Feature(lyr.GetLayerDefn()))
    lyr = ds1.CreateLayer("my_lyr_name_2")
    lyr.CreateFeature(ogr.Feature(lyr.GetLayerDefn()))
    ds2 = gdal.GetDriverByName("MEM").Create("ds2", 0, 0, 0, gdal.GDT_Unknown)
    lyr = ds2.CreateLayer("my_lyr_name_1")
    lyr.CreateFeature(ogr.Feature(lyr.GetLayerDefn()))
    lyr = ds2.CreateLayer("my_lyr_name_2")
    lyr.CreateFeature(ogr.Feature(lyr.GetLayerDefn()))

    alg = get_alg()
    alg["input"] = [ds1, ds2]
    alg["output"] = ""
    alg["output-format"] = "MEM"
    assert alg.Run()
    ds = alg["output"].GetDataset()
    assert ds.GetLayerCount() == 2
    assert [ds.GetLayer(i).GetName() for i in range(2)] == [
        "my_lyr_name_1",
        "my_lyr_name_2",
    ]
    for lyr in ds:
        assert len(lyr) == 2

    alg = get_alg()
    alg["input"] = [ds1, ds2]
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["output-layer"] = "not_allowed"
    with pytest.raises(
        Exception,
        match="'layer-name' name argument cannot be specified in mode=merge-per-layer-name",
    ):
        alg.Run()


def test_gdalalg_vector_concat_mode_stack():

    ds1 = gdal.GetDriverByName("MEM").Create("ds1", 0, 0, 0, gdal.GDT_Unknown)
    lyr = ds1.CreateLayer("my_lyr_name_1")
    lyr.CreateFeature(ogr.Feature(lyr.GetLayerDefn()))
    lyr = ds1.CreateLayer("my_lyr_name_2")
    lyr.CreateFeature(ogr.Feature(lyr.GetLayerDefn()))
    ds2 = gdal.GetDriverByName("MEM").Create("ds2", 0, 0, 0, gdal.GDT_Unknown)
    lyr = ds2.CreateLayer("my_lyr_name_1")
    lyr.CreateFeature(ogr.Feature(lyr.GetLayerDefn()))
    lyr = ds2.CreateLayer("my_lyr_name_2")
    lyr.CreateFeature(ogr.Feature(lyr.GetLayerDefn()))

    alg = get_alg()
    alg["input"] = [ds1, ds2]
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["mode"] = "stack"
    assert alg.Run()
    ds = alg["output"].GetDataset()
    assert ds.GetLayerCount() == 4
    assert [ds.GetLayer(i).GetName() for i in range(4)] == [
        "ds1_my_lyr_name_1",
        "ds1_my_lyr_name_2",
        "ds2_my_lyr_name_1",
        "ds2_my_lyr_name_2",
    ]
    for lyr in ds:
        assert len(lyr) == 1

    alg = get_alg()
    alg["input"] = [ds1, ds2]
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["mode"] = "stack"
    alg["output-layer"] = "{DS_INDEX}-{LAYER_INDEX}"
    assert alg.Run()
    ds = alg["output"].GetDataset()
    assert ds.GetLayerCount() == 4
    assert [ds.GetLayer(i).GetName() for i in range(4)] == ["0-0", "0-1", "1-0", "1-1"]
    for lyr in ds:
        assert len(lyr) == 1


def test_gdalalg_vector_concat_mode_single():

    ds1 = gdal.GetDriverByName("MEM").Create("ds1", 0, 0, 0, gdal.GDT_Unknown)
    lyr = ds1.CreateLayer("my_lyr_name_1")
    lyr.CreateFeature(ogr.Feature(lyr.GetLayerDefn()))
    lyr = ds1.CreateLayer("my_lyr_name_2")
    lyr.CreateFeature(ogr.Feature(lyr.GetLayerDefn()))
    ds2 = gdal.GetDriverByName("MEM").Create("ds2", 0, 0, 0, gdal.GDT_Unknown)
    lyr = ds2.CreateLayer("my_lyr_name_1")
    lyr.CreateFeature(ogr.Feature(lyr.GetLayerDefn()))
    lyr = ds2.CreateLayer("my_lyr_name_2")
    lyr.CreateFeature(ogr.Feature(lyr.GetLayerDefn()))

    alg = get_alg()
    alg["input"] = [ds1, ds2]
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["mode"] = "single"
    alg["output-layer"] = "my-output-layer"
    assert alg.Run()
    ds = alg["output"].GetDataset()
    assert ds.GetLayerCount() == 1
    assert ds.GetLayer(0).GetName() == "my-output-layer"
    assert len(ds.GetLayer(0)) == 4


def test_gdalalg_vector_concat_stack_from_filesystem_source():

    alg = get_alg()
    alg["input"] = "../ogr/data/poly.shp"
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["mode"] = "stack"
    assert alg.Run()
    ds = alg["output"].GetDataset()
    assert ds.GetLayerCount() == 1
    assert ds.GetLayer(0).GetName() == "poly"


@pytest.mark.parametrize(
    "source_layer_field_name,source_layer_field_content,expected_val",
    [
        (None, "{AUTO_NAME}", "my_ds_name_my_lyr_name"),
        ("src", None, "my_ds_name_my_lyr_name"),
        ("src", "{DS_NAME}", "my_ds_name"),
        ("src", "{DS_BASENAME}", "my_ds_name"),
        ("src", "{DS_INDEX}", "0"),
        ("src", "{LAYER_NAME}", "my_lyr_name"),
        ("src", "{LAYER_INDEX}", "0"),
    ],
)
def test_gdalalg_vector_concat_source_layer_field(
    source_layer_field_name, source_layer_field_content, expected_val
):

    ds1 = gdal.GetDriverByName("MEM").Create("my_ds_name", 0, 0, 0, gdal.GDT_Unknown)
    lyr = ds1.CreateLayer("my_lyr_name")
    lyr.CreateFeature(ogr.Feature(lyr.GetLayerDefn()))

    alg = get_alg()
    alg["input"] = ds1
    alg["output"] = ""
    alg["output-format"] = "MEM"
    if source_layer_field_name:
        alg["source-layer-field-name"] = source_layer_field_name
    if source_layer_field_content:
        alg["source-layer-field-content"] = source_layer_field_content
    assert alg.Run()
    ds = alg["output"].GetDataset()
    assert ds.GetLayerCount() == 1
    lyr = ds.GetLayer(0)
    assert lyr.GetName() == "my_lyr_name"
    assert len(lyr) == 1
    f = lyr.GetNextFeature()
    assert (
        f[source_layer_field_name if source_layer_field_name else "source_ds_lyr"]
        == expected_val
    )


@pytest.mark.require_driver("GDALG")
def test_gdalalg_vector_concat_test_ogrsf(tmp_path):

    import test_cli_utilities

    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    gdalg_filename = tmp_path / "tmp.gdalg.json"
    open(gdalg_filename, "wb").write(
        b'{"type": "gdal_streamed_alg","command_line": "gdal vector concat ../ogr/data/poly.shp --output-format=stream foo","relative_paths_relative_to_this_file":false}'
    )

    ret = gdaltest.runexternal(
        test_cli_utilities.get_test_ogrsf_path() + f" -ro {gdalg_filename}"
    )

    assert "INFO" in ret
    assert "ERROR" not in ret
    assert "FAILURE" not in ret
