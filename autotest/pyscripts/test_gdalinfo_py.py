#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdalinfo.py testing
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


import test_py_scripts
import pytest

###############################################################################
# Simple test


def test_gdalinfo_py_1():

    script_path = test_py_scripts.get_py_script('gdalinfo')
    if script_path is None:
        pytest.skip()

    ret = test_py_scripts.run_py_script(script_path, 'gdalinfo', test_py_scripts.get_data_path('gcore') + 'byte.tif')
    assert ret.find('Driver: GTiff/GeoTIFF') != -1

###############################################################################
# Test -checksum option


def test_gdalinfo_py_2():
    script_path = test_py_scripts.get_py_script('gdalinfo')
    if script_path is None:
        pytest.skip()

    ret = test_py_scripts.run_py_script(script_path, 'gdalinfo', '-checksum ' + test_py_scripts.get_data_path('gcore') + 'byte.tif')
    assert ret.find('Checksum=4672') != -1

###############################################################################
# Test -nomd option


def test_gdalinfo_py_3():
    script_path = test_py_scripts.get_py_script('gdalinfo')
    if script_path is None:
        pytest.skip()

    ret = test_py_scripts.run_py_script(script_path, 'gdalinfo', test_py_scripts.get_data_path('gcore') + 'byte.tif')
    assert ret.find('Metadata') != -1

    ret = test_py_scripts.run_py_script(script_path, 'gdalinfo', '-nomd ' + test_py_scripts.get_data_path('gcore') + 'byte.tif')
    assert ret.find('Metadata') == -1

###############################################################################
# Test -noct option


def test_gdalinfo_py_4():
    script_path = test_py_scripts.get_py_script('gdalinfo')
    if script_path is None:
        pytest.skip()

    ret = test_py_scripts.run_py_script(script_path, 'gdalinfo', test_py_scripts.get_data_path('gdrivers')+'gif/bug407.gif')
    assert ret.find('0: 255,255,255,255') != -1

    ret = test_py_scripts.run_py_script(script_path, 'gdalinfo', '-noct '+test_py_scripts.get_data_path('gdrivers')+'gif/bug407.gif')
    assert ret.find('0: 255,255,255,255') == -1

###############################################################################
# Test -stats option


def test_gdalinfo_py_5():
    script_path = test_py_scripts.get_py_script('gdalinfo')
    if script_path is None:
        pytest.skip()

    try:
        os.remove(test_py_scripts.get_data_path('gcore') + 'byte.tif.aux.xml')
    except OSError:
        pass

    ret = test_py_scripts.run_py_script(script_path, 'gdalinfo', test_py_scripts.get_data_path('gcore') + 'byte.tif')
    assert ret.find('STATISTICS_MINIMUM=74') == -1, 'got wrong minimum.'

    ret = test_py_scripts.run_py_script(script_path, 'gdalinfo', '-stats ' + test_py_scripts.get_data_path('gcore') + 'byte.tif')
    assert ret.find('STATISTICS_MINIMUM=74') != -1, 'got wrong minimum (2).'

    # We will blow an exception if the file does not exist now!
    os.remove(test_py_scripts.get_data_path('gcore') + 'byte.tif.aux.xml')

###############################################################################
# Test a dataset with overviews and RAT


def test_gdalinfo_py_6():
    script_path = test_py_scripts.get_py_script('gdalinfo')
    if script_path is None:
        pytest.skip()

    ret = test_py_scripts.run_py_script(script_path, 'gdalinfo', test_py_scripts.get_data_path('gdrivers')+'hfa/int.img')
    assert ret.find('Overviews') != -1

###############################################################################
# Test a dataset with GCPs


def test_gdalinfo_py_7():
    script_path = test_py_scripts.get_py_script('gdalinfo')
    if script_path is None:
        pytest.skip()

    ret = test_py_scripts.run_py_script(script_path, 'gdalinfo', test_py_scripts.get_data_path('gcore') + 'gcps.vrt')
    assert ret.find('GCP Projection =') != -1
    assert ret.find('PROJCS["NAD27 / UTM zone 11N"') != -1
    assert ret.find('(100,100) -> (446720,3745320,0)') != -1

    # Same but with -nogcps
    ret = test_py_scripts.run_py_script(script_path, 'gdalinfo', '-nogcp ' + test_py_scripts.get_data_path('gcore') + 'gcps.vrt')
    assert ret.find('GCP Projection =') == -1
    assert ret.find('PROJCS["NAD27 / UTM zone 11N"') == -1
    assert ret.find('(100,100) -> (446720,3745320,0)') == -1

###############################################################################
# Test -hist option


def test_gdalinfo_py_8():
    script_path = test_py_scripts.get_py_script('gdalinfo')
    if script_path is None:
        pytest.skip()

    try:
        os.remove(test_py_scripts.get_data_path('gcore') + 'byte.tif.aux.xml')
    except OSError:
        pass

    ret = test_py_scripts.run_py_script(script_path, 'gdalinfo', test_py_scripts.get_data_path('gcore') + 'byte.tif')
    assert ret.find('0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 6 0 0 0 0 0 0 0 0 37 0 0 0 0 0 0 0 57 0 0 0 0 0 0 0 62 0 0 0 0 0 0 0 66 0 0 0 0 0 0 0 0 72 0 0 0 0 0 0 0 31 0 0 0 0 0 0 0 24 0 0 0 0 0 0 0 12 0 0 0 0 0 0 0 0 7 0 0 0 0 0 0 0 12 0 0 0 0 0 0 0 5 0 0 0 0 0 0 0 3 0 0 0 0 0 0 0 1 0 0 0 0 0 0 0 0 2 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 0 0 0 0 0 0 1') == -1, \
        'did not expect histogram.'

    ret = test_py_scripts.run_py_script(script_path, 'gdalinfo', '-hist ' + test_py_scripts.get_data_path('gcore') + 'byte.tif')
    assert ret.find('0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 6 0 0 0 0 0 0 0 0 37 0 0 0 0 0 0 0 57 0 0 0 0 0 0 0 62 0 0 0 0 0 0 0 66 0 0 0 0 0 0 0 0 72 0 0 0 0 0 0 0 31 0 0 0 0 0 0 0 24 0 0 0 0 0 0 0 12 0 0 0 0 0 0 0 0 7 0 0 0 0 0 0 0 12 0 0 0 0 0 0 0 5 0 0 0 0 0 0 0 3 0 0 0 0 0 0 0 1 0 0 0 0 0 0 0 0 2 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 0 0 0 0 0 0 1') != -1, \
        'did not get expected histogram.'

    # We will blow an exception if the file does not exist now!
    os.remove(test_py_scripts.get_data_path('gcore') + 'byte.tif.aux.xml')

###############################################################################
# Test -mdd option


def test_gdalinfo_py_9():
    script_path = test_py_scripts.get_py_script('gdalinfo')
    if script_path is None:
        pytest.skip()

    ret = test_py_scripts.run_py_script(script_path, 'gdalinfo', test_py_scripts.get_data_path('gdrivers')+'nitf/fake_nsif.ntf')
    assert ret.find('BLOCKA=010000001000000000') == -1, 'Got unexpected extra MD.'

    ret = test_py_scripts.run_py_script(script_path, 'gdalinfo', '-mdd TRE '+test_py_scripts.get_data_path('gdrivers')+'nitf/fake_nsif.ntf')
    assert ret.find('BLOCKA=010000001000000000') != -1, 'did not get extra MD.'

###############################################################################
# Test -mm option


def test_gdalinfo_py_10():
    script_path = test_py_scripts.get_py_script('gdalinfo')
    if script_path is None:
        pytest.skip()

    ret = test_py_scripts.run_py_script(script_path, 'gdalinfo', test_py_scripts.get_data_path('gcore') + 'byte.tif')
    assert ret.find('Computed Min/Max=74.000,255.000') == -1

    ret = test_py_scripts.run_py_script(script_path, 'gdalinfo', '-mm ' + test_py_scripts.get_data_path('gcore') + 'byte.tif')
    assert ret.find('Computed Min/Max=74.000,255.000') != -1




