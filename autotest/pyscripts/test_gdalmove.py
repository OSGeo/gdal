#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdalmove testing
# Author:   Even Rouault <even dot rouault @ spatialys dot com>
#
###############################################################################
# Copyright (c) 2015, Even Rouault <even dot rouault @ spatialys dot com>
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.
###############################################################################

import shutil

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

    assert "ERROR" not in test_py_scripts.run_py_script(
        script_path, "gdalmove", "--help"
    )


###############################################################################
#


def test_gdalmove_version(script_path):

    assert "ERROR" not in test_py_scripts.run_py_script(
        script_path, "gdalmove", "--version"
    )


###############################################################################
#


def test_gdalmove_1(script_path, tmp_path):

    test_tif = str(tmp_path / "test_gdalmove_1.tif")

    shutil.copy(test_py_scripts.get_data_path("gcore") + "byte.tif", test_tif)

    test_py_scripts.run_py_script(
        script_path,
        "gdalmove",
        f'-s_srs "+proj=utm +zone=11 +ellps=clrk66 +towgs84=0,0,0 +no_defs" -t_srs EPSG:32611 {test_tif} -et 1',
    )

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
