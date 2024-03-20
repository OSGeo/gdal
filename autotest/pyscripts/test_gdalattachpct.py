#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdalattachpct.py testing
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

import pytest
import test_py_scripts

from osgeo import gdal

pytestmark = [
    pytest.mark.skipif(
        test_py_scripts.get_py_script("gdalattachpct") is None,
        reason="gdalattachpct.py not available",
    ),
]


@pytest.fixture()
def script_path():
    return test_py_scripts.get_py_script("gdalattachpct")


@pytest.fixture()
def palette_file(tmp_path):
    palette_filename = str(tmp_path / "pal.txt")
    with open(palette_filename, "wt") as f:
        f.write("0 1 2 3\n")
        f.write("255 254 253 252\n")
    return palette_filename


###############################################################################
# Basic test


def test_gdalattachpct_basic(script_path, tmp_path, palette_file):

    src_filename = str(tmp_path / "src.tif")
    ds = gdal.GetDriverByName("GTiff").Create(src_filename, 1, 1)
    ds.GetRasterBand(1).Fill(1)
    ds = None

    out_filename = str(tmp_path / "dst.tif")

    test_py_scripts.run_py_script(
        script_path,
        "gdalattachpct",
        f" {palette_file} {src_filename} {out_filename}",
    )

    ds = gdal.Open(out_filename)
    assert ds.GetDriver().ShortName == "GTiff"
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_PaletteIndex
    ct = ds.GetRasterBand(1).GetColorTable()
    assert ct
    assert ct.GetCount() == 256
    assert ct.GetColorEntry(0) == (1, 2, 3, 255)
    assert ct.GetColorEntry(255) == (254, 253, 252, 255)
    assert ds.GetRasterBand(1).Checksum() == 1


###############################################################################
# Test outputing to VRT


def test_gdalattachpct_vrt_output(script_path, tmp_path, palette_file):

    src_filename = str(tmp_path / "src.tif")
    ds = gdal.GetDriverByName("GTiff").Create(src_filename, 1, 1)
    ds.GetRasterBand(1).Fill(1)
    ds = None

    out_filename = str(tmp_path / "dst.vrt")

    test_py_scripts.run_py_script(
        script_path,
        "gdalattachpct",
        f" {palette_file} {src_filename} {out_filename}",
    )

    ds = gdal.Open(out_filename)
    assert ds.GetDriver().ShortName == "VRT"
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_PaletteIndex
    ct = ds.GetRasterBand(1).GetColorTable()
    assert ct
    assert ct.GetCount() == 256
    assert ct.GetColorEntry(0) == (1, 2, 3, 255)
    assert ct.GetColorEntry(255) == (254, 253, 252, 255)
    assert ds.GetRasterBand(1).Checksum() == 1

    # Check source file is not altered
    ds = gdal.Open(src_filename)
    assert ds.GetRasterBand(1).GetColorTable() is None
