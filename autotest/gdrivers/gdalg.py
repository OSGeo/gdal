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

import gdaltest
import pytest

from osgeo import gdal, ogr

pytestmark = pytest.mark.require_driver("GDALG")


def test_gdalg_raster_from_file():
    ds = gdal.Open("data/gdalg/read_byte.gdalg.json")
    assert ds.GetRasterBand(1).Checksum() == 4672
    assert ds.GetDriver().GetDescription() == "GDALG"
    assert ds.GetFileList() == ["data/gdalg/read_byte.gdalg.json"]


def test_gdalg_raster_opened_as_vector():
    with pytest.raises(Exception):
        gdal.OpenEx("data/gdalg/read_byte.gdalg.json", gdal.OF_VECTOR)


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
    assert ds.GetFileList() is None


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


def test_gdalg_raster_pipeline_warn_about_config():
    with gdaltest.error_raised(
        gdal.CE_Warning,
        match="Configuration options passed with the 'config' argument are ignored",
    ):
        ds = gdal.Open(
            json.dumps(
                {
                    "type": "gdal_streamed_alg",
                    "command_line": "gdal raster pipeline ! read --config FOO=BAR data/byte.tif",
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
    ds = gdal.OpenEx("data/gdalg/read_poly.gdalg.json", gdal.OF_VECTOR)
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
        gdal.OpenEx("data/gdalg/read_poly.gdalg.json", gdal.OF_RASTER)


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


def test_gdalg_vector_filter_standalone_write_to_file_not_allowed():
    with pytest.raises(
        Exception,
        match="filter: in streamed execution, --format stream should be used",
    ):
        gdal.Open(
            json.dumps(
                {
                    "type": "gdal_streamed_alg",
                    "command_line": "gdal vector filter ../ogr/data/poly.shp --output-format=MEM foo",
                }
            )
        )


def test_gdalg_lower_version(tmp_vsimem):
    with pytest.raises(
        Exception,
        match="The failure might be due to the .gdalg.json file having been created with GDAL VERSION_NUM=99999999 which is newer than current",
    ):
        gdal.Open(
            json.dumps(
                {
                    "type": "gdal_streamed_alg",
                    "command_line": "gdal this is an error",
                    "gdal_version": "99999999",
                }
            )
        )


def test_gdalg_generate_from_raster_pipeline(tmp_vsimem):
    out_filename = str(tmp_vsimem / "test.gdalg.json")

    pipeline = gdal.GetGlobalAlgorithmRegistry()["raster"]["pipeline"]
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
    j = json.loads(gdal.VSIFile(out_filename, "rb").read())
    assert "gdal_version" in j
    del j["gdal_version"]
    assert j == {
        "command_line": "gdal raster pipeline read --input data/byte.tif ! reproject --dst-crs EPSG:4326",
        "type": "gdal_streamed_alg",
    }

    pipeline = gdal.GetGlobalAlgorithmRegistry()["raster"]["pipeline"]
    with pytest.raises(
        Exception,
        match="already exists",
    ):
        pipeline.ParseRunAndFinalize(
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
            ]
        )

    pipeline = gdal.GetGlobalAlgorithmRegistry()["raster"]["pipeline"]
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
    j = json.loads(gdal.VSIFile(out_filename, "rb").read())
    assert "gdal_version" in j
    del j["gdal_version"]
    assert j == {
        "command_line": "gdal raster pipeline read --input data/byte.tif ! reproject --dst-crs EPSG:4326",
        "type": "gdal_streamed_alg",
    }


def test_gdalg_generate_error_due_to_mem_dataset(tmp_vsimem):
    out_filename = str(tmp_vsimem / "test.gdalg.json")

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    with pytest.raises(Exception, match="Cannot serialize argument input"):
        gdal.Run(
            "raster pipeline", input=src_ds, pipeline=f"read ! write {out_filename}"
        )


def test_gdalg_generate_from_raster_mosaic(tmp_vsimem):
    mosaic = gdal.GetGlobalAlgorithmRegistry()["raster"]["mosaic"]
    out_filename = str(tmp_vsimem / "test.gdalg.json")
    assert mosaic.ParseRunAndFinalize(["data/byte.tif", out_filename])
    j = json.loads(gdal.VSIFile(out_filename, "rb").read())
    assert "gdal_version" in j
    del j["gdal_version"]
    assert j == {
        "command_line": "gdal raster mosaic --input data/byte.tif --output-format stream --output streamed_dataset",
        "type": "gdal_streamed_alg",
    }


def test_gdalg_generate_from_raster_reproject(tmp_vsimem):
    reproject = gdal.GetGlobalAlgorithmRegistry()["raster"]["reproject"]
    out_filename = str(tmp_vsimem / "test.gdalg.json")
    assert reproject.ParseRunAndFinalize(
        ["data/byte.tif", out_filename, "--dst-crs=EPSG:4326", "--overwrite"]
    )
    j = json.loads(gdal.VSIFile(out_filename, "rb").read())
    assert "gdal_version" in j
    del j["gdal_version"]
    assert j == {
        "command_line": "gdal raster reproject --input data/byte.tif --dst-crs EPSG:4326 --output-format stream --output streamed_dataset",
        "type": "gdal_streamed_alg",
    }


def test_gdalg_generate_from_vector_pipeline(tmp_vsimem):
    pipeline = gdal.GetGlobalAlgorithmRegistry()["vector"]["pipeline"]
    out_filename = str(tmp_vsimem / "test.gdalg.json")
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
    j = json.loads(gdal.VSIFile(out_filename, "rb").read())
    assert "gdal_version" in j
    del j["gdal_version"]
    assert j == {
        "command_line": "gdal vector pipeline read --input ../ogr/data/poly.shp ! reproject --dst-crs EPSG:4326",
        "type": "gdal_streamed_alg",
    }


def test_gdalg_generate_from_vector_pipeline_geom(tmp_vsimem):
    pipeline = gdal.GetGlobalAlgorithmRegistry()["vector"]["pipeline"]
    out_filename = str(tmp_vsimem / "test.gdalg.json")
    assert pipeline.ParseRunAndFinalize(
        [
            "read",
            "../ogr/data/poly.shp",
            "!",
            "set-type",
            "--geometry-type=MULTIPOLYGON",
            "!",
            "write",
            out_filename,
        ]
    )
    j = json.loads(gdal.VSIFile(out_filename, "rb").read())
    assert "gdal_version" in j
    del j["gdal_version"]
    assert j == {
        "command_line": "gdal vector pipeline read --input ../ogr/data/poly.shp ! set-type --geometry-type MULTIPOLYGON",
        "type": "gdal_streamed_alg",
    }


def test_gdalg_invalid_inline():
    with pytest.raises(Exception, match="JSON parsing error"):
        gdal.Open(
            json.dumps(
                {
                    "type": "gdal_streamed_alg",
                    "command_line": "gdal raster pipeline ! read data/byte.tif",
                }
            )[0:-10]
        )


def test_gdalg_invalid_file(tmp_vsimem):

    gdal.FileFromMemBuffer(
        tmp_vsimem / "tmp.gdalg.json",
        json.dumps(
            {
                "type": "gdal_streamed_alg",
                "command_line": "gdal raster pipeline ! read data/byte.tif",
            }
        )[0:-10],
    )

    with pytest.raises(Exception, match="JSON parsing error"):
        gdal.Open(tmp_vsimem / "tmp.gdalg.json")


def test_gdalg_update():
    with pytest.raises(
        Exception,
        match="The GDALG driver does not support update access to existing datasets",
    ):
        gdal.Open("data/gdalg/read_byte.gdalg.json", gdal.GA_Update)


def test_gdalg_missing_type():
    with pytest.raises(Exception):
        gdal.Open(
            json.dumps(
                {
                    "MISSING_type": "gdal_streamed_alg",
                    "command_line": "gdal raster pipeline ! read data/byte.tif",
                }
            )
        )


def test_gdalg_missing_command_line():
    with pytest.raises(Exception, match="command_line missing"):
        gdal.Open(json.dumps({"type": "gdal_streamed_alg"}))


def test_gdalg_alg_does_not_support_streaming():
    with pytest.raises(
        Exception, match="Algorithm delete does not support a streamed output"
    ):
        gdal.Open(
            json.dumps(
                {
                    "type": "gdal_streamed_alg",
                    "command_line": "gdal raster overview delete data/byte.tif",
                }
            )
        )
