#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test /vsicurl
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2011, Even Rouault <even dot rouault at spatialys.com>
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
import time

import gdaltest
import pytest
import webserver

from osgeo import gdal, ogr

pytestmark = pytest.mark.require_curl()

###############################################################################
#


@pytest.mark.slow()
def test_vsicurl_1():

    ds = ogr.Open(
        "/vsizip/vsicurl/http://publicfiles.dep.state.fl.us/dear/BWR_GIS/2007NWFLULC/NWFWMD2007LULC.zip"
    )
    assert ds is not None


###############################################################################
#


@pytest.mark.slow()
def vsicurl_2():

    ds = gdal.Open(
        "/vsizip//vsicurl/http://eros.usgs.gov/archive/nslrsda/GeoTowns/HongKong/srtm/n22e113.zip/n22e113.bil"
    )
    assert ds is not None


###############################################################################
# This server doesn't support range downloading


@pytest.mark.slow()
def vsicurl_3():

    ds = ogr.Open(
        "/vsizip/vsicurl/http://www.iucnredlist.org/spatial-data/MAMMALS_TERRESTRIAL.zip"
    )
    assert ds is None


###############################################################################
# This server doesn't support range downloading


@pytest.mark.slow()
def test_vsicurl_4():

    ds = ogr.Open(
        "/vsizip/vsicurl/http://lelserver.env.duke.edu:8080/LandscapeTools/export/49/Downloads/1_Habitats.zip"
    )
    assert ds is None


###############################################################################
# Test URL unescaping when reading HTTP file list


@pytest.mark.slow()
def test_vsicurl_5():

    ds = gdal.Open(
        "/vsicurl/http://dds.cr.usgs.gov/srtm/SRTM_image_sample/picture%20examples/N34W119_DEM.tif"
    )
    assert ds is not None


###############################################################################
# Test with FTP server that doesn't support EPSV command


@pytest.mark.slow()
def vsicurl_6_disabled():

    fl = gdal.ReadDir("/vsicurl/ftp://ftp2.cits.rncan.gc.ca/pub/cantopo/250k_tif")
    assert fl


###############################################################################
# Test Microsoft-IIS/6.0 listing


@pytest.mark.slow()
def test_vsicurl_7():

    fl = gdal.ReadDir("/vsicurl/http://ortho.linz.govt.nz/tifs/2005_06")
    assert fl


###############################################################################
# Test interleaved reading between 2 datasets


@pytest.mark.slow()
def vsicurl_8():

    ds1 = gdal.Open(
        "/vsigzip//vsicurl/http://dds.cr.usgs.gov/pub/data/DEM/250/notavail/C/chipicoten-w.gz"
    )
    gdal.Open(
        "/vsizip//vsicurl/http://edcftp.cr.usgs.gov/pub/data/landcover/files/2009/biso/gokn09b_dnbr.zip/nps-serotnbsp-9001-20090321_rd.tif"
    )
    cs = ds1.GetRasterBand(1).Checksum()
    assert cs == 61342


###############################################################################
# Test reading a file with Chinese characters, but the HTTP file listing
# returns escaped sequences instead of the Chinese characters.


@pytest.mark.slow()
def test_vsicurl_9():

    ds = gdal.Open(
        "/vsicurl/http://download.osgeo.org/gdal/data/gtiff/"
        "xx\u4E2D\u6587.\u4E2D\u6587"
    )
    assert ds is not None


###############################################################################
# Test reading a file with escaped Chinese characters.


@pytest.mark.slow()
def test_vsicurl_10():

    ds = gdal.Open(
        "/vsicurl/http://download.osgeo.org/gdal/data/gtiff/xx%E4%B8%AD%E6%96%87.%E4%B8%AD%E6%96%87"
    )
    assert ds is not None


###############################################################################
# Test ReadDir() after reading a file on the same server


@pytest.mark.slow()
def test_vsicurl_11():

    f = gdal.VSIFOpenL(
        "/vsicurl/http://download.osgeo.org/gdal/data/bmp/Bug2236.bmp", "rb"
    )
    if f is None:
        pytest.skip()
    gdal.VSIFSeekL(f, 1000000, 0)
    gdal.VSIFReadL(1, 1, f)
    gdal.VSIFCloseL(f)

    filelist = gdal.ReadDir("/vsicurl/http://download.osgeo.org/gdal/data/gtiff")
    assert filelist is not None and filelist


###############################################################################


def test_vsicurl_start_webserver():

    gdaltest.webserver_process = None
    gdaltest.webserver_port = 0

    (gdaltest.webserver_process, gdaltest.webserver_port) = webserver.launch(
        handler=webserver.DispatcherHttpHandler
    )
    if gdaltest.webserver_port == 0:
        pytest.skip()


###############################################################################
# Test redirection with Expires= type of signed URLs


def test_vsicurl_test_redirect():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add("GET", "/test_redirect/", 404)
    # Simulate a big time difference between server and local machine
    current_time = 1500

    def method(request):
        response = "HTTP/1.1 302\r\n"
        response += "Server: foo\r\n"
        response += (
            "Date: "
            + time.strftime("%a, %d %b %Y %H:%M:%S GMT", time.gmtime(current_time))
            + "\r\n"
        )
        response += "Location: %s\r\n" % (
            "http://localhost:%d/foo.s3.amazonaws.com/test_redirected/test.bin?Signature=foo&Expires=%d"
            % (gdaltest.webserver_port, current_time + 30)
        )
        response += "\r\n"
        request.wfile.write(response.encode("ascii"))

    handler.add("HEAD", "/test_redirect/test.bin", custom_method=method)
    handler.add(
        "HEAD",
        "/foo.s3.amazonaws.com/test_redirected/test.bin?Signature=foo&Expires=%d"
        % (current_time + 30),
        403,
        {"Server": "foo"},
        "",
    )

    def method(request):
        if "Range" in request.headers:
            if request.headers["Range"] == "bytes=0-16383":
                request.protocol_version = "HTTP/1.1"
                request.send_response(200)
                request.send_header("Content-type", "text/plain")
                request.send_header("Content-Range", "bytes 0-16383/1000000")
                request.send_header("Content-Length", 16384)
                request.send_header("Connection", "close")
                request.end_headers()
                request.wfile.write(("x" * 16384).encode("ascii"))
            elif request.headers["Range"] == "bytes=16384-49151":
                # Test expiration of the signed URL
                request.protocol_version = "HTTP/1.1"
                request.send_response(403)
                request.send_header("Content-Length", 0)
                request.end_headers()
            else:
                request.send_response(404)
                request.send_header("Content-Length", 0)
                request.end_headers()
        else:
            # After a failed attempt on a HEAD, the client should go there
            response = "HTTP/1.1 200\r\n"
            response += "Server: foo\r\n"
            response += (
                "Date: "
                + time.strftime("%a, %d %b %Y %H:%M:%S GMT", time.gmtime(current_time))
                + "\r\n"
            )
            response += "Content-type: text/plain\r\n"
            response += "Content-Length: 1000000\r\n"
            response += "Connection: close\r\n"
            response += "\r\n"
            request.wfile.write(response.encode("ascii"))

    handler.add(
        "GET",
        "/foo.s3.amazonaws.com/test_redirected/test.bin?Signature=foo&Expires=%d"
        % (current_time + 30),
        custom_method=method,
    )

    with webserver.install_http_handler(handler):
        f = gdal.VSIFOpenL(
            "/vsicurl/http://localhost:%d/test_redirect/test.bin"
            % gdaltest.webserver_port,
            "rb",
        )
    assert f is not None

    gdal.VSIFSeekL(f, 0, 2)
    try:
        assert gdal.VSIFTellL(f) == 1000000
    except Exception:
        gdal.VSIFCloseL(f)
        raise
    gdal.VSIFSeekL(f, 0, 0)

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/foo.s3.amazonaws.com/test_redirected/test.bin?Signature=foo&Expires=%d"
        % (current_time + 30),
        custom_method=method,
    )
    handler.add(
        "GET",
        "/foo.s3.amazonaws.com/test_redirected/test.bin?Signature=foo&Expires=%d"
        % (current_time + 30),
        custom_method=method,
    )

    current_time = int(time.time())

    def method(request):
        # We should go there after expiration of the first signed URL
        if (
            "Range" in request.headers
            and request.headers["Range"] == "bytes=16384-49151"
        ):
            request.protocol_version = "HTTP/1.1"
            request.send_response(302)
            # Return a new signed URL
            request.send_header(
                "Location",
                "http://localhost:%d/foo.s3.amazonaws.com/test_redirected2/test.bin?Signature=foo&Expires=%d"
                % (request.server.port, current_time + 30),
            )
            request.send_header("Content-Length", 16384)
            request.end_headers()
            request.wfile.write(("x" * 16384).encode("ascii"))

    handler.add("GET", "/test_redirect/test.bin", custom_method=method)

    def method(request):
        # Second signed URL
        if (
            "Range" in request.headers
            and request.headers["Range"] == "bytes=16384-49151"
        ):
            request.protocol_version = "HTTP/1.1"
            request.send_response(200)
            request.send_header("Content-type", "text/plain")
            request.send_header("Content-Range", "bytes 16384-16384/1000000")
            request.send_header("Content-Length", 1)
            request.end_headers()
            request.wfile.write("y".encode("ascii"))

    handler.add(
        "GET",
        "/foo.s3.amazonaws.com/test_redirected2/test.bin?Signature=foo&Expires=%d"
        % (current_time + 30),
        custom_method=method,
    )

    with webserver.install_http_handler(handler):
        try:
            content = gdal.VSIFReadL(1, 16383, f).decode("ascii")
            assert len(content) == 16383
            assert content[0] == "x"
            content = gdal.VSIFReadL(1, 2, f).decode("ascii")
            assert content == "xy"
        finally:
            gdal.VSIFCloseL(f)


###############################################################################
# Test redirection with X-Amz-Expires= + X-Amz-Date= type of signed URLs


def test_vsicurl_test_redirect_x_amz():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add("GET", "/test_redirect/", 404)
    # Simulate a big time difference between server and local machine
    current_time = 1500

    def method(request):
        response = "HTTP/1.1 302\r\n"
        response += "Server: foo\r\n"
        response += (
            "Date: "
            + time.strftime("%a, %d %b %Y %H:%M:%S GMT", time.gmtime(current_time))
            + "\r\n"
        )
        response += "Location: %s\r\n" % (
            "http://localhost:%d/foo.s3.amazonaws.com/test_redirected/test.bin?X-Amz-Signature=foo&X-Amz-Expires=30&X-Amz-Date=%s"
            % (
                gdaltest.webserver_port,
                time.strftime("%Y%m%dT%H%M%SZ", time.gmtime(current_time)),
            )
        )
        response += "\r\n"
        request.wfile.write(response.encode("ascii"))

    handler.add("HEAD", "/test_redirect/test.bin", custom_method=method)
    handler.add(
        "HEAD",
        "/foo.s3.amazonaws.com/test_redirected/test.bin?X-Amz-Signature=foo&X-Amz-Expires=30&X-Amz-Date=%s"
        % time.strftime("%Y%m%dT%H%M%SZ", time.gmtime(current_time)),
        403,
        {"Server": "foo"},
        "",
    )

    def method(request):
        if "Range" in request.headers:
            if request.headers["Range"] == "bytes=0-16383":
                request.protocol_version = "HTTP/1.1"
                request.send_response(200)
                request.send_header("Content-type", "text/plain")
                request.send_header("Content-Range", "bytes 0-16383/1000000")
                request.send_header("Content-Length", 16384)
                request.send_header("Connection", "close")
                request.end_headers()
                request.wfile.write(("x" * 16384).encode("ascii"))
            elif request.headers["Range"] == "bytes=16384-49151":
                # Test expiration of the signed URL
                request.protocol_version = "HTTP/1.1"
                request.send_response(403)
                request.send_header("Content-Length", 0)
                request.end_headers()
            else:
                request.send_response(404)
                request.send_header("Content-Length", 0)
                request.end_headers()
        else:
            # After a failed attempt on a HEAD, the client should go there
            response = "HTTP/1.1 200\r\n"
            response += "Server: foo\r\n"
            response += (
                "Date: "
                + time.strftime("%a, %d %b %Y %H:%M:%S GMT", time.gmtime(current_time))
                + "\r\n"
            )
            response += "Content-type: text/plain\r\n"
            response += "Content-Length: 1000000\r\n"
            response += "Connection: close\r\n"
            response += "\r\n"
            request.wfile.write(response.encode("ascii"))

    handler.add(
        "GET",
        "/foo.s3.amazonaws.com/test_redirected/test.bin?X-Amz-Signature=foo&X-Amz-Expires=30&X-Amz-Date=%s"
        % time.strftime("%Y%m%dT%H%M%SZ", time.gmtime(current_time)),
        custom_method=method,
    )

    with webserver.install_http_handler(handler):
        f = gdal.VSIFOpenL(
            "/vsicurl/http://localhost:%d/test_redirect/test.bin"
            % gdaltest.webserver_port,
            "rb",
        )
    assert f is not None

    gdal.VSIFSeekL(f, 0, 2)
    try:
        assert gdal.VSIFTellL(f) == 1000000
    except Exception:
        gdal.VSIFCloseL(f)
        raise
    gdal.VSIFSeekL(f, 0, 0)

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/foo.s3.amazonaws.com/test_redirected/test.bin?X-Amz-Signature=foo&X-Amz-Expires=30&X-Amz-Date=%s"
        % time.strftime("%Y%m%dT%H%M%SZ", time.gmtime(current_time)),
        custom_method=method,
    )
    handler.add(
        "GET",
        "/foo.s3.amazonaws.com/test_redirected/test.bin?X-Amz-Signature=foo&X-Amz-Expires=30&X-Amz-Date=%s"
        % time.strftime("%Y%m%dT%H%M%SZ", time.gmtime(current_time)),
        custom_method=method,
    )

    current_time = int(time.time())

    def method(request):
        # We should go there after expiration of the first signed URL
        if (
            "Range" in request.headers
            and request.headers["Range"] == "bytes=16384-49151"
        ):
            request.protocol_version = "HTTP/1.1"
            request.send_response(302)
            # Return a new signed URL
            request.send_header(
                "Location",
                "http://localhost:%d/foo.s3.amazonaws.com/test_redirected2/test.bin?X-Amz-Signature=foo&X-Amz-Expires=30&X-Amz-Date=%s"
                % (
                    request.server.port,
                    time.strftime("%Y%m%dT%H%M%SZ", time.gmtime(current_time)),
                ),
            )
            request.send_header("Content-Length", 16384)
            request.end_headers()
            request.wfile.write(("x" * 16384).encode("ascii"))

    handler.add("GET", "/test_redirect/test.bin", custom_method=method)

    def method(request):
        # Second signed URL
        if (
            "Range" in request.headers
            and request.headers["Range"] == "bytes=16384-49151"
        ):
            request.protocol_version = "HTTP/1.1"
            request.send_response(200)
            request.send_header("Content-type", "text/plain")
            request.send_header("Content-Range", "bytes 16384-16384/1000000")
            request.send_header("Content-Length", 1)
            request.end_headers()
            request.wfile.write("y".encode("ascii"))

    handler.add(
        "GET",
        "/foo.s3.amazonaws.com/test_redirected2/test.bin?X-Amz-Signature=foo&X-Amz-Expires=30&X-Amz-Date=%s"
        % time.strftime("%Y%m%dT%H%M%SZ", time.gmtime(current_time)),
        custom_method=method,
    )

    with webserver.install_http_handler(handler):
        try:
            content = gdal.VSIFReadL(1, 16383, f).decode("ascii")
            assert len(content) == 16383
            assert content[0] == "x"
            content = gdal.VSIFReadL(1, 2, f).decode("ascii")
            assert content == "xy"
        finally:
            gdal.VSIFCloseL(f)


###############################################################################
# TODO: better testing


def test_vsicurl_test_clear_cache():

    gdal.VSICurlClearCache()
    gdal.VSICurlClearCache()


###############################################################################


def test_vsicurl_test_retry():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    handler = webserver.SequentialHandler()
    handler.add("GET", "/test_retry/", 404)
    handler.add("HEAD", "/test_retry/test.txt", 200, {"Content-Length": "3"})
    handler.add("GET", "/test_retry/test.txt", 502)
    with webserver.install_http_handler(handler):
        f = gdal.VSIFOpenL(
            "/vsicurl/http://localhost:%d/test_retry/test.txt"
            % gdaltest.webserver_port,
            "rb",
        )
        data_len = 0
        if f:
            data_len = len(gdal.VSIFReadL(1, 1, f))
            gdal.VSIFCloseL(f)
        assert data_len == 0

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add("GET", "/test_retry/", 404)
    handler.add("HEAD", "/test_retry/test.txt", 200, {"Content-Length": "3"})
    handler.add("GET", "/test_retry/test.txt", 502)
    handler.add("GET", "/test_retry/test.txt", 429)
    handler.add("GET", "/test_retry/test.txt", 200, {}, "foo")
    with webserver.install_http_handler(handler):
        f = gdal.VSIFOpenL(
            "/vsicurl?max_retry=2&retry_delay=0.01&url=http://localhost:%d/test_retry/test.txt"
            % gdaltest.webserver_port,
            "rb",
        )
        assert f is not None
        gdal.ErrorReset()
        with gdal.quiet_errors():
            data = gdal.VSIFReadL(1, 3, f).decode("ascii")
        error_msg = gdal.GetLastErrorMsg()
        gdal.VSIFCloseL(f)
        assert data == "foo"
        assert "429" in error_msg


###############################################################################


def test_vsicurl_test_fallback_from_head_to_get():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add("HEAD", "/test_fallback_from_head_to_get", 405)
    handler.add("GET", "/test_fallback_from_head_to_get", 200, {}, "foo")
    with webserver.install_http_handler(handler):
        statres = gdal.VSIStatL(
            "/vsicurl/http://localhost:%d/test_fallback_from_head_to_get"
            % gdaltest.webserver_port
        )
    assert statres.size == 3

    gdal.VSICurlClearCache()


###############################################################################


def test_vsicurl_test_parse_html_filelist_apache():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/mydir/",
        200,
        {},
        """<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 3.2 Final//EN">
<html>
 <head>
  <title>Index of /mydir</title>
 </head>
 <body>
<h1>Index of /mydir</h1>
<table><tr><th><img src="/icons/blank.gif" alt="[ICO]"></th><th><a href="?C=N;O=D">Name</a></th><th><a href="?C=M;O=A">Last modified</a></th><th><a href="?C=S;O=A">Size</a></th><th><a href="?C=D;O=A">Description</a></th></tr><tr><th colspan="5"><hr></th></tr>
<tr><td valign="top"><img src="/icons/back.gif" alt="[DIR]"></td><td><a href="/gdal/data/">Parent Directory</a></td><td>&nbsp;</td><td align="right">  - </td><td>&nbsp;</td></tr>
<tr><td valign="top"><img src="/icons/image2.gif" alt="[IMG]"></td><td><a href="foo.tif">foo.tif</a></td><td align="right">17-May-2010 12:26  </td><td align="right"> 90K</td><td>&nbsp;</td></tr>
<tr><td valign="top"><img src="/icons/image2.gif" alt="[IMG]"></td><td><a href="foo%20with%20space.tif">foo with space.tif</a></td><td align="right">15-Jan-2007 11:02  </td><td align="right">736 </td><td>&nbsp;</td></tr>
<tr><th colspan="5"><hr></th></tr>
</table>
</body></html>""",
    )
    with webserver.install_http_handler(handler):
        fl = gdal.ReadDir(
            "/vsicurl/http://localhost:%d/mydir" % gdaltest.webserver_port
        )
    assert fl == ["foo.tif", "foo%20with%20space.tif"]

    assert (
        gdal.VSIStatL(
            "/vsicurl/http://localhost:%d/mydir/foo%%20with%%20space.tif"
            % gdaltest.webserver_port,
            gdal.VSI_STAT_EXISTS_FLAG,
        )
        is not None
    )

    handler = webserver.SequentialHandler()
    handler.add("HEAD", "/mydir/i_dont_exist", 404, {})
    with webserver.install_http_handler(handler):
        assert (
            gdal.VSIStatL(
                "/vsicurl/http://localhost:%d/mydir/i_dont_exist"
                % gdaltest.webserver_port,
                gdal.VSI_STAT_EXISTS_FLAG,
            )
            is None
        )


###############################################################################


def test_vsicurl_no_size_in_HEAD():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    handler = webserver.SequentialHandler()
    handler.add(
        "HEAD",
        "/test_vsicurl_no_size_in_HEAD.bin",
        200,
        {},
        add_content_length_header=False,
    )
    handler.add("GET", "/test_vsicurl_no_size_in_HEAD.bin", 200, {}, "X" * 10)
    with webserver.install_http_handler(handler):
        statres = gdal.VSIStatL(
            "/vsicurl/http://localhost:%d/test_vsicurl_no_size_in_HEAD.bin"
            % gdaltest.webserver_port
        )
    assert statres.size == 10


###############################################################################


def test_vsicurl_test_CPL_CURL_VERBOSE():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    class MyHandler:
        def __init__(self):
            self.found_CURL_INFO = False
            self.found_CURL_INFO_HEADER_IN = False
            self.found_CURL_INFO_HEADER_OUT = False

        def handler(self, err_type, err_no, err_msg):
            if "CURL_INFO_TEXT:" in err_msg:
                self.found_CURL_INFO_TEXT = True
            if "CURL_INFO_HEADER_IN:" in err_msg:
                self.found_CURL_INFO_HEADER_IN = True
            if "CURL_INFO_HEADER_OUT:" in err_msg:
                self.found_CURL_INFO_HEADER_OUT = True

    handler = webserver.SequentialHandler()
    handler.add(
        "HEAD", "/test_vsicurl_test_CPL_CURL_VERBOSE", 200, {"Content-Length": "3"}
    )
    my_error_handler = MyHandler()
    with gdaltest.config_options({"CPL_CURL_VERBOSE": "YES", "CPL_DEBUG": "ON"}):
        with gdaltest.error_handler(my_error_handler.handler):
            with webserver.install_http_handler(handler):
                statres = gdal.VSIStatL(
                    "/vsicurl/http://localhost:%d/test_vsicurl_test_CPL_CURL_VERBOSE"
                    % gdaltest.webserver_port
                )
    assert statres.size == 3

    assert my_error_handler.found_CURL_INFO_TEXT
    assert my_error_handler.found_CURL_INFO_HEADER_IN
    assert my_error_handler.found_CURL_INFO_HEADER_OUT

    gdal.VSICurlClearCache()


###############################################################################


def test_vsicurl_planetary_computer_url_signing():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/pc_sas_sign_href?href=http://localhost:%d/test_vsicurl_planetary_computer_url_signing.bin"
        % gdaltest.webserver_port,
        200,
        {},
        '{"msft:expiry":"1970-01-01T00:00:00","href":"http://localhost:%d/test_vsicurl_planetary_computer_url_signing.bin?my_token"}'
        % gdaltest.webserver_port,
    )
    handler.add(
        "HEAD",
        "/test_vsicurl_planetary_computer_url_signing.bin?my_token",
        200,
        {"Content-Length": "3"},
    )

    with webserver.install_http_handler(handler):
        with gdaltest.config_option(
            "VSICURL_PC_SAS_SIGN_HREF_URL",
            "http://localhost:%d/pc_sas_sign_href?href=" % gdaltest.webserver_port,
        ):
            statres = gdal.VSIStatL(
                "/vsicurl?pc_url_signing=yes&url=http://localhost:%d/test_vsicurl_planetary_computer_url_signing.bin"
                % gdaltest.webserver_port
            )
            assert statres.size == 3

    # Check that signing request is done since it has expired
    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/pc_sas_sign_href?href=http://localhost:%d/test_vsicurl_planetary_computer_url_signing.bin"
        % gdaltest.webserver_port,
        200,
        {},
        '{"msft:expiry":"9999-01-01T00:00:00","href":"http://localhost:%d/test_vsicurl_planetary_computer_url_signing.bin?my_token"}'
        % gdaltest.webserver_port,
    )
    handler.add(
        "HEAD",
        "/test_vsicurl_planetary_computer_url_signing.bin?my_token",
        200,
        {"Content-Length": "3"},
    )

    with webserver.install_http_handler(handler):
        with gdaltest.config_option(
            "VSICURL_PC_SAS_SIGN_HREF_URL",
            "http://localhost:%d/pc_sas_sign_href?href=" % gdaltest.webserver_port,
        ):
            statres = gdal.VSIStatL(
                "/vsicurl?pc_url_signing=yes&url=http://localhost:%d/test_vsicurl_planetary_computer_url_signing.bin"
                % gdaltest.webserver_port
            )
            assert statres.size == 3

    # Check that signing request is not needed
    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add(
        "HEAD",
        "/test_vsicurl_planetary_computer_url_signing.bin?my_token",
        200,
        {"Content-Length": "3"},
    )

    with webserver.install_http_handler(handler):
        with gdaltest.config_option(
            "VSICURL_PC_SAS_SIGN_HREF_URL",
            "http://localhost:%d/pc_sas_sign_href?href=" % gdaltest.webserver_port,
        ):
            statres = gdal.VSIStatL(
                "/vsicurl?pc_url_signing=yes&url=http://localhost:%d/test_vsicurl_planetary_computer_url_signing.bin"
                % gdaltest.webserver_port
            )
            assert statres.size == 3

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/pc_sas_sign_href?href=http://localhost:%d/test_vsicurl_planetary_computer_url_signing2.bin"
        % gdaltest.webserver_port,
        200,
        {},
        '{"msft:expiry":"9999-01-01T00:00:00","href":"http://localhost:%d/test_vsicurl_planetary_computer_url_signing2.bin?my_token2"}'
        % gdaltest.webserver_port,
    )
    handler.add(
        "HEAD",
        "/test_vsicurl_planetary_computer_url_signing2.bin?my_token2",
        200,
        {"Content-Length": "4"},
    )

    with webserver.install_http_handler(handler):
        with gdaltest.config_option(
            "VSICURL_PC_SAS_SIGN_HREF_URL",
            "http://localhost:%d/pc_sas_sign_href?href=" % gdaltest.webserver_port,
        ):
            statres = gdal.VSIStatL(
                "/vsicurl?pc_url_signing=yes&url=http://localhost:%d/test_vsicurl_planetary_computer_url_signing2.bin"
                % gdaltest.webserver_port
            )
            assert statres.size == 4

    # Check that signing of multiple URL is cached
    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()

    handler.add(
        "HEAD",
        "/test_vsicurl_planetary_computer_url_signing.bin?my_token",
        200,
        {"Content-Length": "3"},
    )

    handler.add(
        "HEAD",
        "/test_vsicurl_planetary_computer_url_signing2.bin?my_token2",
        200,
        {"Content-Length": "4"},
    )

    with webserver.install_http_handler(handler):
        with gdaltest.config_option(
            "VSICURL_PC_SAS_SIGN_HREF_URL",
            "http://localhost:%d/pc_sas_sign_href?href=" % gdaltest.webserver_port,
        ):
            statres = gdal.VSIStatL(
                "/vsicurl?pc_url_signing=yes&url=http://localhost:%d/test_vsicurl_planetary_computer_url_signing.bin"
                % gdaltest.webserver_port
            )
            assert statres.size == 3

            statres = gdal.VSIStatL(
                "/vsicurl?pc_url_signing=yes&url=http://localhost:%d/test_vsicurl_planetary_computer_url_signing2.bin"
                % gdaltest.webserver_port
            )
            assert statres.size == 4


###############################################################################


def test_vsicurl_planetary_computer_url_signing_collection():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/pc_sas_token/my_collection",
        200,
        {},
        '{"msft:expiry":"1970-01-01T00:00:00","token":"my_token"}',
    )
    handler.add(
        "HEAD",
        "/test_vsicurl_planetary_computer_url_signing.bin?my_token",
        200,
        {"Content-Length": "3"},
    )
    handler.add(
        "GET",
        "/pc_sas_token/my_collection",
        200,
        {},
        '{"msft:expiry":"9999-01-01T00:00:00","token":"my_token2"}',
    )
    handler.add(
        "HEAD",
        "/test_vsicurl_planetary_computer_url_signing2.bin?my_token2",
        200,
        {"Content-Length": "4"},
    )
    handler.add(
        "HEAD",
        "/test_vsicurl_planetary_computer_url_signing3.bin?my_token2",
        200,
        {"Content-Length": "5"},
    )
    handler.add(
        "GET",
        "/pc_sas_token/my_collection2",
        200,
        {},
        '{"msft:expiry":"9999-01-01T00:00:00","token":"my_token3"}',
    )
    handler.add(
        "HEAD",
        "/test_vsicurl_planetary_computer_url_signing4.bin?my_token3",
        200,
        {"Content-Length": "6"},
    )
    handler.add(
        "HEAD",
        "/test_vsicurl_planetary_computer_url_signing5.bin?my_token2",
        200,
        {"Content-Length": "7"},
    )

    with webserver.install_http_handler(handler):
        with gdaltest.config_option(
            "VSICURL_PC_SAS_TOKEN_URL",
            "http://localhost:%d/pc_sas_token/" % gdaltest.webserver_port,
        ):
            statres = gdal.VSIStatL(
                "/vsicurl?pc_url_signing=yes&pc_collection=my_collection&url=http://localhost:%d/test_vsicurl_planetary_computer_url_signing.bin"
                % gdaltest.webserver_port
            )
            assert statres.size == 3
            statres = gdal.VSIStatL(
                "/vsicurl?pc_url_signing=yes&pc_collection=my_collection&url=http://localhost:%d/test_vsicurl_planetary_computer_url_signing2.bin"
                % gdaltest.webserver_port
            )
            assert statres.size == 4
            statres = gdal.VSIStatL(
                "/vsicurl?pc_url_signing=yes&pc_collection=my_collection&url=http://localhost:%d/test_vsicurl_planetary_computer_url_signing3.bin"
                % gdaltest.webserver_port
            )
            assert statres.size == 5
            statres = gdal.VSIStatL(
                "/vsicurl?pc_url_signing=yes&pc_collection=my_collection2&url=http://localhost:%d/test_vsicurl_planetary_computer_url_signing4.bin"
                % gdaltest.webserver_port
            )
            assert statres.size == 6
            statres = gdal.VSIStatL(
                "/vsicurl?pc_url_signing=yes&pc_collection=my_collection&url=http://localhost:%d/test_vsicurl_planetary_computer_url_signing5.bin"
                % gdaltest.webserver_port
            )
            assert statres.size == 7


###############################################################################


def test_vsicurl_GDAL_HTTP_HEADERS():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add(
        "HEAD",
        "/test_vsicurl_GDAL_HTTP_HEADERS.bin",
        200,
        {"Content-Length": "3"},
        expected_headers={
            "Foo": "Bar",
            "Baz": r'escaped backslash \, escaped double-quote ", end of value',
            "Another": "Header",
        },
    )

    filename = (
        "/vsicurl/http://localhost:%d/test_vsicurl_GDAL_HTTP_HEADERS.bin"
        % gdaltest.webserver_port
    )
    gdal.SetPathSpecificOption(
        filename,
        "GDAL_HTTP_HEADERS",
        r'Foo: Bar,"Baz: escaped backslash \\, escaped double-quote \", end of value",Another: Header',
    )
    with webserver.install_http_handler(handler):
        statres = gdal.VSIStatL(filename)
    gdal.SetPathSpecificOption(filename, "GDAL_HTTP_HEADERS", None)
    assert statres.size == 3


###############################################################################
# Test CPL_VSIL_CURL_USE_HEAD=NO


def test_vsicurl_test_CPL_VSIL_CURL_USE_HEAD_NO():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add("GET", "/test_CPL_VSIL_CURL_USE_HEAD_NO/", 404)

    def method(request):
        response = "HTTP/1.1 200\r\n"
        response += "Server: foo\r\n"
        response += "Content-type: text/plain\r\n"
        response += "Content-Length: 1000000\r\n"
        response += "Connection: close\r\n"
        response += "\r\n"
        request.wfile.write(response.encode("ascii"))
        # This will be interrupted by the client
        for i in range(1000000):
            request.wfile.write(b"X")

    handler.add(
        "GET",
        "/test_CPL_VSIL_CURL_USE_HEAD_NO/test.bin",
        custom_method=method,
        silence_server_exception=True,
    )

    with webserver.install_http_handler(handler):
        with gdaltest.config_option("CPL_VSIL_CURL_USE_HEAD", "NO"):
            f = gdal.VSIFOpenL(
                "/vsicurl/http://localhost:%d/test_CPL_VSIL_CURL_USE_HEAD_NO/test.bin"
                % gdaltest.webserver_port,
                "rb",
            )
    assert f is not None
    gdal.VSIFSeekL(f, 0, 2)
    assert gdal.VSIFTellL(f) == 1000000

    gdal.VSIFCloseL(f)


###############################################################################


def test_vsicurl_stop_webserver():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    # Clearcache needed to close all connections, since the Python server
    # can only handle one connection at a time
    gdal.VSICurlClearCache()

    webserver.server_stop(gdaltest.webserver_process, gdaltest.webserver_port)


###############################################################################
# Check that GDAL_HTTP_NETRC_FILE is taken into account


@pytest.mark.skipif(sys.platform != "linux", reason="Incorrect platform")
@pytest.mark.skipif(not gdaltest.built_against_curl(), reason="curl not available")
def test_vsicurl_NETRC_FILE():

    python_exe = sys.executable
    cmd = (
        f'strace -f "{python_exe}" -c "'
        + "from osgeo import gdal; "
        + "gdal.SetConfigOption('GDAL_HTTP_NETRC_FILE', '/i_do/not_exist'); "
        + "gdal.Open('/vsicurl/http://i.do.not.exist.com/foo');"
        + '"'
    )
    try:
        (_, err) = gdaltest.runexternal_out_and_err(cmd, encoding="UTF-8")
    except Exception as e:
        pytest.skip("got exception %s" % str(e))

    assert "/i_do/not_exist" in err
