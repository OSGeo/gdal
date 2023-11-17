#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  rgb2pct.py and pct2rgb.py testing
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


import struct

import gdaltest
import pytest
import test_py_scripts

import osgeo_utils.auxiliary.color_table as color_table
from osgeo import gdal
from osgeo_utils import gdalattachpct, rgb2pct

pytestmark = pytest.mark.skipif(
    test_py_scripts.get_py_script("rgb2pct") is None,
    reason="rgb2pct.py not available",
)


@pytest.fixture(scope="module")
def script_path():
    return test_py_scripts.get_py_script("rgb2pct")


###############################################################################
#


def test_rgb2pct_help(script_path):

    assert "ERROR" not in test_py_scripts.run_py_script(
        script_path, "rgb2pct", "--help"
    )


###############################################################################
#


def test_rgb2pct_version(script_path):

    assert "ERROR" not in test_py_scripts.run_py_script(
        script_path, "rgb2pct", "--version"
    )


@pytest.fixture(scope="module")
def rgb2pct1_tif(script_path, tmp_path_factory):

    tif_fname = str(tmp_path_factory.mktemp("tmp") / "test_rgb2pct_1.tif")

    test_py_scripts.run_py_script(
        script_path,
        "rgb2pct",
        test_py_scripts.get_data_path("gcore") + f"rgbsmall.tif {tif_fname}",
    )

    yield tif_fname


@pytest.fixture(scope="module")
def rgb2pct2_tif(script_path, tmp_path_factory):

    tif_fname = str(tmp_path_factory.mktemp("tmp") / "test_rgb2pct_2.tif")

    test_py_scripts.run_py_script(
        script_path,
        "rgb2pct",
        "-n 16 " + test_py_scripts.get_data_path("gcore") + f"rgbsmall.tif {tif_fname}",
    )

    yield tif_fname


###############################################################################
# Test rgb2pct


def test_rgb2pct_1(rgb2pct1_tif):

    with gdal.Open(rgb2pct1_tif) as ds:
        assert ds.GetRasterBand(1).Checksum() == 31231


###############################################################################
#


def test_pct2rgb_help(script_path):
    gdal_array = pytest.importorskip("osgeo.gdal_array")
    try:
        gdal_array.BandRasterIONumPy
    except AttributeError:
        pytest.skip("osgeo.gdal_array.BandRasterIONumPy is unavailable")

    assert "ERROR" not in test_py_scripts.run_py_script(
        script_path, "pct2rgb", "--help"
    )


###############################################################################
#


def test_pct2rgb_version(script_path):
    gdal_array = pytest.importorskip("osgeo.gdal_array")
    try:
        gdal_array.BandRasterIONumPy
    except AttributeError:
        pytest.skip("osgeo.gdal_array.BandRasterIONumPy is unavailable")

    assert "ERROR" not in test_py_scripts.run_py_script(
        script_path, "pct2rgb", "--version"
    )


###############################################################################
# Test pct2rgb


def test_pct2rgb_1(script_path, tmp_path, rgb2pct1_tif):
    gdal_array = pytest.importorskip("osgeo.gdal_array")
    try:
        gdal_array.BandRasterIONumPy
    except AttributeError:
        pytest.skip("osgeo.gdal_array.BandRasterIONumPy is unavailable")

    output_tif = str(tmp_path / "test_pct2rgb_1.tif")

    test_py_scripts.run_py_script(
        script_path, "pct2rgb", f"{rgb2pct1_tif} {output_tif}"
    )

    ds = gdal.Open(output_tif)
    assert ds.GetRasterBand(1).Checksum() == 20963

    ori_ds = gdal.Open(test_py_scripts.get_data_path("gcore") + "rgbsmall.tif")
    max_diff = gdaltest.compare_ds(ori_ds, ds)
    assert max_diff <= 18

    ds = None
    ori_ds = None


###############################################################################
# Test rgb2pct -n option


def test_rgb2pct_2(script_path, rgb2pct2_tif):

    ds = gdal.Open(rgb2pct2_tif)
    assert ds.GetRasterBand(1).Checksum() == 16596

    ct = ds.GetRasterBand(1).GetRasterColorTable()
    for i in range(16, 255):
        entry = ct.GetColorEntry(i)
        assert (
            entry[0] == 0 and entry[1] == 0 and entry[2] == 0
        ), "Color table has more than 16 entries"

    ds = None


###############################################################################
# Test rgb2pct -pct option


def test_rgb2pct_3(script_path, tmp_path, rgb2pct2_tif):

    output_tif = str(tmp_path / "test_rgb2pct_3.tif")

    test_py_scripts.run_py_script(
        script_path,
        "rgb2pct",
        f"-pct {rgb2pct2_tif} "
        + test_py_scripts.get_data_path("gcore")
        + f"rgbsmall.tif {output_tif}",
    )

    ds = gdal.Open(output_tif)
    assert ds.GetRasterBand(1).Checksum() == 16596

    ct = ds.GetRasterBand(1).GetRasterColorTable()
    for i in range(16, 255):
        entry = ct.GetColorEntry(i)
        assert (
            entry[0] == 0 and entry[1] == 0 and entry[2] == 0
        ), "Color table has more than 16 entries"

    ds = None


###############################################################################
# Test pct2rgb with big CT (>256 entries)


def test_pct2rgb_4(script_path, tmp_path):
    gdal_array = pytest.importorskip("osgeo.gdal_array")
    try:
        gdal_array.BandRasterIONumPy
    except AttributeError:
        pytest.skip("osgeo.gdal_array.BandRasterIONumPy is unavailable")

    output_tif = str(tmp_path / "test_pct2rgb_4.tif")

    test_py_scripts.run_py_script(
        script_path,
        "pct2rgb",
        "-rgba " + test_py_scripts.get_data_path("gcore") + f"rat.img {output_tif}",
    )

    ds = gdal.Open(output_tif)
    ori_ds = gdal.Open(test_py_scripts.get_data_path("gcore") + "rat.img")

    ori_data = struct.unpack(
        "H", ori_ds.GetRasterBand(1).ReadRaster(1990, 1990, 1, 1, 1, 1)
    )[0]
    data = (
        struct.unpack("B", ds.GetRasterBand(1).ReadRaster(1990, 1990, 1, 1, 1, 1))[0],
        struct.unpack("B", ds.GetRasterBand(2).ReadRaster(1990, 1990, 1, 1, 1, 1))[0],
        struct.unpack("B", ds.GetRasterBand(3).ReadRaster(1990, 1990, 1, 1, 1, 1))[0],
        struct.unpack("B", ds.GetRasterBand(4).ReadRaster(1990, 1990, 1, 1, 1, 1))[0],
    )

    ct = ori_ds.GetRasterBand(1).GetRasterColorTable()
    entry = ct.GetColorEntry(ori_data)

    assert entry == data

    ds = None
    ori_ds = None


def test_gdalattachpct_1(tmp_path, rgb2pct2_tif):
    pct_filename = rgb2pct2_tif
    src_filename = test_py_scripts.get_data_path("gcore") + "rgbsmall.tif"
    pct_filename4 = str(tmp_path / "test_gdalattachpct_1_4.txt")

    # pct from raster
    ct0 = color_table.get_color_table(pct_filename)
    ds1, err = rgb2pct.doit(src_filename=src_filename, pct_filename=pct_filename)
    ct1 = color_table.get_color_table(ds1)
    assert (
        err == 0
        and ds1 is not None
        and ct1 is not None
        and color_table.are_equal_color_table(ct0, ct1)
    )
    ds1 = None

    # generate some junk color table
    color2 = (1, 2, 3, 4)
    ct2a = color_table.get_fixed_color_table(color2)
    assert color_table.is_fixed_color_table(ct2a, color2)

    # assign junk color table
    ds2, err = gdalattachpct.doit(src_filename=src_filename, pct_filename=ct2a)
    ct2b = color_table.get_color_table(ds2)
    assert (
        err == 0 and ds2 is not None and color_table.are_equal_color_table(ct2a, ct2b)
    )
    ds2 = None

    # pct from gdal.ColorTable object
    ds3, err = gdalattachpct.doit(src_filename=src_filename, pct_filename=ct1)
    ct3 = color_table.get_color_table(ds3)
    assert err == 0 and ds3 is not None and color_table.are_equal_color_table(ct1, ct3)
    ds3 = None

    # write color table to a txt file
    color_table.write_color_table_to_file(ct3, pct_filename4)
    ct4 = color_table.get_color_table(pct_filename4)
    assert color_table.are_equal_color_table(ct3, ct4)

    # pct from txt file
    ds5, err = rgb2pct.doit(src_filename=src_filename, pct_filename=ct4)
    ct5 = color_table.get_color_table(ds5)
    assert (
        err == 0
        and ds5 is not None
        and ct5 is not None
        and color_table.are_equal_color_table(ct4, ct5)
    )
    ds5 = None

    ct1 = None
    ct2b = None
    ct2a = None
    ct3 = None
    ct4 = None
    ct5 = None
