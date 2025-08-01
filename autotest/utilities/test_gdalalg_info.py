#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal info' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import pytest

from osgeo import gdal


def get_info_alg():
    return gdal.GetGlobalAlgorithmRegistry()["info"]


@pytest.mark.parametrize(
    "args",
    [
        (["--format=text", "data/utmsmall.tif"]),
        (["--format=text", "data/utmsmall.tif", "--checksum"]),
    ],
)
def test_gdalalg_info_on_raster(args):
    info = get_info_alg()
    assert info.ParseRunAndFinalize(args)
    output_string = info["output-string"]
    assert output_string.startswith("Driver: GTiff/GeoTIFF")


def test_gdalalg_info_on_raster_invalid_arg():
    info = get_info_alg()
    with pytest.raises(Exception, match="Option '--invalid' is unknown"):
        assert info.ParseRunAndFinalize(
            ["--format=text", "--invalid", "data/utmsmall.tif"]
        )


@pytest.mark.parametrize(
    "args",
    [
        (["--format=text", "data/path.shp"]),
        (["data/path.shp", "--format=text"]),
        (["--format=text", "-i", "data/path.shp"]),
        (["--format=text", "--input", "data/path.shp"]),
        (["--format=text", "--input=data/path.shp"]),
        (["--format=text", "--dialect=OGRSQL", "data/path.shp"]),
    ],
)
def test_gdalalg_info_on_vector(args):
    info = get_info_alg()
    assert info.ParseRunAndFinalize(args)
    output_string = info["output-string"]
    assert output_string.startswith("INFO: Open of")


def test_gdalalg_info_on_vector_invalid_arg():
    info = get_info_alg()
    with pytest.raises(Exception, match="Option '--invalid' is unknown"):
        assert info.ParseRunAndFinalize(["--format=text", "--invalid", "data/path.shp"])


def test_gdalalg_info_invalid_arg():
    info = get_info_alg()
    with pytest.raises(Exception, match="Option '--invalid' is unknown"):
        assert info.ParseRunAndFinalize(["--invalid"])


def test_gdalalg_info_run_cannot_be_run():
    info = get_info_alg()
    info["input"] = gdal.GetDriverByName("MEM").Create("", 1, 1)
    with pytest.raises(Exception, match="method should not be called directly"):
        info.Run()


@pytest.mark.require_driver("GPKG")
def test_gdalalg_info_mixed_raster_vector(tmp_vsimem):

    filename = str(tmp_vsimem / "test.gpkg")
    gdal.Translate(filename, "data/utmsmall.tif")
    with gdal.OpenEx(filename, gdal.OF_UPDATE) as ds:
        ds.CreateLayer("vector_layer")
    info = get_info_alg()
    with pytest.raises(Exception, match="has both raster and vector content"):
        assert info.ParseRunAndFinalize([filename])


@pytest.mark.require_driver("GPKG")
def test_gdalalg_info_mixed_raster_vector_with_invalid_arg(tmp_vsimem):

    filename = str(tmp_vsimem / "test.gpkg")
    gdal.Translate(filename, "data/utmsmall.tif")
    with gdal.OpenEx(filename, gdal.OF_UPDATE) as ds:
        ds.CreateLayer("vector_layer")
    info = get_info_alg()
    with pytest.raises(Exception, match="has both raster and vector content"):
        assert info.ParseRunAndFinalize([filename, "--invalid"])


def test_gdalalg_info_mixed_run_without_arg(tmp_vsimem):

    info = get_info_alg()
    info["input"] = "data/utmsmall.tif"
    with pytest.raises(Exception, match="should not be called directly"):
        assert info.Run()


@pytest.mark.require_driver("netCDF")
def test_gdalalg_info_netcdf_raster():

    info = get_info_alg()
    assert info.ParseRunAndFinalize(["../gdrivers/data/netcdf/byte.nc"])
    output_string = info["output-string"]
    assert '"geoTransform"' in output_string


def test_gdalalg_info_i_do_not_exist_format():
    info = get_info_alg()
    info["input"] = "/i/do_not/exist"
    info["format"] = "text"
    with pytest.raises(Exception, match="/i/do_not/exist"):
        info.Run()


def test_gdalalg_info_help():

    import gdaltest
    import test_cli_utilities

    gdal_path = test_cli_utilities.get_gdal_path()
    if gdal_path is None:
        pytest.skip("gdal binary missing")

    out = gdaltest.runexternal(f"{gdal_path} info --help")
    assert out.startswith("Usage: gdal info [OPTIONS] <INPUT>")
