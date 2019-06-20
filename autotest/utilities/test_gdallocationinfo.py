#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdallocationinfo testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2010-2013, Even Rouault <even dot rouault at spatialys.com>
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
import pytest

sys.path.append('../gcore')

from osgeo import gdal
import gdaltest
import test_cli_utilities

###############################################################################
# Test basic usage


def test_gdallocationinfo_1():
    if test_cli_utilities.get_gdallocationinfo_path() is None:
        pytest.skip()

    (ret, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdallocationinfo_path() + ' ../gcore/data/byte.tif 0 0')
    assert (err is None or err == ''), 'got error/warning'

    ret = ret.replace('\r\n', '\n')
    expected_ret = """Report:
  Location: (0P,0L)
  Band 1:
    Value: 107"""
    assert ret.startswith(expected_ret)

###############################################################################
# Test -xml


def test_gdallocationinfo_2():
    if test_cli_utilities.get_gdallocationinfo_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_gdallocationinfo_path() + ' -xml ../gcore/data/byte.tif 0 0')
    ret = ret.replace('\r\n', '\n')
    expected_ret = """<Report pixel="0" line="0">
  <BandReport band="1">
    <Value>107</Value>
  </BandReport>
</Report>"""
    assert ret.startswith(expected_ret)

###############################################################################
# Test -valonly


def test_gdallocationinfo_3():
    if test_cli_utilities.get_gdallocationinfo_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_gdallocationinfo_path() + ' -b 1 -valonly ../gcore/data/byte.tif 0 0')
    expected_ret = """107"""
    assert ret.startswith(expected_ret)

###############################################################################
# Test -geoloc


def test_gdallocationinfo_4():
    if test_cli_utilities.get_gdallocationinfo_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_gdallocationinfo_path() + ' -geoloc ../gcore/data/byte.tif 440720.000 3751320.000')
    ret = ret.replace('\r\n', '\n')
    expected_ret = """Report:
  Location: (0P,0L)
  Band 1:
    Value: 107"""
    assert ret.startswith(expected_ret)

###############################################################################
# Test -lifonly


def test_gdallocationinfo_5():
    if test_cli_utilities.get_gdallocationinfo_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_gdallocationinfo_path() + ' -lifonly ../gcore/data/byte.vrt 0 0')
    expected_ret1 = """../gcore/data/byte.tif"""
    expected_ret2 = """../gcore/data\\byte.tif"""
    assert expected_ret1 in ret or expected_ret2 in ret

###############################################################################
# Test -overview


def test_gdallocationinfo_6():
    if test_cli_utilities.get_gdallocationinfo_path() is None:
        pytest.skip()

    src_ds = gdal.Open('../gcore/data/byte.tif')
    ds = gdal.GetDriverByName('GTiff').CreateCopy('tmp/test_gdallocationinfo_6.tif', src_ds)
    ds.BuildOverviews('AVERAGE', overviewlist=[2])
    ds = None
    src_ds = None

    ret = gdaltest.runexternal(test_cli_utilities.get_gdallocationinfo_path() + ' tmp/test_gdallocationinfo_6.tif 10 10 -overview 1')

    gdal.GetDriverByName('GTiff').Delete('tmp/test_gdallocationinfo_6.tif')
    expected_ret = """Value: 130"""
    assert expected_ret in ret



def test_gdallocationinfo_wgs84():
    if test_cli_utilities.get_gdallocationinfo_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_gdallocationinfo_path() + ' -valonly -wgs84 ../gcore/data/byte.tif -117.6354747 33.8970515')

    expected_ret = """115"""
    assert expected_ret in ret



