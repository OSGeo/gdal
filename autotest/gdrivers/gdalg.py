#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test functionality for GDALG driver.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import json

import pytest

from osgeo import gdal, ogr

pytestmark = pytest.mark.require_driver("GDALG")


def test_gdalg_raster_from_file():
    ds = gdal.Open("data/gdalg/read_byte.gdalg")
    assert ds.GetRasterBand(1).Checksum() == 4672
    assert ds.GetDriver().GetDescription() == "GDALG"


def test_gdalg_raster_opened_as_vector():
    with pytest.raises(Exception):
        gdal.OpenEx("data/gdalg/read_byte.gdalg", gdal.OF_VECTOR)


def test_gdalg_raster_pipeline_standard():
    # No write step, which is the nominal way
    ds = gdal.Open(
        json.dumps(
            {
                "type": "gdal_streamed_alg",
                "command_line": "gdal raster pipeline ! read data/byte.tif",
            }
        )
    )
    assert ds.GetRasterBand(1).Checksum() == 4672


def test_gdalg_raster_pipeline_explicit_write_step():
    # This is untypical, but reflects what is done internally when not including
    # the last write step
    ds = gdal.Open(
        json.dumps(
            {
                "type": "gdal_streamed_alg",
                "command_line": "gdal raster pipeline ! read data/byte.tif ! write --output-format=stream streamed_output",
            }
        )
    )
    assert ds.GetRasterBand(1).Checksum() == 4672


def test_gdalg_raster_pipeline_error():
    with pytest.raises(Exception):
        gdal.Open(
            json.dumps(
                {
                    "type": "gdal_streamed_alg",
                    "command_line": "gdal raster pipeline ! read i_do_not_exist.tif",
                }
            )
        )


def test_gdalg_raster_pipeline_write_to_file_not_allowed():
    with pytest.raises(
        Exception,
        match="pipeline: in streamed execution, --format stream should be used",
    ):
        gdal.Open(
            json.dumps(
                {
                    "type": "gdal_streamed_alg",
                    "command_line": "gdal raster pipeline ! read data/byte.tif ! write /vsimem/this_is_not_allowed.tif",
                }
            )
        )


def test_gdalg_raster_mosaic():
    ds = gdal.Open(
        json.dumps(
            {
                "type": "gdal_streamed_alg",
                "command_line": "gdal raster mosaic --output-format=stream data/byte.tif streamed_dataset",
            }
        )
    )
    assert ds.GetRasterBand(1).Checksum() == 4672


def test_gdalg_raster_mosaic_write_to_file_not_allowed():
    with pytest.raises(
        Exception,
        match="mosaic: in streamed execution, --format stream should be used",
    ):
        gdal.Open(
            json.dumps(
                {
                    "type": "gdal_streamed_alg",
                    "command_line": "gdal raster mosaic data/byte.tif /vsimem/this_is_not_allowed.tif",
                }
            )
        )


def test_gdalg_raster_stack():
    ds = gdal.Open(
        json.dumps(
            {
                "type": "gdal_streamed_alg",
                "command_line": "gdal raster stack --output-format=stream data/byte.tif streamed_dataset",
            }
        )
    )
    assert ds.GetRasterBand(1).Checksum() == 4672


def test_gdalg_raster_stack_write_to_file_not_allowed():
    with pytest.raises(
        Exception,
        match="stack: in streamed execution, --format stream should be used",
    ):
        gdal.Open(
            json.dumps(
                {
                    "type": "gdal_streamed_alg",
                    "command_line": "gdal raster stack data/byte.tif /vsimem/this_is_not_allowed.tif",
                }
            )
        )


def test_gdalg_raster_reproject():
    ds = gdal.Open(
        json.dumps(
            {
                "type": "gdal_streamed_alg",
                "command_line": "gdal raster reproject --dst-crs=EPSG:26711 --output-format=stream data/byte.tif streamed_dataset",
            }
        )
    )
    assert ds.GetRasterBand(1).Checksum() == 4672


def test_gdalg_raster_reproject_write_to_file_not_allowed():
    with pytest.raises(
        Exception,
        match="reproject: in streamed execution, --format stream should be used",
    ):
        gdal.Open(
            json.dumps(
                {
                    "type": "gdal_streamed_alg",
                    "command_line": "gdal raster reproject --dst-crs=EPSG:26711 data/byte.tif /vsimem/this_is_not_allowed.tif",
                }
            )
        )


def test_gdalg_vector():
    ds = gdal.OpenEx("data/gdalg/read_poly.gdalg", gdal.OF_VECTOR)
    assert ds.GetLayerCount() == 1
    assert ds.GetLayer(0).GetName() == "poly"
    assert ds.GetLayerByName("poly").GetName() == "poly"
    with ds.ExecuteSQL("SELECT * FROM poly") as sql_lyr:
        assert sql_lyr.GetFeatureCount() == 10
    ds.ResetReading()
    count = 0
    while True:
        feat, lyr = ds.GetNextFeature()
        if feat is None:
            break
        count += 1
    assert count == 10
    assert ds.TestCapability(ogr.ODsCMeasuredGeometries) == 1


def test_gdalg_vector_opened_as_raster():
    with pytest.raises(Exception):
        gdal.OpenEx("data/gdalg/read_poly.gdalg", gdal.OF_RASTER)


def test_gdalg_vector_pipeline_write_to_file_not_allowed():
    with pytest.raises(
        Exception,
        match="pipeline: in streamed execution, --format stream should be used",
    ):
        gdal.Open(
            json.dumps(
                {
                    "type": "gdal_streamed_alg",
                    "command_line": "gdal vector pipeline ! read ../ogr/data/poly.shp ! write /vsimem/this_is_not_allowed.shp",
                }
            )
        )


def test_gdalg_generate_from_raster_pipeline(tmp_vsimem):
    pipeline = gdal.GetGlobalAlgorithmRegistry()["raster"]["pipeline"]
    out_filename = str(tmp_vsimem / "test.gdalg")
    assert pipeline.ParseRunAndFinalize(
        [
            "read",
            "data/byte.tif",
            "!",
            "reproject",
            "--dst-crs",
            "EPSG:4326",
            "!",
            "write",
            out_filename,
            "--overwrite",
        ]
    )
    assert json.loads(gdal.VSIFile(out_filename, "rb").read()) == {
        "command_line": "gdal raster pipeline read --input data/byte.tif ! reproject --dst-crs EPSG:4326",
        "type": "gdal_streamed_alg",
    }


def test_gdalg_generate_from_raster_mosaic(tmp_vsimem):
    mosaic = gdal.GetGlobalAlgorithmRegistry()["raster"]["mosaic"]
    out_filename = str(tmp_vsimem / "test.gdalg")
    assert mosaic.ParseRunAndFinalize(["data/byte.tif", out_filename])
    assert json.loads(gdal.VSIFile(out_filename, "rb").read()) == {
        "command_line": "gdal raster mosaic --input data/byte.tif --output-format stream --output streamed_dataset",
        "type": "gdal_streamed_alg",
    }


def test_gdalg_generate_from_raster_reproject(tmp_vsimem):
    reproject = gdal.GetGlobalAlgorithmRegistry()["raster"]["reproject"]
    out_filename = str(tmp_vsimem / "test.gdalg")
    assert reproject.ParseRunAndFinalize(
        ["data/byte.tif", out_filename, "--dst-crs=EPSG:4326", "--overwrite"]
    )
    assert json.loads(gdal.VSIFile(out_filename, "rb").read()) == {
        "command_line": "gdal raster reproject --input data/byte.tif --dst-crs EPSG:4326 --output-format stream --output streamed_dataset",
        "type": "gdal_streamed_alg",
    }


def test_gdalg_generate_from_vector_pipeline(tmp_vsimem):
    pipeline = gdal.GetGlobalAlgorithmRegistry()["vector"]["pipeline"]
    out_filename = str(tmp_vsimem / "test.gdalg")
    assert pipeline.ParseRunAndFinalize(
        [
            "read",
            "../ogr/data/poly.shp",
            "!",
            "reproject",
            "--dst-crs",
            "EPSG:4326",
            "!",
            "write",
            out_filename,
        ]
    )
    assert json.loads(gdal.VSIFile(out_filename, "rb").read()) == {
        "command_line": "gdal vector pipeline read --input ../ogr/data/poly.shp ! reproject --dst-crs EPSG:4326",
        "type": "gdal_streamed_alg",
    }
