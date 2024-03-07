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

import sys

import gdaltest
import pytest
import test_cli_utilities

from osgeo import osr

pytestmark = pytest.mark.skipif(
    test_cli_utilities.get_gdaltransform_path() is None,
    reason="gdaltransform not available",
)


@pytest.fixture()
def gdaltransform_path():
    return test_cli_utilities.get_gdaltransform_path()


###############################################################################
# Test -s_srs and -t_srs


def test_gdaltransform_1(gdaltransform_path):

    strin = "2 49 1\n" + "3 50 2\n"
    ret = gdaltest.runexternal(
        gdaltransform_path + " -s_srs EPSG:4326 -t_srs EPSG:4326",
        strin,
    )

    assert ret.find("2 49 1") != -1
    assert ret.find("3 50 2") != -1


###############################################################################
# Test -gcp


def test_gdaltransform_2(gdaltransform_path):

    strin = "0 0\n" + "20 0\n" + "20 20\n" + "0 20\n"
    ret = gdaltest.runexternal(
        gdaltransform_path
        + " -gcp 0 0  440720.000 3751320.000 -gcp 20 0 441920.000 3751320.000 -gcp 20 20 441920.000 3750120.000 0 -gcp 0 20 440720.000 3750120.000",
        strin,
    )

    assert ret.find("440720 3751320") != -1
    assert ret.find("441920 3751320") != -1
    assert ret.find("441920 3750120") != -1
    assert ret.find("440720 3750120") != -1


###############################################################################
# Test -gcp -tps


def test_gdaltransform_3(gdaltransform_path):

    strin = "0 0\n" + "20 0\n" + "20 20\n" + "0 20\n"
    ret = gdaltest.runexternal(
        gdaltransform_path
        + " -tps -gcp 0 0  440720.000 3751320.000 -gcp 20 0 441920.000 3751320.000 -gcp 20 20 441920.000 3750120.000 0 -gcp 0 20 440720.000 3750120.000",
        strin,
    )

    assert ret.find("440720 3751320") != -1
    assert ret.find("441920 3751320") != -1
    assert ret.find("441920 3750120") != -1
    assert ret.find("440720 3750120") != -1


###############################################################################
# Test -gcp -order 1


def test_gdaltransform_4(gdaltransform_path):

    strin = "0 0\n" + "20 0\n" + "20 20\n" + "0 20\n"
    ret = gdaltest.runexternal(
        gdaltransform_path
        + " -order 1 -gcp 0 0  440720.000 3751320.000 -gcp 20 0 441920.000 3751320.000 -gcp 20 20 441920.000 3750120.000 0 -gcp 0 20 440720.000 3750120.000",
        strin,
    )

    assert ret.find("440720 3751320") != -1
    assert ret.find("441920 3751320") != -1
    assert ret.find("441920 3750120") != -1
    assert ret.find("440720 3750120") != -1


###############################################################################
# Test with input file and -t_srs


def test_gdaltransform_5(gdaltransform_path):

    strin = "0 0\n"
    ret = gdaltest.runexternal(
        gdaltransform_path + " -t_srs EPSG:26711 ../gcore/data/byte.tif",
        strin,
    )

    text_split = ret.split(" ")
    x = float(text_split[0])
    y = float(text_split[1])

    assert x == pytest.approx(440720, abs=1e-4) and y == pytest.approx(
        3751320, abs=1e-4
    ), ret


###############################################################################
# Test with input file and output file


def test_gdaltransform_6(gdaltransform_path):

    strin = "440720 3751320\n"
    ret = gdaltest.runexternal(
        gdaltransform_path + " ../gcore/data/byte.tif ../gcore/data/byte.tif",
        strin,
    )

    text_split = ret.split(" ")
    x = float(text_split[0])
    y = float(text_split[1])

    assert x == pytest.approx(440720, abs=1e-4) and y == pytest.approx(
        3751320, abs=1e-4
    ), ret


###############################################################################
# Test with input file and -t_srs and -i


def test_gdaltransform_7(gdaltransform_path):

    strin = "440720 3751320\n"
    ret = gdaltest.runexternal(
        gdaltransform_path + " -t_srs EPSG:26711 ../gcore/data/byte.tif -i",
        strin,
    )

    text_split = ret.split(" ")
    x = float(text_split[0])
    y = float(text_split[1])

    assert x == pytest.approx(0, abs=1e-4) and y == pytest.approx(0, abs=1e-4), ret


###############################################################################
# Test -to


def test_gdaltransform_8(gdaltransform_path):

    strin = "2 49 1\n"
    ret = gdaltest.runexternal(
        gdaltransform_path + ' -to "SRC_SRS=WGS84" -to "DST_SRS=WGS84"',
        strin,
    )

    assert ret.find("2 49 1") != -1


###############################################################################
# Test -output_xy


def test_gdaltransform_9(gdaltransform_path):

    strin = "0 0 0\n"
    ret = gdaltest.runexternal(
        gdaltransform_path + " ../gcore/data/byte.tif -output_xy",
        strin,
    )

    text_split = ret.split(" ")
    assert len(text_split) == 2, ret


###############################################################################
# Test -ct and 4D


def test_gdaltransform_ct_4D(gdaltransform_path):

    ret = gdaltest.runexternal(
        gdaltransform_path
        + ' -ct "+proj=pipeline +step +proj=unitconvert +xy_in=deg +xy_out=rad +step +proj=cart +step +proj=helmert +convention=position_vector +x=0.0127 +dx=-0.0029 +rx=-0.00039 +drx=-0.00011 +y=0.0065 +dy=-0.0002 +ry=0.00080 +dry=-0.00019 +z=-0.0209 +dz=-0.0006 +rz=-0.00114 +drz=0.00007 +s=0.00195 +ds=0.00001 +t_epoch=1988.0 +step +proj=cart +inv +step +proj=unitconvert +xy_in=rad +xy_out=deg" -coord 2 49 0 2000'
    )

    values = [float(x) for x in ret.split(" ")]
    assert len(values) == 3, ret
    assert values[0] == pytest.approx(2.0000005420366, abs=1e-10), ret
    assert values[1] == pytest.approx(49.0000003766711, abs=1e-10), ret
    assert values[2] == pytest.approx(-0.0222802283242345, abs=1e-8), ret


###############################################################################
# Test s_coord_epoch


@pytest.mark.require_proj(7, 2)
def test_gdaltransform_s_coord_epoch(gdaltransform_path):

    ret = gdaltest.runexternal(
        gdaltransform_path
        + " -s_srs EPSG:9000 -s_coord_epoch 2030 -t_srs EPSG:7844 -coord 150 -30"
    )

    values = [float(x) for x in ret.split(" ")]
    assert len(values) == 3, ret
    assert abs(values[0] - 150) > 1e-8, ret
    assert abs(values[1] - -30) > 1e-8, ret


###############################################################################
# Test t_coord_epoch


@pytest.mark.require_proj(7, 2)
def test_gdaltransform_t_coord_epoch(gdaltransform_path):

    ret = gdaltest.runexternal(
        gdaltransform_path
        + " -s_srs EPSG:7844 -t_srs EPSG:9000 -t_coord_epoch 2030 -coord 150 -30"
    )

    values = [float(x) for x in ret.split(" ")]
    assert len(values) == 3, ret
    assert abs(values[0] - 150) > 1e-8, ret
    assert abs(values[1] - -30) > 1e-8, ret


###############################################################################
# Test s_coord_epoch and t_coord_epoch


@pytest.mark.require_proj(9, 4)
def test_gdaltransform_s_t_coord_epoch(gdaltransform_path):

    sep = ";" if sys.platform == "win32" else ":"
    PROJ_DATA = sep.join(osr.GetPROJSearchPaths())

    ret = gdaltest.runexternal(
        gdaltransform_path
        + f' -s_srs EPSG:8254 -s_coord_epoch 2002 -t_srs EPSG:8254 -t_coord_epoch 2010 -coord -79.5 60.5 --config PROJ_DATA "{PROJ_DATA}"'
    )

    values = [float(x) for x in ret.split(" ")]
    assert len(values) == 3, ret
    assert abs(values[0] - -79.499999630188) < 1e-8, ret
    assert abs(values[1] - 60.4999999378478) < 1e-8, ret


###############################################################################
# Test extra input


def test_gdaltransform_extra_input(gdaltransform_path):

    strin = (
        "2 49 1 my first point\n" + "3 50 second point\n" + "4 51 10 2 third point\n"
    )
    ret = gdaltest.runexternal(
        gdaltransform_path + " -field_sep ,",
        strin,
    )

    assert "2,49,1,my first point" in ret
    assert "3,50,0,second point" in ret
    assert "4,51,10,third point" in ret


###############################################################################
# Test ignoring extra input


def test_gdaltransform_extra_input_ignored(gdaltransform_path):

    strin = "2 49 1 my first point\n"
    ret = gdaltest.runexternal(
        gdaltransform_path + " -ignore_extra_input",
        strin,
    )

    assert "my first point" not in ret


###############################################################################
# Test echo mode


def test_gdaltransform_echo(gdaltransform_path):

    strin = "0 0 1 my first point\n"

    ret = gdaltest.runexternal(
        gdaltransform_path + " -s_srs EPSG:4326 -t_srs EPSG:4978 -E -field_sep ,",
        strin,
    )

    assert "0,0,1,6378138,0,0,my first point" in ret

    ret = gdaltest.runexternal(
        gdaltransform_path + " -s_srs EPSG:4326 -t_srs EPSG:4978 -E -output_xy",
        strin,
    )

    assert "0 0 6378138 0 my first point" in ret
