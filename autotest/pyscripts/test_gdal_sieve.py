#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test gdal_sieve.py utility
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2008, Frank Warmerdam <warmerdam@pobox.com>
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



import test_py_scripts

from osgeo import gdal
import pytest

###############################################################################
# Test a fairly default case.


def test_gdal_sieve_1():

    script_path = test_py_scripts.get_py_script('gdal_sieve')
    if script_path is None:
        pytest.skip()

    drv = gdal.GetDriverByName('GTiff')
    dst_ds = drv.Create('tmp/sieve_1.tif', 5, 7, 1, gdal.GDT_Byte)
    dst_ds = None

    test_py_scripts.run_py_script(script_path, 'gdal_sieve', '-nomask -st 2 -4 '+test_py_scripts.get_data_path('alg')+'sieve_src.grd tmp/sieve_1.tif')

    dst_ds = gdal.Open('tmp/sieve_1.tif')
    dst_band = dst_ds.GetRasterBand(1)
    assert dst_band.GetNoDataValue() == 132 # nodata value of alg/sieve_src.grd

    cs_expected = 364
    cs = dst_band.Checksum()

    dst_band = None
    dst_ds = None

    if cs == cs_expected \
       or gdal.GetConfigOption('CPL_DEBUG', 'OFF') != 'ON':
        # Reload because of side effects of run_py_script()
        drv = gdal.GetDriverByName('GTiff')
        drv.Delete('tmp/sieve_1.tif')

    if cs != cs_expected:
        print('Got: ', cs)
        pytest.fail('got wrong checksum')

###############################################################################
# Test with source dataset without nodata


def test_gdal_sieve_src_without_nodata():

    script_path = test_py_scripts.get_py_script('gdal_sieve')
    if script_path is None:
        pytest.skip()

    test_py_scripts.run_py_script(script_path, 'gdal_sieve', '-st 0 ' + test_py_scripts.get_data_path('gcore')+'byte.tif tmp/test_gdal_sieve_src_without_nodata.tif')

    dst_ds = gdal.Open('tmp/test_gdal_sieve_src_without_nodata.tif')
    dst_band = dst_ds.GetRasterBand(1)
    assert dst_band.GetNoDataValue() is None
    assert dst_band.Checksum() == 4672

    dst_band = None
    dst_ds = None

    gdal.GetDriverByName('GTiff').Delete('tmp/test_gdal_sieve_src_without_nodata.tif')
