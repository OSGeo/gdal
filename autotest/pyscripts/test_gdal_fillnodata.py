#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdal_fillnodata.py testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2010, Even Rouault <even dot rouault at spatialys.com>
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, fillnodata, publish, distribute, sublicense,
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

from osgeo import gdal
import test_py_scripts
import pytest

###############################################################################
# Dummy test : there is no nodata value in the source dataset !


def test_gdal_fillnodata_1():

    script_path = test_py_scripts.get_py_script('gdal_fillnodata')
    if script_path is None:
        pytest.skip()

    test_py_scripts.run_py_script(script_path, 'gdal_fillnodata', test_py_scripts.get_data_path('gcore') + 'byte.tif tmp/test_gdal_fillnodata_1.tif')

    try:
        ds = gdal.Open('tmp/test_gdal_fillnodata_1.tif')
        assert ds.GetRasterBand(1).Checksum() == 4672
        ds = None
    finally:
        gdal.GetDriverByName('GTiff').Delete('tmp/test_gdal_fillnodata_1.tif')

###############################################################################
# Make sure we copy the no data value to the dst when created
# No data value for nodata_byte.tif is 0.


def test_gdal_fillnodata_2():

    script_path = test_py_scripts.get_py_script('gdal_fillnodata')
    if script_path is None:
        pytest.skip()

    test_py_scripts.run_py_script(script_path, 'gdal_fillnodata', '-si 0 ' + test_py_scripts.get_data_path('gcore') + 'nodata_byte.tif tmp/test_gdal_fillnodata_2.tif')


    try:
        ds = gdal.Open('tmp/test_gdal_fillnodata_2.tif')
        assert ds.GetRasterBand(1).GetNoDataValue() == 0, \
            'Failed to copy No Data Value to dst dataset.'
        ds = None
    finally:
        gdal.GetDriverByName('GTiff').Delete('tmp/test_gdal_fillnodata_2.tif')


###############################################################################
# Test -si 1 and -md

def test_gdal_fillnodata_smoothing():

    script_path = test_py_scripts.get_py_script('gdal_fillnodata')
    if script_path is None:
        pytest.skip()

    ds = gdal.GetDriverByName('GTiff').Create('tmp/test_gdal_fillnodata_smoothing_in.tif', 4, 4)
    ds.GetRasterBand(1).SetNoDataValue(0)
    input_data = (
        20, 30, 40, 50,
        30, 0,  0,  60,
        40, 0,  0,  70,
        50, 60, 70, 80)
    ds.GetRasterBand(1).WriteRaster(0, 0, 4, 4, b''.join([struct.pack('B', x) for x in input_data]))
    ds = None

    test_py_scripts.run_py_script(script_path, 'gdal_fillnodata', '-md 1 -si 1 tmp/test_gdal_fillnodata_smoothing_in.tif tmp/test_gdal_fillnodata_smoothing.tif')

    expected_data = (
        20, 30, 40, 50,
        30, 40, 50, 60,
        40, 50, 60, 70,
        50, 60, 70, 80)
    try:
        ds = gdal.Open('tmp/test_gdal_fillnodata_smoothing.tif')
        assert struct.unpack('B' * 16, ds.GetRasterBand(1).ReadRaster()) == expected_data
        ds = None
    finally:
        gdal.GetDriverByName('GTiff').Delete('tmp/test_gdal_fillnodata_smoothing_in.tif')
        gdal.GetDriverByName('GTiff').Delete('tmp/test_gdal_fillnodata_smoothing.tif')

