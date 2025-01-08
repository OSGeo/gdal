#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test some URL specific translation issues.
# Author:   Howard Butler <hobu.inc@gmail.com>
#
###############################################################################
# Copyright (c) 2007, Howard Butler <hobu.inc@gmail.com>
# Copyright (c) 2008-2009, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import socket

import gdaltest
import pytest

from osgeo import osr

expected_wkt = 'GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.01745329251994328,AUTHORITY["EPSG","9122"]],AUTHORITY["EPSG","4326"]]'


def osr_url_test(url, expected_wkt):
    timeout = 10
    socket.setdefaulttimeout(timeout)
    if gdaltest.gdalurlopen(url) is None:
        pytest.skip()

    # Depend on the Accepts headers that ImportFromUrl sets to request SRS from sr.org
    srs = osr.SpatialReference()
    from osgeo import gdal

    with gdal.quiet_errors():
        try:
            srs.ImportFromUrl(url)
        except AttributeError:  # old-gen bindings don't have this method yet
            pytest.skip()
        except Exception:
            gdal.PopErrorHandler()
            if (
                gdal.GetLastErrorMsg()
                == "GDAL/OGR not compiled with libcurl support, remote requests not supported."
                or gdal.GetLastErrorMsg().find("timed out") != -1
            ):
                pytest.skip()
            pytest.fail("exception: " + gdal.GetLastErrorMsg())

    if (
        gdal.GetLastErrorMsg()
        == "GDAL/OGR not compiled with libcurl support, remote requests not supported."
        or gdal.GetLastErrorMsg().find("timed out") != -1
    ):
        pytest.skip()

    assert gdaltest.equal_srs_from_wkt(expected_wkt, srs.ExportToWkt())


def test_osr_url_1():
    osr_url_test("http://spatialreference.org/ref/epsg/4326/", expected_wkt)


def test_osr_url_2():
    osr_url_test("http://spatialreference.org/ref/epsg/4326/ogcwkt/", expected_wkt)


def test_osr_spatialreference_https_4326():
    osr_url_test("https://spatialreference.org/ref/epsg/4326/", expected_wkt)


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


def test_osr_SetFromUserInput_http_disabled():
    srs = osr.SpatialReference()
    with pytest.raises(Exception, match="due to ALLOW_NETWORK_ACCESS=NO"):
        srs.SetFromUserInput(
            "https://spatialreference.org/ref/epsg/4326/",
            options=["ALLOW_NETWORK_ACCESS=NO"],
        )
