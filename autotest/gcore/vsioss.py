#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test /vsioss
# Author:   Even Rouault <even dot rouault at spatialys dot com>
#
###############################################################################
# Copyright (c) 2017, Even Rouault <even dot rouault at spatialys dot com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import os
import stat
import sys

import gdaltest
import pytest
import webserver

from osgeo import gdal

pytestmark = pytest.mark.require_curl()


def open_for_read(uri):
    """
    Opens a test file for reading.
    """
    return gdal.VSIFOpenExL(uri, "rb", 1)


###############################################################################


@pytest.fixture(scope="module", autouse=True)
def setup_and_cleanup():

    with gdal.config_options(
        {
            "OSS_SECRET_ACCESS_KEY": "",
            "OSS_ACCESS_KEY_ID": "",
            "OSS_HTTPS": "",
            "OSS_VIRTUAL_HOSTING": "",
            "OSS_ENDPOINT": "",
        },
        thread_local=False,
    ):
        assert gdal.GetSignedURL("/vsioss/foo/bar") is None

        yield


@pytest.fixture(scope="module")
def server_backend():

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


@pytest.fixture(
    scope="function"
)  # function scope because not all tests run with the config options set below
def server(server_backend):

    with gdal.config_options(
        {
            "OSS_SECRET_ACCESS_KEY": "OSS_SECRET_ACCESS_KEY",
            "OSS_ACCESS_KEY_ID": "OSS_ACCESS_KEY_ID",
            "CPL_OSS_TIMESTAMP": "my_timestamp",
            "OSS_HTTPS": "NO",
            "OSS_VIRTUAL_HOSTING": "NO",
            "OSS_ENDPOINT": "127.0.0.1:%d" % server_backend.port,
        },
        thread_local=False,
    ):
        yield server_backend


###############################################################################
# Error cases


def test_vsioss_1():

    # Missing OSS_SECRET_ACCESS_KEY
    gdal.ErrorReset()
    with gdal.quiet_errors():
        f = open_for_read("/vsioss/foo/bar")
    assert f is None
    assert gdal.VSIGetLastErrorMsg().find("OSS_SECRET_ACCESS_KEY") >= 0


def test_vsioss_1a():

    gdal.ErrorReset()
    with gdal.quiet_errors():
        f = open_for_read("/vsioss_streaming/foo/bar")
    assert f is None
    assert gdal.VSIGetLastErrorMsg().find("OSS_SECRET_ACCESS_KEY") >= 0


def test_vsioss_1b():

    with gdal.config_option("OSS_SECRET_ACCESS_KEY", "OSS_SECRET_ACCESS_KEY"):

        # Missing OSS_ACCESS_KEY_ID
        gdal.ErrorReset()
        with gdal.quiet_errors():
            f = open_for_read("/vsioss/foo/bar")
        assert f is None
        assert gdal.VSIGetLastErrorMsg().find("OSS_ACCESS_KEY_ID") >= 0


def test_vsioss_real_test():

    gdaltest.skip_on_travis()

    # ERROR 1: The OSS Access Key Id you provided does not exist in our records.
    gdal.ErrorReset()
    with gdal.quiet_errors():
        f = open_for_read("/vsioss/foo/bar.baz")
    if f is not None or gdal.VSIGetLastErrorMsg() == "":
        if f is not None:
            gdal.VSIFCloseL(f)
        if gdal.GetConfigOption("APPVEYOR") is not None:
            return
        pytest.fail(gdal.VSIGetLastErrorMsg())

    gdal.ErrorReset()
    with gdal.quiet_errors():
        f = open_for_read("/vsioss_streaming/foo/bar.baz")
    assert f is None and gdal.VSIGetLastErrorMsg() != ""


###############################################################################


def get_oss_fake_bucket_resource_method(request):
    request.protocol_version = "HTTP/1.1"

    if "Authorization" not in request.headers:
        sys.stderr.write("Bad headers: %s\n" % str(request.headers))
        request.send_response(403)
        return
    expected_authorization = "OSS OSS_ACCESS_KEY_ID:ZFgKjvMtWUwm9CTeCYoPomhuJiE="
    if request.headers["Authorization"] != expected_authorization:
        sys.stderr.write(
            "Bad Authorization: '%s'\n" % str(request.headers["Authorization"])
        )
        request.send_response(403)
        return

    request.send_response(200)
    request.send_header("Content-type", "text/plain")
    request.send_header("Content-Length", 3)
    request.send_header("Connection", "close")
    request.end_headers()
    request.wfile.write("""foo""".encode("ascii"))


###############################################################################
# Test with a fake OSS server


@gdaltest.disable_exceptions()
def test_vsioss_2(server):

    signed_url = gdal.GetSignedURL(
        "/vsioss/oss_fake_bucket/resource", ["START_DATE=20180212T123456Z"]
    )
    port = server.port
    assert (
        signed_url
        == f"http://127.0.0.1:{port}/oss_fake_bucket/resource?Expires=1518442496&OSSAccessKeyId=OSS_ACCESS_KEY_ID&Signature=bpFqur6tQMNN7Xe7UHVFFrugmgs%3D"
    )

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/oss_fake_bucket/resource",
        custom_method=get_oss_fake_bucket_resource_method,
    )

    with webserver.install_http_handler(handler):
        f = open_for_read("/vsioss/oss_fake_bucket/resource")
        assert f is not None
        data = gdal.VSIFReadL(1, 4, f).decode("ascii")
        gdal.VSIFCloseL(f)

        assert data == "foo"

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/oss_fake_bucket/resource",
        custom_method=get_oss_fake_bucket_resource_method,
    )
    with webserver.install_http_handler(handler):
        f = open_for_read("/vsioss_streaming/oss_fake_bucket/resource")
        assert f is not None
        data = gdal.VSIFReadL(1, 4, f).decode("ascii")
        gdal.VSIFCloseL(f)

    assert data == "foo"

    handler = webserver.SequentialHandler()

    def method(request):
        request.protocol_version = "HTTP/1.1"
        request.send_response(400)
        response = """<?xml version="1.0" encoding="UTF-8"?>
<Error>
  <Code>AccessDenied</Code>
  <Message>The bucket you are attempting to access must be addressed using the specified endpoint. Please send all future requests to this endpoint.</Message>
  <HostId>unused</HostId>
  <Bucket>unuset</Bucket>
  <Endpoint>localhost:%d</Endpoint>
</Error>""" % request.server.port
        response = "%x\r\n%s\r\n0\r\n\r\n" % (len(response), response)
        request.send_header("Content-type", "application/xml")
        request.send_header("Transfer-Encoding", "chunked")
        request.send_header("Connection", "close")
        request.end_headers()
        request.wfile.write(response.encode("ascii"))

    handler.add("GET", "/oss_fake_bucket/redirect", custom_method=method)

    def method(request):
        request.protocol_version = "HTTP/1.1"
        if request.headers["Host"].startswith("localhost"):
            request.send_response(200)
            request.send_header("Content-type", "text/plain")
            request.send_header("Content-Length", 3)
            request.send_header("Connection", "close")
            request.end_headers()
            request.wfile.write("""foo""".encode("ascii"))
        else:
            sys.stderr.write("Bad headers: %s\n" % str(request.headers))
            request.send_response(403)

    handler.add("GET", "/oss_fake_bucket/redirect", custom_method=method)

    # Test region and endpoint 'redirects'
    with webserver.install_http_handler(handler):
        f = open_for_read("/vsioss/oss_fake_bucket/redirect")
        assert f is not None
        data = gdal.VSIFReadL(1, 4, f).decode("ascii")
        gdal.VSIFCloseL(f)

    if data != "foo":

        if gdaltest.is_travis_branch("trusty"):
            pytest.skip("Skipped on trusty branch, but should be investigated")

        pytest.fail(data)

    # Test region and endpoint 'redirects'

    handler = webserver.SequentialHandler()

    def method(request):
        request.protocol_version = "HTTP/1.1"
        if request.headers["Host"].startswith("localhost"):
            request.send_response(200)
            request.send_header("Content-type", "text/plain")
            request.send_header("Content-Length", 3)
            request.send_header("Connection", "close")
            request.end_headers()
            request.wfile.write("""foo""".encode("ascii"))
        else:
            sys.stderr.write("Bad headers: %s\n" % str(request.headers))
            request.send_response(403)

    handler.add("GET", "/oss_fake_bucket/redirect", custom_method=method)

    with webserver.install_http_handler(handler):
        f = open_for_read("/vsioss_streaming/oss_fake_bucket/redirect")
        assert f is not None
        data = gdal.VSIFReadL(1, 4, f).decode("ascii")
        gdal.VSIFCloseL(f)

    assert data == "foo"

    handler = webserver.SequentialHandler()

    def method(request):
        # /vsioss_streaming/ should have remembered the change of region and endpoint
        if not request.headers["Host"].startswith("localhost"):
            sys.stderr.write("Bad headers: %s\n" % str(request.headers))
            request.send_response(403)

        request.protocol_version = "HTTP/1.1"
        request.send_response(400)
        response = "bla"
        response = "%x\r\n%s\r\n0\r\n\r\n" % (len(response), response)
        request.send_header("Content-type", "application/xml")
        request.send_header("Transfer-Encoding", "chunked")
        request.send_header("Connection", "close")
        request.end_headers()
        request.wfile.write(response.encode("ascii"))

    handler.add("GET", "/oss_fake_bucket/non_xml_error", custom_method=method)

    gdal.ErrorReset()
    with webserver.install_http_handler(handler):
        with gdal.quiet_errors():
            f = open_for_read("/vsioss_streaming/oss_fake_bucket/non_xml_error")
    assert f is None and gdal.VSIGetLastErrorMsg().find("bla") >= 0

    handler = webserver.SequentialHandler()
    response = '<?xml version="1.0" encoding="UTF-8"?><oops>'
    response = "%x\r\n%s\r\n0\r\n\r\n" % (len(response), response)
    handler.add(
        "GET",
        "/oss_fake_bucket/invalid_xml_error",
        400,
        {
            "Content-type": "application/xml",
            "Transfer-Encoding": "chunked",
            "Connection": "close",
        },
        response,
    )
    gdal.ErrorReset()
    with webserver.install_http_handler(handler):
        with gdal.quiet_errors():
            f = open_for_read("/vsioss_streaming/oss_fake_bucket/invalid_xml_error")
    assert f is None and gdal.VSIGetLastErrorMsg().find("<oops>") >= 0

    handler = webserver.SequentialHandler()
    response = '<?xml version="1.0" encoding="UTF-8"?><Error/>'
    response = "%x\r\n%s\r\n0\r\n\r\n" % (len(response), response)
    handler.add(
        "GET",
        "/oss_fake_bucket/no_code_in_error",
        400,
        {
            "Content-type": "application/xml",
            "Transfer-Encoding": "chunked",
            "Connection": "close",
        },
        response,
    )
    gdal.ErrorReset()
    with webserver.install_http_handler(handler):
        with gdal.quiet_errors():
            f = open_for_read("/vsioss_streaming/oss_fake_bucket/no_code_in_error")
    assert f is None and gdal.VSIGetLastErrorMsg().find("<Error/>") >= 0

    handler = webserver.SequentialHandler()
    response = '<?xml version="1.0" encoding="UTF-8"?><Error><Code>AuthorizationHeaderMalformed</Code></Error>'
    response = "%x\r\n%s\r\n0\r\n\r\n" % (len(response), response)
    handler.add(
        "GET",
        "/oss_fake_bucket/no_region_in_AuthorizationHeaderMalformed_error",
        400,
        {
            "Content-type": "application/xml",
            "Transfer-Encoding": "chunked",
            "Connection": "close",
        },
        response,
    )
    gdal.ErrorReset()
    with webserver.install_http_handler(handler):
        with gdal.quiet_errors():
            f = open_for_read(
                "/vsioss_streaming/oss_fake_bucket/no_region_in_AuthorizationHeaderMalformed_error"
            )
    assert f is None and gdal.VSIGetLastErrorMsg().find("<Error>") >= 0

    handler = webserver.SequentialHandler()
    response = '<?xml version="1.0" encoding="UTF-8"?><Error><Code>PermanentRedirect</Code></Error>'
    response = "%x\r\n%s\r\n0\r\n\r\n" % (len(response), response)
    handler.add(
        "GET",
        "/oss_fake_bucket/no_endpoint_in_PermanentRedirect_error",
        400,
        {
            "Content-type": "application/xml",
            "Transfer-Encoding": "chunked",
            "Connection": "close",
        },
        response,
    )
    gdal.ErrorReset()
    with webserver.install_http_handler(handler):
        with gdal.quiet_errors():
            f = open_for_read(
                "/vsioss_streaming/oss_fake_bucket/no_endpoint_in_PermanentRedirect_error"
            )
    assert f is None and gdal.VSIGetLastErrorMsg().find("<Error>") >= 0

    handler = webserver.SequentialHandler()
    response = '<?xml version="1.0" encoding="UTF-8"?><Error><Code>bla</Code></Error>'
    response = "%x\r\n%s\r\n0\r\n\r\n" % (len(response), response)
    handler.add(
        "GET",
        "/oss_fake_bucket/no_message_in_error",
        400,
        {
            "Content-type": "application/xml",
            "Transfer-Encoding": "chunked",
            "Connection": "close",
        },
        response,
    )
    gdal.ErrorReset()
    with webserver.install_http_handler(handler):
        with gdal.quiet_errors():
            f = open_for_read("/vsioss_streaming/oss_fake_bucket/no_message_in_error")
    assert f is None and gdal.VSIGetLastErrorMsg().find("<Error>") >= 0


###############################################################################
# Test ReadDir() with a fake OSS server


def test_vsioss_3(server):

    handler = webserver.SequentialHandler()

    def method(request):
        request.protocol_version = "HTTP/1.1"
        request.send_response(200)
        request.send_header("Content-type", "application/xml")
        response = """<?xml version="1.0" encoding="UTF-8"?>
            <ListBucketResult>
                <Prefix>a_dir/</Prefix>
                <NextContinuationToken>bla</NextContinuationToken>
                <Contents>
                    <Key>a_dir/resource3.bin</Key>
                    <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                    <Size>123456</Size>
                </Contents>
            </ListBucketResult>
        """
        request.send_header("Content-Length", len(response))
        request.end_headers()
        request.wfile.write(response.encode("ascii"))

    handler.add(
        "GET",
        "/oss_fake_bucket2/?delimiter=%2F&list-type=2&prefix=a_dir%2F",
        custom_method=method,
    )

    def method(request):
        request.protocol_version = "HTTP/1.1"
        request.send_response(200)
        request.send_header("Content-type", "application/xml")
        response = """<?xml version="1.0" encoding="UTF-8"?>
            <ListBucketResult>
                <Prefix>a_dir/</Prefix>
                <Contents>
                    <Key>a_dir/resource4.bin</Key>
                    <LastModified>2015-10-16T12:34:56.000Z</LastModified>
                    <Size>456789</Size>
                </Contents>
                <CommonPrefixes>
                    <Prefix>a_dir/subdir/</Prefix>
                </CommonPrefixes>
            </ListBucketResult>
        """
        request.send_header("Content-Length", len(response))
        request.end_headers()
        request.wfile.write(response.encode("ascii"))

    handler.add(
        "GET",
        "/oss_fake_bucket2/?continuation-token=bla&delimiter=%2F&list-type=2&prefix=a_dir%2F",
        custom_method=method,
    )

    with webserver.install_http_handler(handler):
        f = open_for_read("/vsioss/oss_fake_bucket2/a_dir/resource3.bin")
    if f is None:

        if gdaltest.is_travis_branch("trusty"):
            pytest.skip("Skipped on trusty branch, but should be investigated")

        pytest.fail()
    gdal.VSIFCloseL(f)

    with webserver.install_http_handler(webserver.SequentialHandler()):
        dir_contents = gdal.ReadDir("/vsioss/oss_fake_bucket2/a_dir")
    assert dir_contents == ["resource3.bin", "resource4.bin", "subdir"]
    assert gdal.VSIStatL("/vsioss/oss_fake_bucket2/a_dir/resource3.bin").size == 123456
    assert gdal.VSIStatL("/vsioss/oss_fake_bucket2/a_dir/resource3.bin").mtime == 1

    # ReadDir on something known to be a file shouldn't cause network access
    dir_contents = gdal.ReadDir("/vsioss/oss_fake_bucket2/a_dir/resource3.bin")
    assert dir_contents is None

    # Test CPL_VSIL_CURL_NON_CACHED
    for config_option_value in [
        "/vsioss/oss_non_cached/test.txt",
        "/vsioss/oss_non_cached",
        "/vsioss/oss_non_cached:/vsioss/unrelated",
        "/vsioss/unrelated:/vsioss/oss_non_cached",
        "/vsioss/unrelated:/vsioss/oss_non_cached:/vsioss/unrelated",
    ]:
        with gdaltest.config_option("CPL_VSIL_CURL_NON_CACHED", config_option_value):

            handler = webserver.SequentialHandler()
            handler.add("GET", "/oss_non_cached/test.txt", 200, {}, "foo")
            handler.add("GET", "/oss_non_cached/test.txt", 200, {}, "foo")

            with webserver.install_http_handler(handler):
                f = open_for_read("/vsioss/oss_non_cached/test.txt")
                assert f is not None, config_option_value
                data = gdal.VSIFReadL(1, 3, f).decode("ascii")
                gdal.VSIFCloseL(f)
                assert data == "foo", config_option_value

            handler = webserver.SequentialHandler()
            handler.add("GET", "/oss_non_cached/test.txt", 200, {}, "bar2")

            with webserver.install_http_handler(handler):
                size = gdal.VSIStatL("/vsioss/oss_non_cached/test.txt").size
            assert size == 4, config_option_value

            handler = webserver.SequentialHandler()
            handler.add("GET", "/oss_non_cached/test.txt", 200, {}, "foo")

            with webserver.install_http_handler(handler):
                size = gdal.VSIStatL("/vsioss/oss_non_cached/test.txt").size
                if size != 3:
                    print(config_option_value)
                    pytest.fail(data)

            handler = webserver.SequentialHandler()
            handler.add("GET", "/oss_non_cached/test.txt", 200, {}, "bar2")
            handler.add("GET", "/oss_non_cached/test.txt", 200, {}, "bar2")

            with webserver.install_http_handler(handler):
                f = open_for_read("/vsioss/oss_non_cached/test.txt")
                assert f is not None, config_option_value
                data = gdal.VSIFReadL(1, 4, f).decode("ascii")
                gdal.VSIFCloseL(f)
                assert data == "bar2", config_option_value

    # Retry without option
    for config_option_value in [None, "/vsioss/oss_non_cached/bar.txt"]:
        with gdaltest.config_option("CPL_VSIL_CURL_NON_CACHED", config_option_value):

            handler = webserver.SequentialHandler()
            if config_option_value is None:
                handler.add(
                    "GET",
                    "/oss_non_cached/?delimiter=%2F&list-type=2",
                    200,
                    {"Content-type": "application/xml"},
                    """<?xml version="1.0" encoding="UTF-8"?>
                        <ListBucketResult>
                            <Prefix>/</Prefix>
                            <Contents>
                                <Key>/test.txt</Key>
                                <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                                <Size>40</Size>
                            </Contents>
                            <Contents>
                                <Key>/test2.txt</Key>
                                <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                                <Size>40</Size>
                            </Contents>
                        </ListBucketResult>
                    """,
                )
                handler.add("GET", "/oss_non_cached/test.txt", 200, {}, "foo")

            with webserver.install_http_handler(handler):
                f = open_for_read("/vsioss/oss_non_cached/test.txt")
                assert f is not None, config_option_value
                data = gdal.VSIFReadL(1, 3, f).decode("ascii")
                gdal.VSIFCloseL(f)
                assert data == "foo", config_option_value

            handler = webserver.SequentialHandler()
            with webserver.install_http_handler(handler):
                f = open_for_read("/vsioss/oss_non_cached/test.txt")
                assert f is not None, config_option_value
                data = gdal.VSIFReadL(1, 4, f).decode("ascii")
                gdal.VSIFCloseL(f)
                # We should still get foo because of caching
                assert data == "foo", config_option_value

    # List buckets (empty result)
    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/",
        200,
        {"Content-type": "application/xml"},
        """<?xml version="1.0" encoding="UTF-8"?>
        <ListAllMyBucketsResult>
        <Buckets>
        </Buckets>
        </ListAllMyBucketsResult>
        """,
    )
    with webserver.install_http_handler(handler):
        dir_contents = gdal.ReadDir("/vsioss/")
    assert dir_contents == ["."]

    gdal.VSICurlClearCache()

    # List buckets
    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/",
        200,
        {"Content-type": "application/xml"},
        """<?xml version="1.0" encoding="UTF-8"?>
        <ListAllMyBucketsResult>
        <Buckets>
            <Bucket>
                <Name>mybucket</Name>
            </Bucket>
        </Buckets>
        </ListAllMyBucketsResult>
        """,
    )
    with webserver.install_http_handler(handler):
        dir_contents = gdal.ReadDir("/vsioss/")
    assert dir_contents == ["mybucket"]


###############################################################################
# Test simple PUT support with a fake OSS server


@gdaltest.disable_exceptions()
def test_vsioss_4(server):

    with webserver.install_http_handler(webserver.SequentialHandler()):
        with gdal.quiet_errors():
            f = gdal.VSIFOpenL("/vsioss/oss_fake_bucket3", "wb")
    assert f is None

    handler = webserver.SequentialHandler()
    handler.add(
        "GET", "/oss_fake_bucket3/empty_file.bin", 200, {"Connection": "close"}, "foo"
    )
    with webserver.install_http_handler(handler):
        assert gdal.VSIStatL("/vsioss/oss_fake_bucket3/empty_file.bin").size == 3

    # Empty file
    handler = webserver.SequentialHandler()

    def method(request):
        if request.headers["Content-Length"] != "0":
            sys.stderr.write(
                "Did not get expected headers: %s\n" % str(request.headers)
            )
            request.send_response(400)
            return

        request.send_response(200)
        request.send_header("Content-Length", 0)
        request.end_headers()

    handler.add("PUT", "/oss_fake_bucket3/empty_file.bin", custom_method=method)

    with webserver.install_http_handler(handler):
        f = gdal.VSIFOpenL("/vsioss/oss_fake_bucket3/empty_file.bin", "wb")
        assert f is not None
        gdal.ErrorReset()
        gdal.VSIFCloseL(f)
    assert gdal.GetLastErrorMsg() == ""

    handler = webserver.SequentialHandler()
    handler.add(
        "GET", "/oss_fake_bucket3/empty_file.bin", 200, {"Connection": "close"}, ""
    )
    with webserver.install_http_handler(handler):
        assert gdal.VSIStatL("/vsioss/oss_fake_bucket3/empty_file.bin").size == 0

    # Invalid seek
    handler = webserver.SequentialHandler()
    with webserver.install_http_handler(handler):
        f = gdal.VSIFOpenL("/vsioss/oss_fake_bucket3/empty_file.bin", "wb")
        assert f is not None
        with gdal.quiet_errors():
            ret = gdal.VSIFSeekL(f, 1, 0)
        assert ret != 0
        gdal.VSIFCloseL(f)

    # Invalid read
    handler = webserver.SequentialHandler()
    with webserver.install_http_handler(handler):
        f = gdal.VSIFOpenL("/vsioss/oss_fake_bucket3/empty_file.bin", "wb")
        assert f is not None
        with gdal.quiet_errors():
            ret = gdal.VSIFReadL(1, 1, f)
        assert not ret
        gdal.VSIFCloseL(f)

    # Error case
    handler = webserver.SequentialHandler()
    handler.add("PUT", "/oss_fake_bucket3/empty_file_error.bin", 403)
    with webserver.install_http_handler(handler):
        f = gdal.VSIFOpenL("/vsioss/oss_fake_bucket3/empty_file_error.bin", "wb")
        assert f is not None
        gdal.ErrorReset()
        with gdal.quiet_errors():
            gdal.VSIFCloseL(f)
    assert gdal.GetLastErrorMsg() != ""

    # Nominal case
    with webserver.install_http_handler(webserver.SequentialHandler()):
        f = gdal.VSIFOpenL("/vsioss/oss_fake_bucket3/another_file.bin", "wb")
        assert f is not None
        assert gdal.VSIFSeekL(f, gdal.VSIFTellL(f), 0) == 0
        assert gdal.VSIFSeekL(f, 0, 1) == 0
        assert gdal.VSIFSeekL(f, 0, 2) == 0
        assert gdal.VSIFWriteL("foo", 1, 3, f) == 3
        assert gdal.VSIFSeekL(f, gdal.VSIFTellL(f), 0) == 0
        assert gdal.VSIFWriteL("bar", 1, 3, f) == 3

    handler = webserver.SequentialHandler()

    def method(request):
        if request.headers["Content-Length"] != "6":
            sys.stderr.write(
                "Did not get expected headers: %s\n" % str(request.headers)
            )
            request.send_response(400)
            request.send_header("Content-Length", 0)
            request.end_headers()
            return

        request.wfile.write("HTTP/1.1 100 Continue\r\n\r\n".encode("ascii"))

        content = request.rfile.read(6).decode("ascii")
        if content != "foobar":
            sys.stderr.write("Did not get expected content: %s\n" % content)
            request.send_response(400)
            request.send_header("Content-Length", 0)
            request.end_headers()
            return

        request.send_response(200)
        request.send_header("Content-Length", 0)
        request.end_headers()

    handler.add("PUT", "/oss_fake_bucket3/another_file.bin", custom_method=method)

    gdal.ErrorReset()
    with webserver.install_http_handler(handler):
        gdal.VSIFCloseL(f)
    assert gdal.GetLastErrorMsg() == ""


###############################################################################
# Test simple DELETE support with a fake OSS server


@gdaltest.disable_exceptions()
def test_vsioss_5(server):

    with webserver.install_http_handler(webserver.SequentialHandler()):
        with gdal.quiet_errors():
            ret = gdal.Unlink("/vsioss/foo")
    assert ret != 0

    handler = webserver.SequentialHandler()
    handler.add(
        "GET", "/oss_delete_bucket/delete_file", 200, {"Connection": "close"}, "foo"
    )
    with webserver.install_http_handler(handler):
        assert gdal.VSIStatL("/vsioss/oss_delete_bucket/delete_file").size == 3

    with webserver.install_http_handler(webserver.SequentialHandler()):
        assert gdal.VSIStatL("/vsioss/oss_delete_bucket/delete_file").size == 3

    handler = webserver.SequentialHandler()
    handler.add("DELETE", "/oss_delete_bucket/delete_file", 204)
    with webserver.install_http_handler(handler):
        ret = gdal.Unlink("/vsioss/oss_delete_bucket/delete_file")
    assert ret == 0

    handler = webserver.SequentialHandler()
    handler.add(
        "GET", "/oss_delete_bucket/delete_file", 404, {"Connection": "close"}, "foo"
    )
    handler.add(
        "GET",
        "/oss_delete_bucket/?delimiter=%2F&list-type=2&max-keys=100&prefix=delete_file%2F",
        404,
        {"Connection": "close"},
        "foo",
    )
    with webserver.install_http_handler(handler):
        assert gdal.VSIStatL("/vsioss/oss_delete_bucket/delete_file") is None

    handler = webserver.SequentialHandler()
    handler.add("GET", "/oss_delete_bucket/delete_file_error", 200)
    handler.add("DELETE", "/oss_delete_bucket/delete_file_error", 403)
    with webserver.install_http_handler(handler):
        with gdal.quiet_errors():
            ret = gdal.Unlink("/vsioss/oss_delete_bucket/delete_file_error")
    assert ret != 0


###############################################################################
# Test multipart upload with a fake OSS server


@pytest.mark.skipif(
    "CI" in os.environ,
    reason="Flaky",
)
@gdaltest.disable_exceptions()
def test_vsioss_6(server):

    with gdaltest.config_option("VSIOSS_CHUNK_SIZE", "1"):  # 1 MB
        with webserver.install_http_handler(webserver.SequentialHandler()):
            f = gdal.VSIFOpenL("/vsioss/oss_fake_bucket4/large_file.bin", "wb")
    assert f is not None
    size = 1024 * 1024 + 1
    big_buffer = "a" * size

    handler = webserver.SequentialHandler()

    def method(request):
        request.protocol_version = "HTTP/1.1"
        response = '<?xml version="1.0" encoding="UTF-8"?><InitiateMultipartUploadResult><UploadId>my_id</UploadId></InitiateMultipartUploadResult>'
        request.send_response(200)
        request.send_header("Content-type", "application/xml")
        request.send_header("Content-Length", len(response))
        request.end_headers()
        request.wfile.write(response.encode("ascii"))

    handler.add(
        "POST", "/oss_fake_bucket4/large_file.bin?uploads", custom_method=method
    )

    def method(request):
        if request.headers["Content-Length"] != "1048576":
            sys.stderr.write(
                "Did not get expected headers: %s\n" % str(request.headers)
            )
            request.send_response(400)
            request.send_header("Content-Length", 0)
            request.end_headers()
            return
        request.send_response(200)
        request.send_header("ETag", '"first_etag"')
        request.send_header("Content-Length", 0)
        request.end_headers()

    handler.add(
        "PUT",
        "/oss_fake_bucket4/large_file.bin?partNumber=1&uploadId=my_id",
        custom_method=method,
    )

    with webserver.install_http_handler(handler):
        ret = gdal.VSIFWriteL(big_buffer, 1, size, f)
    assert ret == size
    handler = webserver.SequentialHandler()

    def method(request):
        if request.headers["Content-Length"] != "1":
            sys.stderr.write(
                "Did not get expected headers: %s\n" % str(request.headers)
            )
            request.send_response(400)
            return
        request.send_response(200)
        request.send_header("ETag", '"second_etag"')
        request.send_header("Content-Length", 0)
        request.end_headers()

    handler.add(
        "PUT",
        "/oss_fake_bucket4/large_file.bin?partNumber=2&uploadId=my_id",
        custom_method=method,
    )

    def method(request):

        if request.headers["Content-Length"] != "186":
            sys.stderr.write(
                "Did not get expected headers: %s\n" % str(request.headers)
            )
            request.send_response(400)
            request.send_header("Content-Length", 0)
            request.end_headers()
            return

        content = request.rfile.read(186).decode("ascii")
        if content != """<CompleteMultipartUpload>
<Part>
<PartNumber>1</PartNumber><ETag>"first_etag"</ETag></Part>
<Part>
<PartNumber>2</PartNumber><ETag>"second_etag"</ETag></Part>
</CompleteMultipartUpload>
""":
            sys.stderr.write("Did not get expected content: %s\n" % content)
            request.send_response(400)
            request.send_header("Content-Length", 0)
            request.end_headers()
            return

        request.send_response(200)
        request.send_header("Content-Length", 0)
        request.end_headers()

    handler.add(
        "POST", "/oss_fake_bucket4/large_file.bin?uploadId=my_id", custom_method=method
    )

    gdal.ErrorReset()
    with webserver.install_http_handler(handler):
        gdal.VSIFCloseL(f)
    assert gdal.GetLastErrorMsg() == ""

    handler = webserver.SequentialHandler()
    handler.add(
        "POST", "/oss_fake_bucket4/large_file_initiate_403_error.bin?uploads", 403
    )
    handler.add(
        "POST", "/oss_fake_bucket4/large_file_initiate_empty_result.bin?uploads", 200
    )
    handler.add(
        "POST",
        "/oss_fake_bucket4/large_file_initiate_invalid_xml_result.bin?uploads",
        200,
        {},
        "foo",
    )
    handler.add(
        "POST",
        "/oss_fake_bucket4/large_file_initiate_no_uploadId.bin?uploads",
        200,
        {},
        "<foo/>",
    )
    with webserver.install_http_handler(handler):
        for filename in [
            "/vsioss/oss_fake_bucket4/large_file_initiate_403_error.bin",
            "/vsioss/oss_fake_bucket4/large_file_initiate_empty_result.bin",
            "/vsioss/oss_fake_bucket4/large_file_initiate_invalid_xml_result.bin",
            "/vsioss/oss_fake_bucket4/large_file_initiate_no_uploadId.bin",
        ]:
            with gdaltest.config_option("VSIOSS_CHUNK_SIZE", "1"):  # 1 MB
                f = gdal.VSIFOpenL(filename, "wb")
            assert f is not None
            with gdal.quiet_errors():
                ret = gdal.VSIFWriteL(big_buffer, 1, size, f)
            assert ret == 0
            gdal.ErrorReset()
            gdal.VSIFCloseL(f)
            assert gdal.GetLastErrorMsg() == ""

    handler = webserver.SequentialHandler()
    handler.add(
        "POST",
        "/oss_fake_bucket4/large_file_upload_part_403_error.bin?uploads",
        200,
        {},
        '<?xml version="1.0" encoding="UTF-8"?><InitiateMultipartUploadResult><UploadId>my_id</UploadId></InitiateMultipartUploadResult>',
    )
    handler.add(
        "PUT",
        "/oss_fake_bucket4/large_file_upload_part_403_error.bin?partNumber=1&uploadId=my_id",
        403,
    )
    handler.add(
        "DELETE",
        "/oss_fake_bucket4/large_file_upload_part_403_error.bin?uploadId=my_id",
        204,
    )

    handler.add(
        "POST",
        "/oss_fake_bucket4/large_file_upload_part_no_etag.bin?uploads",
        200,
        {},
        '<?xml version="1.0" encoding="UTF-8"?><InitiateMultipartUploadResult><UploadId>my_id</UploadId></InitiateMultipartUploadResult>',
    )
    handler.add(
        "PUT",
        "/oss_fake_bucket4/large_file_upload_part_no_etag.bin?partNumber=1&uploadId=my_id",
        200,
    )
    handler.add(
        "DELETE",
        "/oss_fake_bucket4/large_file_upload_part_no_etag.bin?uploadId=my_id",
        204,
    )

    with webserver.install_http_handler(handler):
        for filename in [
            "/vsioss/oss_fake_bucket4/large_file_upload_part_403_error.bin",
            "/vsioss/oss_fake_bucket4/large_file_upload_part_no_etag.bin",
        ]:
            with gdaltest.config_option("VSIOSS_CHUNK_SIZE", "1"):  # 1 MB
                f = gdal.VSIFOpenL(filename, "wb")
            assert f is not None, filename
            with gdal.quiet_errors():
                ret = gdal.VSIFWriteL(big_buffer, 1, size, f)
            assert ret == 0, filename
            gdal.ErrorReset()
            gdal.VSIFCloseL(f)
            assert gdal.GetLastErrorMsg() == "", filename

    # Simulate failure in AbortMultipart stage
    handler = webserver.SequentialHandler()
    handler.add(
        "POST",
        "/oss_fake_bucket4/large_file_abortmultipart_403_error.bin?uploads",
        200,
        {},
        '<?xml version="1.0" encoding="UTF-8"?><InitiateMultipartUploadResult><UploadId>my_id</UploadId></InitiateMultipartUploadResult>',
    )
    handler.add(
        "PUT",
        "/oss_fake_bucket4/large_file_abortmultipart_403_error.bin?partNumber=1&uploadId=my_id",
        403,
    )
    handler.add(
        "DELETE",
        "/oss_fake_bucket4/large_file_abortmultipart_403_error.bin?uploadId=my_id",
        403,
    )

    filename = "/vsioss/oss_fake_bucket4/large_file_abortmultipart_403_error.bin"
    with webserver.install_http_handler(handler):
        with gdaltest.config_option("VSIOSS_CHUNK_SIZE", "1"):  # 1 MB
            f = gdal.VSIFOpenL(filename, "wb")
        assert f is not None, filename
        with gdal.quiet_errors():
            ret = gdal.VSIFWriteL(big_buffer, 1, size, f)
        assert ret == 0, filename
        gdal.ErrorReset()
        with gdal.quiet_errors():
            gdal.VSIFCloseL(f)
        assert gdal.GetLastErrorMsg() != "", filename

    # Simulate failure in CompleteMultipartUpload stage
    handler = webserver.SequentialHandler()
    handler.add(
        "POST",
        "/oss_fake_bucket4/large_file_completemultipart_403_error.bin?uploads",
        200,
        {},
        '<?xml version="1.0" encoding="UTF-8"?><InitiateMultipartUploadResult><UploadId>my_id</UploadId></InitiateMultipartUploadResult>',
    )
    handler.add(
        "PUT",
        "/oss_fake_bucket4/large_file_completemultipart_403_error.bin?partNumber=1&uploadId=my_id",
        200,
        {"ETag": "first_etag"},
        "",
    )
    handler.add(
        "PUT",
        "/oss_fake_bucket4/large_file_completemultipart_403_error.bin?partNumber=2&uploadId=my_id",
        200,
        {"ETag": "second_etag"},
        "",
    )
    handler.add(
        "POST",
        "/oss_fake_bucket4/large_file_completemultipart_403_error.bin?uploadId=my_id",
        403,
    )
    # handler.add('DELETE', '/oss_fake_bucket4/large_file_completemultipart_403_error.bin?uploadId=my_id', 204)

    filename = "/vsioss/oss_fake_bucket4/large_file_completemultipart_403_error.bin"
    with webserver.install_http_handler(handler):
        with gdaltest.config_option("VSIOSS_CHUNK_SIZE", "1"):  # 1 MB
            f = gdal.VSIFOpenL(filename, "wb")
            assert f is not None, filename
            ret = gdal.VSIFWriteL(big_buffer, 1, size, f)
            assert ret == size, filename
            gdal.ErrorReset()
            with gdal.quiet_errors():
                gdal.VSIFCloseL(f)
            assert gdal.GetLastErrorMsg() != "", filename


###############################################################################
# Test VSIMultipartUploadXXXX()


def test_vsioss_MultipartUpload(server):

    # Test MultipartUploadGetCapabilities()
    info = gdal.MultipartUploadGetCapabilities("/vsioss/")
    assert info.non_sequential_upload_supported
    assert info.parallel_upload_supported
    assert info.abort_supported
    assert info.min_part_size == 5
    assert info.max_part_size >= 1024
    assert info.max_part_count == 10000


###############################################################################
# Test Mkdir() / Rmdir()


@gdaltest.disable_exceptions()
def test_vsioss_7(server):

    handler = webserver.SequentialHandler()
    handler.add("GET", "/oss_bucket_test_mkdir/dir/", 404, {"Connection": "close"})
    handler.add(
        "GET",
        "/oss_bucket_test_mkdir/?delimiter=%2F&list-type=2&max-keys=100&prefix=dir%2F",
        404,
        {"Connection": "close"},
    )
    handler.add("PUT", "/oss_bucket_test_mkdir/dir/", 200)
    with webserver.install_http_handler(handler):
        ret = gdal.Mkdir("/vsioss/oss_bucket_test_mkdir/dir", 0)
    assert ret == 0

    # Try creating already existing directory
    handler = webserver.SequentialHandler()
    handler.add("GET", "/oss_bucket_test_mkdir/dir/", 416)
    with webserver.install_http_handler(handler):
        ret = gdal.Mkdir("/vsioss/oss_bucket_test_mkdir/dir", 0)
    assert ret != 0

    handler = webserver.SequentialHandler()
    handler.add("DELETE", "/oss_bucket_test_mkdir/dir/", 204)
    with webserver.install_http_handler(handler):
        ret = gdal.Rmdir("/vsioss/oss_bucket_test_mkdir/dir")
    assert ret == 0

    # Try deleting already deleted directory
    handler = webserver.SequentialHandler()
    handler.add("GET", "/oss_bucket_test_mkdir/dir/", 404)
    handler.add(
        "GET",
        "/oss_bucket_test_mkdir/?delimiter=%2F&list-type=2&max-keys=100&prefix=dir%2F",
        404,
        {"Connection": "close"},
    )
    with webserver.install_http_handler(handler):
        ret = gdal.Rmdir("/vsioss/oss_bucket_test_mkdir/dir")
    assert ret != 0

    # Try deleting non-empty directory
    handler = webserver.SequentialHandler()
    handler.add("GET", "/oss_bucket_test_mkdir/dir_nonempty/", 416)
    handler.add(
        "GET",
        "/oss_bucket_test_mkdir/?delimiter=%2F&list-type=2&max-keys=100&prefix=dir_nonempty%2F",
        200,
        {"Content-type": "application/xml"},
        """<?xml version="1.0" encoding="UTF-8"?>
                    <ListBucketResult>
                        <Prefix>dir_nonempty/</Prefix>
                        <Contents>
                            <Key>dir_nonempty/test.txt</Key>
                            <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                            <Size>40</Size>
                        </Contents>
                    </ListBucketResult>
                """,
    )
    with webserver.install_http_handler(handler):
        ret = gdal.Rmdir("/vsioss/oss_bucket_test_mkdir/dir_nonempty")
    assert ret != 0


###############################################################################
# Test handling of file and directory with same name


def test_vsioss_8(server):

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/vsioss_8/?delimiter=%2F&list-type=2",
        200,
        {"Content-type": "application/xml"},
        """<?xml version="1.0" encoding="UTF-8"?>
                    <ListBucketResult>
                        <Prefix></Prefix>
                        <Contents>
                            <Key>test</Key>
                            <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                            <Size>40</Size>
                        </Contents>
                        <CommonPrefixes>
                            <Prefix>test/</Prefix>
                        </CommonPrefixes>
                    </ListBucketResult>
                """,
    )

    with webserver.install_http_handler(handler):
        listdir = gdal.ReadDir("/vsioss/vsioss_8", 0)
    assert listdir == ["test", "test/"]

    handler = webserver.SequentialHandler()
    with webserver.install_http_handler(handler):
        assert not stat.S_ISDIR(gdal.VSIStatL("/vsioss/vsioss_8/test").mode)

    handler = webserver.SequentialHandler()
    with webserver.install_http_handler(handler):
        assert stat.S_ISDIR(gdal.VSIStatL("/vsioss/vsioss_8/test/").mode)


###############################################################################
# Nominal cases (require valid credentials)


def test_vsioss_extra_1():

    # Either a bucket name or bucket/filename
    OSS_RESOURCE = gdal.GetConfigOption("OSS_RESOURCE")

    if gdal.GetConfigOption("OSS_SECRET_ACCESS_KEY") is None:
        pytest.skip("Missing OSS_SECRET_ACCESS_KEY")
    elif gdal.GetConfigOption("OSS_ACCESS_KEY_ID") is None:
        pytest.skip("Missing OSS_ACCESS_KEY_ID")
    elif OSS_RESOURCE is None:
        pytest.skip("Missing OSS_RESOURCE")

    if "/" not in OSS_RESOURCE:
        path = "/vsioss/" + OSS_RESOURCE
        statres = gdal.VSIStatL(path)
        assert statres is not None and stat.S_ISDIR(statres.mode), (
            "%s is not a valid bucket" % path
        )

        readdir = gdal.ReadDir(path)
        assert readdir is not None, "ReadDir() should not return empty list"
        for filename in readdir:
            if filename != ".":
                subpath = path + "/" + filename
                assert gdal.VSIStatL(subpath) is not None, (
                    "Stat(%s) should not return an error" % subpath
                )

        unique_id = "vsioss_test"
        subpath = path + "/" + unique_id
        ret = gdal.Mkdir(subpath, 0)
        assert ret >= 0, "Mkdir(%s) should not return an error" % subpath

        readdir = gdal.ReadDir(path)
        assert unique_id in readdir, "ReadDir(%s) should contain %s" % (path, unique_id)

        ret = gdal.Mkdir(subpath, 0)
        assert ret != 0, "Mkdir(%s) repeated should return an error" % subpath

        ret = gdal.Rmdir(subpath)
        assert ret >= 0, "Rmdir(%s) should not return an error" % subpath

        readdir = gdal.ReadDir(path)
        assert unique_id not in readdir, "ReadDir(%s) should not contain %s" % (
            path,
            unique_id,
        )

        ret = gdal.Rmdir(subpath)
        assert ret != 0, "Rmdir(%s) repeated should return an error" % subpath

        ret = gdal.Mkdir(subpath, 0)
        assert ret >= 0, "Mkdir(%s) should not return an error" % subpath

        f = gdal.VSIFOpenL(subpath + "/test.txt", "wb")
        assert f is not None
        gdal.VSIFWriteL("hello", 1, 5, f)
        gdal.VSIFCloseL(f)

        ret = gdal.Rmdir(subpath)
        assert ret != 0, (
            "Rmdir(%s) on non empty directory should return an error" % subpath
        )

        f = gdal.VSIFOpenL(subpath + "/test.txt", "rb")
        assert f is not None
        data = gdal.VSIFReadL(1, 5, f).decode("utf-8")
        assert data == "hello"
        gdal.VSIFCloseL(f)

        ret = gdal.Unlink(subpath + "/test.txt")
        assert ret >= 0, "Unlink(%s) should not return an error" % (
            subpath + "/test.txt"
        )

        ret = gdal.Rmdir(subpath)
        assert ret >= 0, "Rmdir(%s) should not return an error" % subpath

        return

    f = open_for_read("/vsioss/" + OSS_RESOURCE)
    assert f is not None, "cannot open %s" % ("/vsioss/" + OSS_RESOURCE)
    ret = gdal.VSIFReadL(1, 1, f)
    gdal.VSIFCloseL(f)

    assert len(ret) == 1

    # Same with /vsioss_streaming/
    f = open_for_read("/vsioss_streaming/" + OSS_RESOURCE)
    assert f is not None
    ret = gdal.VSIFReadL(1, 1, f)
    gdal.VSIFCloseL(f)

    assert len(ret) == 1

    if False:  # pylint: disable=using-constant-test
        # we actually try to read at read() time and bSetError = false:
        # Invalid bucket : "The specified bucket does not exist"
        gdal.ErrorReset()
        f = open_for_read("/vsioss/not_existing_bucket/foo")
        with gdal.quiet_errors():
            gdal.VSIFReadL(1, 1, f)
        gdal.VSIFCloseL(f)
        assert gdal.VSIGetLastErrorMsg() != ""

    # Invalid resource
    gdal.ErrorReset()
    f = open_for_read("/vsioss_streaming/" + OSS_RESOURCE + "/invalid_resource.baz")
    assert f is None, gdal.VSIGetLastErrorMsg()

    # Test GetSignedURL()
    signed_url = gdal.GetSignedURL("/vsioss/" + OSS_RESOURCE)
    f = open_for_read("/vsicurl_streaming/" + signed_url)
    assert f is not None
    ret = gdal.VSIFReadL(1, 1, f)
    gdal.VSIFCloseL(f)

    assert len(ret) == 1
