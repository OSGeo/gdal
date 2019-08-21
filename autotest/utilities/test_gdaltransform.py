#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdaltransform testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2008-2009, Even Rouault <even dot rouault at spatialys.com>
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

    assert x == pytest.approx(440720, abs=1e-4) and y == pytest.approx(3751320, abs=1e-4), ret

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

    assert x == pytest.approx(440720, abs=1e-4) and y == pytest.approx(3751320, abs=1e-4), ret


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

    assert x == pytest.approx(0, abs=1e-4) and y == pytest.approx(0, abs=1e-4), ret

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


###############################################################################
# Test -ct and 4D


def test_gdaltransform_ct_4D():
    if test_cli_utilities.get_gdaltransform_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_gdaltransform_path() + ' -ct "+proj=pipeline +step +proj=unitconvert +xy_in=deg +xy_out=rad +step +proj=cart +step +proj=helmert +convention=position_vector +x=0.0127 +dx=-0.0029 +rx=-0.00039 +drx=-0.00011 +y=0.0065 +dy=-0.0002 +ry=0.00080 +dry=-0.00019 +z=-0.0209 +dz=-0.0006 +rz=-0.00114 +drz=0.00007 +s=0.00195 +ds=0.00001 +t_epoch=1988.0 +step +proj=cart +inv +step +proj=unitconvert +xy_in=rad +xy_out=deg" -coord 2 49 0 2000')

    values = [float(x) for x in ret.split(' ')]
    assert len(values) == 3, ret
    assert values[0] == pytest.approx(2.0000005420366, abs=1e-10), ret
    assert values[1] == pytest.approx(49.0000003766711, abs=1e-10), ret
    assert values[2] == pytest.approx(-0.0222802283242345, abs=1e-8), ret

