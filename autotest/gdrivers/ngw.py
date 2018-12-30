#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test NGW support.
# Author:   Dmitry Baryshnikov <polimax@mail.ru>
#
###############################################################################
# Copyright (c) 2018, NextGIS <info@nextgis.com>
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
import shutil
from osgeo import gdal

sys.path.append('../pymod')

import gdaltest
import json
import pytest

def check_availability(url):
    version_url = url + '/api/component/pyramid/pkg_version'
    if gdaltest.gdalurlopen(version_url) is None:
        return False

    # Check quota
    quota_url = url + '/api/resource/quota'
    quota_conn = gdaltest.gdalurlopen(quota_url)
    try:
        quota_json = json.loads(quota_conn.read())
        quota_conn.close()
        if quota_json is None:
            return False
        limit = quota_json['limit']
        count = quota_json['count']
        return limit - count > 10
    except:
        return False

###############################################################################
# Verify we have the driver.

def test_ngw_1():

    gdaltest.ngw_drv = gdal.GetDriverByName('NGW')
    if gdaltest.ngw_drv is None:
        pytest.skip()

    # Check support CreateCopy
    if gdaltest.ngw_drv.GetMetadataItem(gdal.DCAP_CREATECOPY) is None:
        gdaltest.ngw_drv = None
        pytest.skip()

    gdaltest.ngw_test_server = 'http://dev.nextgis.com/sandbox'

    if check_availability(gdaltest.ngw_test_server) == False:
        gdaltest.ngw_drv = None
        pytest.skip()

###############################################################################
# TODO: Create the NGW raster layer

###############################################################################
# Open the NGW dataset

def test_ngw_2():

    if gdaltest.ngw_drv is None:
        pytest.skip()

    if check_availability(gdaltest.ngw_test_server) == False:
        gdaltest.ngw_drv = None
        pytest.skip()

    url = 'NGW:' + gdaltest.ngw_test_server + '/resource/1734'
    gdaltest.ngw_ds = gdal.Open(url)

    assert gdaltest.ngw_ds is not None, 'Open {} failed.'.format(url)

###############################################################################
# Check various things about the configuration.

def test_ngw_3():

    if gdaltest.ngw_drv is None or gdaltest.ngw_ds is None:
        pytest.skip()

    if check_availability(gdaltest.ngw_test_server) == False:
        gdaltest.ngw_drv = None
        pytest.skip()

    assert gdaltest.ngw_ds.RasterXSize == 1073741824 and \
           gdaltest.ngw_ds.RasterYSize == 1073741824 and \
           gdaltest.ngw_ds.RasterCount == 4, 'Wrong size or band count.'

    wkt = gdaltest.ngw_ds.GetProjectionRef()
    assert wkt[:33] == 'PROJCS["WGS 84 / Pseudo-Mercator"', 'Got wrong SRS: ' + wkt

    gt = gdaltest.ngw_ds.GetGeoTransform()
    # -20037508.34, 0.037322767712175846, 0.0, 20037508.34, 0.0, -0.037322767712175846
    assert abs(gt[0] - -20037508.34) < 0.00001 \
       or abs(gt[3] - 20037508.34) < 0.00001 \
       or abs(gt[1] - 0.037322767712175846) < 0.00001 \
       or abs(gt[2] - 0.0) < 0.00001 \
       or abs(gt[5] - -0.037322767712175846) < 0.00001 \
       or abs(gt[4] - 0.0) < 0.00001, 'Wrong geotransform. {}'.format(gt)

    assert gdaltest.ngw_ds.GetRasterBand(1).GetOverviewCount() > 0, 'No overviews!'
    assert gdaltest.ngw_ds.GetRasterBand(1).DataType == gdal.GDT_Byte, \
        'Wrong band data type.'

###############################################################################
# Check checksum for a small region.

def test_ngw_4():

    if gdaltest.ngw_drv is None or gdaltest.ngw_ds is None:
        pytest.skip()

    if check_availability(gdaltest.ngw_test_server) == False:
        gdaltest.ngw_drv = None
        pytest.skip()

    gdal.SetConfigOption('CPL_ACCUM_ERROR_MSG', 'ON')
    gdal.PushErrorHandler('CPLQuietErrorHandler')

    cs = gdaltest.ngw_ds.GetRasterBand(1).Checksum(0, 0, 100, 100)

    gdal.PopErrorHandler()
    gdal.SetConfigOption('CPL_ACCUM_ERROR_MSG', 'OFF')
    msg = gdal.GetLastErrorMsg()
    gdal.ErrorReset()

    if msg is not None and msg != '':
        print('Last error message: {}.'.format(msg))
        pytest.skip()

    assert cs == 57182, 'Wrong checksum: ' + str(cs)

###############################################################################
# Test getting subdatasets from GetCapabilities

def test_ngw_5():

    if gdaltest.ngw_drv is None:
        pytest.skip()

    if check_availability(gdaltest.ngw_test_server) == False:
        gdaltest.ngw_drv = None
        pytest.skip()

    url = 'NGW:' + gdaltest.ngw_test_server + '/resource/1730'
    ds = gdal.Open(url)
    assert ds is not None, 'Open of {} failed.'.format(url)

    subdatasets = ds.GetMetadata("SUBDATASETS")
    assert subdatasets, 'Did not get expected subdataset count.'

    ds = None

    name = subdatasets['SUBDATASET_0_NAME']
    ds = gdal.Open(name)
    assert ds is not None, 'Open of {} failed.'.format(name)

    ds = None

###############################################################################

def test_ngw_cleanup():

    gdaltest.ngw_ds = None
    gdaltest.clean_tmp()

    try:
        shutil.rmtree('gdalwmscache')
    except OSError:
        pass
