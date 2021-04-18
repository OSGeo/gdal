#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdallocationinfo.py testing
# Author:   Idan Miara <idan@miara.com>
#
###############################################################################
# Copyright (c) 2010-2013, Even Rouault <even dot rouault at spatialys.com>
# Copyright (c) 2021, Idan Miara <idan@miara.com>
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


from osgeo import gdal
import test_py_scripts
import pytest

# test that numpy is available, if not skip all tests
np = pytest.importorskip('numpy')
gdallocationinfo = pytest.importorskip('osgeo_utils.samples.gdallocationinfo')


def test_gdallocationinfo_py_1():
    """ Test basic usage """
    script_path = test_py_scripts.get_py_script('gdallocationinfo')
    if script_path is None:
        pytest.skip()
    ret = test_py_scripts.run_py_script(script_path, 'gdallocationinfo', ' ../gcore/data/byte.tif 0 0')
    ret = ret.replace('\r\n', '\n')
    expected_ret = """Report:
  Location: (0P,0L)
  Band 1:
    Value: 107"""
    assert ret.startswith(expected_ret)


def test_gdallocationinfo_py_3():
    """ Test -valonly """
    script_path = test_py_scripts.get_py_script('gdallocationinfo')
    if script_path is None:
        pytest.skip()
    ret = test_py_scripts.run_py_script(script_path, 'gdallocationinfo', ' -b 1 -valonly ../gcore/data/byte.tif 0 0')
    expected_ret = """107"""
    assert ret.startswith(expected_ret)


def test_gdallocationinfo_py_4():
    """  Test -geoloc """
    script_path = test_py_scripts.get_py_script('gdallocationinfo')
    if script_path is None:
        pytest.skip()
    ret = test_py_scripts.run_py_script(script_path, 'gdallocationinfo', ' -geoloc ../gcore/data/byte.tif 440720.000 3751320.000')
    ret = ret.replace('\r\n', '\n')
    expected_ret = """Report:
  Location: (0P,0L)
  Band 1:
    Value: 107"""
    assert ret.startswith(expected_ret)


def test_gdallocationinfo_py_6():
    """ Test -overview """
    script_path = test_py_scripts.get_py_script('gdallocationinfo')
    if script_path is None:
        pytest.skip()
    src_ds = gdal.Open('../gcore/data/byte.tif')
    ds = gdal.GetDriverByName('GTiff').CreateCopy('tmp/test_gdallocationinfo_py_6.tif', src_ds)
    ds.BuildOverviews('AVERAGE', overviewlist=[2])
    ds = None
    src_ds = None

    ret = test_py_scripts.run_py_script(script_path, 'gdallocationinfo', ' tmp/test_gdallocationinfo_py_6.tif 10 10 -overview 1')

    gdal.GetDriverByName('GTiff').Delete('tmp/test_gdallocationinfo_py_6.tif')
    expected_ret = """Value: 130"""
    assert expected_ret in ret


def test_gdallocationinfo_py_wgs84():
    script_path = test_py_scripts.get_py_script('gdallocationinfo')
    if script_path is None:
        pytest.skip()
    ret = test_py_scripts.run_py_script(script_path, 'gdallocationinfo', ' -valonly -wgs84 ../gcore/data/byte.tif -117.6354747 33.8970515')

    expected_ret = """115"""
    assert expected_ret in ret
