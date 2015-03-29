#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdallocationinfo testing
# Author:   Even Rouault <even dot rouault @ mines-paris dot org>
# 
###############################################################################
# Copyright (c) 2010-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

sys.path.append( '../pymod' )
sys.path.append( '../gcore' )

from osgeo import gdal
import gdaltest
import test_cli_utilities

###############################################################################
# Test basic usage

def test_gdallocationinfo_1():
    if test_cli_utilities.get_gdallocationinfo_path() is None:
        return 'skip'

    (ret, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdallocationinfo_path() + ' ../gcore/data/byte.tif 0 0')
    if not (err is None or err == '') :
        gdaltest.post_reason('got error/warning')
        print(err)
        return 'fail'

    ret = ret.replace('\r\n', '\n')
    expected_ret = """Report:
  Location: (0P,0L)
  Band 1:
    Value: 107"""
    if ret.find(expected_ret) != 0:
        print(ret)
        return 'fail'

    return 'success'

###############################################################################
# Test -xml

def test_gdallocationinfo_2():
    if test_cli_utilities.get_gdallocationinfo_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_gdallocationinfo_path() + ' -xml ../gcore/data/byte.tif 0 0')
    ret = ret.replace('\r\n', '\n')
    expected_ret = """<Report pixel="0" line="0">
  <BandReport band="1">
    <Value>107</Value>
  </BandReport>
</Report>"""
    if ret.find(expected_ret) != 0:
        print(ret)
        return 'fail'

    return 'success'

###############################################################################
# Test -valonly

def test_gdallocationinfo_3():
    if test_cli_utilities.get_gdallocationinfo_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_gdallocationinfo_path() + ' -b 1 -valonly ../gcore/data/byte.tif 0 0')
    expected_ret = """107"""
    if ret.find(expected_ret) != 0:
        print(ret)
        return 'fail'

    return 'success'

###############################################################################
# Test -geoloc

def test_gdallocationinfo_4():
    if test_cli_utilities.get_gdallocationinfo_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_gdallocationinfo_path() + ' -geoloc ../gcore/data/byte.tif 440720.000 3751320.000')
    ret = ret.replace('\r\n', '\n')
    expected_ret = """Report:
  Location: (0P,0L)
  Band 1:
    Value: 107"""
    if ret.find(expected_ret) != 0:
        print(ret)
        return 'fail'

    return 'success'

###############################################################################
# Test -lifonly

def test_gdallocationinfo_5():
    if test_cli_utilities.get_gdallocationinfo_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_gdallocationinfo_path() + ' -lifonly ../gcore/data/byte.vrt 0 0')
    expected_ret1 = """../gcore/data/byte.tif"""
    expected_ret2 = """../gcore/data\\byte.tif"""
    if ret.find(expected_ret1) < 0 and ret.find(expected_ret2) < 0:
        print(ret)
        return 'fail'

    return 'success'

###############################################################################
# Test -overview

def test_gdallocationinfo_6():
    if test_cli_utilities.get_gdallocationinfo_path() is None:
        return 'skip'

    src_ds = gdal.Open('../gcore/data/byte.tif')
    ds = gdal.GetDriverByName('GTiff').CreateCopy('tmp/test_gdallocationinfo_6.tif', src_ds)
    ds.BuildOverviews('AVERAGE', overviewlist = [2])
    ds = None
    src_ds = None

    ret = gdaltest.runexternal(test_cli_utilities.get_gdallocationinfo_path() + ' tmp/test_gdallocationinfo_6.tif 10 10 -overview 1')

    gdal.GetDriverByName('GTiff').Delete('tmp/test_gdallocationinfo_6.tif')
    expected_ret = """Value: 130"""
    if ret.find(expected_ret) < 0:
        print(ret)
        return 'fail'

    return 'success'

gdaltest_list = [
    test_gdallocationinfo_1,
    test_gdallocationinfo_2,
    test_gdallocationinfo_3,
    test_gdallocationinfo_4,
    test_gdallocationinfo_5,
    test_gdallocationinfo_6,
    ]


if __name__ == '__main__':

    gdaltest.setup_run( 'test_gdallocationinfo' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()





