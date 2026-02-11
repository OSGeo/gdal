#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test /vsis3
# Author:   Even Rouault <even dot rouault at spatialys dot com>
#
###############################################################################
# Copyright (c) 2015, Even Rouault <even dot rouault at spatialys dot com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import copy
import json
import os
import os.path
import stat
import sys
import tempfile
import urllib

import gdaltest
import pytest
import webserver

from osgeo import gdal

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


general_s3_options = {
    # To avoid user AWS credentials in ~/.aws/credentials
    # and ~/.aws/config to mess up our tests
    "CPL_AWS_CREDENTIALS_FILE": "",
    "AWS_CONFIG_FILE": "",
    "CPL_AWS_EC2_API_ROOT_URL": "",
    "AWS_NO_SIGN_REQUEST": "NO",
    "AWS_SECRET_ACCESS_KEY": "AWS_SECRET_ACCESS_KEY",
    "AWS_ACCESS_KEY_ID": "AWS_ACCESS_KEY_ID",
    "AWS_TIMESTAMP": "20150101T000000Z",
    "AWS_HTTPS": "NO",
    "AWS_VIRTUAL_HOSTING": "NO",
    "AWS_REQUEST_PAYER": "",
    "AWS_DEFAULT_REGION": "us-east-1",
    "AWS_DEFAULT_PROFILE": "",
    "AWS_PROFILE": "default",
    "AWS_CONTAINER_CREDENTIALS_RELATIVE_URI": "",
    "AWS_CONTAINER_CREDENTIALS_FULL_URI": "",
}


@pytest.fixture(params=[True, False])
def aws_test_config_as_config_options_or_credentials(request):
    options = general_s3_options

    with (
        gdaltest.config_options(options, thread_local=False)
        if request.param
        else gdaltest.credentials("/vsis3/", options)
    ):
        yield request.param


@pytest.fixture()
def aws_test_config():
    options = general_s3_options

    with gdaltest.config_options(options, thread_local=False):
        yield


# Launch a single webserver in a module-scoped fixture.
# Provide the port in a function-scoped fixture so that we only
# set AWS_S3_ENDPOINT for tests that are using it.
@pytest.fixture(scope="module")
def webserver_launch():

    process, port = webserver.launch(handler=webserver.DispatcherHttpHandler)

    yield process, port

    webserver.server_stop(process, port)


@pytest.fixture(scope="function")
def webserver_port(webserver_launch):

    webserver_process, webserver_port = webserver_launch

    try:
        if webserver_port == 0:
            pytest.skip()
        with gdaltest.config_option(
            "AWS_S3_ENDPOINT", f"127.0.0.1:{webserver_port}", thread_local=False
        ):
            yield webserver_port
    finally:
        gdal.VSICurlClearCache()


###############################################################################


def test_vsis3_init(aws_test_config):

    options = {
        "AWS_SECRET_ACCESS_KEY": "",
        "AWS_ACCESS_KEY_ID": "",
    }
    with gdaltest.config_options(options, thread_local=False):
        assert gdal.GetSignedURL("/vsis3/foo/bar") is None


###############################################################################
# Test AWS_NO_SIGN_REQUEST=YES


@pytest.mark.network
def test_vsis3_no_sign_request(aws_test_config_as_config_options_or_credentials):

    options = {
        "AWS_S3_ENDPOINT": "https://s3.amazonaws.com",
        "AWS_NO_SIGN_REQUEST": "YES",
        "AWS_VIRTUAL_HOSTING": "TRUE",
    }

    bucket = "noaa-goes16"
    obj = "ABI-L1b-RadC/2022/001/00/OR_ABI-L1b-RadC-M6C01_G16_s20220010001173_e20220010003546_c20220010003587.nc"
    vsis3_path = "/vsis3/" + bucket + "/" + obj
    url = "https://" + bucket + ".s3.us-east-1.amazonaws.com/" + obj

    with (
        gdaltest.config_options(options, thread_local=False)
        if aws_test_config_as_config_options_or_credentials
        else gdaltest.credentials("/vsis3/" + bucket, options)
    ):
        actual_url = gdal.GetActualURL(vsis3_path)
        assert actual_url == url

        actual_url = gdal.GetActualURL(
            vsis3_path.replace("/vsis3/", "/vsis3_streaming/")
        )
        assert actual_url == url

        f = open_for_read(vsis3_path)
        if f is None:
            if gdaltest.gdalurlopen(url) is None:
                pytest.skip("cannot open URL")
            pytest.fail()
        gdal.VSIFCloseL(f)


###############################################################################
# Test Sync() and multithreaded download


@pytest.mark.network
def test_vsis3_sync_multithreaded_download(
    tmp_vsimem,
    aws_test_config_as_config_options_or_credentials,
):
    def cbk(pct, _, tab):
        assert pct >= tab[0]
        tab[0] = pct
        return True

    tab = [-1]
    options = {
        "AWS_S3_ENDPOINT": "http://s3.amazonaws.com",
        "AWS_NO_SIGN_REQUEST": "YES",
        "AWS_VIRTUAL_HOSTING": "FALSE",
    }
    # Use a public bucket with /test_dummy/foo and /test_dummy/bar files
    with (
        gdaltest.config_options(options, thread_local=False)
        if aws_test_config_as_config_options_or_credentials
        else gdaltest.credentials("/vsis3/cdn.proj.org", options)
    ):
        assert gdal.Sync(
            "/vsis3/cdn.proj.org/test_dummy",
            tmp_vsimem / "test_vsis3_no_sign_request_sync",
            options=["NUM_THREADS=2"],
            callback=cbk,
            callback_data=tab,
        )
    assert tab[0] == 1.0
    assert (
        gdal.VSIStatL(
            tmp_vsimem / "test_vsis3_no_sign_request_sync/test_dummy/foo"
        ).size
        == 4
    )
    assert (
        gdal.VSIStatL(
            tmp_vsimem / "test_vsis3_no_sign_request_sync/test_dummy/bar"
        ).size
        == 4
    )


###############################################################################
# Test Sync() and multithreaded download and CHUNK_SIZE


@pytest.mark.network
def test_vsis3_sync_multithreaded_download_chunk_size(tmp_vsimem, aws_test_config):
    def cbk(pct, _, tab):
        assert pct >= tab[0]
        tab[0] = pct
        return True

    tab = [-1]
    options = {
        "AWS_S3_ENDPOINT": "s3.amazonaws.com",
        "AWS_NO_SIGN_REQUEST": "YES",
        "AWS_VIRTUAL_HOSTING": "FALSE",
    }
    # Use a public bucket with /test_dummy/foo and /test_dummy/bar files
    with gdaltest.config_options(options, thread_local=False):
        assert gdal.Sync(
            "/vsis3/cdn.proj.org/test_dummy",
            tmp_vsimem / "test_vsis3_no_sign_request_sync",
            options=["NUM_THREADS=2", "CHUNK_SIZE=3"],
            callback=cbk,
            callback_data=tab,
        )
    assert tab[0] == 1.0
    assert (
        gdal.VSIStatL(
            tmp_vsimem / "test_vsis3_no_sign_request_sync/test_dummy/foo"
        ).size
        == 4
    )
    assert (
        gdal.VSIStatL(
            tmp_vsimem / "test_vsis3_no_sign_request_sync/test_dummy/bar"
        ).size
        == 4
    )


###############################################################################
# Error cases


def test_vsis3_1(aws_test_config):

    # Missing AWS_SECRET_ACCESS_KEY
    with gdaltest.config_options({"AWS_SECRET_ACCESS_KEY": ""}, thread_local=False):
        gdal.ErrorReset()

        with gdal.quiet_errors():
            f = open_for_read("/vsis3/foo/bar")
        assert f is None
        assert gdal.VSIGetLastErrorMsg().find("AWS_SECRET_ACCESS_KEY") >= 0

        gdal.ErrorReset()
        with gdal.quiet_errors():
            f = open_for_read("/vsis3_streaming/foo/bar")
        assert f is None
        assert gdal.VSIGetLastErrorMsg().find("AWS_SECRET_ACCESS_KEY") >= 0

    with gdaltest.config_options(
        {"AWS_SECRET_ACCESS_KEY": "AWS_SECRET_ACCESS_KEY", "AWS_ACCESS_KEY_ID": ""},
        thread_local=False,
    ):
        # Missing AWS_ACCESS_KEY_ID
        gdal.ErrorReset()
        with gdal.quiet_errors():
            f = open_for_read("/vsis3/foo/bar")
        assert f is None
        assert gdal.VSIGetLastErrorMsg().find("AWS_ACCESS_KEY_ID") >= 0

    with gdaltest.config_options(
        {
            "AWS_SECRET_ACCESS_KEY": "AWS_SECRET_ACCESS_KEY",
            "AWS_ACCESS_KEY_ID": "AWS_ACCESS_KEY_ID",
        },
        thread_local=False,
    ):
        # ERROR 1: The AWS Access Key Id you provided does not exist in our
        # records.
        gdal.ErrorReset()
        with gdal.quiet_errors():
            f = open_for_read("/vsis3/foo/bar.baz")
        if f is not None or gdal.VSIGetLastErrorMsg() == "":
            if f is not None:
                gdal.VSIFCloseL(f)
            if gdal.GetConfigOption("APPVEYOR") is not None:
                return
            pytest.fail(gdal.VSIGetLastErrorMsg())

        gdal.ErrorReset()
        with gdal.quiet_errors():
            f = open_for_read("/vsis3_streaming/foo/bar.baz")
        assert f is None and gdal.VSIGetLastErrorMsg() != ""


###############################################################################


def get_s3_fake_bucket_resource_method(
    request, with_security_token_AWS_SESSION_TOKEN=False
):
    request.protocol_version = "HTTP/1.1"

    if "Authorization" not in request.headers:
        sys.stderr.write("Bad headers: %s\n" % str(request.headers))
        request.send_response(403)
        return
    expected_authorization_8080 = (
        "AWS4-HMAC-SHA256 Credential=AWS_ACCESS_KEY_ID/20150101/us-east-1/"
        "s3/aws4_request,SignedHeaders=host;x-amz-content-sha256;"
        "x-amz-date,Signature="
        "38901846b865b12ac492bc005bb394ca8d60c098b68db57c084fac686a932f9e"
    )
    expected_authorization_8081 = (
        "AWS4-HMAC-SHA256 Credential=AWS_ACCESS_KEY_ID/20150101/us-east-1/"
        "s3/aws4_request,SignedHeaders=host;x-amz-content-sha256;"
        "x-amz-date,Signature="
        "9f623b7ffce76188a456c70fb4813eb31969e88d130d6b4d801b3accbf050d6c"
    )
    expected_authorization_8082 = (
        "AWS4-HMAC-SHA256 Credential=AWS_ACCESS_KEY_ID/20150101/us-east-1/"
        "s3/aws4_request,SignedHeaders=host;x-amz-content-sha256;"
        "x-amz-date,Signature="
        "c626ef0b5d4eb7329b0822e95bb26493570b31db7848d8a35a99a45c5fa73fb7"
    )
    expected_authorization_8080_AWS_SESSION_TOKEN = (
        "AWS4-HMAC-SHA256 Credential=AWS_ACCESS_KEY_ID/20150101/us-east-1/"
        "s3/aws4_request,SignedHeaders=host;x-amz-content-sha256;x-amz-date;"
        "x-amz-security-token,Signature="
        "a78e2d484679a19bec940a72d40c7fda37d1651a8ab82a6ed8fd7be46a53afb1"
    )
    expected_authorization_8081_AWS_SESSION_TOKEN = (
        "AWS4-HMAC-SHA256 Credential=AWS_ACCESS_KEY_ID/20150101/us-east-1/"
        "s3/aws4_request,SignedHeaders=host;x-amz-content-sha256;x-amz-date;"
        "x-amz-security-token,Signature="
        "008300e66bf58b81c57a61581f91fc70e545717ec9f2ab08a8c3e8446d75a7f3"
    )
    expected_authorization_8082_AWS_SESSION_TOKEN = (
        "AWS4-HMAC-SHA256 Credential=AWS_ACCESS_KEY_ID/20150101/us-east-1/"
        "s3/aws4_request,SignedHeaders=host;x-amz-content-sha256;x-amz-date;"
        "x-amz-security-token,Signature="
        "b798d885377cbdb89d5b0e430ed45215f86e25b01a69981c0b1ef0190d4e50d0"
    )
    actual_authorization = request.headers["Authorization"]
    if with_security_token_AWS_SESSION_TOKEN:
        expected_list = [
            expected_authorization_8080_AWS_SESSION_TOKEN,
            expected_authorization_8081_AWS_SESSION_TOKEN,
            expected_authorization_8082_AWS_SESSION_TOKEN,
        ]
    else:
        expected_list = [
            expected_authorization_8080,
            expected_authorization_8081,
            expected_authorization_8082,
        ]
    if actual_authorization not in expected_list:
        sys.stderr.write("Bad Authorization: '%s'\n" % str(actual_authorization))
        request.send_response(403)
        return

    request.send_response(200)
    request.send_header("Content-type", "text/plain")
    request.send_header("Content-Length", 3)
    request.send_header("Connection", "close")
    request.end_headers()
    request.wfile.write("""foo""".encode("ascii"))


def get_s3_fake_bucket_resource_method_with_security_token(request):
    return get_s3_fake_bucket_resource_method(
        request, with_security_token_AWS_SESSION_TOKEN=True
    )


###############################################################################
# Test with a fake AWS server


def test_vsis3_2(aws_test_config_as_config_options_or_credentials, webserver_port):

    gdal.VSICurlClearCache()

    signed_url = gdal.GetSignedURL("/vsis3/s3_fake_bucket/resource")
    expected_url_8080 = (
        "http://127.0.0.1:8080/s3_fake_bucket/resource"
        "?X-Amz-Algorithm=AWS4-HMAC-SHA256"
        "&X-Amz-Credential="
        "AWS_ACCESS_KEY_ID%2F20150101%2Fus-east-1%2Fs3%2Faws4_request"
        "&X-Amz-Date=20150101T000000Z&X-Amz-Expires=3600"
        "&X-Amz-Signature="
        "dca239dd95f72ff8c37c15c840afc54cd19bdb07f7aaee2223108b5b0ad35da8"
        "&X-Amz-SignedHeaders=host"
    )
    expected_url_8081 = (
        "http://127.0.0.1:8081/s3_fake_bucket/resource"
        "?X-Amz-Algorithm=AWS4-HMAC-SHA256"
        "&X-Amz-Credential="
        "AWS_ACCESS_KEY_ID%2F20150101%2Fus-east-1%2Fs3%2Faws4_request"
        "&X-Amz-Date=20150101T000000Z&X-Amz-Expires=3600"
        "&X-Amz-Signature="
        "ef5216bc5971863414c69f6ca095276c0d62c0da97fa4f6ab80c30bd7fc146ac"
        "&X-Amz-SignedHeaders=host"
    )
    expected_url_8082 = (
        "http://127.0.0.1:8082/s3_fake_bucket/resource"
        "?X-Amz-Algorithm=AWS4-HMAC-SHA256"
        "&X-Amz-Credential="
        "AWS_ACCESS_KEY_ID%2F20150101%2Fus-east-1%2Fs3%2Faws4_request"
        "&X-Amz-Date=20150101T000000Z&X-Amz-Expires=3600"
        "&X-Amz-Signature="
        "9b14dd2c511c8916b2bffced71ab3405980cda1cc6019f6159b60dd0d9dac9b2"
        "&X-Amz-SignedHeaders=host"
    )
    assert signed_url in (expected_url_8080, expected_url_8081, expected_url_8082)

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/s3_fake_bucket/resource",
        custom_method=get_s3_fake_bucket_resource_method,
    )

    with webserver.install_http_handler(handler):
        f = open_for_read("/vsis3/s3_fake_bucket/resource")
        assert f is not None
        data = gdal.VSIFReadL(1, 4, f).decode("ascii")
        gdal.VSIFCloseL(f)

        assert data == "foo"

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/s3_fake_bucket/resource",
        custom_method=get_s3_fake_bucket_resource_method,
    )
    with webserver.install_http_handler(handler):
        f = open_for_read("/vsis3_streaming/s3_fake_bucket/resource")
        assert f is not None
        data = gdal.VSIFReadL(1, 4, f).decode("ascii")
        gdal.VSIFCloseL(f)

    assert data == "foo"

    handler = webserver.SequentialHandler()

    def method(request):
        request.protocol_version = "HTTP/1.1"

        if "Authorization" not in request.headers:
            sys.stderr.write("Bad headers: %s\n" % str(request.headers))
            request.send_response(403)
            return
        expected_authorization_8080 = (
            "AWS4-HMAC-SHA256 Credential=AWS_ACCESS_KEY_ID/20150101/us-east-1"
            "/s3/aws4_request,SignedHeaders=host;x-amz-content-sha256;"
            "x-amz-date;x-amz-security-token,"
            "Signature="
            "464a21835038b4f4d292b6463b8a005b9aaa980513aa8c42fc170abb733dce85"
        )
        expected_authorization_8081 = (
            "AWS4-HMAC-SHA256 Credential=AWS_ACCESS_KEY_ID/20150101/us-east-1"
            "/s3/aws4_request,SignedHeaders=host;x-amz-content-sha256;"
            "x-amz-date;x-amz-security-token,"
            "Signature="
            "b10e91575186342f9f2acfc91c4c2c9938c4a9e8cdcbc043d09d59d9641ad7fb"
        )
        expected_authorization_8082 = (
            "AWS4-HMAC-SHA256 Credential=AWS_ACCESS_KEY_ID/20150101/us-east-1"
            "/s3/aws4_request,SignedHeaders=host;x-amz-content-sha256;"
            "x-amz-date;x-amz-security-token,"
            "Signature="
            "6b52b6d418d75d9d440cc0535b5d65e28724359554fc16ee3493acc06e7fc4d6"
        )
        actual_authorization = request.headers["Authorization"]
        if actual_authorization not in (
            expected_authorization_8080,
            expected_authorization_8081,
            expected_authorization_8082,
        ):
            sys.stderr.write("Bad Authorization: '%s'\n" % str(actual_authorization))
            request.send_response(403)
            return

        request.send_response(200)
        request.send_header("Content-type", "text/plain")
        request.send_header("Content-Length", 3)
        request.end_headers()
        request.wfile.write("""foo""".encode("ascii"))

    handler.add(
        "GET", "/s3_fake_bucket_with_session_token/resource", custom_method=method
    )

    # Test with temporary credentials
    with (
        gdaltest.config_option(
            "AWS_SESSION_TOKEN", "AWS_SESSION_TOKEN", thread_local=False
        )
        if aws_test_config_as_config_options_or_credentials
        else gdaltest.credentials(
            "/vsis3/s3_fake_bucket_with_session_token",
            {"AWS_SESSION_TOKEN": "AWS_SESSION_TOKEN"},
        )
    ):
        with webserver.install_http_handler(handler):
            f = open_for_read("/vsis3/s3_fake_bucket_with_session_token/resource")
            assert f is not None
            data = gdal.VSIFReadL(1, 4, f).decode("ascii")
            gdal.VSIFCloseL(f)

    handler = webserver.SequentialHandler()

    def method(request):
        request.protocol_version = "HTTP/1.1"
        if "Range" in request.headers:
            if request.headers["Range"] != "bytes=0-16383":
                sys.stderr.write("Bad Range: '%s'\n" % str(request.headers["Range"]))
                request.send_response(403)
                return
            request.send_response(206)
            request.send_header("Content-type", "text/plain")
            request.send_header("Content-Range", "bytes 0-16383/1000000")
            request.send_header("Content-Length", 16384)
            request.send_header("Connection", "close")
            request.end_headers()
            request.wfile.write(("a" * 16384).encode("ascii"))
        else:
            request.send_response(200)
            request.send_header("Content-type", "text/plain")
            request.send_header("Content-Length", 1000000)
            request.send_header("Connection", "close")
            request.end_headers()
            request.wfile.write(("a" * 1000000).encode("ascii"))

    handler.add("GET", "/s3_fake_bucket/resource2.bin", custom_method=method)

    with webserver.install_http_handler(handler):
        # old_val = gdal.GetConfigOption('GDAL_DISABLE_READDIR_ON_OPEN')
        # gdal.SetConfigOption('GDAL_DISABLE_READDIR_ON_OPEN', 'EMPTY_DIR')
        stat_res = gdal.VSIStatL("/vsis3/s3_fake_bucket/resource2.bin")
        # gdal.SetConfigOption('GDAL_DISABLE_READDIR_ON_OPEN', old_val)
        if stat_res is None or stat_res.size != 1000000:
            if stat_res is not None:
                print(stat_res.size)
            else:
                print(stat_res)
            pytest.fail()

    handler = webserver.SequentialHandler()
    handler.add(
        "HEAD",
        "/s3_fake_bucket/resource2.bin",
        200,
        {
            "Content-type": "text/plain",
            "Content-Length": 1000000,
            "Connection": "close",
        },
    )
    with webserver.install_http_handler(handler):
        stat_res = gdal.VSIStatL("/vsis3_streaming/s3_fake_bucket/resource2.bin")
    if stat_res is None or stat_res.size != 1000000:
        if stat_res is not None:
            print(stat_res.size)
        else:
            print(stat_res)
        pytest.fail()

    handler = webserver.SequentialHandler()

    def method(request):
        request.protocol_version = "HTTP/1.1"
        if request.headers["Authorization"].find("us-east-1") >= 0:
            request.send_response(400)
            response = """<?xml version="1.0" encoding="UTF-8"?>
            <Error>
            <Message>bla</Message>
            <Code>AuthorizationHeaderMalformed</Code>
            <Region>us-west-2</Region>
            </Error>'
            """
            response = "%x\r\n%s\r\n0\r\n\r\n" % (len(response), response)
            request.send_header("Content-type", "application/xml")
            request.send_header("Transfer-Encoding", "chunked")
            request.send_header("Connection", "close")
            request.end_headers()
            request.wfile.write(response.encode("ascii"))
        else:
            sys.stderr.write("Bad headers: %s\n" % str(request.headers))
            request.send_response(403)

    handler.add("GET", "/s3_fake_bucket/redirect", custom_method=method)

    def method(request):
        request.protocol_version = "HTTP/1.1"
        includes_us_west_2 = request.headers["Authorization"].find("us-west-2") >= 0
        host_is_127_0_0_1 = request.headers["Host"].startswith("127.0.0.1")

        if includes_us_west_2 and host_is_127_0_0_1:
            request.send_response(301)
            response = """<?xml version="1.0" encoding="UTF-8"?>
            <Error>
            <Message>bla</Message>
            <Code>PermanentRedirect</Code>
            <Endpoint>localhost:%d</Endpoint>
            </Error>""" % request.server.port
            response = "%x\r\n%s\r\n0\r\n\r\n" % (len(response), response)
            request.send_header("Content-type", "application/xml")
            request.send_header("Transfer-Encoding", "chunked")
            request.send_header("Connection", "close")
            request.end_headers()
            request.wfile.write(response.encode("ascii"))
        else:
            sys.stderr.write("Bad headers: %s\n" % str(request.headers))
            request.send_response(403)

    handler.add("GET", "/s3_fake_bucket/redirect", custom_method=method)

    def method(request):
        request.protocol_version = "HTTP/1.1"
        includes_us_west_2 = request.headers["Authorization"].find("us-west-2") >= 0
        host_is_localhost = request.headers["Host"].startswith("localhost")

        if includes_us_west_2 and host_is_localhost:
            request.send_response(200)
            request.send_header("Content-type", "text/plain")
            request.send_header("Content-Length", 3)
            request.send_header("Connection", "close")
            request.end_headers()
            request.wfile.write("""foo""".encode("ascii"))
        else:
            sys.stderr.write("Bad headers: %s\n" % str(request.headers))
            request.send_response(403)

    handler.add("GET", "/s3_fake_bucket/redirect", custom_method=method)

    # Test region and endpoint 'redirects'
    with webserver.install_http_handler(handler):
        f = open_for_read("/vsis3/s3_fake_bucket/redirect")
        assert f is not None
        data = gdal.VSIFReadL(1, 4, f).decode("ascii")
        gdal.VSIFCloseL(f)

    if data != "foo":
        pytest.fail(data)

    # Test region and endpoint 'redirects'
    gdal.VSICurlClearCache()

    handler.req_count = 0
    with webserver.install_http_handler(handler):
        f = open_for_read("/vsis3_streaming/s3_fake_bucket/redirect")
        assert f is not None
        data = gdal.VSIFReadL(1, 4, f).decode("ascii")
        gdal.VSIFCloseL(f)

    assert data == "foo"

    handler = webserver.SequentialHandler()

    def method(request):
        # /vsis3_streaming/ should have remembered the change of region and
        # endpoint
        if request.headers["Authorization"].find(
            "us-west-2"
        ) < 0 or not request.headers["Host"].startswith("localhost"):
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

    handler.add("GET", "/s3_fake_bucket/non_xml_error", custom_method=method)

    gdal.ErrorReset()
    with webserver.install_http_handler(handler):
        with gdal.quiet_errors():
            f = open_for_read("/vsis3_streaming/s3_fake_bucket/non_xml_error")
    assert f is None and gdal.VSIGetLastErrorMsg().find("bla") >= 0

    handler = webserver.SequentialHandler()
    response = '<?xml version="1.0" encoding="UTF-8"?><oops>'
    response = "%x\r\n%s\r\n0\r\n\r\n" % (len(response), response)
    handler.add(
        "GET",
        "/s3_fake_bucket/invalid_xml_error",
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
            f = open_for_read("/vsis3_streaming/s3_fake_bucket/invalid_xml_error")
    assert f is None and gdal.VSIGetLastErrorMsg().find("<oops>") >= 0

    handler = webserver.SequentialHandler()
    response = '<?xml version="1.0" encoding="UTF-8"?><Error/>'
    response = "%x\r\n%s\r\n0\r\n\r\n" % (len(response), response)
    handler.add(
        "GET",
        "/s3_fake_bucket/no_code_in_error",
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
            f = open_for_read("/vsis3_streaming/s3_fake_bucket/no_code_in_error")
    assert f is None and gdal.VSIGetLastErrorMsg().find("<Error/>") >= 0

    handler = webserver.SequentialHandler()
    response = """<?xml version="1.0" encoding="UTF-8"?>
    <Error>
    <Code>AuthorizationHeaderMalformed</Code>
    </Error>"""
    response = "%x\r\n%s\r\n0\r\n\r\n" % (len(response), response)
    handler.add(
        "GET",
        "/s3_fake_bucket/no_region_in_AuthorizationHeaderMalformed_error",
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
                "/vsis3_streaming/s3_fake_bucket"
                "/no_region_in_AuthorizationHeaderMalformed_error"
            )
    assert f is None and gdal.VSIGetLastErrorMsg().find("<Error>") >= 0

    handler = webserver.SequentialHandler()
    response = """<?xml version="1.0" encoding="UTF-8"?>
    <Error>
    <Code>PermanentRedirect</Code>
    </Error>"""
    response = "%x\r\n%s\r\n0\r\n\r\n" % (len(response), response)
    handler.add(
        "GET",
        "/s3_fake_bucket/no_endpoint_in_PermanentRedirect_error",
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
                "/vsis3_streaming/s3_fake_bucket"
                "/no_endpoint_in_PermanentRedirect_error"
            )
    assert f is None and gdal.VSIGetLastErrorMsg().find("<Error>") >= 0

    handler = webserver.SequentialHandler()
    response = """<?xml version="1.0" encoding="UTF-8"?>
    <Error>
    <Code>bla</Code>
    </Error>"""
    response = "%x\r\n%s\r\n0\r\n\r\n" % (len(response), response)
    handler.add(
        "GET",
        "/s3_fake_bucket/no_message_in_error",
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
            f = open_for_read("/vsis3_streaming/s3_fake_bucket/no_message_in_error")
    assert f is None and gdal.VSIGetLastErrorMsg().find("<Error>") >= 0

    # Test with requester pays
    handler = webserver.SequentialHandler()

    def method(request):
        if "x-amz-request-payer" not in request.headers:
            sys.stderr.write("Bad headers: %s\n" % str(request.headers))
            request.send_response(403)
            return
        expected_authorization_8080 = (
            "AWS4-HMAC-SHA256 Credential=AWS_ACCESS_KEY_ID/20150101/us-east-1/"
            "s3/aws4_request,SignedHeaders=host;x-amz-content-sha256;"
            "x-amz-date;x-amz-request-payer,"
            "Signature="
            "cf713a394e1b629ac0e468d60d3d4a12f5236fd72d21b6005c758b0dfc7049cd"
        )
        expected_authorization_8081 = (
            "AWS4-HMAC-SHA256 Credential=AWS_ACCESS_KEY_ID/20150101/us-east-1/"
            "s3/aws4_request,SignedHeaders=host;x-amz-content-sha256;"
            "x-amz-date;x-amz-request-payer,"
            "Signature="
            "4756166679008a1a40cd6ff91dbbef670a71c11bf8e3c998dd7385577c3ac4d9"
        )
        expected_authorization_8082 = (
            "AWS4-HMAC-SHA256 Credential=AWS_ACCESS_KEY_ID/20150101/us-east-1/"
            "s3/aws4_request,SignedHeaders=host;x-amz-content-sha256;"
            "x-amz-date;x-amz-request-payer,"
            "Signature="
            "09fba5115f690cda0a602332854e29ed59e5767fdce1ff0b5b0880ebe5c6dc85"
        )
        actual_authorization = request.headers["Authorization"]
        if actual_authorization not in (
            expected_authorization_8080,
            expected_authorization_8081,
            expected_authorization_8082,
        ):
            sys.stderr.write("Bad Authorization: '%s'\n" % str(actual_authorization))
            request.send_response(403)
            return
        if request.headers["x-amz-request-payer"] != "requester":
            sys.stderr.write(
                "Bad x-amz-request-payer: '%s'\n"
                % str(request.headers["x-amz-request-payer"])
            )
            request.send_response(403)
            return

        request.send_response(200)
        request.send_header("Content-type", "text/plain")
        request.send_header("Content-Length", 3)
        request.send_header("Connection", "close")
        request.end_headers()
        request.wfile.write("""foo""".encode("ascii"))

    handler.add(
        "GET", "/s3_fake_bucket_with_requester_pays/resource", custom_method=method
    )

    with (
        gdaltest.config_option("AWS_REQUEST_PAYER", "requester", thread_local=False)
        if aws_test_config_as_config_options_or_credentials
        else gdaltest.credentials(
            "/vsis3/s3_fake_bucket_with_requester_pays",
            {"AWS_REQUEST_PAYER": "requester"},
        )
    ):
        with webserver.install_http_handler(handler):
            with gdal.quiet_errors():
                f = open_for_read("/vsis3/s3_fake_bucket_with_requester_pays/resource")
                assert f is not None
                data = gdal.VSIFReadL(1, 3, f).decode("ascii")
                gdal.VSIFCloseL(f)

    assert data == "foo"

    # Test temporary redirect
    handler = webserver.SequentialHandler()

    class HandlerClass:
        def __init__(self, response_value):
            self.old_authorization = None
            self.response_value = response_value

        def method_req_1(self, request):
            if request.headers["Host"].find("127.0.0.1") < 0:
                sys.stderr.write("Bad headers: %s\n" % str(request.headers))
                request.send_response(403)
                return
            self.old_authorization = request.headers["Authorization"]
            request.protocol_version = "HTTP/1.1"
            request.send_response(307)
            response = """<?xml version="1.0" encoding="UTF-8"?>
            <Error>
            <Message>bla</Message>
            <Code>TemporaryRedirect</Code>
            <Endpoint>localhost:%d</Endpoint>
            </Error>""" % request.server.port
            response = "%x\r\n%s\r\n0\r\n\r\n" % (len(response), response)
            request.send_header("Content-type", "application/xml")
            request.send_header("Transfer-Encoding", "chunked")
            request.end_headers()
            request.wfile.write(response.encode("ascii"))

        def method_req_2(self, request):
            if request.headers["Host"].find("localhost") < 0:
                sys.stderr.write("Bad headers: %s\n" % str(request.headers))
                request.send_response(403)
                return
            if self.old_authorization == request.headers["Authorization"]:
                sys.stderr.write(
                    "Should have get a different Authorization. "
                    "Bad headers: %s\n" % str(request.headers)
                )
                request.send_response(403)
                return
            request.protocol_version = "HTTP/1.1"
            request.send_response(200)
            response = self.response_value
            request.send_header("Content-Length", len(response))
            request.end_headers()
            request.wfile.write(response.encode("ascii"))

    h = HandlerClass("foo")
    handler.add(
        "GET", "/s3_test_temporary_redirect_read/resource", custom_method=h.method_req_1
    )
    handler.add(
        "GET", "/s3_test_temporary_redirect_read/resource", custom_method=h.method_req_2
    )

    with webserver.install_http_handler(handler):
        f = open_for_read("/vsis3/s3_test_temporary_redirect_read/resource")
        assert f is not None
        data = gdal.VSIFReadL(1, 3, f).decode("ascii")
        gdal.VSIFCloseL(f)

    assert data == "foo"

    # Retry on the same bucket and check that the redirection was indeed
    # temporary
    handler = webserver.SequentialHandler()

    h = HandlerClass("bar")
    handler.add(
        "GET",
        "/s3_test_temporary_redirect_read/resource2",
        custom_method=h.method_req_1,
    )
    handler.add(
        "GET",
        "/s3_test_temporary_redirect_read/resource2",
        custom_method=h.method_req_2,
    )

    with webserver.install_http_handler(handler):
        f = open_for_read("/vsis3/s3_test_temporary_redirect_read/resource2")
        assert f is not None
        data = gdal.VSIFReadL(1, 3, f).decode("ascii")
        gdal.VSIFCloseL(f)

    assert data == "bar"


###############################################################################
# Test re-opening after changing configuration option (#2294)


def test_vsis3_open_after_config_option_change(aws_test_config, webserver_port):
    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add(
        "GET", "/test_vsis3_change_config_options/?delimiter=%2F&list-type=2", 403
    )
    handler.add("GET", "/test_vsis3_change_config_options/test.bin", 403)
    with webserver.install_http_handler(handler):
        with gdal.quiet_errors():
            f = open_for_read("/vsis3/test_vsis3_change_config_options/test.bin")
        assert f is None

    # Does not attempt any network access since we didn't change significant
    # parameters
    f = open_for_read("/vsis3/test_vsis3_change_config_options/test.bin")
    assert f is None

    with gdaltest.config_option(
        "AWS_ACCESS_KEY_ID", "another_key_id", thread_local=False
    ):
        handler = webserver.SequentialHandler()
        handler.add(
            "GET",
            "/test_vsis3_change_config_options/?delimiter=%2F&list-type=2",
            200,
            {"Content-type": "application/xml"},
            """<?xml version="1.0" encoding="UTF-8"?>
            <ListBucketResult>
                <Prefix></Prefix>
                <Contents>
                    <Key>test.bin</Key>
                    <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                    <Size>123456</Size>
                </Contents>
            </ListBucketResult>
            """,
        )
        with webserver.install_http_handler(handler):
            f = open_for_read("/vsis3/test_vsis3_change_config_options/test.bin")
            assert f is not None
            gdal.VSIFCloseL(f)


###############################################################################
# Test ReadDir() with a fake AWS server


def test_vsis3_readdir(aws_test_config, webserver_port):
    handler = webserver.SequentialHandler()

    def method(request):
        request.protocol_version = "HTTP/1.1"
        if request.headers["Authorization"].find("us-east-1") >= 0:
            request.send_response(400)
            response = """<?xml version="1.0" encoding="UTF-8"?>
            <Error>
            <Message>bla</Message>
            <Code>AuthorizationHeaderMalformed</Code>
            <Region>us-west-2</Region>
            </Error>"""
            response = "%x\r\n%s\r\n0\r\n\r\n" % (len(response), response)
            request.send_header("Content-type", "application/xml")
            request.send_header("Transfer-Encoding", "chunked")
            request.end_headers()
            request.wfile.write(response.encode("ascii"))
        elif request.headers["Authorization"].find("us-west-2") >= 0:
            if request.headers["Host"].startswith("127.0.0.1"):
                request.send_response(301)
                response = """<?xml version="1.0" encoding="UTF-8"?>
                <Error>
                <Message>bla</Message>
                <Code>PermanentRedirect</Code>
                <Endpoint>localhost:%d</Endpoint>
                </Error>""" % request.server.port
                response = "%x\r\n%s\r\n0\r\n\r\n" % (len(response), response)
                request.send_header("Content-type", "application/xml")
                request.send_header("Transfer-Encoding", "chunked")
                request.end_headers()
                request.wfile.write(response.encode("ascii"))
            elif request.headers["Host"].startswith("localhost"):
                request.send_response(200)
                request.send_header("Content-type", "application/xml")
                response = """<?xml version="1.0" encoding="UTF-8"?>
                <ListBucketResult>
                    <Prefix>a_dir with_space/</Prefix>
                    <NextContinuationToken>bla</NextContinuationToken>
                    <Contents>
                        <Key>a_dir with_space/resource3 with_space.bin</Key>
                        <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                        <Size>123456</Size>
                    </Contents>
                </ListBucketResult>
                """
                request.send_header("Content-Length", len(response))
                request.end_headers()
                request.wfile.write(response.encode("ascii"))
            else:
                sys.stderr.write("Bad headers: %s\n" % str(request.headers))
                request.send_response(403)
        else:
            sys.stderr.write("Bad headers: %s\n" % str(request.headers))
            request.send_response(403)

    handler.add(
        "GET",
        "/s3_fake_bucket2/?delimiter=%2F&list-type=2&prefix=a_dir%20with_space%2F",
        custom_method=method,
    )
    handler.add(
        "GET",
        "/s3_fake_bucket2/?delimiter=%2F&list-type=2&prefix=a_dir%20with_space%2F",
        custom_method=method,
    )
    handler.add(
        "GET",
        "/s3_fake_bucket2/?delimiter=%2F&list-type=2&prefix=a_dir%20with_space%2F",
        custom_method=method,
    )

    def method(request):
        # /vsis3/ should have remembered the change of region and endpoint
        if request.headers["Authorization"].find(
            "us-west-2"
        ) < 0 or not request.headers["Host"].startswith("localhost"):
            sys.stderr.write("Bad headers: %s\n" % str(request.headers))
            request.send_response(403)

        request.protocol_version = "HTTP/1.1"
        request.send_response(200)
        request.send_header("Content-type", "application/xml")
        response = """<?xml version="1.0" encoding="UTF-8"?>
            <ListBucketResult>
                <Prefix>a_dir with_space/</Prefix>
                <Contents>
                    <Key>a_dir with_space/resource4.bin</Key>
                    <LastModified>2015-10-16T12:34:56.000Z</LastModified>
                    <Size>456789</Size>
                </Contents>
                <Contents>
                    <Key>a_dir with_space/i_am_a_glacier_file</Key>
                    <LastModified>2015-10-16T12:34:56.000Z</LastModified>
                    <Size>456789</Size>
                     <StorageClass>GLACIER</StorageClass>
                </Contents>
                <Contents>
                    <Key>a_dir with_space/i_am_a_deep_archive_file</Key>
                    <LastModified>2015-10-16T12:34:56.000Z</LastModified>
                    <Size>456789</Size>
                     <StorageClass>DEEP_ARCHIVE</StorageClass>
                </Contents>
                <Contents>
                    <Key>a_dir with_space/../is_ok</Key>
                    <LastModified>2015-10-16T12:34:56.000Z</LastModified>
                    <Size>456789</Size>
                </Contents>
                <Contents>
                    <Key>a_dir with_space/../../not_ok</Key>
                    <LastModified>2015-10-16T12:34:56.000Z</LastModified>
                    <Size>456789</Size>
                </Contents>
                <CommonPrefixes>
                    <Prefix>a_dir with_space/subdir/</Prefix>
                    <Prefix>a_dir with_space/../../subdir_not_ok/</Prefix>
                </CommonPrefixes>
            </ListBucketResult>
        """
        request.send_header("Content-Length", len(response))
        request.end_headers()
        request.wfile.write(response.encode("ascii"))

    handler.add(
        "GET",
        (
            "/s3_fake_bucket2/"
            "?continuation-token=bla&delimiter=%2F&list-type=2&prefix=a_dir%20with_space%2F"
        ),
        custom_method=method,
    )

    with webserver.install_http_handler(handler):
        f = open_for_read(
            "/vsis3/s3_fake_bucket2/a_dir with_space/resource3 with_space.bin"
        )
    if f is None:
        pytest.fail()
    gdal.VSIFCloseL(f)

    with webserver.install_http_handler(webserver.SequentialHandler()):
        dir_contents = gdal.ReadDir("/vsis3/s3_fake_bucket2/a_dir with_space")
    expected_dir_contents = [
        "resource3 with_space.bin",
        "resource4.bin",
        "../is_ok",
        "subdir",
    ]
    assert dir_contents == expected_dir_contents

    assert (
        gdal.ReadDir("/vsis3_streaming/s3_fake_bucket2/a_dir with_space")
        == expected_dir_contents
    )

    assert (
        gdal.VSIStatL(
            "/vsis3/s3_fake_bucket2/a_dir with_space/resource3 with_space.bin"
        ).size
        == 123456
    )
    assert (
        gdal.VSIStatL(
            "/vsis3/s3_fake_bucket2/a_dir with_space/resource3 with_space.bin"
        ).mtime
        == 1
    )

    # Same as above: cached
    dir_contents = gdal.ReadDir("/vsis3/s3_fake_bucket2/a_dir with_space")
    assert dir_contents == expected_dir_contents

    # ReadDir on something known to be a file shouldn't cause network access
    dir_contents = gdal.ReadDir(
        "/vsis3/s3_fake_bucket2/a_dir with_space/resource3 with_space.bin"
    )
    assert dir_contents is None

    # Test unrelated partial clear of the cache
    gdal.VSICurlPartialClearCache("/vsis3/s3_fake_bucket_unrelated")

    assert (
        gdal.VSIStatL(
            "/vsis3/s3_fake_bucket2/a_dir with_space/resource3 with_space.bin"
        ).size
        == 123456
    )

    dir_contents = gdal.ReadDir("/vsis3/s3_fake_bucket2/a_dir with_space")
    assert dir_contents == expected_dir_contents

    # Test partial clear of the cache
    gdal.VSICurlPartialClearCache("/vsis3/s3_fake_bucket2/a_dir with_space")

    handler = webserver.SequentialHandler()
    handler.add(
        "GET", "/s3_fake_bucket2/a_dir%20with_space/resource3%20with_space.bin", 400
    )
    handler.add(
        "GET",
        (
            "/s3_fake_bucket2/?delimiter=%2F&list-type=2&max-keys=100"
            "&prefix=a_dir%20with_space%2Fresource3%20with_space.bin%2F"
        ),
        400,
    )
    with webserver.install_http_handler(handler):
        gdal.VSIStatL(
            "/vsis3/s3_fake_bucket2/a_dir with_space/resource3 with_space.bin"
        )

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/s3_fake_bucket2/?delimiter=%2F&list-type=2&prefix=a_dir%2F",
        200,
        {"Content-type": "application/xml"},
        """<?xml version="1.0" encoding="UTF-8"?>
            <ListBucketResult>
                <Prefix>a_dir/</Prefix>
                <Contents>
                    <Key>a_dir/test.txt</Key>
                    <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                    <Size>40</Size>
                </Contents>
            </ListBucketResult>
        """,
        expected_headers={"foo": "bar"},
    )
    gdal.SetPathSpecificOption(
        "/vsis3/s3_fake_bucket2/a_dir",
        "GDAL_HTTP_HEADERS",
        "foo:bar",
    )
    with webserver.install_http_handler(handler):
        dir_contents = gdal.ReadDir("/vsis3/s3_fake_bucket2/a_dir")
    gdal.ClearPathSpecificOptions()
    assert dir_contents == ["test.txt"]

    # Test CPL_VSIL_CURL_IGNORE_GLACIER_STORAGE=NO
    gdal.VSICurlClearCache()
    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/s3_fake_bucket2/?delimiter=%2F&list-type=2&prefix=a_dir%2F",
        200,
        {},
        """<?xml version="1.0" encoding="UTF-8"?>
            <ListBucketResult>
                <Prefix>a_dir/</Prefix>
                <Contents>
                    <Key>a_dir/resource4.bin</Key>
                    <LastModified>2015-10-16T12:34:56.000Z</LastModified>
                    <Size>456789</Size>
                </Contents>
                <Contents>
                    <Key>a_dir/i_am_a_glacier_file</Key>
                    <LastModified>2015-10-16T12:34:56.000Z</LastModified>
                    <Size>456789</Size>
                     <StorageClass>GLACIER</StorageClass>
                </Contents>
                <Contents>
                    <Key>a_dir/i_am_a_deep_archive_file</Key>
                    <LastModified>2015-10-16T12:34:56.000Z</LastModified>
                    <Size>456789</Size>
                     <StorageClass>DEEP_ARCHIVE</StorageClass>
                </Contents>
                <CommonPrefixes>
                    <Prefix>a_dir/subdir/</Prefix>
                </CommonPrefixes>
            </ListBucketResult>
        """,
    )
    with gdaltest.config_option(
        "CPL_VSIL_CURL_IGNORE_GLACIER_STORAGE", "NO", thread_local=False
    ):
        with webserver.install_http_handler(handler):
            dir_contents = gdal.ReadDir("/vsis3/s3_fake_bucket2/a_dir")
    assert dir_contents == [
        "resource4.bin",
        "i_am_a_glacier_file",
        "i_am_a_deep_archive_file",
        "subdir",
    ]

    # Test CPL_VSIL_CURL_IGNORE_STORAGE_CLASSES=
    gdal.VSICurlClearCache()
    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/s3_fake_bucket2/?delimiter=%2F&list-type=2&prefix=a_dir%2F",
        200,
        {},
        """<?xml version="1.0" encoding="UTF-8"?>
            <ListBucketResult>
                <Prefix>a_dir/</Prefix>
                <Contents>
                    <Key>a_dir/resource4.bin</Key>
                    <LastModified>2015-10-16T12:34:56.000Z</LastModified>
                    <Size>456789</Size>
                </Contents>
                <Contents>
                    <Key>a_dir/i_am_a_glacier_file</Key>
                    <LastModified>2015-10-16T12:34:56.000Z</LastModified>
                    <Size>456789</Size>
                     <StorageClass>GLACIER</StorageClass>
                </Contents>
                <Contents>
                    <Key>a_dir/i_am_a_deep_archive_file</Key>
                    <LastModified>2015-10-16T12:34:56.000Z</LastModified>
                    <Size>456789</Size>
                     <StorageClass>DEEP_ARCHIVE</StorageClass>
                </Contents>
                <CommonPrefixes>
                    <Prefix>a_dir/subdir/</Prefix>
                </CommonPrefixes>
            </ListBucketResult>
        """,
    )
    with gdaltest.config_option(
        "CPL_VSIL_CURL_IGNORE_STORAGE_CLASSES", "", thread_local=False
    ):
        with webserver.install_http_handler(handler):
            dir_contents = gdal.ReadDir("/vsis3/s3_fake_bucket2/a_dir")
    assert dir_contents == [
        "resource4.bin",
        "i_am_a_glacier_file",
        "i_am_a_deep_archive_file",
        "subdir",
    ]

    # Test CPL_VSIL_CURL_NON_CACHED
    for config_option_value in [
        "/vsis3/s3_non_cached/test.txt",
        "/vsis3/s3_non_cached",
        "/vsis3/s3_non_cached:/vsis3/unrelated",
        "/vsis3/unrelated:/vsis3/s3_non_cached",
        "/vsis3/unrelated:/vsis3/s3_non_cached:/vsis3/unrelated",
    ]:
        with gdaltest.config_option(
            "CPL_VSIL_CURL_NON_CACHED", config_option_value, thread_local=False
        ):

            handler = webserver.SequentialHandler()
            handler.add("GET", "/s3_non_cached/test.txt", 200, {}, "foo")
            handler.add("GET", "/s3_non_cached/test.txt", 200, {}, "foo")

            with webserver.install_http_handler(handler):
                f = open_for_read("/vsis3/s3_non_cached/test.txt")
                assert f is not None, config_option_value
                data = gdal.VSIFReadL(1, 3, f).decode("ascii")
                gdal.VSIFCloseL(f)
                assert data == "foo", config_option_value

            handler = webserver.SequentialHandler()
            handler.add("GET", "/s3_non_cached/test.txt", 200, {}, "bar2")

            with webserver.install_http_handler(handler):
                size = gdal.VSIStatL("/vsis3/s3_non_cached/test.txt").size
            assert size == 4, config_option_value

            handler = webserver.SequentialHandler()
            handler.add("GET", "/s3_non_cached/test.txt", 200, {}, "foo")

            with webserver.install_http_handler(handler):
                size = gdal.VSIStatL("/vsis3/s3_non_cached/test.txt").size
                if size != 3:
                    print(config_option_value)
                    pytest.fail(data)

            handler = webserver.SequentialHandler()
            handler.add("GET", "/s3_non_cached/test.txt", 200, {}, "bar2")
            handler.add("GET", "/s3_non_cached/test.txt", 200, {}, "bar2")

            with webserver.install_http_handler(handler):
                f = open_for_read("/vsis3/s3_non_cached/test.txt")
                assert f is not None, config_option_value
                data = gdal.VSIFReadL(1, 4, f).decode("ascii")
                gdal.VSIFCloseL(f)
                assert data == "bar2", config_option_value

    # Retry without option
    for config_option_value in [None, "/vsis3/s3_non_cached/bar.txt"]:
        with gdaltest.config_option(
            "CPL_VSIL_CURL_NON_CACHED", config_option_value, thread_local=False
        ):

            handler = webserver.SequentialHandler()
            if config_option_value is None:
                handler.add(
                    "GET",
                    "/s3_non_cached/?delimiter=%2F&list-type=2",
                    200,
                    {"Content-type": "application/xml"},
                    """<?xml version="1.0" encoding="UTF-8"?>
                        <ListBucketResult>
                            <Prefix></Prefix>
                            <Contents>
                                <Key>test.txt</Key>
                                <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                                <Size>40</Size>
                            </Contents>
                            <Contents>
                                <Key>test2.txt</Key>
                                <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                                <Size>40</Size>
                            </Contents>
                        </ListBucketResult>
                    """,
                )
                handler.add("GET", "/s3_non_cached/test.txt", 200, {}, "foo")

            with webserver.install_http_handler(handler):
                f = open_for_read("/vsis3/s3_non_cached/test.txt")
                assert f is not None, config_option_value
                data = gdal.VSIFReadL(1, 3, f).decode("ascii")
                gdal.VSIFCloseL(f)
                assert data == "foo", config_option_value

            handler = webserver.SequentialHandler()
            with webserver.install_http_handler(handler):
                f = open_for_read("/vsis3/s3_non_cached/test.txt")
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
    # This one is a s3express request for directory buckets
    handler.add("GET", "/", 403)
    with webserver.install_http_handler(handler):
        dir_contents = gdal.ReadDir("/vsis3/")
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
    # This one is a s3express request for directory buckets
    handler.add(
        "GET",
        "/",
        200,
        {"Content-type": "application/xml"},
        """<?xml version="1.0" encoding="UTF-8"?>
        <ListAllMyDirectoryBucketsResult>
        <Buckets>
            <Bucket>
                <Name>mydirectorybucket</Name>
            </Bucket>
        </Buckets>
        </ListAllMyDirectoryBucketsResult>
        """,
    )
    with webserver.install_http_handler(handler):
        dir_contents = gdal.ReadDir("/vsis3/")
    assert dir_contents == ["mybucket", "mydirectorybucket"]

    # Test temporary redirect
    handler = webserver.SequentialHandler()

    class HandlerClass:
        def __init__(self, response_value):
            self.old_authorization = None
            self.response_value = response_value

        def method_req_1(self, request):
            if request.headers["Host"].find("127.0.0.1") < 0:
                sys.stderr.write("Bad headers: %s\n" % str(request.headers))
                request.send_response(403)
                return
            self.old_authorization = request.headers["Authorization"]
            request.protocol_version = "HTTP/1.1"
            request.send_response(307)
            response = """<?xml version="1.0" encoding="UTF-8"?>
            <Error>
            <Message>bla</Message>
            <Code>TemporaryRedirect</Code>
            <Endpoint>localhost:%d</Endpoint>
            </Error>""" % request.server.port
            response = "%x\r\n%s\r\n0\r\n\r\n" % (len(response), response)
            request.send_header("Content-type", "application/xml")
            request.send_header("Transfer-Encoding", "chunked")
            request.end_headers()
            request.wfile.write(response.encode("ascii"))

        def method_req_2(self, request):
            if request.headers["Host"].find("localhost") < 0:
                sys.stderr.write("Bad headers: %s\n" % str(request.headers))
                request.send_response(403)
                return
            if self.old_authorization == request.headers["Authorization"]:
                sys.stderr.write(
                    "Should have get a different Authorization. "
                    "Bad headers: %s\n" % str(request.headers)
                )
                request.send_response(403)
                return
            request.protocol_version = "HTTP/1.1"
            request.send_response(200)
            request.send_header("Content-type", "application/xml")
            response = self.response_value
            request.send_header("Content-Length", len(response))
            request.end_headers()
            request.wfile.write(response.encode("ascii"))

    h = HandlerClass("""<?xml version="1.0" encoding="UTF-8"?>
                <ListBucketResult>
                    <Prefix></Prefix>
                    <CommonPrefixes>
                        <Prefix>test</Prefix>
                    </CommonPrefixes>
                </ListBucketResult>
            """)
    handler.add(
        "GET",
        "/s3_test_temporary_redirect_read_dir/?delimiter=%2F&list-type=2",
        custom_method=h.method_req_1,
    )
    handler.add(
        "GET",
        "/s3_test_temporary_redirect_read_dir/?delimiter=%2F&list-type=2",
        custom_method=h.method_req_2,
    )

    with webserver.install_http_handler(handler):
        dir_contents = gdal.ReadDir("/vsis3/s3_test_temporary_redirect_read_dir")
    assert dir_contents == ["test"]

    # Retry on the same bucket and check that the redirection was indeed
    # temporary
    handler = webserver.SequentialHandler()

    h = HandlerClass("""<?xml version="1.0" encoding="UTF-8"?>
            <ListBucketResult>
                <Prefix>test/</Prefix>
                <CommonPrefixes>
                    <Prefix>test/test2</Prefix>
                </CommonPrefixes>
            </ListBucketResult>
        """)
    handler.add(
        "GET",
        "/s3_test_temporary_redirect_read_dir/?delimiter=%2F&list-type=2&prefix=test%2F",
        custom_method=h.method_req_1,
    )
    handler.add(
        "GET",
        "/s3_test_temporary_redirect_read_dir/?delimiter=%2F&list-type=2&prefix=test%2F",
        custom_method=h.method_req_2,
    )

    with webserver.install_http_handler(handler):
        dir_contents = gdal.ReadDir("/vsis3/s3_test_temporary_redirect_read_dir/test")
    assert dir_contents == ["test2"]


###############################################################################
# Test OpenDir() with a fake AWS server


def test_vsis3_opendir(aws_test_config, webserver_port):
    # Unlimited depth
    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/vsis3_opendir/?list-type=2",
        200,
        {"Content-type": "application/xml"},
        """<?xml version="1.0" encoding="UTF-8"?>
            <ListBucketResult>
                <Prefix/>
                <Marker/>
                <Contents>
                    <Key>test.txt</Key>
                    <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                    <Size>40</Size>
                </Contents>
                <Contents>
                    <Key>subdir/</Key>
                    <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                    <Size>0</Size>
                </Contents>
                <Contents>
                    <Key>subdir/test.txt</Key>
                    <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                    <Size>5</Size>
                </Contents>
            </ListBucketResult>
        """,
    )
    with webserver.install_http_handler(handler):
        d = gdal.OpenDir("/vsis3/vsis3_opendir")
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

    entry = gdal.GetNextDirEntry(d)
    assert entry is None

    gdal.CloseDir(d)

    # Depth = 0
    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/vsis3_opendir/?delimiter=%2F&list-type=2",
        200,
        {"Content-type": "application/xml"},
        """<?xml version="1.0" encoding="UTF-8"?>
            <ListBucketResult>
                <Prefix/>
                <Marker/>
                <Contents>
                    <Key>test.txt</Key>
                    <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                    <Size>40</Size>
                </Contents>
                <CommonPrefixes>
                    <Prefix>subdir/</Prefix>
                </CommonPrefixes>
            </ListBucketResult>
        """,
    )
    with webserver.install_http_handler(handler):
        d = gdal.OpenDir("/vsis3/vsis3_opendir", 0)
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
    assert entry is None

    gdal.CloseDir(d)

    # Depth = 1
    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/vsis3_opendir/?delimiter=%2F&list-type=2",
        200,
        {"Content-type": "application/xml"},
        """<?xml version="1.0" encoding="UTF-8"?>
            <ListBucketResult>
                <Prefix/>
                <Marker/>
                <Contents>
                    <Key>test.txt</Key>
                    <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                    <Size>40</Size>
                </Contents>
                <CommonPrefixes>
                    <Prefix>subdir/</Prefix>
                </CommonPrefixes>
            </ListBucketResult>
        """,
    )
    with webserver.install_http_handler(handler):
        d = gdal.OpenDir("/vsis3/vsis3_opendir", 1)
        assert d is not None

    entry = gdal.GetNextDirEntry(d)
    assert entry.name == "test.txt"
    assert entry.size == 40
    assert entry.mode == 32768
    assert entry.mtime == 1

    entry = gdal.GetNextDirEntry(d)
    assert entry.name == "subdir"
    assert entry.mode == 16384

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/vsis3_opendir/?delimiter=%2F&list-type=2&prefix=subdir%2F",
        200,
        {"Content-type": "application/xml"},
        """<?xml version="1.0" encoding="UTF-8"?>
            <ListBucketResult>
                <Prefix>subdir/</Prefix>
                <Marker/>
                <Contents>
                    <Key>subdir/test.txt</Key>
                    <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                    <Size>5</Size>
                </Contents>
            </ListBucketResult>
        """,
    )
    with webserver.install_http_handler(handler):
        entry = gdal.GetNextDirEntry(d)
        assert entry.name == "subdir/test.txt"

    entry = gdal.GetNextDirEntry(d)
    assert entry is None

    gdal.CloseDir(d)

    # Prefix filtering on root of bucket
    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/vsis3_opendir/?list-type=2&prefix=my_prefix",
        200,
        {"Content-type": "application/xml"},
        """<?xml version="1.0" encoding="UTF-8"?>
            <ListBucketResult>
                <Prefix>my_prefix</Prefix>
                <Marker/>
                <Contents>
                    <Key>my_prefix_test.txt</Key>
                    <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                    <Size>40</Size>
                </Contents>
            </ListBucketResult>
        """,
    )
    with webserver.install_http_handler(handler):
        d = gdal.OpenDir("/vsis3/vsis3_opendir", -1, ["PREFIX=my_prefix"])
        assert d is not None

    entry = gdal.GetNextDirEntry(d)
    assert entry.name == "my_prefix_test.txt"
    assert entry.size == 40
    assert entry.mode == 32768
    assert entry.mtime == 1

    entry = gdal.GetNextDirEntry(d)
    assert entry is None

    gdal.CloseDir(d)

    # Prefix filtering on subdir
    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/vsis3_opendir/?list-type=2&prefix=some_dir%2Fmy_prefix",
        200,
        {"Content-type": "application/xml"},
        """<?xml version="1.0" encoding="UTF-8"?>
            <ListBucketResult>
                <Prefix>some_dir/my_prefix</Prefix>
                <Marker/>
                <Contents>
                    <Key>some_dir/my_prefix_test.txt</Key>
                    <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                    <Size>40</Size>
                </Contents>
            </ListBucketResult>
        """,
    )
    with webserver.install_http_handler(handler):
        d = gdal.OpenDir("/vsis3/vsis3_opendir/some_dir", -1, ["PREFIX=my_prefix"])
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
        "/vsis3/vsis3_opendir/some_dir/my_prefix_test.txt",
        (
            gdal.VSI_STAT_EXISTS_FLAG
            | gdal.VSI_STAT_NATURE_FLAG
            | gdal.VSI_STAT_SIZE_FLAG
            | gdal.VSI_STAT_CACHE_ONLY
        ),
    )
    assert s
    assert (s.mode & 32768) != 0
    assert s.size == 40
    assert s.mtime == 1

    # No network access done
    assert (
        gdal.VSIStatL(
            "/vsis3/vsis3_opendir/some_dir/i_do_not_exist.txt",
            (
                gdal.VSI_STAT_EXISTS_FLAG
                | gdal.VSI_STAT_NATURE_FLAG
                | gdal.VSI_STAT_SIZE_FLAG
                | gdal.VSI_STAT_CACHE_ONLY
            ),
        )
        is None
    )


###############################################################################
# Test OpenDir(['SYNTHETIZE_MISSING_DIRECTORIES=YES']) with a fake AWS server


def test_vsis3_opendir_synthetize_missing_directory(aws_test_config, webserver_port):
    # Unlimited depth
    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/vsis3_opendir/?list-type=2&prefix=maindir%2F",
        200,
        {"Content-type": "application/xml"},
        """<?xml version="1.0" encoding="UTF-8"?>
            <ListBucketResult>
                <Prefix>maindir/</Prefix>
                <Marker/>
                <Contents>
                    <Key>maindir/test.txt</Key>
                    <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                    <Size>40</Size>
                </Contents>
                <Contents>
                    <Key>maindir/explicit_subdir/</Key>
                    <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                    <Size>0</Size>
                </Contents>
                <Contents>
                    <Key>maindir/explicit_subdir/test.txt</Key>
                    <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                    <Size>5</Size>
                </Contents>
                <Contents>
                    <Key>maindir/explicit_subdir/implicit_subdir/test.txt</Key>
                    <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                    <Size>5</Size>
                </Contents>
                <Contents>
                    <Key>maindir/explicit_subdir/implicit_subdir/test2.txt</Key>
                    <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                    <Size>5</Size>
                </Contents>
                <Contents>
                    <Key>maindir/explicit_subdir/implicit_subdir2/test.txt</Key>
                    <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                    <Size>5</Size>
                </Contents>
                <Contents>
                    <Key>maindir/implicit_subdir/implicit_subdir2/test.txt</Key>
                    <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                    <Size>5</Size>
                </Contents>
            </ListBucketResult>
        """,
    )
    with webserver.install_http_handler(handler):
        d = gdal.OpenDir(
            "/vsis3/vsis3_opendir/maindir", -1, ["SYNTHETIZE_MISSING_DIRECTORIES=YES"]
        )
    assert d is not None

    entry = gdal.GetNextDirEntry(d)
    assert entry.name == "test.txt"
    assert entry.size == 40
    assert entry.mode == 32768
    assert entry.mtime == 1

    entry = gdal.GetNextDirEntry(d)
    assert entry.name == "explicit_subdir"
    assert entry.mode == 16384

    entry = gdal.GetNextDirEntry(d)
    assert entry.name == "explicit_subdir/test.txt"

    entry = gdal.GetNextDirEntry(d)
    assert entry.name == "explicit_subdir/implicit_subdir"
    assert entry.mode == 16384

    entry = gdal.GetNextDirEntry(d)
    assert entry.name == "explicit_subdir/implicit_subdir/test.txt"

    entry = gdal.GetNextDirEntry(d)
    assert entry.name == "explicit_subdir/implicit_subdir/test2.txt"

    entry = gdal.GetNextDirEntry(d)
    assert entry.name == "explicit_subdir/implicit_subdir2"
    assert entry.mode == 16384

    entry = gdal.GetNextDirEntry(d)
    assert entry.name == "explicit_subdir/implicit_subdir2/test.txt"

    entry = gdal.GetNextDirEntry(d)
    assert entry.name == "implicit_subdir"
    assert entry.mode == 16384

    entry = gdal.GetNextDirEntry(d)
    assert entry.name == "implicit_subdir/implicit_subdir2"
    assert entry.mode == 16384

    entry = gdal.GetNextDirEntry(d)
    assert entry.name == "implicit_subdir/implicit_subdir2/test.txt"

    entry = gdal.GetNextDirEntry(d)
    assert entry is None

    gdal.CloseDir(d)


###############################################################################
# Test OpenDir() with a fake AWS server on /vsis3/ root


def test_vsis3_opendir_from_prefix(aws_test_config, webserver_port):

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
                <Name>bucket1</Name>
            </Bucket>
            <Bucket>
                <Name>bucket2</Name>
            </Bucket>
        </Buckets>
        </ListAllMyBucketsResult>
        """,
    )
    handler.add(
        "GET",
        "/bucket1/?list-type=2",
        200,
        {"Content-type": "application/xml"},
        """<?xml version="1.0" encoding="UTF-8"?>
            <ListBucketResult>
                <Prefix/>
                <Marker/>
                <Contents>
                    <Key>test1.txt</Key>
                    <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                    <Size>40</Size>
                </Contents>
                <Contents>
                    <Key>test2.txt</Key>
                    <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                    <Size>40</Size>
                </Contents>
            </ListBucketResult>
        """,
    )
    handler.add(
        "GET",
        "/bucket2/?list-type=2",
        200,
        {"Content-type": "application/xml"},
        """<?xml version="1.0" encoding="UTF-8"?>
            <ListBucketResult>
                <Prefix/>
                <Marker/>
                <Contents>
                    <Key>test3.txt</Key>
                    <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                    <Size>40</Size>
                </Contents>
            </ListBucketResult>
        """,
    )
    # This one is a s3express request for directory buckets
    handler.add("GET", "/", 403)
    with webserver.install_http_handler(handler):
        d = gdal.OpenDir("/vsis3/")
        assert d is not None
        try:

            entry = gdal.GetNextDirEntry(d)
            assert entry.name == "bucket1"
            assert entry.mode == 16384

            entry = gdal.GetNextDirEntry(d)
            assert entry.name == "bucket1/test1.txt"
            assert entry.mode == 32768

            entry = gdal.GetNextDirEntry(d)
            assert entry.name == "bucket1/test2.txt"
            assert entry.mode == 32768

            entry = gdal.GetNextDirEntry(d)
            assert entry.name == "bucket2"
            assert entry.mode == 16384

            entry = gdal.GetNextDirEntry(d)
            assert entry.name == "bucket2/test3.txt"
            assert entry.mode == 32768

            assert gdal.GetNextDirEntry(d) is None

        finally:
            gdal.CloseDir(d)


###############################################################################
# Test simple PUT support with a fake AWS server


def test_vsis3_4(aws_test_config, webserver_port):
    with webserver.install_http_handler(webserver.SequentialHandler()):
        with gdal.quiet_errors():
            f = gdal.VSIFOpenL("/vsis3/s3_fake_bucket3", "wb")
    assert f is None

    handler = webserver.SequentialHandler()
    handler.add(
        "GET", "/s3_fake_bucket3/empty_file.bin", 200, {"Connection": "close"}, "foo"
    )
    with webserver.install_http_handler(handler):
        assert gdal.VSIStatL("/vsis3/s3_fake_bucket3/empty_file.bin").size == 3

    handler = webserver.SequentialHandler()
    handler.add(
        "HEAD",
        "/s3_fake_bucket3/empty_file.bin",
        200,
        {"Connection": "close", "Content-Length": "3"},
    )
    with webserver.install_http_handler(handler):
        assert (
            gdal.VSIStatL("/vsis3_streaming/s3_fake_bucket3/empty_file.bin").size == 3
        )

    # Empty file
    handler = webserver.SequentialHandler()

    handler.add(
        "PUT",
        "/s3_fake_bucket3/empty_file.bin",
        200,
        headers={"Content-Length": "0"},
        expected_headers={
            "Content-Length": "0",
            "Content-Type": "foo",
            "Content-Encoding": "bar",
            "x-amz-storage-class": "GLACIER",
        },
    )

    with webserver.install_http_handler(handler):
        f = gdal.VSIFOpenExL(
            "/vsis3/s3_fake_bucket3/empty_file.bin",
            "wb",
            0,
            ["Content-Type=foo", "Content-Encoding=bar", "x-amz-storage-class=GLACIER"],
        )
        assert f is not None
        gdal.ErrorReset()
        gdal.VSIFCloseL(f)
    assert gdal.GetLastErrorMsg() == ""

    handler = webserver.SequentialHandler()
    handler.add(
        "GET", "/s3_fake_bucket3/empty_file.bin", 200, {"Connection": "close"}, ""
    )
    with webserver.install_http_handler(handler):
        assert gdal.VSIStatL("/vsis3/s3_fake_bucket3/empty_file.bin").size == 0

    # Check that the update of the file results in the /vsis3_streaming/
    # cached properties to be updated
    assert gdal.VSIStatL("/vsis3_streaming/s3_fake_bucket3/empty_file.bin").size == 0

    # Invalid seek
    handler = webserver.SequentialHandler()
    with webserver.install_http_handler(handler):
        f = gdal.VSIFOpenL("/vsis3/s3_fake_bucket3/empty_file.bin", "wb")
        assert f is not None
        with gdal.quiet_errors():
            ret = gdal.VSIFSeekL(f, 1, 0)
        assert ret != 0
        gdal.VSIFCloseL(f)

    # Invalid read
    handler = webserver.SequentialHandler()
    with webserver.install_http_handler(handler):
        f = gdal.VSIFOpenL("/vsis3/s3_fake_bucket3/empty_file.bin", "wb")
        assert f is not None
        with gdal.quiet_errors():
            ret = gdal.VSIFReadL(1, 1, f)
        assert not ret
        gdal.VSIFCloseL(f)

    # Error case
    handler = webserver.SequentialHandler()
    handler.add("PUT", "/s3_fake_bucket3/empty_file_error.bin", 403)
    with webserver.install_http_handler(handler):
        f = gdal.VSIFOpenL("/vsis3/s3_fake_bucket3/empty_file_error.bin", "wb")
        assert f is not None
        gdal.ErrorReset()
        with gdal.quiet_errors():
            gdal.VSIFCloseL(f)
    assert gdal.GetLastErrorMsg() != ""

    # Nominal case

    gdal.NetworkStatsReset()
    with gdaltest.config_option(
        "CPL_VSIL_NETWORK_STATS_ENABLED", "YES", thread_local=False
    ):
        with webserver.install_http_handler(webserver.SequentialHandler()):
            f = gdal.VSIFOpenL("/vsis3/s3_fake_bucket3/another_file.bin", "wb")
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

        handler.add("PUT", "/s3_fake_bucket3/another_file.bin", custom_method=method)

        gdal.ErrorReset()
        with webserver.install_http_handler(handler):
            gdal.VSIFCloseL(f)
        assert gdal.GetLastErrorMsg() == ""

    j = json.loads(gdal.NetworkStatsGetAsSerializedJSON())
    assert j == {
        "methods": {"PUT": {"count": 1, "uploaded_bytes": 6}},
        "handlers": {
            "vsis3": {
                "files": {
                    "/vsis3/s3_fake_bucket3/another_file.bin": {
                        "methods": {"PUT": {"count": 1, "uploaded_bytes": 6}},
                        "actions": {
                            "Write": {
                                "methods": {"PUT": {"count": 1, "uploaded_bytes": 6}}
                            }
                        },
                    }
                },
                "methods": {"PUT": {"count": 1, "uploaded_bytes": 6}},
            }
        },
    }

    gdal.NetworkStatsReset()

    # Redirect case
    with webserver.install_http_handler(webserver.SequentialHandler()):
        f = gdal.VSIFOpenL("/vsis3/s3_fake_bucket3/redirect.tif", "wb")
        assert f is not None
        assert gdal.VSIFWriteL("foobar", 1, 6, f) == 6

    handler = webserver.SequentialHandler()

    def method(request):
        request.protocol_version = "HTTP/1.1"
        if request.headers["Authorization"].find("us-east-1") >= 0:
            request.send_response(400)
            response = """<?xml version="1.0" encoding="UTF-8"?>
            <Error>
            <Message>bla</Message>
            <Code>AuthorizationHeaderMalformed</Code>
            <Region>us-west-2</Region>
            </Error>"""
            response = "%x\r\n%s\r\n0\r\n\r\n" % (len(response), response)
            request.send_header("Content-type", "application/xml")
            request.send_header("Transfer-Encoding", "chunked")
            request.end_headers()
            request.wfile.write(response.encode("ascii"))
        elif request.headers["Authorization"].find("us-west-2") >= 0:
            if (
                request.headers["Content-Length"] != "6"
                or request.headers["Content-Type"] != "image/tiff"
            ):
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
        else:
            sys.stderr.write("Bad headers: %s\n" % str(request.headers))
            request.send_response(403)
            request.send_header("Content-Length", 0)
            request.end_headers()

    handler.add("PUT", "/s3_fake_bucket3/redirect.tif", custom_method=method)
    handler.add("PUT", "/s3_fake_bucket3/redirect.tif", custom_method=method)

    gdal.ErrorReset()
    with webserver.install_http_handler(handler):
        gdal.VSIFCloseL(f)
    assert gdal.GetLastErrorMsg() == ""


###############################################################################
# Test that PUT invalidates cached data


def test_vsis3_put_invalidate(aws_test_config, webserver_port):

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add("GET", "/s3_fake_bucket3/?delimiter=%2F&list-type=2", 200)
    handler.add("GET", "/s3_fake_bucket3/test_put_invalidate.bin", 200, {}, b"foo")
    handler.add("GET", "/s3_fake_bucket3/test_put_invalidate.bin", 200, {}, b"foo")

    with webserver.install_http_handler(handler):
        f = gdal.VSIFOpenL("/vsis3/s3_fake_bucket3/test_put_invalidate.bin", "rb")
        assert f is not None
        try:
            assert gdal.VSIFReadL(3, 1, f) == b"foo"
        finally:
            gdal.VSIFCloseL(f)

    handler = webserver.SequentialHandler()
    with webserver.install_http_handler(handler):
        f = gdal.VSIFOpenL("/vsis3/s3_fake_bucket3/test_put_invalidate.bin", "rb")
        assert f is not None
        try:
            assert gdal.VSIFReadL(3, 1, f) == b"foo"
        finally:
            gdal.VSIFCloseL(f)

    handler = webserver.SequentialHandler()
    handler.add("PUT", "/s3_fake_bucket3/test_put_invalidate.bin", 200)
    with webserver.install_http_handler(handler):
        f = gdal.VSIFOpenL("/vsis3/s3_fake_bucket3/test_put_invalidate.bin", "wb")
        assert f is not None
        try:
            assert gdal.VSIFWriteL("barbaw", 1, 6, f) == 6
        finally:
            gdal.VSIFCloseL(f)

    handler = webserver.SequentialHandler()
    handler.add("GET", "/s3_fake_bucket3/?delimiter=%2F&list-type=2", 200)
    handler.add("GET", "/s3_fake_bucket3/test_put_invalidate.bin", 200, {}, b"barbaw")
    handler.add("GET", "/s3_fake_bucket3/test_put_invalidate.bin", 200, {}, b"barbaw")

    with webserver.install_http_handler(handler):
        f = gdal.VSIFOpenL("/vsis3/s3_fake_bucket3/test_put_invalidate.bin", "rb")
        assert f is not None
        try:
            assert gdal.VSIFReadL(6, 1, f) == b"barbaw"
        finally:
            gdal.VSIFCloseL(f)


###############################################################################
# Test that CopyFile invalidates cached data


def test_vsis3_copy_invalidate(aws_test_config, webserver_port, tmp_vsimem):

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add("GET", "/s3_fake_bucket3/?delimiter=%2F&list-type=2", 200)
    handler.add("GET", "/s3_fake_bucket3/test_put_invalidate.bin", 200, {}, b"foo")
    handler.add("GET", "/s3_fake_bucket3/test_put_invalidate.bin", 200, {}, b"foo")

    with webserver.install_http_handler(handler):
        f = gdal.VSIFOpenL("/vsis3/s3_fake_bucket3/test_put_invalidate.bin", "rb")
        assert f is not None
        try:
            assert gdal.VSIFReadL(3, 1, f) == b"foo"
        finally:
            gdal.VSIFCloseL(f)

    handler = webserver.SequentialHandler()
    with webserver.install_http_handler(handler):
        f = gdal.VSIFOpenL("/vsis3/s3_fake_bucket3/test_put_invalidate.bin", "rb")
        assert f is not None
        try:
            assert gdal.VSIFReadL(3, 1, f) == b"foo"
        finally:
            gdal.VSIFCloseL(f)

    handler = webserver.SequentialHandler()
    handler.add("PUT", "/s3_fake_bucket3/test_put_invalidate.bin", 200)
    memfilename = str(tmp_vsimem / "tmp.bin")
    with webserver.install_http_handler(handler), gdaltest.tempfile(
        memfilename, b"barbaw"
    ):
        gdal.CopyFile(memfilename, "/vsis3/s3_fake_bucket3/test_put_invalidate.bin")

    handler = webserver.SequentialHandler()
    handler.add("GET", "/s3_fake_bucket3/?delimiter=%2F&list-type=2", 200)
    handler.add("GET", "/s3_fake_bucket3/test_put_invalidate.bin", 200, {}, b"barbaw")
    handler.add("GET", "/s3_fake_bucket3/test_put_invalidate.bin", 200, {}, b"barbaw")

    with webserver.install_http_handler(handler):
        f = gdal.VSIFOpenL("/vsis3/s3_fake_bucket3/test_put_invalidate.bin", "rb")
        assert f is not None
        try:
            assert gdal.VSIFReadL(6, 1, f) == b"barbaw"
        finally:
            gdal.VSIFCloseL(f)


###############################################################################
# Test simple PUT support with retry logic


def test_vsis3_write_single_put_retry(aws_test_config, webserver_port):
    with gdaltest.config_options(
        {"GDAL_HTTP_MAX_RETRY": "2", "GDAL_HTTP_RETRY_DELAY": "0.01"},
        thread_local=False,
    ):

        with webserver.install_http_handler(webserver.SequentialHandler()):
            f = gdal.VSIFOpenL("/vsis3/s3_fake_bucket3/put_with_retry.bin", "wb")
            assert f is not None
            assert gdal.VSIFWriteL("foo", 1, 3, f) == 3

        handler = webserver.SequentialHandler()

        def method(request):
            if request.headers["Content-Length"] != "3":
                sys.stderr.write(
                    "Did not get expected headers: %s\n" % str(request.headers)
                )
                request.send_response(400)
                request.send_header("Content-Length", 0)
                request.end_headers()
                return

            request.wfile.write("HTTP/1.1 100 Continue\r\n\r\n".encode("ascii"))

            content = request.rfile.read(3).decode("ascii")
            if content != "foo":
                sys.stderr.write("Did not get expected content: %s\n" % content)
                request.send_response(400)
                request.send_header("Content-Length", 0)
                request.end_headers()
                return

            request.send_response(200)
            request.send_header("Content-Length", 0)
            request.end_headers()

        handler.add("PUT", "/s3_fake_bucket3/put_with_retry.bin", 502)
        handler.add("PUT", "/s3_fake_bucket3/put_with_retry.bin", custom_method=method)

        with gdal.quiet_errors():
            with webserver.install_http_handler(handler):
                gdal.VSIFCloseL(f)


###############################################################################
# Test simple DELETE support with a fake AWS server


def test_vsis3_5(aws_test_config, webserver_port):
    with webserver.install_http_handler(webserver.SequentialHandler()):
        with gdal.quiet_errors():
            ret = gdal.Unlink("/vsis3/foo")
    assert ret != 0

    handler = webserver.SequentialHandler()
    handler.add(
        "GET", "/s3_delete_bucket/delete_file", 200, {"Connection": "close"}, "foo"
    )
    with webserver.install_http_handler(handler):
        assert gdal.VSIStatL("/vsis3/s3_delete_bucket/delete_file").size == 3

    handler = webserver.SequentialHandler()
    with webserver.install_http_handler(handler):
        assert gdal.VSIStatL("/vsis3/s3_delete_bucket/delete_file").size == 3

    handler = webserver.SequentialHandler()
    handler.add("DELETE", "/s3_delete_bucket/delete_file", 204)
    with webserver.install_http_handler(handler):
        ret = gdal.Unlink("/vsis3/s3_delete_bucket/delete_file")
    assert ret == 0

    handler = webserver.SequentialHandler()
    handler.add("GET", "/s3_delete_bucket/delete_file", 404, {"Connection": "close"})
    handler.add(
        "GET",
        "/s3_delete_bucket/?delimiter=%2F&list-type=2&max-keys=100&prefix=delete_file%2F",
        404,
        {"Connection": "close"},
    )
    with webserver.install_http_handler(handler):
        assert gdal.VSIStatL("/vsis3/s3_delete_bucket/delete_file") is None

    handler = webserver.SequentialHandler()
    handler.add("GET", "/s3_delete_bucket/delete_file_error", 200)
    handler.add("DELETE", "/s3_delete_bucket/delete_file_error", 403)
    with webserver.install_http_handler(handler):
        with gdal.quiet_errors():
            ret = gdal.Unlink("/vsis3/s3_delete_bucket/delete_file_error")
    assert ret != 0

    handler = webserver.SequentialHandler()

    handler.add("GET", "/s3_delete_bucket/redirect", 200)

    def method(request):
        request.protocol_version = "HTTP/1.1"
        if request.headers["Authorization"].find("us-east-1") >= 0:
            request.send_response(400)
            response = """<?xml version="1.0" encoding="UTF-8"?>
            <Error>
            <Message>bla</Message>
            <Code>AuthorizationHeaderMalformed</Code>
            <Region>us-west-2</Region>
            </Error>"""
            response = "%x\r\n%s\r\n0\r\n\r\n" % (len(response), response)
            request.send_header("Content-type", "application/xml")
            request.send_header("Transfer-Encoding", "chunked")
            request.end_headers()
            request.wfile.write(response.encode("ascii"))
        elif request.headers["Authorization"].find("us-west-2") >= 0:
            request.send_response(204)
            request.send_header("Content-Length", 0)
            request.end_headers()
        else:
            sys.stderr.write("Bad headers: %s\n" % str(request.headers))
            request.send_response(403)
            request.send_header("Content-Length", 0)
            request.end_headers()

    handler.add("DELETE", "/s3_delete_bucket/redirect", custom_method=method)
    handler.add("DELETE", "/s3_delete_bucket/redirect", custom_method=method)

    with webserver.install_http_handler(handler):
        ret = gdal.Unlink("/vsis3/s3_delete_bucket/redirect")
    assert ret == 0


###############################################################################
# Test DeleteObjects with a fake AWS server


def test_vsis3_unlink_batch(aws_test_config, webserver_port):

    handler = webserver.SequentialHandler()
    handler.add(
        "POST",
        "/unlink_batch/?delete",
        200,
        {"Content-type": "application/xml"},
        """<DeleteResult xmlns="http://s3.amazonaws.com/doc/2006-03-01/">
        <Deleted>
        <Key>foo</Key>
        </Deleted>
        <Deleted>
        <Key>bar/baz</Key>
        </Deleted>
        </DeleteResult>""",
        expected_headers={"Content-MD5": "Ze0X4LdlTwCsT+WpNxD9FA=="},
        expected_body=b"""<?xml version="1.0" encoding="UTF-8"?>
<Delete xmlns="http://s3.amazonaws.com/doc/2006-03-01/">
  <Object>
    <Key>foo</Key>
  </Object>
  <Object>
    <Key>bar/baz</Key>
  </Object>
</Delete>
""",
    )

    handler.add(
        "POST",
        "/unlink_batch/?delete",
        200,
        {},
        """
        <DeleteResult xmlns="http://s3.amazonaws.com/doc/2006-03-01/">
        <Deleted>
        <Key>baw</Key>
        </Deleted>
        </DeleteResult>""",
    )

    with gdaltest.config_option("CPL_VSIS3_UNLINK_BATCH_SIZE", "2", thread_local=False):
        with webserver.install_http_handler(handler):
            ret = gdal.UnlinkBatch(
                [
                    "/vsis3/unlink_batch/foo",
                    "/vsis3/unlink_batch/bar/baz",
                    "/vsis3/unlink_batch/baw",
                ]
            )
    assert ret

    handler = webserver.SequentialHandler()
    handler.add(
        "POST",
        "/unlink_batch/?delete",
        200,
        {},
        """
        <DeleteResult xmlns="http://s3.amazonaws.com/doc/2006-03-01/">
        <Failed>
        <Key>foo</Key>
        </Failed>
        </DeleteResult>""",
    )

    with webserver.install_http_handler(handler):
        ret = gdal.UnlinkBatch(["/vsis3/unlink_batch/foo"])
    assert not ret


###############################################################################
# Test RmdirRecursive() with a fake AWS server


def test_vsis3_rmdir_recursive(aws_test_config, webserver_port):
    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/test_rmdir_recursive/?list-type=2&prefix=somedir%2F",
        200,
        {"Content-type": "application/xml"},
        """<?xml version="1.0" encoding="UTF-8"?>
        <ListBucketResult>
            <Prefix>somedir/</Prefix>
            <Marker/>
            <Contents>
                <Key>somedir/test.txt</Key>
                <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                <Size>40</Size>
            </Contents>
            <Contents>
                <Key>somedir/subdir/</Key>
                <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                <Size>0</Size>
            </Contents>
            <Contents>
                <Key>somedir/subdir/test.txt</Key>
                <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                <Size>5</Size>
            </Contents>
        </ListBucketResult>
        """,
    )

    handler.add(
        "POST",
        "/test_rmdir_recursive/?delete",
        200,
        {"Content-type": "application/xml"},
        """
        <DeleteResult xmlns="http://s3.amazonaws.com/doc/2006-03-01/">
        <Deleted>
        <Key>somedir/test.txt</Key>
        </Deleted>
        <Deleted>
        <Key>somedir/subdir/</Key>
        </Deleted>
        </DeleteResult>""",
        expected_body=b"""<?xml version="1.0" encoding="UTF-8"?>
<Delete xmlns="http://s3.amazonaws.com/doc/2006-03-01/">
  <Object>
    <Key>somedir/test.txt</Key>
  </Object>
  <Object>
    <Key>somedir/subdir/</Key>
  </Object>
</Delete>
""",
    )

    handler.add(
        "POST",
        "/test_rmdir_recursive/?delete",
        200,
        {"Content-type": "application/xml"},
        """
        <DeleteResult xmlns="http://s3.amazonaws.com/doc/2006-03-01/">
        <Deleted>
        <Key>somedir/subdir/test.txt</Key>
        </Deleted>
        <Deleted>
        <Key>somedir/</Key>
        </Deleted>
        </DeleteResult>""",
        expected_body=b"""<?xml version="1.0" encoding="UTF-8"?>
<Delete xmlns="http://s3.amazonaws.com/doc/2006-03-01/">
  <Object>
    <Key>somedir/subdir/test.txt</Key>
  </Object>
  <Object>
    <Key>somedir/</Key>
  </Object>
</Delete>
""",
    )

    with gdaltest.config_option("CPL_VSIS3_UNLINK_BATCH_SIZE", "2", thread_local=False):
        with webserver.install_http_handler(handler):
            assert gdal.RmdirRecursive("/vsis3/test_rmdir_recursive/somedir") == 0


###############################################################################
# Test RmdirRecursive() with CPL_VSIS3_USE_BASE_RMDIR_RECURSIVE=YES


def test_vsis3_rmdir_recursive_no_batch_deletion(aws_test_config, webserver_port):

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/test_vsis3_rmdir_recursive_no_batch_deletion/?list-type=2&prefix=somedir%2F",
        200,
        {"Content-type": "application/xml"},
        """<?xml version="1.0" encoding="UTF-8"?>
        <ListBucketResult>
            <Prefix>somedir/</Prefix>
            <Marker/>
            <Contents>
                <Key>somedir/test.txt</Key>
                <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                <Size>40</Size>
            </Contents>
            <Contents>
                <Key>somedir/test2.txt</Key>
                <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                <Size>40</Size>
            </Contents>
        </ListBucketResult>
        """,
    )
    handler.add(
        "DELETE",
        "/test_vsis3_rmdir_recursive_no_batch_deletion/somedir/test.txt",
        204,
    )
    handler.add(
        "DELETE",
        "/test_vsis3_rmdir_recursive_no_batch_deletion/somedir/test2.txt",
        204,
    )
    handler.add("GET", "/test_vsis3_rmdir_recursive_no_batch_deletion/somedir/", 404)
    handler.add(
        "GET",
        "/test_vsis3_rmdir_recursive_no_batch_deletion/?delimiter=%2F&list-type=2&max-keys=100&prefix=somedir%2F",
        200,
        {"Content-type": "application/xml"},
        """<?xml version="1.0" encoding="UTF-8"?>
            <ListBucketResult>
                <Prefix>somedir/</Prefix>
                <Contents/>
            </ListBucketResult>
            """,
    )
    handler.add(
        "DELETE",
        "/test_vsis3_rmdir_recursive_no_batch_deletion/somedir/",
        204,
    )
    gdal.SetPathSpecificOption(
        "/vsis3/test_vsis3_rmdir_recursive_no_batch_deletion",
        "CPL_VSIS3_USE_BASE_RMDIR_RECURSIVE",
        "YES",
    )
    with webserver.install_http_handler(handler):
        assert (
            gdal.RmdirRecursive(
                "/vsis3/test_vsis3_rmdir_recursive_no_batch_deletion/somedir"
            )
            == 0
        )
    gdal.ClearPathSpecificOptions()


###############################################################################
# Test multipart upload with a fake AWS server


@pytest.mark.skipif(gdaltest.is_ci(), reason="randomly fails on CI")
def test_vsis3_6(aws_test_config, webserver_port):
    with gdaltest.config_option("VSIS3_CHUNK_SIZE", "1", thread_local=False):  # 1 MB
        with webserver.install_http_handler(webserver.SequentialHandler()):
            f = gdal.VSIFOpenL("/vsis3/s3_fake_bucket4/large_file.tif", "wb")
    assert f is not None
    size = 1024 * 1024 + 1
    big_buffer = "a" * size

    handler = webserver.SequentialHandler()

    def method(request):
        request.protocol_version = "HTTP/1.1"
        if request.headers["Authorization"].find("us-east-1") >= 0:
            request.send_response(400)
            response = """<?xml version="1.0" encoding="UTF-8"?>
            <Error>
            <Message>bla</Message>
            <Code>AuthorizationHeaderMalformed</Code>
            <Region>us-west-2</Region>
            </Error>"""
            response = "%x\r\n%s\r\n0\r\n\r\n" % (len(response), response)
            request.send_header("Content-type", "application/xml")
            request.send_header("Transfer-Encoding", "chunked")
            request.end_headers()
            request.wfile.write(response.encode("ascii"))
        elif request.headers["Authorization"].find("us-west-2") >= 0:
            if request.headers["Content-Type"] != "image/tiff":
                sys.stderr.write(
                    "Did not get expected headers: %s\n" % str(request.headers)
                )
                request.send_response(400)
                request.send_header("Content-Length", 0)
                request.end_headers()
                return
            response = """<?xml version="1.0" encoding="UTF-8"?>
            <InitiateMultipartUploadResult>
            <UploadId>my_id</UploadId>
            </InitiateMultipartUploadResult>"""
            request.send_response(200)
            request.send_header("Content-type", "application/xml")
            request.send_header("Content-Length", len(response))
            request.end_headers()
            request.wfile.write(response.encode("ascii"))
        else:
            sys.stderr.write("Bad headers: %s\n" % str(request.headers))
            request.send_response(403)
            request.send_header("Content-Length", 0)
            request.end_headers()

    handler.add("POST", "/s3_fake_bucket4/large_file.tif?uploads", custom_method=method)
    handler.add("POST", "/s3_fake_bucket4/large_file.tif?uploads", custom_method=method)
    handler.add(
        "PUT",
        "/s3_fake_bucket4/large_file.tif?partNumber=1&uploadId=my_id",
        200,
        {"ETag": '"first_etag"', "Content-Length": "0"},
        b"",
        expected_headers={"Content-Length": "1048576"},
    )

    with webserver.install_http_handler(handler):
        ret = gdal.VSIFWriteL(big_buffer, 1, size, f)
    assert ret == size
    handler = webserver.SequentialHandler()

    handler.add(
        "PUT",
        "/s3_fake_bucket4/large_file.tif?partNumber=2&uploadId=my_id",
        200,
        {"ETag": '"second_etag"', "Content-Length": "0"},
        b"",
        expected_headers={"Content-Length": "1"},
    )

    handler.add(
        "POST",
        "/s3_fake_bucket4/large_file.tif?uploadId=my_id",
        200,
        {},
        b"",
        expected_headers={"Content-Length": "186"},
        expected_body=b"""<CompleteMultipartUpload>
<Part>
<PartNumber>1</PartNumber><ETag>"first_etag"</ETag></Part>
<Part>
<PartNumber>2</PartNumber><ETag>"second_etag"</ETag></Part>
</CompleteMultipartUpload>
""",
    )

    gdal.ErrorReset()
    with webserver.install_http_handler(handler):
        gdal.VSIFCloseL(f)
    assert gdal.GetLastErrorMsg() == ""

    handler = webserver.SequentialHandler()
    handler.add(
        "POST", "/s3_fake_bucket4/large_file_initiate_403_error.bin?uploads", 403
    )
    handler.add(
        "POST", "/s3_fake_bucket4/large_file_initiate_empty_result.bin?uploads", 200
    )
    handler.add(
        "POST",
        "/s3_fake_bucket4/large_file_initiate_invalid_xml_result.bin?uploads",
        200,
        {},
        "foo",
    )
    handler.add(
        "POST",
        "/s3_fake_bucket4/large_file_initiate_no_uploadId.bin?uploads",
        200,
        {},
        "<foo/>",
    )
    filenames = [
        "/vsis3/s3_fake_bucket4/large_file_initiate_403_error.bin",
        "/vsis3/s3_fake_bucket4/large_file_initiate_empty_result.bin",
        "/vsis3/s3_fake_bucket4/large_file_initiate_invalid_xml_result.bin",
        "/vsis3/s3_fake_bucket4/large_file_initiate_no_uploadId.bin",
    ]
    with webserver.install_http_handler(handler):
        for filename in filenames:
            # CHUNK_SIZE = 1 MB
            f = gdal.VSIFOpenExL(filename, "wb", False, ["CHUNK_SIZE=1"])
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
        "/s3_fake_bucket4/large_file_upload_part_403_error.bin?uploads",
        200,
        {},
        """xml version="1.0" encoding="UTF-8"?>
        <InitiateMultipartUploadResult>
        <UploadId>my_id</UploadId>
        </InitiateMultipartUploadResult>""",
    )
    handler.add(
        "PUT",
        (
            "/s3_fake_bucket4/large_file_upload_part_403_error.bin"
            "?partNumber=1&uploadId=my_id"
        ),
        403,
    )
    handler.add(
        "DELETE",
        ("/s3_fake_bucket4/large_file_upload_part_403_error.bin" "?uploadId=my_id"),
        204,
    )

    handler.add(
        "POST",
        "/s3_fake_bucket4/large_file_upload_part_no_etag.bin?uploads",
        200,
        {},
        """<?xml version="1.0" encoding="UTF-8"?>
        <InitiateMultipartUploadResult>
        <UploadId>my_id</UploadId>
        </InitiateMultipartUploadResult>""",
    )
    handler.add(
        "PUT",
        (
            "/s3_fake_bucket4/large_file_upload_part_no_etag.bin"
            "?partNumber=1&uploadId=my_id"
        ),
        200,
    )
    handler.add(
        "DELETE",
        "/s3_fake_bucket4/large_file_upload_part_no_etag.bin?uploadId=my_id",
        204,
    )

    filenames = [
        "/vsis3/s3_fake_bucket4/large_file_upload_part_403_error.bin",
        "/vsis3/s3_fake_bucket4/large_file_upload_part_no_etag.bin",
    ]
    with webserver.install_http_handler(handler):
        for filename in filenames:
            with gdaltest.config_option(
                "VSIS3_CHUNK_SIZE", "1", thread_local=False
            ):  # 1 MB
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
        "/s3_fake_bucket4/large_file_abortmultipart_403_error.bin?uploads",
        200,
        {},
        """<?xml version="1.0" encoding="UTF-8"?>
        <InitiateMultipartUploadResult>
        <UploadId>my_id</UploadId>
        </InitiateMultipartUploadResult>""",
    )
    handler.add(
        "PUT",
        (
            "/s3_fake_bucket4/large_file_abortmultipart_403_error.bin"
            "?partNumber=1&uploadId=my_id"
        ),
        403,
    )
    handler.add(
        "DELETE",
        ("/s3_fake_bucket4/large_file_abortmultipart_403_error.bin" "?uploadId=my_id"),
        403,
    )

    filename = "/vsis3/s3_fake_bucket4/large_file_abortmultipart_403_error.bin"
    with webserver.install_http_handler(handler):
        with gdaltest.config_option(
            "VSIS3_CHUNK_SIZE", "1", thread_local=False
        ):  # 1 MB
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
        "/s3_fake_bucket4/large_file_completemultipart_403_error.bin?uploads",
        200,
        {},
        """<?xml version="1.0" encoding="UTF-8"?>
        <InitiateMultipartUploadResult>
        <UploadId>my_id</UploadId>
        </InitiateMultipartUploadResult>""",
    )
    handler.add(
        "PUT",
        (
            "/s3_fake_bucket4/large_file_completemultipart_403_error.bin"
            "?partNumber=1&uploadId=my_id"
        ),
        200,
        {"ETag": "first_etag"},
        "",
    )
    handler.add(
        "PUT",
        (
            "/s3_fake_bucket4/large_file_completemultipart_403_error.bin"
            "?partNumber=2&uploadId=my_id"
        ),
        200,
        {"ETag": "second_etag"},
        "",
    )
    handler.add(
        "POST",
        (
            "/s3_fake_bucket4/large_file_completemultipart_403_error.bin"
            "?uploadId=my_id"
        ),
        403,
    )

    filename = "/vsis3/s3_fake_bucket4/large_file_completemultipart_403_error.bin"
    with webserver.install_http_handler(handler):
        with gdaltest.config_option(
            "VSIS3_CHUNK_SIZE", "1", thread_local=False
        ):  # 1 MB
            f = gdal.VSIFOpenL(filename, "wb")
            assert f is not None, filename
            ret = gdal.VSIFWriteL(big_buffer, 1, size, f)
            assert ret == size, filename
            gdal.ErrorReset()
            with gdal.quiet_errors():
                gdal.VSIFCloseL(f)
            assert gdal.GetLastErrorMsg() != "", filename


###############################################################################
# Test multipart upload with retry logic


@pytest.mark.skipif(gdaltest.is_ci(), reason="randomly fails on CI")
def test_vsis3_write_multipart_retry(aws_test_config, webserver_port):

    with gdaltest.config_options(
        {"GDAL_HTTP_MAX_RETRY": "2", "GDAL_HTTP_RETRY_DELAY": "0.01"},
        thread_local=False,
    ):

        with gdaltest.config_option(
            "VSIS3_CHUNK_SIZE", "1", thread_local=False
        ):  # 1 MB
            with webserver.install_http_handler(webserver.SequentialHandler()):
                f = gdal.VSIFOpenL("/vsis3/s3_fake_bucket4/large_file.tif", "wb")
        assert f is not None
        size = 1024 * 1024 + 1
        big_buffer = "a" * size

        handler = webserver.SequentialHandler()

        response = """<?xml version="1.0" encoding="UTF-8"?>
        <InitiateMultipartUploadResult>
        <UploadId>my_id</UploadId>
        </InitiateMultipartUploadResult>"""
        handler.add("POST", "/s3_fake_bucket4/large_file.tif?uploads", 502)
        handler.add(
            "POST",
            "/s3_fake_bucket4/large_file.tif?uploads",
            200,
            {
                "Content-type": "application/xml",
                "Content-Length": len(response),
                "Connection": "close",
            },
            response,
        )

        handler.add(
            "PUT", "/s3_fake_bucket4/large_file.tif?partNumber=1&uploadId=my_id", 502
        )
        handler.add(
            "PUT",
            "/s3_fake_bucket4/large_file.tif?partNumber=1&uploadId=my_id",
            200,
            {"Content-Length": "0", "ETag": '"first_etag"', "Connection": "close"},
            {},
        )

        with gdal.quiet_errors():
            with webserver.install_http_handler(handler):
                ret = gdal.VSIFWriteL(big_buffer, 1, size, f)
        assert ret == size
        handler = webserver.SequentialHandler()

        handler.add(
            "PUT",
            "/s3_fake_bucket4/large_file.tif?partNumber=2&uploadId=my_id",
            200,
            {"Content-Length": "0", "ETag": '"second_etag"', "Connection": "close"},
            {},
        )

        handler.add("POST", "/s3_fake_bucket4/large_file.tif?uploadId=my_id", 502)
        handler.add(
            "POST",
            "/s3_fake_bucket4/large_file.tif?uploadId=my_id",
            200,
            {"Content-Length": "0", "Connection": "close"},
            {},
        )

        with gdal.quiet_errors():
            with webserver.install_http_handler(handler):
                gdal.VSIFCloseL(f)


###############################################################################
# Test abort pending multipart uploads


def test_vsis3_abort_pending_uploads(aws_test_config, webserver_port):

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/my_bucket/?max-uploads=1&uploads",
        200,
        {},
        """<?xml version="1.0"?>
        <ListMultipartUploadsResult>
            <NextKeyMarker>next_key_marker</NextKeyMarker>
            <NextUploadIdMarker>next_upload_id_marker</NextUploadIdMarker>
            <IsTruncated>true</IsTruncated>
            <Upload>
                <Key>my_key</Key>
                <UploadId>my_upload_id</UploadId>
            </Upload>
        </ListMultipartUploadsResult>
        """,
    )
    handler.add(
        "GET",
        (
            "/my_bucket/?key-marker=next_key_marker&max-uploads=1"
            "&upload-id-marker=next_upload_id_marker&uploads"
        ),
        200,
        {},
        """<?xml version="1.0"?>
        <ListMultipartUploadsResult>
            <IsTruncated>false</IsTruncated>
            <Upload>
                <Key>my_key2</Key>
                <UploadId>my_upload_id2</UploadId>
            </Upload>
        </ListMultipartUploadsResult>
        """,
    )
    handler.add("DELETE", "/my_bucket/my_key?uploadId=my_upload_id", 204)
    handler.add("DELETE", "/my_bucket/my_key2?uploadId=my_upload_id2", 204)
    with webserver.install_http_handler(handler):
        with gdaltest.config_option(
            "CPL_VSIS3_LIST_UPLOADS_MAX", "1", thread_local=False
        ):
            assert gdal.AbortPendingUploads("/vsis3/my_bucket")


###############################################################################
# Test Mkdir() / Rmdir()


def test_vsis3_7(aws_test_config, webserver_port):

    handler = webserver.SequentialHandler()
    handler.add("GET", "/s3_bucket_test_mkdir/dir/", 404, {"Connection": "close"})
    handler.add(
        "GET",
        "/s3_bucket_test_mkdir/?delimiter=%2F&list-type=2&max-keys=100&prefix=dir%2F",
        404,
        {"Connection": "close"},
    )
    handler.add("PUT", "/s3_bucket_test_mkdir/dir/", 200)
    with webserver.install_http_handler(handler):
        ret = gdal.Mkdir("/vsis3/s3_bucket_test_mkdir/dir", 0)
    assert ret == 0

    assert stat.S_ISDIR(gdal.VSIStatL("/vsis3/s3_bucket_test_mkdir/dir").mode)

    dir_content = gdal.ReadDir("/vsis3/s3_bucket_test_mkdir/dir")
    assert dir_content == ["."]

    # Try creating already existing directory
    handler = webserver.SequentialHandler()
    handler.add("GET", "/s3_bucket_test_mkdir/dir/", 416, {"Connection": "close"})
    with webserver.install_http_handler(handler):
        ret = gdal.Mkdir("/vsis3/s3_bucket_test_mkdir/dir", 0)
    assert ret != 0

    handler = webserver.SequentialHandler()
    handler.add("DELETE", "/s3_bucket_test_mkdir/dir/", 204)
    with webserver.install_http_handler(handler):
        ret = gdal.Rmdir("/vsis3/s3_bucket_test_mkdir/dir")
    assert ret == 0

    # Try deleting already deleted directory
    handler = webserver.SequentialHandler()
    handler.add("GET", "/s3_bucket_test_mkdir/dir/", 404)
    handler.add(
        "GET",
        "/s3_bucket_test_mkdir/?delimiter=%2F&list-type=2&max-keys=100&prefix=dir%2F",
        404,
        {"Connection": "close"},
    )
    with webserver.install_http_handler(handler):
        ret = gdal.Rmdir("/vsis3/s3_bucket_test_mkdir/dir")
    assert ret != 0

    # Try deleting non-empty directory
    handler = webserver.SequentialHandler()
    handler.add("GET", "/s3_bucket_test_mkdir/dir_nonempty/", 416)
    handler.add(
        "GET",
        (
            "/s3_bucket_test_mkdir/?delimiter=%2F&list-type=2&max-keys=100"
            "&prefix=dir_nonempty%2F"
        ),
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
        ret = gdal.Rmdir("/vsis3/s3_bucket_test_mkdir/dir_nonempty")
    assert ret != 0

    # Try stat'ing a directory not ending with slash
    handler = webserver.SequentialHandler()
    handler.add("GET", "/s3_bucket_test_dir_stat/test_dir_stat", 400)
    handler.add(
        "GET",
        (
            "/s3_bucket_test_dir_stat/?delimiter=%2F&list-type=2&max-keys=100"
            "&prefix=test_dir_stat%2F"
        ),
        200,
        {"Content-type": "application/xml"},
        """<?xml version="1.0" encoding="UTF-8"?>
            <ListBucketResult>
                <Prefix>test_dir_stat/</Prefix>
                <Contents>
                    <Key>test_dir_stat/test.txt</Key>
                    <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                    <Size>40</Size>
                </Contents>
            </ListBucketResult>
        """,
    )
    with webserver.install_http_handler(handler):
        assert stat.S_ISDIR(
            gdal.VSIStatL("/vsis3/s3_bucket_test_dir_stat/test_dir_stat").mode
        )

    # Try ReadDi'ing a directory not ending with slash
    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/s3_bucket_test_readdir/?delimiter=%2F&list-type=2&prefix=test_dirread%2F",
        200,
        {"Content-type": "application/xml"},
        """<?xml version="1.0" encoding="UTF-8"?>
            <ListBucketResult>
                <Prefix>test_dirread/</Prefix>
                <Contents>
                    <Key>test_dirread/test.txt</Key>
                    <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                    <Size>40</Size>
                </Contents>
            </ListBucketResult>
        """,
    )
    with webserver.install_http_handler(handler):
        assert gdal.ReadDir("/vsis3/s3_bucket_test_readdir/test_dirread") is not None

    # Try stat'ing a directory ending with slash
    handler = webserver.SequentialHandler()
    handler.add("GET", "/s3_bucket_test_dir_stat_2/test_dir_stat/", 400)
    handler.add(
        "GET",
        (
            "/s3_bucket_test_dir_stat_2/?delimiter=%2F&list-type=2&max-keys=100"
            "&prefix=test_dir_stat%2F"
        ),
        200,
        {"Content-type": "application/xml"},
        """<?xml version="1.0" encoding="UTF-8"?>
            <ListBucketResult>
                <Prefix>test_dir_stat/</Prefix>
                <Contents>
                    <Key>test_dir_stat/test.txt</Key>
                    <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                    <Size>40</Size>
                </Contents>
            </ListBucketResult>
        """,
    )
    with webserver.install_http_handler(handler):
        assert stat.S_ISDIR(
            gdal.VSIStatL("/vsis3/s3_bucket_test_dir_stat_2/test_dir_stat/").mode
        )

    # Try ReadDi'ing a directory ending with slash
    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/s3_bucket_test_readdir2/?delimiter=%2F&list-type=2&prefix=test_dirread%2F",
        200,
        {"Content-type": "application/xml"},
        """<?xml version="1.0" encoding="UTF-8"?>
            <ListBucketResult>
                <Prefix>test_dirread/</Prefix>
                <Contents>
                    <Key>test_dirread/test.txt</Key>
                    <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                    <Size>40</Size>
                </Contents>
            </ListBucketResult>
        """,
    )
    with webserver.install_http_handler(handler):
        assert gdal.ReadDir("/vsis3/s3_bucket_test_readdir2/test_dirread") is not None


###############################################################################
# Test handling of file and directory with same name


def test_vsis3_8(aws_test_config, webserver_port):

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/vsis3_8/?delimiter=%2F&list-type=2",
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
        listdir = gdal.ReadDir("/vsis3/vsis3_8", 0)
    assert listdir == ["test", "test/"]

    handler = webserver.SequentialHandler()
    with webserver.install_http_handler(handler):
        assert not stat.S_ISDIR(gdal.VSIStatL("/vsis3/vsis3_8/test").mode)

    handler = webserver.SequentialHandler()
    with webserver.install_http_handler(handler):
        assert stat.S_ISDIR(gdal.VSIStatL("/vsis3/vsis3_8/test/").mode)


###############################################################################
# Test vsisync() with SYNC_STRATEGY=ETAG


def test_vsis3_sync_etag(tmp_vsimem, aws_test_config, webserver_port):

    gdal.VSICurlClearCache()

    options = ["SYNC_STRATEGY=ETAG"]

    with gdal.quiet_errors():
        handler = webserver.SequentialHandler()
        with webserver.install_http_handler(handler):
            assert not gdal.Sync("/i_do/not/exist", "/vsis3/", options=options)

    with gdal.quiet_errors():
        handler = webserver.SequentialHandler()
        handler.add("GET", "/do_not/exist", 404)
        handler.add(
            "GET",
            "/do_not/?delimiter=%2F&list-type=2&max-keys=100&prefix=exist%2F",
            404,
        )
        handler.add("PUT", "/do_not/exist", 404)
        with webserver.install_http_handler(handler):
            assert not gdal.Sync("vsifile.py", "/vsis3/do_not/exist", options=options)

    handler = webserver.SequentialHandler()
    handler.add("GET", "/out/", 200)
    handler.add("GET", "/out/testsync.txt", 404)
    handler.add(
        "GET",
        "/out/?delimiter=%2F&list-type=2&max-keys=100&prefix=testsync.txt%2F",
        404,
    )

    handler.add(
        "PUT",
        "/out/testsync.txt",
        200,
        headers={"Content-Length": 0, "ETag": '"acbd18db4cc2f85cedef654fccc4a4d8"'},
        expected_body=b"foo",
        expected_headers={"Content-Length": "3", "x-amz-storage-class": "GLACIER"},
    )

    gdal.FileFromMemBuffer(tmp_vsimem / "testsync.txt", "foo")

    def cbk(pct, _, tab):
        assert pct > tab[0]
        tab[0] = pct
        return True

    tab = [0]
    with webserver.install_http_handler(handler):
        assert gdal.Sync(
            tmp_vsimem / "testsync.txt",
            "/vsis3/out",
            options=options + ["x-amz-storage-class=GLACIER"],
            callback=cbk,
            callback_data=tab,
        )
    assert tab[0] == 1.0

    # Re-try with cached ETag. Should generate no network access
    handler = webserver.SequentialHandler()
    with webserver.install_http_handler(handler):
        assert gdal.Sync(tmp_vsimem / "testsync.txt", "/vsis3/out", options=options)
        assert gdal.Sync(
            tmp_vsimem / "testsync.txt", "/vsis3/out/testsync.txt", options=options
        )

    gdal.VSICurlClearCache()

    # Other direction: S3 to /vsimem
    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/out/testsync.txt",
        206,
        {
            "Content-Length": "3",
            "Content-Range": "bytes 0-2/3",
            "ETag": '"acbd18db4cc2f85cedef654fccc4a4d8"',
        },
        "foo",
    )
    with webserver.install_http_handler(handler):
        assert gdal.Sync("/vsis3/out/testsync.txt", tmp_vsimem, options=options)

    # Shouldn't do any copy, but hard to verify
    with webserver.install_http_handler(webserver.SequentialHandler()):
        assert gdal.Sync("/vsis3/out/testsync.txt", tmp_vsimem, options=options)
        assert gdal.Sync(
            "/vsis3/out/testsync.txt", tmp_vsimem / "testsync.txt", options=options
        )

    # Modify target file, and redo synchronization
    gdal.FileFromMemBuffer(tmp_vsimem / "testsync.txt", "bar")

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/out/testsync.txt",
        200,
        {"Content-Length": "3", "ETag": '"acbd18db4cc2f85cedef654fccc4a4d8"'},
        "foo",
    )
    with webserver.install_http_handler(handler):
        assert gdal.Sync("/vsis3/out/testsync.txt", tmp_vsimem, options=options)

    f = gdal.VSIFOpenL(tmp_vsimem / "testsync.txt", "rb")
    data = gdal.VSIFReadL(1, 3, f).decode("ascii")
    gdal.VSIFCloseL(f)
    assert data == "foo"

    # /vsimem to S3, but after cleaning the cache
    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add("GET", "/out/", 200)
    handler.add(
        "GET",
        "/out/testsync.txt",
        206,
        {
            "Content-Length": "3",
            "Content-Range": "bytes 0-2/3",
            "ETag": '"acbd18db4cc2f85cedef654fccc4a4d8"',
        },
        "foo",
    )
    with webserver.install_http_handler(handler):
        assert gdal.Sync(tmp_vsimem / "testsync.txt", "/vsis3/out", options=options)

    gdal.Unlink(tmp_vsimem / "testsync.txt")

    # Directory copying
    gdal.VSICurlClearCache()

    gdal.Mkdir(tmp_vsimem / "subdir", 0)
    gdal.FileFromMemBuffer(tmp_vsimem / "subdir/testsync.txt", "foo")
    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/out/?list-type=2",
        200,
        {},
        """<?xml version="1.0" encoding="UTF-8"?>
            <ListBucketResult>
                <Prefix/>
                <Marker/>
                <IsTruncated>false</IsTruncated>
                <Contents>
                    <Key>testsync.txt</Key>
                    <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                    <Size>3</Size>
                    <ETag>"acbd18db4cc2f85cedef654fccc4a4d8"</ETag>
                </Contents>
            </ListBucketResult>
        """,
    )
    with webserver.install_http_handler(handler):
        assert gdal.Sync(f"{tmp_vsimem}/subdir/", "/vsis3/out", options=options)


###############################################################################
# Test vsisync() with SYNC_STRATEGY=TIMESTAMP


def test_vsis3_sync_timestamp(tmp_vsimem, aws_test_config, webserver_port):

    options = ["SYNC_STRATEGY=TIMESTAMP"]

    gdal.FileFromMemBuffer(tmp_vsimem / "testsync.txt", "foo")

    # S3 to local: S3 file is older -> download
    gdal.VSICurlClearCache()
    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/out/testsync.txt",
        206,
        {
            "Content-Length": "3",
            "Content-Range": "bytes 0-2/3",
            "Last-Modified": "Mon, 01 Jan 1970 00:00:01 GMT",
        },
        "foo",
    )
    handler.add(
        "GET",
        "/out/testsync.txt",
        200,
        {"Content-Length": "3", "Last-Modified": "Mon, 01 Jan 1970 00:00:01 GMT"},
        "foo",
    )
    with webserver.install_http_handler(handler):
        assert gdal.Sync("/vsis3/out/testsync.txt", tmp_vsimem, options=options)

    # S3 to local: S3 file is newer -> do nothing
    gdal.VSICurlClearCache()
    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/out/testsync.txt",
        206,
        {
            "Content-Length": "3",
            "Content-Range": "bytes 0-2/3",
            # TODO: Will this test fail post-2037?
            "Last-Modified": "Mon, 01 Jan 2037 00:00:01 GMT",
        },
        "foo",
    )
    with webserver.install_http_handler(handler):
        assert gdal.Sync("/vsis3/out/testsync.txt", tmp_vsimem, options=options)

    # Local to S3: S3 file is older -> upload
    gdal.VSICurlClearCache()
    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/out/testsync.txt",
        206,
        {
            "Content-Length": "3",
            "Content-Range": "bytes 0-2/3",
            "Last-Modified": "Mon, 01 Jan 1970 00:00:01 GMT",
        },
        "foo",
    )
    handler.add("PUT", "/out/testsync.txt", 200)
    with webserver.install_http_handler(handler):
        assert gdal.Sync(
            tmp_vsimem / "testsync.txt", "/vsis3/out/testsync.txt", options=options
        )

    # Local to S3: S3 file is newer -> do nothing
    gdal.VSICurlClearCache()
    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/out/testsync.txt",
        206,
        {
            "Content-Length": "3",
            "Content-Range": "bytes 0-2/3",
            "Last-Modified": "Mon, 01 Jan 2037 00:00:01 GMT",
        },
        "foo",
    )
    with webserver.install_http_handler(handler):
        assert gdal.Sync(
            tmp_vsimem / "testsync.txt", "/vsis3/out/testsync.txt", options=options
        )


###############################################################################
# Test vsisync() failure


@gdaltest.enable_exceptions()
def test_vsis3_sync_failed(tmp_vsimem, aws_test_config, webserver_port):

    gdal.FileFromMemBuffer(tmp_vsimem / "testsync.txt", "x" * 30000)

    # S3 to local: S3 file is older -> download
    gdal.VSICurlClearCache()
    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/out/testsync.txt",
        206,
        {
            "Content-Length": "16384",
            "Content-Range": "bytes 0-16383/30000",
            "Last-Modified": "Mon, 01 Jan 1970 00:00:01 GMT",
        },
        "x" * 16384,
    )
    handler.add(
        "GET",
        "/out/?delimiter=%2F&list-type=2",
        200,
        {"Content-type": "application/xml"},
        """<?xml version="1.0" encoding="UTF-8"?>
        <ListBucketResult>
            <Prefix></Prefix>
            <Contents>
                <Key>testsync.txt</Key>
                <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                <Size>30000</Size>
            </Contents>
        </ListBucketResult>
        """,
    )
    handler.add("GET", "/out/testsync.txt", 400)
    # Do not use /vsicurl_streaming/ as source, otherwise errors may be
    # emitted in worker thread, which isn't properly handled (should ideally
    # be fixed)
    with gdal.config_option(
        "VSIS3_COPYFILE_USE_STREAMING_SOURCE", "NO"
    ), webserver.install_http_handler(handler):
        with pytest.raises(
            Exception,
            match=f"Copying of /vsis3/out/testsync.txt to {tmp_vsimem}/testsync.txt failed: 0 bytes were copied whereas 30000 were expected",
        ):
            gdal.Sync("/vsis3/out/testsync.txt", tmp_vsimem)


###############################################################################
# Test vsisync() with SYNC_STRATEGY=OVERWRITE


def test_vsis3_sync_overwrite(tmp_vsimem, aws_test_config, webserver_port):

    options = ["SYNC_STRATEGY=OVERWRITE"]

    gdal.FileFromMemBuffer(tmp_vsimem / "testsync.txt", "foo")

    # S3 to local: S3 file is newer
    gdal.VSICurlClearCache()
    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/out/testsync.txt",
        206,
        {
            "Content-Length": "3",
            "Content-Range": "bytes 0-2/3",
            "Last-Modified": "Mon, 01 Jan 2037 00:00:01 GMT",
        },
        "foo",
    )
    handler.add(
        "GET",
        "/out/testsync.txt",
        200,
        {"Content-Length": "3", "Last-Modified": "Mon, 01 Jan 2037 00:00:01 GMT"},
        "foo",
    )
    with webserver.install_http_handler(handler):
        assert gdal.Sync("/vsis3/out/testsync.txt", tmp_vsimem, options=options)

    # Local to S3: S3 file is newer
    gdal.VSICurlClearCache()
    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/out/testsync.txt",
        206,
        {
            "Content-Length": "3",
            "Content-Range": "bytes 0-2/3",
            "Last-Modified": "Mon, 01 Jan 2037 00:00:01 GMT",
        },
        "foo",
    )
    handler.add("PUT", "/out/testsync.txt", 200)
    with webserver.install_http_handler(handler):
        assert gdal.Sync(
            tmp_vsimem / "testsync.txt", "/vsis3/out/testsync.txt", options=options
        )


###############################################################################
# Test vsisync() with source in /vsis3 with implicit directories


def test_vsis3_sync_implicit_directories(tmp_path, aws_test_config, webserver_port):

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/mybucket/?list-type=2&prefix=subdir%2F",
        200,
        {},
        """<?xml version="1.0" encoding="UTF-8"?>
            <ListBucketResult>
                <Prefix>subdir/</Prefix>
                <Marker/>
                <IsTruncated>false</IsTruncated>
                <Contents>
                    <Key>subdir/implicit_subdir/testsync.txt</Key>
                    <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                    <Size>3</Size>
                    <ETag>"acbd18db4cc2f85cedef654fccc4a4d8"</ETag>
                </Contents>
            </ListBucketResult>
        """,
    )
    handler.add("GET", "/mybucket/subdir", 404)
    handler.add(
        "GET",
        "/mybucket/?delimiter=%2F&list-type=2&max-keys=100&prefix=subdir%2F",
        200,
        {},
        """<?xml version="1.0" encoding="UTF-8"?>
            <ListBucketResult>
                <Prefix>subdir/</Prefix>
                <Marker/>
                <IsTruncated>false</IsTruncated>
                <Contents>
                    <Key>subdir/implicit_subdir/testsync.txt</Key>
                    <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                    <Size>3</Size>
                    <ETag>"acbd18db4cc2f85cedef654fccc4a4d8"</ETag>
                </Contents>
            </ListBucketResult>
        """,
    )
    handler.add("GET", "/mybucket/subdir/implicit_subdir/testsync.txt", 200, {}, b"abc")
    tmpdirname = f"{tmp_path}/test_vsis3_sync_implicit_directories"
    gdal.Mkdir(tmpdirname, 0o755)

    with webserver.install_http_handler(handler):
        assert gdal.Sync("/vsis3/mybucket/subdir/", tmpdirname + "/")
    assert os.path.exists(tmpdirname + "/implicit_subdir")
    assert os.path.exists(tmpdirname + "/implicit_subdir/testsync.txt")


###############################################################################
# Test vsisync() with source and target in /vsis3


def test_vsis3_sync_source_target_in_vsis3(aws_test_config, webserver_port):

    gdal.VSICurlClearCache()
    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/in/testsync.txt",
        200,
        {
            "Content-Length": "3",
            "Content-Range": "bytes 0-2/3",
            "Last-Modified": "Mon, 01 Jan 1970 00:00:01 GMT",
        },
        "foo",
    )
    handler.add("GET", "/out/", 200)
    handler.add(
        "GET",
        "/out/testsync.txt",
        200,
        {"Content-Length": "3", "Last-Modified": "Mon, 01 Jan 1970 00:00:01 GMT"},
        "foo",
    )

    handler.add(
        "PUT",
        "/out/testsync.txt",
        200,
        headers={"Content-Length": 0},
        expected_headers={
            "Content-Length": "0",
            "x-amz-copy-source": "/in/testsync.txt",
            "x-amz-storage-class": "GLACIER",
        },
    )

    with webserver.install_http_handler(handler):
        assert gdal.Sync(
            "/vsis3/in/testsync.txt",
            "/vsis3/out/",
            options=["x-amz-storage-class=GLACIER"],
        )


###############################################################################
# Test VSISync() with Windows special filenames (prefix with "\\?\")


@pytest.mark.skipif(sys.platform != "win32", reason="Windows specific test")
def test_vsis3_sync_win32_special_filenames(aws_test_config, webserver_port, tmp_path):

    options = ["SYNC_STRATEGY=OVERWRITE"]

    tmp_path_str = str(tmp_path)
    if "/" in tmp_path_str:
        pytest.skip("Found forward slash in tmp_path")

    prefix_path = "\\\\?\\" + tmp_path_str

    # S3 to local
    gdal.VSICurlClearCache()
    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/bucket/?list-type=2",
        200,
        {},
        """<?xml version="1.0" encoding="UTF-8"?>
            <ListBucketResult>
                <Prefix></Prefix>
                <Marker/>
                <IsTruncated>false</IsTruncated>
                <Contents>
                    <Key>subdir/</Key>
                    <LastModified>2037-01-01T00:00:01.000Z</LastModified>
                    <Size>0</Size>
                </Contents>
                <Contents>
                    <Key>subdir/testsync.txt</Key>
                    <LastModified>2037-01-01T00:00:01.000Z</LastModified>
                    <Size>3</Size>
                </Contents>
            </ListBucketResult>
        """,
    )
    handler.add(
        "GET",
        "/bucket/",
        200,
        {},
        """<?xml version="1.0" encoding="UTF-8"?>
            <ListBucketResult>
                <Prefix></Prefix>
                <Marker/>
                <IsTruncated>false</IsTruncated>
                <Contents>
                    <Key>subdir/</Key>
                    <LastModified>2037-01-01T00:00:01.000Z</LastModified>
                    <Size>0</Size>
                </Contents>
                <Contents>
                    <Key>subdir/testsync.txt</Key>
                    <LastModified>2037-01-01T00:00:01.000Z</LastModified>
                    <Size>3</Size>
                </Contents>
            </ListBucketResult>
        """,
    )
    handler.add(
        "GET",
        "/bucket/subdir/testsync.txt",
        206,
        {
            "Content-Length": "3",
            "Content-Range": "bytes 0-2/3",
            "Last-Modified": "Mon, 01 Jan 2037 00:00:01 GMT",
        },
        "foo",
    )
    handler.add(
        "GET",
        "/bucket/?delimiter=%2F&list-type=2&prefix=subdir%2F",
        200,
        {},
        """<?xml version="1.0" encoding="UTF-8"?>
            <ListBucketResult>
                <Prefix>subdir/</Prefix>
                <Marker/>
                <IsTruncated>false</IsTruncated>
                <Contents>
                    <Key>subdir/testsync.txt</Key>
                    <LastModified>2037-01-01T00:00:01.000Z</LastModified>
                    <Size>3</Size>
                </Contents>
            </ListBucketResult>
        """,
    )
    handler.add(
        "GET",
        "/bucket/subdir/testsync.txt",
        200,
        {"Content-Length": "3", "Last-Modified": "Mon, 01 Jan 2037 00:00:01 GMT"},
        "foo",
    )
    with webserver.install_http_handler(handler):
        assert gdal.Sync("/vsis3/bucket/", prefix_path, options=options)

    assert gdal.VSIStatL(prefix_path + "\\subdir\\testsync.txt") is not None

    # Local to S3
    gdal.VSICurlClearCache()
    handler = webserver.SequentialHandler()
    handler.add("GET", "/out/?list-type=2", 404)
    handler.add("GET", "/out/", 404)
    handler.add(
        "GET",
        "/out/?delimiter=%2F&list-type=2&max-keys=100",
        200,
        {},
        """<?xml version="1.0" encoding="UTF-8"?>
            <ListBucketResult>
                <Prefix></Prefix>
                <Marker/>
                <IsTruncated>false</IsTruncated>
                <Contents>
                </Contents>
            </ListBucketResult>
        """,
    )
    handler.add("PUT", "/out/subdir/", 200)
    handler.add("PUT", "/out/subdir/testsync.txt", 200)
    with webserver.install_http_handler(handler):
        assert gdal.Sync(prefix_path + "\\", "/vsis3/out/", options=options)


###############################################################################
# Test rename


def test_vsis3_fake_rename(aws_test_config, webserver_port):

    gdal.VSICurlClearCache()
    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/test/source.txt",
        206,
        {"Content-Length": "3", "Content-Range": "bytes 0-2/3"},
        "foo",
    )
    handler.add("GET", "/test/target.txt", 404)
    handler.add(
        "GET", "/test/?delimiter=%2F&list-type=2&max-keys=100&prefix=target.txt%2F", 200
    )
    handler.add(
        "PUT",
        "/test/target.txt",
        200,
        {"Content-Length": "0"},
        b"",
        expected_headers={
            "Content-Length": "0",
            "x-amz-copy-source": "/test/source.txt",
        },
    )
    handler.add("DELETE", "/test/source.txt", 204)

    with webserver.install_http_handler(handler):
        assert gdal.Rename("/vsis3/test/source.txt", "/vsis3/test/target.txt") == 0


###############################################################################
# Test rename


def test_vsis3_fake_rename_dir(aws_test_config, webserver_port):

    gdal.VSICurlClearCache()
    handler = webserver.SequentialHandler()
    handler.add("GET", "/test/source_dir", 404)
    handler.add(
        "GET",
        "/test/?delimiter=%2F&list-type=2&max-keys=100&prefix=source_dir%2F",
        200,
        {"Content-type": "application/xml"},
        """<?xml version="1.0" encoding="UTF-8"?>
            <ListBucketResult>
                <Prefix>source_dir/</Prefix>
                <Contents>
                    <Key>source_dir/test.txt</Key>
                    <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                    <Size>3</Size>
                </Contents>
            </ListBucketResult>
        """,
    )
    handler.add("GET", "/test/target_dir/", 404)
    handler.add(
        "GET", "/test/?delimiter=%2F&list-type=2&max-keys=100&prefix=target_dir%2F", 404
    )

    def method(request):
        if request.headers["Content-Length"] != "0":
            sys.stderr.write(
                "Did not get expected headers: %s\n" % str(request.headers)
            )
            request.send_response(400)
            request.send_header("Content-Length", 0)
            request.end_headers()
            return

        request.wfile.write("HTTP/1.1 100 Continue\r\n\r\n".encode("ascii"))
        request.send_response(200)
        request.send_header("Content-Length", 0)
        request.end_headers()

    handler.add("PUT", "/test/target_dir/", custom_method=method)

    handler.add(
        "PUT",
        "/test/target_dir/test.txt",
        200,
        {"Content-Length": "0"},
        b"",
        expected_headers={
            "Content-Length": "0",
            "x-amz-copy-source": "/test/source_dir/test.txt",
        },
    )

    handler.add("DELETE", "/test/source_dir/test.txt", 204)

    handler.add("GET", "/test/source_dir/", 404)
    handler.add(
        "GET", "/test/?delimiter=%2F&list-type=2&max-keys=100&prefix=source_dir%2F", 404
    )

    with webserver.install_http_handler(handler):
        assert gdal.Rename("/vsis3/test/source_dir", "/vsis3/test/target_dir") == 0


###############################################################################
# Test rename onto existing dir is not allowed


def test_vsis3_fake_rename_on_existing_dir(aws_test_config, webserver_port):

    gdal.VSICurlClearCache()
    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/test/source.txt",
        206,
        {"Content-Length": "3", "Content-Range": "bytes 0-2/3"},
        "foo",
    )
    handler.add("GET", "/test_target_dir/", 200)

    with webserver.install_http_handler(handler):
        assert gdal.Rename("/vsis3/test/source.txt", "/vsis3/test_target_dir") == -1


###############################################################################
# Test Sync() and multithreaded download and CHUNK_SIZE


def test_vsis3_fake_sync_multithreaded_upload_chunk_size(
    tmp_vsimem, aws_test_config, webserver_port
):

    gdal.VSICurlClearCache()

    def cbk(pct, _, tab):
        assert pct >= tab[0]
        tab[0] = pct
        return True

    gdal.Mkdir(tmp_vsimem / "test", 0)
    gdal.FileFromMemBuffer(tmp_vsimem / "test/foo", "foo\n")

    tab = [-1]
    handler = webserver.SequentialHandler()
    handler.add("GET", "/test_bucket/?list-type=2&prefix=test%2F", 200)
    handler.add("GET", "/test_bucket/test", 404)
    handler.add(
        "GET",
        "/test_bucket/?delimiter=%2F&list-type=2&max-keys=100&prefix=test%2F",
        200,
    )
    handler.add("GET", "/test_bucket/", 200)
    handler.add("GET", "/test_bucket/test/", 404)
    handler.add("PUT", "/test_bucket/test/", 200)

    handler.add(
        "POST",
        "/test_bucket/test/foo?uploads",
        200,
        expected_headers={"x-amz-storage-class": "GLACIER"},
        body=b"""<?xml version="1.0" encoding="UTF-8"?>
        <InitiateMultipartUploadResult>
        <UploadId>my_id</UploadId>
        </InitiateMultipartUploadResult>""",
        headers={"Content-type": "application/xml"},
    )

    handler.add(
        "PUT",
        "/test_bucket/test/foo?partNumber=1&uploadId=my_id",
        200,
        {"ETag": '"first_etag"'},
        b"",
        expected_headers={"Content-Length": "3"},
    )

    handler.add(
        "PUT",
        "/test_bucket/test/foo?partNumber=2&uploadId=my_id",
        200,
        {"ETag": '"second_etag"'},
        b"",
        expected_headers={"Content-Length": "1"},
    )

    handler.add(
        "POST",
        "/test_bucket/test/foo?uploadId=my_id",
        200,
        {},
        b"",
        expected_headers={"Content-Length": "186"},
        expected_body=b"""<CompleteMultipartUpload>
<Part>
<PartNumber>1</PartNumber><ETag>"first_etag"</ETag></Part>
<Part>
<PartNumber>2</PartNumber><ETag>"second_etag"</ETag></Part>
</CompleteMultipartUpload>
""",
    )

    with gdaltest.config_option("VSIS3_SIMULATE_THREADING", "YES", thread_local=False):
        with webserver.install_http_handler(handler):
            assert gdal.Sync(
                tmp_vsimem / "test",
                "/vsis3/test_bucket",
                options=[
                    "NUM_THREADS=1",
                    "CHUNK_SIZE=3",
                    "x-amz-storage-class=GLACIER",
                ],
                callback=cbk,
                callback_data=tab,
            )
    assert tab[0] == 1.0


def test_vsis3_fake_sync_multithreaded_upload_chunk_size_failure(
    tmp_vsimem, aws_test_config, webserver_port
):

    gdal.VSICurlClearCache()

    gdal.Mkdir(tmp_vsimem / "test", 0)
    gdal.FileFromMemBuffer(tmp_vsimem / "test/foo", "foo\n")

    handler = webserver.SequentialHandler()
    handler.add("GET", "/test_bucket/?list-type=2&prefix=test%2F", 200)
    handler.add("GET", "/test_bucket/test", 404)
    handler.add(
        "GET",
        "/test_bucket/?delimiter=%2F&list-type=2&max-keys=100&prefix=test%2F",
        200,
    )
    handler.add("GET", "/test_bucket/", 200)
    handler.add("GET", "/test_bucket/test/", 404)
    handler.add("PUT", "/test_bucket/test/", 200)
    handler.add(
        "POST",
        "/test_bucket/test/foo?uploads",
        200,
        {"Content-type": "application:/xml"},
        b"""<?xml version="1.0" encoding="UTF-8"?>
        <InitiateMultipartUploadResult>
        <UploadId>my_id</UploadId>
        </InitiateMultipartUploadResult>""",
    )
    handler.add(
        "PUT",
        "/test_bucket/test/foo?partNumber=1&uploadId=my_id",
        200,
        {"ETag": '"first_etag"'},
        expected_headers={"Content-Length": "3"},
    )
    handler.add("DELETE", "/test_bucket/test/foo?uploadId=my_id", 204)

    with gdaltest.config_options(
        {"VSIS3_SIMULATE_THREADING": "YES", "VSIS3_SYNC_MULTITHREADING": "NO"},
        thread_local=False,
    ):
        with webserver.install_http_handler(handler):
            with gdal.quiet_errors():
                assert not gdal.Sync(
                    tmp_vsimem / "test",
                    "/vsis3/test_bucket",
                    options=["NUM_THREADS=1", "CHUNK_SIZE=3"],
                )


###############################################################################
# Test gdal.CopyFileRestartable() where upload is completed in a single attempt


def test_vsis3_CopyFileRestartable_no_error(
    tmp_vsimem, aws_test_config, webserver_port
):

    gdal.VSICurlClearCache()

    srcfilename = str(tmp_vsimem / "foo")
    gdal.FileFromMemBuffer(srcfilename, "foo\n")

    dstfilename = "/vsis3/test_bucket/foo"

    handler = webserver.SequentialHandler()
    handler.add(
        "POST",
        "/test_bucket/foo?uploads",
        200,
        {"Content-type": "application:/xml"},
        b"""<?xml version="1.0" encoding="UTF-8"?>
        <InitiateMultipartUploadResult>
        <UploadId>my_id</UploadId>
        </InitiateMultipartUploadResult>""",
    )
    handler.add(
        "PUT",
        "/test_bucket/foo?partNumber=1&uploadId=my_id",
        200,
        {"ETag": '"first_etag"'},
        expected_headers={"Content-Length": "4"},
        expected_body=b"foo\n",
    )
    handler.add("POST", "/test_bucket/foo?uploadId=my_id", 200)

    with webserver.install_http_handler(handler):
        ret_code, restart_payload = gdal.CopyFileRestartable(
            srcfilename, dstfilename, None, ["NUM_THREADS=1"]
        )
    assert ret_code == 0
    assert restart_payload is None


###############################################################################
# Test multithreaded gdal.CopyFileRestartable() where upload is completed in a single attempt


def test_vsis3_CopyFileRestartable_multithreaded(
    tmp_vsimem, aws_test_config, webserver_port
):

    gdal.VSICurlClearCache()

    srcfilename = str(tmp_vsimem / "foo")
    gdal.FileFromMemBuffer(srcfilename, "foo\n")

    dstfilename = "/vsis3/test_bucket/foo"

    # Use a non sequential HTTP handler as the PUT could be emitted in
    # any order
    handler = webserver.NonSequentialMockedHttpHandler()
    handler.add(
        "POST",
        "/test_bucket/foo?uploads",
        200,
        {"Content-type": "application:/xml"},
        b"""<?xml version="1.0" encoding="UTF-8"?>
        <InitiateMultipartUploadResult>
        <UploadId>my_id</UploadId>
        </InitiateMultipartUploadResult>""",
    )
    handler.add(
        "PUT",
        "/test_bucket/foo?partNumber=1&uploadId=my_id",
        200,
        {"ETag": '"first_etag"'},
        expected_headers={"Content-Length": "3"},
        expected_body=b"foo",
    )
    handler.add(
        "PUT",
        "/test_bucket/foo?partNumber=2&uploadId=my_id",
        200,
        {"ETag": '"second_etag"'},
        expected_headers={"Content-Length": "1"},
        expected_body=b"\n",
    )
    handler.add("POST", "/test_bucket/foo?uploadId=my_id", 200)

    with webserver.install_http_handler(handler):
        ret_code, restart_payload = gdal.CopyFileRestartable(
            srcfilename,
            dstfilename,
            None,
            ["CHUNK_SIZE=3"],
        )
    assert ret_code == 0
    assert restart_payload is None


###############################################################################
# Test gdal.CopyFileRestartable() with one restart to complete the upload


@pytest.mark.parametrize("failure_reason", ["progress_cbk", "failed_part_put"])
def test_vsis3_CopyFileRestartable_with_restart(
    tmp_vsimem, aws_test_config, webserver_port, failure_reason
):

    gdal.VSICurlClearCache()

    srcfilename = str(tmp_vsimem / "foo")
    gdal.FileFromMemBuffer(srcfilename, "foo\n")

    dstfilename = "/vsis3/test_bucket/foo"

    def progress_cbk(pct, msg, user_data):
        if failure_reason == "progress_cbk":
            return pct < 0.5
        else:
            return True

    handler = webserver.SequentialHandler()
    handler.add(
        "POST",
        "/test_bucket/foo?uploads",
        200,
        {"Content-type": "application:/xml"},
        b"""<?xml version="1.0" encoding="UTF-8"?>
        <InitiateMultipartUploadResult>
        <UploadId>my_id</UploadId>
        </InitiateMultipartUploadResult>""",
    )
    handler.add(
        "PUT",
        "/test_bucket/foo?partNumber=1&uploadId=my_id",
        200,
        {"ETag": '"first_etag"'},
        expected_headers={"Content-Length": "3"},
        expected_body=b"foo",
    )
    if failure_reason == "failed_part_put":
        handler.add(
            "PUT",
            "/test_bucket/foo?partNumber=2&uploadId=my_id",
            400,
            expected_headers={"Content-Length": "1"},
            expected_body=b"\n",
        )
    with webserver.install_http_handler(handler), gdal.quiet_errors():
        ret_code, restart_payload = gdal.CopyFileRestartable(
            srcfilename,
            dstfilename,
            None,  # input payload
            ["CHUNK_SIZE=3", "NUM_THREADS=1"],
            progress_cbk,
        )
    assert ret_code == 1
    j = json.loads(restart_payload)
    assert "source_mtime" in j
    del j["source_mtime"]
    assert j == {
        "type": "CopyFileRestartablePayload",
        "source": srcfilename,
        "target": dstfilename,
        "source_size": 4,
        "chunk_size": 3,
        "upload_id": "my_id",
        "chunk_etags": ['"first_etag"', None],
    }

    handler = webserver.SequentialHandler()
    handler.add(
        "PUT",
        "/test_bucket/foo?partNumber=2&uploadId=my_id",
        200,
        {"ETag": '"second_etag"'},
        expected_headers={"Content-Length": "1"},
        expected_body=b"\n",
    )
    handler.add(
        "POST",
        "/test_bucket/foo?uploadId=my_id",
        200,
        expected_body=b"""<CompleteMultipartUpload>
<Part>
<PartNumber>1</PartNumber><ETag>"first_etag"</ETag></Part>
<Part>
<PartNumber>2</PartNumber><ETag>"second_etag"</ETag></Part>
</CompleteMultipartUpload>
""",
    )
    with webserver.install_http_handler(handler):
        ret_code, restart_payload = gdal.CopyFileRestartable(
            srcfilename, dstfilename, restart_payload, ["NUM_THREADS=1"]
        )
    assert ret_code == 0
    assert restart_payload is None


###############################################################################
# Test gdal.CopyFileRestartable() with error cases


def test_vsis3_CopyFileRestartable_src_file_does_not_exist(
    tmp_vsimem, aws_test_config, webserver_port
):

    gdal.VSICurlClearCache()

    with gdal.quiet_errors():
        ret_code, restart_payload = gdal.CopyFileRestartable(
            "/vsimem/i/do/not/exist", "/vsis3/test_bucket/dst", None, ["NUM_THREADS=1"]
        )
    assert ret_code == -1
    assert restart_payload is None


###############################################################################
# Test gdal.CopyFileRestartable() with error cases


def test_vsis3_CopyFileRestartable_InitiateMultipartUpload_failed(
    tmp_vsimem, aws_test_config, webserver_port
):

    gdal.VSICurlClearCache()

    srcfilename = str(tmp_vsimem / "foo")
    gdal.FileFromMemBuffer(srcfilename, "foo\n")

    dstfilename = "/vsis3/test_bucket/foo"

    handler = webserver.SequentialHandler()
    handler.add("POST", "/test_bucket/foo?uploads", 400)
    with webserver.install_http_handler(handler), gdal.quiet_errors():
        ret_code, restart_payload = gdal.CopyFileRestartable(
            srcfilename, dstfilename, None, ["NUM_THREADS=1"]  # input payload
        )
    assert ret_code == -1


###############################################################################
# Test gdal.CopyFileRestartable() with error cases


def test_vsis3_CopyFileRestartable_CompleteMultipartUpload_failed(
    tmp_vsimem, aws_test_config, webserver_port
):

    gdal.VSICurlClearCache()

    srcfilename = str(tmp_vsimem / "foo")
    gdal.FileFromMemBuffer(srcfilename, "foo\n")

    dstfilename = "/vsis3/test_bucket/foo"

    handler = webserver.SequentialHandler()
    handler.add(
        "POST",
        "/test_bucket/foo?uploads",
        200,
        {"Content-type": "application:/xml"},
        b"""<?xml version="1.0" encoding="UTF-8"?>
        <InitiateMultipartUploadResult>
        <UploadId>my_id</UploadId>
        </InitiateMultipartUploadResult>""",
    )
    handler.add(
        "PUT",
        "/test_bucket/foo?partNumber=1&uploadId=my_id",
        200,
        {"ETag": '"first_etag"'},
        expected_headers={"Content-Length": "4"},
        expected_body=b"foo\n",
    )
    handler.add("POST", "/test_bucket/foo?uploadId=my_id", 400)
    handler.add("DELETE", "/test_bucket/foo?uploadId=my_id", 200)

    with webserver.install_http_handler(handler), gdal.quiet_errors():
        ret_code, restart_payload = gdal.CopyFileRestartable(
            srcfilename, dstfilename, None, ["NUM_THREADS=1"]  # input payload
        )
    assert ret_code == -1


###############################################################################
# Test gdal.CopyFileRestartable() with errors in input payload


@pytest.mark.parametrize(
    "key,value,error_msg",
    [
        ("source", None, "'source' field in input payload does not match pszSource"),
        ("target", None, "'target' field in input payload does not match pszTarget"),
        ("chunk_size", None, "'chunk_size' field in input payload missing or invalid"),
        (
            "source_size",
            None,
            "'source_size' field in input payload does not match source file size",
        ),
        (
            "source_mtime",
            None,
            "'source_mtime' field in input payload does not match source file modification time",
        ),
        ("upload_id", None, "'upload_id' field in input payload missing or invalid"),
        (
            "chunk_etags",
            None,
            "'chunk_etags' field in input payload missing or invalid",
        ),
        (
            "chunk_etags",
            [],
            "'chunk_etags' field in input payload has not expected size",
        ),
    ],
)
def test_vsis3_CopyFileRestartable_errors_input_payload(
    tmp_vsimem, aws_test_config, key, value, error_msg
):

    srcfilename = str(tmp_vsimem / "foo")
    gdal.FileFromMemBuffer(srcfilename, "foo\n")

    dstfilename = "/vsis3/test_bucket/foo"

    j = {
        "type": "CopyFileRestartablePayload",
        "source": srcfilename,
        "target": dstfilename,
        "source_size": 4,
        "chunk_size": 3,
        "upload_id": "my_id",
        "chunk_etags": ['"first_etag"', None],
    }
    j["source_mtime"] = gdal.VSIStatL(srcfilename).mtime

    j[key] = value
    restart_payload = json.dumps(j)

    gdal.ErrorReset()
    with gdal.quiet_errors():
        ret_code, restart_payload = gdal.CopyFileRestartable(
            srcfilename, dstfilename, restart_payload, ["NUM_THREADS=1"]
        )
    assert ret_code == -1
    assert restart_payload is None
    assert gdal.GetLastErrorMsg() == error_msg


###############################################################################
# Test gdal.CopyFileRestartable() with /vsis3/ to /vsis3/


def test_vsis3_CopyFileRestartable_server_side(
    tmp_vsimem, aws_test_config, webserver_port
):

    gdal.VSICurlClearCache()

    srcfilename = "/vsis3/test_bucket/src"
    dstfilename = "/vsis3/test_bucket/dst"

    handler = webserver.SequentialHandler()
    handler.add(
        "PUT",
        "/test_bucket/dst",
        200,
        expected_headers={"x-amz-copy-source": "/test_bucket/src"},
    )
    with webserver.install_http_handler(handler):
        ret_code, restart_payload = gdal.CopyFileRestartable(
            srcfilename, dstfilename, None, ["NUM_THREADS=1"]
        )
    assert ret_code == 0
    assert restart_payload is None


###############################################################################
# Test reading/writing metadata


def test_vsis3_metadata(aws_test_config, webserver_port):

    gdal.VSICurlClearCache()

    # Read HEADERS domain
    handler = webserver.SequentialHandler()
    handler.add("GET", "/test_metadata/foo.txt", 200, {"foo": "bar"})
    with webserver.install_http_handler(handler):
        md = gdal.GetFileMetadata("/vsis3/test_metadata/foo.txt", "HEADERS")
    assert "foo" in md and md["foo"] == "bar"

    # Read TAGS domain
    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/test_metadata/foo.txt?tagging",
        200,
        {},
        """<Tagging>
        <TagSet>
        <Tag>
        <Key>foo</Key>
        <Value>bar</Value>
        </Tag>
        </TagSet>
        </Tagging>""",
    )
    with webserver.install_http_handler(handler):
        md = gdal.GetFileMetadata("/vsis3/test_metadata/foo.txt", "TAGS")
    assert "foo" in md and md["foo"] == "bar"

    # Write HEADERS domain
    handler = webserver.SequentialHandler()
    handler.add(
        "PUT",
        "/test_metadata/foo.txt",
        200,
        {},
        expected_headers={
            "foo": "bar",
            "x-amz-metadata-directive": "REPLACE",
            "x-amz-copy-source": "/test_metadata/foo.txt",
        },
    )
    with webserver.install_http_handler(handler):
        assert gdal.SetFileMetadata(
            "/vsis3/test_metadata/foo.txt", {"foo": "bar"}, "HEADERS"
        )

    # Write TAGS domain
    handler = webserver.SequentialHandler()
    handler.add(
        "PUT",
        "/test_metadata/foo.txt?tagging",
        200,
        expected_body=b"""<?xml version="1.0" encoding="UTF-8"?>
<Tagging xmlns="http://s3.amazonaws.com/doc/2006-03-01/">
  <TagSet>
    <Tag>
      <Key>foo</Key>
      <Value>bar</Value>
    </Tag>
  </TagSet>
</Tagging>
""",
    )
    with webserver.install_http_handler(handler):
        assert gdal.SetFileMetadata(
            "/vsis3/test_metadata/foo.txt", {"foo": "bar"}, "TAGS"
        )

    # Write TAGS domain (wiping tags)
    handler = webserver.SequentialHandler()
    handler.add("DELETE", "/test_metadata/foo.txt?tagging", 204)
    with webserver.install_http_handler(handler):
        assert gdal.SetFileMetadata("/vsis3/test_metadata/foo.txt", {}, "TAGS")

    # Error case
    with gdal.quiet_errors():
        assert gdal.GetFileMetadata("/vsis3/test_metadata/foo.txt", "UNSUPPORTED") == {}

    # Error case
    with gdal.quiet_errors():
        assert not gdal.SetFileMetadata(
            "/vsis3/test_metadata/foo.txt", {}, "UNSUPPORTED"
        )


###############################################################################
# Test that we take into account directory listing to avoid useless
# requests


def test_vsis3_no_useless_requests(aws_test_config, webserver_port):

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/no_useless_requests/?delimiter=%2F&list-type=2",
        200,
        {"Content-type": "application/xml"},
        """<?xml version="1.0" encoding="UTF-8"?>
        <ListBucketResult>
            <Prefix></Prefix>
            <Contents>
            </Contents>
        </ListBucketResult>
        """,
    )
    with webserver.install_http_handler(handler):
        assert gdal.VSIFOpenL("/vsis3/no_useless_requests/foo.txt", "rb") is None
        assert gdal.VSIFOpenL("/vsis3/no_useless_requests/bar.txt", "rb") is None
        assert gdal.VSIStatL("/vsis3/no_useless_requests/baz.txt") is None


###############################################################################
# Test w+ access


def test_vsis3_random_write(aws_test_config, webserver_port):

    gdal.VSICurlClearCache()

    with gdal.quiet_errors():
        assert gdal.VSIFOpenL("/vsis3/random_write/test.bin", "w+b") is None

    with gdaltest.config_option(
        "CPL_VSIL_USE_TEMP_FILE_FOR_RANDOM_WRITE", "YES", thread_local=False
    ):
        f = gdal.VSIFOpenL("/vsis3/random_write/test.bin", "w+b")
    assert f
    assert gdal.VSIFWriteL("foo", 3, 1, f) == 1
    assert gdal.VSIFSeekL(f, 0, 0) == 0
    assert gdal.VSIFReadL(3, 1, f).decode("ascii") == "foo"
    assert gdal.VSIFEofL(f) == 0
    assert gdal.VSIFTellL(f) == 3

    handler = webserver.SequentialHandler()
    handler.add("PUT", "/random_write/test.bin", 200, {}, expected_body=b"foo")
    with webserver.install_http_handler(handler):
        assert gdal.VSIFCloseL(f) == 0


###############################################################################
# Test w+ access


def test_vsis3_random_write_failure_1(aws_test_config, webserver_port):

    gdal.VSICurlClearCache()

    with gdaltest.config_option(
        "CPL_VSIL_USE_TEMP_FILE_FOR_RANDOM_WRITE", "YES", thread_local=False
    ):
        f = gdal.VSIFOpenL("/vsis3/random_write/test.bin", "w+b")
    assert f

    handler = webserver.SequentialHandler()
    handler.add("PUT", "/random_write/test.bin", 400, {})
    with webserver.install_http_handler(handler):
        with gdal.quiet_errors():
            assert gdal.VSIFCloseL(f) != 0


###############################################################################
# Test w+ access


def test_vsis3_random_write_failure_2(aws_test_config, webserver_port):

    gdal.VSICurlClearCache()

    with gdaltest.config_option(
        "CPL_VSIL_USE_TEMP_FILE_FOR_RANDOM_WRITE", "YES", thread_local=False
    ):
        with gdaltest.config_option("VSIS3_CHUNK_SIZE_BYTES", "1"):
            f = gdal.VSIFOpenL("/vsis3/random_write/test.bin", "w+b")
    assert f
    assert gdal.VSIFWriteL("foo", 3, 1, f) == 1

    handler = webserver.SequentialHandler()
    handler.add("POST", "/random_write/test.bin?uploads", 400, {})
    with webserver.install_http_handler(handler):
        with gdal.quiet_errors():
            assert gdal.VSIFCloseL(f) != 0


###############################################################################
# Test w+ access


def test_vsis3_random_write_gtiff_create_copy(aws_test_config, webserver_port):

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add("GET", "/random_write/test.tif", 404, {})
    handler.add(
        "GET",
        "/random_write/?delimiter=%2F&list-type=2&max-keys=100&prefix=test.tif%2F",
        404,
        {},
    )

    src_ds = gdal.Open("data/byte.tif")

    with gdaltest.config_option(
        "CPL_VSIL_USE_TEMP_FILE_FOR_RANDOM_WRITE", "YES", thread_local=False
    ):
        with webserver.install_http_handler(handler):
            ds = gdal.GetDriverByName("GTiff").CreateCopy(
                "/vsis3/random_write/test.tif", src_ds
            )
    assert ds is not None

    handler = webserver.SequentialHandler()
    handler.add("PUT", "/random_write/test.tif", 200, {})
    with webserver.install_http_handler(handler):
        ds = None


###############################################################################
# Test r+ access


def test_vsis3_random_write_on_existing_file(aws_test_config, webserver_port):

    gdal.VSICurlClearCache()

    with gdal.quiet_errors():
        assert gdal.VSIFOpenL("/vsis3/random_write/test.bin", "r+b") is None

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/random_write/?delimiter=%2F&list-type=2",
        200,
        {"Content-type": "application/xml"},
        """<?xml version="1.0" encoding="UTF-8"?>
            <ListBucketResult>
                <Prefix></Prefix>
                <Contents>
                    <Key>test.bin</Key>
                    <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                    <Size>1</Size>
                </Contents>
            </ListBucketResult>
            """,
    )
    handler.add("GET", "/random_write/test.bin", 200, {}, "f")
    handler.add("PUT", "/random_write/test.bin", 200, {}, expected_body=b"foo")
    with webserver.install_http_handler(handler), gdaltest.config_option(
        "CPL_VSIL_USE_TEMP_FILE_FOR_RANDOM_WRITE", "YES", thread_local=False
    ):
        f = gdal.VSIFOpenL("/vsis3/random_write/test.bin", "r+b")
        assert f
        assert gdal.VSIFSeekL(f, 1, 0) == 0
        assert gdal.VSIFWriteL("oo", 2, 1, f) == 1
        assert gdal.VSIFCloseL(f) == 0


###############################################################################
# Test r+ access


def test_vsis3_random_write_on_existing_file_that_does_not_exist(
    aws_test_config, webserver_port
):

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/random_write/?delimiter=%2F&list-type=2",
        200,
        {"Content-type": "application/xml"},
        """<?xml version="1.0" encoding="UTF-8"?>
            <ListBucketResult>
                <Prefix></Prefix>
                <Contents/>
            </ListBucketResult>
            """,
    )
    with webserver.install_http_handler(handler), gdaltest.config_option(
        "CPL_VSIL_USE_TEMP_FILE_FOR_RANDOM_WRITE", "YES", thread_local=False
    ):
        f = gdal.VSIFOpenL("/vsis3/random_write/test.bin", "r+b")
        assert f is None


###############################################################################
# Read credentials from simulated ~/.aws/credentials


def test_vsis3_read_credentials_file(tmp_vsimem, aws_test_config, webserver_port):

    options = {
        "AWS_SECRET_ACCESS_KEY": "",
        "AWS_ACCESS_KEY_ID": "",
        "CPL_AWS_CREDENTIALS_FILE": f"{tmp_vsimem}/aws_credentials",
    }

    gdal.VSICurlClearCache()

    gdal.FileFromMemBuffer(
        tmp_vsimem / "aws_credentials",
        """
[unrelated]
aws_access_key_id = foo
aws_secret_access_key = bar
[default]
aws_access_key_id = AWS_ACCESS_KEY_ID
aws_secret_access_key = AWS_SECRET_ACCESS_KEY
[unrelated]
aws_access_key_id = foo
aws_secret_access_key = bar
""",
    )

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/s3_fake_bucket/resource",
        custom_method=get_s3_fake_bucket_resource_method,
    )
    with webserver.install_http_handler(handler):
        with gdaltest.config_options(options, thread_local=False):
            f = open_for_read("/vsis3/s3_fake_bucket/resource")
        assert f is not None
        data = gdal.VSIFReadL(1, 4, f).decode("ascii")
        gdal.VSIFCloseL(f)

    assert data == "foo"


###############################################################################
# Read credentials from simulated  ~/.aws/config


def test_vsis3_read_config_file(tmp_vsimem, aws_test_config, webserver_port):
    options = {
        "AWS_SECRET_ACCESS_KEY": "",
        "AWS_ACCESS_KEY_ID": "",
        "AWS_CONFIG_FILE": f"{tmp_vsimem}/aws_config",
    }

    gdal.VSICurlClearCache()

    gdal.FileFromMemBuffer(
        tmp_vsimem / "aws_config",
        """
[unrelated]
aws_access_key_id = foo
aws_secret_access_key = bar
[default]
aws_access_key_id = AWS_ACCESS_KEY_ID
aws_secret_access_key = AWS_SECRET_ACCESS_KEY
region = us-east-1
[unrelated]
aws_access_key_id = foo
aws_secret_access_key = bar
""",
    )

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/s3_fake_bucket/resource",
        custom_method=get_s3_fake_bucket_resource_method,
    )
    with webserver.install_http_handler(handler):
        with gdaltest.config_options(options, thread_local=False):
            f = open_for_read("/vsis3/s3_fake_bucket/resource")
        assert f is not None
        data = gdal.VSIFReadL(1, 4, f).decode("ascii")
        gdal.VSIFCloseL(f)

    assert data == "foo"


###############################################################################
# Read credentials from simulated ~/.aws/credentials and ~/.aws/config


def test_vsis3_read_credentials_config_file(
    tmp_vsimem, aws_test_config, webserver_port
):
    options = {
        "AWS_SECRET_ACCESS_KEY": "",
        "AWS_ACCESS_KEY_ID": "",
        "CPL_AWS_CREDENTIALS_FILE": f"{tmp_vsimem}/aws_credentials",
        "AWS_CONFIG_FILE": f"{tmp_vsimem}/aws_config",
    }

    gdal.VSICurlClearCache()

    gdal.FileFromMemBuffer(
        tmp_vsimem / "aws_credentials",
        """
[unrelated]
aws_access_key_id = foo
aws_secret_access_key = bar
[default]
aws_access_key_id = AWS_ACCESS_KEY_ID
aws_secret_access_key = AWS_SECRET_ACCESS_KEY
[unrelated]
aws_access_key_id = foo
aws_secret_access_key = bar
""",
    )

    gdal.FileFromMemBuffer(
        tmp_vsimem / "aws_config",
        """
[unrelated]
aws_access_key_id = foo
aws_secret_access_key = bar
[default]
aws_access_key_id = AWS_ACCESS_KEY_ID
aws_secret_access_key = AWS_SECRET_ACCESS_KEY
region = us-east-1
[unrelated]
aws_access_key_id = foo
aws_secret_access_key = bar
""",
    )

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/s3_fake_bucket/resource",
        custom_method=get_s3_fake_bucket_resource_method,
    )
    with webserver.install_http_handler(handler):
        with gdaltest.config_options(options, thread_local=False):
            f = open_for_read("/vsis3/s3_fake_bucket/resource")
        assert f is not None
        data = gdal.VSIFReadL(1, 4, f).decode("ascii")
        gdal.VSIFCloseL(f)

    assert data == "foo"


###############################################################################
# Read credentials from simulated ~/.aws/credentials and ~/.aws/config with
# a non default profile


def test_vsis3_read_credentials_config_file_non_default_profile(
    aws_test_config, webserver_port, tmpdir
):
    options = {
        "AWS_SECRET_ACCESS_KEY": "",
        "AWS_ACCESS_KEY_ID": "",
        "CPL_AWS_CREDENTIALS_FILE": None,
        "AWS_CONFIG_FILE": None,
        "AWS_PROFILE": "myprofile",
    }

    os_aws = tmpdir.mkdir(".aws")

    gdal.VSICurlClearCache()

    os_aws.join("credentials").write("""
[unrelated]
aws_access_key_id = foo
aws_secret_access_key = bar
[myprofile]
aws_access_key_id = AWS_ACCESS_KEY_ID
aws_secret_access_key = AWS_SECRET_ACCESS_KEY
[default]
aws_access_key_id = foo
aws_secret_access_key = bar
""")

    os_aws.join("config").write("""
[unrelated]
aws_access_key_id = foo
aws_secret_access_key = bar
[profile myprofile]
region = us-east-1
[default]
aws_access_key_id = foo
aws_secret_access_key = bar
""")

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/s3_fake_bucket/resource",
        custom_method=get_s3_fake_bucket_resource_method,
    )
    with webserver.install_http_handler(handler):
        with gdaltest.config_options(options, thread_local=False):
            with gdaltest.config_option(
                "USERPROFILE" if sys.platform == "win32" else "HOME",
                str(tmpdir),
                thread_local=False,
            ):
                f = open_for_read("/vsis3/s3_fake_bucket/resource")
        assert f is not None
        data = gdal.VSIFReadL(1, 4, f).decode("ascii")
        gdal.VSIFCloseL(f)

    assert data == "foo"


###############################################################################
# Read credentials from simulated ~/.aws/credentials and ~/.aws/config


def test_vsis3_read_credentials_config_file_inconsistent(
    tmp_vsimem, aws_test_config, webserver_port
):

    options = {
        "AWS_SECRET_ACCESS_KEY": "",
        "AWS_ACCESS_KEY_ID": "",
        "CPL_AWS_CREDENTIALS_FILE": f"{tmp_vsimem}/aws_credentials",
        "AWS_CONFIG_FILE": f"{tmp_vsimem}/aws_config",
    }

    gdal.VSICurlClearCache()

    gdal.FileFromMemBuffer(
        tmp_vsimem / "aws_credentials",
        """
[unrelated]
aws_access_key_id = foo
aws_secret_access_key = bar
[default]
aws_access_key_id = AWS_ACCESS_KEY_ID
aws_secret_access_key = AWS_SECRET_ACCESS_KEY
[unrelated]
aws_access_key_id = foo
aws_secret_access_key = bar
""",
    )

    gdal.FileFromMemBuffer(
        tmp_vsimem / "aws_config",
        """
[unrelated]
aws_access_key_id = foo
aws_secret_access_key = bar
[default]
aws_access_key_id = AWS_ACCESS_KEY_ID_inconsistent
aws_secret_access_key = AWS_SECRET_ACCESS_KEY_inconsistent
region = us-east-1
[unrelated]
aws_access_key_id = foo
aws_secret_access_key = bar
""",
    )

    gdal.ErrorReset()
    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/s3_fake_bucket/resource",
        custom_method=get_s3_fake_bucket_resource_method,
    )
    with webserver.install_http_handler(handler):
        with gdaltest.config_options(options, thread_local=False):
            with gdal.quiet_errors():
                f = open_for_read("/vsis3/s3_fake_bucket/resource")
        assert f is not None
        assert gdal.GetLastErrorMsg() != ""
        data = gdal.VSIFReadL(1, 4, f).decode("ascii")
        gdal.VSIFCloseL(f)

    assert data == "foo"


###############################################################################
# Read credentials from sts AssumeRoleWithWebIdentity
@pytest.mark.skipif(sys.platform not in ("linux", "win32"), reason="Incorrect platform")
def test_vsis3_read_credentials_sts_assume_role_with_web_identity(
    aws_test_config, webserver_port
):
    fp = tempfile.NamedTemporaryFile(delete=False)
    fp.write(b"token")
    fp.close()

    aws_role_arn = "arn:aws:iam:role/test"
    aws_role_arn_encoded = urllib.parse.quote_plus(aws_role_arn)
    options = {
        "CPL_AWS_CREDENTIALS_FILE": "",
        "AWS_CONFIG_FILE": "",
        "AWS_SECRET_ACCESS_KEY": "",
        "AWS_ACCESS_KEY_ID": "",
        "AWS_ROLE_ARN": aws_role_arn,
        "AWS_WEB_IDENTITY_TOKEN_FILE": fp.name,
    }

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        f"/?Action=AssumeRoleWithWebIdentity&RoleSessionName=gdal&Version=2011-06-15&RoleArn={aws_role_arn_encoded}&WebIdentityToken=token",
        200,
        {},
        """<AssumeRoleWithWebIdentityResponse xmlns="https://sts.amazonaws.com/doc/2011-06-15/">
          <AssumeRoleWithWebIdentityResult>
            <SubjectFromWebIdentityToken>amzn1.account.AF6RHO7KZU5XRVQJGXK6HB56KR2A</SubjectFromWebIdentityToken>
            <Audience>client.5498841531868486423.1548@apps.example.com</Audience>
            <AssumedRoleUser>
              <Arn>arn:aws:sts::123456789012:assumed-role/FederatedWebIdentityRole/app1</Arn>
              <AssumedRoleId>AROACLKWSDQRAOEXAMPLE:app1</AssumedRoleId>
            </AssumedRoleUser>
            <Credentials>
              <SessionToken>AWS_SESSION_TOKEN</SessionToken>
              <SecretAccessKey>AWS_SECRET_ACCESS_KEY</SecretAccessKey>
              <Expiration>3000-01-01T00:00:00Z</Expiration>
              <AccessKeyId>AWS_ACCESS_KEY_ID</AccessKeyId>
            </Credentials>
            <SourceIdentity>SourceIdentityValue</SourceIdentity>
            <Provider>www.amazon.com</Provider>
          </AssumeRoleWithWebIdentityResult>
          <ResponseMetadata>
            <RequestId>ad4156e9-bce1-11e2-82e6-6b6efEXAMPLE</RequestId>
          </ResponseMetadata>
        </AssumeRoleWithWebIdentityResponse>""",
    )
    handler.add(
        "GET",
        "/s3_fake_bucket/resource",
        custom_method=get_s3_fake_bucket_resource_method_with_security_token,
    )
    with webserver.install_http_handler(handler):
        with gdaltest.config_options(options, thread_local=False):
            with gdaltest.config_option(
                "CPL_AWS_STS_ROOT_URL",
                "http://localhost:%d" % webserver_port,
                thread_local=False,
            ):
                f = open_for_read("/vsis3/s3_fake_bucket/resource")
        assert f is not None
        data = gdal.VSIFReadL(1, 4, f).decode("ascii")
        gdal.VSIFCloseL(f)

    gdal.Unlink(fp.name)
    assert data == "foo"


###############################################################################
# Read credentials from simulated EC2 instance
@pytest.mark.skipif(sys.platform not in ("linux", "win32"), reason="Incorrect platform")
def test_vsis3_read_credentials_ec2_imdsv2(aws_test_config, webserver_port):
    options = {
        "CPL_AWS_CREDENTIALS_FILE": "",
        "AWS_CONFIG_FILE": "",
        "AWS_SECRET_ACCESS_KEY": "",
        "AWS_ACCESS_KEY_ID": "",
        # Disable hypervisor related check to test if we are really on EC2
        "CPL_AWS_AUTODETECT_EC2": "NO",
    }

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add(
        "PUT",
        "/latest/api/token",
        200,
        {},
        "mytoken",
        expected_headers={"X-aws-ec2-metadata-token-ttl-seconds": "10"},
    )
    handler.add(
        "GET",
        "/latest/meta-data/iam/security-credentials/",
        200,
        {},
        "myprofile",
        expected_headers={"X-aws-ec2-metadata-token": "mytoken"},
    )
    handler.add(
        "GET",
        "/latest/meta-data/iam/security-credentials/myprofile",
        200,
        {},
        """{
        "AccessKeyId": "AWS_ACCESS_KEY_ID",
        "SecretAccessKey": "AWS_SECRET_ACCESS_KEY",
        "Expiration": "3000-01-01T00:00:00Z"
        }""",
        expected_headers={"X-aws-ec2-metadata-token": "mytoken"},
    )

    handler.add(
        "GET",
        "/s3_fake_bucket/resource",
        custom_method=get_s3_fake_bucket_resource_method,
    )
    with webserver.install_http_handler(handler):
        initial_options = copy.copy(options)
        initial_options["CPL_AWS_WEB_IDENTITY_ENABLE"] = "NO"
        with gdaltest.config_options(initial_options, thread_local=False):
            with gdaltest.config_option(
                "CPL_AWS_EC2_API_ROOT_URL",
                "http://localhost:%d" % webserver_port,
                thread_local=False,
            ):
                f = open_for_read("/vsis3/s3_fake_bucket/resource")
        assert f is not None
        data = gdal.VSIFReadL(1, 4, f).decode("ascii")
        gdal.VSIFCloseL(f)

    assert data == "foo"

    handler = webserver.SequentialHandler()
    handler.add("GET", "/s3_fake_bucket/bar", 200, {}, "bar")
    with webserver.install_http_handler(handler):
        with gdaltest.config_options(options, thread_local=False):
            # Set a fake URL to check that credentials re-use works
            with gdaltest.config_option(
                "CPL_AWS_EC2_API_ROOT_URL", "", thread_local=False
            ):
                f = open_for_read("/vsis3/s3_fake_bucket/bar")
        assert f is not None
        data = gdal.VSIFReadL(1, 4, f).decode("ascii")
        gdal.VSIFCloseL(f)

    assert data == "bar"

    # We can reuse credentials here as their expiration is far away in the future
    with gdaltest.config_options(
        {**options, "CPL_AWS_EC2_API_ROOT_URL": "http://localhost:%d" % webserver_port},
        thread_local=False,
    ):
        signed_url = gdal.GetSignedURL("/vsis3/s3_fake_bucket/resource")
    expected_url_8080 = (
        "http://127.0.0.1:8080/s3_fake_bucket/resource"
        "?X-Amz-Algorithm=AWS4-HMAC-SHA256"
        "&X-Amz-Credential="
        "AWS_ACCESS_KEY_ID%2F20150101%2Fus-east-1%2Fs3%2Faws4_request"
        "&X-Amz-Date=20150101T000000Z&X-Amz-Expires=3600"
        "&X-Amz-Signature="
        "dca239dd95f72ff8c37c15c840afc54cd19bdb07f7aaee2223108b5b0ad35da8"
        "&X-Amz-SignedHeaders=host"
    )
    expected_url_8081 = (
        "http://127.0.0.1:8081/s3_fake_bucket/resource"
        "?X-Amz-Algorithm=AWS4-HMAC-SHA256"
        "&X-Amz-Credential="
        "AWS_ACCESS_KEY_ID%2F20150101%2Fus-east-1%2Fs3%2Faws4_request"
        "&X-Amz-Date=20150101T000000Z&X-Amz-Expires=3600"
        "&X-Amz-Signature="
        "ef5216bc5971863414c69f6ca095276c0d62c0da97fa4f6ab80c30bd7fc146ac"
        "&X-Amz-SignedHeaders=host"
    )
    expected_url_8082 = (
        "http://127.0.0.1:8082/s3_fake_bucket/resource"
        "?X-Amz-Algorithm=AWS4-HMAC-SHA256"
        "&X-Amz-Credential="
        "AWS_ACCESS_KEY_ID%2F20150101%2Fus-east-1%2Fs3%2Faws4_request"
        "&X-Amz-Date=20150101T000000Z&X-Amz-Expires=3600"
        "&X-Amz-Signature="
        "9b14dd2c511c8916b2bffced71ab3405980cda1cc6019f6159b60dd0d9dac9b2"
        "&X-Amz-SignedHeaders=host"
    )
    assert signed_url in (expected_url_8080, expected_url_8081, expected_url_8082)

    # Now test asking for an expiration in a super long delay, which will
    # cause credentials to be queried again
    handler = webserver.SequentialHandler()
    handler.add(
        "PUT",
        "/latest/api/token",
        200,
        {},
        "mytoken2",
        expected_headers={"X-aws-ec2-metadata-token-ttl-seconds": "10"},
    )
    handler.add(
        "GET",
        "/latest/meta-data/iam/security-credentials/myprofile",
        200,
        {},
        """{
        "AccessKeyId": "AWS_ACCESS_KEY_ID",
        "SecretAccessKey": "AWS_SECRET_ACCESS_KEY",
        "Token": "AWS_SESSION_TOKEN",
        "Expiration": "5000-01-01T00:00:00Z"
        }""",
        expected_headers={"X-aws-ec2-metadata-token": "mytoken2"},
    )

    with gdaltest.config_options(
        {**options, "CPL_AWS_EC2_API_ROOT_URL": "http://localhost:%d" % webserver_port},
        thread_local=False,
    ):
        with webserver.install_http_handler(handler):
            signed_url = gdal.GetSignedURL(
                "/vsis3/s3_fake_bucket/resource",
                ["EXPIRATION_DELAY=" + str(2000 * 365 * 86400)],
            )
    expected_url_8080 = (
        "http://127.0.0.1:8080/s3_fake_bucket/resource"
        "?X-Amz-Algorithm=AWS4-HMAC-SHA256&"
        "X-Amz-Credential="
        "AWS_ACCESS_KEY_ID%2F20150101%2Fus-east-1%2Fs3%2Faws4_request"
        "&X-Amz-Date=20150101T000000Z"
        "&X-Amz-Expires=63072000000"
        "&X-Amz-Security-Token=AWS_SESSION_TOKEN"
        "&X-Amz-Signature="
        "42770a74a5ad96940a42d5660959d36bb027d3ec8433d66d1b003983ef9f47c9"
        "&X-Amz-SignedHeaders=host"
    )
    expected_url_8081 = (
        "http://127.0.0.1:8081/s3_fake_bucket/resource"
        "?X-Amz-Algorithm=AWS4-HMAC-SHA256&"
        "X-Amz-Credential="
        "AWS_ACCESS_KEY_ID%2F20150101%2Fus-east-1%2Fs3%2Faws4_request"
        "&X-Amz-Date=20150101T000000Z"
        "&X-Amz-Expires=63072000000"
        "&X-Amz-Security-Token=AWS_SESSION_TOKEN"
        "&X-Amz-Signature="
        "20e35d2707bd2e2896879dc009f5327d4dfd43500e16bb1c6e157dd5eda4403f"
        "&X-Amz-SignedHeaders=host"
    )
    expected_url_8082 = (
        "http://127.0.0.1:8082/s3_fake_bucket/resource"
        "?X-Amz-Algorithm=AWS4-HMAC-SHA256&"
        "X-Amz-Credential="
        "AWS_ACCESS_KEY_ID%2F20150101%2Fus-east-1%2Fs3%2Faws4_request"
        "&X-Amz-Date=20150101T000000Z"
        "&X-Amz-Expires=63072000000"
        "&X-Amz-Security-Token=AWS_SESSION_TOKEN"
        "&X-Amz-Signature="
        "78de87b917169b13420a6f45363e5ae0b50f215476d70318d0c64efd0dc82edc"
        "&X-Amz-SignedHeaders=host"
    )
    assert signed_url in (
        expected_url_8080,
        expected_url_8081,
        expected_url_8082,
    ), signed_url


###############################################################################
# Read credentials from simulated EC2 instance that only supports IMDSv1
@pytest.mark.skipif(sys.platform not in ("linux", "win32"), reason="Incorrect platform")
def test_vsis3_read_credentials_ec2_imdsv1(aws_test_config, webserver_port):
    options = {
        "CPL_AWS_CREDENTIALS_FILE": "",
        "AWS_CONFIG_FILE": "",
        "AWS_SECRET_ACCESS_KEY": "",
        "AWS_ACCESS_KEY_ID": "",
        "CPL_AWS_EC2_API_ROOT_URL": "http://localhost:%d" % webserver_port,
        # Disable hypervisor related check to test if we are really on EC2
        "CPL_AWS_AUTODETECT_EC2": "NO",
    }

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add(
        "PUT",
        "/latest/api/token",
        403,
        {},
        expected_headers={"X-aws-ec2-metadata-token-ttl-seconds": "10"},
    )
    handler.add(
        "GET",
        "/latest/meta-data/iam/security-credentials/",
        200,
        {},
        "myprofile",
        unexpected_headers=["X-aws-ec2-metadata-token"],
    )
    handler.add(
        "GET",
        "/latest/meta-data/iam/security-credentials/myprofile",
        200,
        {},
        """{
        "AccessKeyId": "AWS_ACCESS_KEY_ID",
        "SecretAccessKey": "AWS_SECRET_ACCESS_KEY",
        "Expiration": "3000-01-01T00:00:00Z"
        }""",
        unexpected_headers=["X-aws-ec2-metadata-token"],
    )

    handler.add(
        "GET",
        "/s3_fake_bucket/resource",
        custom_method=get_s3_fake_bucket_resource_method,
    )
    with webserver.install_http_handler(handler):
        initial_options = copy.copy(options)
        initial_options["CPL_AWS_WEB_IDENTITY_ENABLE"] = "NO"
        with gdaltest.config_options(initial_options, thread_local=False):
            f = open_for_read("/vsis3/s3_fake_bucket/resource")
        assert f is not None
        data = gdal.VSIFReadL(1, 4, f).decode("ascii")
        gdal.VSIFCloseL(f)

    assert data == "foo"

    handler = webserver.SequentialHandler()
    handler.add("GET", "/s3_fake_bucket/bar", 200, {}, "bar")
    with webserver.install_http_handler(handler):
        with gdaltest.config_options(options, thread_local=False):
            # Set a fake URL to check that credentials re-use works
            with gdaltest.config_option(
                "CPL_AWS_EC2_API_ROOT_URL", "", thread_local=False
            ):
                f = open_for_read("/vsis3/s3_fake_bucket/bar")
        assert f is not None
        data = gdal.VSIFReadL(1, 4, f).decode("ascii")
        gdal.VSIFCloseL(f)

    assert data == "bar"


###############################################################################
# Read credentials from simulated EC2 instance with expiration of the
# cached credentials
@pytest.mark.skipif(sys.platform not in ("linux", "win32"), reason="Incorrect platform")
def test_vsis3_read_credentials_ec2_expiration(aws_test_config, webserver_port):

    options = {
        "CPL_AWS_CREDENTIALS_FILE": "",
        "AWS_CONFIG_FILE": "",
        "AWS_SECRET_ACCESS_KEY": "",
        "AWS_ACCESS_KEY_ID": "",
        # Disable hypervisor related check to test if we are really on EC2
        "CPL_AWS_AUTODETECT_EC2": "NO",
    }

    valid_url = "http://localhost:%d" % webserver_port
    invalid_url = "http://localhost:%d/invalid" % webserver_port

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add(
        "PUT",
        "/latest/api/token",
        200,
        {},
        "mytoken",
        expected_headers={"X-aws-ec2-metadata-token-ttl-seconds": "10"},
    )
    handler.add(
        "GET",
        "/latest/meta-data/iam/security-credentials/",
        200,
        {},
        "myprofile",
        expected_headers={"X-aws-ec2-metadata-token": "mytoken"},
    )
    handler.add(
        "GET",
        "/latest/meta-data/iam/security-credentials/myprofile",
        200,
        {},
        """{
        "AccessKeyId": "AWS_ACCESS_KEY_ID",
        "SecretAccessKey": "AWS_SECRET_ACCESS_KEY",
        "Expiration": "1970-01-01T00:00:00Z"
        }""",
        expected_headers={"X-aws-ec2-metadata-token": "mytoken"},
    )
    handler.add(
        "PUT",
        "/latest/api/token",
        200,
        {},
        "mytoken2",
        expected_headers={"X-aws-ec2-metadata-token-ttl-seconds": "10"},
    )
    handler.add(
        "GET",
        "/latest/meta-data/iam/security-credentials/myprofile",
        200,
        {},
        """{
        "AccessKeyId": "AWS_ACCESS_KEY_ID",
        "SecretAccessKey": "AWS_SECRET_ACCESS_KEY",
        "Expiration": "1970-01-01T00:00:00Z"
        }""",
        expected_headers={"X-aws-ec2-metadata-token": "mytoken2"},
    )
    handler.add(
        "GET",
        "/s3_fake_bucket/resource",
        custom_method=get_s3_fake_bucket_resource_method,
    )
    with webserver.install_http_handler(handler):
        initial_options = copy.copy(options)
        initial_options["CPL_AWS_WEB_IDENTITY_ENABLE"] = "NO"
        with gdaltest.config_options(initial_options, thread_local=False):
            with gdaltest.config_option(
                "CPL_AWS_EC2_API_ROOT_URL", valid_url, thread_local=False
            ):
                f = open_for_read("/vsis3/s3_fake_bucket/resource")
                assert f is not None
                data = gdal.VSIFReadL(1, 4, f).decode("ascii")
                gdal.VSIFCloseL(f)

    assert data == "foo"

    handler = webserver.SequentialHandler()
    handler.add("PUT", "/invalid/latest/api/token", 404)
    handler.add(
        "GET", "/invalid/latest/meta-data/iam/security-credentials/myprofile", 404
    )
    with webserver.install_http_handler(handler):
        with gdaltest.config_options(options, thread_local=False):
            # Set a fake URL to demonstrate we try to re-fetch credentials
            with gdaltest.config_option(
                "CPL_AWS_EC2_API_ROOT_URL", invalid_url, thread_local=False
            ):
                with gdal.quiet_errors():
                    f = open_for_read("/vsis3/s3_fake_bucket/bar")
        assert f is None


###############################################################################
# Read credentials from simulated instance with AWS_CONTAINER_CREDENTIALS_FULL_URI


@pytest.mark.skipif(sys.platform not in ("linux", "win32"), reason="Incorrect platform")
def test_vsis3_read_credentials_AWS_CONTAINER_CREDENTIALS_FULL_URI(
    aws_test_config, webserver_port
):
    options = {
        "CPL_AWS_CREDENTIALS_FILE": "",
        "AWS_CONFIG_FILE": "",
        "AWS_SECRET_ACCESS_KEY": "",
        "AWS_ACCESS_KEY_ID": "",
        # Disable hypervisor related check to test if we are really on EC2
        "CPL_AWS_AUTODETECT_EC2": "NO",
        "AWS_CONTAINER_CREDENTIALS_FULL_URI": f"http://localhost:{webserver_port}/AWS_CONTAINER_CREDENTIALS_FULL_URI",
    }

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/AWS_CONTAINER_CREDENTIALS_FULL_URI",
        200,
        {},
        """{
        "AccessKeyId": "AWS_ACCESS_KEY_ID",
        "SecretAccessKey": "AWS_SECRET_ACCESS_KEY",
        "Expiration": "3000-01-01T00:00:00Z"
        }""",
    )

    handler.add(
        "GET",
        "/s3_fake_bucket/resource",
        custom_method=get_s3_fake_bucket_resource_method,
    )
    with webserver.install_http_handler(handler):
        initial_options = copy.copy(options)
        initial_options["CPL_AWS_WEB_IDENTITY_ENABLE"] = "NO"
        with gdaltest.config_options(initial_options, thread_local=False):
            f = open_for_read("/vsis3/s3_fake_bucket/resource")
        assert f is not None
        data = gdal.VSIFReadL(1, 4, f).decode("ascii")
        gdal.VSIFCloseL(f)

    assert data == "foo"

    handler = webserver.SequentialHandler()
    handler.add("GET", "/s3_fake_bucket/bar", 200, {}, "bar")
    with webserver.install_http_handler(handler):
        with gdaltest.config_options(options, thread_local=False):
            # Set a fake URL to check that credentials re-use works
            with gdaltest.config_option(
                "CPL_AWS_EC2_API_ROOT_URL", "", thread_local=False
            ):
                f = open_for_read("/vsis3/s3_fake_bucket/bar")
        assert f is not None
        data = gdal.VSIFReadL(1, 4, f).decode("ascii")
        gdal.VSIFCloseL(f)

    assert data == "bar"


###############################################################################
# Read credentials from simulated instance with AWS_CONTAINER_AUTHORIZATION_TOKEN_FILE


@pytest.mark.skipif(sys.platform not in ("linux", "win32"), reason="Incorrect platform")
def test_vsis3_read_credentials_AWS_CONTAINER_AUTHORIZATION_TOKEN_FILE(
    tmp_vsimem, aws_test_config, webserver_port
):
    options = {
        "CPL_AWS_CREDENTIALS_FILE": "",
        "AWS_CONFIG_FILE": "",
        "AWS_SECRET_ACCESS_KEY": "",
        "AWS_ACCESS_KEY_ID": "",
        # Disable hypervisor related check to test if we are really on EC2
        "CPL_AWS_AUTODETECT_EC2": "NO",
        "AWS_CONTAINER_CREDENTIALS_FULL_URI": f"http://localhost:{webserver_port}/AWS_CONTAINER_CREDENTIALS_FULL_URI",
        "AWS_CONTAINER_AUTHORIZATION_TOKEN_FILE": f"{tmp_vsimem}/container_authorization_token_file",
        "AWS_CONTAINER_AUTHORIZATION_TOKEN": "invalid",
    }

    gdal.VSICurlClearCache()

    gdal.FileFromMemBuffer(tmp_vsimem / "container_authorization_token_file", "valid\n")

    handler = webserver.SequentialHandler()

    handler.add(
        "GET",
        "/AWS_CONTAINER_CREDENTIALS_FULL_URI",
        200,
        {"Content-Type": "application/json"},
        b"""{
            "AccessKeyId": "AWS_ACCESS_KEY_ID",
            "SecretAccessKey": "AWS_SECRET_ACCESS_KEY",
            "Expiration": "3000-01-01T00:00:00Z"
            }""",
        expected_headers={"Authorization": "valid"},
    )
    handler.add(
        "GET",
        "/s3_fake_bucket/resource",
        custom_method=get_s3_fake_bucket_resource_method,
    )
    with webserver.install_http_handler(handler):
        initial_options = copy.copy(options)
        initial_options["CPL_AWS_WEB_IDENTITY_ENABLE"] = "NO"
        with gdaltest.config_options(initial_options, thread_local=False):
            f = open_for_read("/vsis3/s3_fake_bucket/resource")
        assert f is not None
        data = gdal.VSIFReadL(1, 4, f).decode("ascii")
        gdal.VSIFCloseL(f)

    assert data == "foo"

    handler = webserver.SequentialHandler()
    handler.add("GET", "/s3_fake_bucket/bar", 200, {}, "bar")
    with webserver.install_http_handler(handler):
        with gdaltest.config_options(options, thread_local=False):
            # Set a fake URL to check that credentials re-use works
            with gdaltest.config_option(
                "CPL_AWS_EC2_API_ROOT_URL", "", thread_local=False
            ):
                f = open_for_read("/vsis3/s3_fake_bucket/bar")
        assert f is not None
        data = gdal.VSIFReadL(1, 4, f).decode("ascii")
        gdal.VSIFCloseL(f)

    assert data == "bar"


###############################################################################
# Read credentials from simulated instance with AWS_CONTAINER_AUTHORIZATION_TOKEN


@pytest.mark.skipif(sys.platform not in ("linux", "win32"), reason="Incorrect platform")
def test_vsis3_read_credentials_AWS_CONTAINER_AUTHORIZATION_TOKEN(
    aws_test_config, webserver_port
):
    options = {
        "CPL_AWS_CREDENTIALS_FILE": "",
        "AWS_CONFIG_FILE": "",
        "AWS_SECRET_ACCESS_KEY": "",
        "AWS_ACCESS_KEY_ID": "",
        # Disable hypervisor related check to test if we are really on EC2
        "CPL_AWS_AUTODETECT_EC2": "NO",
        "CPL_AWS_WEB_IDENTITY_ENABLE": "NO",
        "AWS_CONTAINER_CREDENTIALS_FULL_URI": f"http://localhost:{webserver_port}/AWS_CONTAINER_CREDENTIALS_FULL_URI",
        "AWS_CONTAINER_AUTHORIZATION_TOKEN_FILE": "",
        "AWS_CONTAINER_AUTHORIZATION_TOKEN": "valid",
    }

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()

    handler.add(
        "GET",
        "/AWS_CONTAINER_CREDENTIALS_FULL_URI",
        200,
        {"Content-Type": "application/json"},
        b"""{
            "AccessKeyId": "AWS_ACCESS_KEY_ID",
            "SecretAccessKey": "AWS_SECRET_ACCESS_KEY",
            "Expiration": "3000-01-01T00:00:00Z"
            }""",
        expected_headers={"Authorization": "valid"},
    )
    handler.add(
        "GET",
        "/s3_fake_bucket/resource",
        custom_method=get_s3_fake_bucket_resource_method,
    )
    with webserver.install_http_handler(handler):
        with gdaltest.config_options(options, thread_local=False):
            f = open_for_read("/vsis3/s3_fake_bucket/resource")
        assert f is not None
        data = gdal.VSIFReadL(1, 4, f).decode("ascii")
        gdal.VSIFCloseL(f)

    assert data == "foo"


###############################################################################
# Read credentials from an assumed role


def test_vsis3_read_credentials_assumed_role(
    tmp_vsimem, aws_test_config, webserver_port
):

    if webserver_port == 8080:
        expected_signature1 = (
            "3dd83fa260ec68bb50814f7fceb0ad79712de94a1ee0b285d13a8069e0a16ab4"
        )
        expected_signature2 = (
            "d5e8167e066e7439e0e57a43e1167f9ee7efe4b451c72de1a3a150f6fc033403"
        )
        expected_signature3 = (
            "9716b5928ed350263c9492159dccbdc9aac321cfea383d7f67bd8b4c7ca33463"
        )
        expected_signature4 = (
            "27e28bd4dad95495b851b54ff875b8ebcec6e0f6f5e4adf045153bd0d7958fbb"
        )
    elif webserver_port == 8081:
        expected_signature1 = (
            "07c7dbd1115cbe87c6f8817d69c722d1b943b12fe3da8e20916a2bec2b02ea6e"
        )
        expected_signature2 = (
            "9db8e06522b6bad431787bd8268248e4f5ae755eeae906ada71ce8641b76998d"
        )
        expected_signature3 = (
            "e560358eaf19d00b98ffea4fb23b0b6572a5b946ad915105dfb8b40ce6d8ed1b"
        )
        expected_signature4 = (
            "ef71ab77159f30793c320cd053081605084b3ac7f30f470b0a6fb499df2d4c77"
        )
    elif webserver_port == 8082:
        expected_signature1 = (
            "3848be422122d11c4fc4f94c832b1934dedf7edbd2c47136dc21a479a5f746cf"
        )
        expected_signature2 = (
            "75876d78a08bfe8d1c4e3f32627d5185f7f336f27cf315a197c52e36402e9867"
        )
        expected_signature3 = (
            "fd3c9d5d8536616ccc389848a343775c105139428ab44571f9a180c4afe86911"
        )
        expected_signature4 = (
            "4e241bf76cebda2ab48cd414d47db465d4cdbd77ecaa5d64fac14b0ba4834ac4"
        )
    else:
        pytest.skip("Expected results coded for webserver_port = 8080, 8081 or 8082")

    options = {
        "AWS_SECRET_ACCESS_KEY": "",
        "AWS_ACCESS_KEY_ID": "",
        "CPL_AWS_CREDENTIALS_FILE": f"{tmp_vsimem}/aws_credentials",
        "AWS_CONFIG_FILE": f"{tmp_vsimem}/aws_config",
        "AWS_PROFILE": "my_profile",
        "AWS_STS_ENDPOINT": "localhost:%d" % webserver_port,
    }

    gdal.VSICurlClearCache()

    gdal.FileFromMemBuffer(
        tmp_vsimem / "aws_credentials",
        """
[foo]
aws_access_key_id = AWS_ACCESS_KEY_ID
aws_secret_access_key = AWS_SECRET_ACCESS_KEY
""",
    )

    gdal.FileFromMemBuffer(
        tmp_vsimem / "aws_config",
        """
[profile my_profile]
role_arn = arn:aws:iam::557268267719:role/role
source_profile = foo
# below are optional
external_id = my_external_id
mfa_serial = my_mfa_serial
role_session_name = my_role_session_name
""",
    )

    expired_xml_response = """
        <AssumeRoleResponse><AssumeRoleResult><Credentials>
            <AccessKeyId>TEMP_ACCESS_KEY_ID</AccessKeyId>
            <SecretAccessKey>TEMP_SECRET_ACCESS_KEY</SecretAccessKey>
            <SessionToken>TEMP_SESSION_TOKEN</SessionToken>
            <Expiration>1970-01-01T01:00:00Z</Expiration>
        </Credentials></AssumeRoleResult></AssumeRoleResponse>"""

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/?Action=AssumeRole&ExternalId=my_external_id&RoleArn=arn%3Aaws%3Aiam%3A%3A557268267719%3Arole%2Frole&RoleSessionName=my_role_session_name&SerialNumber=my_mfa_serial&Version=2011-06-15",
        200,
        {},
        expired_xml_response,
        expected_headers={
            "Authorization": f"AWS4-HMAC-SHA256 Credential=AWS_ACCESS_KEY_ID/20150101/us-east-1/sts/aws4_request,SignedHeaders=host,Signature={expected_signature1}",
            "X-Amz-Date": "20150101T000000Z",
        },
    )
    handler.add(
        "GET",
        "/?Action=AssumeRole&ExternalId=my_external_id&RoleArn=arn%3Aaws%3Aiam%3A%3A557268267719%3Arole%2Frole&RoleSessionName=my_role_session_name&SerialNumber=my_mfa_serial&Version=2011-06-15",
        200,
        {},
        expired_xml_response,
        expected_headers={
            "Authorization": f"AWS4-HMAC-SHA256 Credential=AWS_ACCESS_KEY_ID/20150101/us-east-1/sts/aws4_request,SignedHeaders=host,Signature={expected_signature1}",
            "X-Amz-Date": "20150101T000000Z",
        },
    )
    handler.add(
        "GET",
        "/s3_fake_bucket/resource",
        200,
        {},
        "foo",
        expected_headers={
            "Authorization": f"AWS4-HMAC-SHA256 Credential=TEMP_ACCESS_KEY_ID/20150101/us-east-1/s3/aws4_request,SignedHeaders=host;x-amz-content-sha256;x-amz-date;x-amz-security-token,Signature={expected_signature2}",
            "X-Amz-Security-Token": "TEMP_SESSION_TOKEN",
        },
    )
    with webserver.install_http_handler(handler):
        with gdaltest.config_options(options, thread_local=False):
            f = open_for_read("/vsis3/s3_fake_bucket/resource")
            assert f is not None
            data = gdal.VSIFReadL(1, 4, f).decode("ascii")
        gdal.VSIFCloseL(f)
    assert data == "foo"

    # Get another resource and check that we renew the expired temporary credentials
    non_expired_xml_response = """
        <AssumeRoleResponse><AssumeRoleResult><Credentials>
            <AccessKeyId>ANOTHER_TEMP_ACCESS_KEY_ID</AccessKeyId>
            <SecretAccessKey>ANOTHER_TEMP_SECRET_ACCESS_KEY</SecretAccessKey>
            <SessionToken>ANOTHER_TEMP_SESSION_TOKEN</SessionToken>
            <Expiration>3000-01-01T01:00:00Z</Expiration>
        </Credentials></AssumeRoleResult></AssumeRoleResponse>"""

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/?Action=AssumeRole&ExternalId=my_external_id&RoleArn=arn%3Aaws%3Aiam%3A%3A557268267719%3Arole%2Frole&RoleSessionName=my_role_session_name&SerialNumber=my_mfa_serial&Version=2011-06-15",
        200,
        {},
        non_expired_xml_response,
    )
    handler.add(
        "GET",
        "/s3_fake_bucket/resource2",
        200,
        {},
        "foo",
        expected_headers={
            "Authorization": f"AWS4-HMAC-SHA256 Credential=ANOTHER_TEMP_ACCESS_KEY_ID/20150101/us-east-1/s3/aws4_request,SignedHeaders=host;x-amz-content-sha256;x-amz-date;x-amz-security-token,Signature={expected_signature3}",
            "X-Amz-Security-Token": "ANOTHER_TEMP_SESSION_TOKEN",
        },
    )
    with webserver.install_http_handler(handler):
        with gdaltest.config_options(options, thread_local=False):
            f = open_for_read("/vsis3/s3_fake_bucket/resource2")
        assert f is not None
        data = gdal.VSIFReadL(1, 4, f).decode("ascii")
        gdal.VSIFCloseL(f)
    assert data == "foo"

    # Get another resource and check that we reuse the still valid temporary credentials
    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/s3_fake_bucket/resource3",
        200,
        {},
        "foo",
        expected_headers={
            "Authorization": f"AWS4-HMAC-SHA256 Credential=ANOTHER_TEMP_ACCESS_KEY_ID/20150101/us-east-1/s3/aws4_request,SignedHeaders=host;x-amz-content-sha256;x-amz-date;x-amz-security-token,Signature={expected_signature4}",
            "X-Amz-Security-Token": "ANOTHER_TEMP_SESSION_TOKEN",
        },
    )
    with webserver.install_http_handler(handler):
        with gdaltest.config_options(options, thread_local=False):
            f = open_for_read("/vsis3/s3_fake_bucket/resource3")
        assert f is not None
        data = gdal.VSIFReadL(1, 4, f).decode("ascii")
        gdal.VSIFCloseL(f)
    assert data == "foo"


###############################################################################
# Read credentials from sts AssumeRoleWithWebIdentity
def test_vsis3_read_credentials_sts_assume_role_with_web_identity_from_config_file(
    tmp_vsimem, aws_test_config, webserver_port
):

    if webserver_port == 8080:
        expected_signature1 = (
            "d5e8167e066e7439e0e57a43e1167f9ee7efe4b451c72de1a3a150f6fc033403"
        )
        expected_signature2 = (
            "d5abb4e09ad29ad3810cfe21702e7e2e9071798c441acaed9613d62ed8600556"
        )
        expected_signature3 = (
            "a158ddb8b5fd40fd5226c0ca28c14620863b8157c870e7e96ff841662aaef79a"
        )
    elif webserver_port == 8081:
        expected_signature1 = (
            "9db8e06522b6bad431787bd8268248e4f5ae755eeae906ada71ce8641b76998d"
        )
        expected_signature2 = (
            "467838ad283d0f3af9635bc432137504c73ff32a8091dfc1ac98fc11958d91e1"
        )
        expected_signature3 = (
            "d88e0aaaf375cf9f2f065287186455d7aea8f298fb8762011381cd03369c78e0"
        )
    elif webserver_port == 8082:
        expected_signature1 = (
            "75876d78a08bfe8d1c4e3f32627d5185f7f336f27cf315a197c52e36402e9867"
        )
        expected_signature2 = (
            "2bbf96df0fe8b2a1a5249de458cddd5b13de71118347ca661238f165ad6d55af"
        )
        expected_signature3 = (
            "25966d29f0dcd11c0f0f82bc2148762179bd9f9eaea24806d231251db8da6b6d"
        )
    else:
        pytest.skip("Expected results coded for webserver_port = 8080, 8081 or 8082")

    options = {
        "AWS_SECRET_ACCESS_KEY": "",
        "AWS_ACCESS_KEY_ID": "",
        "CPL_AWS_CREDENTIALS_FILE": f"{tmp_vsimem}/aws_credentials",
        "AWS_CONFIG_FILE": f"{tmp_vsimem}/aws_config",
        "AWS_PROFILE": "my_profile",
        "AWS_STS_ENDPOINT": "localhost:%d" % webserver_port,
        "CPL_AWS_STS_ROOT_URL": "http://localhost:%d" % webserver_port,
    }

    gdal.VSICurlClearCache()

    gdal.FileFromMemBuffer(tmp_vsimem / "web_identity_token_file", "token\n")

    gdal.FileFromMemBuffer(tmp_vsimem / "aws_credentials", "")

    gdal.FileFromMemBuffer(
        tmp_vsimem / "aws_config",
        f"""
[profile foo]
role_arn = foo_role_arn
web_identity_token_file = {tmp_vsimem}/web_identity_token_file
[profile my_profile]
role_arn = my_profile_role_arn
source_profile = foo
""",
    )

    gdal.VSICurlClearCache()

    assumeRoleWithWebIdentityResponseXML = """<AssumeRoleWithWebIdentityResponse xmlns="https://sts.amazonaws.com/doc/2011-06-15/">
          <AssumeRoleWithWebIdentityResult>
            <SubjectFromWebIdentityToken>amzn1.account.AF6RHO7KZU5XRVQJGXK6HB56KR2A</SubjectFromWebIdentityToken>
            <Audience>client.5498841531868486423.1548@apps.example.com</Audience>
            <AssumedRoleUser>
              <Arn>arn:aws:sts::123456789012:assumed-role/FederatedWebIdentityRole/app1</Arn>
              <AssumedRoleId>AROACLKWSDQRAOEXAMPLE:app1</AssumedRoleId>
            </AssumedRoleUser>
            <Credentials>
              <SessionToken>AWS_SESSION_TOKEN</SessionToken>
              <SecretAccessKey>AWS_SECRET_ACCESS_KEY</SecretAccessKey>
              <Expiration>9999-01-01T00:00:00Z</Expiration>
              <AccessKeyId>AWS_ACCESS_KEY_ID</AccessKeyId>
            </Credentials>
            <SourceIdentity>SourceIdentityValue</SourceIdentity>
            <Provider>www.amazon.com</Provider>
          </AssumeRoleWithWebIdentityResult>
          <ResponseMetadata>
            <RequestId>ad4156e9-bce1-11e2-82e6-6b6efEXAMPLE</RequestId>
          </ResponseMetadata>
        </AssumeRoleWithWebIdentityResponse>"""

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/?Action=AssumeRoleWithWebIdentity&RoleSessionName=gdal&Version=2011-06-15&RoleArn=foo_role_arn&WebIdentityToken=token",
        200,
        {},
        assumeRoleWithWebIdentityResponseXML,
    )

    # Note that the Expiration is in the past, so for a next request we will
    # have to renew
    handler.add(
        "GET",
        "/?Action=AssumeRole&RoleArn=my_profile_role_arn&RoleSessionName=GDAL-session&Version=2011-06-15",
        200,
        {},
        """<AssumeRoleResponse><AssumeRoleResult><Credentials>
            <AccessKeyId>TEMP_ACCESS_KEY_ID</AccessKeyId>
            <SecretAccessKey>TEMP_SECRET_ACCESS_KEY</SecretAccessKey>
            <SessionToken>TEMP_SESSION_TOKEN</SessionToken>
            <Expiration>1970-01-01T01:00:00Z</Expiration>
        </Credentials></AssumeRoleResult></AssumeRoleResponse>""",
    )

    handler.add(
        "GET",
        "/s3_fake_bucket/resource",
        200,
        {},
        "foo",
        expected_headers={
            "Authorization": f"AWS4-HMAC-SHA256 Credential=TEMP_ACCESS_KEY_ID/20150101/us-east-1/s3/aws4_request,SignedHeaders=host;x-amz-content-sha256;x-amz-date;x-amz-security-token,Signature={expected_signature1}",
            "X-Amz-Security-Token": "TEMP_SESSION_TOKEN",
        },
    )

    handler2 = webserver.SequentialHandler()
    handler2.add(
        "GET",
        "/?Action=AssumeRoleWithWebIdentity&RoleSessionName=gdal&Version=2011-06-15&RoleArn=foo_role_arn&WebIdentityToken=token",
        200,
        {},
        assumeRoleWithWebIdentityResponseXML,
    )

    handler2.add(
        "GET",
        "/?Action=AssumeRole&RoleArn=my_profile_role_arn&RoleSessionName=GDAL-session&Version=2011-06-15",
        200,
        {},
        """<AssumeRoleResponse><AssumeRoleResult><Credentials>
            <AccessKeyId>TEMP_ACCESS_KEY_ID</AccessKeyId>
            <SecretAccessKey>TEMP_SECRET_ACCESS_KEY</SecretAccessKey>
            <SessionToken>TEMP_SESSION_TOKEN</SessionToken>
            <Expiration>9999-01-01T01:00:00Z</Expiration>
        </Credentials></AssumeRoleResult></AssumeRoleResponse>""",
    )

    handler2.add(
        "GET",
        "/s3_fake_bucket/resource2",
        200,
        {},
        "foo",
        expected_headers={
            "Authorization": f"AWS4-HMAC-SHA256 Credential=TEMP_ACCESS_KEY_ID/20150101/us-east-1/s3/aws4_request,SignedHeaders=host;x-amz-content-sha256;x-amz-date;x-amz-security-token,Signature={expected_signature2}",
            "X-Amz-Security-Token": "TEMP_SESSION_TOKEN",
        },
    )

    handler2.add(
        "GET",
        "/s3_fake_bucket/resource3",
        200,
        {},
        "foo",
        expected_headers={
            "Authorization": f"AWS4-HMAC-SHA256 Credential=TEMP_ACCESS_KEY_ID/20150101/us-east-1/s3/aws4_request,SignedHeaders=host;x-amz-content-sha256;x-amz-date;x-amz-security-token,Signature={expected_signature3}",
            "X-Amz-Security-Token": "TEMP_SESSION_TOKEN",
        },
    )

    with webserver.install_http_handler(handler):
        with gdaltest.config_options(options, thread_local=False):
            f = open_for_read("/vsis3/s3_fake_bucket/resource")
        assert f is not None
        data = gdal.VSIFReadL(1, 4, f).decode("ascii")
        gdal.VSIFCloseL(f)
    assert data == "foo"

    with webserver.install_http_handler(handler2):
        with gdaltest.config_options(options, thread_local=False):
            f = open_for_read("/vsis3/s3_fake_bucket/resource2")
            assert f is not None
            data = gdal.VSIFReadL(1, 4, f).decode("ascii")
            gdal.VSIFCloseL(f)
            assert data == "foo"

            f = open_for_read("/vsis3/s3_fake_bucket/resource3")
            assert f is not None
            data = gdal.VSIFReadL(1, 4, f).decode("ascii")
            gdal.VSIFCloseL(f)
            assert data == "foo"


###############################################################################
# Read credentials from cached SSO file


def test_vsis3_read_credentials_sso(tmp_vsimem, aws_test_config, webserver_port):

    if webserver_port != 8080:
        pytest.skip("only works for webserver on port 8080")

    options = {
        "AWS_SECRET_ACCESS_KEY": "",
        "AWS_ACCESS_KEY_ID": "",
        "AWS_PROFILE": "my_profile",
        "CPL_AWS_SSO_ENDPOINT": "localhost:%d" % webserver_port,
        "CPL_AWS_ROOT_DIR": str(tmp_vsimem),
    }

    gdal.VSICurlClearCache()

    gdal.FileFromMemBuffer(
        tmp_vsimem / "config",
        """
[sso-session my-sso]
sso_start_url = https://example.com
sso_region = eu-central-1
sso_registration_scopes = sso:account:access

[profile my_profile]
sso_session = my-sso
sso_account_id = my_sso_account_id
sso_role_name = my_sso_role_name
region = eu-east-1
""",
    )

    gdal.FileFromMemBuffer(
        tmp_vsimem / "sso" / "cache" / "0ad374308c5a4e22f723adf10145eafad7c4031c.json",
        '{"startUrl": "https://example.com", "region": "us-east-1", "accessToken": "sso-accessToken", "expiresAt": "9999-01-01T00:00:00Z"}',
    )

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()

    handler.add(
        "GET",
        "/federation/credentials?role_name=my_sso_role_name&account_id=my_sso_account_id",
        200,
        {},
        """{
  "roleCredentials": {
    "accessKeyId": "accessKeyId",
    "secretAccessKey": "secretAccessKey",
    "sessionToken": "sessionToken",
    "expiration": 9999999999000
  }
}""",
        expected_headers={
            "x-amz-sso_bearer_token": "sso-accessToken",
        },
    )

    handler.add(
        "GET",
        "/s3_fake_bucket/resource",
        200,
        {},
        """foo""",
        expected_headers={
            "x-amz-date": "20150101T000000Z",
            "x-amz-content-sha256": "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
            "X-Amz-Security-Token": "sessionToken",
            "Authorization": "AWS4-HMAC-SHA256 Credential=accessKeyId/20150101/us-east-1/s3/aws4_request,SignedHeaders=host;x-amz-content-sha256;x-amz-date;x-amz-security-token,Signature=bfa9fb8c88286e3ef6537303784efe45721ede5e5bf51091565a66cf1ad8084a",
        },
    )

    with webserver.install_http_handler(handler):
        with gdaltest.config_options(options, thread_local=False):
            f = open_for_read("/vsis3/s3_fake_bucket/resource")
        assert f is not None
        try:
            data = gdal.VSIFReadL(1, 4, f).decode("ascii")
        finally:
            gdal.VSIFCloseL(f)

    assert data == "foo"

    handler = webserver.SequentialHandler()

    handler.add(
        "GET",
        "/s3_fake_bucket/resource2",
        200,
        {},
        """bar""",
        expected_headers={
            "x-amz-date": "20150101T000000Z",
            "x-amz-content-sha256": "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
            "X-Amz-Security-Token": "sessionToken",
            "Authorization": "AWS4-HMAC-SHA256 Credential=accessKeyId/20150101/us-east-1/s3/aws4_request,SignedHeaders=host;x-amz-content-sha256;x-amz-date;x-amz-security-token,Signature=b081c4f3195807de3fc934626f7ef7f3fd8e1143226bcbf2afce478c4bb7d4ff",
        },
    )

    with webserver.install_http_handler(handler):
        with gdaltest.config_options(options, thread_local=False):
            f = open_for_read("/vsis3/s3_fake_bucket/resource2")
        assert f is not None
        try:
            data = gdal.VSIFReadL(1, 4, f).decode("ascii")
        finally:
            gdal.VSIFCloseL(f)

    assert data == "bar"


###############################################################################


def test_vsis3_non_existing_file_GDAL_DISABLE_READDIR_ON_OPEN(
    aws_test_config, webserver_port
):
    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add("GET", "/test_bucket/non_existing.tif", 404)
    handler.add(
        "GET",
        "/test_bucket/?delimiter=%2F&list-type=2&max-keys=100&prefix=non_existing.tif%2F",
        200,
    )
    gdal.ErrorReset()
    with gdaltest.config_option(
        "GDAL_DISABLE_READDIR_ON_OPEN", "YES", thread_local=False
    ):
        with webserver.install_http_handler(handler):
            with gdal.quiet_errors():
                gdal.Open("/vsis3/test_bucket/non_existing.tif")
    assert gdal.GetLastErrorMsg() == "HTTP response code: 404"


###############################################################################
# Test DISABLE_READDIR_ON_OPEN=YES VSIFOpenEx2L() option


def test_vsis3_DISABLE_READDIR_ON_OPEN_option(aws_test_config, webserver_port):
    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/test_vsis3_DISABLE_READDIR_ON_OPEN_option/test.bin",
        206,
        {"Content-Length": "3", "Content-Range": "bytes 0-2/3"},
        "foo",
    )
    with webserver.install_http_handler(handler):
        f = gdal.VSIFOpenExL(
            "/vsis3/test_vsis3_DISABLE_READDIR_ON_OPEN_option/test.bin",
            "rb",
            0,
            ["DISABLE_READDIR_ON_OPEN=YES"],
        )
        assert f is not None
        assert gdal.VSIFReadL(3, 1, f) == b"foo"
        gdal.VSIFCloseL(f)


###############################################################################
# Test VSIMultipartUploadXXXX()


def test_vsis3_MultipartUpload(aws_test_config, webserver_port):

    # Test MultipartUploadGetCapabilities()
    info = gdal.MultipartUploadGetCapabilities("/vsis3/")
    assert info.non_sequential_upload_supported
    assert info.parallel_upload_supported
    assert info.abort_supported
    assert info.min_part_size == 5
    assert info.max_part_size >= 1024
    assert info.max_part_count == 10000

    # Test MultipartUploadStart()
    handler = webserver.SequentialHandler()
    handler.add("POST", "/test_multipartupload/test.bin?uploads", 400)
    with webserver.install_http_handler(handler), gdal.quiet_errors():
        upload_id = gdal.MultipartUploadStart("/vsis3/test_multipartupload/test.bin")
    assert upload_id is None

    handler = webserver.SequentialHandler()
    handler.add(
        "POST",
        "/test_multipartupload/test.bin?uploads",
        200,
        {},
        """xml version="1.0" encoding="UTF-8"?>
        <InitiateMultipartUploadResult>
        <UploadId>my_upload_id</UploadId>
        </InitiateMultipartUploadResult>""",
    )
    with webserver.install_http_handler(handler):
        upload_id = gdal.MultipartUploadStart("/vsis3/test_multipartupload/test.bin")
    assert upload_id == "my_upload_id"

    # Test MultipartUploadAddPart()
    handler = webserver.SequentialHandler()
    handler.add(
        "PUT",
        "/test_multipartupload/test.bin?partNumber=1&uploadId=my_upload_id",
        400,
    )
    with webserver.install_http_handler(handler), gdal.quiet_errors():
        part_id = gdal.MultipartUploadAddPart(
            "/vsis3/test_multipartupload/test.bin", "my_upload_id", 1, 0, b"foo"
        )
    assert part_id is None

    handler = webserver.SequentialHandler()
    handler.add(
        "PUT",
        "/test_multipartupload/test.bin?partNumber=1&uploadId=my_upload_id",
        200,
        {"ETag": '"my_part_id"', "Content-Length": "0"},
        b"",
        expected_body=b"foo",
    )
    with webserver.install_http_handler(handler):
        part_id = gdal.MultipartUploadAddPart(
            "/vsis3/test_multipartupload/test.bin", "my_upload_id", 1, 0, b"foo"
        )
    assert part_id == '"my_part_id"'

    # Test MultipartUploadEnd()

    handler = webserver.SequentialHandler()
    handler.add("POST", "/test_multipartupload/test.bin?uploadId=my_upload_id", 400)
    with webserver.install_http_handler(handler), gdal.quiet_errors():
        assert not gdal.MultipartUploadEnd(
            "/vsis3/test_multipartupload/test.bin", "my_upload_id", ['"my_part_id"'], 3
        )

    handler = webserver.SequentialHandler()
    handler.add(
        "POST",
        "/test_multipartupload/test.bin?uploadId=my_upload_id",
        200,
        expected_body=b"""<CompleteMultipartUpload>
<Part>
<PartNumber>1</PartNumber><ETag>"my_part_id"</ETag></Part>
</CompleteMultipartUpload>
""",
    )
    with webserver.install_http_handler(handler):
        assert gdal.MultipartUploadEnd(
            "/vsis3/test_multipartupload/test.bin", "my_upload_id", ['"my_part_id"'], 3
        )

    # Test MultipartUploadAbort()

    handler = webserver.SequentialHandler()
    handler.add("DELETE", "/test_multipartupload/test.bin?uploadId=my_upload_id", 400)
    with webserver.install_http_handler(handler), gdal.quiet_errors():
        assert not gdal.MultipartUploadAbort(
            "/vsis3/test_multipartupload/test.bin", "my_upload_id"
        )

    handler = webserver.SequentialHandler()
    handler.add("DELETE", "/test_multipartupload/test.bin?uploadId=my_upload_id", 204)
    with webserver.install_http_handler(handler):
        assert gdal.MultipartUploadAbort(
            "/vsis3/test_multipartupload/test.bin", "my_upload_id"
        )


###############################################################################
# Test VSIMultipartUploadXXXX() when authentication fails


def test_vsis3_MultipartUpload_unauthenticated():

    options = {
        "AWS_SECRET_ACCESS_KEY": "",
        "AWS_ACCESS_KEY_ID": "",
    }
    with gdaltest.config_options(options, thread_local=False), gdal.quiet_errors():
        assert gdal.MultipartUploadStart("/vsis3/test_multipartupload/test.bin") is None
        assert (
            gdal.MultipartUploadAddPart(
                "/vsis3/test_multipartupload/test.bin", "my_upload_id", 1, 0, b"foo"
            )
            is None
        )
        assert not gdal.MultipartUploadEnd(
            "/vsis3/test_multipartupload/test.bin", "my_upload_id", ['"my_part_id"'], 3
        )
        assert not gdal.MultipartUploadAbort(
            "/vsis3/test_multipartupload/test.bin", "my_upload_id"
        )


###############################################################################
# Test listing a directory bucket (ListObjectsV2 API)


def test_vsis3_list_directory_bucket(aws_test_config, webserver_port):
    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/mydirbucket--regionid--x-s3/?delimiter=%2F&list-type=2",
        200,
        {"Content-type": "application/xml"},
        """<?xml version="1.0" encoding="UTF-8"?>
        <ListBucketResult>
            <Prefix></Prefix>
            <Contents>
                <Key>test.bin</Key>
                <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                <Size>123456</Size>
            </Contents>
            <NextMarker>do_not_use_me</NextMarker> <!-- Apache Ozone includes both NextMarker and NextContinuationToken -->
            <NextContinuationToken>next_continuation_token</NextContinuationToken>
        </ListBucketResult>
        """,
    )
    handler.add(
        "GET",
        "/mydirbucket--regionid--x-s3/?continuation-token=next_continuation_token&delimiter=%2F&list-type=2",
        200,
        {"Content-type": "application/xml"},
        """<?xml version="1.0" encoding="UTF-8"?>
        <ListBucketResult>
            <Prefix></Prefix>
            <Contents>
                <Key>test2.bin</Key>
                <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                <Size>123456</Size>
            </Contents>
        </ListBucketResult>
        """,
    )
    with webserver.install_http_handler(handler):
        assert gdal.ReadDir("/vsis3/mydirbucket--regionid--x-s3") == [
            "test.bin",
            "test2.bin",
        ]


###############################################################################
# Test AWS_S3SESSION_TOKEN config option (for directory buckets)


def test_vsis3_AWS_S3SESSION_TOKEN(aws_test_config, webserver_port):
    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/mydirbucket--regionid--x-s3/?delimiter=%2F&list-type=2",
        200,
        {"Content-type": "application/xml"},
        """<?xml version="1.0" encoding="UTF-8"?>
        <ListBucketResult>
            <Prefix></Prefix>
            <Contents>
                <Key>test.bin</Key>
                <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                <Size>123456</Size>
            </Contents>
        </ListBucketResult>
        """,
        expected_headers={"x-amz-s3session-token": "the_token"},
    )
    with gdal.config_option("AWS_S3SESSION_TOKEN", "the_token"):
        with webserver.install_http_handler(handler):
            assert gdal.ReadDir("/vsis3/mydirbucket--regionid--x-s3") == ["test.bin"]


###############################################################################
# Test PATH_VERBATIM


def test_vsis3_PATH_VERBATIM(aws_test_config, webserver_port):

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add(
        "GET", "/test_vsis3_PATH_VERBATIM/test.bin", 200, {"Connection": "close"}, "a"
    )
    with webserver.install_http_handler(handler):
        assert gdal.VSIStatL("/vsis3/test_vsis3_PATH_VERBATIM/./test.bin").size == 1

    handler = webserver.SequentialHandler()
    handler.add(
        "GET", "/test_vsis3_PATH_VERBATIM/test2.bin", 200, {"Connection": "close"}, "ab"
    )
    with webserver.install_http_handler(handler):
        assert (
            gdal.VSIStatL(
                "/vsis3/test_vsis3_PATH_VERBATIM/a/b/../../c/../test2.bin"
            ).size
            == 2
        )

    with gdal.config_option("GDAL_HTTP_PATH_VERBATIM", "YES"):
        handler = webserver.SequentialHandler()
        handler.add(
            "GET",
            "/test_vsis3_PATH_VERBATIM/./test3.bin",
            200,
            {"Connection": "close"},
            "abc",
        )
        with webserver.install_http_handler(handler):
            assert (
                gdal.VSIStatL("/vsis3/test_vsis3_PATH_VERBATIM/./test3.bin").size == 3
            )


###############################################################################
# Nominal cases (require valid credentials)


def test_vsis3_extra_1():

    credentials_filename = (
        gdal.GetConfigOption("HOME", gdal.GetConfigOption("USERPROFILE", ""))
        + "/.aws/credentials"
    )

    # Either a bucket name or bucket/filename
    s3_resource = gdal.GetConfigOption("S3_RESOURCE")

    if not os.path.exists(credentials_filename):
        if gdal.GetConfigOption("AWS_SECRET_ACCESS_KEY") is None:
            pytest.skip("Missing AWS_SECRET_ACCESS_KEY")
        elif gdal.GetConfigOption("AWS_ACCESS_KEY_ID") is None:
            pytest.skip("Missing AWS_ACCESS_KEY_ID")

    if s3_resource is None:
        pytest.skip("Missing S3_RESOURCE")

    if "/" not in s3_resource:
        path = "/vsis3/" + s3_resource
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

        unique_id = "vsis3_test"
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

        f = gdal.VSIFOpenExL(
            subpath + "/test.txt", "wb", 0, ["Content-Type=foo", "Content-Encoding=bar"]
        )
        assert f is not None
        gdal.VSIFWriteL("hello", 1, 5, f)
        gdal.VSIFCloseL(f)

        md = gdal.GetFileMetadata(subpath + "/test.txt", "HEADERS")
        assert "Content-Type" in md
        assert md["Content-Type"] == "foo"
        assert "Content-Encoding" in md
        assert md["Content-Encoding"] == "bar"

        ret = gdal.Rmdir(subpath)
        assert ret != 0, (
            "Rmdir(%s) on non empty directory should return an error" % subpath
        )

        f = gdal.VSIFOpenL(subpath + "/test.txt", "rb")
        assert f is not None
        data = gdal.VSIFReadL(1, 5, f).decode("utf-8")
        assert data == "hello"
        gdal.VSIFCloseL(f)

        assert gdal.Rename(subpath + "/test.txt", subpath + "/test2.txt") == 0

        f = gdal.VSIFOpenL(subpath + "/test2.txt", "rb")
        assert f is not None
        data = gdal.VSIFReadL(1, 5, f).decode("utf-8")
        assert data == "hello"
        gdal.VSIFCloseL(f)

        ret = gdal.Unlink(subpath + "/test2.txt")
        assert ret >= 0, "Unlink(%s) should not return an error" % (
            subpath + "/test2.txt"
        )

        ret = gdal.Rmdir(subpath)
        assert ret >= 0, "Rmdir(%s) should not return an error" % subpath

        def test_sync():
            local_file = "tmp/gdal_sync_test.bin"
            remote_file = path + "/gdal_sync_test.bin"

            f = gdal.VSIFOpenL(local_file, "wb")
            gdal.VSIFWriteL("foo" * 10000, 1, 3 * 10000, f)
            gdal.VSIFCloseL(f)

            gdal.Sync(local_file, remote_file)
            gdal.Unlink(local_file)
            gdal.Sync(remote_file, local_file)

            assert gdal.VSIStatL(local_file).size == 3 * 10000

            f = gdal.VSIFOpenL(local_file, "wb")
            gdal.VSIFWriteL("foobar" * 10000, 1, 6 * 10000, f)
            gdal.VSIFCloseL(f)

            gdal.Sync(local_file, remote_file)

            assert gdal.VSIStatL(remote_file).size == 6 * 10000

            s = gdal.VSIStatL(remote_file.replace("/vsis3/", "/vsis3_streaming/")).size
            assert s == 6 * 10000

            gdal.Unlink(local_file)
            gdal.Sync(remote_file, local_file)

            assert gdal.VSIStatL(local_file).size == 6 * 10000

            gdal.Unlink(local_file)
            gdal.Unlink(remote_file)

        test_sync()

        return

    f = open_for_read("/vsis3/" + s3_resource)
    assert f is not None, "cannot open %s" % ("/vsis3/" + s3_resource)
    ret = gdal.VSIFReadL(1, 1, f)
    gdal.VSIFCloseL(f)

    assert len(ret) == 1

    # Same with /vsis3_streaming/
    f = open_for_read("/vsis3_streaming/" + s3_resource)
    assert f is not None
    ret = gdal.VSIFReadL(1, 1, f)
    gdal.VSIFCloseL(f)

    assert len(ret) == 1

    if False:  # pylint: disable=using-constant-test
        # we actually try to read at read() time and bSetError = false
        # Invalid bucket : "The specified bucket does not exist"
        gdal.ErrorReset()
        f = open_for_read("/vsis3/not_existing_bucket/foo")
        with gdal.quiet_errors():
            gdal.VSIFReadL(1, 1, f)
        gdal.VSIFCloseL(f)
        assert gdal.VSIGetLastErrorMsg() != ""

    # Invalid resource
    gdal.ErrorReset()
    f = open_for_read(
        "/vsis3_streaming/"
        + gdal.GetConfigOption("S3_RESOURCE")
        + "/invalid_resource.baz"
    )
    assert f is None, gdal.VSIGetLastErrorMsg()

    # Test GetSignedURL()
    signed_url = gdal.GetSignedURL("/vsis3/" + s3_resource)
    f = open_for_read("/vsicurl_streaming/" + signed_url)
    assert f is not None
    ret = gdal.VSIFReadL(1, 1, f)
    gdal.VSIFCloseL(f)

    assert len(ret) == 1


###############################################################################
# Test credential_process authentication


def test_vsis3_credential_process(
    tmp_path, tmp_vsimem, aws_test_config, webserver_port
):
    script_content = """#!/usr/bin/env python3
import json
import sys
credentials = {
    "Version": "1",
    "AccessKeyId": "AWS_ACCESS_KEY_ID",
    "SecretAccessKey": "AWS_SECRET_ACCESS_KEY",
    "SessionToken": "AWS_SESSION_TOKEN",
    "Expiration": "3000-01-01T12:00:00Z"
}
print(json.dumps(credentials))
"""

    script_path = tmp_path / "script.py"
    with open(script_path, "wb") as f:
        f.write(script_content.encode("utf-8"))

    os.chmod(script_path, stat.S_IRUSR | stat.S_IWUSR | stat.S_IXUSR)

    options = {
        "AWS_SECRET_ACCESS_KEY": "",
        "AWS_ACCESS_KEY_ID": "",
        "AWS_SESSION_TOKEN": "",
        "AWS_CONFIG_FILE": f"{tmp_vsimem}/aws_config",
        "CPL_AWS_AUTODETECT_EC2": "NO",
    }

    gdal.VSICurlClearCache()

    gdal.FileFromMemBuffer(
        tmp_vsimem / "aws_config",
        f"""
[default]
credential_process = {sys.executable} {script_path}
region = us-east-1
""",
    )

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/s3_fake_bucket/resource",
        custom_method=get_s3_fake_bucket_resource_method_with_security_token,
    )

    with webserver.install_http_handler(handler):
        with gdaltest.config_options(options, thread_local=False):
            f = open_for_read("/vsis3/s3_fake_bucket/resource")
        assert f is not None
        data = gdal.VSIFReadL(1, 4, f).decode("ascii")
        gdal.VSIFCloseL(f)

    assert data == "foo"

    # Given the expiration long in the uture, test that re-trying a request
    # does not involve running the script
    os.unlink(script_path)
    gdal.VSICurlPartialClearCache("/vsis3/s3_fake_bucket/resource")

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/s3_fake_bucket/resource",
        custom_method=get_s3_fake_bucket_resource_method_with_security_token,
    )
    with webserver.install_http_handler(handler):
        with gdaltest.config_options(options, thread_local=False):
            f = open_for_read("/vsis3/s3_fake_bucket/resource")
        assert f is not None
        data = gdal.VSIFReadL(1, 4, f).decode("ascii")
        gdal.VSIFCloseL(f)

    assert data == "foo"


###############################################################################
# Test credential_process authentication


def test_vsis3_credential_process_expired(
    tmp_path, tmp_vsimem, aws_test_config, webserver_port
):
    script_content = """#!/usr/bin/env python3
import json
import sys
credentials = {
    "Version": "1",
    "AccessKeyId": "AWS_ACCESS_KEY_ID",
    "SecretAccessKey": "AWS_SECRET_ACCESS_KEY",
    "SessionToken": "AWS_SESSION_TOKEN",
    "Expiration": "2025-01-01T12:00:00Z"
}
print(json.dumps(credentials))
"""

    script_path = tmp_path / "script.py"
    with open(script_path, "wb") as f:
        f.write(script_content.encode("utf-8"))

    os.chmod(script_path, stat.S_IRUSR | stat.S_IWUSR | stat.S_IXUSR)

    options = {
        "AWS_SECRET_ACCESS_KEY": "",
        "AWS_ACCESS_KEY_ID": "",
        "AWS_SESSION_TOKEN": "",
        "AWS_CONFIG_FILE": f"{tmp_vsimem}/aws_config",
        "CPL_AWS_AUTODETECT_EC2": "NO",
    }

    gdal.VSICurlClearCache()

    gdal.FileFromMemBuffer(
        tmp_vsimem / "aws_config",
        f"""
[default]
credential_process = {sys.executable} {script_path}
region = us-east-1
""",
    )

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/s3_fake_bucket/resource",
        custom_method=get_s3_fake_bucket_resource_method_with_security_token,
    )

    with webserver.install_http_handler(handler):
        with gdaltest.config_options(options, thread_local=False):
            f = open_for_read("/vsis3/s3_fake_bucket/resource")
        assert f is not None
        data = gdal.VSIFReadL(1, 4, f).decode("ascii")
        gdal.VSIFCloseL(f)

    assert data == "foo"

    os.unlink(script_path)
    gdal.VSICurlPartialClearCache("/vsis3/s3_fake_bucket/resource")

    with gdaltest.config_options(options, thread_local=False), gdal.quiet_errors():
        f = open_for_read("/vsis3/s3_fake_bucket/resource")
    assert f is None


###############################################################################
# Test credential_process with invalid command


def test_vsis3_credential_process_invalid_command(tmp_vsimem, aws_test_config):
    options = {
        "AWS_SECRET_ACCESS_KEY": "",
        "AWS_ACCESS_KEY_ID": "",
        "AWS_SESSION_TOKEN": "",
        "AWS_CONFIG_FILE": f"{tmp_vsimem}/aws_config",
        "CPL_AWS_AUTODETECT_EC2": "NO",
    }

    gdal.VSICurlClearCache()

    gdal.FileFromMemBuffer(
        tmp_vsimem / "aws_config",
        """
[default]
credential_process = /nonexistent/command
region = us-east-1
""",
    )

    with gdaltest.config_options(options, thread_local=False):
        with gdal.quiet_errors():
            f = open_for_read("/vsis3/s3_fake_bucket/resource")
        assert f is None


###############################################################################
# Test credential_process with invalid JSON


def test_vsis3_credential_process_invalid_json(tmp_vsimem, aws_test_config):
    script_content = """#!/usr/bin/env python3
print('invalid json response')
"""

    with tempfile.NamedTemporaryFile(mode="w", suffix=".py", delete=False) as f:
        f.write(script_content)
        script_path = f.name

    os.chmod(script_path, stat.S_IRUSR | stat.S_IWUSR | stat.S_IXUSR)

    try:
        options = {
            "AWS_SECRET_ACCESS_KEY": "",
            "AWS_ACCESS_KEY_ID": "",
            "AWS_SESSION_TOKEN": "",
            "AWS_CONFIG_FILE": f"{tmp_vsimem}/aws_config",
            "CPL_AWS_AUTODETECT_EC2": "NO",
        }

        gdal.VSICurlClearCache()

        gdal.FileFromMemBuffer(
            tmp_vsimem / "aws_config",
            f"""
[default]
credential_process = {sys.executable} {script_path}
region = us-east-1
""",
        )

        with gdaltest.config_options(options, thread_local=False):
            with gdal.quiet_errors():
                f = open_for_read("/vsis3/s3_fake_bucket/resource")
            assert f is None

    finally:
        os.unlink(script_path)


###############################################################################
# Test credential_process with missing required fields


def test_vsis3_credential_process_missing_fields(tmp_vsimem, aws_test_config):

    script_content = """#!/usr/bin/env python3
import json
credentials = {"AccessKeyId": "test_key"}
print(json.dumps(credentials))
"""

    with tempfile.NamedTemporaryFile(mode="w", suffix=".py", delete=False) as f:
        f.write(script_content)
        script_path = f.name

    os.chmod(script_path, stat.S_IRUSR | stat.S_IWUSR | stat.S_IXUSR)

    try:
        options = {
            "AWS_SECRET_ACCESS_KEY": "",
            "AWS_ACCESS_KEY_ID": "",
            "AWS_SESSION_TOKEN": "",
            "AWS_CONFIG_FILE": f"{tmp_vsimem}/aws_config",
            "CPL_AWS_AUTODETECT_EC2": "NO",
        }

        gdal.VSICurlClearCache()

        gdal.FileFromMemBuffer(
            tmp_vsimem / "aws_config",
            f"""
[default]
credential_process = {sys.executable} {script_path}
region = us-east-1
""",
        )

        with gdaltest.config_options(options, thread_local=False):
            with gdal.quiet_errors():
                f = open_for_read("/vsis3/s3_fake_bucket/resource")
            assert f is None

    finally:
        os.unlink(script_path)
