#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdal_retile.py testing
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

import shutil
import os

from osgeo import gdal
from osgeo import osr
import test_py_scripts
import pytest

###############################################################################
# Test gdal_retile.py


def test_gdal_retile_1():

    script_path = test_py_scripts.get_py_script('gdal_retile')
    if script_path is None:
        pytest.skip()

    try:
        os.mkdir('tmp/outretile')
    except OSError:
        pass

    test_py_scripts.run_py_script(script_path, 'gdal_retile', '-v -levels 2 -r bilinear -targetDir tmp/outretile ' + test_py_scripts.get_data_path('gcore') + 'byte.tif')

    ds = gdal.Open('tmp/outretile/byte_1_1.tif')
    assert ds.GetRasterBand(1).Checksum() == 4672
    ds = None

    ds = gdal.Open('tmp/outretile/1/byte_1_1.tif')
    assert ds.RasterXSize == 10
    # if ds.GetRasterBand(1).Checksum() != 1152:
    #    print(ds.GetRasterBand(1).Checksum())
    #    return 'fail'
    ds = None

    ds = gdal.Open('tmp/outretile/2/byte_1_1.tif')
    assert ds.RasterXSize == 5
    # if ds.GetRasterBand(1).Checksum() != 215:
    #    print(ds.GetRasterBand(1).Checksum())
    #    return 'fail'
    ds = None

###############################################################################
# Test gdal_retile.py with RGBA dataset


def test_gdal_retile_2():

    script_path = test_py_scripts.get_py_script('gdal_retile')
    if script_path is None:
        pytest.skip()

    try:
        os.mkdir('tmp/outretile2')
    except OSError:
        pass

    test_py_scripts.run_py_script(script_path, 'gdal_retile', '-v -levels 2 -r bilinear -targetDir tmp/outretile2 ' + test_py_scripts.get_data_path('gcore') + 'rgba.tif')

    ds = gdal.Open('tmp/outretile2/2/rgba_1_1.tif')
    assert ds.GetRasterBand(1).Checksum() == 35, 'wrong checksum for band 1'
    assert ds.GetRasterBand(4).Checksum() == 35, 'wrong checksum for band 4'
    ds = None

###############################################################################
# Test gdal_retile.py with input images of different pixel sizes


def test_gdal_retile_3():

    script_path = test_py_scripts.get_py_script('gdal_retile')
    if script_path is None:
        pytest.skip()

    drv = gdal.GetDriverByName('GTiff')
    srs = osr.SpatialReference()
    srs.SetWellKnownGeogCS('WGS84')
    wkt = srs.ExportToWkt()

    # Create two images to tile together. The images will cover the geographic
    # range 0E-30E and 0-60N, split horizontally at 30N. The pixel size in the
    # second image will be twice that of the first time. If the make the first
    # image black and the second gray, then the result of tiling these two
    # together should be gray square stacked on top of a black square.
    #
    # 60 N ---------------
    #      |             | \
    #      |    50x50    |  \ Image 2
    #      |             |  /
    #      |             | /
    # 30 N ---------------
    #      |             | \
    #      |  100x100    |  \ Image 1
    #      |             |  /
    #      |             | /
    #  0 N ---------------
    #      0 E           30 E

    ds = drv.Create('tmp/in1.tif', 100, 100, 1)
    px1_x = 30.0 / ds.RasterXSize
    px1_y = 30.0 / ds.RasterYSize
    ds.SetProjection(wkt)
    ds.SetGeoTransform([0, px1_x, 0, 30, 0, -px1_y])
    ds.GetRasterBand(1).Fill(0)
    ds = None

    ds = drv.Create('tmp/in2.tif', 50, 50, 1)
    px2_x = 30.0 / ds.RasterXSize
    px2_y = 30.0 / ds.RasterYSize
    ds.SetProjection(wkt)
    ds.SetGeoTransform([0, px2_x, 0, 60, 0, -px2_y])
    ds.GetRasterBand(1).Fill(42)
    ds = None

    try:
        os.mkdir('tmp/outretile3')
    except OSError:
        pass

    test_py_scripts.run_py_script(script_path, 'gdal_retile', '-v -levels 2 -r bilinear -targetDir tmp/outretile3 tmp/in1.tif tmp/in2.tif')

    ds = gdal.Open('tmp/outretile3/in1_1_1.tif')
    assert ds.GetProjectionRef().find('WGS 84') != -1, \
        ('Expected WGS 84\nGot : %s' % (ds.GetProjectionRef()))

    gt = ds.GetGeoTransform()
    expected_gt = [0, px1_x, 0, 60, 0, -px1_y]
    for i in range(6):
        assert not abs(gt[i] - expected_gt[i] > 1e-5), \
            ('Expected : %s\nGot : %s' % (expected_gt, gt))

    assert ds.RasterXSize == 100 and ds.RasterYSize == 200, \
        ('Wrong raster dimensions : %d x %d' % (ds.RasterXSize, ds.RasterYSize))

    assert ds.RasterCount == 1, ('Wrong raster count : %d ' % (ds.RasterCount))

    assert ds.GetRasterBand(1).Checksum() == 38999, 'Wrong checksum'


###############################################################################
# Test gdal_retile.py -overlap

def test_gdal_retile_4():

    script_path = test_py_scripts.get_py_script('gdal_retile')
    if script_path is None:
        pytest.skip()

    try:
        os.mkdir('tmp/outretile4')
    except OSError:
        pass

    test_py_scripts.run_py_script(script_path, 'gdal_retile', '-v -ps 8 7 -overlap 3 -targetDir tmp/outretile4 ' + test_py_scripts.get_data_path('gcore') + 'byte.tif')

    expected_results = [['tmp/outretile4/byte_1_1.tif', 8, 7],
                        ['tmp/outretile4/byte_1_2.tif', 8, 7],
                        ['tmp/outretile4/byte_1_3.tif', 8, 7],
                        ['tmp/outretile4/byte_1_4.tif', 5, 7],
                        ['tmp/outretile4/byte_2_1.tif', 8, 7],
                        ['tmp/outretile4/byte_2_2.tif', 8, 7],
                        ['tmp/outretile4/byte_2_3.tif', 8, 7],
                        ['tmp/outretile4/byte_2_4.tif', 5, 7],
                        ['tmp/outretile4/byte_3_1.tif', 8, 7],
                        ['tmp/outretile4/byte_3_2.tif', 8, 7],
                        ['tmp/outretile4/byte_3_3.tif', 8, 7],
                        ['tmp/outretile4/byte_3_4.tif', 5, 7],
                        ['tmp/outretile4/byte_4_1.tif', 8, 7],
                        ['tmp/outretile4/byte_4_2.tif', 8, 7],
                        ['tmp/outretile4/byte_4_3.tif', 8, 7],
                        ['tmp/outretile4/byte_4_4.tif', 5, 7],
                        ['tmp/outretile4/byte_5_1.tif', 8, 4],
                        ['tmp/outretile4/byte_5_2.tif', 8, 4],
                        ['tmp/outretile4/byte_5_3.tif', 8, 4],
                        ['tmp/outretile4/byte_5_4.tif', 5, 4]]

    for (filename, width, height) in expected_results:
        ds = gdal.Open(filename)
        assert ds.RasterXSize == width, filename
        assert ds.RasterYSize == height, filename
        ds = None

    test_py_scripts.run_py_script(script_path, 'gdal_retile', '-v -levels 1 -ps 8 8 -overlap 4 -targetDir tmp/outretile4 ' + test_py_scripts.get_data_path('gcore') + 'byte.tif')

    expected_results = [['tmp/outretile4/byte_1_1.tif', 8, 8],
                        ['tmp/outretile4/byte_1_2.tif', 8, 8],
                        ['tmp/outretile4/byte_1_3.tif', 8, 8],
                        ['tmp/outretile4/byte_1_4.tif', 8, 8],
                        ['tmp/outretile4/byte_2_1.tif', 8, 8],
                        ['tmp/outretile4/byte_2_2.tif', 8, 8],
                        ['tmp/outretile4/byte_2_3.tif', 8, 8],
                        ['tmp/outretile4/byte_2_4.tif', 8, 8],
                        ['tmp/outretile4/byte_3_1.tif', 8, 8],
                        ['tmp/outretile4/byte_3_2.tif', 8, 8],
                        ['tmp/outretile4/byte_3_3.tif', 8, 8],
                        ['tmp/outretile4/byte_3_4.tif', 8, 8],
                        ['tmp/outretile4/byte_4_1.tif', 8, 8],
                        ['tmp/outretile4/byte_4_2.tif', 8, 8],
                        ['tmp/outretile4/byte_4_3.tif', 8, 8],
                        ['tmp/outretile4/byte_4_4.tif', 8, 8],
                        ['tmp/outretile4/1/byte_1_1.tif', 8, 8],
                        ['tmp/outretile4/1/byte_1_2.tif', 6, 8],
                        ['tmp/outretile4/1/byte_2_1.tif', 8, 6],
                        ['tmp/outretile4/1/byte_2_2.tif', 6, 6]]

    for (filename, width, height) in expected_results:
        ds = gdal.Open(filename)
        assert ds.RasterXSize == width, filename
        assert ds.RasterYSize == height, filename
        ds = None


###############################################################################
# Test gdal_retile.py with input having a NoData value


def test_gdal_retile_5():

    np = pytest.importorskip('numpy')

    nodata_value = -3.4028234663852886e+38
    raster_array = np.array(([0.0, 2.0], [-1.0, nodata_value]))

    script_path = test_py_scripts.get_py_script('gdal_retile')
    if script_path is None:
        pytest.skip()

    drv = gdal.GetDriverByName('GTiff')
    srs = osr.SpatialReference()
    srs.SetWellKnownGeogCS('WGS84')
    wkt = srs.ExportToWkt()

    ds = drv.Create('tmp/in5.tif', 2, 2, 1, gdal.GDT_Float32)
    px1_x = 0.1 / ds.RasterXSize
    px1_y = 0.1 / ds.RasterYSize
    ds.SetProjection(wkt)
    ds.SetGeoTransform([0, px1_x, 0, 30, 0, -px1_y])
    raster_band = ds.GetRasterBand(1)
    raster_band.SetNoDataValue(nodata_value)
    raster_band.WriteArray(raster_array)
    raster_band = None
    ds = None

    try:
        os.mkdir('tmp/outretile5')
    except OSError:
        pass

    test_py_scripts.run_py_script(script_path, 'gdal_retile', '-v -targetDir tmp/outretile5 tmp/in5.tif')

    ds = gdal.Open('tmp/outretile5/in5_1_1.tif')
    raster_band = ds.GetRasterBand(1)

    assert raster_band.GetNoDataValue() == nodata_value, \
        ('Wrong nodata value.\nExpected %f, Got: %f' % (nodata_value, raster_band.GetNoDataValue()))

    min_val, max_val = raster_band.ComputeRasterMinMax()
    assert max_val, \
        ('Wrong maximum value.\nExpected 2.0, Got: %f' % max_val)

    assert min_val == -1.0, \
        ('Wrong minimum value.\nExpected -1.0, Got: %f' % min_val)

    ds = None


###############################################################################
# Cleanup


def test_gdal_retile_cleanup():

    lst = ['tmp/outretile/1/byte_1_1.tif',
           'tmp/outretile/2/byte_1_1.tif',
           'tmp/outretile/byte_1_1.tif',
           'tmp/outretile/1',
           'tmp/outretile/2',
           'tmp/outretile',
           'tmp/outretile2/1/rgba_1_1.tif',
           'tmp/outretile2/2/rgba_1_1.tif',
           'tmp/outretile2/1',
           'tmp/outretile2/2',
           'tmp/outretile2/rgba_1_1.tif',
           'tmp/outretile2',
           'tmp/in1.tif',
           'tmp/in2.tif',
           'tmp/outretile3/1/in1_1_1.tif',
           'tmp/outretile3/2/in1_1_1.tif',
           'tmp/outretile3/1',
           'tmp/outretile3/2',
           'tmp/outretile3/in1_1_1.tif',
           'tmp/outretile3',
           'tmp/in5.tif']
    for filename in lst:
        try:
            os.remove(filename)
        except OSError:
            try:
                os.rmdir(filename)
            except OSError:
                pass

    shutil.rmtree('tmp/outretile4')

    if os.path.exists('tmp/outretile5'):
        shutil.rmtree('tmp/outretile5')

