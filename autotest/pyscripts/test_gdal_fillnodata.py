#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdal_fillnodata.py testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2010, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import struct

import gdaltest
import pytest
import test_py_scripts

from osgeo import gdal

pytestmark = pytest.mark.skipif(
    test_py_scripts.get_py_script("gdal_fillnodata") is None,
    reason="gdal_fillnodata not available",
)


@pytest.fixture()
def script_path():
    return test_py_scripts.get_py_script("gdal_fillnodata")


###############################################################################
#


def test_gdal_fillnodata_help(script_path):

    if gdaltest.is_travis_branch("sanitize"):
        pytest.skip("fails on sanitize for unknown reason")

    assert "ERROR" not in test_py_scripts.run_py_script(
        script_path, "gdal_fillnodata", "--help"
    )


###############################################################################
#


def test_gdal_fillnodata_version(script_path):

    if gdaltest.is_travis_branch("sanitize"):
        pytest.skip("fails on sanitize for unknown reason")

    assert "ERROR" not in test_py_scripts.run_py_script(
        script_path, "gdal_fillnodata", "--version"
    )


###############################################################################
# Dummy test : there is no nodata value in the source dataset !


def test_gdal_fillnodata_1(script_path, tmp_path):

    result_tif = str(tmp_path / "test_gdal_fillnodata_1.tif")

    _, err = test_py_scripts.run_py_script(
        script_path,
        "gdal_fillnodata",
        test_py_scripts.get_data_path("gcore") + f"byte.tif {result_tif}",
        return_stderr=True,
    )
    assert "UseExceptions" not in err

    ds = gdal.Open(result_tif)
    assert ds.GetRasterBand(1).Checksum() == 4672
    ds = None


###############################################################################
# Make sure we copy the no data value to the dst when created
# No data value for nodata_byte.tif is 0.


def test_gdal_fillnodata_2(script_path, tmp_path):

    result_tif = str(tmp_path / "test_gdal_fillnodata_2.tif")

    test_py_scripts.run_py_script(
        script_path,
        "gdal_fillnodata",
        "-si 0 "
        + test_py_scripts.get_data_path("gcore")
        + f"nodata_byte.tif {result_tif}",
    )

    ds = gdal.Open(result_tif)
    assert (
        ds.GetRasterBand(1).GetNoDataValue() == 0
    ), "Failed to copy No Data Value to dst dataset."
    ds = None


###############################################################################
# Test -si 1 and -md


def test_gdal_fillnodata_smoothing(script_path, tmp_path):

    input_tif = str(tmp_path / "test_gdal_fillnodata_smoothing_in.tif")
    result_tif = str(tmp_path / "test_gdal_fillnodata_smoothing.tif")

    ds = gdal.GetDriverByName("GTiff").Create(input_tif, 4, 4)
    ds.GetRasterBand(1).SetNoDataValue(0)
    input_data = (20, 30, 40, 50, 30, 0, 0, 60, 40, 0, 0, 70, 50, 60, 70, 80)
    ds.GetRasterBand(1).WriteRaster(
        0, 0, 4, 4, b"".join([struct.pack("B", x) for x in input_data])
    )
    ds = None

    test_py_scripts.run_py_script(
        script_path,
        "gdal_fillnodata",
        f"-md 1 -si 1 {input_tif} {result_tif}",
    )

    expected_data = (20, 30, 40, 50, 30, 40, 50, 60, 40, 50, 60, 70, 50, 60, 70, 80)

    ds = gdal.Open(result_tif)
    assert struct.unpack("B" * 16, ds.GetRasterBand(1).ReadRaster()) == expected_data
    ds = None


###############################################################################
# Test -interp nearest


def test_gdal_fillnodata_nearest(script_path, tmp_path):

    input_tif = str(tmp_path / "test_gdal_fillnodata_nearest_in.tif")
    result_tif = str(tmp_path / "test_gdal_fillnodata_nearest.tif")

    ds = gdal.GetDriverByName("GTiff").Create(input_tif, 3, 3)
    ds.GetRasterBand(1).SetNoDataValue(0)
    input_data = (20, 30, 40, 50, 0, 60, 70, 80, 90)
    ds.GetRasterBand(1).WriteRaster(
        0, 0, 3, 3, b"".join([struct.pack("B", x) for x in input_data])
    )
    ds = None

    test_py_scripts.run_py_script(
        script_path,
        "gdal_fillnodata",
        f"-interp nearest {input_tif} {result_tif}",
    )

    expected_data = (20, 30, 40, 50, 30, 60, 70, 80, 90)

    ds = gdal.Open(result_tif)
    assert struct.unpack("B" * 9, ds.GetRasterBand(1).ReadRaster()) == expected_data
    ds = None
