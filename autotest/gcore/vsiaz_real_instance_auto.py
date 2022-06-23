#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test /vsiaz
# Author:   Even Rouault <even dot rouault at spatialys dot com>
#
###############################################################################
# Copyright (c) 2017 Even Rouault <even dot rouault at spatialys dot com>
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

import gdaltest
import os
import pytest
import stat
import sys

pytestmark = pytest.mark.skipif(not gdaltest.built_against_curl(), reason="GDAL not built against curl")


def open_for_read(uri):
    """
    Opens a test file for reading.
    """
    return gdal.VSIFOpenExL(uri, 'rb', 1)

###############################################################################
@pytest.fixture(autouse=True, scope='module')
def startup_and_cleanup():

    az_vars = {}
    for var, reset_val in (
                ('AZURE_STORAGE_CONNECTION_STRING', None),
                ('AZURE_STORAGE_ACCOUNT', None),
                ('AZURE_STORAGE_ACCESS_KEY', None),
                ('AZURE_STORAGE_SAS_TOKEN', None),
                ('AZURE_NO_SIGN_REQUEST', None),
                ('AZURE_CONFIG_DIR', ''),
                ('AZURE_STORAGE_ACCESS_TOKEN', '')):
        az_vars[var] = gdal.GetConfigOption(var)
        gdal.SetConfigOption(var, reset_val)

    assert gdal.GetSignedURL('/vsiaz/foo/bar') is None

    yield

    for var in az_vars:
        gdal.SetConfigOption(var, az_vars[var])

###############################################################################
# Error cases


def test_vsiaz_real_server_errors():

    if not gdaltest.built_against_curl():
        pytest.skip()

    # Missing AZURE_STORAGE_ACCOUNT
    gdal.ErrorReset()
    with gdaltest.error_handler():
        f = open_for_read('/vsiaz/foo/bar')
    assert f is None and gdal.VSIGetLastErrorMsg().find('AZURE_STORAGE_ACCOUNT') >= 0

    gdal.ErrorReset()
    with gdaltest.error_handler():
        f = open_for_read('/vsiaz_streaming/foo/bar')
    assert f is None and gdal.VSIGetLastErrorMsg().find('AZURE_STORAGE_ACCOUNT') >= 0

    # Invalid AZURE_STORAGE_CONNECTION_STRING
    with gdaltest.config_option('AZURE_STORAGE_CONNECTION_STRING', 'invalid'):
        gdal.ErrorReset()
        with gdaltest.error_handler():
            f = open_for_read('/vsiaz/foo/bar')
        assert f is None

    # Missing AZURE_STORAGE_ACCESS_KEY
    gdal.ErrorReset()
    with gdaltest.config_options({'AZURE_STORAGE_ACCOUNT': 'AZURE_STORAGE_ACCOUNT',
                                  'CPL_AZURE_VM_API_ROOT_URL': 'disabled'}):
        with gdaltest.error_handler():
            f = open_for_read('/vsiaz/foo/bar')
        assert f is None and gdal.VSIGetLastErrorMsg().find('AZURE_STORAGE_ACCESS_KEY') >= 0

    # AZURE_STORAGE_ACCOUNT and AZURE_STORAGE_ACCESS_KEY but invalid
    gdal.ErrorReset()
    with gdaltest.config_options({'AZURE_STORAGE_ACCOUNT': 'AZURE_STORAGE_ACCOUNT',
                                  'AZURE_STORAGE_ACCESS_KEY': 'AZURE_STORAGE_ACCESS_KEY'}):
        with gdaltest.error_handler():
            f = open_for_read('/vsiaz/foo/bar.baz')
        if f is not None:
            if f is not None:
                gdal.VSIFCloseL(f)
            if gdal.GetConfigOption('APPVEYOR') is not None:
                return
            pytest.fail(gdal.VSIGetLastErrorMsg())

        gdal.ErrorReset()
        with gdaltest.error_handler():
            f = open_for_read('/vsiaz_streaming/foo/bar.baz')
        assert f is None, gdal.VSIGetLastErrorMsg()


###############################################################################
# Test AZURE_NO_SIGN_REQUEST=YES
# FIXME: this resource is no longer accessible through no sign request...
# Find another one.

def test_vsiaz_no_sign_request():

    if not gdaltest.built_against_curl():
        pytest.skip()

    gdal.VSICurlClearCache()

    with gdaltest.config_options({ 'AZURE_STORAGE_ACCOUNT': 'naipblobs', 'AZURE_NO_SIGN_REQUEST': 'YES'}):
        actual_url = gdal.GetActualURL('/vsiaz/naip/v002/al/2015/al_100cm_2015/30086/m_3008601_ne_16_1_20150804.tif')
        assert actual_url == 'https://naipblobs.blob.core.windows.net/naip/v002/al/2015/al_100cm_2015/30086/m_3008601_ne_16_1_20150804.tif'
        assert actual_url == gdal.GetSignedURL('/vsiaz/naip/v002/al/2015/al_100cm_2015/30086/m_3008601_ne_16_1_20150804.tif')

        f = open_for_read('/vsiaz/naip/v002/al/2015/al_100cm_2015/30086/m_3008601_ne_16_1_20150804.tif')
        if f is None:
            if gdaltest.gdalurlopen('https://naipblobs.blob.core.windows.net/naip/v002/al/2015/al_100cm_2015/30086/m_3008601_ne_16_1_20150804.tif') is None:
                pytest.skip('cannot open URL')
            pytest.fail()

        gdal.VSIFCloseL(f)

        assert 'm_3008601_ne_16_1_20150804.tif' in gdal.ReadDir('/vsiaz/naip/v002/al/2015/al_100cm_2015/30086/')

        gdal.VSICurlClearCache()

        assert stat.S_ISDIR(gdal.VSIStatL('/vsiaz/naip').mode)

        gdal.VSICurlClearCache()

        assert stat.S_ISDIR(gdal.VSIStatL('/vsiaz/naip/').mode)

        gdal.VSICurlClearCache()

        assert gdal.VSIStatL('/vsiaz/naip_i_dont_exist') is None

        gdal.VSICurlClearCache()

        assert stat.S_ISDIR(gdal.VSIStatL('/vsiaz/naip/v002').mode)


###############################################################################
# Test AZURE_SAS option

@pytest.mark.skipif(sys.platform == 'darwin' and 'CI' in os.environ, reason='Randomly fails on MacOSX. Not sure why.')
def test_vsiaz_sas():

    if not gdaltest.built_against_curl():
        pytest.skip()

    gdal.VSICurlClearCache()

    # See https://azure.microsoft.com/en-us/services/open-datasets/catalog/naip/ for the value of AZURE_SAS
    with gdaltest.config_options({ 'AZURE_STORAGE_ACCOUNT': 'naipblobs', 'AZURE_SAS': 'st=2019-07-18T03%3A53%3A22Z&se=2035-07-19T03%3A53%3A00Z&sp=rl&sv=2018-03-28&sr=c&sig=2RIXmLbLbiagYnUd49rgx2kOXKyILrJOgafmkODhRAQ%3D'}):
        actual_url = gdal.GetActualURL('/vsiaz/naip/v002/al/2015/al_100cm_2015/30086/m_3008601_ne_16_1_20150804.tif')
        assert actual_url == 'https://naipblobs.blob.core.windows.net/naip/v002/al/2015/al_100cm_2015/30086/m_3008601_ne_16_1_20150804.tif'
        assert gdal.GetSignedURL('/vsiaz/naip/v002/al/2015/al_100cm_2015/30086/m_3008601_ne_16_1_20150804.tif') == 'https://naipblobs.blob.core.windows.net/naip/v002/al/2015/al_100cm_2015/30086/m_3008601_ne_16_1_20150804.tif?st=2019-07-18T03%3A53%3A22Z&se=2035-07-19T03%3A53%3A00Z&sp=rl&sv=2018-03-28&sr=c&sig=2RIXmLbLbiagYnUd49rgx2kOXKyILrJOgafmkODhRAQ%3D'

        f = open_for_read('/vsiaz/naip/v002/al/2015/al_100cm_2015/30086/m_3008601_ne_16_1_20150804.tif')
        if f is None:
            if gdaltest.gdalurlopen('https://naipblobs.blob.core.windows.net/naip/v002/al/2015/al_100cm_2015/30086/m_3008601_ne_16_1_20150804.tif') is None:
                pytest.skip('cannot open URL')
            pytest.fail()

        gdal.VSIFCloseL(f)

        assert 'm_3008601_ne_16_1_20150804.tif' in gdal.ReadDir('/vsiaz/naip/v002/al/2015/al_100cm_2015/30086/')

        gdal.VSICurlClearCache()

        assert stat.S_ISDIR(gdal.VSIStatL('/vsiaz/naip').mode)

        gdal.VSICurlClearCache()

        assert stat.S_ISDIR(gdal.VSIStatL('/vsiaz/naip/').mode)

        gdal.VSICurlClearCache()

        assert gdal.VSIStatL('/vsiaz/naip_i_dont_exist') is None

        gdal.VSICurlClearCache()

        assert stat.S_ISDIR(gdal.VSIStatL('/vsiaz/naip/v002').mode)
