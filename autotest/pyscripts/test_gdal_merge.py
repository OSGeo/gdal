#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdal_merge.py testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2010, Even Rouault <even dot rouault at spatialys.com>
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


import os

import pytest
import test_py_scripts

from osgeo import gdal, osr

pytestmark = pytest.mark.skipif(
    test_py_scripts.get_py_script("gdal_merge") is None,
    reason="gdal_merge not available",
)


@pytest.fixture()
def script_path():
    return test_py_scripts.get_py_script("gdal_merge")


@pytest.fixture(scope="module")
def sample_tifs(tmp_path_factory):
    tmpdir = tmp_path_factory.mktemp("tmp")

    drv = gdal.GetDriverByName("GTiff")
    srs = osr.SpatialReference()
    srs.SetWellKnownGeogCS("WGS84")
    wkt = srs.ExportToWkt()

    sample1_tif = str(tmpdir / "in1.tif")
    with drv.Create(sample1_tif, 10, 10, 1) as ds:
        ds.SetProjection(wkt)
        ds.SetGeoTransform([2, 0.1, 0, 49, 0, -0.1])
        ds.GetRasterBand(1).Fill(0)

    sample2_tif = str(tmpdir / "in2.tif")
    with drv.Create(sample2_tif, 10, 10, 1) as ds:
        ds.SetProjection(wkt)
        ds.SetGeoTransform([3, 0.1, 0, 49, 0, -0.1])
        ds.GetRasterBand(1).Fill(63)

    sample3_tif = str(tmpdir / "in3.tif")
    with drv.Create(sample3_tif, 10, 10, 1) as ds:
        ds.SetProjection(wkt)
        ds.SetGeoTransform([2, 0.1, 0, 48, 0, -0.1])
        ds.GetRasterBand(1).Fill(127)

    sample4_tif = str(tmpdir / "in4.tif")
    with drv.Create(sample4_tif, 10, 10, 1) as ds:
        ds.SetProjection(wkt)
        ds.SetGeoTransform([3, 0.1, 0, 48, 0, -0.1])
        ds.GetRasterBand(1).Fill(255)

    yield (sample1_tif, sample2_tif, sample3_tif, sample4_tif)


###############################################################################
#


def test_gdal_merge_help(script_path):

    assert "ERROR" not in test_py_scripts.run_py_script(
        script_path, "gdal_merge", "--help"
    )


###############################################################################
#


def test_gdal_merge_version(script_path):

    assert "ERROR" not in test_py_scripts.run_py_script(
        script_path, "gdal_merge", "--version"
    )


###############################################################################
# Basic test


def test_gdal_merge_1(script_path, tmp_path):

    output_tif = str(tmp_path / "test_gdal_merge_1.tif")

    _, err = test_py_scripts.run_py_script(
        script_path,
        "gdal_merge",
        f"-o {output_tif} " + test_py_scripts.get_data_path("gcore") + "byte.tif",
        return_stderr=True,
    )
    assert "UseExceptions" not in err

    ds = gdal.Open(output_tif)
    assert ds.GetRasterBand(1).Checksum() == 4672
    ds = None


###############################################################################
# Merge 4 tiles


def test_gdal_merge_2(script_path, tmp_path, sample_tifs):

    output_tif = str(tmp_path / "test_gdal_merge_2.tif")

    test_py_scripts.run_py_script(
        script_path,
        "gdal_merge",
        f"-q -o {output_tif} {' '.join(sample_tifs)}",
    )

    ds = gdal.Open(output_tif)
    assert ds.GetProjectionRef().find("WGS 84") != -1, "Expected WGS 84\nGot : %s" % (
        ds.GetProjectionRef()
    )

    gt = ds.GetGeoTransform()
    expected_gt = [2, 0.1, 0, 49, 0, -0.1]
    for i in range(6):
        assert not abs(gt[i] - expected_gt[i] > 1e-5), "Expected : %s\nGot : %s" % (
            expected_gt,
            gt,
        )

    assert (
        ds.RasterXSize == 20 and ds.RasterYSize == 20
    ), "Wrong raster dimensions : %d x %d" % (ds.RasterXSize, ds.RasterYSize)

    assert ds.RasterCount == 1, "Wrong raster count : %d " % (ds.RasterCount)

    assert ds.GetRasterBand(1).Checksum() == 3508, "Wrong checksum"


###############################################################################
# Test -separate and -v options


def test_gdal_merge_3(script_path, tmp_path, sample_tifs):

    output_tif = str(tmp_path / "test_gdal_merge_3.tif")

    test_py_scripts.run_py_script(
        script_path,
        "gdal_merge",
        f"-separate -v -o {output_tif} {' '.join(sample_tifs)}",
    )

    ds = gdal.Open(output_tif)
    assert ds.GetProjectionRef().find("WGS 84") != -1, "Expected WGS 84\nGot : %s" % (
        ds.GetProjectionRef()
    )

    gt = ds.GetGeoTransform()
    expected_gt = [2, 0.1, 0, 49, 0, -0.1]
    for i in range(6):
        assert not abs(gt[i] - expected_gt[i] > 1e-5), "Expected : %s\nGot : %s" % (
            expected_gt,
            gt,
        )

    assert (
        ds.RasterXSize == 20 and ds.RasterYSize == 20
    ), "Wrong raster dimensions : %d x %d" % (ds.RasterXSize, ds.RasterYSize)

    assert ds.RasterCount == 4, "Wrong raster count : %d " % (ds.RasterCount)

    assert ds.GetRasterBand(1).Checksum() == 0, "Wrong checksum"


###############################################################################
# Test -init option


def test_gdal_merge_4(script_path, tmp_path, sample_tifs):

    output_tif = str(tmp_path / "test_gdal_merge_4.tif")

    test_py_scripts.run_py_script(
        script_path,
        "gdal_merge",
        f"-init 255 -o {output_tif} {sample_tifs[1]} {sample_tifs[2]}",
    )

    ds = gdal.Open(output_tif)

    assert ds.GetRasterBand(1).Checksum() == 4725, "Wrong checksum"


###############################################################################
# Test merging with alpha band (#3669)


def test_gdal_merge_5(script_path, tmp_path):
    gdal_array = pytest.importorskip("osgeo.gdal_array")
    try:
        gdal_array.BandRasterIONumPy
    except AttributeError:
        pytest.skip()

    drv = gdal.GetDriverByName("GTiff")
    srs = osr.SpatialReference()
    srs.SetWellKnownGeogCS("WGS84")
    wkt = srs.ExportToWkt()

    input1_tif = str(tmp_path / "in5.tif")
    with drv.Create(input1_tif, 10, 10, 4) as ds:
        ds.SetProjection(wkt)
        ds.SetGeoTransform([2, 0.1, 0, 49, 0, -0.1])
        ds.GetRasterBand(1).Fill(255)

    input2_tif = str(tmp_path / "in6.tif")
    with drv.Create(input2_tif, 10, 10, 4) as ds:
        ds.SetProjection(wkt)
        ds.SetGeoTransform([2, 0.1, 0, 49, 0, -0.1])
        ds.GetRasterBand(2).Fill(255)
        ds.GetRasterBand(4).Fill(255)
        cs = ds.GetRasterBand(4).Checksum()

    output_tif = str(tmp_path / "test_gdal_merge_5.tif")

    test_py_scripts.run_py_script(
        script_path,
        "gdal_merge",
        f" -o {output_tif} {input1_tif} {input2_tif}",
    )

    with gdal.Open(output_tif) as ds:

        assert ds.GetRasterBand(1).Checksum() == 0, "Wrong checksum"
        assert ds.GetRasterBand(2).Checksum() == cs, "Wrong checksum"
        assert ds.GetRasterBand(3).Checksum() == 0, "Wrong checksum"
        assert ds.GetRasterBand(4).Checksum() == cs, "Wrong checksum"

    os.unlink(output_tif)

    test_py_scripts.run_py_script(
        script_path,
        "gdal_merge",
        f" -o {output_tif} {input1_tif} {input2_tif}",
    )

    with gdal.Open(output_tif) as ds:

        assert ds.GetRasterBand(1).Checksum() == 0, "Wrong checksum"
        assert ds.GetRasterBand(2).Checksum() == cs, "Wrong checksum"
        assert ds.GetRasterBand(3).Checksum() == 0, "Wrong checksum"
        assert ds.GetRasterBand(4).Checksum() == cs, "Wrong checksum"
