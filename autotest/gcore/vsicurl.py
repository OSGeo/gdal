#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test /vsicurl
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2011, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import sys
import time

import gdaltest
import pytest
import webserver

from osgeo import gdal, ogr


def curl_version():
    actual_version = [0, 0, 0]
    for build_info_item in gdal.VersionInfo("BUILD_INFO").strip().split("\n"):
        if build_info_item.startswith("CURL_VERSION="):
            curl_version = build_info_item[len("CURL_VERSION=") :]
            # Remove potential -rcX postfix.
            dashrc_pos = curl_version.find("-rc")
            if dashrc_pos > 0:
                curl_version = curl_version[0:dashrc_pos]
            actual_version = [int(x) for x in curl_version.split(".")]
    return actual_version


pytestmark = pytest.mark.require_curl()

###############################################################################
#


@pytest.mark.slow()
@pytest.mark.skip("File is no longer available")
def test_vsicurl_1():

    ds = ogr.Open(
        "/vsizip/vsicurl/http://publicfiles.dep.state.fl.us/dear/BWR_GIS/2007NWFLULC/NWFWMD2007LULC.zip"
    )
    assert ds is not None


###############################################################################
#


@pytest.mark.slow()
@pytest.mark.skip("File is no longer available")
def test_vsicurl_2():

    ds = gdal.Open(
        "/vsizip//vsicurl/http://eros.usgs.gov/archive/nslrsda/GeoTowns/HongKong/srtm/n22e113.zip/n22e113.bil"
    )
    assert ds is not None


###############################################################################
# This server doesn't support range downloading


@pytest.mark.slow()
@pytest.mark.skip("File is no longer available")
def test_vsicurl_3():

    ds = ogr.Open(
        "/vsizip/vsicurl/http://www.iucnredlist.org/spatial-data/MAMMALS_TERRESTRIAL.zip"
    )
    assert ds is None


###############################################################################
# This server doesn't support range downloading


@pytest.mark.slow()
@gdaltest.disable_exceptions()
@pytest.mark.network
def test_vsicurl_4():

    ds = ogr.Open(
        "/vsizip/vsicurl/http://lelserver.env.duke.edu:8080/LandscapeTools/export/49/Downloads/1_Habitats.zip"
    )
    assert ds is None


###############################################################################
# Test URL unescaping when reading HTTP file list


@pytest.mark.slow()
@pytest.mark.skip("File is no longer available")
def test_vsicurl_5():

    ds = gdal.Open(
        "/vsicurl/http://dds.cr.usgs.gov/srtm/SRTM_image_sample/picture%20examples/N34W119_DEM.tif"
    )
    assert ds is not None


###############################################################################
# Test with FTP server that doesn't support EPSV command


@pytest.mark.slow()
@pytest.mark.skip("Server is no longer available")
def test_vsicurl_6():

    fl = gdal.ReadDir("/vsicurl/ftp://ftp2.cits.rncan.gc.ca/pub/cantopo/250k_tif")
    assert fl


###############################################################################
# Test Microsoft-IIS/6.0 listing


@pytest.mark.slow()
@pytest.mark.skip("Server is no longer available")
def test_vsicurl_7():

    fl = gdal.ReadDir("/vsicurl/http://ortho.linz.govt.nz/tifs/2005_06")
    assert fl


###############################################################################
# Test interleaved reading between 2 datasets


@pytest.mark.slow()
@pytest.mark.skip("File is no longer available")
def test_vsicurl_8():

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
@pytest.mark.network
def test_vsicurl_9():

    ds = gdal.Open(
        "/vsicurl/http://download.osgeo.org/gdal/data/gtiff/"
        "xx\u4e2d\u6587.\u4e2d\u6587"
    )
    assert ds is not None


###############################################################################
# Test reading a file with escaped Chinese characters.


@pytest.mark.slow()
@pytest.mark.network
def test_vsicurl_10():

    ds = gdal.Open(
        "/vsicurl/http://download.osgeo.org/gdal/data/gtiff/xx%E4%B8%AD%E6%96%87.%E4%B8%AD%E6%96%87"
    )
    assert ds is not None


###############################################################################
# Test ReadDir() after reading a file on the same server


@pytest.mark.slow()
@pytest.mark.network
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


@pytest.fixture(scope="module")
def server():

    process, port = webserver.launch(handler=webserver.DispatcherHttpHandler)
    if port == 0:
        pytest.skip()

    import collections

    WebServer = collections.namedtuple("WebServer", "process port")

    yield WebServer(process, port)

    # Clearcache needed to close all connections, since the Python server
    # can only handle one connection at a time
    gdal.VSICurlClearCache()

    webserver.server_stop(process, port)


###############################################################################
# Test regular redirection


@pytest.mark.parametrize(
    "authorization_header_allowed", [None, "YES", "NO", "IF_SAME_HOST"]
)
def test_vsicurl_test_redirect(server, authorization_header_allowed):

    gdal.VSICurlClearCache()

    expected_headers = None
    unexpected_headers = []
    if authorization_header_allowed != "NO":
        expected_headers = {"Authorization": "Bearer xxx"}
    else:
        unexpected_headers = ["Authorization"]

    handler = webserver.SequentialHandler()
    handler.add("GET", "/test_redirect/", 404)
    handler.add(
        "HEAD",
        "/test_redirect/test.bin",
        301,
        {"Location": "http://localhost:%d/redirected/test.bin" % server.port},
        expected_headers={"Authorization": "Bearer xxx"},
    )

    # Curl always forward Authorization if same server when handling itself
    # the redirect, so this means that CPL_VSIL_CURL_AUTHORIZATION_HEADER_ALLOWED_IF_REDIRECT=NO
    # is not honored for that particular request. To honour it, we would have
    # to disable CURLOPT_FOLLOWLOCATION and implement it at hand
    handler.add(
        "HEAD",
        "/redirected/test.bin",
        200,
        {"Content-Length": "3"},
        expected_headers={"Authorization": "Bearer xxx"},
    )

    handler.add(
        "GET",
        "/redirected/test.bin",
        200,
        {"Content-Length": "3"},
        b"xyz",
        expected_headers=expected_headers,
        unexpected_headers=unexpected_headers,
    )

    options = {"GDAL_HTTP_HEADERS": "Authorization: Bearer xxx"}
    if authorization_header_allowed:
        options["CPL_VSIL_CURL_AUTHORIZATION_HEADER_ALLOWED_IF_REDIRECT"] = (
            authorization_header_allowed
        )
    with webserver.install_http_handler(handler), gdal.config_options(options):
        f = gdal.VSIFOpenL(
            "/vsicurl/http://localhost:%d/test_redirect/test.bin" % server.port,
            "rb",
        )
        assert f is not None
        try:
            assert gdal.VSIFReadL(1, 3, f) == b"xyz"
        finally:
            gdal.VSIFCloseL(f)


###############################################################################
# Test regular redirection


@pytest.mark.parametrize(
    "authorization_header_allowed", [None, "YES", "NO", "IF_SAME_HOST"]
)
@pytest.mark.parametrize("redirect_code", [301, 302])
def test_vsicurl_test_redirect_different_server(
    server, authorization_header_allowed, redirect_code
):

    gdal.VSICurlClearCache()

    expected_headers = None
    unexpected_headers = []
    if authorization_header_allowed == "YES":
        expected_headers = {"Authorization": "Bearer xxx"}
    else:
        unexpected_headers = ["Authorization"]

    handler = webserver.SequentialHandler()
    handler.add("GET", "/test_redirect/", 404)
    handler.add(
        "HEAD",
        "/test_redirect/test.bin",
        redirect_code,
        {"Location": "http://127.0.0.1:%d/redirected/test.bin" % server.port},
        expected_headers={"Authorization": "Bearer xxx"},
    )
    if redirect_code == 302 and authorization_header_allowed is None:
        handler.add(
            "HEAD",
            "/redirected/test.bin",
            403,
            expected_headers=expected_headers,
            unexpected_headers=unexpected_headers,
        )
        handler.add(
            "GET",
            "/redirected/test.bin",
            200,
            {"Content-Length": "3"},
            b"xyz",
        )
    else:
        handler.add(
            "HEAD",
            "/redirected/test.bin",
            200,
            {"Content-Length": "3"},
            expected_headers=expected_headers,
            unexpected_headers=unexpected_headers,
        )
    if redirect_code == 302:
        handler.add(
            "GET",
            "/test_redirect/test.bin",
            redirect_code,
            {"Location": "http://127.0.0.1:%d/redirected/test.bin" % server.port},
            expected_headers={"Authorization": "Bearer xxx"},
        )
    handler.add(
        "GET",
        "/redirected/test.bin",
        200,
        {"Content-Length": "3"},
        b"xyz",
        expected_headers=expected_headers,
        unexpected_headers=unexpected_headers,
    )

    options = {"GDAL_HTTP_HEADERS": "Authorization: Bearer xxx"}
    if authorization_header_allowed:
        options["CPL_VSIL_CURL_AUTHORIZATION_HEADER_ALLOWED_IF_REDIRECT"] = (
            authorization_header_allowed
        )
    with webserver.install_http_handler(handler), gdal.config_options(options):
        f = gdal.VSIFOpenL(
            "/vsicurl/http://localhost:%d/test_redirect/test.bin" % server.port,
            "rb",
        )
        try:
            assert gdal.VSIFReadL(1, 3, f) == b"xyz"
        finally:
            gdal.VSIFCloseL(f)


###############################################################################
# Test regular redirection


@gdaltest.enable_exceptions()
@pytest.mark.require_curl(7, 61, 0)
@pytest.mark.parametrize(
    "authorization_header_allowed", [None, "YES", "NO", "IF_SAME_HOST"]
)
def test_vsicurl_test_redirect_different_server_with_bearer(
    server, authorization_header_allowed
):

    gdal.VSICurlClearCache()

    expected_headers = None
    unexpected_headers = []
    if authorization_header_allowed == "YES":
        expected_headers = {"Authorization": "Bearer xxx"}
    else:
        unexpected_headers = ["Authorization"]

    handler = webserver.SequentialHandler()
    handler.add("GET", "/test_redirect/", 404)
    handler.add(
        "HEAD",
        "/test_redirect/test.bin",
        301,
        {"Location": "http://127.0.0.1:%d/redirected/test.bin" % server.port},
        expected_headers={"Authorization": "Bearer xxx"},
    )
    handler.add(
        "HEAD",
        "/redirected/test.bin",
        200,
        {"Content-Length": "3"},
        expected_headers=expected_headers,
        unexpected_headers=unexpected_headers,
    )
    handler.add(
        "GET",
        "/redirected/test.bin",
        200,
        {"Content-Length": "3"},
        b"xyz",
        expected_headers=expected_headers,
        unexpected_headers=unexpected_headers,
    )

    options = {"GDAL_HTTP_AUTH": "BEARER", "GDAL_HTTP_BEARER": "xxx"}
    if authorization_header_allowed:
        options["CPL_VSIL_CURL_AUTHORIZATION_HEADER_ALLOWED_IF_REDIRECT"] = (
            authorization_header_allowed
        )
    with webserver.install_http_handler(handler), gdal.config_options(options):
        f = gdal.VSIFOpenL(
            "/vsicurl/http://localhost:%d/test_redirect/test.bin" % server.port,
            "rb",
        )
        try:
            assert gdal.VSIFReadL(1, 3, f) == b"xyz"
        finally:
            gdal.VSIFCloseL(f)


###############################################################################
# Test redirection with Expires= type of signed URLs


def test_vsicurl_test_redirect_with_expires(server):

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add("GET", "/test_redirect/", 404)
    # Simulate a big time difference between server and local machine
    current_time = 1500

    def method(request):
        response = "HTTP/1.1 302 Found\r\n"
        response += "Server: foo\r\n"
        response += (
            "Date: "
            + time.strftime("%a, %d %b %Y %H:%M:%S GMT", time.gmtime(current_time))
            + "\r\n"
        )
        response += "Location: %s\r\n" % (
            "http://localhost:%d/foo.s3.amazonaws.com/test_redirected/test.bin?Signature=foo&Expires=%d"
            % (server.port, current_time + 30)
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
            response = "HTTP/1.1 200 OK\r\n"
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
            "/vsicurl/http://localhost:%d/test_redirect/test.bin" % server.port,
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


def test_vsicurl_test_redirect_x_amz(server):

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add("GET", "/test_redirect/", 404)
    # Simulate a big time difference between server and local machine
    current_time = 1500

    def method(request):

        assert request.headers["Authorization"] == "Bearer xxx"

        response = "HTTP/1.1 302 Found\r\n"
        response += "Server: foo\r\n"
        response += (
            "Date: "
            + time.strftime("%a, %d %b %Y %H:%M:%S GMT", time.gmtime(current_time))
            + "\r\n"
        )
        response += "Location: %s\r\n" % (
            "http://127.0.0.1:%d/foo.s3.amazonaws.com/test_redirected/test.bin?X-Amz-Signature=foo&X-Amz-Expires=30&X-Amz-Date=%s"
            % (
                server.port,
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
        unexpected_headers=["Authorization"],
    )

    def method(request):

        assert "Authorization" not in request.headers

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
            response = "HTTP/1.1 200 OK\r\n"
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

    gdal.SetPathSpecificOption(
        "/vsicurl/http://localhost:%d/test_redirect" % server.port,
        "GDAL_HTTP_AUTH",
        "BEARER",
    )
    gdal.SetPathSpecificOption(
        "/vsicurl/http://localhost:%d/test_redirect" % server.port,
        "GDAL_HTTP_BEARER",
        "xxx",
    )
    try:
        with webserver.install_http_handler(handler):
            f = gdal.VSIFOpenL(
                "/vsicurl/http://localhost:%d/test_redirect/test.bin" % server.port,
                "rb",
            )
    finally:
        gdal.ClearPathSpecificOptions(
            "/vsicurl/http://localhost:%d/test_redirect" % server.port
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


def test_vsicurl_test_retry(server):

    handler = webserver.SequentialHandler()
    handler.add("GET", "/test_retry/", 404)
    handler.add("HEAD", "/test_retry/test.txt", 200, {"Content-Length": "3"})
    handler.add("GET", "/test_retry/test.txt", 502)
    with webserver.install_http_handler(handler):
        f = gdal.VSIFOpenL(
            "/vsicurl/http://localhost:%d/test_retry/test.txt" % server.port,
            "rb",
        )
        data_len = 0
        if f:
            try:
                data_len = len(gdal.VSIFReadL(1, 1, f))
                assert data_len == 0
                assert gdal.VSIFEofL(f) == 0
                assert gdal.VSIFErrorL(f) == 1
                gdal.VSIFClearErrL(f)
                assert gdal.VSIFEofL(f) == 0
                assert gdal.VSIFErrorL(f) == 0
            finally:
                gdal.VSIFCloseL(f)

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
            % server.port,
            "rb",
        )
        assert f is not None
        try:
            gdal.ErrorReset()
            with gdal.quiet_errors():
                data = gdal.VSIFReadL(1, 3, f).decode("ascii")
                assert data == "foo"
            error_msg = gdal.GetLastErrorMsg()
            assert "429" in error_msg
            assert gdal.VSIFEofL(f) == 0
            assert gdal.VSIFErrorL(f) == 0
        finally:
            gdal.VSIFCloseL(f)


###############################################################################


def test_vsicurl_retry_codes_ALL(server):

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add("GET", "/test_retry/", 404)
    handler.add("HEAD", "/test_retry/test.txt", 200, {"Content-Length": "3"})
    handler.add(
        "GET", "/test_retry/test.txt", 400
    )  #  non retriable by default, but here allowed because of retry_codes=ALL
    handler.add("GET", "/test_retry/test.txt", 200, {}, "foo")
    with webserver.install_http_handler(handler):
        f = gdal.VSIFOpenL(
            "/vsicurl?max_retry=1&retry_delay=0.01&retry_codes=ALL&url=http://localhost:%d/test_retry/test.txt"
            % server.port,
            "rb",
        )
        assert f is not None
        gdal.ErrorReset()
        with gdal.quiet_errors():
            data = gdal.VSIFReadL(1, 3, f).decode("ascii")
        error_msg = gdal.GetLastErrorMsg()
        gdal.VSIFCloseL(f)
        assert data == "foo"
        assert "400" in error_msg


###############################################################################


def test_vsicurl_retry_codes_enumerated(server):

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add("GET", "/test_retry/", 404)
    handler.add("HEAD", "/test_retry/test.txt", 200, {"Content-Length": "3"})
    handler.add(
        "GET", "/test_retry/test.txt", 400
    )  #  non retriable by default, but here allowed because of retry_codes=ALL
    handler.add("GET", "/test_retry/test.txt", 200, {}, "foo")
    with webserver.install_http_handler(handler), gdal.config_option(
        "GDAL_HTTP_RETRY_CODES", "400"
    ):
        f = gdal.VSIFOpenL(
            "/vsicurl?max_retry=1&retry_delay=0.01&url=http://localhost:%d/test_retry/test.txt"
            % server.port,
            "rb",
        )
        assert f is not None
        gdal.ErrorReset()
        with gdal.quiet_errors():
            data = gdal.VSIFReadL(1, 3, f).decode("ascii")
        error_msg = gdal.GetLastErrorMsg()
        gdal.VSIFCloseL(f)
        assert data == "foo"
        assert "400" in error_msg


###############################################################################


def test_vsicurl_retry_codes_no_match(server):

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add("GET", "/test_retry/", 404)
    handler.add("HEAD", "/test_retry/test.txt", 200, {"Content-Length": "3"})
    handler.add(
        "GET", "/test_retry/test.txt", 400
    )  #  non retriable by default, and not listed in GDAL_HTTP_RETRY_CODES
    with webserver.install_http_handler(handler), gdal.config_option(
        "GDAL_HTTP_RETRY_CODES", "409"
    ):
        f = gdal.VSIFOpenL(
            "/vsicurl?max_retry=1&retry_delay=0.01&url=http://localhost:%d/test_retry/test.txt"
            % server.port,
            "rb",
        )
        assert f is not None
        gdal.ErrorReset()
        with gdal.quiet_errors():
            data = gdal.VSIFReadL(1, 3, f).decode("ascii")
        gdal.VSIFCloseL(f)
        assert len(data) == 0


###############################################################################
# Test that ReadMultiRange retries on 429


def test_vsicurl_readmultirange_retry(server):

    gdal.VSICurlClearCache()

    filesize = 262976

    def serve_range(request):
        if "Range" not in request.headers:
            request.send_response(404)
            request.end_headers()
            return
        rng = request.headers["Range"][len("bytes=") :]
        start = int(rng.split("-")[0])
        end = int(rng.split("-")[1])
        request.protocol_version = "HTTP/1.1"
        request.send_response(206)
        request.send_header("Content-type", "application/octet-stream")
        request.send_header("Content-Range", "bytes %d-%d/%d" % (start, end, filesize))
        request.send_header("Content-Length", end - start + 1)
        request.send_header("Connection", "close")
        request.end_headers()
        with open("../gdrivers/data/utm.tif", "rb") as f:
            f.seek(start, 0)
            request.wfile.write(f.read(end - start + 1))

    def serve_429(request):
        request.protocol_version = "HTTP/1.1"
        request.send_response(429)
        request.send_header("Connection", "close")
        request.end_headers()

    handler = webserver.SequentialHandler()
    handler.add(
        "HEAD",
        "/readmultirange_retry.tif",
        200,
        {"Content-Length": "%d" % filesize},
    )
    # GETs 1-3: header/IFD reads + first tile strip (all succeed)
    for i in range(3):
        handler.add("GET", "/readmultirange_retry.tif", custom_method=serve_range)
    # GET 4: second tile strip -> 429
    handler.add("GET", "/readmultirange_retry.tif", custom_method=serve_429)
    # GETs 5-6: remaining tile strips succeed
    for i in range(2):
        handler.add("GET", "/readmultirange_retry.tif", custom_method=serve_range)
    # GET 7: retry of the failed tile strip
    handler.add("GET", "/readmultirange_retry.tif", custom_method=serve_range)

    with webserver.install_http_handler(handler):
        with gdaltest.config_options(
            {
                "GTIFF_DIRECT_IO": "YES",
                "CPL_VSIL_CURL_ALLOWED_EXTENSIONS": ".tif",
                "GDAL_DISABLE_READDIR_ON_OPEN": "EMPTY_DIR",
                "GDAL_HTTP_MAX_RETRY": "2",
                "GDAL_HTTP_RETRY_DELAY": "0.01",
            }
        ):
            ds = gdal.Open(
                "/vsicurl/http://localhost:%d/readmultirange_retry.tif" % server.port
            )
            assert ds is not None
            subsampled_data = ds.ReadRaster(0, 0, 512, 32, 128, 4)
            ds = None
            assert subsampled_data is not None
            ds = gdal.GetDriverByName("MEM").Create("", 128, 4)
            ds.WriteRaster(0, 0, 128, 4, subsampled_data)
            cs = ds.GetRasterBand(1).Checksum()
            ds = None
            assert cs == 6429


###############################################################################


def test_vsicurl_test_fallback_from_head_to_get(server):

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add("HEAD", "/test_fallback_from_head_to_get", 405)
    handler.add("GET", "/test_fallback_from_head_to_get", 200, {}, "foo")
    with webserver.install_http_handler(handler):
        statres = gdal.VSIStatL(
            "/vsicurl/http://localhost:%d/test_fallback_from_head_to_get" % server.port
        )
    assert statres.size == 3

    gdal.VSICurlClearCache()


###############################################################################


def test_vsicurl_test_parse_html_filelist_apache(server):

    gdal.VSICurlClearCache()

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
        fl = gdal.ReadDir("/vsicurl/http://localhost:%d/mydir" % server.port)
    assert fl == ["foo.tif", "foo%20with%20space.tif"]

    assert (
        gdal.VSIStatL(
            "/vsicurl/http://localhost:%d/mydir/foo%%20with%%20space.tif" % server.port,
            gdal.VSI_STAT_EXISTS_FLAG,
        )
        is not None
    )

    handler = webserver.SequentialHandler()
    handler.add("HEAD", "/mydir/i_dont_exist", 404, {})
    with webserver.install_http_handler(handler):
        assert (
            gdal.VSIStatL(
                "/vsicurl/http://localhost:%d/mydir/i_dont_exist" % server.port,
                gdal.VSI_STAT_EXISTS_FLAG,
            )
            is None
        )


###############################################################################


def test_vsicurl_test_parse_html_filelist_nginx_cdn(server):

    gdal.VSICurlClearCache()

    # Format of https://cdn.star.nesdis.noaa.gov/GOES18/ABI/MESO/M1/GEOCOLOR/

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/mydir/",
        200,
        {},
        """<html>
<head><title>Index of /ma-cdn02/mydir/</title></head>
<body>
<h1>Index of /ma-cdn02/mydir/</h1><hr><pre><a href="../">../</a>
<a href="1000x1000.jpg">1000x1000.jpg</a>                                      28-Oct-2025 18:48              665983
<a href="2000x2000.jpg">2000x2000.jpg</a>                                      28-Oct-2025 18:48             2013236
</pre><hr></body>
</html>

""",
    )
    with webserver.install_http_handler(handler):
        fl = gdal.ReadDir("/vsicurl/http://localhost:%d/mydir" % server.port)
    assert fl == ["1000x1000.jpg", "2000x2000.jpg"]

    stat = gdal.VSIStatL(
        "/vsicurl/http://localhost:%d/mydir/1000x1000.jpg" % server.port
    )
    assert stat
    assert stat.size == 665983


###############################################################################


def test_vsicurl_no_size_in_HEAD(server):

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
            % server.port
        )
    assert statres.size == 10


###############################################################################


def test_vsicurl_test_CPL_CURL_VERBOSE(server):

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
                    % server.port
                )
    assert statres.size == 3

    assert my_error_handler.found_CURL_INFO_TEXT
    assert my_error_handler.found_CURL_INFO_HEADER_IN
    assert my_error_handler.found_CURL_INFO_HEADER_OUT

    gdal.VSICurlClearCache()


###############################################################################


def test_vsicurl_planetary_computer_url_signing(server):

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/pc_sas_sign_href?href=http://localhost:%d/test_vsicurl_planetary_computer_url_signing.bin"
        % server.port,
        200,
        {},
        '{"msft:expiry":"1970-01-01T00:00:00","href":"http://localhost:%d/test_vsicurl_planetary_computer_url_signing.bin?my_token"}'
        % server.port,
    )
    handler.add(
        "HEAD",
        "/test_vsicurl_planetary_computer_url_signing.bin?my_token",
        200,
        {"Content-Length": "3"},
    )

    with webserver.install_http_handler(handler):
        try:
            gdal.SetPathSpecificOption(
                "/vsicurl/http://localhost:%d/test_vsicurl_planetary_computer_url_signing"
                % server.port,
                "VSICURL_PC_URL_SIGNING",
                "YES",
            )

            with gdaltest.config_option(
                "VSICURL_PC_SAS_SIGN_HREF_URL",
                "http://localhost:%d/pc_sas_sign_href?href=" % server.port,
            ):
                statres = gdal.VSIStatL(
                    "/vsicurl/http://localhost:%d/test_vsicurl_planetary_computer_url_signing.bin"
                    % server.port
                )
                assert statres.size == 3
        finally:
            gdal.SetPathSpecificOption(
                "/vsicurl/http://localhost:%d/test_vsicurl_planetary_computer_url_signing"
                % server.port,
                "VSICURL_PC_URL_SIGNING",
                None,
            )

    # Check that signing request is done since it has expired
    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/pc_sas_sign_href?href=http://localhost:%d/test_vsicurl_planetary_computer_url_signing.bin"
        % server.port,
        200,
        {},
        '{"msft:expiry":"9999-01-01T00:00:00","href":"http://localhost:%d/test_vsicurl_planetary_computer_url_signing.bin?my_token2"}'
        % server.port,
    )
    handler.add(
        "HEAD",
        "/test_vsicurl_planetary_computer_url_signing.bin?my_token2",
        200,
        {"Content-Length": "4"},
    )

    with webserver.install_http_handler(handler):
        with gdaltest.config_option(
            "VSICURL_PC_SAS_SIGN_HREF_URL",
            "http://localhost:%d/pc_sas_sign_href?href=" % server.port,
        ):
            statres = gdal.VSIStatL(
                "/vsicurl?pc_url_signing=yes&url=http://localhost:%d/test_vsicurl_planetary_computer_url_signing.bin"
                % server.port
            )
            assert statres.size == 4

    # Check that signing request is not needed
    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add(
        "HEAD",
        "/test_vsicurl_planetary_computer_url_signing.bin?my_token2",
        200,
        {"Content-Length": "3"},
    )

    with webserver.install_http_handler(handler):
        with gdaltest.config_option(
            "VSICURL_PC_SAS_SIGN_HREF_URL",
            "http://localhost:%d/pc_sas_sign_href?href=" % server.port,
        ):
            statres = gdal.VSIStatL(
                "/vsicurl?pc_url_signing=yes&url=http://localhost:%d/test_vsicurl_planetary_computer_url_signing.bin"
                % server.port
            )
            assert statres.size == 3

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/pc_sas_sign_href?href=http://localhost:%d/test_vsicurl_planetary_computer_url_signing2.bin"
        % server.port,
        200,
        {},
        '{"msft:expiry":"9999-01-01T00:00:00","href":"http://localhost:%d/test_vsicurl_planetary_computer_url_signing2.bin?my_token3"}'
        % server.port,
    )
    handler.add(
        "HEAD",
        "/test_vsicurl_planetary_computer_url_signing2.bin?my_token3",
        200,
        {"Content-Length": "4"},
    )

    with webserver.install_http_handler(handler):
        with gdaltest.config_option(
            "VSICURL_PC_SAS_SIGN_HREF_URL",
            "http://localhost:%d/pc_sas_sign_href?href=" % server.port,
        ):
            statres = gdal.VSIStatL(
                "/vsicurl?pc_url_signing=yes&url=http://localhost:%d/test_vsicurl_planetary_computer_url_signing2.bin"
                % server.port
            )
            assert statres.size == 4

    # Check that signing of multiple URL is cached
    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()

    handler.add(
        "HEAD",
        "/test_vsicurl_planetary_computer_url_signing.bin?my_token2",
        200,
        {"Content-Length": "3"},
    )

    handler.add(
        "HEAD",
        "/test_vsicurl_planetary_computer_url_signing2.bin?my_token3",
        200,
        {"Content-Length": "4"},
    )

    with webserver.install_http_handler(handler):
        with gdaltest.config_option(
            "VSICURL_PC_SAS_SIGN_HREF_URL",
            "http://localhost:%d/pc_sas_sign_href?href=" % server.port,
        ):
            statres = gdal.VSIStatL(
                "/vsicurl?pc_url_signing=yes&url=http://localhost:%d/test_vsicurl_planetary_computer_url_signing.bin"
                % server.port
            )
            assert statres.size == 3

            statres = gdal.VSIStatL(
                "/vsicurl?pc_url_signing=yes&url=http://localhost:%d/test_vsicurl_planetary_computer_url_signing2.bin"
                % server.port
            )
            assert statres.size == 4


###############################################################################


def test_vsicurl_planetary_computer_url_signing_collection(server):

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
            "http://localhost:%d/pc_sas_token/" % server.port,
        ):
            statres = gdal.VSIStatL(
                "/vsicurl?pc_url_signing=yes&pc_collection=my_collection&url=http://localhost:%d/test_vsicurl_planetary_computer_url_signing.bin"
                % server.port
            )
            assert statres.size == 3
            statres = gdal.VSIStatL(
                "/vsicurl?pc_url_signing=yes&pc_collection=my_collection&url=http://localhost:%d/test_vsicurl_planetary_computer_url_signing2.bin"
                % server.port
            )
            assert statres.size == 4
            statres = gdal.VSIStatL(
                "/vsicurl?pc_url_signing=yes&pc_collection=my_collection&url=http://localhost:%d/test_vsicurl_planetary_computer_url_signing3.bin"
                % server.port
            )
            assert statres.size == 5
            statres = gdal.VSIStatL(
                "/vsicurl?pc_url_signing=yes&pc_collection=my_collection2&url=http://localhost:%d/test_vsicurl_planetary_computer_url_signing4.bin"
                % server.port
            )
            assert statres.size == 6
            statres = gdal.VSIStatL(
                "/vsicurl?pc_url_signing=yes&pc_collection=my_collection&url=http://localhost:%d/test_vsicurl_planetary_computer_url_signing5.bin"
                % server.port
            )
            assert statres.size == 7


###############################################################################


def test_vsicurl_GDAL_HTTP_HEADERS(server):

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
        "/vsicurl/http://localhost:%d/test_vsicurl_GDAL_HTTP_HEADERS.bin" % server.port
    )
    try:
        gdal.SetPathSpecificOption(
            filename,
            "GDAL_HTTP_HEADERS",
            r'Foo: Bar,"Baz: escaped backslash \\, escaped double-quote \", end of value",Another: Header',
        )
        with webserver.install_http_handler(handler):
            statres = gdal.VSIStatL(filename)
        assert statres.size == 3

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
        with webserver.install_http_handler(handler):
            statres = gdal.VSIStatL(
                "/vsicurl_streaming/http://localhost:%d/test_vsicurl_GDAL_HTTP_HEADERS.bin"
                % server.port
            )
        assert statres.size == 3

    finally:
        gdal.SetPathSpecificOption(filename, "GDAL_HTTP_HEADERS", None)


###############################################################################
# Test CPL_VSIL_CURL_USE_HEAD=NO


# Cf fix of https://github.com/curl/curl/pull/14390
@pytest.mark.skipif(
    curl_version() == [8, 9, 1],
    reason="fail with SIGPIPE with curl 8.9.1",
)
def test_vsicurl_test_CPL_VSIL_CURL_USE_HEAD_NO(server):

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add("GET", "/test_CPL_VSIL_CURL_USE_HEAD_NO/", 404)

    def method(request):
        response = "HTTP/1.1 200 OK\r\n"
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
                % server.port,
                "rb",
            )
    assert f is not None
    gdal.VSIFSeekL(f, 0, 2)
    assert gdal.VSIFTellL(f) == 1000000

    gdal.VSIFCloseL(f)


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
        _, err = gdaltest.runexternal_out_and_err(cmd, encoding="UTF-8")
    except Exception as e:
        pytest.skip("got exception %s" % str(e))

    assert "/i_do/not_exist" in err


###############################################################################
# Check auth with bearer token


@gdaltest.enable_exceptions()
@pytest.mark.require_curl(7, 61, 0)
def test_vsicurl_bearer(server):

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add("GET", "/", 404)
    handler.add(
        "HEAD",
        "/test_vsicurl_bearer.bin",
        200,
        {"Content-Length": "3"},
        expected_headers={
            "Authorization": "Bearer myuniqtok",
        },
    )
    handler.add(
        "GET",
        "/test_vsicurl_bearer.bin",
        200,
        {"Content-Length": "3"},
        b"foo",
        expected_headers={
            "Authorization": "Bearer myuniqtok",
        },
    )

    token = "myuniqtok"
    with webserver.install_http_handler(handler):
        with gdal.config_options(
            {"GDAL_HTTP_AUTH": "BEARER", "GDAL_HTTP_BEARER": token}
        ):
            f = gdal.VSIFOpenL(
                "/vsicurl/http://localhost:%d/test_vsicurl_bearer.bin" % server.port,
                "rb",
            )
            assert f
            data = gdal.VSIFReadL(1, 3, f)
            gdal.VSIFCloseL(f)
            assert data == b"foo"


###############################################################################
# Test https://github.com/OSGeo/gdal/issues/8922


@gdaltest.enable_exceptions()
def test_vsicurl_404_repeated_same_resource(server):

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add("HEAD", "/does/not/exist.bin", 404)
    handler.add("GET", "/does/not/", 404)
    with webserver.install_http_handler(handler):
        with pytest.raises(Exception, match="404"):
            gdal.Open("/vsicurl/http://localhost:%d/does/not/exist.bin" % server.port)

    with pytest.raises(Exception, match="404"):
        gdal.Open("/vsicurl/http://localhost:%d/does/not/exist.bin" % server.port)


###############################################################################


@gdaltest.enable_exceptions()
def test_vsicurl_cache_control_not_set(server):

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add("GET", "/", 404)
    handler.add("HEAD", "/test.txt", 200, {"Content-Length": "3"})
    handler.add("GET", "/test.txt", 200, {}, "foo")
    with webserver.install_http_handler(handler):
        f = gdal.VSIFOpenL(
            "/vsicurl/http://localhost:%d/test.txt" % server.port,
            "rb",
        )
        assert f is not None
        data = gdal.VSIFReadL(1, 3, f).decode("ascii")
        gdal.VSIFCloseL(f)
        assert data == "foo"

    with webserver.install_http_handler(webserver.SequentialHandler()):
        f = gdal.VSIFOpenL(
            "/vsicurl/http://localhost:%d/test.txt" % server.port,
            "rb",
        )
        assert f is not None
        data = gdal.VSIFReadL(1, 3, f).decode("ascii")
        gdal.VSIFCloseL(f)
        assert data == "foo"


###############################################################################


@gdaltest.enable_exceptions()
def test_vsicurl_cache_control_no_cache(server):

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add("GET", "/", 404)
    handler.add(
        "HEAD", "/test.txt", 200, {"Content-Length": "3", "Cache-Control": "no-cache"}
    )
    handler.add(
        "GET",
        "/test.txt",
        200,
        {"Content-Length": "3", "Cache-Control": "no-cache"},
        "foo",
    )
    with webserver.install_http_handler(handler):
        f = gdal.VSIFOpenL(
            "/vsicurl/http://localhost:%d/test.txt" % server.port,
            "rb",
        )
        assert f is not None
        data = gdal.VSIFReadL(1, 3, f).decode("ascii")
        gdal.VSIFCloseL(f)
        assert data == "foo"

    handler = webserver.SequentialHandler()
    handler.add("GET", "/", 404)
    handler.add(
        "HEAD", "/test.txt", 200, {"Content-Length": "6", "Cache-Control": "no-cache"}
    )
    handler.add(
        "GET",
        "/test.txt",
        200,
        {"Content-Length": "6", "Cache-Control": "no-cache"},
        "barbaz",
    )
    with webserver.install_http_handler(handler):
        f = gdal.VSIFOpenL(
            "/vsicurl/http://localhost:%d/test.txt" % server.port,
            "rb",
        )
        assert f is not None
        data = gdal.VSIFReadL(1, 6, f).decode("ascii")
        gdal.VSIFCloseL(f)
        assert data == "barbaz"


###############################################################################
# Test VSICURL_QUERY_STRING path specific option.


@gdaltest.enable_exceptions()
@pytest.mark.parametrize(
    "filename,query_string",
    [
        ("test_vsicurl_VSICURL_QUERY_STRING.bin", "foo=bar"),
        ("test_vsicurl_VSICURL_QUERY_STRING.bin?", "foo=bar"),
        ("test_vsicurl_VSICURL_QUERY_STRING.bin", "?foo=bar"),
        ("test_vsicurl_VSICURL_QUERY_STRING.bin?", "?foo=bar"),
    ],
)
def test_vsicurl_VSICURL_QUERY_STRING(server, filename, query_string):

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add(
        "HEAD",
        "/test_vsicurl_VSICURL_QUERY_STRING.bin?foo=bar",
        200,
        {"Content-Length": "3"},
    )

    with webserver.install_http_handler(handler):
        full_filename = f"/vsicurl/http://localhost:{server.port}/{filename}"
        gdal.SetPathSpecificOption(full_filename, "VSICURL_QUERY_STRING", query_string)
        try:
            statres = gdal.VSIStatL(full_filename)
            assert statres.size == 3
        finally:
            gdal.SetPathSpecificOption(full_filename, "VSICURL_QUERY_STRING", None)


###############################################################################
# Test /vsicurl?header.foo=bar&


@gdaltest.enable_exceptions()
def test_vsicurl_header_option(server):

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add(
        "HEAD",
        "/test_vsicurl_header_option.bin",
        200,
        {"Content-Length": "3"},
        expected_headers={"foo": "bar", "Accept": "application/json"},
    )

    with webserver.install_http_handler(handler):
        full_filename = f"/vsicurl?header.foo=bar&header.Accept=application%2Fjson&url=http%3A%2F%2Flocalhost%3A{server.port}%2Ftest_vsicurl_header_option.bin"
        statres = gdal.VSIStatL(full_filename)
        assert statres.size == 3


###############################################################################
# Test GDAL_HTTP_MAX_CACHED_CONNECTIONS
# This test is rather dummy as it cannot check the effect of setting the option


@gdaltest.enable_exceptions()
def test_vsicurl_GDAL_HTTP_MAX_CACHED_CONNECTIONS(server):

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add("HEAD", "/test.bin", 200, {"Content-Length": "3"})

    with gdal.config_option(
        "GDAL_HTTP_MAX_CACHED_CONNECTIONS", "0"
    ), webserver.install_http_handler(handler):
        full_filename = f"/vsicurl/http://localhost:{server.port}/test.bin"
        statres = gdal.VSIStatL(full_filename)
        assert statres.size == 3


###############################################################################
# Test GDAL_HTTP_MAX_TOTAL_CONNECTIONS
# This test is rather dummy as it cannot check the effect of setting the option


@gdaltest.enable_exceptions()
def test_vsicurl_GDAL_HTTP_MAX_TOTAL_CONNECTIONS(server):

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add("HEAD", "/test.bin", 200, {"Content-Length": "3"})

    with gdal.config_option(
        "GDAL_HTTP_MAX_TOTAL_CONNECTIONS", "0"
    ), webserver.install_http_handler(handler):
        full_filename = f"/vsicurl/http://localhost:{server.port}/test.bin"
        statres = gdal.VSIStatL(full_filename)
        assert statres.size == 3


###############################################################################
# Test CACHE=NO file open option


@gdaltest.enable_exceptions()
def test_VSIFOpenExL_CACHE_NO(server):

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add("HEAD", "/test.bin", 200, {"Content-Length": "3"})
    handler.add("GET", "/test.bin", 200, {"Content-Length": "3"}, b"abc")
    handler.add("HEAD", "/test.bin", 200, {"Content-Length": "4"})
    handler.add("GET", "/test.bin", 200, {"Content-Length": "4"}, b"1234")

    with webserver.install_http_handler(handler):
        filename = f"/vsicurl/http://localhost:{server.port}/test.bin"

        with gdal.VSIFile(filename, "rb", False, {"CACHE": "NO"}) as f:
            assert f.read() == b"abc"

        with gdal.VSIFile(filename, "rb", False, {"CACHE": "NO"}) as f:
            assert f.read() == b"1234"


###############################################################################
# Test redirection to a URL ending with a slash, followed by a 403


def test_vsicurl_test_redirect_301_to_url_ending_slash_and_then_403(server):

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add(
        "HEAD",
        "/test_redirect",
        301,
        {"Location": "http://localhost:%d/test_redirect/" % server.port},
    )
    handler.add("HEAD", "/test_redirect/", 403)

    with webserver.install_http_handler(handler), gdal.quiet_errors():
        assert (
            gdal.VSIStatL("/vsicurl/http://localhost:%d/test_redirect" % server.port)
            is None
        )
