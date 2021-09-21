#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test HTTP Driver.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2007, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2009-2012, Even Rouault <even dot rouault at spatialys.com>
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

import subprocess
import sys
from osgeo import gdal
from osgeo import ogr

import pytest

try:
    import gdaltest
except ImportError:
    # running as a subprocess in the windows build (see __main__ block),
    # so conftest.py hasn't run, so sys.path doesn't have pymod in it
    sys.path.append('../pymod')
    import gdaltest


pytestmark = pytest.mark.require_driver('HTTP')


@pytest.fixture(autouse=True, scope="module")
def disable_dods_driver():
    dods_drv = gdal.GetDriverByName('DODS')
    if dods_drv is not None:
        dods_drv.Deregister()
    yield
    if dods_drv is not None:
        dods_drv.Register()
    dods_drv = None


@pytest.fixture(autouse=True)
def set_http_timeout():
    with gdaltest.config_option('GDAL_HTTP_TIMEOUT', '5'):
        yield


def skip_if_unreachable(url, try_read=False):
    conn = gdaltest.gdalurlopen(url, timeout=4)
    if conn is None:
        pytest.skip('cannot open URL')
    if try_read:
        try:
            conn.read()
        except Exception:
            pytest.skip('cannot read')
    conn.close()


###############################################################################
# Verify we have the driver.


def test_http_1():
    url = 'http://gdal.org/gdalicon.png'
    tst = gdaltest.GDALTest('PNG', url, 1, 7617, filename_absolute=1)
    ret = tst.testOpen()
    if ret == 'fail':
        skip_if_unreachable(url)
        pytest.fail()

###############################################################################
# Verify /vsicurl (subversion file listing)


def test_http_2():
    url = 'https://raw.githubusercontent.com/OSGeo/gdal/release/3.1/autotest/gcore/data/byte.tif'
    tst = gdaltest.GDALTest('GTiff', '/vsicurl/' + url, 1, 4672, filename_absolute=1)
    ret = tst.testOpen()
    if ret == 'fail':
        skip_if_unreachable(url)
        pytest.fail()

###############################################################################
# Verify /vsicurl (apache file listing)


def test_http_3():
    url = 'http://download.osgeo.org/gdal/data/ehdr/elggll.bil'
    ds = gdal.Open('/vsicurl/' + url)
    if ds is None:
        skip_if_unreachable(url)
        pytest.fail()

###############################################################################
# Verify /vsicurl (ftp)


def test_http_4():
    # Too unreliable
    if gdaltest.skip_on_travis():
        pytest.skip()

    url = 'ftp://download.osgeo.org/gdal/data/gtiff/utm.tif'
    ds = gdal.Open('/vsicurl/' + url)
    if ds is None:
        skip_if_unreachable(url, try_read=True)
        if sys.platform == 'darwin' and gdal.GetConfigOption('TRAVIS', None) is not None:
            pytest.skip("Fails on MacOSX Travis sometimes. Not sure why.")
        pytest.fail()

    filelist = ds.GetFileList()
    assert '/vsicurl/ftp://download.osgeo.org/gdal/data/gtiff/utm.tif' in filelist

###############################################################################
# Test HTTP driver with OGR driver


def test_http_6():
    url = 'https://raw.githubusercontent.com/OSGeo/gdal/release/3.1/autotest/ogr/data/poly.dbf'
    ds = ogr.Open(url)
    if ds is None:
        skip_if_unreachable(url, try_read=True)
        pytest.fail()
    ds = None


###############################################################################

def test_http_ssl_verifystatus():
    with gdaltest.config_option('GDAL_HTTP_SSL_VERIFYSTATUS', 'YES'):
        with gdaltest.error_handler():
            # For now this URL doesn't support OCSP stapling...
            gdal.OpenEx('https://google.com', allowed_drivers=['HTTP'])
    last_err = gdal.GetLastErrorMsg()
    if 'No OCSP response received' not in last_err and 'libcurl too old' not in last_err:

        # The test actually works on Travis Mac
        if sys.platform == 'darwin' and gdal.GetConfigOption('TRAVIS', None) is not None:
            pytest.skip()

        pytest.fail(last_err)

    
###############################################################################


def test_http_use_capi_store():
    if sys.platform != 'win32':
        with gdaltest.error_handler():
            return test_http_use_capi_store_sub()

    # Prints this to stderr in many cases (but doesn't error)
    # Warning 6: GDAL_HTTP_USE_CAPI_STORE requested, but libcurl too old, non-Windows platform or OpenSSL missing.
    subprocess.check_call(
        [sys.executable, 'gdalhttp.py', '-use_capi_store'],
    )


def test_http_use_capi_store_sub():

    with gdaltest.config_option('GDAL_HTTP_USE_CAPI_STORE', 'YES'):
        gdal.OpenEx('https://google.com', allowed_drivers=['HTTP'])

    
###############################################################################
#


if __name__ == '__main__':
    if len(sys.argv) == 2 and sys.argv[1] == '-use_capi_store':
        test_http_use_capi_store_sub()
