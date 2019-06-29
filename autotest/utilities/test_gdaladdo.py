#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdaladdo testing
# Author:   Even Rouault <even dot rouault @ mines-paris dot org>
#
###############################################################################
# Copyright (c) 2008-2013, Even Rouault <even dot rouault at mines-paris dot org>
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
import shutil
import pytest

from osgeo import gdal
import gdaltest
import test_cli_utilities
from gcore import tiff_ovr

###############################################################################
# Similar to tiff_ovr_1


def test_gdaladdo_1():
    if test_cli_utilities.get_gdaladdo_path() is None:
        pytest.skip()

    shutil.copy('../gcore/data/mfloat32.vrt', 'tmp/mfloat32.vrt')
    shutil.copy('../gcore/data/float32.tif', 'tmp/float32.tif')

    (_, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdaladdo_path() + ' tmp/mfloat32.vrt 2 4')
    assert (err is None or err == ''), 'got error/warning'

    ds = gdal.Open('tmp/mfloat32.vrt')
    ret = tiff_ovr.tiff_ovr_check(ds)
    ds = None

    os.remove('tmp/mfloat32.vrt')
    os.remove('tmp/mfloat32.vrt.ovr')
    os.remove('tmp/float32.tif')

    return ret


###############################################################################
# Test -r average. Similar to tiff_ovr_5

def test_gdaladdo_2():
    if test_cli_utilities.get_gdaladdo_path() is None:
        pytest.skip()

    shutil.copyfile('../gcore/data/nodata_byte.tif', 'tmp/ovr5.tif')

    gdaltest.runexternal(test_cli_utilities.get_gdaladdo_path() + ' -r average tmp/ovr5.tif 2')

    ds = gdal.Open('tmp/ovr5.tif')
    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    exp_cs = 1130

    assert cs == exp_cs, 'got wrong overview checksum.'

    ds = None

    os.remove('tmp/ovr5.tif')

###############################################################################
# Test -ro


def test_gdaladdo_3():
    if test_cli_utilities.get_gdaladdo_path() is None:
        pytest.skip()

    gdal.Translate('tmp/test_gdaladdo_3.tif', '../gcore/data/nodata_byte.tif', options='-outsize 1024 1024')

    gdaltest.runexternal(test_cli_utilities.get_gdaladdo_path() + ' -ro tmp/test_gdaladdo_3.tif 2')

    ds = gdal.Open('tmp/test_gdaladdo_3.tif')
    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    exp_cs = 20683

    assert cs == exp_cs, 'got wrong overview checksum.'

    ds = None

    try:
        os.stat('tmp/test_gdaladdo_3.tif.ovr')
    except OSError:
        pytest.fail('no external overview.')

    
###############################################################################
# Test -clean


def test_gdaladdo_4():
    if test_cli_utilities.get_gdaladdo_path() is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_gdaladdo_path() + ' -clean tmp/test_gdaladdo_3.tif')

    ds = gdal.Open('tmp/test_gdaladdo_3.tif')
    cnt = ds.GetRasterBand(1).GetOverviewCount()
    ds = None

    assert cnt == 0, 'did not clean overviews.'

    assert not os.path.exists('tmp/test_gdaladdo_3.tif.ovr')

    os.remove('tmp/test_gdaladdo_3.tif')

###############################################################################
# Test implicit levels


def test_gdaladdo_5():
    if test_cli_utilities.get_gdaladdo_path() is None:
        pytest.skip()

    shutil.copyfile('../gcore/data/nodata_byte.tif', 'tmp/test_gdaladdo_5.tif')

    # Will not do anything given than the file is smaller than 256x256 already
    gdaltest.runexternal(test_cli_utilities.get_gdaladdo_path() + ' tmp/test_gdaladdo_5.tif')

    ds = gdal.Open('tmp/test_gdaladdo_5.tif')
    cnt = ds.GetRasterBand(1).GetOverviewCount()
    ds = None

    assert cnt == 0

    # Will generate overviews of size 10 5 3 2 1
    gdaltest.runexternal(test_cli_utilities.get_gdaladdo_path() + ' -minsize 1 tmp/test_gdaladdo_5.tif')

    ds = gdal.Open('tmp/test_gdaladdo_5.tif')
    cnt = ds.GetRasterBand(1).GetOverviewCount()
    ds = None

    assert cnt == 5

    gdal.Translate('tmp/test_gdaladdo_5.tif', '../gcore/data/nodata_byte.tif', options='-outsize 257 257')

    # Will generate overviews of size 129x129
    gdaltest.runexternal(test_cli_utilities.get_gdaladdo_path() + ' tmp/test_gdaladdo_5.tif')

    ds = gdal.Open('tmp/test_gdaladdo_5.tif')
    cnt = ds.GetRasterBand(1).GetOverviewCount()
    ds = None

    assert cnt == 1

    os.remove('tmp/test_gdaladdo_5.tif')




