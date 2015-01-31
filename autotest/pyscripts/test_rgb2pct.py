#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  rgb2pct.py and pct2rgb.py testing
# Author:   Even Rouault <even dot rouault @ mines-paris dot org>
#
###############################################################################
# Copyright (c) 2010, Even Rouault <even dot rouault at mines-paris dot org>
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


import sys
import os
import struct

sys.path.append( '../pymod' )

from osgeo import gdal
import gdaltest
import test_py_scripts

###############################################################################
# Test rgb2pct

def test_rgb2pct_1():

    script_path = test_py_scripts.get_py_script('rgb2pct')
    if script_path is None:
        return 'skip'

    test_py_scripts.run_py_script(script_path, 'rgb2pct', '../gcore/data/rgbsmall.tif tmp/test_rgb2pct_1.tif' )

    ds = gdal.Open('tmp/test_rgb2pct_1.tif')
    if ds.GetRasterBand(1).Checksum() != 31231:
        print(ds.GetRasterBand(1).Checksum())
        return 'fail'
    ds = None

    return 'success'

###############################################################################
# Test pct2rgb

def test_pct2rgb_1():
    try:
        from osgeo import gdalnumeric
        gdalnumeric.BandRasterIONumPy
    except:
        return 'skip'

    script_path = test_py_scripts.get_py_script('pct2rgb')
    if script_path is None:
        return 'skip'

    test_py_scripts.run_py_script(script_path, 'pct2rgb', 'tmp/test_rgb2pct_1.tif tmp/test_pct2rgb_1.tif' )

    ds = gdal.Open('tmp/test_pct2rgb_1.tif')
    if ds.GetRasterBand(1).Checksum() != 20963:
        print(ds.GetRasterBand(1).Checksum())
        return 'fail'

    ori_ds = gdal.Open('../gcore/data/rgbsmall.tif')
    max_diff = gdaltest.compare_ds(ori_ds, ds)
    if max_diff > 18:
        return 'fail'

    ds = None
    ori_ds = None

    return 'success'

###############################################################################
# Test rgb2pct -n option

def test_rgb2pct_2():

    script_path = test_py_scripts.get_py_script('rgb2pct')
    if script_path is None:
        return 'skip'

    test_py_scripts.run_py_script(script_path, 'rgb2pct', '-n 16 ../gcore/data/rgbsmall.tif tmp/test_rgb2pct_2.tif')

    ds = gdal.Open('tmp/test_rgb2pct_2.tif')
    if ds.GetRasterBand(1).Checksum() != 16596:
        print(ds.GetRasterBand(1).Checksum())
        return 'fail'

    ct = ds.GetRasterBand(1).GetRasterColorTable()
    for i in range(16, 255):
        entry = ct.GetColorEntry(i)
        if not (entry[0] == 0 and entry[1] == 0 and entry[2] == 0):
            gdaltest.post_reason('Color table has more than 16 entries')
            return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test rgb2pct -pct option

def test_rgb2pct_3():

    script_path = test_py_scripts.get_py_script('rgb2pct')
    if script_path is None:
        return 'skip'

    test_py_scripts.run_py_script(script_path, 'rgb2pct', '-pct tmp/test_rgb2pct_2.tif ../gcore/data/rgbsmall.tif tmp/test_rgb2pct_3.tif' )

    ds = gdal.Open('tmp/test_rgb2pct_3.tif')
    if ds.GetRasterBand(1).Checksum() != 16596:
        print(ds.GetRasterBand(1).Checksum())
        return 'fail'

    ct = ds.GetRasterBand(1).GetRasterColorTable()
    for i in range(16, 255):
        entry = ct.GetColorEntry(i)
        if not (entry[0] == 0 and entry[1] == 0 and entry[2] == 0):
            gdaltest.post_reason('Color table has more than 16 entries')
            return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test pct2rgb with big CT (>256 entries)


def test_pct2rgb_4():
    try:
        from osgeo import gdalnumeric
        gdalnumeric.BandRasterIONumPy
    except:
        return 'skip'

    script_path = test_py_scripts.get_py_script('pct2rgb')
    if script_path is None:
        return 'skip'

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

    if entry != data:
        return 'fail'

    ds = None
    ori_ds = None

    return 'success'


###############################################################################
# Cleanup

def test_rgb2pct_cleanup():

    lst = [ 'tmp/test_rgb2pct_1.tif',
            'tmp/test_pct2rgb_1.tif',
            'tmp/test_rgb2pct_2.tif',
            'tmp/test_rgb2pct_3.tif',
            'tmp/test_pct2rgb_1.tif',
            'tmp/test_pct2rgb_4.tif' ]
    for filename in lst:
        try:
            os.remove(filename)
        except:
            pass

    return 'success'

gdaltest_list = [
    test_rgb2pct_1,
    test_pct2rgb_1,
    test_rgb2pct_2,
    test_rgb2pct_3,
    test_pct2rgb_4,
    test_rgb2pct_cleanup
    ]


if __name__ == '__main__':

    gdaltest.setup_run( 'test_rgb2pct' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()
