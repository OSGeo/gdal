#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal raster info' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import json

import pytest

from osgeo import gdal


def get_info_alg():
    return gdal.GetGlobalAlgorithmRegistry()["raster"]["info"]


def test_gdalalg_raster_info_stdout_text_default_format():
    import gdaltest
    import test_cli_utilities

    gdal_path = test_cli_utilities.get_gdal_path()
    if gdal_path is None:
        pytest.skip("gdal binary missing")
    out, _ = gdaltest.runexternal_out_and_err(
        f"{gdal_path} raster info data/utmsmall.tif"
    )
    assert out.startswith("Driver: GTiff/GeoTIFF")


def test_gdalalg_raster_info_stdout_json():
    import gdaltest
    import test_cli_utilities

    gdal_path = test_cli_utilities.get_gdal_path()
    if gdal_path is None:
        pytest.skip("gdal binary missing")
    out, _ = gdaltest.runexternal_out_and_err(
        f"{gdal_path} raster info --format json data/utmsmall.tif"
    )
    j = json.loads(out)
    assert len(j["bands"]) == 1


def test_gdalalg_raster_info():
    info = get_info_alg()
    assert info.ParseRunAndFinalize(["--format=text", "data/utmsmall.tif"])
    output_string = info["output-string"]
    assert output_string.startswith("Driver: GTiff/GeoTIFF")


def test_gdalalg_raster_info_mm_checksum():
    info = get_info_alg()
    assert info.ParseRunAndFinalize(
        ["--format=text", "--mm", "--checksum", "data/utmsmall.tif"]
    )
    output_string = info["output-string"]
    assert "   Computed Min/Max=0.000,255.000" in output_string
    assert "Checksum=" in output_string


def test_gdalalg_raster_info_stats():
    info = get_info_alg()
    ds = gdal.Translate("", "../gcore/data/byte.tif", format="MEM")
    info["input"] = ds
    assert info.ParseRunAndFinalize(["--stats"])
    output_string = info["output-string"]
    j = json.loads(output_string)
    assert "stdDev" in j["bands"][0]


def test_gdalalg_raster_info_approx_stats():
    info = get_info_alg()
    ds = gdal.Translate("", "../gcore/data/byte.tif", format="MEM")
    info["input"] = ds
    assert info.ParseRunAndFinalize(["--approx-stats"])
    output_string = info["output-string"]
    j = json.loads(output_string)
    assert "stdDev" in j["bands"][0]


def test_gdalalg_raster_info_hist():
    info = get_info_alg()
    ds = gdal.Translate("", "../gcore/data/byte.tif", format="MEM")
    info["input"] = ds
    assert info.ParseRunAndFinalize(["--hist"])
    output_string = info["output-string"]
    j = json.loads(output_string)
    assert "histogram" in j["bands"][0]


def test_gdalalg_raster_info_no_options():
    info = get_info_alg()
    ds = gdal.Translate("", "../gcore/data/byte.tif", format="MEM")
    info["input"] = ds
    assert info.ParseRunAndFinalize(
        ["--no-gcp", "--no-md", "--no-ct", "--no-fl", "--no-nodata", "--no-mask"]
    )


def test_gdalalg_raster_info_list_mdd():
    info = get_info_alg()
    ds = gdal.Translate("", "../gcore/data/byte.tif", format="MEM")
    ds.SetMetadataItem("foo", "bar", "MY_DOMAIN")
    info["input"] = ds
    assert info.ParseRunAndFinalize(["--list-mdd"])
    output_string = info["output-string"]
    j = json.loads(output_string)
    assert "MY_DOMAIN" in j["metadata"]["metadataDomains"]


def test_gdalalg_raster_info_mdd_all():
    info = get_info_alg()
    ds = gdal.Translate("", "../gcore/data/byte.tif", format="MEM")
    ds.SetMetadataItem("foo", "bar", "MY_DOMAIN")
    info["input"] = ds
    assert info.ParseRunAndFinalize(["--mdd=all"])
    output_string = info["output-string"]
    j = json.loads(output_string)
    assert j["metadata"] == {
        "": {"AREA_OR_POINT": "Area"},
        "IMAGE_STRUCTURE": {"INTERLEAVE": "BAND"},
        "MY_DOMAIN": {"foo": "bar"},
        "DERIVED_SUBDATASETS": {
            "DERIVED_SUBDATASET_1_NAME": "DERIVED_SUBDATASET:LOGAMPLITUDE:",
            "DERIVED_SUBDATASET_1_DESC": "log10 of amplitude of input bands from ",
        },
    }


def test_gdalalg_raster_info_list_subdataset():
    info = get_info_alg()
    assert info.ParseRunAndFinalize(
        ["--input=../gcore/data/tiff_with_subifds.tif", "--subdataset=2"]
    )
    output_string = info["output-string"]
    j = json.loads(output_string)
    assert j["description"] == "GTIFF_DIR:2:../gcore/data/tiff_with_subifds.tif"


def test_gdalalg_raster_info_list_subdataset_error():
    info = get_info_alg()
    with pytest.raises(
        Exception,
        match="Invalid value for 'subdataset' argument. Should be between 1 and 2",
    ):
        info.ParseRunAndFinalize(
            ["--input=../gcore/data/tiff_with_subifds.tif", "--subdataset=3"]
        )


def test_gdalalg_raster_info_list_subdataset_error_cannot_open_subdataset():
    info = get_info_alg()
    ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    ds.SetMetadataItem("SUBDATASET_1_DESC", "desc", "SUBDATASETS")
    ds.SetMetadataItem("SUBDATASET_1_NAME", "i_do_not_exist", "SUBDATASETS")
    info["input"] = ds
    with pytest.raises(
        Exception,
        match="i_do_not_exist",
    ):
        info.ParseRunAndFinalize(["--subdataset=1"])


@pytest.mark.require_driver("GDALG")
def test_gdalalg_raster_info_read_gdalg_with_input_format():
    info = get_info_alg()
    info["input"] = "../gdrivers/data/gdalg/read_byte.gdalg.json"
    info["input-format"] = "GDALG"
    assert info.Run()
