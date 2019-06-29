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
# Copyright (c) 2009-2012, Even Rouault <even dot rouault at mines-paris dot org>
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

###############################################################################
# Verify we have the driver.


def test_http_1():

    gdaltest.dods_drv = None

    drv = gdal.GetDriverByName('HTTP')
    if drv is None:
        pytest.skip()

    gdaltest.dods_drv = gdal.GetDriverByName('DODS')
    if gdaltest.dods_drv is not None:
        gdaltest.dods_drv.Deregister()

    tst = gdaltest.GDALTest('PNG', 'http://gdal.org/gdalicon.png',
                            1, 7617, filename_absolute=1)
    ret = tst.testOpen()
    if ret == 'fail':
        conn = gdaltest.gdalurlopen('http://gdal.org/gdalicon.png')
        if conn is None:
            pytest.skip('cannot open URL')
        conn.close()

    return ret

###############################################################################
# Verify /vsicurl (subversion file listing)


def test_http_2():

    drv = gdal.GetDriverByName('HTTP')
    if drv is None:
        pytest.skip()

    tst = gdaltest.GDALTest('GTiff', '/vsicurl/https://raw.githubusercontent.com/OSGeo/gdal/master/autotest/gcore/data/byte.tif',
                            1, 4672, filename_absolute=1)
    ret = tst.testOpen()
    if ret == 'fail':
        conn = gdaltest.gdalurlopen('https://raw.githubusercontent.com/OSGeo/gdal/master/autotest/gcore/data/byte.tif')
        if conn is None:
            pytest.skip('cannot open URL')
        conn.close()

    return ret

###############################################################################
# Verify /vsicurl (apache file listing)


def test_http_3():

    drv = gdal.GetDriverByName('HTTP')
    if drv is None:
        pytest.skip()

    gdal.SetConfigOption('GDAL_HTTP_TIMEOUT', '5')
    ds = gdal.Open('/vsicurl/http://download.osgeo.org/gdal/data/ehdr/elggll.bil')
    gdal.SetConfigOption('GDAL_HTTP_TIMEOUT', None)
    if ds is None:
        conn = gdaltest.gdalurlopen('http://download.osgeo.org/gdal/data/ehdr/elggll.bil')
        if conn is None:
            pytest.skip('cannot open URL')
        conn.close()
        pytest.fail()

    
###############################################################################
# Verify /vsicurl (ftp)


def http_4_old():

    drv = gdal.GetDriverByName('HTTP')
    if drv is None:
        pytest.skip()

    ds = gdal.Open('/vsicurl/ftp://ftp2.cits.rncan.gc.ca/pub/cantopo/250k_tif/MCR2010_01.tif')
    if ds is None:

        # Workaround unexplained failure on Tamas test machine. The test works fine with his
        # builds on other machines...
        # This heuristics might be fragile !
        if "GDAL_DATA" in os.environ and os.environ["GDAL_DATA"].find("E:\\builds\\..\\sdk\\") == 0:
            pytest.skip()

        conn = gdaltest.gdalurlopen('ftp://ftp2.cits.rncan.gc.ca/pub/cantopo/250k_tif/MCR2010_01.tif')
        if conn is None:
            pytest.skip('cannot open URL')
        conn.close()
        pytest.fail()

    filelist = ds.GetFileList()
    assert filelist[0] == '/vsicurl/ftp://ftp2.cits.rncan.gc.ca/pub/cantopo/250k_tif/MCR2010_01.tif'

###############################################################################
# Verify /vsicurl (ftp)


def test_http_4():

    # Too unreliable
    if gdaltest.skip_on_travis():
        pytest.skip()

    drv = gdal.GetDriverByName('HTTP')
    if drv is None:
        pytest.skip()

    ds = gdal.Open('/vsicurl/ftp://download.osgeo.org/gdal/data/gtiff/utm.tif')
    if ds is None:
        conn = gdaltest.gdalurlopen('ftp://download.osgeo.org/gdal/data/gtiff/utm.tif', timeout=4)
        if conn is None:
            pytest.skip('cannot open URL')
        try:
            conn.read()
        except:
            pytest.skip('cannot read')
        conn.close()
        if sys.platform == 'darwin' and gdal.GetConfigOption('TRAVIS', None) is not None:
            pytest.skip("Fails on MacOSX Travis sometimes. Not sure why.")
        pytest.fail()

    filelist = ds.GetFileList()
    assert '/vsicurl/ftp://download.osgeo.org/gdal/data/gtiff/utm.tif' in filelist

###############################################################################
# Test HTTP driver with non VSIL driver


def test_http_5():

    drv = gdal.GetDriverByName('HTTP')
    if drv is None:
        pytest.skip()

    ds = gdal.Open('https://raw.githubusercontent.com/OSGeo/gdal/master/autotest/gdrivers/data/s4103.blx')
    if ds is None:
        conn = gdaltest.gdalurlopen('https://raw.githubusercontent.com/OSGeo/gdal/master/autotest/gdrivers/data/s4103.blx')
        if conn is None:
            pytest.skip('cannot open URL')
        try:
            conn.read()
        except:
            pytest.skip('cannot read')
        conn.close()
        pytest.fail()
    filename = ds.GetDescription()
    ds = None

    with pytest.raises(OSError):
        os.stat(filename)
        pytest.fail('file %s should have been removed' % filename)

    
###############################################################################
# Test HTTP driver with OGR driver


def test_http_6():

    drv = gdal.GetDriverByName('HTTP')
    if drv is None:
        pytest.skip()

    ds = ogr.Open('https://raw.githubusercontent.com/OSGeo/gdal/master/autotest/ogr/data/test.jml')
    if ds is None:
        conn = gdaltest.gdalurlopen('https://raw.githubusercontent.com/OSGeo/gdal/master/autotest/ogr/data/test.jml')
        if conn is None:
            pytest.skip('cannot open URL')
        try:
            conn.read()
        except:
            pytest.skip('cannot read')
        conn.close()
        pytest.fail()
    ds = None


###############################################################################

def test_http_ssl_verifystatus():

    if gdal.GetDriverByName('HTTP') is None:
        pytest.skip()

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

    if gdal.GetDriverByName('HTTP') is None:
        pytest.skip()

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


def test_http_cleanup():
    if gdaltest.dods_drv is not None:
        gdaltest.dods_drv.Register()
    gdaltest.dods_drv = None


if __name__ == '__main__':
    if len(sys.argv) == 2 and sys.argv[1] == '-use_capi_store':
        test_http_use_capi_store_sub()
