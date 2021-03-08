#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test some URL specific translation issues.
# Author:   Howard Butler <hobu.inc@gmail.com>
#
###############################################################################
# Copyright (c) 2007, Howard Butler <hobu.inc@gmail.com>
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

import socket


import gdaltest
from osgeo import osr
import pytest

expected_wkt = 'GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.01745329251994328,AUTHORITY["EPSG","9122"]],AUTHORITY["EPSG","4326"]]'


def osr_url_test(url, expected_wkt):
    timeout = 10
    socket.setdefaulttimeout(timeout)
    if gdaltest.gdalurlopen(url) is None:
        pytest.skip()

    # Depend on the Accepts headers that ImportFromUrl sets to request SRS from sr.org
    srs = osr.SpatialReference()
    from osgeo import gdal
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    try:
        srs.ImportFromUrl(url)
    except AttributeError:  # old-gen bindings don't have this method yet
        pytest.skip()
    except Exception:
        gdal.PopErrorHandler()
        if gdal.GetLastErrorMsg() == "GDAL/OGR not compiled with libcurl support, remote requests not supported." or \
           gdal.GetLastErrorMsg().find("timed out") != -1:
            pytest.skip()
        pytest.fail('exception: ' + gdal.GetLastErrorMsg())

    gdal.PopErrorHandler()
    if gdal.GetLastErrorMsg() == "GDAL/OGR not compiled with libcurl support, remote requests not supported." or \
       gdal.GetLastErrorMsg().find("timed out") != -1:
        pytest.skip()

    assert gdaltest.equal_srs_from_wkt(expected_wkt,
                                       srs.ExportToWkt())


def test_osr_url_1():
    osr_url_test('http://spatialreference.org/ref/epsg/4326/', expected_wkt)

def test_osr_url_2():
    osr_url_test('http://spatialreference.org/ref/epsg/4326/ogcwkt/', expected_wkt)

def test_osr_spatialreference_https_4326():
    osr_url_test('https://spatialreference.org/ref/epsg/4326/', expected_wkt)

def test_osr_www_opengis_http_4326():
    srs = osr.SpatialReference()
    assert srs.SetFromUserInput("http://www.opengis.net/def/crs/EPSG/0/4326") == 0
    assert gdaltest.equal_srs_from_wkt(expected_wkt, srs.ExportToWkt())

def test_osr_opengis_http_4326():
    srs = osr.SpatialReference()
    assert srs.SetFromUserInput("http://opengis.net/def/crs/EPSG/0/4326") == 0
    assert gdaltest.equal_srs_from_wkt(expected_wkt, srs.ExportToWkt())

def test_osr_www_opengis_https_4326():
    srs = osr.SpatialReference()
    assert srs.SetFromUserInput("https://www.opengis.net/def/crs/EPSG/0/4326") == 0
    assert gdaltest.equal_srs_from_wkt(expected_wkt, srs.ExportToWkt())

def test_osr_opengis_https_4326():
    srs = osr.SpatialReference()
    assert srs.SetFromUserInput("https://opengis.net/def/crs/EPSG/0/4326") == 0
    assert gdaltest.equal_srs_from_wkt(expected_wkt, srs.ExportToWkt())
