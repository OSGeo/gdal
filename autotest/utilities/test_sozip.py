#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  sozip testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2022, Even Rouault <even dot rouault at spatialys.com>
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
import pytest
import test_cli_utilities

from osgeo import gdal

###############################################################################
#


def get_sozip_path():
    return test_cli_utilities.get_cli_utility_path("sozip")


###############################################################################


def test_sozip_list():
    if get_sozip_path() is None:
        pytest.skip()

    (out, err) = gdaltest.runexternal_out_and_err(
        get_sozip_path() + " --list ../gcore/data/zero_5GB_sozip_of_sozip.zip"
    )
    assert err is None or err == "", "got error/warning"
    assert " 5230829 " in out
    assert " 2022-12-19 17:41:08 " in out
    assert " yes " in out
    assert "32768 bytes" in out
    assert " zero_5GB.bin.zip" in out


###############################################################################


def test_sozip_create():
    if get_sozip_path() is None:
        pytest.skip()

    (out, err) = gdaltest.runexternal_out_and_err(
        get_sozip_path()
        + " -j --overwrite --enable-sozip=yes --sozip-chunk-size 128 tmp/sozip.zip ../gcore/data/byte.tif"
    )
    assert err is None or err == "", "got error/warning"

    md = gdal.GetFileMetadata("/vsizip/tmp/sozip.zip/byte.tif", "ZIP")
    assert md["SEEK_OPTIMIZED_VALID"] == "YES"
    assert md["SOZIP_CHUNK_SIZE"] == "128"

    gdal.Unlink("tmp/sozip.zip")


###############################################################################


def test_sozip_append():
    if get_sozip_path() is None:
        pytest.skip()

    gdal.Unlink("tmp/sozip.zip")

    (out, err) = gdaltest.runexternal_out_and_err(
        get_sozip_path()
        + " -j --enable-sozip=yes --sozip-chunk-size 128 tmp/sozip.zip ../gcore/data/byte.tif"
    )
    assert err is None or err == "", "got error/warning"

    (out, err) = gdaltest.runexternal_out_and_err(
        get_sozip_path() + " -j tmp/sozip.zip ../gcore/data/uint16.tif"
    )
    assert err is None or err == "", "got error/warning"

    md = gdal.GetFileMetadata("/vsizip/tmp/sozip.zip/byte.tif", "ZIP")
    assert md["SEEK_OPTIMIZED_VALID"] == "YES"
    assert md["SOZIP_CHUNK_SIZE"] == "128"

    md = gdal.GetFileMetadata("/vsizip/tmp/sozip.zip/uint16.tif", "ZIP")
    assert md != {}
    assert "SEEK_OPTIMIZED_VALID" not in md

    gdal.Unlink("tmp/sozip.zip")


###############################################################################


def test_sozip_validate():
    if get_sozip_path() is None:
        pytest.skip()

    gdal.Unlink("tmp/sozip.zip")

    (out, err) = gdaltest.runexternal_out_and_err(
        get_sozip_path()
        + " -j --enable-sozip=yes --sozip-chunk-size 128 tmp/sozip.zip ../gcore/data/byte.tif"
    )
    assert err is None or err == "", "got error/warning"

    (out, err) = gdaltest.runexternal_out_and_err(
        get_sozip_path() + " --validate tmp/sozip.zip"
    )
    assert err is None or err == "", "got error/warning"
    assert "File byte.tif has a valid SOZip index, using chunk_size = 128" in out
    assert (
        "tmp/sozip.zip is a valid .zip file, and contains 1 SOZip-enabled file(s)"
        in out
    )

    gdal.Unlink("tmp/sozip.zip")
