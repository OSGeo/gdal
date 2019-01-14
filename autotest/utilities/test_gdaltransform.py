#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdaltransform testing
# Author:   Even Rouault <even dot rouault @ mines-paris dot org>
#
###############################################################################
# Copyright (c) 2008-2009, Even Rouault <even dot rouault at mines-paris dot org>
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



import gdaltest
import test_cli_utilities
import pytest

###############################################################################
# Test -s_srs and -t_srs


def test_gdaltransform_1():
    if test_cli_utilities.get_gdaltransform_path() is None:
        pytest.skip()

    strin = '2 49 1\n' + '3 50 2\n'
    ret = gdaltest.runexternal(test_cli_utilities.get_gdaltransform_path() + ' -s_srs EPSG:4326 -t_srs EPSG:4326', strin)

    assert ret.find('2 49 1') != -1
    assert ret.find('3 50 2') != -1

###############################################################################
# Test -gcp


def test_gdaltransform_2():
    if test_cli_utilities.get_gdaltransform_path() is None:
        pytest.skip()

    strin = '0 0\n' + '20 0\n' + '20 20\n' + '0 20\n'
    ret = gdaltest.runexternal(test_cli_utilities.get_gdaltransform_path() + ' -gcp 0 0  440720.000 3751320.000 -gcp 20 0 441920.000 3751320.000 -gcp 20 20 441920.000 3750120.000 0 -gcp 0 20 440720.000 3750120.000', strin)

    assert ret.find('440720 3751320') != -1
    assert ret.find('441920 3751320') != -1
    assert ret.find('441920 3750120') != -1
    assert ret.find('440720 3750120') != -1

###############################################################################
# Test -gcp -tps


def test_gdaltransform_3():
    if test_cli_utilities.get_gdaltransform_path() is None:
        pytest.skip()

    strin = '0 0\n' + '20 0\n' + '20 20\n' + '0 20\n'
    ret = gdaltest.runexternal(test_cli_utilities.get_gdaltransform_path() + ' -tps -gcp 0 0  440720.000 3751320.000 -gcp 20 0 441920.000 3751320.000 -gcp 20 20 441920.000 3750120.000 0 -gcp 0 20 440720.000 3750120.000', strin)

    assert ret.find('440720 3751320') != -1
    assert ret.find('441920 3751320') != -1
    assert ret.find('441920 3750120') != -1
    assert ret.find('440720 3750120') != -1

###############################################################################
# Test -gcp -order 1


def test_gdaltransform_4():
    if test_cli_utilities.get_gdaltransform_path() is None:
        pytest.skip()

    strin = '0 0\n' + '20 0\n' + '20 20\n' + '0 20\n'
    ret = gdaltest.runexternal(test_cli_utilities.get_gdaltransform_path() + ' -order 1 -gcp 0 0  440720.000 3751320.000 -gcp 20 0 441920.000 3751320.000 -gcp 20 20 441920.000 3750120.000 0 -gcp 0 20 440720.000 3750120.000', strin)

    assert ret.find('440720 3751320') != -1
    assert ret.find('441920 3751320') != -1
    assert ret.find('441920 3750120') != -1
    assert ret.find('440720 3750120') != -1

###############################################################################
# Test with input file and -t_srs


def test_gdaltransform_5():
    if test_cli_utilities.get_gdaltransform_path() is None:
        pytest.skip()

    strin = '0 0\n'
    ret = gdaltest.runexternal(test_cli_utilities.get_gdaltransform_path() + ' -t_srs EPSG:26711 ../gcore/data/byte.tif', strin)

    text_split = ret.split(' ')
    x = float(text_split[0])
    y = float(text_split[1])

    assert abs(x - 440720) <= 1e-4 and abs(y - 3751320) <= 1e-4, ret

###############################################################################
# Test with input file and output file


def test_gdaltransform_6():
    if test_cli_utilities.get_gdaltransform_path() is None:
        pytest.skip()

    strin = '440720 3751320\n'
    ret = gdaltest.runexternal(test_cli_utilities.get_gdaltransform_path() + ' ../gcore/data/byte.tif ../gcore/data/byte.tif', strin)

    text_split = ret.split(' ')
    x = float(text_split[0])
    y = float(text_split[1])

    assert abs(x - 440720) <= 1e-4 and abs(y - 3751320) <= 1e-4, ret


###############################################################################
# Test with input file and -t_srs and -i

def test_gdaltransform_7():
    if test_cli_utilities.get_gdaltransform_path() is None:
        pytest.skip()

    strin = '440720 3751320\n'
    ret = gdaltest.runexternal(test_cli_utilities.get_gdaltransform_path() + ' -t_srs EPSG:26711 ../gcore/data/byte.tif -i', strin)

    text_split = ret.split(' ')
    x = float(text_split[0])
    y = float(text_split[1])

    assert abs(x - 0) <= 1e-4 and abs(y - 0) <= 1e-4, ret

###############################################################################
# Test -to


def test_gdaltransform_8():
    if test_cli_utilities.get_gdaltransform_path() is None:
        pytest.skip()

    strin = '2 49 1\n'
    ret = gdaltest.runexternal(test_cli_utilities.get_gdaltransform_path() + ' -to "SRC_SRS=WGS84" -to "DST_SRS=WGS84"', strin)

    assert ret.find('2 49 1') != -1

###############################################################################
# Test -output_xy


def test_gdaltransform_9():
    if test_cli_utilities.get_gdaltransform_path() is None:
        pytest.skip()

    strin = '0 0 0\n'
    ret = gdaltest.runexternal(test_cli_utilities.get_gdaltransform_path() + ' ../gcore/data/byte.tif -output_xy', strin)

    text_split = ret.split(' ')
    assert len(text_split) == 2, ret




