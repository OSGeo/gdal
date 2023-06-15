#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdal_ls.py testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2011-2012, Even Rouault <even dot rouault at spatialys.com>
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

import io
import sys

import gdaltest
import pytest
import test_py_scripts

from osgeo import gdal

pytestmark = pytest.mark.skipif(
    test_py_scripts.get_py_script("gdal_ls") is None,
    reason="gdal_ls not available",
)


@pytest.fixture()
def script_path():
    return test_py_scripts.get_py_script("gdal_ls")


###############################################################################


def run_gdal_ls(script_path, argv):

    saved_syspath = sys.path
    sys.path.append(script_path)
    try:
        import gdal_ls
    except ImportError:
        sys.path = saved_syspath
        pytest.fail()

    sys.path = saved_syspath

    with io.StringIO() as outstr:
        ret = gdal_ls.gdal_ls(argv, outstr)
        retstr = outstr.getvalue()

    assert ret == 0, "got error code : %d" % ret

    return retstr


###############################################################################
# List one file


def test_gdal_ls_py_1(script_path):
    # TODO: Why the '' as the first element of the list here and below?
    ret_str = run_gdal_ls(
        script_path, ["", "-l", test_py_scripts.get_data_path("ogr") + "poly.shp"]
    )

    assert ret_str.find("poly.shp") != -1


###############################################################################
# List one dir


def test_gdal_ls_py_2(script_path):
    ret_str = run_gdal_ls(script_path, ["", "-l", test_py_scripts.get_data_path("ogr")])

    assert ret_str.find("poly.shp") != -1


###############################################################################
# List recursively


def test_gdal_ls_py_3(script_path):
    ret_str = run_gdal_ls(script_path, ["", "-R", test_py_scripts.get_data_path("ogr")])

    assert "topojson1.topojson" in ret_str


###############################################################################
# List in a .zip


def test_gdal_ls_py_4(script_path):
    ret_str = run_gdal_ls(
        script_path,
        ["", "-l", "/vsizip/" + test_py_scripts.get_data_path("ogr") + "shp/poly.zip"],
    )

    if (
        ret_str.find(
            "-r--r--r--  1 unknown unknown          415 2008-02-11 21:35 /vsizip/"
            + test_py_scripts.get_data_path("ogr")
            + "shp/poly.zip/poly.PRJ"
        )
        == -1
    ):
        gdaltest.skip_on_travis()
        # FIXME
        # Fails on Travis with dates at 1970-01-01 00:00
        # Looks like a 32/64bit issue with Python bindings of VSIStatL()
        pytest.fail(ret_str)


###############################################################################
# List dir in /vsicurl/


@pytest.mark.require_curl()
def test_gdal_ls_py_5(script_path):

    f = gdal.VSIFOpenL(
        "/vsicurl/https://raw.githubusercontent.com/OSGeo/gdal/release/3.1/autotest/ogr/data/poly.zip",
        "rb",
    )
    if f is None:
        pytest.skip()
    d = gdal.VSIFReadL(1, 1, f)
    gdal.VSIFCloseL(f)
    if not d:
        pytest.skip()

    # ret_str = run_gdal_ls(script_path, ['', '-R', 'https://raw.githubusercontent.com/OSGeo/gdal/release/3.1/autotest/ogr/data/'])

    #
    # if ret_str.find('/vsicurl/https://raw.githubusercontent.com/OSGeo/gdal/release/3.1/autotest/ogr/data/wkb_wkt/3d_broken_line.wkb') == -1:
    #    print(ret_str)
    #    pytest.fail()


###############################################################################
# List in a .zip in /vsicurl/


@pytest.mark.require_curl()
def test_gdal_ls_py_6(script_path):

    f = gdal.VSIFOpenL(
        "/vsicurl/https://raw.githubusercontent.com/OSGeo/gdal/release/3.1/autotest/ogr/data/poly.zip",
        "rb",
    )
    if f is None:
        pytest.skip()
    d = gdal.VSIFReadL(1, 1, f)
    gdal.VSIFCloseL(f)
    if not d:
        pytest.skip()

    ret_str = run_gdal_ls(
        script_path,
        [
            "",
            "-l",
            "/vsizip/vsicurl/https://raw.githubusercontent.com/OSGeo/gdal/release/3.1/autotest/ogr/data/poly.zip",
        ],
    )

    if (
        ret_str.find(
            "-r--r--r--  1 unknown unknown          415 2008-02-11 21:35 /vsizip/vsicurl/https://raw.githubusercontent.com/OSGeo/gdal/release/3.1/autotest/ogr/data/poly.zip/poly.PRJ"
        )
        == -1
    ):
        gdaltest.skip_on_travis()
        # FIXME
        # Fails on Travis with dates at 1970-01-01 00:00
        # Looks like a 32/64bit issue with Python bindings of VSIStatL()
        pytest.fail(ret_str)


###############################################################################
# List dir in /vsicurl/ and recurse in zip


@pytest.mark.require_curl()
def test_gdal_ls_py_7(script_path):

    # Super slow on AppVeyor since a few weeks (Apr 2016)
    if gdal.GetConfigOption("APPVEYOR") is not None:
        pytest.skip("Slow on AppVeyor")

    f = gdal.VSIFOpenL(
        "/vsicurl/https://raw.githubusercontent.com/OSGeo/gdal/release/3.1/autotest/ogr/data/poly.zip",
        "rb",
    )
    if f is None:
        pytest.skip()
    d = gdal.VSIFReadL(1, 1, f)
    gdal.VSIFCloseL(f)
    if not d:
        pytest.skip()

    # ret_str = run_gdal_ls(script_path, ['', '-R', '-Rzip', 'https://raw.githubusercontent.com/OSGeo/gdal/release/3.1/autotest/ogr/data/'])

    # if ret_str.find('/vsizip//vsicurl/https://raw.githubusercontent.com/OSGeo/gdal/release/3.1/autotest/ogr/data/poly.zip/poly.PRJ') == -1:
    #    print(ret_str)
    #    pytest.fail()


###############################################################################
# List FTP dir in /vsicurl/


@pytest.mark.require_curl()
def test_gdal_ls_py_8(script_path):
    if not gdaltest.run_slow_tests():
        pytest.skip()

    f = gdal.VSIFOpenL(
        "/vsicurl/https://raw.githubusercontent.com/OSGeo/gdal/release/3.1/autotest/ogr/data/poly.zip",
        "rb",
    )
    if f is None:
        pytest.skip()
    d = gdal.VSIFReadL(1, 1, f)
    gdal.VSIFCloseL(f)
    if not d:
        pytest.skip()

    ret_str = run_gdal_ls(
        script_path, ["", "-l", "-R", "-Rzip", "ftp://download.osgeo.org/gdal/data/aig"]
    )

    assert (
        ret_str.find(
            "-r--r--r--  1 unknown unknown        24576 2007-03-29 00:00 /vsicurl/ftp://download.osgeo.org/gdal/data/aig/nzdem/info/arc0002r.001"
        )
        != -1
    )

    assert (
        ret_str.find(
            "-r--r--r--  1 unknown unknown        24576 2007-03-29 12:20 /vsizip//vsicurl/ftp://download.osgeo.org/gdal/data/aig/nzdem.zip/nzdem/info/arc0002r.001"
        )
        != -1
    )
