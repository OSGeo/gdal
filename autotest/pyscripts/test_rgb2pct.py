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


import os
import struct


from osgeo import gdal
import gdaltest
import test_py_scripts
import pytest

###############################################################################
# Test rgb2pct


def test_rgb2pct_1():

    script_path = test_py_scripts.get_py_script('rgb2pct')
    if script_path is None:
        pytest.skip()

    test_py_scripts.run_py_script(script_path, 'rgb2pct', '../gcore/data/rgbsmall.tif tmp/test_rgb2pct_1.tif')

    ds = gdal.Open('tmp/test_rgb2pct_1.tif')
    assert ds.GetRasterBand(1).Checksum() == 31231
    ds = None

###############################################################################
# Test pct2rgb


def test_pct2rgb_1():
    try:
        from osgeo import gdalnumeric
        gdalnumeric.BandRasterIONumPy
    except:
        pytest.skip()

    script_path = test_py_scripts.get_py_script('pct2rgb')
    if script_path is None:
        pytest.skip()

    test_py_scripts.run_py_script(script_path, 'pct2rgb', 'tmp/test_rgb2pct_1.tif tmp/test_pct2rgb_1.tif')

    ds = gdal.Open('tmp/test_pct2rgb_1.tif')
    assert ds.GetRasterBand(1).Checksum() == 20963

    ori_ds = gdal.Open('../gcore/data/rgbsmall.tif')
    max_diff = gdaltest.compare_ds(ori_ds, ds)
    assert max_diff <= 18

    ds = None
    ori_ds = None

###############################################################################
# Test rgb2pct -n option


def test_rgb2pct_2():

    script_path = test_py_scripts.get_py_script('rgb2pct')
    if script_path is None:
        pytest.skip()

    test_py_scripts.run_py_script(script_path, 'rgb2pct', '-n 16 ../gcore/data/rgbsmall.tif tmp/test_rgb2pct_2.tif')

    ds = gdal.Open('tmp/test_rgb2pct_2.tif')
    assert ds.GetRasterBand(1).Checksum() == 16596

    ct = ds.GetRasterBand(1).GetRasterColorTable()
    for i in range(16, 255):
        entry = ct.GetColorEntry(i)
        assert (entry[0] == 0 and entry[1] == 0 and entry[2] == 0), \
            'Color table has more than 16 entries'

    ds = None

###############################################################################
# Test rgb2pct -pct option


def test_rgb2pct_3():

    script_path = test_py_scripts.get_py_script('rgb2pct')
    if script_path is None:
        pytest.skip()

    test_py_scripts.run_py_script(script_path, 'rgb2pct', '-pct tmp/test_rgb2pct_2.tif ../gcore/data/rgbsmall.tif tmp/test_rgb2pct_3.tif')

    ds = gdal.Open('tmp/test_rgb2pct_3.tif')
    assert ds.GetRasterBand(1).Checksum() == 16596

    ct = ds.GetRasterBand(1).GetRasterColorTable()
    for i in range(16, 255):
        entry = ct.GetColorEntry(i)
        assert (entry[0] == 0 and entry[1] == 0 and entry[2] == 0), \
            'Color table has more than 16 entries'

    ds = None

###############################################################################
# Test pct2rgb with big CT (>256 entries)


def test_pct2rgb_4():
    try:
        from osgeo import gdalnumeric
        gdalnumeric.BandRasterIONumPy
    except (ImportError, AttributeError):
        pytest.skip()

    script_path = test_py_scripts.get_py_script('pct2rgb')
    if script_path is None:
        pytest.skip()

    test_py_scripts.run_py_script(script_path, 'pct2rgb', '-rgba ../gcore/data/rat.img tmp/test_pct2rgb_4.tif')

    ds = gdal.Open('tmp/test_pct2rgb_4.tif')
    ori_ds = gdal.Open('../gcore/data/rat.img')

    ori_data = struct.unpack('H', ori_ds.GetRasterBand(1).ReadRaster(1990, 1990, 1, 1, 1, 1))[0]
    data = (struct.unpack('B', ds.GetRasterBand(1).ReadRaster(1990, 1990, 1, 1, 1, 1))[0],
            struct.unpack('B', ds.GetRasterBand(2).ReadRaster(1990, 1990, 1, 1, 1, 1))[0],
            struct.unpack('B', ds.GetRasterBand(3).ReadRaster(1990, 1990, 1, 1, 1, 1))[0],
            struct.unpack('B', ds.GetRasterBand(4).ReadRaster(1990, 1990, 1, 1, 1, 1))[0],)

    ct = ori_ds.GetRasterBand(1).GetRasterColorTable()
    entry = ct.GetColorEntry(ori_data)

    assert entry == data

    ds = None
    ori_ds = None


###############################################################################
# Cleanup

def test_rgb2pct_cleanup():

    lst = ['tmp/test_rgb2pct_1.tif',
           'tmp/test_pct2rgb_1.tif',
           'tmp/test_rgb2pct_2.tif',
           'tmp/test_rgb2pct_3.tif',
           'tmp/test_pct2rgb_1.tif',
           'tmp/test_pct2rgb_4.tif']
    for filename in lst:
        try:
            os.remove(filename)
        except OSError:
            pass

    



