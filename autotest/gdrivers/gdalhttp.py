#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test HTTP Driver.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2007, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2009-2012, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import os
import subprocess
import sys

import pytest

from osgeo import gdal, ogr

try:
    import gdaltest
except ImportError:
    # running as a subprocess in the windows build (see __main__ block),
    # so conftest.py hasn't run, so sys.path doesn't have pymod in it
    sys.path.append("../pymod")
    import gdaltest


pytestmark = pytest.mark.require_driver("HTTP")


###############################################################################
@pytest.fixture(autouse=True, scope="module")
def module_disable_exceptions():
    with gdaltest.disable_exceptions():
        yield


@pytest.fixture(autouse=True)
def set_http_timeout():
    with gdaltest.config_option("GDAL_HTTP_TIMEOUT", "5"):
        yield


def skip_if_unreachable(url, try_read=False):
    conn = gdaltest.gdalurlopen(url, timeout=4)
    if conn is None:
        pytest.skip("cannot open URL")
    if try_read:
        try:
            conn.read()
        except Exception:
            pytest.skip("cannot read")
    conn.close()


###############################################################################
# Verify we have the driver.


@pytest.mark.network
def test_http_1():
    # Regularly fails on Travis graviton2 configuration
    gdaltest.skip_on_travis()

    url = "http://gdal.org/gdalicon.png"
    tst = gdaltest.GDALTest("PNG", url, 1, 7617, filename_absolute=1)
    try:
        tst.testOpen()
    except Exception:
        skip_if_unreachable(url)
        if gdaltest.is_travis_branch(
            "build-windows-conda"
        ) or gdaltest.is_travis_branch("build-windows-minimum"):
            pytest.xfail("randomly fail on that configuration for unknown reason")
        pytest.fail()


###############################################################################
# Verify /vsicurl (subversion file listing)


@pytest.mark.network
def test_http_2():
    url = "https://raw.githubusercontent.com/OSGeo/gdal/release/3.1/autotest/gcore/data/byte.tif"
    tst = gdaltest.GDALTest("GTiff", "/vsicurl/" + url, 1, 4672, filename_absolute=1)

    try:
        tst.testOpen()
    except Exception:
        skip_if_unreachable(url)
        pytest.fail()


###############################################################################
# Verify /vsicurl (apache file listing)


@pytest.mark.network
def test_http_3():
    url = "http://download.osgeo.org/gdal/data/ehdr/elggll.bil"
    ds = gdal.Open("/vsicurl/" + url)
    if ds is None:
        skip_if_unreachable(url)
        pytest.fail()


###############################################################################
# Verify /vsicurl (ftp)


@pytest.mark.network
@pytest.mark.skip(reason="remove server does not work")
def test_http_4():
    # Too unreliable
    gdaltest.skip_on_travis()

    url = "ftp://download.osgeo.org/gdal/data/gtiff/utm.tif"
    ds = gdal.Open("/vsicurl/" + url)
    if ds is None:
        skip_if_unreachable(url, try_read=True)
        if (
            sys.platform == "darwin"
            and gdal.GetConfigOption("TRAVIS", None) is not None
        ):
            pytest.skip("Fails on MacOSX Travis sometimes. Not sure why.")
        pytest.fail()

    filelist = ds.GetFileList()
    assert "/vsicurl/ftp://download.osgeo.org/gdal/data/gtiff/utm.tif" in filelist


###############################################################################
# Test HTTP driver with OGR driver


@pytest.mark.network
def test_http_6():
    url = "https://raw.githubusercontent.com/OSGeo/gdal/release/3.1/autotest/ogr/data/poly.dbf"
    ds = ogr.Open(url)
    if ds is None:
        skip_if_unreachable(url, try_read=True)
        pytest.fail()
    ds = None


###############################################################################


@pytest.mark.network
def test_http_ssl_verifystatus():
    with gdaltest.config_option("GDAL_HTTP_SSL_VERIFYSTATUS", "YES"):
        with gdal.quiet_errors():
            # For now this URL doesn't support OCSP stapling...
            gdal.OpenEx("https://google.com", allowed_drivers=["HTTP"])
    last_err = gdal.GetLastErrorMsg()
    if "timed out" in last_err:
        pytest.skip(last_err)
    if (
        "No OCSP response received" not in last_err
        and "libcurl too old" not in last_err
    ):

        # The test actually works on Travis Mac
        if (
            sys.platform == "darwin"
            and gdal.GetConfigOption("TRAVIS", None) is not None
        ):
            pytest.skip()

        # The test actually works on build-windows-conda
        if "SKIP_GDAL_HTTP_SSL_VERIFYSTATUS" in os.environ:
            pytest.skip()

        pytest.fail(last_err)


###############################################################################


@pytest.mark.network
def test_http_use_capi_store():
    if sys.platform != "win32":
        with gdal.quiet_errors():
            return test_http_use_capi_store_sub()

    # Prints this to stderr in many cases (but doesn't error)
    # Warning 6: GDAL_HTTP_USE_CAPI_STORE requested, but libcurl too old, non-Windows platform or OpenSSL missing.
    subprocess.check_call(
        [sys.executable, "gdalhttp.py", "-use_capi_store"],
    )


def test_http_use_capi_store_sub():

    with gdaltest.disable_exceptions(), gdaltest.config_option(
        "GDAL_HTTP_USE_CAPI_STORE", "YES"
    ):
        gdal.OpenEx("https://google.com", allowed_drivers=["HTTP"])


###############################################################################


@pytest.mark.network
def test_http_keep_alive():

    # Rather dummy test. Just trigger the code path

    with gdaltest.config_option("GDAL_HTTP_TCP_KEEPALIVE", "YES"):
        gdal.OpenEx("https://google.com", allowed_drivers=["HTTP"])

    with gdaltest.config_options(
        {
            "GDAL_HTTP_TCP_KEEPALIVE": "YES",
            "GDAL_HTTP_TCP_KEEPINTVL": "1",
            "GDAL_HTTP_TCP_KEEPIDLE": "1",
        }
    ):
        gdal.OpenEx("https://google.com", allowed_drivers=["HTTP"])


###############################################################################
#


if __name__ == "__main__":
    if len(sys.argv) == 2 and sys.argv[1] == "-use_capi_store":
        test_http_use_capi_store_sub()
