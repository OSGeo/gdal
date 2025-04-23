#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdalmove testing
# Author:   Even Rouault <even dot rouault @ spatialys dot com>
#
###############################################################################
# Copyright (c) 2015, Even Rouault <even dot rouault @ spatialys dot com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import shutil

import gdaltest
import pytest
import test_py_scripts

from osgeo import gdal

pytestmark = pytest.mark.skipif(
    test_py_scripts.get_py_script("gdalmove") is None,
    reason="gdalmove.py not available",
)


@pytest.fixture()
def script_path():
    return test_py_scripts.get_py_script("gdalmove")


###############################################################################
#


def test_gdalmove_help(script_path):

    if gdaltest.is_travis_branch("sanitize"):
        pytest.skip("fails on sanitize for unknown reason")

    assert "ERROR" not in test_py_scripts.run_py_script(
        script_path, "gdalmove", "--help"
    )


###############################################################################
#


def test_gdalmove_version(script_path):

    if gdaltest.is_travis_branch("sanitize"):
        pytest.skip("fails on sanitize for unknown reason")

    assert "ERROR" not in test_py_scripts.run_py_script(
        script_path, "gdalmove", "--version"
    )


###############################################################################
#


def test_gdalmove_1(script_path, tmp_path):

    test_tif = str(tmp_path / "test_gdalmove_1.tif")

    shutil.copy(test_py_scripts.get_data_path("gcore") + "byte.tif", test_tif)

    _, err = test_py_scripts.run_py_script(
        script_path,
        "gdalmove",
        f'-s_srs "+proj=utm +zone=11 +ellps=clrk66 +towgs84=0,0,0 +no_defs" -t_srs EPSG:32611 {test_tif} -et 1',
        return_stderr=True,
    )
    assert "UseExceptions" not in err

    ds = gdal.Open(test_tif)
    got_gt = ds.GetGeoTransform()
    expected_gt = (
        440719.95870935748,
        60.000041745067577,
        1.9291142234578728e-05,
        3751294.2109841029,
        1.9099167548120022e-05,
        -60.000041705276814,
    )
    for i in range(6):
        assert abs(got_gt[i] - expected_gt[i]) / abs(got_gt[i]) <= 1e-5, "bad gt"
    wkt = ds.GetProjection()
    assert "32611" in wkt, "bad geotransform"
    ds = None
