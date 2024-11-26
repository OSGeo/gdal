#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test gdal_sieve.py utility
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2008, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2010, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################


import pytest
import test_py_scripts

from osgeo import gdal

pytestmark = pytest.mark.skipif(
    test_py_scripts.get_py_script("gdal_sieve") is None,
    reason="gdal_sieve not available",
)


@pytest.fixture()
def script_path():
    return test_py_scripts.get_py_script("gdal_sieve")


###############################################################################
#


def test_gdal_sieve_help(script_path):

    assert "ERROR" not in test_py_scripts.run_py_script(
        script_path, "gdal_sieve", "--help"
    )


###############################################################################
#


def test_gdal_sieve_version(script_path):

    assert "ERROR" not in test_py_scripts.run_py_script(
        script_path, "gdal_sieve", "--version"
    )


###############################################################################
# Test a fairly default case.


@pytest.mark.require_driver("AAIGRID")
def test_gdal_sieve_1(script_path, tmp_path):

    test_tif = str(tmp_path / "sieve_1.tif")

    drv = gdal.GetDriverByName("GTiff")
    dst_ds = drv.Create(test_tif, 5, 7, 1, gdal.GDT_Byte)
    dst_ds = None

    _, err = test_py_scripts.run_py_script(
        script_path,
        "gdal_sieve",
        "-nomask -st 2 -4 "
        + test_py_scripts.get_data_path("alg")
        + f"sieve_src.grd {test_tif}",
        return_stderr=True,
    )
    assert "UseExceptions" not in err

    dst_ds = gdal.Open(test_tif)
    dst_band = dst_ds.GetRasterBand(1)
    assert dst_band.GetNoDataValue() == 132  # nodata value of alg/sieve_src.grd

    cs_expected = 364
    cs = dst_band.Checksum()

    dst_band = None
    dst_ds = None

    assert cs == cs_expected, "got wrong checksum"


###############################################################################
# Test with source dataset without nodata


def test_gdal_sieve_src_without_nodata(script_path, tmp_path):

    test_tif = str(tmp_path / "test_gdal_sieve_src_without_nodata.tif")

    test_py_scripts.run_py_script(
        script_path,
        "gdal_sieve",
        "-st 0 " + test_py_scripts.get_data_path("gcore") + f"byte.tif {test_tif}",
    )

    dst_ds = gdal.Open(test_tif)
    dst_band = dst_ds.GetRasterBand(1)
    assert dst_band.GetNoDataValue() is None
    assert dst_band.Checksum() == 4672

    dst_band = None
    dst_ds = None
