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


from osgeo import gdal
from osgeo import osr
import test_py_scripts
import pytest

###############################################################################
# Basic test


def test_gdal_merge_1():

    script_path = test_py_scripts.get_py_script('gdal_merge')
    if script_path is None:
        pytest.skip()

    test_py_scripts.run_py_script(script_path, 'gdal_merge', '-o tmp/test_gdal_merge_1.tif ' + test_py_scripts.get_data_path('gcore') + 'byte.tif')

    ds = gdal.Open('tmp/test_gdal_merge_1.tif')
    assert ds.GetRasterBand(1).Checksum() == 4672
    ds = None

###############################################################################
# Merge 4 tiles


def test_gdal_merge_2():

    script_path = test_py_scripts.get_py_script('gdal_merge')
    if script_path is None:
        pytest.skip()

    drv = gdal.GetDriverByName('GTiff')
    srs = osr.SpatialReference()
    srs.SetWellKnownGeogCS('WGS84')
    wkt = srs.ExportToWkt()

    ds = drv.Create('tmp/in1.tif', 10, 10, 1)
    ds.SetProjection(wkt)
    ds.SetGeoTransform([2, 0.1, 0, 49, 0, -0.1])
    ds.GetRasterBand(1).Fill(0)
    ds = None

    ds = drv.Create('tmp/in2.tif', 10, 10, 1)
    ds.SetProjection(wkt)
    ds.SetGeoTransform([3, 0.1, 0, 49, 0, -0.1])
    ds.GetRasterBand(1).Fill(63)
    ds = None

    ds = drv.Create('tmp/in3.tif', 10, 10, 1)
    ds.SetProjection(wkt)
    ds.SetGeoTransform([2, 0.1, 0, 48, 0, -0.1])
    ds.GetRasterBand(1).Fill(127)
    ds = None

    ds = drv.Create('tmp/in4.tif', 10, 10, 1)
    ds.SetProjection(wkt)
    ds.SetGeoTransform([3, 0.1, 0, 48, 0, -0.1])
    ds.GetRasterBand(1).Fill(255)
    ds = None

    test_py_scripts.run_py_script(script_path, 'gdal_merge', '-q -o tmp/test_gdal_merge_2.tif tmp/in1.tif tmp/in2.tif tmp/in3.tif tmp/in4.tif')

    ds = gdal.Open('tmp/test_gdal_merge_2.tif')
    assert ds.GetProjectionRef().find('WGS 84') != -1, \
        ('Expected WGS 84\nGot : %s' % (ds.GetProjectionRef()))

    gt = ds.GetGeoTransform()
    expected_gt = [2, 0.1, 0, 49, 0, -0.1]
    for i in range(6):
        assert not abs(gt[i] - expected_gt[i] > 1e-5), \
            ('Expected : %s\nGot : %s' % (expected_gt, gt))

    assert ds.RasterXSize == 20 and ds.RasterYSize == 20, \
        ('Wrong raster dimensions : %d x %d' % (ds.RasterXSize, ds.RasterYSize))

    assert ds.RasterCount == 1, ('Wrong raster count : %d ' % (ds.RasterCount))

    assert ds.GetRasterBand(1).Checksum() == 3508, 'Wrong checksum'

###############################################################################
# Test -separate and -v options


def test_gdal_merge_3():

    script_path = test_py_scripts.get_py_script('gdal_merge')
    if script_path is None:
        pytest.skip()

    test_py_scripts.run_py_script(script_path, 'gdal_merge', '-separate -v -o tmp/test_gdal_merge_3.tif tmp/in1.tif tmp/in2.tif tmp/in3.tif tmp/in4.tif')

    ds = gdal.Open('tmp/test_gdal_merge_3.tif')
    assert ds.GetProjectionRef().find('WGS 84') != -1, \
        ('Expected WGS 84\nGot : %s' % (ds.GetProjectionRef()))

    gt = ds.GetGeoTransform()
    expected_gt = [2, 0.1, 0, 49, 0, -0.1]
    for i in range(6):
        assert not abs(gt[i] - expected_gt[i] > 1e-5), \
            ('Expected : %s\nGot : %s' % (expected_gt, gt))

    assert ds.RasterXSize == 20 and ds.RasterYSize == 20, \
        ('Wrong raster dimensions : %d x %d' % (ds.RasterXSize, ds.RasterYSize))

    assert ds.RasterCount == 4, ('Wrong raster count : %d ' % (ds.RasterCount))

    assert ds.GetRasterBand(1).Checksum() == 0, 'Wrong checksum'

###############################################################################
# Test -init option


def test_gdal_merge_4():

    script_path = test_py_scripts.get_py_script('gdal_merge')
    if script_path is None:
        pytest.skip()

    test_py_scripts.run_py_script(script_path, 'gdal_merge', '-init 255 -o tmp/test_gdal_merge_4.tif tmp/in2.tif tmp/in3.tif')

    ds = gdal.Open('tmp/test_gdal_merge_4.tif')

    assert ds.GetRasterBand(1).Checksum() == 4725, 'Wrong checksum'

###############################################################################
# Test merging with alpha band (#3669)


def test_gdal_merge_5():
    try:
        from osgeo import gdal_array
        gdal_array.BandRasterIONumPy
    except (ImportError, AttributeError):
        pytest.skip()

    script_path = test_py_scripts.get_py_script('gdal_merge')
    if script_path is None:
        pytest.skip()

    drv = gdal.GetDriverByName('GTiff')
    srs = osr.SpatialReference()
    srs.SetWellKnownGeogCS('WGS84')
    wkt = srs.ExportToWkt()

    ds = drv.Create('tmp/in5.tif', 10, 10, 4)
    ds.SetProjection(wkt)
    ds.SetGeoTransform([2, 0.1, 0, 49, 0, -0.1])
    ds.GetRasterBand(1).Fill(255)
    ds = None

    ds = drv.Create('tmp/in6.tif', 10, 10, 4)
    ds.SetProjection(wkt)
    ds.SetGeoTransform([2, 0.1, 0, 49, 0, -0.1])
    ds.GetRasterBand(2).Fill(255)
    ds.GetRasterBand(4).Fill(255)
    cs = ds.GetRasterBand(4).Checksum()
    ds = None

    test_py_scripts.run_py_script(script_path, 'gdal_merge', ' -o tmp/test_gdal_merge_5.tif tmp/in5.tif tmp/in6.tif')

    ds = gdal.Open('tmp/test_gdal_merge_5.tif')

    assert ds.GetRasterBand(1).Checksum() == 0, 'Wrong checksum'
    assert ds.GetRasterBand(2).Checksum() == cs, 'Wrong checksum'
    assert ds.GetRasterBand(3).Checksum() == 0, 'Wrong checksum'
    assert ds.GetRasterBand(4).Checksum() == cs, 'Wrong checksum'
    ds = None

    os.unlink('tmp/test_gdal_merge_5.tif')

    test_py_scripts.run_py_script(script_path, 'gdal_merge', ' -o tmp/test_gdal_merge_5.tif tmp/in6.tif tmp/in5.tif')

    ds = gdal.Open('tmp/test_gdal_merge_5.tif')

    assert ds.GetRasterBand(1).Checksum() == 0, 'Wrong checksum'
    assert ds.GetRasterBand(2).Checksum() == cs, 'Wrong checksum'
    assert ds.GetRasterBand(3).Checksum() == 0, 'Wrong checksum'
    assert ds.GetRasterBand(4).Checksum() == cs, 'Wrong checksum'

###############################################################################
# Cleanup


def test_gdal_merge_cleanup():

    lst = ['tmp/test_gdal_merge_1.tif',
           'tmp/test_gdal_merge_2.tif',
           'tmp/test_gdal_merge_3.tif',
           'tmp/test_gdal_merge_4.tif',
           'tmp/test_gdal_merge_5.tif',
           'tmp/in1.tif',
           'tmp/in2.tif',
           'tmp/in3.tif',
           'tmp/in4.tif',
           'tmp/in5.tif',
           'tmp/in6.tif']
    for filename in lst:
        try:
            os.remove(filename)
        except OSError:
            pass

