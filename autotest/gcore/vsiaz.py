#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test /vsiaz
# Author:   Even Rouault <even dot rouault at spatialys dot com>
#
###############################################################################
# Copyright (c) 2017 Even Rouault <even dot rouault at spatialys dot com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import copy
import os
import sys

import gdaltest
import pytest
import webserver

from osgeo import gdal

from .vsis3 import general_s3_options

pytestmark = pytest.mark.require_curl()


###############################################################################
@pytest.fixture(autouse=True, scope="module")
def module_disable_exceptions():
    with gdaltest.disable_exceptions():
        yield


def open_for_read(uri):
    """
    Opens a test file for reading.
    """
    return gdal.VSIFOpenExL(uri, "rb", 1)


###############################################################################
@pytest.fixture(autouse=True, scope="module")
def startup_and_cleanup():

    options = {}
    for var, reset_val in (
        ("AZURE_STORAGE_CONNECTION_STRING", None),
        ("AZURE_STORAGE_ACCOUNT", None),
        ("AZURE_STORAGE_ACCESS_KEY", None),
        ("AZURE_STORAGE_SAS_TOKEN", None),
        ("AZURE_NO_SIGN_REQUEST", None),
        ("AZURE_CONFIG_DIR", ""),
        ("AZURE_STORAGE_ACCESS_TOKEN", ""),
        ("AZURE_FEDERATED_TOKEN_FILE", ""),
    ):
        options[var] = reset_val

    with gdal.config_options(options, thread_local=False):
        assert gdal.GetSignedURL("/vsiaz/foo/bar") is None

        gdaltest.webserver_process = None
        gdaltest.webserver_port = 0

        gdaltest.webserver_process, gdaltest.webserver_port = webserver.launch(
            handler=webserver.DispatcherHttpHandler
        )
        if gdaltest.webserver_port == 0:
            pytest.skip()

        with gdal.config_options(
            {
                "AZURE_STORAGE_CONNECTION_STRING": "DefaultEndpointsProtocol=http;AccountName=myaccount;AccountKey=MY_ACCOUNT_KEY;BlobEndpoint=http://127.0.0.1:%d/azure/blob/myaccount"
                % gdaltest.webserver_port,
                "CPL_AZURE_TIMESTAMP": "my_timestamp",
            },
            thread_local=False,
        ):
            yield

        # Clearcache needed to close all connections, since the Python server
        # can only handle one connection at a time
        gdal.VSICurlClearCache()

        webserver.server_stop(gdaltest.webserver_process, gdaltest.webserver_port)


###############################################################################
# Test with a fake Azure Blob server


def test_vsiaz_fake_basic():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    signed_url = gdal.GetSignedURL(
        "/vsiaz/az_fake_bucket/resource", ["START_DATE=20180213T123456"]
    )
    assert (
        "azure/blob/myaccount/az_fake_bucket/resource?se=2018-02-13T13%3A34%3A56Z&sig=j0cUaaHtf2SW2usSsiN79DYx%2Fo1vWwq4lLYZSC5%2Bv7I%3D&sp=r&spr=https&sr=b&st=2018-02-13T12%3A34%3A56Z&sv=2020-12-06"
        in signed_url
    )

    def method(request):

        request.protocol_version = "HTTP/1.1"
        h = request.headers
        if (
            "Authorization" not in h
            or h["Authorization"]
            != "SharedKey myaccount:+n9wC1twBBP4T84fioDIGi9bz/CrbwRaQL0LV4sACnw="
            or "x-ms-date" not in h
            or h["x-ms-date"] != "my_timestamp"
        ):
            sys.stderr.write("Bad headers: %s\n" % str(h))
            request.send_response(403)
            return
        request.send_response(200)
        request.send_header("Content-type", "text/plain")
        request.send_header("Content-Length", 3)
        request.send_header("Connection", "close")
        request.end_headers()
        request.wfile.write("""foo""".encode("ascii"))

    handler = webserver.SequentialHandler()
    handler.add(
        "GET", "/azure/blob/myaccount/az_fake_bucket/resource", custom_method=method
    )
    with webserver.install_http_handler(handler):
        f = open_for_read("/vsiaz/az_fake_bucket/resource")
        assert f is not None
        data = gdal.VSIFReadL(1, 4, f).decode("ascii")
        gdal.VSIFCloseL(f)

    assert data == "foo"

    def method(request):

        request.protocol_version = "HTTP/1.1"
        h = request.headers
        if (
            "Authorization" not in h
            or h["Authorization"]
            != "SharedKey myaccount:EbxgYgvs7jUPNq14XrbmFBAj4eLE3ymYAHIGfMhUI9A="
            or "x-ms-date" not in h
            or h["x-ms-date"] != "my_timestamp"
            or "Accept-Encoding" not in h
            or h["Accept-Encoding"] != "gzip"
        ):
            sys.stderr.write("Bad headers: %s\n" % str(h))
            request.send_response(403)
            return
        request.send_response(200)
        request.send_header("Content-type", "text/plain")
        request.send_header("Content-Length", 3)
        request.send_header("Connection", "close")
        request.end_headers()
        request.wfile.write("""foo""".encode("ascii"))

    handler = webserver.SequentialHandler()
    handler.add(
        "GET", "/azure/blob/myaccount/az_fake_bucket/resource", custom_method=method
    )
    with webserver.install_http_handler(handler):
        f = open_for_read("/vsiaz_streaming/az_fake_bucket/resource")
        assert f is not None
        data = gdal.VSIFReadL(1, 4, f).decode("ascii")
        gdal.VSIFCloseL(f)

        assert data == "foo"

    handler = webserver.SequentialHandler()
    handler.add(
        "HEAD",
        "/azure/blob/myaccount/az_fake_bucket/resource2.bin",
        200,
        {"Content-Length": "1000000"},
    )
    with webserver.install_http_handler(handler):
        stat_res = gdal.VSIStatL("/vsiaz/az_fake_bucket/resource2.bin")
        if stat_res is None or stat_res.size != 1000000:
            if stat_res is not None:
                print(stat_res.size)
            else:
                print(stat_res)
            pytest.fail()

    handler = webserver.SequentialHandler()
    handler.add(
        "HEAD",
        "/azure/blob/myaccount/az_fake_bucket/resource2.bin",
        200,
        {"Content-Length": 1000000},
    )
    with webserver.install_http_handler(handler):
        stat_res = gdal.VSIStatL("/vsiaz_streaming/az_fake_bucket/resource2.bin")
        if stat_res is None or stat_res.size != 1000000:
            if stat_res is not None:
                print(stat_res.size)
            else:
                print(stat_res)
            pytest.fail()


###############################################################################
# Test ReadDir() with a fake Azure Blob server


def test_vsiaz_fake_readdir():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/azure/blob/myaccount/az_fake_bucket2?comp=list&delimiter=%2F&maxresults=1000&prefix=a_dir%20with_space%2F&restype=container",
        200,
        {"Content-type": "application/xml"},
        """<?xml version="1.0" encoding="UTF-8"?>
                    <EnumerationResults>
                        <Prefix>a_dir with_space/</Prefix>
                        <NextMarker>bla</NextMarker>
                        <Blobs>
                          <Blob>
                            <Name>a_dir with_space/resource3 with_space.bin</Name>
                            <Properties>
                              <Last-Modified>01 Jan 1970 00:00:01</Last-Modified>
                              <Content-Length>123456</Content-Length>
                            </Properties>
                          </Blob>
                        </Blobs>
                    </EnumerationResults>
                """,
    )
    handler.add(
        "GET",
        "/azure/blob/myaccount/az_fake_bucket2?comp=list&delimiter=%2F&marker=bla&maxresults=1000&prefix=a_dir%20with_space%2F&restype=container",
        200,
        {"Content-type": "application/xml"},
        """<?xml version="1.0" encoding="UTF-8"?>
                    <EnumerationResults>
                        <Prefix>a_dir with_space/</Prefix>
                        <Blobs>
                          <Blob>
                            <Name>a_dir with_space/resource4.bin</Name>
                            <Properties>
                              <Last-Modified>16 Oct 2016 12:34:56</Last-Modified>
                              <Content-Length>456789</Content-Length>
                            </Properties>
                          </Blob>
                          <BlobPrefix>
                            <Name>a_dir with_space/subdir/</Name>
                          </BlobPrefix>
                        </Blobs>
                    </EnumerationResults>
                """,
    )

    with webserver.install_http_handler(handler):
        f = open_for_read(
            "/vsiaz/az_fake_bucket2/a_dir with_space/resource3 with_space.bin"
        )
    if f is None:

        if gdaltest.is_travis_branch("trusty"):
            pytest.skip("Skipped on trusty branch, but should be investigated")

        pytest.fail()
    gdal.VSIFCloseL(f)

    dir_contents = gdal.ReadDir("/vsiaz/az_fake_bucket2/a_dir with_space")
    assert dir_contents == ["resource3 with_space.bin", "resource4.bin", "subdir"]
    assert (
        gdal.VSIStatL(
            "/vsiaz/az_fake_bucket2/a_dir with_space/resource3 with_space.bin"
        ).size
        == 123456
    )
    assert (
        gdal.VSIStatL(
            "/vsiaz/az_fake_bucket2/a_dir with_space/resource3 with_space.bin"
        ).mtime
        == 1
    )

    # ReadDir on something known to be a file shouldn't cause network access
    dir_contents = gdal.ReadDir(
        "/vsiaz/az_fake_bucket2/a_dir with_space/resource3 with_space.bin"
    )
    assert dir_contents is None

    # Test error on ReadDir()
    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/azure/blob/myaccount/az_fake_bucket2?comp=list&delimiter=%2F&prefix=error_test%2F&restype=container",
        500,
    )
    with webserver.install_http_handler(handler):
        dir_contents = gdal.ReadDir("/vsiaz/az_fake_bucket2/error_test/")
    assert dir_contents is None

    # List containers (empty result)
    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/azure/blob/myaccount/?comp=list",
        200,
        {"Content-type": "application/xml"},
        """<?xml version="1.0" encoding="UTF-8"?>
        <EnumerationResults ServiceEndpoint="https://myaccount.blob.core.windows.net">
            <Containers/>
            </EnumerationResults>
        """,
    )
    with webserver.install_http_handler(handler):
        dir_contents = gdal.ReadDir("/vsiaz/")
    assert dir_contents == ["."]

    gdal.VSICurlClearCache()

    # List containers
    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/azure/blob/myaccount/?comp=list",
        200,
        {"Content-type": "application/xml"},
        """<?xml version="1.0" encoding="UTF-8"?>
        <EnumerationResults>
            <Containers>
                <Container>
                    <Name>mycontainer1</Name>
                </Container>
            </Containers>
            <NextMarker>bla</NextMarker>
            </EnumerationResults>
        """,
    )
    handler.add(
        "GET",
        "/azure/blob/myaccount/?comp=list&marker=bla",
        200,
        {"Content-type": "application/xml"},
        """<?xml version="1.0" encoding="UTF-8"?>
        <EnumerationResults>
            <Containers>
                <Container>
                    <Name>mycontainer2</Name>
                </Container>
            </Containers>
        </EnumerationResults>
        """,
    )
    with webserver.install_http_handler(handler):
        dir_contents = gdal.ReadDir("/vsiaz/")
    assert dir_contents == ["mycontainer1", "mycontainer2"]

    stat = gdal.VSIStatL("/vsiaz/mycontainer1", gdal.VSI_STAT_CACHE_ONLY)
    assert stat is not None
    assert stat.mode == 16384

    stat = gdal.VSIStatL("/vsiaz/")
    assert stat is not None
    assert stat.mode == 16384


###############################################################################
# Test ReadDir() when first response has no blobs but a non-empty NextMarker


def test_vsiaz_fake_readdir_no_blobs_in_first_request():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/azure/blob/myaccount/az_fake_bucket2?comp=list&delimiter=%2F&prefix=a_dir%20with_space%2F&restype=container",
        200,
        {"Content-type": "application/xml"},
        """<?xml version="1.0" encoding="UTF-8"?>
                    <EnumerationResults>
                        <Prefix>a_dir with_space/</Prefix>
                        <Blobs/>
                        <NextMarker>bla</NextMarker>
                    </EnumerationResults>
                """,
    )
    handler.add(
        "GET",
        "/azure/blob/myaccount/az_fake_bucket2?comp=list&delimiter=%2F&marker=bla&prefix=a_dir%20with_space%2F&restype=container",
        200,
        {"Content-type": "application/xml"},
        """<?xml version="1.0" encoding="UTF-8"?>
                    <EnumerationResults>
                        <Prefix>a_dir with_space/</Prefix>
                        <Blobs>
                          <Blob>
                            <Name>a_dir with_space/resource4.bin</Name>
                            <Properties>
                              <Last-Modified>16 Oct 2016 12:34:56</Last-Modified>
                              <Content-Length>456789</Content-Length>
                            </Properties>
                          </Blob>
                          <BlobPrefix>
                            <Name>a_dir with_space/subdir/</Name>
                          </BlobPrefix>
                        </Blobs>
                    </EnumerationResults>
                """,
    )

    with webserver.install_http_handler(handler):
        dir_contents = gdal.ReadDir("/vsiaz/az_fake_bucket2/a_dir with_space")
    assert dir_contents == ["resource4.bin", "subdir"]


###############################################################################
#


@gdaltest.enable_exceptions()
def test_vsiaz_fake_readdir_protection_again_infinite_looping():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/azure/blob/myaccount/az_fake_bucket2?comp=list&delimiter=%2F&prefix=a_dir%20with_space%2F&restype=container",
        200,
        {"Content-type": "application/xml"},
        """<?xml version="1.0" encoding="UTF-8"?>
                    <EnumerationResults>
                        <Prefix>a_dir with_space/</Prefix>
                        <Blobs/>
                        <NextMarker>bla0</NextMarker>
                    </EnumerationResults>
                """,
    )
    for i in range(10):
        handler.add(
            "GET",
            f"/azure/blob/myaccount/az_fake_bucket2?comp=list&delimiter=%2F&marker=bla{i}&prefix=a_dir%20with_space%2F&restype=container",
            200,
            {"Content-type": "application/xml"},
            f"""<?xml version="1.0" encoding="UTF-8"?>
                        <EnumerationResults>
                            <Prefix>a_dir with_space/</Prefix>
                            <Blobs/>
                            <NextMarker>bla{i + 1}</NextMarker>
                        </EnumerationResults>
                    """,
        )

    with webserver.install_http_handler(handler):
        with pytest.raises(
            Exception,
            match="More than 10 consecutive List Blob requests returning no blobs",
        ):
            gdal.ReadDir("/vsiaz/az_fake_bucket2/a_dir with_space")


###############################################################################
# Test AZURE_STORAGE_SAS_TOKEN option with fake server


def test_vsiaz_sas_fake():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    with gdaltest.config_options(
        {
            "AZURE_STORAGE_ACCOUNT": "test",
            "AZURE_STORAGE_SAS_TOKEN": "sig=sas",
            "CPL_AZURE_ENDPOINT": "http://127.0.0.1:%d/azure/blob/test"
            % gdaltest.webserver_port,
            "CPL_AZURE_USE_HTTPS": "NO",
            "AZURE_STORAGE_CONNECTION_STRING": "",
        },
        thread_local=False,
    ):

        handler = webserver.SequentialHandler()

        handler.add(
            "GET",
            "/azure/blob/test/test?comp=list&delimiter=%2F&restype=container&sig=sas",
            200,
            {"Content-type": "application/xml"},
            """<?xml version="1.0" encoding="UTF-8"?>
                    <EnumerationResults>
                        <Prefix></Prefix>
                        <Blobs>
                          <Blob>
                            <Name>foo.bin</Name>
                            <Properties>
                              <Last-Modified>16 Oct 2016 12:34:56</Last-Modified>
                              <Content-Length>456789</Content-Length>
                            </Properties>
                          </Blob>
                        </Blobs>
                    </EnumerationResults>
                """,
            expected_headers={"x-ms-version": "2019-12-12"},
        )

        with webserver.install_http_handler(handler):
            assert "foo.bin" in gdal.ReadDir("/vsiaz/test")

        assert gdal.VSIStatL("/vsiaz/test/foo.bin").size == 456789


###############################################################################
# Test AZURE_NO_SIGN_REQUEST option with fake server


def test_vsiaz_AZURE_NO_SIGN_REQUEST_fake_stat_file():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    # Test that we don't emit a Authorization header in AZURE_NO_SIGN_REQUEST
    # mode, even if we have credentials
    with gdaltest.config_option("AZURE_NO_SIGN_REQUEST", "YES", thread_local=False):
        handler = webserver.SequentialHandler()
        handler.add(
            "HEAD",
            "/azure/blob/myaccount/az_fake_bucket/test_AZURE_NO_SIGN_REQUEST.bin",
            200,
            {"Content-Length": 1000000},
            unexpected_headers=["Authorization"],
        )
        with webserver.install_http_handler(handler):
            stat_res = gdal.VSIStatL(
                "/vsiaz_streaming/az_fake_bucket/test_AZURE_NO_SIGN_REQUEST.bin"
            )
            assert stat_res is not None


###############################################################################
# Test AZURE_NO_SIGN_REQUEST option with fake server


def test_vsiaz_AZURE_NO_SIGN_REQUEST_fake_readdir_bucket():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()
    with gdaltest.config_option("AZURE_NO_SIGN_REQUEST", "YES", thread_local=False):
        handler = webserver.SequentialHandler()
        handler.add(
            "GET",
            "/azure/blob/myaccount/test?comp=list&delimiter=%2F&restype=container",
            200,
            {"Content-type": "application/xml"},
            """<?xml version="1.0" encoding="UTF-8"?>
                    <EnumerationResults>
                        <Prefix></Prefix>
                        <Blobs>
                          <Blob>
                            <Name>foo.bin</Name>
                            <Properties>
                              <Last-Modified>16 Oct 2016 12:34:56</Last-Modified>
                              <Content-Length>456789</Content-Length>
                            </Properties>
                          </Blob>
                        </Blobs>
                    </EnumerationResults>
                """,
            expected_headers={"x-ms-version": "2019-12-12"},
            unexpected_headers=["Authorization"],
        )

        with webserver.install_http_handler(handler):
            assert "foo.bin" in gdal.ReadDir("/vsiaz/test")

        assert gdal.VSIStatL("/vsiaz/test/foo.bin").size == 456789


###############################################################################
# Test AZURE_NO_SIGN_REQUEST option with fake server


def test_vsiaz_AZURE_NO_SIGN_REQUEST_fake_stat_bucket_root():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()
    with gdaltest.config_option("AZURE_NO_SIGN_REQUEST", "YES", thread_local=False):
        handler = webserver.SequentialHandler()
        handler.add(
            "GET",
            "/azure/blob/myaccount/test?comp=list&delimiter=%2F&maxresults=100&restype=container",
            200,
            {"Content-type": "application/xml"},
            """<?xml version="1.0" encoding="UTF-8"?>
                    <EnumerationResults>
                        <Prefix></Prefix>
                        <Blobs>
                          <Blob>
                            <Name>foo.bin</Name>
                            <Properties>
                              <Last-Modified>16 Oct 2016 12:34:56</Last-Modified>
                              <Content-Length>456789</Content-Length>
                            </Properties>
                          </Blob>
                        </Blobs>
                    </EnumerationResults>
                """,
            expected_headers={"x-ms-version": "2019-12-12"},
            unexpected_headers=["Authorization"],
        )

        with webserver.install_http_handler(handler):
            assert gdal.VSIStatL("/vsiaz/test").mode == 16384


###############################################################################
# Test write


@pytest.mark.skipif(
    "CI" in os.environ,
    reason="Flaky",
)
def test_vsiaz_fake_write():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    # Test creation of BlockBob
    f = gdal.VSIFOpenExL(
        "/vsiaz/test_copy/file.tif",
        "wb",
        0,
        ["Content-Encoding=bar", "x-ms-client-request-id=REQUEST_ID"],
    )
    assert f is not None

    handler = webserver.SequentialHandler()

    def method(request):
        h = request.headers
        if (
            "Authorization" not in h
            or h["Authorization"]
            != "SharedKey myaccount:QQv1veT5YQPRSJz8rymEvH2VBNNXnlGnqQhRLAu+MII="
            or "Expect" not in h
            or h["Expect"] != "100-continue"
            or "Content-Length" not in h
            or h["Content-Length"] != "40000"
            or "x-ms-date" not in h
            or h["x-ms-date"] != "my_timestamp"
            or "x-ms-blob-type" not in h
            or h["x-ms-blob-type"] != "BlockBlob"
            or "Content-Type" not in h
            or h["Content-Type"] != "image/tiff"
            or "Content-Encoding" not in h
            or h["Content-Encoding"] != "bar"
            or "x-ms-client-request-id" not in h
            or h["x-ms-client-request-id"] != "REQUEST_ID"
        ):
            sys.stderr.write("Bad headers: %s\n" % str(h))
            request.send_response(403)
            return

        request.protocol_version = "HTTP/1.1"
        request.wfile.write("HTTP/1.1 100 Continue\r\n\r\n".encode("ascii"))
        content = request.rfile.read(40000).decode("ascii")
        if len(content) != 40000:
            sys.stderr.write("Bad headers: %s\n" % str(request.headers))
            request.send_response(403)
            request.send_header("Content-Length", 0)
            request.end_headers()
            return
        request.send_response(201)
        request.send_header("Content-Length", 0)
        request.end_headers()

    handler.add("PUT", "/azure/blob/myaccount/test_copy/file.tif", custom_method=method)
    with webserver.install_http_handler(handler):
        ret = gdal.VSIFWriteL("x" * 35000, 1, 35000, f)
        ret += gdal.VSIFWriteL("x" * 5000, 1, 5000, f)
        if ret != 40000:
            gdal.VSIFCloseL(f)
            pytest.fail(ret)
        gdal.VSIFCloseL(f)

    # Simulate illegal read
    f = gdal.VSIFOpenL("/vsiaz/test_copy/file.tif", "wb")
    assert f is not None
    with gdal.quiet_errors():
        ret = gdal.VSIFReadL(1, 1, f)
    assert not ret
    gdal.VSIFCloseL(f)

    # Simulate illegal seek
    f = gdal.VSIFOpenL("/vsiaz/test_copy/file.tif", "wb")
    assert f is not None
    with gdal.quiet_errors():
        ret = gdal.VSIFSeekL(f, 1, 0)
    assert ret != 0
    gdal.VSIFCloseL(f)

    # Simulate failure when putting BlockBob
    f = gdal.VSIFOpenL("/vsiaz/test_copy/file.tif", "wb")
    assert f is not None

    handler = webserver.SequentialHandler()

    def method(request):
        request.protocol_version = "HTTP/1.1"
        request.send_response(403)
        request.send_header("Content-Length", 0)
        request.end_headers()

    handler.add("PUT", "/azure/blob/myaccount/test_copy/file.tif", custom_method=method)

    if gdal.VSIFSeekL(f, 0, 0) != 0:
        gdal.VSIFCloseL(f)
        pytest.fail()

    gdal.VSIFWriteL("x" * 35000, 1, 35000, f)

    if gdal.VSIFTellL(f) != 35000:
        gdal.VSIFCloseL(f)
        pytest.fail()

    if gdal.VSIFSeekL(f, 35000, 0) != 0:
        gdal.VSIFCloseL(f)
        pytest.fail()

    if gdal.VSIFSeekL(f, 0, 1) != 0:
        gdal.VSIFCloseL(f)
        pytest.fail()
    if gdal.VSIFSeekL(f, 0, 2) != 0:
        gdal.VSIFCloseL(f)
        pytest.fail()

    if gdal.VSIFEofL(f) != 0:
        gdal.VSIFCloseL(f)
        pytest.fail()

    with webserver.install_http_handler(handler):
        with gdal.quiet_errors():
            ret = gdal.VSIFCloseL(f)
        if ret == 0:
            gdal.VSIFCloseL(f)
            pytest.fail(ret)

    # Simulate creation of BlockBob over an existing blob of incompatible type
    f = gdal.VSIFOpenL("/vsiaz/test_copy/file.tif", "wb")
    assert f is not None

    handler = webserver.SequentialHandler()
    handler.add("PUT", "/azure/blob/myaccount/test_copy/file.tif", 409)
    handler.add("DELETE", "/azure/blob/myaccount/test_copy/file.tif", 202)
    handler.add("PUT", "/azure/blob/myaccount/test_copy/file.tif", 201)
    with webserver.install_http_handler(handler):
        gdal.VSIFCloseL(f)

    # Test creation of AppendBlob
    with gdal.config_option("VSIAZ_CHUNK_SIZE_BYTES", "10", thread_local=False):
        f = gdal.VSIFOpenExL(
            "/vsiaz/test_copy/file.tif", "wb", 0, ["x-ms-client-request-id=REQUEST_ID"]
        )
    assert f is not None

    handler = webserver.SequentialHandler()

    def method(request):
        h = request.headers
        if (
            "Authorization" not in h
            or h["Authorization"]
            != "SharedKey myaccount:DCVvJjXpnSkpAbuzpZU+ZnAiIo2Jy2oh8xyrHoU3ygw="
            or "Content-Length" not in h
            or h["Content-Length"] != "0"
            or "x-ms-date" not in h
            or h["x-ms-date"] != "my_timestamp"
            or "x-ms-blob-type" not in h
            or h["x-ms-blob-type"] != "AppendBlob"
            or "x-ms-client-request-id" not in h
            or h["x-ms-client-request-id"] != "REQUEST_ID"
        ):
            sys.stderr.write("Bad headers: %s\n" % str(h))
            request.send_response(403)
            return

        request.protocol_version = "HTTP/1.1"
        request.send_response(201)
        request.send_header("Content-Length", 0)
        request.end_headers()

    handler.add("PUT", "/azure/blob/myaccount/test_copy/file.tif", custom_method=method)

    def method(request):
        h = request.headers
        if (
            "Content-Length" not in h
            or h["Content-Length"] != "10"
            or "x-ms-date" not in h
            or h["x-ms-date"] != "my_timestamp"
            or "x-ms-blob-type"
            in h  #  specifying x-ms-blob-type here does not work with Azurite
            or "x-ms-blob-condition-appendpos" not in h
            or h["x-ms-blob-condition-appendpos"] != "0"
        ):
            sys.stderr.write("Bad headers: %s\n" % str(h))
            request.send_response(403)
            return

        request.protocol_version = "HTTP/1.1"
        content = request.rfile.read(10).decode("ascii")
        if content != "0123456789":
            sys.stderr.write("Bad headers: %s\n" % str(request.headers))
            request.send_response(403)
            request.send_header("Content-Length", 0)
            request.end_headers()
            return
        request.send_response(201)
        request.send_header("Content-Length", 0)
        request.end_headers()

    handler.add(
        "PUT",
        "/azure/blob/myaccount/test_copy/file.tif?comp=appendblock",
        custom_method=method,
    )

    def method(request):
        h = request.headers
        if (
            "Content-Length" not in h
            or h["Content-Length"] != "6"
            or "x-ms-date" not in h
            or h["x-ms-date"] != "my_timestamp"
            or "x-ms-blob-type" not in h
            or h["x-ms-blob-type"] != "AppendBlob"
            or "x-ms-blob-condition-appendpos" not in h
            or h["x-ms-blob-condition-appendpos"] != "10"
        ):
            sys.stderr.write("Bad headers: %s\n" % str(h))
            request.send_response(403)
            return

        request.protocol_version = "HTTP/1.1"
        content = request.rfile.read(6).decode("ascii")
        if content != "abcdef":
            sys.stderr.write("Bad headers: %s\n" % str(request.headers))
            request.send_response(403)
            request.send_header("Content-Length", 0)
            request.end_headers()
            return
        request.send_response(201)
        request.send_header("Content-Length", 0)
        request.end_headers()

    handler.add(
        "PUT",
        "/azure/blob/myaccount/test_copy/file.tif?comp=appendblock",
        custom_method=method,
    )

    with webserver.install_http_handler(handler):
        ret = gdal.VSIFWriteL("0123456789abcdef", 1, 16, f)
        if ret != 16:
            gdal.VSIFCloseL(f)
            pytest.fail(ret)
        gdal.VSIFCloseL(f)

    # Test failed creation of AppendBlob
    with gdal.config_option("VSIAZ_CHUNK_SIZE_BYTES", "10", thread_local=False):
        f = gdal.VSIFOpenL("/vsiaz/test_copy/file.tif", "wb")
    assert f is not None

    handler = webserver.SequentialHandler()

    def method(request):
        request.protocol_version = "HTTP/1.1"
        request.send_response(403)
        request.send_header("Content-Length", 0)
        request.end_headers()

    handler.add("PUT", "/azure/blob/myaccount/test_copy/file.tif", custom_method=method)

    with webserver.install_http_handler(handler):
        with gdal.quiet_errors():
            ret = gdal.VSIFWriteL("0123456789abcdef", 1, 16, f)
        if ret != 0:
            gdal.VSIFCloseL(f)
            pytest.fail(ret)
        gdal.VSIFCloseL(f)

    # Test failed writing of a block of an AppendBlob
    with gdal.config_option("VSIAZ_CHUNK_SIZE_BYTES", "10", thread_local=False):
        f = gdal.VSIFOpenL("/vsiaz/test_copy/file.tif", "wb")
    assert f is not None

    handler = webserver.SequentialHandler()
    handler.add("PUT", "/azure/blob/myaccount/test_copy/file.tif", 201)
    handler.add("PUT", "/azure/blob/myaccount/test_copy/file.tif?comp=appendblock", 403)
    with webserver.install_http_handler(handler):
        with gdal.quiet_errors():
            ret = gdal.VSIFWriteL("0123456789abcdef", 1, 16, f)
        if ret != 0:
            gdal.VSIFCloseL(f)
            pytest.fail(ret)
        gdal.VSIFCloseL(f)


###############################################################################
# Test write with retry


def test_vsiaz_write_blockblob_retry():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    with gdaltest.config_options(
        {"GDAL_HTTP_MAX_RETRY": "2", "GDAL_HTTP_RETRY_DELAY": "0.01"},
        thread_local=False,
    ):

        # Test creation of BlockBob
        f = gdal.VSIFOpenL("/vsiaz/test_copy/file.bin", "wb")
        assert f is not None

        handler = webserver.SequentialHandler()

        def method(request):
            request.protocol_version = "HTTP/1.1"
            request.wfile.write("HTTP/1.1 100 Continue\r\n\r\n".encode("ascii"))
            content = request.rfile.read(3).decode("ascii")
            if len(content) != 3:
                sys.stderr.write("Bad headers: %s\n" % str(request.headers))
                request.send_response(403)
                request.send_header("Content-Length", 0)
                request.end_headers()
                return
            request.send_response(201)
            request.send_header("Content-Length", 0)
            request.end_headers()

        handler.add("PUT", "/azure/blob/myaccount/test_copy/file.bin", 502)
        handler.add(
            "PUT", "/azure/blob/myaccount/test_copy/file.bin", custom_method=method
        )
        with gdal.quiet_errors():
            with webserver.install_http_handler(handler):
                assert gdal.VSIFWriteL("foo", 1, 3, f) == 3
                gdal.VSIFCloseL(f)


###############################################################################
# Test write with retry


def test_vsiaz_write_appendblob_retry():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    with gdaltest.config_options(
        {
            "GDAL_HTTP_MAX_RETRY": "2",
            "GDAL_HTTP_RETRY_DELAY": "0.01",
            "VSIAZ_CHUNK_SIZE_BYTES": "10",
        },
        thread_local=False,
    ):

        f = gdal.VSIFOpenL("/vsiaz/test_copy/file.bin", "wb")
        assert f is not None

        handler = webserver.SequentialHandler()
        handler.add("PUT", "/azure/blob/myaccount/test_copy/file.bin", 502)
        handler.add("PUT", "/azure/blob/myaccount/test_copy/file.bin", 201)
        handler.add(
            "PUT", "/azure/blob/myaccount/test_copy/file.bin?comp=appendblock", 502
        )
        handler.add(
            "PUT", "/azure/blob/myaccount/test_copy/file.bin?comp=appendblock", 201
        )
        handler.add(
            "PUT", "/azure/blob/myaccount/test_copy/file.bin?comp=appendblock", 502
        )
        handler.add(
            "PUT", "/azure/blob/myaccount/test_copy/file.bin?comp=appendblock", 201
        )

        with gdal.quiet_errors():
            with webserver.install_http_handler(handler):
                assert gdal.VSIFWriteL("0123456789abcdef", 1, 16, f) == 16
                gdal.VSIFCloseL(f)


###############################################################################
# Test writing a block blob


# Often fails at the gdal.VSIFWriteL(b"x" * (1024 * 1024 - 1), 1024 * 1024 - 1, 1, f)
# which returns 0
@pytest.mark.skipif(
    "CI" in os.environ,
    reason="Flaky",
)
def test_vsiaz_write_blockblob_chunk_size_1():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    f = gdal.VSIFOpenExL(
        "/vsiaz/test_create/file.bin", "wb", False, ["BLOB_TYPE=BLOCK", "CHUNK_SIZE=1"]
    )
    assert f is not None

    assert gdal.VSIFWriteL(b"x", 1, 1, f) == 1

    handler = webserver.SequentialHandler()
    handler.add(
        "PUT",
        "/azure/blob/myaccount/test_create/file.bin?blockid=000000000001&comp=block",
        201,
        expected_headers={"Content-Length": str(1024 * 1024)},
    )

    with webserver.install_http_handler(handler):
        assert gdal.VSIFWriteL(b"x" * (1024 * 1024 - 1), 1024 * 1024 - 1, 1, f) == 1

    assert gdal.VSIFWriteL(b"x", 1, 1, f) == 1

    handler = webserver.SequentialHandler()
    handler.add(
        "PUT",
        "/azure/blob/myaccount/test_create/file.bin?blockid=000000000002&comp=block",
        201,
        expected_headers={"Content-Length": "1"},
    )

    handler.add(
        "PUT",
        "/azure/blob/myaccount/test_create/file.bin?comp=blocklist",
        201,
        expected_body=b'<?xml version="1.0" encoding="utf-8"?>\n<BlockList>\n<Latest>000000000001</Latest>\n<Latest>000000000002</Latest>\n</BlockList>\n',
    )

    with webserver.install_http_handler(handler):
        gdal.VSIFCloseL(f)


###############################################################################
# Test writing a block blob default chunk size


def test_vsiaz_write_blockblob_default_chunk_size():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    f = gdal.VSIFOpenExL(
        "/vsiaz/test_create/file.bin", "wb", False, ["BLOB_TYPE=BLOCK"]
    )
    assert f is not None

    handler = webserver.SequentialHandler()
    handler.add(
        "PUT",
        "/azure/blob/myaccount/test_create/file.bin?blockid=000000000001&comp=block",
        201,
        expected_headers={"Content-Length": str(50 * 1024 * 1024)},
    )
    handler.add(
        "PUT",
        "/azure/blob/myaccount/test_create/file.bin?blockid=000000000002&comp=block",
        201,
        expected_headers={"Content-Length": "1"},
    )

    handler.add(
        "PUT",
        "/azure/blob/myaccount/test_create/file.bin?comp=blocklist",
        201,
        expected_body=b'<?xml version="1.0" encoding="utf-8"?>\n<BlockList>\n<Latest>000000000001</Latest>\n<Latest>000000000002</Latest>\n</BlockList>\n',
    )

    with webserver.install_http_handler(handler):
        assert (
            gdal.VSIFWriteL(b"x" * (50 * 1024 * 1024 + 1), 50 * 1024 * 1024 + 1, 1, f)
            == 1
        )
        gdal.VSIFCloseL(f)


###############################################################################
# Test writing a block blob single PUT


def test_vsiaz_write_blockblob_single_put():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    f = gdal.VSIFOpenExL(
        "/vsiaz/test_create/file.bin", "wb", False, ["BLOB_TYPE=BLOCK"]
    )
    assert f is not None

    handler = webserver.SequentialHandler()
    handler.add(
        "PUT",
        "/azure/blob/myaccount/test_create/file.bin",
        201,
        expected_headers={"Content-Length": "1"},
    )

    with webserver.install_http_handler(handler):
        assert gdal.VSIFWriteL(b"x", 1, 1, f) == 1
        gdal.VSIFCloseL(f)


###############################################################################
# Test Unlink()


def test_vsiaz_fake_unlink():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    # Success
    handler = webserver.SequentialHandler()
    handler.add(
        "HEAD",
        "/azure/blob/myaccount/az_bucket_test_unlink/myfile",
        200,
        {"Content-Length": "1"},
    )
    handler.add(
        "DELETE",
        "/azure/blob/myaccount/az_bucket_test_unlink/myfile",
        202,
        {"Connection": "close"},
    )
    with webserver.install_http_handler(handler):
        ret = gdal.Unlink("/vsiaz/az_bucket_test_unlink/myfile")
    assert ret == 0

    # Failure
    handler = webserver.SequentialHandler()
    handler.add(
        "HEAD",
        "/azure/blob/myaccount/az_bucket_test_unlink/myfile",
        200,
        {"Content-Length": "1"},
    )
    handler.add(
        "DELETE",
        "/azure/blob/myaccount/az_bucket_test_unlink/myfile",
        400,
        {"Connection": "close"},
    )
    with webserver.install_http_handler(handler):
        with gdal.quiet_errors():
            ret = gdal.Unlink("/vsiaz/az_bucket_test_unlink/myfile")
    assert ret == -1


###############################################################################
# Test UnlinkBatch()


def test_vsiaz_fake_unlink_batch():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    handler = webserver.SequentialHandler()
    handler.add(
        "POST",
        "/azure/blob/myaccount/?comp=batch",
        202,
        {"content-type": "multipart/mixed; boundary=my_boundary"},
        """--my_boundary
Content-Type: application/http
Content-ID: <0>

HTTP/1.1 202 Accepted

--my_boundary
Content-Type: application/http
Content-ID: <1>

HTTP/1.1 202 Accepted

--my_boundary--
""",
        expected_body=b"--batch_ec2ce0a7-deaf-11ed-9ad8-3fabe5ecd589\r\nContent-Type: application/http\r\nContent-ID: <0>\r\nContent-Transfer-Encoding: binary\r\n\r\nDELETE /az_bucket_test_unlink/myfile HTTP/1.1\r\nx-ms-date: my_timestamp\r\nAuthorization: SharedKey myaccount:Dnfp0tNObKAYSOkqSNuMyzxeHo75tKnFv0SP74SLlGg=\r\nContent-Length: 0\r\n\r\n\r\n--batch_ec2ce0a7-deaf-11ed-9ad8-3fabe5ecd589\r\nContent-Type: application/http\r\nContent-ID: <1>\r\nContent-Transfer-Encoding: binary\r\n\r\nDELETE /az_bucket_test_unlink/myfile2 HTTP/1.1\r\nx-ms-date: my_timestamp\r\nAuthorization: SharedKey myaccount:j9rNG0PKzqhgOF45zZi2V6Gvkq12Zql3cXO8TVjizTc=\r\nContent-Length: 0\r\n\r\n\r\n--batch_ec2ce0a7-deaf-11ed-9ad8-3fabe5ecd589--\r\n",
    )

    with webserver.install_http_handler(handler):
        ret = gdal.UnlinkBatch(
            [
                "/vsiaz/az_bucket_test_unlink/myfile",
                "/vsiaz/az_bucket_test_unlink/myfile2",
            ]
        )
    assert ret


###############################################################################
# Test UnlinkBatch()


def test_vsiaz_fake_unlink_batch_max_batch_size_1():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    handler = webserver.SequentialHandler()
    handler.add(
        "POST",
        "/azure/blob/myaccount/?comp=batch",
        202,
        {"content-type": "multipart/mixed; boundary=my_boundary"},
        """--my_boundary
Content-Type: application/http
Content-ID: <0>

HTTP/1.1 202 Accepted

--my_boundary--
""",
        expected_body=b"--batch_ec2ce0a7-deaf-11ed-9ad8-3fabe5ecd589\r\nContent-Type: application/http\r\nContent-ID: <0>\r\nContent-Transfer-Encoding: binary\r\n\r\nDELETE /az_bucket_test_unlink/myfile HTTP/1.1\r\nx-ms-date: my_timestamp\r\nAuthorization: SharedKey myaccount:Dnfp0tNObKAYSOkqSNuMyzxeHo75tKnFv0SP74SLlGg=\r\nContent-Length: 0\r\n\r\n\r\n--batch_ec2ce0a7-deaf-11ed-9ad8-3fabe5ecd589--\r\n",
    )
    handler.add(
        "POST",
        "/azure/blob/myaccount/?comp=batch",
        202,
        {"content-type": "multipart/mixed; boundary=my_boundary"},
        """--my_boundary
Content-Type: application/http
Content-ID: <1>

HTTP/1.1 202 Accepted

--my_boundary--
""",
        expected_body=b"--batch_ec2ce0a7-deaf-11ed-9ad8-3fabe5ecd589\r\nContent-Type: application/http\r\nContent-ID: <1>\r\nContent-Transfer-Encoding: binary\r\n\r\nDELETE /az_bucket_test_unlink/myfile2 HTTP/1.1\r\nx-ms-date: my_timestamp\r\nAuthorization: SharedKey myaccount:j9rNG0PKzqhgOF45zZi2V6Gvkq12Zql3cXO8TVjizTc=\r\nContent-Length: 0\r\n\r\n\r\n--batch_ec2ce0a7-deaf-11ed-9ad8-3fabe5ecd589--\r\n",
    )

    with gdaltest.config_option("CPL_VSIAZ_UNLINK_BATCH_SIZE", "1"):
        with webserver.install_http_handler(handler):
            ret = gdal.UnlinkBatch(
                [
                    "/vsiaz/az_bucket_test_unlink/myfile",
                    "/vsiaz/az_bucket_test_unlink/myfile2",
                ]
            )
    assert ret


###############################################################################
# Test UnlinkBatch()


def test_vsiaz_fake_unlink_batch_max_payload_4MB():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    longfilename1 = "/az_bucket_test_unlink/myfile" + ("X" * (3000 * 1000))
    longfilename2 = "/az_bucket_test_unlink/myfile2" + ("X" * (3000 * 1000))

    handler = webserver.SequentialHandler()
    handler.add(
        "POST",
        "/azure/blob/myaccount/?comp=batch",
        202,
        {"content-type": "multipart/mixed; boundary=my_boundary"},
        """--my_boundary
Content-Type: application/http
Content-ID: <0>

HTTP/1.1 202 Accepted

--my_boundary--
""",
        expected_body=b"--batch_ec2ce0a7-deaf-11ed-9ad8-3fabe5ecd589\r\nContent-Type: application/http\r\nContent-ID: <0>\r\nContent-Transfer-Encoding: binary\r\n\r\nDELETE "
        + longfilename1.encode("UTF-8")
        + b" HTTP/1.1\r\nx-ms-date: my_timestamp\r\nAuthorization: SharedKey myaccount:/yczq3N49gssy5a0exoyTyS6FIWXXwOBLPMxgaflJBI=\r\nContent-Length: 0\r\n\r\n\r\n--batch_ec2ce0a7-deaf-11ed-9ad8-3fabe5ecd589--\r\n",
    )
    handler.add(
        "POST",
        "/azure/blob/myaccount/?comp=batch",
        202,
        {"content-type": "multipart/mixed; boundary=my_boundary"},
        """--my_boundary
Content-Type: application/http
Content-ID: <1>

HTTP/1.1 202 Accepted

--my_boundary
Content-Type: application/http
Content-ID: <2>

HTTP/1.1 202 Accepted

--my_boundary--
""",
        expected_body=b"--batch_ec2ce0a7-deaf-11ed-9ad8-3fabe5ecd589\r\nContent-Type: application/http\r\nContent-ID: <1>\r\nContent-Transfer-Encoding: binary\r\n\r\nDELETE "
        + longfilename2.encode("UTF-8")
        + b" HTTP/1.1\r\nx-ms-date: my_timestamp\r\nAuthorization: SharedKey myaccount:MSulMcLyy+3xYWT7RGIfS0pd6zjc9Gq3OV9jIR2Rh5w=\r\nContent-Length: 0\r\n\r\n\r\n--batch_ec2ce0a7-deaf-11ed-9ad8-3fabe5ecd589\r\nContent-Type: application/http\r\nContent-ID: <2>\r\nContent-Transfer-Encoding: binary\r\n\r\nDELETE /az_bucket_test_unlink/myfile HTTP/1.1\r\nx-ms-date: my_timestamp\r\nAuthorization: SharedKey myaccount:Dnfp0tNObKAYSOkqSNuMyzxeHo75tKnFv0SP74SLlGg=\r\nContent-Length: 0\r\n\r\n\r\n--batch_ec2ce0a7-deaf-11ed-9ad8-3fabe5ecd589--\r\n",
    )

    with webserver.install_http_handler(handler):
        ret = gdal.UnlinkBatch(
            [
                "/vsiaz" + longfilename1,
                "/vsiaz" + longfilename2,
                "/vsiaz/az_bucket_test_unlink/myfile",
            ]
        )
    assert ret


###############################################################################
# Test Mkdir() / Rmdir()


def test_vsiaz_fake_mkdir_rmdir():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    # Invalid name
    ret = gdal.Mkdir("/vsiaz", 0)
    assert ret != 0

    handler = webserver.SequentialHandler()
    handler.add(
        "HEAD",
        "/azure/blob/myaccount/az_bucket_test_mkdir/dir/",
        404,
        {"Connection": "close"},
    )
    handler.add(
        "GET",
        "/azure/blob/myaccount/az_bucket_test_mkdir?comp=list&delimiter=%2F&maxresults=1&prefix=dir%2F&restype=container",
        200,
        {"Connection": "close"},
    )
    handler.add(
        "PUT",
        "/azure/blob/myaccount/az_bucket_test_mkdir/dir/.gdal_marker_for_dir",
        201,
    )
    with webserver.install_http_handler(handler):
        ret = gdal.Mkdir("/vsiaz/az_bucket_test_mkdir/dir", 0)
    assert ret == 0

    # Try creating already existing directory
    handler = webserver.SequentialHandler()
    handler.add("HEAD", "/azure/blob/myaccount/az_bucket_test_mkdir/dir/", 404)
    handler.add(
        "GET",
        "/azure/blob/myaccount/az_bucket_test_mkdir?comp=list&delimiter=%2F&maxresults=1&prefix=dir%2F&restype=container",
        200,
        {"Connection": "close", "Content-type": "application/xml"},
        """<?xml version="1.0" encoding="UTF-8"?>
                    <EnumerationResults>
                        <Prefix>dir/</Prefix>
                        <Blobs>
                          <Blob>
                            <Name>dir/.gdal_marker_for_dir</Name>
                          </Blob>
                        </Blobs>
                    </EnumerationResults>
                """,
    )
    with webserver.install_http_handler(handler):
        ret = gdal.Mkdir("/vsiaz/az_bucket_test_mkdir/dir", 0)
    assert ret != 0

    # Invalid name
    ret = gdal.Rmdir("/vsiaz")
    assert ret != 0

    # Not a directory
    handler = webserver.SequentialHandler()
    handler.add("HEAD", "/azure/blob/myaccount/az_bucket_test_mkdir/it_is_a_file/", 404)
    handler.add(
        "GET",
        "/azure/blob/myaccount/az_bucket_test_mkdir?comp=list&delimiter=%2F&maxresults=1&prefix=it_is_a_file%2F&restype=container",
        200,
        {"Connection": "close", "Content-type": "application/xml"},
        """<?xml version="1.0" encoding="UTF-8"?>
                    <EnumerationResults>
                        <Prefix>az_bucket_test_mkdir/</Prefix>
                        <Blobs>
                          <Blob>
                            <Name>az_bucket_test_mkdir/it_is_a_file</Name>
                          </Blob>
                        </Blobs>
                    </EnumerationResults>
                """,
    )
    handler.add(
        "GET",
        "/azure/blob/myaccount/az_bucket_test_mkdir?comp=list&delimiter=%2F&maxresults=1&prefix=it_is_a_file%2F&restype=container",
        200,
    )
    with webserver.install_http_handler(handler):
        ret = gdal.Rmdir("/vsiaz/az_bucket_test_mkdir/it_is_a_file")
    assert ret != 0

    # Valid
    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/azure/blob/myaccount/az_bucket_test_mkdir?comp=list&delimiter=%2F&maxresults=1&prefix=dir%2F&restype=container",
        200,
        {"Connection": "close", "Content-type": "application/xml"},
        """<?xml version="1.0" encoding="UTF-8"?>
                    <EnumerationResults>
                        <Prefix>dir/</Prefix>
                        <Blobs>
                          <Blob>
                            <Name>dir/.gdal_marker_for_dir</Name>
                          </Blob>
                        </Blobs>
                    </EnumerationResults>
                """,
    )
    handler.add(
        "DELETE",
        "/azure/blob/myaccount/az_bucket_test_mkdir/dir/.gdal_marker_for_dir",
        202,
    )
    with webserver.install_http_handler(handler):
        ret = gdal.Rmdir("/vsiaz/az_bucket_test_mkdir/dir")
    assert ret == 0

    # Try deleting already deleted directory
    # --> do not consider this as an error because Azure directories are removed
    # as soon as the last object in it is removed. So when directories are created
    # without .gdal_marker_for_dir they will disappear without explicit removal
    handler = webserver.SequentialHandler()
    handler.add("HEAD", "/azure/blob/myaccount/az_bucket_test_mkdir/dir/", 404)
    handler.add(
        "GET",
        "/azure/blob/myaccount/az_bucket_test_mkdir?comp=list&delimiter=%2F&maxresults=1&prefix=dir%2F&restype=container",
        200,
    )
    with webserver.install_http_handler(handler):
        ret = gdal.Rmdir("/vsiaz/az_bucket_test_mkdir/dir")
    assert ret == 0

    # Try deleting non-empty directory
    handler = webserver.SequentialHandler()
    handler.add("HEAD", "/azure/blob/myaccount/az_bucket_test_mkdir/dir_nonempty/", 404)
    handler.add(
        "GET",
        "/azure/blob/myaccount/az_bucket_test_mkdir?comp=list&delimiter=%2F&maxresults=1&prefix=dir_nonempty%2F&restype=container",
        200,
        {"Connection": "close", "Content-type": "application/xml"},
        """<?xml version="1.0" encoding="UTF-8"?>
                    <EnumerationResults>
                        <Prefix>dir_nonempty/</Prefix>
                        <Blobs>
                          <Blob>
                            <Name>dir_nonempty/foo</Name>
                          </Blob>
                        </Blobs>
                    </EnumerationResults>
                """,
    )
    handler.add(
        "GET",
        "/azure/blob/myaccount/az_bucket_test_mkdir?comp=list&delimiter=%2F&maxresults=1&prefix=dir_nonempty%2F&restype=container",
        200,
        {"Connection": "close", "Content-type": "application/xml"},
        """<?xml version="1.0" encoding="UTF-8"?>
                    <EnumerationResults>
                        <Prefix>dir_nonempty/</Prefix>
                        <Blobs>
                          <Blob>
                            <Name>dir_nonempty/foo</Name>
                          </Blob>
                        </Blobs>
                    </EnumerationResults>
                """,
    )
    with webserver.install_http_handler(handler):
        ret = gdal.Rmdir("/vsiaz/az_bucket_test_mkdir/dir_nonempty")
    assert ret != 0


###############################################################################
# Test Mkdir() / Rmdir() on a container


def test_vsiaz_fake_mkdir_rmdir_container():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/azure/blob/myaccount/?comp=list",
        200,
        {"Content-type": "application/xml"},
        """<?xml version="1.0" encoding="UTF-8"?>
        <EnumerationResults ServiceEndpoint="https://myaccount.blob.core.windows.net">
            <Containers/>
        </EnumerationResults>
        """,
    )
    handler.add("HEAD", "/azure/blob/myaccount/new_container", 400)
    handler.add("PUT", "/azure/blob/myaccount/new_container?restype=container", 201)
    with webserver.install_http_handler(handler):
        ret = gdal.Mkdir("/vsiaz/new_container", 0o755)
    assert ret == 0

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/azure/blob/myaccount/?comp=list",
        200,
        {"Content-type": "application/xml"},
        """<?xml version="1.0" encoding="UTF-8"?>
        <EnumerationResults ServiceEndpoint="https://myaccount.blob.core.windows.net">
            <Containers><Container><Name>new_container</Name></Container></Containers>
        </EnumerationResults>
        """,
    )
    handler.add(
        "GET",
        "/azure/blob/myaccount/new_container?comp=list&delimiter=%2F&maxresults=1&restype=container",
        200,
        {"Content-type": "application/xml"},
        """"<?xml version="1.0" encoding="UTF-8"?>
        <EnumerationResults>
            <Prefix></Prefix>
            <Blobs>
            </Blobs>
        </EnumerationResults>
        """,
    )
    handler.add("DELETE", "/azure/blob/myaccount/new_container?restype=container", 202)
    with webserver.install_http_handler(handler):
        ret = gdal.Rmdir("/vsiaz/new_container")
    assert ret == 0


###############################################################################


def test_vsiaz_fake_test_BlobEndpointInConnectionString():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    # Add a trailing slash to BlobEndpoint to test bugfix for https://github.com/OSGeo/gdal/issues/9519
    with gdaltest.config_option(
        "AZURE_STORAGE_CONNECTION_STRING",
        "DefaultEndpointsProtocol=http;AccountName=myaccount;AccountKey=MY_ACCOUNT_KEY;BlobEndpoint=http://127.0.0.1:%d/myaccount/"
        % gdaltest.webserver_port,
        thread_local=False,
    ):

        signed_url = gdal.GetSignedURL("/vsiaz/az_fake_bucket/resource")
        assert (
            "http://127.0.0.1:%d/myaccount/az_fake_bucket/resource"
            % gdaltest.webserver_port
            in signed_url
        )


###############################################################################


def test_vsiaz_fake_test_SharedAccessSignatureInConnectionString():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    with gdaltest.config_option(
        "AZURE_STORAGE_CONNECTION_STRING",
        "BlobEndpoint=http://127.0.0.1:%d/myaccount;SharedAccessSignature=sp=rl&st=2022-12-06T20:41:17Z&se=2022-12-07T04:41:17Z&spr=https&sv=2021-06-08&sr=c&sig=xxxxxxxx"
        % gdaltest.webserver_port,
        thread_local=False,
    ):

        signed_url = gdal.GetSignedURL("/vsiaz/az_fake_bucket/resource")
        assert (
            signed_url
            == "http://127.0.0.1:%d/myaccount/az_fake_bucket/resource?sp=rl&st=2022-12-06T20:41:17Z&se=2022-12-07T04:41:17Z&spr=https&sv=2021-06-08&sr=c&sig=xxxxxxxx"
            % gdaltest.webserver_port
        )

        def method(request):

            request.protocol_version = "HTTP/1.1"
            h = request.headers
            if "Authorization" in h:
                sys.stderr.write("Bad headers: %s\n" % str(h))
                request.send_response(403)
                return
            request.send_response(200)
            request.send_header("Content-type", "text/plain")
            request.send_header("Content-Length", 3)
            request.send_header("Connection", "close")
            request.end_headers()
            request.wfile.write("""foo""".encode("ascii"))

        handler = webserver.SequentialHandler()
        handler.add(
            "GET",
            "/myaccount/az_fake_bucket/resource?sp=rl&st=2022-12-06T20:41:17Z&se=2022-12-07T04:41:17Z&spr=https&sv=2021-06-08&sr=c&sig=xxxxxxxx",
            custom_method=method,
        )
        with webserver.install_http_handler(handler):
            f = open_for_read("/vsiaz_streaming/az_fake_bucket/resource")
            assert f is not None
            data = gdal.VSIFReadL(1, 4, f).decode("ascii")
            gdal.VSIFCloseL(f)

            assert data == "foo"


###############################################################################
# Test rename


def test_vsiaz_fake_rename():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()
    handler = webserver.SequentialHandler()
    handler.add(
        "HEAD", "/azure/blob/myaccount/test/source.txt", 200, {"Content-Length": "3"}
    )
    handler.add("HEAD", "/azure/blob/myaccount/test/target.txt", 404)
    handler.add(
        "GET",
        "/azure/blob/myaccount/test?comp=list&delimiter=%2F&maxresults=1&prefix=target.txt%2F&restype=container",
        200,
    )

    def method(request):
        if request.headers["Content-Length"] != "0":
            sys.stderr.write(
                "Did not get expected headers: %s\n" % str(request.headers)
            )
            request.send_response(400)
            return
        expected = (
            "http://127.0.0.1:%d/azure/blob/myaccount/test/source.txt"
            % gdaltest.webserver_port
        )
        if request.headers["x-ms-copy-source"] != expected:
            sys.stderr.write(
                "Did not get expected headers: %s\n" % str(request.headers)
            )
            request.send_response(400)
            return

        request.send_response(202)
        request.send_header("Content-Length", 0)
        request.end_headers()

    handler.add("PUT", "/azure/blob/myaccount/test/target.txt", custom_method=method)
    handler.add("DELETE", "/azure/blob/myaccount/test/source.txt", 202)

    with webserver.install_http_handler(handler):
        assert gdal.Rename("/vsiaz/test/source.txt", "/vsiaz/test/target.txt") == 0


###############################################################################
# Test OpenDir() with a fake server


def test_vsiaz_opendir():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    # Unlimited depth
    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/azure/blob/myaccount/opendir?comp=list&restype=container",
        200,
        {"Content-type": "application/xml"},
        """<?xml version="1.0" encoding="UTF-8"?>
                    <EnumerationResults>
                        <Prefix></Prefix>
                        <Blobs>
                          <Blob>
                            <Name>test.txt</Name>
                            <Properties>
                              <Last-Modified>01 Jan 1970 00:00:01</Last-Modified>
                              <Content-Length>40</Content-Length>
                            </Properties>
                          </Blob>
                          <Blob>
                            <Name>subdir/.gdal_marker_for_dir</Name>
                          </Blob>
                          <Blob>
                            <Name>subdir/test.txt</Name>
                            <Properties>
                              <Last-Modified>01 Jan 1970 00:00:01</Last-Modified>
                              <Content-Length>4</Content-Length>
                            </Properties>
                          </Blob>
                        </Blobs>
                    </EnumerationResults>""",
    )
    with webserver.install_http_handler(handler):
        d = gdal.OpenDir("/vsiaz/opendir")
    assert d is not None

    entry = gdal.GetNextDirEntry(d)
    assert entry.name == "test.txt"
    assert entry.size == 40
    assert entry.mode == 32768
    assert entry.mtime == 1

    entry = gdal.GetNextDirEntry(d)
    assert entry.name == "subdir"
    assert entry.mode == 16384

    entry = gdal.GetNextDirEntry(d)
    assert entry.name == "subdir/test.txt"
    assert entry.size == 4
    assert entry.mode == 32768

    entry = gdal.GetNextDirEntry(d)
    assert entry is None

    gdal.CloseDir(d)

    # Prefix filtering on root of bucket
    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/azure/blob/myaccount/opendir?comp=list&prefix=my_prefix&restype=container",
        200,
        {"Content-type": "application/xml"},
        """<?xml version="1.0" encoding="UTF-8"?>
                    <EnumerationResults>
                        <Prefix>my_prefix</Prefix>
                        <Blobs>
                          <Blob>
                            <Name>my_prefix_test.txt</Name>
                            <Properties>
                              <Last-Modified>01 Jan 1970 00:00:01</Last-Modified>
                              <Content-Length>40</Content-Length>
                            </Properties>
                          </Blob>
                        </Blobs>
                    </EnumerationResults>""",
    )
    with webserver.install_http_handler(handler):
        d = gdal.OpenDir("/vsiaz/opendir", -1, ["PREFIX=my_prefix"])
    assert d is not None

    entry = gdal.GetNextDirEntry(d)
    assert entry.name == "my_prefix_test.txt"
    assert entry.size == 40
    assert entry.mode == 32768
    assert entry.mtime == 1

    entry = gdal.GetNextDirEntry(d)
    assert entry is None

    gdal.CloseDir(d)

    # Prefix filtering on root of subdir
    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/azure/blob/myaccount/opendir?comp=list&prefix=some_dir%2Fmy_prefix&restype=container",
        200,
        {"Content-type": "application/xml"},
        """<?xml version="1.0" encoding="UTF-8"?>
                    <EnumerationResults>
                        <Prefix>some_dir/my_prefix</Prefix>
                        <Blobs>
                          <Blob>
                            <Name>some_dir/my_prefix_test.txt</Name>
                            <Properties>
                              <Last-Modified>01 Jan 1970 00:00:01</Last-Modified>
                              <Content-Length>40</Content-Length>
                            </Properties>
                          </Blob>
                        </Blobs>
                    </EnumerationResults>""",
    )
    with webserver.install_http_handler(handler):
        d = gdal.OpenDir("/vsiaz/opendir/some_dir", -1, ["PREFIX=my_prefix"])
    assert d is not None

    entry = gdal.GetNextDirEntry(d)
    assert entry.name == "my_prefix_test.txt"
    assert entry.size == 40
    assert entry.mode == 32768
    assert entry.mtime == 1

    entry = gdal.GetNextDirEntry(d)
    assert entry is None

    gdal.CloseDir(d)

    # No network access done
    s = gdal.VSIStatL(
        "/vsiaz/opendir/some_dir/my_prefix_test.txt",
        gdal.VSI_STAT_EXISTS_FLAG
        | gdal.VSI_STAT_NATURE_FLAG
        | gdal.VSI_STAT_SIZE_FLAG
        | gdal.VSI_STAT_CACHE_ONLY,
    )
    assert s
    assert (s.mode & 32768) != 0
    assert s.size == 40
    assert s.mtime == 1

    # No network access done
    assert (
        gdal.VSIStatL(
            "/vsiaz/opendir/some_dir/i_do_not_exist.txt",
            gdal.VSI_STAT_EXISTS_FLAG
            | gdal.VSI_STAT_NATURE_FLAG
            | gdal.VSI_STAT_SIZE_FLAG
            | gdal.VSI_STAT_CACHE_ONLY,
        )
        is None
    )


###############################################################################
# Test RmdirRecursive() with a fake server


def test_vsiaz_rmdirrecursive():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/azure/blob/myaccount/rmdirrec?comp=list&prefix=subdir%2F&restype=container",
        200,
        {"Content-type": "application/xml"},
        """<?xml version="1.0" encoding="UTF-8"?>
                    <EnumerationResults>
                        <Prefix>subdir/</Prefix>
                        <Blobs>
                          <Blob>
                            <Name>subdir/test.txt</Name>
                            <Properties>
                              <Last-Modified>01 Jan 1970 00:00:01</Last-Modified>
                              <Content-Length>40</Content-Length>
                            </Properties>
                          </Blob>
                          <Blob>
                            <Name>subdir/subdir2/.gdal_marker_for_dir</Name>
                          </Blob>
                          <Blob>
                            <Name>subdir/subdir2/test.txt</Name>
                            <Properties>
                              <Last-Modified>01 Jan 1970 00:00:01</Last-Modified>
                              <Content-Length>4</Content-Length>
                            </Properties>
                          </Blob>
                        </Blobs>
                    </EnumerationResults>""",
    )
    handler.add("DELETE", "/azure/blob/myaccount/rmdirrec/subdir/test.txt", 202)
    handler.add("DELETE", "/azure/blob/myaccount/rmdirrec/subdir/subdir2/test.txt", 202)
    handler.add("HEAD", "/azure/blob/myaccount/rmdirrec/subdir/subdir2/", 404)
    handler.add(
        "GET",
        "/azure/blob/myaccount/rmdirrec?comp=list&delimiter=%2F&maxresults=1&prefix=subdir%2Fsubdir2%2F&restype=container",
        200,
        {"Content-type": "application/xml"},
        """<?xml version="1.0" encoding="UTF-8"?>
                    <EnumerationResults>
                        <Prefix>subdir/subdir2/</Prefix>
                        <Blobs>
                          <Blob>
                            <Name>subdir/subdir2/.gdal_marker_for_dir</Name>
                          </Blob>
                        </Blobs>
                    </EnumerationResults>""",
    )
    handler.add(
        "GET",
        "/azure/blob/myaccount/rmdirrec?comp=list&delimiter=%2F&maxresults=1&prefix=subdir%2Fsubdir2%2F&restype=container",
        200,
        {"Content-type": "application/xml"},
        """<?xml version="1.0" encoding="UTF-8"?>
                    <EnumerationResults>
                        <Prefix>subdir/subdir2/</Prefix>
                        <Blobs>
                          <Blob>
                            <Name>subdir/subdir2/.gdal_marker_for_dir</Name>
                          </Blob>
                        </Blobs>
                    </EnumerationResults>""",
    )
    handler.add(
        "DELETE",
        "/azure/blob/myaccount/rmdirrec/subdir/subdir2/.gdal_marker_for_dir",
        202,
    )
    handler.add("HEAD", "/azure/blob/myaccount/rmdirrec/subdir/", 404)
    handler.add(
        "GET",
        "/azure/blob/myaccount/rmdirrec?comp=list&delimiter=%2F&maxresults=1&prefix=subdir%2F&restype=container",
        200,
    )
    with webserver.install_http_handler(handler):
        assert gdal.RmdirRecursive("/vsiaz/rmdirrec/subdir") == 0


###############################################################################
# Test RmdirRecursive() with a fake server


def test_vsiaz_rmdirrecursive_empty_dir():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/azure/blob/myaccount/rmdirrec?comp=list&prefix=empty_dir%2F&restype=container",
        200,
        {"Content-type": "application/xml"},
        """<?xml version="1.0" encoding="UTF-8"?>
                    <EnumerationResults>
                        <Prefix>empty_dir/</Prefix>
                        <Blobs>
                          <Blob>
                            <Name>empty_dir/.gdal_marker_for_dir</Name>
                          </Blob>
                        </Blobs>
                    </EnumerationResults>""",
    )
    handler.add("HEAD", "/azure/blob/myaccount/rmdirrec/empty_dir/", 404)
    handler.add(
        "GET",
        "/azure/blob/myaccount/rmdirrec?comp=list&delimiter=%2F&maxresults=1&prefix=empty_dir%2F&restype=container",
        200,
        {"Content-type": "application/xml"},
        """<?xml version="1.0" encoding="UTF-8"?>
                    <EnumerationResults>
                        <Prefix>empty_dir/</Prefix>
                        <Blobs>
                          <Blob>
                            <Name>empty_dir/.gdal_marker_for_dir</Name>
                          </Blob>
                        </Blobs>
                    </EnumerationResults>""",
    )
    handler.add(
        "GET",
        "/azure/blob/myaccount/rmdirrec?comp=list&delimiter=%2F&maxresults=1&prefix=empty_dir%2F&restype=container",
        200,
        {"Content-type": "application/xml"},
        """<?xml version="1.0" encoding="UTF-8"?>
                    <EnumerationResults>
                        <Prefix>empty_dir/</Prefix>
                        <Blobs>
                          <Blob>
                            <Name>empty_dir/.gdal_marker_for_dir</Name>
                          </Blob>
                        </Blobs>
                    </EnumerationResults>""",
    )
    handler.add(
        "DELETE", "/azure/blob/myaccount/rmdirrec/empty_dir/.gdal_marker_for_dir", 202
    )
    with webserver.install_http_handler(handler):
        assert gdal.RmdirRecursive("/vsiaz/rmdirrec/empty_dir") == 0


###############################################################################
# Test Sync() and multithreaded download and CHUNK_SIZE


@pytest.mark.skipif(
    gdaltest.is_travis_branch("macos_build"), reason="randomly fails on macos"
)
def test_vsiaz_fake_sync_multithreaded_upload_chunk_size(tmp_vsimem):

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    gdal.Mkdir(tmp_vsimem / "test", 0)
    gdal.FileFromMemBuffer(tmp_vsimem / "test/foo", "foo\n")

    tab = [-1]
    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/azure/blob/myaccount/test_bucket?comp=list&prefix=test%2F&restype=container",
        200,
        {"Content-type": "application/xml"},
        """<?xml version="1.0" encoding="UTF-8"?>
                    <EnumerationResults>
                        <Prefix>test/</Prefix>
                        <Blobs/>
                    </EnumerationResults>
                """,
    )
    handler.add("HEAD", "/azure/blob/myaccount/test_bucket/test", 404)
    handler.add(
        "GET",
        "/azure/blob/myaccount/test_bucket?comp=list&delimiter=%2F&maxresults=1&prefix=test%2F&restype=container",
        200,
        {"Content-type": "application/xml"},
        """<?xml version="1.0" encoding="UTF-8"?>
                    <EnumerationResults>
                        <Prefix>test/</Prefix>
                        <Blobs/>
                    </EnumerationResults>
                """,
    )
    handler.add(
        "GET",
        "/azure/blob/myaccount/?comp=list",
        200,
        {"Content-type": "application/xml"},
        """<?xml version="1.0" encoding="UTF-8"?>
                    <EnumerationResults>
                        <Prefix></Prefix>
                        <Blobs/>
                    </EnumerationResults>
                """,
    )
    handler.add("HEAD", "/azure/blob/myaccount/test_bucket", 404)
    handler.add(
        "GET",
        "/azure/blob/myaccount/test_bucket?comp=list&delimiter=%2F&maxresults=1&restype=container",
        200,
        {"Content-type": "application/xml"},
        """<?xml version="1.0" encoding="UTF-8"?>
                    <EnumerationResults>
                        <Prefix></Prefix>
                        <Blobs>
                            <BlobPrefix>
                                <Name>something</Name>
                            </BlobPrefix>
                        </Blobs>
                    </EnumerationResults>
                """,
    )
    handler.add("HEAD", "/azure/blob/myaccount/test_bucket/test/", 404)
    handler.add(
        "GET",
        "/azure/blob/myaccount/test_bucket?comp=list&delimiter=%2F&maxresults=1&prefix=test%2F&restype=container",
        200,
        {"Content-type": "application/xml"},
        """<?xml version="1.0" encoding="UTF-8"?>
                    <EnumerationResults>
                        <Prefix>test/</Prefix>
                        <Blobs/>
                    </EnumerationResults>
                """,
    )
    handler.add(
        "PUT", "/azure/blob/myaccount/test_bucket/test/.gdal_marker_for_dir", 201
    )

    # Simulate an existing blob of another type
    handler.add(
        "PUT",
        "/azure/blob/myaccount/test_bucket/test/foo?blockid=000000000001&comp=block",
        409,
        expected_headers={"Content-Length": "3"},
    )

    handler.add("DELETE", "/azure/blob/myaccount/test_bucket/test/foo", 202)

    handler.add(
        "PUT",
        "/azure/blob/myaccount/test_bucket/test/foo?blockid=000000000001&comp=block",
        201,
        expected_headers={"Content-Length": "3"},
    )

    handler.add(
        "PUT",
        "/azure/blob/myaccount/test_bucket/test/foo?blockid=000000000002&comp=block",
        201,
        expected_headers={"Content-Length": "1"},
    )

    def method(request):
        h = request.headers
        if "Content-Length" not in h or h["Content-Length"] != "124":
            sys.stderr.write("Bad headers: %s\n" % str(h))
            request.send_response(403)
            return

        request.protocol_version = "HTTP/1.1"
        request.wfile.write("HTTP/1.1 100 Continue\r\n\r\n".encode("ascii"))
        content = request.rfile.read(124).decode("ascii")
        if content != """<?xml version="1.0" encoding="utf-8"?>
<BlockList>
<Latest>000000000001</Latest>
<Latest>000000000002</Latest>
</BlockList>
""":
            sys.stderr.write("Bad content: %s\n" % str(content))
            request.send_response(403)
            request.send_header("Content-Length", 0)
            request.end_headers()
            return
        request.send_response(201)
        request.send_header("Content-Length", 0)
        request.end_headers()

    handler.add(
        "PUT",
        "/azure/blob/myaccount/test_bucket/test/foo?comp=blocklist",
        custom_method=method,
    )

    def cbk(pct, _, tab):
        assert pct >= tab[0]
        tab[0] = pct
        return True

    with gdaltest.config_option("VSIS3_SIMULATE_THREADING", "YES", thread_local=False):
        with webserver.install_http_handler(handler):
            assert gdal.Sync(
                tmp_vsimem / "test",
                "/vsiaz/test_bucket",
                options=["NUM_THREADS=1", "CHUNK_SIZE=3"],
                callback=cbk,
                callback_data=tab,
            )
    assert tab[0] == 1.0


###############################################################################
# Test Sync() and multithreaded download of a single file


def test_vsiaz_fake_sync_multithreaded_upload_single_file(tmp_vsimem):

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    gdal.Mkdir(tmp_vsimem / "test", 0)
    gdal.FileFromMemBuffer(tmp_vsimem / "test/foo", "foo\n")

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/azure/blob/myaccount/?comp=list",
        200,
        {"Content-type": "application/xml"},
        """<?xml version="1.0" encoding="UTF-8"?>
                    <EnumerationResults>
                        <Prefix></Prefix>
                        <Containers>
                            <Container>
                                <Name>test_bucket</Name>
                            </Container>
                        </Containers>
                    </EnumerationResults>
                """,
    )
    handler.add("HEAD", "/azure/blob/myaccount/test_bucket/foo", 404)
    handler.add(
        "GET",
        "/azure/blob/myaccount/test_bucket?comp=list&delimiter=%2F&maxresults=1&prefix=foo%2F&restype=container",
        200,
    )

    handler.add(
        "PUT",
        "/azure/blob/myaccount/test_bucket/foo?blockid=000000000001&comp=block",
        201,
        expected_headers={"Content-Length": "3"},
    )

    handler.add(
        "PUT",
        "/azure/blob/myaccount/test_bucket/foo?blockid=000000000002&comp=block",
        201,
        expected_headers={"Content-Length": "1"},
    )

    def method(request):
        h = request.headers
        if "Content-Length" not in h or h["Content-Length"] != "124":
            sys.stderr.write("Bad headers: %s\n" % str(h))
            request.send_response(403)
            return

        request.protocol_version = "HTTP/1.1"
        request.wfile.write("HTTP/1.1 100 Continue\r\n\r\n".encode("ascii"))
        content = request.rfile.read(124).decode("ascii")
        if content != """<?xml version="1.0" encoding="utf-8"?>
<BlockList>
<Latest>000000000001</Latest>
<Latest>000000000002</Latest>
</BlockList>
""":
            sys.stderr.write("Bad content: %s\n" % str(content))
            request.send_response(403)
            request.send_header("Content-Length", 0)
            request.end_headers()
            return
        request.send_response(201)
        request.send_header("Content-Length", 0)
        request.end_headers()

    handler.add(
        "PUT",
        "/azure/blob/myaccount/test_bucket/foo?comp=blocklist",
        custom_method=method,
    )

    with gdaltest.config_option("VSIS3_SIMULATE_THREADING", "YES", thread_local=False):
        with webserver.install_http_handler(handler):
            assert gdal.Sync(
                tmp_vsimem / "test/foo",
                "/vsiaz/test_bucket",
                options=["NUM_THREADS=1", "CHUNK_SIZE=3"],
            )


###############################################################################
# Read credentials from simulated Azure VM


def test_vsiaz_imds_authentication():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    with gdaltest.config_options(
        {
            "AZURE_STORAGE_CONNECTION_STRING": "",
            "AZURE_STORAGE_ACCOUNT": "myaccount",
            "CPL_AZURE_ENDPOINT": "http://127.0.0.1:%d/azure/blob/myaccount"
            % gdaltest.webserver_port,
            "CPL_AZURE_USE_HTTPS": "NO",
            "CPL_AZURE_VM_API_ROOT_URL": "http://localhost:%d"
            % gdaltest.webserver_port,
        },
        thread_local=False,
    ):

        handler = webserver.SequentialHandler()
        handler.add(
            "GET",
            "/metadata/identity/oauth2/token?api-version=2018-02-01&resource=https%3A%2F%2Fstorage.azure.com%2F",
            200,
            {},
            """{
                    "access_token": "my_bearer",
                    "expires_on": "99999999999",
                    }""",
            expected_headers={"Metadata": "true"},
        )

        handler.add(
            "GET",
            "/azure/blob/myaccount/az_fake_bucket/resource",
            200,
            {"Content-Length": 3},
            "foo",
            expected_headers={
                "Authorization": "Bearer my_bearer",
                "x-ms-version": "2019-12-12",
            },
        )
        with webserver.install_http_handler(handler):
            f = open_for_read("/vsiaz/az_fake_bucket/resource")
            assert f is not None
            data = gdal.VSIFReadL(1, 4, f).decode("ascii")
            gdal.VSIFCloseL(f)

        assert data == "foo"

    # Set a fake URL to check that credentials re-use works
    with gdaltest.config_options(
        {
            "AZURE_STORAGE_CONNECTION_STRING": "",
            "AZURE_STORAGE_ACCOUNT": "myaccount",
            "CPL_AZURE_ENDPOINT": "http://127.0.0.1:%d/azure/blob/myaccount"
            % gdaltest.webserver_port,
            "CPL_AZURE_USE_HTTPS": "NO",
            "CPL_AZURE_VM_API_ROOT_URL": "invalid",
        },
        thread_local=False,
    ):

        handler = webserver.SequentialHandler()
        handler.add(
            "GET",
            "/azure/blob/myaccount/az_fake_bucket/bar",
            200,
            {"Content-Length": 3},
            "bar",
            expected_headers={
                "Authorization": "Bearer my_bearer",
                "x-ms-version": "2019-12-12",
            },
        )
        with webserver.install_http_handler(handler):
            f = open_for_read("/vsiaz/az_fake_bucket/bar")
            assert f is not None
            data = gdal.VSIFReadL(1, 4, f).decode("ascii")
            gdal.VSIFCloseL(f)

        assert data == "bar"


###############################################################################
# Read credentials from simulated Azure VM with expiration


def test_vsiaz_imds_authentication_expiration():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    with gdaltest.config_options(
        {
            "AZURE_STORAGE_CONNECTION_STRING": "",
            "AZURE_STORAGE_ACCOUNT": "myaccount",
            "CPL_AZURE_ENDPOINT": "http://127.0.0.1:%d/azure/blob/myaccount"
            % gdaltest.webserver_port,
            "CPL_AZURE_USE_HTTPS": "NO",
            "CPL_AZURE_VM_API_ROOT_URL": "http://localhost:%d"
            % gdaltest.webserver_port,
        },
        thread_local=False,
    ):

        handler = webserver.SequentialHandler()
        handler.add(
            "GET",
            "/metadata/identity/oauth2/token?api-version=2018-02-01&resource=https%3A%2F%2Fstorage.azure.com%2F",
            200,
            {},
            """{
                    "access_token": "my_bearer",
                    "expires_on": "1000",
                    }""",
            expected_headers={"Metadata": "true"},
        )
        # Credentials requested again since they are expired
        handler.add(
            "GET",
            "/metadata/identity/oauth2/token?api-version=2018-02-01&resource=https%3A%2F%2Fstorage.azure.com%2F",
            200,
            {},
            """{
                    "access_token": "my_bearer",
                    "expires_on": "1000",
                    }""",
            expected_headers={"Metadata": "true"},
        )
        handler.add(
            "GET",
            "/azure/blob/myaccount/az_fake_bucket/resource",
            200,
            {"Content-Length": 3},
            "foo",
            expected_headers={
                "Authorization": "Bearer my_bearer",
                "x-ms-version": "2019-12-12",
            },
        )
        with webserver.install_http_handler(handler):
            f = open_for_read("/vsiaz/az_fake_bucket/resource")
            assert f is not None
            data = gdal.VSIFReadL(1, 4, f).decode("ascii")
            gdal.VSIFCloseL(f)

        assert data == "foo"


###############################################################################
# Test support for object_id/client_id/msi_res_id parameters of IMDS


def test_vsiaz_imds_authentication_object_id_client_is_msi_res_id():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    try:
        with gdaltest.config_options(
            {
                "AZURE_STORAGE_CONNECTION_STRING": "",
                "AZURE_STORAGE_ACCOUNT": "myaccount",
                "CPL_AZURE_ENDPOINT": "http://127.0.0.1:%d/azure/blob/myaccount"
                % gdaltest.webserver_port,
                "CPL_AZURE_USE_HTTPS": "NO",
                "CPL_AZURE_VM_API_ROOT_URL": "http://localhost:%d"
                % gdaltest.webserver_port,
            },
            thread_local=False,
        ):

            handler = webserver.SequentialHandler()
            handler.add(
                "GET",
                "/metadata/identity/oauth2/token?api-version=2018-02-01&resource=https%3A%2F%2Fstorage.azure.com%2F&object_id=my_object_id&client_id=my_client_id&msi_res_id=my_msi_res_id",
                200,
                {},
                """{
                        "access_token": "my_bearer",
                        "expires_on": "99999999999",
                        }""",
                expected_headers={"Metadata": "true"},
            )
            handler.add(
                "GET",
                "/azure/blob/myaccount/az_fake_bucket/resource",
                200,
                {"Content-Length": 3},
                "foo",
                expected_headers={
                    "Authorization": "Bearer my_bearer",
                    "x-ms-version": "2019-12-12",
                },
            )

            gdal.SetPathSpecificOption(
                "/vsiaz/az_fake_bucket/", "AZURE_IMDS_OBJECT_ID", "my_object_id"
            )
            gdal.SetPathSpecificOption(
                "/vsiaz/az_fake_bucket/", "AZURE_IMDS_CLIENT_ID", "my_client_id"
            )
            gdal.SetPathSpecificOption(
                "/vsiaz/az_fake_bucket/", "AZURE_IMDS_MSI_RES_ID", "my_msi_res_id"
            )
            with webserver.install_http_handler(handler):
                f = open_for_read("/vsiaz/az_fake_bucket/resource")
                assert f is not None
                data = gdal.VSIFReadL(1, 4, f).decode("ascii")
                gdal.VSIFCloseL(f)
            assert data == "foo"

            # Query another bucket with different object_id/client_id/msi_res_id
            handler = webserver.SequentialHandler()
            handler.add(
                "GET",
                "/metadata/identity/oauth2/token?api-version=2018-02-01&resource=https%3A%2F%2Fstorage.azure.com%2F&object_id=my_object_id2&client_id=my_client_id2&msi_res_id=my_msi_res_id2",
                200,
                {},
                """{
                        "access_token": "my_bearer2",
                        "expires_on": "99999999999",
                        }""",
                expected_headers={"Metadata": "true"},
            )

            handler.add(
                "POST",
                "/azure/blob/myaccount/?comp=batch",
                202,
                {"content-type": "multipart/mixed; boundary=my_boundary"},
                """--my_boundary
    Content-Type: application/http
    Content-ID: <0>

    HTTP/1.1 202 Accepted

    --my_boundary--
        """,
                expected_body=b"--batch_ec2ce0a7-deaf-11ed-9ad8-3fabe5ecd589\r\nContent-Type: application/http\r\nContent-ID: <0>\r\nContent-Transfer-Encoding: binary\r\n\r\nDELETE /az_fake_bucket2/myfile HTTP/1.1\r\n\r\nAuthorization: Bearer my_bearer2\r\nContent-Length: 0\r\n\r\n\r\n--batch_ec2ce0a7-deaf-11ed-9ad8-3fabe5ecd589--\r\n",
            )

            gdal.SetPathSpecificOption(
                "/vsiaz/az_fake_bucket2/", "AZURE_IMDS_OBJECT_ID", "my_object_id2"
            )
            gdal.SetPathSpecificOption(
                "/vsiaz/az_fake_bucket2/", "AZURE_IMDS_CLIENT_ID", "my_client_id2"
            )
            gdal.SetPathSpecificOption(
                "/vsiaz/az_fake_bucket2/", "AZURE_IMDS_MSI_RES_ID", "my_msi_res_id2"
            )

            with webserver.install_http_handler(handler):
                ret = gdal.UnlinkBatch(
                    [
                        "/vsiaz/az_fake_bucket2/myfile",
                    ]
                )
            assert ret

            # Check that querying again under /vsiaz/az_fake_bucket/ reuses
            # the cached token
            handler.add(
                "GET",
                "/azure/blob/myaccount/az_fake_bucket/resource2",
                200,
                {"Content-Length": 4},
                "foo2",
                expected_headers={
                    "Authorization": "Bearer my_bearer",
                    "x-ms-version": "2019-12-12",
                },
            )
            with webserver.install_http_handler(handler):
                f = open_for_read("/vsiaz/az_fake_bucket/resource2")
                assert f is not None
                data = gdal.VSIFReadL(1, 5, f).decode("ascii")
                gdal.VSIFCloseL(f)
            assert data == "foo2"

    finally:
        gdal.ClearPathSpecificOptions("/vsiaz/az_fake_bucket/")
        gdal.ClearPathSpecificOptions("/vsiaz/az_fake_bucket2/")


###############################################################################
# Get authentication for Workload Identity from simulated Azure VM


def test_vsiaz_workload_identity_managed_authentication():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    with gdaltest.config_options(
        {
            "AZURE_STORAGE_CONNECTION_STRING": "",
            "AZURE_STORAGE_ACCOUNT": "myaccount",
            "CPL_AZURE_ENDPOINT": "http://127.0.0.1:%d/azure/blob/myaccount"
            % gdaltest.webserver_port,
            "CPL_AZURE_USE_HTTPS": "NO",
            "AZURE_AUTHORITY_HOST": "http://localhost:%d/" % gdaltest.webserver_port,
            "AZURE_TENANT_ID": "tenant_id",
            "AZURE_CLIENT_ID": "client_id_value",
            "AZURE_FEDERATED_TOKEN_FILE": "/vsimem/AZURE_FEDERATED_TOKEN_FILE",
        },
        thread_local=False,
    ), gdaltest.tempfile(
        "/vsimem/AZURE_FEDERATED_TOKEN_FILE", "content_of_AZURE_FEDERATED_TOKEN_FILE"
    ):

        handler = webserver.SequentialHandler()
        handler.add(
            "POST",
            "/tenant_id/oauth2/v2.0/token",
            200,
            {},
            """{"access_token": "my_bearer", "expires_in": "100"}""",
            expected_headers={"Content-Type": "application/x-www-form-urlencoded"},
            expected_body=b"client_assertion=content_of_AZURE_FEDERATED_TOKEN_FILE&client_assertion_type=urn:ietf:params:oauth:client-assertion-type:jwt-bearer&client_id=client_id_value&grant_type=client_credentials&scope=https://storage.azure.com/.default",
        )

        handler.add(
            "GET",
            "/azure/blob/myaccount/az_fake_bucket/resource",
            200,
            {"Content-Length": 3},
            "foo",
            expected_headers={
                "Authorization": "Bearer my_bearer",
                "x-ms-version": "2019-12-12",
            },
        )
        with webserver.install_http_handler(handler):
            f = open_for_read("/vsiaz/az_fake_bucket/resource")
            assert f is not None
            data = gdal.VSIFReadL(1, 4, f).decode("ascii")
            gdal.VSIFCloseL(f)

        assert data == "foo"

    # Set a fake AZURE_FEDERATED_TOKEN_FILE to check that credentials re-use works
    with gdaltest.config_options(
        {
            "AZURE_STORAGE_CONNECTION_STRING": "",
            "AZURE_STORAGE_ACCOUNT": "myaccount",
            "CPL_AZURE_ENDPOINT": "http://127.0.0.1:%d/azure/blob/myaccount"
            % gdaltest.webserver_port,
            "CPL_AZURE_USE_HTTPS": "NO",
            "AZURE_AUTHORITY_HOST": "http://localhost:%d/" % gdaltest.webserver_port,
            "AZURE_TENANT_ID": "tenant_id",
            "AZURE_CLIENT_ID": "client_id_value",
            "AZURE_FEDERATED_TOKEN_FILE": "****invalid******",
        },
        thread_local=False,
    ):

        handler = webserver.SequentialHandler()
        handler.add(
            "GET",
            "/azure/blob/myaccount/az_fake_bucket/bar",
            200,
            {"Content-Length": 3},
            "bar",
            expected_headers={
                "Authorization": "Bearer my_bearer",
                "x-ms-version": "2019-12-12",
            },
        )
        with webserver.install_http_handler(handler):
            f = open_for_read("/vsiaz/az_fake_bucket/bar")
            assert f is not None
            data = gdal.VSIFReadL(1, 4, f).decode("ascii")
            gdal.VSIFCloseL(f)

        assert data == "bar"


###############################################################################
# Get authentication for Workload Identity from simulated Azure VM


def test_vsiaz_workload_identity_managed_authentication_expiration():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    with gdaltest.config_options(
        {
            "AZURE_STORAGE_CONNECTION_STRING": "",
            "AZURE_STORAGE_ACCOUNT": "myaccount",
            "CPL_AZURE_ENDPOINT": "http://127.0.0.1:%d/azure/blob/myaccount"
            % gdaltest.webserver_port,
            "CPL_AZURE_USE_HTTPS": "NO",
            "AZURE_AUTHORITY_HOST": "http://localhost:%d/" % gdaltest.webserver_port,
            "AZURE_TENANT_ID": "tenant_id",
            "AZURE_CLIENT_ID": "client_id_value",
            "AZURE_FEDERATED_TOKEN_FILE": "/vsimem/AZURE_FEDERATED_TOKEN_FILE",
        },
        thread_local=False,
    ):

        handler = webserver.SequentialHandler()

        # Done once because of VSICurlClearCache()
        handler.add(
            "POST",
            "/tenant_id/oauth2/v2.0/token",
            200,
            {},
            """{"access_token": "my_bearer", "expires_in": "10"}""",
            expected_headers={"Content-Type": "application/x-www-form-urlencoded"},
            expected_body=b"client_assertion=content_of_AZURE_FEDERATED_TOKEN_FILE&client_assertion_type=urn:ietf:params:oauth:client-assertion-type:jwt-bearer&client_id=client_id_value&grant_type=client_credentials&scope=https://storage.azure.com/.default",
        )

        # We have a security margin of 60 seconds over the expires_in delay
        # so we will need to fetch the token again
        handler.add(
            "POST",
            "/tenant_id/oauth2/v2.0/token",
            200,
            {},
            """{"access_token": "my_bearer2", "expires_in": "10"}""",
            expected_headers={"Content-Type": "application/x-www-form-urlencoded"},
            expected_body=b"client_assertion=content_of_AZURE_FEDERATED_TOKEN_FILE&client_assertion_type=urn:ietf:params:oauth:client-assertion-type:jwt-bearer&client_id=client_id_value&grant_type=client_credentials&scope=https://storage.azure.com/.default",
        )

        handler.add(
            "GET",
            "/azure/blob/myaccount/az_fake_bucket/resource",
            200,
            {"Content-Length": 3},
            "foo",
            expected_headers={
                "Authorization": "Bearer my_bearer2",
                "x-ms-version": "2019-12-12",
            },
        )
        with webserver.install_http_handler(handler), gdaltest.tempfile(
            "/vsimem/AZURE_FEDERATED_TOKEN_FILE",
            "content_of_AZURE_FEDERATED_TOKEN_FILE",
        ):
            f = open_for_read("/vsiaz/az_fake_bucket/resource")
            assert f is not None
            data = gdal.VSIFReadL(1, 4, f).decode("ascii")
            gdal.VSIFCloseL(f)

        assert data == "foo"

        # Check that AZURE_FEDERATED_TOKEN_FILE isn't actually read

        handler = webserver.SequentialHandler()
        handler.add(
            "POST",
            "/tenant_id/oauth2/v2.0/token",
            200,
            {},
            """{"access_token": "my_bearer3", "expires_in": "100"}""",
            expected_headers={"Content-Type": "application/x-www-form-urlencoded"},
            expected_body=b"client_assertion=content_of_AZURE_FEDERATED_TOKEN_FILE&client_assertion_type=urn:ietf:params:oauth:client-assertion-type:jwt-bearer&client_id=client_id_value&grant_type=client_credentials&scope=https://storage.azure.com/.default",
        )

        handler.add(
            "GET",
            "/azure/blob/myaccount/az_fake_bucket2/resource",
            200,
            {"Content-Length": 3},
            "bar",
            expected_headers={
                "Authorization": "Bearer my_bearer3",
                "x-ms-version": "2019-12-12",
            },
        )
        with webserver.install_http_handler(handler), gdaltest.tempfile(
            "/vsimem/AZURE_FEDERATED_TOKEN_FILE",
            "another_content_of_AZURE_FEDERATED_TOKEN_FILE",
        ):
            f = open_for_read("/vsiaz/az_fake_bucket2/resource")
            assert f is not None
            data = gdal.VSIFReadL(1, 4, f).decode("ascii")
            gdal.VSIFCloseL(f)

        assert data == "bar"


###############################################################################
# Test GetFileMetadata () / SetFileMetadata()


def test_vsiaz_fake_metadata():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add(
        "HEAD",
        "/azure/blob/myaccount/test/foo.bin",
        200,
        {"Content-Length": "3", "x-ms-foo": "bar"},
    )
    with webserver.install_http_handler(handler):
        md = gdal.GetFileMetadata("/vsiaz/test/foo.bin", "HEADERS")
        assert "x-ms-foo" in md
        assert md["x-ms-foo"] == "bar"

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/azure/blob/myaccount/test/foo.bin?comp=metadata",
        200,
        {"x-ms-meta-foo": "bar"},
    )
    with webserver.install_http_handler(handler):
        md = gdal.GetFileMetadata("/vsiaz/test/foo.bin", "METADATA")
        assert "x-ms-meta-foo" in md
        assert md["x-ms-meta-foo"] == "bar"

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/azure/blob/myaccount/test/foo.bin?comp=tags",
        200,
        {},
        """<Tags><TagSet><Tag><Key>foo</Key><Value>bar</Value></Tag></TagSet></Tags>""",
    )
    with webserver.install_http_handler(handler):
        md = gdal.GetFileMetadata("/vsiaz/test/foo.bin", "TAGS")
        assert "foo" in md
        assert md["foo"] == "bar"

    # Error case
    handler = webserver.SequentialHandler()
    handler.add("GET", "/azure/blob/myaccount/test/foo.bin?comp=metadata", 404)
    with webserver.install_http_handler(handler):
        assert gdal.GetFileMetadata("/vsiaz/test/foo.bin", "METADATA") == {}

    # SetMetadata()
    handler = webserver.SequentialHandler()
    handler.add(
        "PUT",
        "/azure/blob/myaccount/test/foo.bin?comp=properties",
        200,
        expected_headers={"x-ms-foo": "bar"},
    )
    with webserver.install_http_handler(handler):
        assert gdal.SetFileMetadata(
            "/vsiaz/test/foo.bin", {"x-ms-foo": "bar"}, "PROPERTIES"
        )

    handler = webserver.SequentialHandler()
    handler.add(
        "PUT",
        "/azure/blob/myaccount/test/foo.bin?comp=metadata",
        200,
        expected_headers={"x-ms-meta-foo": "bar"},
    )
    with webserver.install_http_handler(handler):
        assert gdal.SetFileMetadata(
            "/vsiaz/test/foo.bin", {"x-ms-meta-foo": "bar"}, "METADATA"
        )

    handler = webserver.SequentialHandler()
    handler.add(
        "PUT", "/azure/blob/myaccount/test/foo.bin?comp=tags", 204, expected_body=b""
    )
    with webserver.install_http_handler(handler):
        assert gdal.SetFileMetadata("/vsiaz/test/foo.bin", {"FOO": "BAR"}, "TAGS")

    # Error case
    handler = webserver.SequentialHandler()
    handler.add("PUT", "/azure/blob/myaccount/test/foo.bin?comp=metadata", 404)
    with webserver.install_http_handler(handler):
        assert not gdal.SetFileMetadata(
            "/vsiaz/test/foo.bin", {"x-ms-meta-foo": "bar"}, "METADATA"
        )


###############################################################################
# Read credentials from configuration file


def test_vsiaz_read_credentials_config_file_connection_string():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    config_content = """
[unrelated]
account=foo
[storage]
connection_string = DefaultEndpointsProtocol=http;AccountName=myaccount2;AccountKey=MY_ACCOUNT_KEY;BlobEndpoint=http://127.0.0.1:%d/azure/blob/myaccount2
""" % gdaltest.webserver_port

    with gdaltest.tempfile(
        "/vsimem/azure_config_dir/config", config_content
    ), gdaltest.config_options(
        {
            "AZURE_STORAGE_CONNECTION_STRING": None,
            "AZURE_CONFIG_DIR": "/vsimem/azure_config_dir",
        },
        thread_local=False,
    ):
        handler = webserver.SequentialHandler()
        handler.add(
            "GET",
            "/azure/blob/myaccount2/az_fake_bucket/resource",
            200,
            {"Content-Length": 3},
            "foo",
            expected_headers={
                "Authorization": "SharedKey myaccount2:Cm8BtA8Wkst7zAdGmcoKoR0tWuj2rzO+WpfBwWQ4RrY="
            },
        )
        with webserver.install_http_handler(handler):
            f = open_for_read("/vsiaz/az_fake_bucket/resource")
            assert f is not None
            data = gdal.VSIFReadL(1, 4, f).decode("ascii")
            gdal.VSIFCloseL(f)

        assert data == "foo"


###############################################################################
# Read credentials from configuration file


def test_vsiaz_read_credentials_config_file_account_and_key():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    config_content = """
[unrelated]
account=foo
[storage]
account = myaccount2
key = MY_ACCOUNT_KEY
"""

    with gdaltest.tempfile(
        "/vsimem/azure_config_dir/config", config_content
    ), gdaltest.config_options(
        {
            "AZURE_STORAGE_CONNECTION_STRING": None,
            "CPL_AZURE_ENDPOINT": "http://127.0.0.1:%d/azure/blob/myaccount2"
            % gdaltest.webserver_port,
            "CPL_AZURE_USE_HTTPS": "NO",
            "AZURE_CONFIG_DIR": "/vsimem/azure_config_dir",
        },
        thread_local=False,
    ):
        handler = webserver.SequentialHandler()
        handler.add(
            "GET",
            "/azure/blob/myaccount2/az_fake_bucket/resource",
            200,
            {"Content-Length": 3},
            "foo",
            expected_headers={
                "Authorization": "SharedKey myaccount2:Cm8BtA8Wkst7zAdGmcoKoR0tWuj2rzO+WpfBwWQ4RrY="
            },
        )
        with webserver.install_http_handler(handler):
            f = open_for_read("/vsiaz/az_fake_bucket/resource")
            assert f is not None
            data = gdal.VSIFReadL(1, 4, f).decode("ascii")
            gdal.VSIFCloseL(f)

        assert data == "foo"


###############################################################################
# Read credentials from configuration file


def test_vsiaz_read_credentials_config_file_account_and_sas_token():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    config_content = """
[unrelated]
account=foo
[storage]
account = myaccount2
sas_token = sig=sas
"""

    with gdaltest.tempfile(
        "/vsimem/azure_config_dir/config", config_content
    ), gdaltest.config_options(
        {
            "AZURE_STORAGE_CONNECTION_STRING": None,
            "CPL_AZURE_ENDPOINT": "http://127.0.0.1:%d/azure/blob/myaccount2"
            % gdaltest.webserver_port,
            "CPL_AZURE_USE_HTTPS": "NO",
            "AZURE_CONFIG_DIR": "/vsimem/azure_config_dir",
        },
        thread_local=False,
    ):
        handler = webserver.SequentialHandler()
        handler.add(
            "GET",
            "/azure/blob/myaccount2/az_fake_bucket/resource?sig=sas",
            200,
            {"Content-Length": 3},
            "foo",
        )
        with webserver.install_http_handler(handler):
            f = open_for_read("/vsiaz/az_fake_bucket/resource")
            assert f is not None
            data = gdal.VSIFReadL(1, 4, f).decode("ascii")
            gdal.VSIFCloseL(f)

        assert data == "foo"


###############################################################################
# Read credentials from configuration file


def test_vsiaz_read_credentials_config_file_missing_account():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    config_content = """
[unrelated]
account=foo
[storage]
"""

    with gdaltest.tempfile(
        "/vsimem/azure_config_dir/config", config_content
    ), gdaltest.config_options(
        {
            "AZURE_STORAGE_CONNECTION_STRING": None,
            "CPL_AZURE_ENDPOINT": "http://127.0.0.1:%d/azure/blob/foo"
            % gdaltest.webserver_port,
            "CPL_AZURE_USE_HTTPS": "NO",
            "AZURE_CONFIG_DIR": "/vsimem/azure_config_dir",
        },
        thread_local=False,
    ):
        handler = webserver.SequentialHandler()
        with webserver.install_http_handler(handler):
            f = open_for_read("/vsiaz/az_fake_bucket/resource")
            assert f is None


###############################################################################
# Read credentials from configuration file


def test_vsiaz_access_token():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    with gdaltest.config_options(
        {
            "AZURE_STORAGE_CONNECTION_STRING": None,
            "AZURE_STORAGE_ACCOUNT": "myaccount",
            "CPL_AZURE_ENDPOINT": "http://127.0.0.1:%d/azure/blob/myaccount"
            % gdaltest.webserver_port,
            "CPL_AZURE_USE_HTTPS": "NO",
            "AZURE_STORAGE_ACCESS_TOKEN": "my_token",
        },
        thread_local=False,
    ):
        handler = webserver.SequentialHandler()
        handler.add(
            "GET",
            "/azure/blob/myaccount/az_fake_bucket/resource",
            200,
            {"Content-Length": 3},
            "foo",
            expected_headers={"Authorization": "Bearer my_token"},
        )
        with webserver.install_http_handler(handler):
            f = open_for_read("/vsiaz/az_fake_bucket/resource")
            assert f is not None
            data = gdal.VSIFReadL(1, 4, f).decode("ascii")
            gdal.VSIFCloseL(f)

        assert data == "foo"


###############################################################################
# Test server-side copy from S3 to Azure


def test_vsiaz_copy_from_vsis3():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    options = copy.copy(general_s3_options)
    options["AWS_S3_ENDPOINT"] = f"127.0.0.1:{gdaltest.webserver_port}"

    with gdaltest.config_options(options, thread_local=False):
        handler = webserver.SequentialHandler()
        handler.add(
            "GET",
            "/s3_bucket/test.bin",
            200,
            {"Content-Length": "3"},
            "foo",
        )

        if gdaltest.webserver_port == 8080:
            expected_headers = {
                "x-ms-copy-source": "http://127.0.0.1:%d/s3_bucket/test.bin?X-Amz-Algorithm=AWS4-HMAC-SHA256&X-Amz-Credential=AWS_ACCESS_KEY_ID%%2F20150101%%2Fus-east-1%%2Fs3%%2Faws4_request&X-Amz-Date=20150101T000000Z&X-Amz-Expires=3600&X-Amz-Signature=49294bd260338188b336ff2ed2c202e95d503439aca8fd9b2982c91992fa584d&X-Amz-SignedHeaders=host"
                % gdaltest.webserver_port
            }
        elif gdaltest.webserver_port == 8081:
            expected_headers = {
                "x-ms-copy-source": "http://127.0.0.1:%d/s3_bucket/test.bin?X-Amz-Algorithm=AWS4-HMAC-SHA256&X-Amz-Credential=AWS_ACCESS_KEY_ID%%2F20150101%%2Fus-east-1%%2Fs3%%2Faws4_request&X-Amz-Date=20150101T000000Z&X-Amz-Expires=3600&X-Amz-Signature=20dfcb6bdd7a4e55fc58a171b7a25dcce55244f990e4d1e0361eed1bbb729a07&X-Amz-SignedHeaders=host"
                % gdaltest.webserver_port
            }
        else:
            expected_headers = {}

        handler.add(
            "PUT",
            "/azure/blob/myaccount/az_container/test.bin",
            202,
            expected_headers=expected_headers,
        )

        with webserver.install_http_handler(handler):
            assert (
                gdal.CopyFile(
                    "/vsis3/s3_bucket/test.bin", "/vsiaz/az_container/test.bin"
                )
                == 0
            )


###############################################################################
# Test server-side copy from Azure to Azure, but with source and target being
# in same bucket


def test_vsiaz_copy_from_vsiaz_same_bucket():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()

    def method(request):

        request.protocol_version = "HTTP/1.1"
        h = request.headers
        if (
            "x-ms-copy-source" not in h
            or h["x-ms-copy-source"]
            != f"http://127.0.0.1:{gdaltest.webserver_port}/azure/blob/myaccount/az_container/test.bin"
        ):
            sys.stderr.write("Bad headers: %s\n" % str(h))
            request.send_response(403)
            return
        request.send_response(202)
        request.send_header("Connection", "close")
        request.end_headers()

    handler.add(
        "PUT",
        "/azure/blob/myaccount/az_container/test2.bin",
        custom_method=method,
    )

    with webserver.install_http_handler(handler):
        assert (
            gdal.CopyFile(
                "/vsiaz/az_container/test.bin",
                "/vsiaz/az_container/test2.bin",
            )
            == 0
        )


###############################################################################
# Test server-side copy from Azure to Azure, but with source and target being
# in different buckets


def test_vsiaz_copy_from_vsiaz_different_storage_bucket():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add(
        "HEAD",
        "/azure/blob/myaccount/az_source_container/test.bin",
        200,
        {"Content-Length": "3"},
    )

    def method(request):

        request.protocol_version = "HTTP/1.1"
        h = request.headers
        if "x-ms-copy-source" not in h or (
            not h["x-ms-copy-source"].startswith(
                f"http://127.0.0.1:{gdaltest.webserver_port}/azure/blob/myaccount/az_source_container/test.bin?se="
            )
        ):
            sys.stderr.write("Bad headers: %s\n" % str(h))
            request.send_response(403)
            return
        request.send_response(202)
        request.send_header("Connection", "close")
        request.end_headers()

    handler.add(
        "PUT",
        "/azure/blob/myaccount/az_target_container/test.bin",
        custom_method=method,
    )

    with webserver.install_http_handler(handler):
        assert (
            gdal.CopyFile(
                "/vsiaz/az_source_container/test.bin",
                "/vsiaz/az_target_container/test.bin",
            )
            == 0
        )


###############################################################################
# Test VSIMultipartUploadXXXX()


def test_vsiaz_MultipartUpload():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    # Test MultipartUploadGetCapabilities()
    info = gdal.MultipartUploadGetCapabilities("/vsiaz/")
    assert info.non_sequential_upload_supported
    assert info.parallel_upload_supported
    assert not info.abort_supported
    assert info.min_part_size == 0
    assert info.max_part_size >= 1024
    assert info.max_part_count == 50000

    # Test unsupported MultipartUploadAbort()
    with gdal.ExceptionMgr(useExceptions=True):
        with pytest.raises(
            Exception,
            match=r"MultipartUploadAbort\(\) not supported by this file system",
        ):
            gdal.MultipartUploadAbort("/vsiaz/foo/bar", "upload_id")
