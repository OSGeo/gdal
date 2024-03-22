#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdalbuildvrtofvrt.py testing
# Author:   Even Rouault <even dot rouault at spatialys dot com>
#
###############################################################################
# Copyright (c) 2024, Even Rouault <even dot rouault at spatialys dot com>
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

from osgeo import gdal

pytestmark = [
    pytest.mark.require_driver("GPKG"),
    pytest.mark.skipif(
        test_py_scripts.get_py_script("gdalbuildvrtofvrt") is None,
        reason="gdalbuildvrtofvrt.py not available",
    ),
]


@pytest.fixture()
def script_path():
    return test_py_scripts.get_py_script("gdalbuildvrtofvrt")


###############################################################################
# Test


def test_gdalbuildvrtofvrt_basic(script_path, tmp_path):

    out_filename = str(tmp_path / "out.vrt")

    for j in range(8):
        for i in range(15):
            tif_filename = str(tmp_path / f"src_{i}_{j}.tif")
            ds = gdal.GetDriverByName("GTiff").Create(tif_filename, 20, 10)
            ds.SetGeoTransform([i * 20, 1, 0, -j * 10, 0, -1])
            ds = None

    max_files_per_vrt = 10
    test_py_scripts.run_py_script(
        script_path,
        "gdalbuildvrtofvrt",
        f" --max-files-per-vrt {max_files_per_vrt} {out_filename} {tmp_path}/*.tif",
    )

    ds = gdal.Open(out_filename)
    assert ds.RasterXSize == 15 * 20
    assert ds.RasterYSize == 8 * 10
    assert len(ds.GetFileList()) >= (8 * 15) // max_files_per_vrt


###############################################################################
# Test


def test_gdalbuildvrtofvrt_intermediate_vrt_path(script_path, tmp_path):

    out_filename = str(tmp_path / "out.vrt")
    intermediate_vrt_path = tmp_path / "intermediate_vrt_path"
    os.mkdir(intermediate_vrt_path, 0o755)

    src_filename = str(tmp_path / "src.tif")
    gdal.Translate(src_filename, "../gcore/data/byte.tif", width=1024)

    test_py_scripts.run_py_script(
        script_path,
        "gdalbuildvrtofvrt",
        f" --intermediate-vrt-path {intermediate_vrt_path} -r average --intermediate-vrt-add-overviews --overview-compression DEFLATE {out_filename} {src_filename}",
    )

    assert os.path.exists(str(intermediate_vrt_path / "out_0_0.vrt"))
    assert os.path.exists(str(intermediate_vrt_path / "out_0_0.vrt.ovr"))

    ds = gdal.Open(out_filename)
    assert ds.RasterXSize == 1024
    assert ds.RasterYSize == 1024

    ds = gdal.Open(intermediate_vrt_path / "out_0_0.vrt.ovr")
    assert ds.GetRasterBand(1).GetOverviewCount() == 1
    assert ds.GetRasterBand(1).GetMetadataItem("RESAMPLING") == "AVERAGE"
    assert ds.GetMetadataItem("COMPRESSION", "IMAGE_STRUCTURE") == "DEFLATE"


###############################################################################
# Test


def test_gdalbuildvrtofvrt_intermediate_vrt_path_specified_ovr_factors(
    script_path, tmp_path
):

    out_filename = str(tmp_path / "out.vrt")

    src_filename = str(tmp_path / "src.tif")
    gdal.Translate(src_filename, "../gcore/data/byte.tif", width=1024)

    test_py_scripts.run_py_script(
        script_path,
        "gdalbuildvrtofvrt",
        f" --intermediate-vrt-overview-factors 2,4,8 {out_filename} {src_filename}",
    )

    ds = gdal.Open(out_filename)
    assert ds.RasterXSize == 1024
    assert ds.RasterYSize == 1024

    ds = gdal.Open(tmp_path / "out_0_0.vrt.ovr")
    assert ds.GetRasterBand(1).GetOverviewCount() == 2
