#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test gdal_proximity.py script
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2008, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2010, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import gdaltest
import pytest
import test_py_scripts

from osgeo import gdal

pytestmark = pytest.mark.skipif(
    test_py_scripts.get_py_script("gdal_proximity") is None,
    reason="gdal_proximity not available",
)


@pytest.fixture()
def script_path():
    return test_py_scripts.get_py_script("gdal_proximity")


###############################################################################
#


def test_gdal_proximity_help(script_path):

    if gdaltest.is_travis_branch("sanitize"):
        pytest.skip("fails on sanitize for unknown reason")

    assert "ERROR" not in test_py_scripts.run_py_script(
        script_path, "gdal_proximity", "--help"
    )


###############################################################################
#


def test_gdal_proximity_version(script_path):

    if gdaltest.is_travis_branch("sanitize"):
        pytest.skip("fails on sanitize for unknown reason")

    assert "ERROR" not in test_py_scripts.run_py_script(
        script_path, "gdal_proximity", "--version"
    )


###############################################################################
# Test a fairly default case.


def test_gdal_proximity_1(script_path, tmp_path):

    output_tif = str(tmp_path / "proximity_1.tif")

    drv = gdal.GetDriverByName("GTiff")
    dst_ds = drv.Create(output_tif, 25, 25, 1, gdal.GDT_UInt8)
    dst_ds = None

    _, err = test_py_scripts.run_py_script(
        script_path,
        "gdal_proximity",
        test_py_scripts.get_data_path("alg") + f"pat.tif {output_tif}",
        return_stderr=True,
    )
    assert "UseExceptions" not in err

    dst_ds = gdal.Open(output_tif)
    dst_band = dst_ds.GetRasterBand(1)

    cs_expected = 1941
    cs = dst_band.Checksum()

    dst_band = None
    dst_ds = None

    assert cs == cs_expected, "got wrong checksum"


###############################################################################
# Try several options


def test_gdal_proximity_2(script_path, tmp_path):

    output_tif = str(tmp_path / "proximity_2.tif")

    test_py_scripts.run_py_script(
        script_path,
        "gdal_proximity",
        "-q -values 65,64 -maxdist 12 -nodata -1 -fixed-buf-val 255 "
        + test_py_scripts.get_data_path("alg")
        + f"pat.tif {output_tif}",
    )

    dst_ds = gdal.Open(output_tif)
    dst_band = dst_ds.GetRasterBand(1)

    cs_expected = 3256
    cs = dst_band.Checksum()

    dst_band = None
    dst_ds = None

    assert cs == cs_expected, "got wrong checksum"


###############################################################################
# Try input nodata option


def test_gdal_proximity_3(script_path, tmp_path):

    output_tif = str(tmp_path / "proximity_3.tif")

    test_py_scripts.run_py_script(
        script_path,
        "gdal_proximity",
        "-q -values 65,64 -maxdist 12 -nodata 0 -use_input_nodata yes "
        + test_py_scripts.get_data_path("alg")
        + f"pat.tif {output_tif}",
    )

    dst_ds = gdal.Open(output_tif)
    dst_band = dst_ds.GetRasterBand(1)

    cs_expected = 1465
    cs = dst_band.Checksum()

    dst_band = None
    dst_ds = None

    assert cs == cs_expected, "got wrong checksum"
