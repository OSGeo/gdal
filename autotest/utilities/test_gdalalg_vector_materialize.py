#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  Testing of 'materialize' step in 'gdal vector pipeline'
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import json

import pytest

from osgeo import gdal, ogr


def _get_cleaned_list(lst):
    if not lst:
        return []
    ret = []
    for elt in lst:
        if elt != "." and elt != "..":
            ret.append(elt)
    return ret


@pytest.mark.require_driver("GPKG")
def test_gdalalg_vector_materialize_temp_output_gpkg(tmp_path):

    tab_pct = [0]

    def my_progress(pct, msg, user_data):
        assert pct >= tab_pct[0]
        tab_pct[0] = pct
        return True

    with gdal.config_option("CPL_TMPDIR", str(tmp_path)):
        with gdal.Run(
            "vector",
            "pipeline",
            pipeline="read ../ogr/data/poly.shp ! materialize ! write --of stream streamed_dataset",
            progress=my_progress,
        ) as alg:
            assert tab_pct[0] == 1.0
            with alg.Output() as ds:
                assert ds.GetLayer(0).GetFeatureCount() == 10

    assert _get_cleaned_list(gdal.ReadDir(tmp_path)) == []


@pytest.mark.require_driver("ESRI Shapefile")
def test_gdalalg_vector_materialize_temp_output_shapefile(tmp_path):

    with gdal.config_option("CPL_TMPDIR", str(tmp_path)):
        with gdal.Run(
            "vector",
            "pipeline",
            pipeline='read ../ogr/data/poly.shp ! materialize --format "ESRI Shapefile" ! write --of stream streamed_dataset',
        ) as alg:
            with alg.Output() as ds:
                assert ds.GetDriver().GetDescription() == "ESRI Shapefile"
                assert ds.GetLayer(0).GetFeatureCount() == 10

    assert _get_cleaned_list(gdal.ReadDir(tmp_path)) == []


def test_gdalalg_vector_materialize_temp_output_mem(tmp_path):

    with gdal.config_option("CPL_TMPDIR", str(tmp_path)):
        with gdal.Run(
            "vector",
            "pipeline",
            pipeline="read ../ogr/data/poly.shp ! materialize --output-format=MEM ! write --of stream streamed_dataset",
        ) as alg:
            with alg.Output() as ds:
                assert ds.GetDriver().GetDescription() == "MEM"
                assert ds.GetLayer(0).GetFeatureCount() == 10

    assert _get_cleaned_list(gdal.ReadDir(tmp_path)) == []


@pytest.mark.require_driver("SQLite")
def test_gdalalg_vector_materialize_temp_output_sqlite_because_of_several_geom_fields(
    tmp_path,
):

    src_ds = gdal.GetDriverByName("MEM").CreateVector("")
    lyr = src_ds.CreateLayer("test", geom_type=ogr.wkbNone)
    lyr.CreateGeomField(ogr.GeomFieldDefn("geom1"))
    lyr.CreateGeomField(ogr.GeomFieldDefn("geom2"))

    with gdal.config_option("CPL_TMPDIR", str(tmp_path)):
        with gdal.Run(
            "vector",
            "pipeline",
            input=src_ds,
            pipeline="read ! materialize ! write --of stream streamed_dataset",
        ) as alg:
            with alg.Output() as ds:
                assert ds.GetDriver().GetDescription() == "SQLite"
                assert ds.GetLayer(0).GetLayerDefn().GetGeomFieldCount() == 2

    assert _get_cleaned_list(gdal.ReadDir(tmp_path)) == []


@pytest.mark.require_driver("SQLite")
def test_gdalalg_vector_materialize_temp_output_sqlite_because_of_list_field_type(
    tmp_path,
):

    src_ds = gdal.GetDriverByName("MEM").CreateVector("")
    lyr = src_ds.CreateLayer("test", geom_type=ogr.wkbPoint)
    lyr.CreateField(ogr.FieldDefn("strlist", ogr.OFTStringList))

    with gdal.config_option("CPL_TMPDIR", str(tmp_path)):
        with gdal.Run(
            "vector",
            "pipeline",
            input=src_ds,
            pipeline="read ! materialize --co SPATIALITE=NO ! write --of stream streamed_dataset",
        ) as alg:
            with alg.Output() as ds:
                assert ds.GetDriver().GetDescription() == "SQLite"
                assert (
                    ds.GetLayer(0).GetLayerDefn().GetFieldDefn(0).GetType()
                    == ogr.OFTStringList
                )

    assert _get_cleaned_list(gdal.ReadDir(tmp_path)) == []


@pytest.mark.require_driver("FlatGeoBuf")
def test_gdalalg_vector_materialize_temp_output_flatgeobuf(tmp_path):

    with gdal.config_option("CPL_TMPDIR", str(tmp_path)):
        with gdal.Run(
            "vector",
            "pipeline",
            pipeline="read ../ogr/data/poly.shp ! materialize --format FlatGeoBuf --layer-creation-option SPATIAL_INDEX=NO ! write --of stream streamed_dataset",
        ) as alg:
            with alg.Output() as ds:
                assert ds.GetLayer(0).GetFeatureCount() == 10

    assert _get_cleaned_list(gdal.ReadDir(tmp_path)) == []


@pytest.mark.require_driver("Parquet")
def test_gdalalg_vector_materialize_temp_output_parquet(tmp_path):

    with gdal.config_option("CPL_TMPDIR", str(tmp_path)):
        with gdal.Run(
            "vector",
            "pipeline",
            pipeline="read ../ogr/data/poly.shp ! materialize --format Parquet ! write --of stream streamed_dataset",
        ) as alg:
            with alg.Output() as ds:
                assert ds.GetLayer(0).GetFeatureCount() == 10

    assert _get_cleaned_list(gdal.ReadDir(tmp_path)) == []


@pytest.mark.require_driver("GDALG")
@pytest.mark.require_driver("GPKG")
def test_gdalalg_vector_materialize_read_from_gdalg():
    with ogr.Open(
        json.dumps(
            {
                "type": "gdal_streamed_alg",
                "command_line": "gdal pipeline read ../ogr/data/poly.shp ! materialize",
            }
        )
    ) as ds:
        assert ds.GetLayer(0).GetFeatureCount() == 10


@pytest.mark.require_driver("GDALG")
@pytest.mark.require_driver("GPKG")
def test_gdalalg_vector_materialize_read_from_gdalg_error():
    with pytest.raises(
        Exception,
        match="Step 'materialize' not allowed in stream execution, unless the GDAL_ALGORITHM_ALLOW_WRITES_IN_STREAM configuration option is set",
    ):
        ogr.Open(
            json.dumps(
                {
                    "type": "gdal_streamed_alg",
                    "command_line": "gdal pipeline read ../ogr/data/poly.shp ! materialize --output=illegal",
                }
            )
        )
