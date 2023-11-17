#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdalcompare.py testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2023, Even Rouault <even dot rouault at spatialys.com>
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
from osgeo_utils import gdalcompare

pytestmark = pytest.mark.skipif(
    test_py_scripts.get_py_script("gdalcompare") is None,
    reason="gdalcompare not available",
)


@pytest.fixture()
def script_path():
    return test_py_scripts.get_py_script("gdalcompare")


@pytest.fixture()
def captured_print():
    def noop_print(*args, **kwargs):
        pass

    ori_print = gdalcompare.my_print
    gdalcompare.my_print = noop_print
    try:
        yield
    finally:
        gdalcompare.my_print = ori_print


@pytest.fixture()
def source_filename(tmp_vsimem):
    filename = str(tmp_vsimem / "src.tif")
    gdal.FileFromMemBuffer(filename, open("../gcore/data/byte.tif", "rb").read())
    return filename


###############################################################################
#


def test_gdalcompare_help(script_path):

    assert "ERROR" not in test_py_scripts.run_py_script(
        script_path, "gdalcompare", "--help"
    )


###############################################################################
#


def test_gdalcompare_version(script_path):

    assert "ERROR" not in test_py_scripts.run_py_script(
        script_path, "gdalcompare", "--version"
    )


###############################################################################


def test_gdalcompare_same(script_path, tmp_path):

    source_filename = str(tmp_path / "src.tif")
    shutil.copy("../gcore/data/byte.tif", source_filename)
    ret = test_py_scripts.run_py_script(
        script_path, "gdalcompare", f"{source_filename} {source_filename}"
    )
    assert "Differences Found: 0" in ret


###############################################################################


def test_gdalcompare_different_type(script_path, tmp_path):

    source_filename = str(tmp_path / "src.tif")
    shutil.copy("../gcore/data/byte.tif", source_filename)
    ret = test_py_scripts.run_py_script(
        script_path, "gdalcompare", f"{source_filename} ../gcore/data/uint16.tif"
    )
    assert "Differences Found: 0" not in ret
    assert "Band 1 pixel types differ" in ret


###############################################################################


def test_gdalcompare_different_pixel_content(
    tmp_vsimem, captured_print, source_filename
):

    golden_filename = source_filename
    filename = str(tmp_vsimem / "new.tif")
    gdal.Translate(filename, golden_filename, options="-scale 0 1 0 0")
    assert (
        gdalcompare.find_diff(golden_filename, filename, options=["SKIP_BINARY"]) == 1
    )

    prefix = str(tmp_vsimem / "")
    gdalcompare.find_diff(
        golden_filename,
        filename,
        options=["SKIP_BINARY", "DUMP_DIFFS", "DUMP_DIFFS_PREFIX=" + prefix],
    )
    ds = gdal.Open(prefix + "1.tif")
    assert ds.GetRasterBand(1).Checksum() == 4672


###############################################################################


def test_gdalcompare_different_band_count(tmp_vsimem, captured_print, source_filename):

    golden_filename = source_filename
    filename = str(tmp_vsimem / "new.tif")
    gdal.Translate(filename, golden_filename, options="-b 1 -b 1")
    assert (
        gdalcompare.find_diff(golden_filename, filename, options=["SKIP_BINARY"]) == 1
    )
    assert (
        gdalcompare.find_diff(filename, golden_filename, options=["SKIP_BINARY"]) == 1
    )


###############################################################################


def test_gdalcompare_different_dimension(tmp_vsimem, captured_print, source_filename):

    golden_filename = source_filename
    filename = str(tmp_vsimem / "new.tif")
    gdal.Translate(filename, golden_filename, options="-srcwin 0 0 20 19")
    assert (
        gdalcompare.find_diff(golden_filename, filename, options=["SKIP_BINARY"]) == 1
    )


###############################################################################


def test_gdalcompare_different_nodata(tmp_vsimem, captured_print, source_filename):

    golden_filename = "../gcore/data/byte.tif"
    filename = str(tmp_vsimem / "new.tif")
    gdal.Translate(filename, golden_filename, options="-a_nodata 1")
    # diff_count = 2 because of mask flags as well
    assert (
        gdalcompare.find_diff(golden_filename, filename, options=["SKIP_BINARY"]) == 2
    )
    assert (
        gdalcompare.find_diff(filename, golden_filename, options=["SKIP_BINARY"]) == 2
    )


###############################################################################


def test_gdalcompare_different_nodata_nan(tmp_vsimem, captured_print, source_filename):

    golden_filename = str(tmp_vsimem / "golden.tif")
    filename = str(tmp_vsimem / "new.tif")
    gdal.Translate(
        golden_filename, source_filename, options="-ot Float32 -a_nodata nan"
    )
    gdal.Translate(filename, source_filename, options="-ot Float32 -a_nodata 5")
    assert (
        gdalcompare.find_diff(golden_filename, golden_filename, options=["SKIP_BINARY"])
        == 0
    )
    assert (
        gdalcompare.find_diff(golden_filename, filename, options=["SKIP_BINARY"]) == 1
    )
    assert (
        gdalcompare.find_diff(golden_filename, filename, options=["SKIP_BINARY"]) == 1
    )


###############################################################################


def test_gdalcompare_different_srs(tmp_vsimem, captured_print, source_filename):

    golden_filename = source_filename
    filename = str(tmp_vsimem / "new.tif")

    gdal.Translate(filename, golden_filename, options="-a_srs EPSG:4326")
    assert (
        gdalcompare.find_diff(golden_filename, filename, options=["SKIP_BINARY"]) == 1
    )
    assert (
        gdalcompare.find_diff(
            golden_filename, filename, options=["SKIP_SRS", "SKIP_BINARY"]
        )
        == 0
    )

    ds = gdal.Translate(filename, golden_filename)
    ds.SetSpatialRef(None)
    ds = None
    assert (
        gdalcompare.find_diff(golden_filename, filename, options=["SKIP_BINARY"]) != 0
    )
    assert (
        gdalcompare.find_diff(filename, golden_filename, options=["SKIP_BINARY"]) != 0
    )


###############################################################################


def test_gdalcompare_different_geotransform(
    tmp_vsimem, captured_print, source_filename
):

    golden_filename = source_filename
    filename = str(tmp_vsimem / "new.tif")
    gdal.Translate(filename, golden_filename, options="-a_ullr 0 1 1 0")
    assert (
        gdalcompare.find_diff(golden_filename, filename, options=["SKIP_BINARY"]) == 1
    )
    assert (
        gdalcompare.find_diff(
            golden_filename, filename, options=["SKIP_GEOTRANSFORM", "SKIP_BINARY"]
        )
        == 0
    )

    ds = gdal.Translate(filename, golden_filename)
    ds.SetGeoTransform([0, 0, 0, 0, 0, 0])
    ds = None
    assert (
        gdalcompare.find_diff(golden_filename, filename, options=["SKIP_BINARY"]) != 0
    )
    assert (
        gdalcompare.find_diff(filename, golden_filename, options=["SKIP_BINARY"]) != 0
    )


###############################################################################


def test_gdalcompare_different_metadata(tmp_vsimem, captured_print, source_filename):

    golden_filename = source_filename
    filename = str(tmp_vsimem / "new.tif")
    gdal.Translate(filename, golden_filename, options="-mo FOO=BAR")
    assert (
        gdalcompare.find_diff(golden_filename, filename, options=["SKIP_BINARY"]) == 1
    )
    assert (
        gdalcompare.find_diff(filename, golden_filename, options=["SKIP_BINARY"]) == 2
    )
    assert (
        gdalcompare.find_diff(
            golden_filename, filename, options=["SKIP_METADATA", "SKIP_BINARY"]
        )
        == 0
    )


###############################################################################


def test_gdalcompare_different_overview(tmp_vsimem, captured_print, source_filename):

    golden_filename = str(tmp_vsimem / "golden.tif")
    gdal.FileFromMemBuffer(golden_filename, open("../gcore/data/byte.tif", "rb").read())
    ds = gdal.Open(golden_filename, gdal.GA_Update)
    ds.BuildOverviews("NEAR", [2])
    ds = None
    assert (
        gdalcompare.find_diff(golden_filename, golden_filename, options=["SKIP_BINARY"])
        == 0
    )

    filename = str(tmp_vsimem / "new.tif")
    gdal.Translate(filename, source_filename)
    assert (
        gdalcompare.find_diff(golden_filename, filename, options=["SKIP_BINARY"]) == 1
    )
    assert (
        gdalcompare.find_diff(filename, golden_filename, options=["SKIP_BINARY"]) == 1
    )

    ds = gdal.Translate(filename, source_filename)
    ds.BuildOverviews("AVERAGE", [2])
    ds = None
    assert (
        gdalcompare.find_diff(golden_filename, filename, options=["SKIP_BINARY"]) == 1
    )
