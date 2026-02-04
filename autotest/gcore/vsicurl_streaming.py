#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test /vsicurl_streaming
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2012-2013, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import os
import threading
import time

import gdaltest
import pytest
import webserver

from osgeo import gdal

pytestmark = pytest.mark.require_curl()

###############################################################################
#


@pytest.mark.network
def test_vsicurl_streaming_real_resource():
    # Occasionally fails on Travis graviton2 configuration
    gdaltest.skip_on_travis()

    with gdal.config_option("GDAL_HTTP_CONNECTTIMEOUT", "5"):
        fp = gdal.VSIFOpenL(
            "/vsicurl_streaming/http://download.osgeo.org/gdal/data/usgsdem/cded/114p01_0100_deme.dem",
            "rb",
        )

    if fp is None:
        if (
            gdaltest.gdalurlopen(
                "http://download.osgeo.org/gdal/data/usgsdem/cded/114p01_0100_deme.dem",
                timeout=4,
            )
            is None
        ):
            pytest.skip("cannot open URL")
        pytest.fail()

    if gdal.VSIFTellL(fp) != 0:
        gdal.VSIFCloseL(fp)
        pytest.fail()
    data = gdal.VSIFReadL(1, 50, fp)
    if data.decode("ascii") != "                              114p01DEMe   Base Ma":
        gdal.VSIFCloseL(fp)
        pytest.fail()
    if gdal.VSIFTellL(fp) != 50:
        gdal.VSIFCloseL(fp)
        pytest.fail()

    gdal.VSIFSeekL(fp, 0, 0)

    if gdal.VSIFTellL(fp) != 0:
        gdal.VSIFCloseL(fp)
        pytest.fail()
    data = gdal.VSIFReadL(1, 50, fp)
    if data.decode("ascii") != "                              114p01DEMe   Base Ma":
        gdal.VSIFCloseL(fp)
        pytest.fail()
    if gdal.VSIFTellL(fp) != 50:
        gdal.VSIFCloseL(fp)
        pytest.fail()

    time.sleep(0.5)
    gdal.VSIFSeekL(fp, 2001, 0)
    data_2001 = gdal.VSIFReadL(1, 20, fp)
    if data_2001.decode("ascii") != "7-32767-32767-32767-":
        gdal.VSIFCloseL(fp)
        pytest.fail(data_2001)
    if gdal.VSIFTellL(fp) != 2001 + 20:
        gdal.VSIFCloseL(fp)
        pytest.fail()

    gdal.VSIFSeekL(fp, 0, 2)
    if gdal.VSIFTellL(fp) != 9839616:
        gdal.VSIFCloseL(fp)
        pytest.fail()

    nRet = len(gdal.VSIFReadL(1, 10, fp))
    if nRet != 0:
        gdal.VSIFCloseL(fp)
        pytest.fail()

    gdal.VSIFSeekL(fp, 2001, 0)
    data_2001_2 = gdal.VSIFReadL(1, 20, fp)
    if gdal.VSIFTellL(fp) != 2001 + 20:
        gdal.VSIFCloseL(fp)
        pytest.fail()

    if data_2001 != data_2001_2:
        gdal.VSIFCloseL(fp)
        pytest.fail()

    gdal.VSIFSeekL(fp, 1024 * 1024 + 100, 0)
    data = gdal.VSIFReadL(1, 20, fp)
    if data.decode("ascii") != "67-32767-32767-32767":
        gdal.VSIFCloseL(fp)
        pytest.fail(data)
    if gdal.VSIFTellL(fp) != 1024 * 1024 + 100 + 20:
        gdal.VSIFCloseL(fp)
        pytest.fail()

    gdal.VSIFCloseL(fp)


###############################################################################
#


@pytest.fixture(scope="module")
def webserver_port():

    webserver_process, webserver_port = webserver.launch(
        handler=webserver.DispatcherHttpHandler
    )
    try:
        if webserver_port == 0:
            pytest.skip()
        yield webserver_port
    finally:
        gdal.VSICurlClearCache()

        webserver.server_stop(webserver_process, webserver_port)


###############################################################################
#


def test_vsicurl_streaming_ring_buffer_saturation(webserver_port):

    gdal.VSICurlClearCache()

    def method(request):
        request.protocol_version = "HTTP/1.1"
        request.send_response(200)
        request.send_header("Content-Length", 2 * 1024 * 1024)
        request.end_headers()
        request.wfile.write(("x" * (2 * 1024 * 1024)).encode("ascii"))

    handler = webserver.SequentialHandler()
    handler.add("GET", "/test.bin", custom_method=method)

    with webserver.install_http_handler(handler):
        f = gdal.VSIFOpenL(
            f"/vsicurl_streaming/http://localhost:{webserver_port}/test.bin", "rb"
        )
        try:
            assert f
            assert gdal.VSIFReadL(1, 1, f) == b"x"
            time.sleep(0.5)
            read = gdal.VSIFReadL(1, 1024 * 1024 - 1, f)
            if read != b"x" * (
                1024 * 1024 - 1
            ):  # do not use assertion as pytest is really slow
                assert False
            read = gdal.VSIFReadL(1, 1024 * 1024, f)
            if read != b"x" * (1024 * 1024):
                assert False
        finally:
            gdal.VSIFCloseL(f)


###############################################################################
#


def test_vsicurl_streaming_partial_read(webserver_port):

    gdal.VSICurlClearCache()

    def method(request):
        request.protocol_version = "HTTP/1.1"
        request.send_response(200)
        request.send_header("Content-Length", 128 * 1024)
        request.end_headers()
        request.wfile.write(("x" * (128 * 1024)).encode("ascii"))

    handler = webserver.SequentialHandler()
    handler.add("GET", "/test.bin", custom_method=method)

    with webserver.install_http_handler(handler):
        f = gdal.VSIFOpenL(
            f"/vsicurl_streaming/http://localhost:{webserver_port}/test.bin", "rb"
        )
        try:
            assert f
            assert gdal.VSIFReadL(1, 1, f) == b"x"
        finally:
            gdal.VSIFCloseL(f)


###############################################################################
#


def test_vsicurl_streaming_retry_at_beginning(webserver_port):

    gdal.VSICurlClearCache()

    with gdaltest.config_options(
        {
            "GDAL_HTTP_MAX_RETRY": "2",
            "GDAL_HTTP_RETRY_DELAY": "0.01",
            "CPL_VSIL_CURL_STREMAING_SIMULATED_CURL_ERROR": "Send failure: Connection was reset",
        },
        thread_local=False,
    ):
        handler = webserver.SequentialHandler()
        handler.add("GET", "/test.bin", 200, {"Content-Length": "50000"}, "x")
        handler.add("GET", "/test.bin", 200, {"Content-Length": "50000"}, "x" * 50000)
        with webserver.install_http_handler(handler):
            f = gdal.VSIFOpenL(
                f"/vsicurl_streaming/http://localhost:{webserver_port}/test.bin", "rb"
            )
            assert f
            try:
                read = gdal.VSIFReadL(1, 50, f)
                if read != b"x" * 50:
                    print(read)
                    assert False
                read = gdal.VSIFReadL(1, 50000 - 50, f)
                if read != b"x" * (50000 - 50):
                    assert False
            finally:
                gdal.VSIFCloseL(f)


###############################################################################
#


def test_vsicurl_streaming_retry_in_middle(webserver_port):

    gdal.VSICurlClearCache()

    with gdaltest.config_options(
        {
            "GDAL_HTTP_MAX_RETRY": "2",
            "GDAL_HTTP_RETRY_DELAY": "0.01",
            "CPL_VSIL_CURL_STREMAING_SIMULATED_CURL_ERROR": "Send failure: Connection was reset",
        },
        thread_local=False,
    ):
        handler = webserver.SequentialHandler()

        file_size = 50000
        first_batch_len = 1024

        lock = threading.Lock()
        cv = threading.Condition(lock)
        stop_server = False

        def method(request):
            request.protocol_version = "HTTP/1.1"
            request.send_response(200)
            request.send_header("Content-Length", file_size)
            request.end_headers()
            request.wfile.write(("x" * first_batch_len).encode("ascii"))
            request.wfile.flush()
            with lock:
                while not stop_server:
                    cv.wait()

        handler.add("GET", "/test.bin", custom_method=method)
        # Not very realistic: the server changes the content from x to y in
        # the retry, but this enables us to easily check where we got data from
        handler.add("GET", "/test.bin", 200, {}, "y" * file_size)
        with webserver.install_http_handler(handler):
            f = gdal.VSIFOpenL(
                f"/vsicurl_streaming/http://localhost:{webserver_port}/test.bin", "rb"
            )
            assert f
            try:
                first_read_len = 50
                assert first_read_len <= first_batch_len

                read = gdal.VSIFReadL(1, first_read_len, f)
                if read != b"x" * first_read_len:
                    print(read)
                    assert False

                with lock:
                    stop_server = True
                    cv.notify()

                read = gdal.VSIFReadL(1, file_size - first_read_len, f)
                if read != b"x" * (first_batch_len - first_read_len) + b"y" * (
                    file_size - first_batch_len
                ):
                    print(read)
                    assert False
            finally:
                gdal.VSIFCloseL(f)


###############################################################################
#


def test_vsicurl_streaming_retry_in_middle_failed(webserver_port):

    gdal.VSICurlClearCache()

    with gdaltest.config_options(
        {
            "GDAL_HTTP_MAX_RETRY": "2",
            "GDAL_HTTP_RETRY_DELAY": "0.01",
            "CPL_VSIL_CURL_STREMAING_SIMULATED_CURL_ERROR": "Send failure: Connection was reset",
        },
        thread_local=False,
    ):
        handler = webserver.SequentialHandler()

        file_size = 50000
        first_batch_len = 1024

        lock = threading.Lock()
        cv = threading.Condition(lock)
        stop_server = False

        def method(request):
            request.protocol_version = "HTTP/1.1"
            request.send_response(200)
            request.send_header("Content-Length", file_size)
            request.end_headers()
            request.wfile.write(("x" * first_batch_len).encode("ascii"))
            request.wfile.flush()
            with lock:
                while not stop_server:
                    cv.wait()

        handler.add("GET", "/test.bin", custom_method=method)
        handler.add("GET", "/test.bin", 200, {"Content-Length": str(file_size)}, "y")
        handler.add("GET", "/test.bin", 200, {"Content-Length": str(file_size)}, "y")
        with webserver.install_http_handler(handler):
            f = gdal.VSIFOpenL(
                f"/vsicurl_streaming/http://localhost:{webserver_port}/test.bin", "rb"
            )
            assert f
            try:
                first_read_len = 50
                assert first_read_len <= first_batch_len

                read = gdal.VSIFReadL(1, first_read_len, f)
                if read != b"x" * first_read_len:
                    print(read)
                    assert False

                with lock:
                    stop_server = True
                    cv.notify()

                read = gdal.VSIFReadL(1, file_size - first_read_len, f)
                if read != b"x" * (first_batch_len - first_read_len):
                    print(read)
                    assert False
                assert gdal.VSIFEofL(f)
            finally:
                gdal.VSIFCloseL(f)


###############################################################################
#


def test_vsicurl_streaming_cached_file_size(webserver_port):

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add(
        "GET", "/test.bin", 200, {"Content-Length": 1024 * 1024}, b"x" * (1024 * 1024)
    )

    with webserver.install_http_handler(handler):
        with gdal.VSIFile(
            f"/vsicurl_streaming/http://localhost:{webserver_port}/test.bin", "rb"
        ) as f:
            assert f.read(1) == b"x"
            f.seek(0, os.SEEK_END)
            assert f.tell() == 1024 * 1024
