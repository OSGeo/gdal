#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test /vsiwebhdfs
# Author:   Even Rouault <even dot rouault at spatialys dot com>
#
###############################################################################
# Copyright (c) 2018 Even Rouault <even dot rouault at spatialys dot com>
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

import stat
import sys

import gdaltest
import pytest
import webserver

from osgeo import gdal

pytestmark = pytest.mark.require_curl()


@pytest.fixture(scope="module", autouse=True)
def startup_and_cleanup():

    options = {"WEBHDFS_USERNAME": None, "WEBHDFS_DELEGATION": None}

    with gdal.config_options(options):
        yield


###############################################################################


def open_for_read(uri):
    """
    Opens a test file for reading.
    """
    return gdal.VSIFOpenExL(uri, "rb", 1)


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


@pytest.fixture(scope="module")
def webhdfs_base_connection(server):

    return f"/vsiwebhdfs/http://localhost:{server.port}/webhdfs/v1"


@pytest.fixture(scope="module")
def webhdfs_redirected_url(server):

    return f"http://non_existing_host:{server.port}/redirected"


###############################################################################
# Test VSIFOpenL()


def test_vsiwebhdfs_open(webhdfs_base_connection, webhdfs_redirected_url):

    gdal.VSICurlClearCache()

    # Download without redirect (not nominal)
    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/webhdfs/v1/foo/bar?op=OPEN&offset=9999990784&length=16384",
        200,
        {},
        "0123456789data",
    )
    with webserver.install_http_handler(handler):
        f = open_for_read(webhdfs_base_connection + "/foo/bar")
        assert f is not None
        gdal.VSIFSeekL(f, 9999990784 + 10, 0)
        assert gdal.VSIFReadL(1, 4, f).decode("ascii") == "data"
        gdal.VSIFCloseL(f)

    # Download with redirect (nominal) and permissions

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/webhdfs/v1/foo/bar?op=OPEN&offset=0&length=16384&user.name=root&delegation=token",
        307,
        {
            "Location": webhdfs_redirected_url
            + "/webhdfs/v1/foo/bar?op=OPEN&offset=0&length=16384"
        },
    )
    handler.add(
        "GET",
        "/redirected/webhdfs/v1/foo/bar?op=OPEN&offset=0&length=16384",
        200,
        {},
        "yeah",
    )
    with gdaltest.config_options(
        {
            "WEBHDFS_USERNAME": "root",
            "WEBHDFS_DELEGATION": "token",
            "WEBHDFS_DATANODE_HOST": "localhost",
        }
    ):
        with webserver.install_http_handler(handler):
            f = open_for_read(webhdfs_base_connection + "/foo/bar")
            assert f is not None
            assert gdal.VSIFReadL(1, 4, f).decode("ascii") == "yeah"
            gdal.VSIFCloseL(f)

    # Test error

    gdal.VSICurlClearCache()

    f = open_for_read(webhdfs_base_connection + "/foo/bar")
    assert f is not None

    handler = webserver.SequentialHandler()
    handler.add("GET", "/webhdfs/v1/foo/bar?op=OPEN&offset=0&length=16384", 404)
    with webserver.install_http_handler(handler):
        assert len(gdal.VSIFReadL(1, 4, f)) == 0

    # Retry: shouldn't not cause network access
    assert len(gdal.VSIFReadL(1, 4, f)) == 0

    gdal.VSIFCloseL(f)


###############################################################################
# Test VSIStatL()


def test_vsiwebhdfs_stat(webhdfs_base_connection):

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/webhdfs/v1/foo/bar?op=GETFILESTATUS",
        200,
        {},
        '{"FileStatus":{"type":"FILE","length":1000000}}',
    )
    with webserver.install_http_handler(handler):
        stat_res = gdal.VSIStatL(webhdfs_base_connection + "/foo/bar")
    if stat_res is None or stat_res.size != 1000000:
        if stat_res is not None:
            print(stat_res.size)
        else:
            print(stat_res)
        pytest.fail()

    # Test caching
    stat_res = gdal.VSIStatL(webhdfs_base_connection + "/foo/bar")
    assert stat_res.size == 1000000

    # Test missing file
    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/webhdfs/v1/unexisting?op=GETFILESTATUS",
        404,
        {},
        '{"RemoteException":{"exception":"FileNotFoundException","javaClassName":"java.io.FileNotFoundException","message":"File does not exist: /unexisting"}}',
    )
    with webserver.install_http_handler(handler):
        stat_res = gdal.VSIStatL(webhdfs_base_connection + "/unexisting")
    assert stat_res is None


###############################################################################
# Test ReadDir()


def test_vsiwebhdfs_readdir(webhdfs_base_connection):

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/webhdfs/v1/foo/?op=LISTSTATUS",
        200,
        {},
        '{"FileStatuses":{"FileStatus":[{"type":"FILE","modificationTime":1000,"pathSuffix":"bar.baz","length":123456},{"type":"DIRECTORY","pathSuffix":"mysubdir","length":0}]}}',
    )
    with webserver.install_http_handler(handler):
        dir_contents = gdal.ReadDir(webhdfs_base_connection + "/foo")
    assert dir_contents == ["bar.baz", "mysubdir"]
    stat_res = gdal.VSIStatL(webhdfs_base_connection + "/foo/bar.baz")
    assert stat_res.size == 123456
    assert stat_res.mtime == 1

    # ReadDir on something known to be a file shouldn't cause network access
    dir_contents = gdal.ReadDir(webhdfs_base_connection + "/foo/bar.baz")
    assert dir_contents is None

    # Test error on ReadDir()
    handler = webserver.SequentialHandler()
    handler.add("GET", "/webhdfs/v1foo/error_test/?op=LISTSTATUS", 404)
    with webserver.install_http_handler(handler):
        dir_contents = gdal.ReadDir(webhdfs_base_connection + "foo/error_test/")
    assert dir_contents is None


###############################################################################
# Test write


@gdaltest.disable_exceptions()
def test_vsiwebhdfs_write(webhdfs_base_connection, webhdfs_redirected_url):

    gdal.VSICurlClearCache()

    # Zero length file
    handler = webserver.SequentialHandler()
    with webserver.install_http_handler(handler):
        # Missing required config options
        with gdal.quiet_errors():
            f = gdal.VSIFOpenL(webhdfs_base_connection + "/foo/bar", "wb")
        assert f is None

    handler = webserver.SequentialHandler()
    handler.add(
        "PUT",
        "/webhdfs/v1/foo/bar?op=CREATE&overwrite=true&user.name=root",
        307,
        {
            "Location": webhdfs_redirected_url
            + "/webhdfs/v1/foo/bar?op=CREATE&overwrite=true&user.name=root"
        },
    )
    handler.add(
        "PUT",
        "/redirected/webhdfs/v1/foo/bar?op=CREATE&overwrite=true&user.name=root",
        201,
    )

    with gdaltest.config_options(
        {"WEBHDFS_USERNAME": "root", "WEBHDFS_DATANODE_HOST": "localhost"}
    ):
        with webserver.install_http_handler(handler):
            f = gdal.VSIFOpenL(webhdfs_base_connection + "/foo/bar", "wb")
            assert f is not None
    assert gdal.VSIFCloseL(f) == 0

    # Non-empty file

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add(
        "PUT",
        "/webhdfs/v1/foo/bar?op=CREATE&overwrite=true&user.name=root",
        307,
        {
            "Location": webhdfs_redirected_url
            + "/webhdfs/v1/foo/bar?op=CREATE&overwrite=true&user.name=root"
        },
    )
    handler.add(
        "PUT",
        "/redirected/webhdfs/v1/foo/bar?op=CREATE&overwrite=true&user.name=root",
        201,
    )

    with gdaltest.config_options(
        {"WEBHDFS_USERNAME": "root", "WEBHDFS_DATANODE_HOST": "localhost"}
    ):
        with webserver.install_http_handler(handler):
            f = gdal.VSIFOpenL(webhdfs_base_connection + "/foo/bar", "wb")
            assert f is not None

    assert gdal.VSIFWriteL("foobar", 1, 6, f) == 6

    handler = webserver.SequentialHandler()

    def method(request):
        h = request.headers
        if "Content-Length" in h and h["Content-Length"] != 0:
            sys.stderr.write("Bad headers: %s\n" % str(request.headers))
            request.send_response(403)
            request.send_header("Content-Length", 0)
            request.end_headers()

        request.protocol_version = "HTTP/1.1"
        request.send_response(307)
        request.send_header(
            "Location",
            webhdfs_redirected_url + "/webhdfs/v1/foo/bar?op=APPEND&user.name=root",
        )
        request.end_headers()

    handler.add(
        "POST",
        "/webhdfs/v1/foo/bar?op=APPEND&user.name=root",
        307,
        custom_method=method,
    )
    handler.add(
        "POST",
        "/redirected/webhdfs/v1/foo/bar?op=APPEND&user.name=root",
        200,
        expected_body="foobar".encode("ascii"),
    )

    with webserver.install_http_handler(handler):
        assert gdal.VSIFCloseL(f) == 0

    # Errors during file creation

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add(
        "PUT", "/webhdfs/v1/foo/bar?op=CREATE&overwrite=true&user.name=root", 404
    )
    with gdaltest.config_options(
        {"WEBHDFS_USERNAME": "root", "WEBHDFS_DATANODE_HOST": "localhost"}
    ):
        with webserver.install_http_handler(handler):
            with gdal.quiet_errors():
                f = gdal.VSIFOpenL(webhdfs_base_connection + "/foo/bar", "wb")
                assert f is None

    handler = webserver.SequentialHandler()
    handler.add(
        "PUT",
        "/webhdfs/v1/foo/bar?op=CREATE&overwrite=true&user.name=root",
        307,
        {
            "Location": webhdfs_redirected_url
            + "/webhdfs/v1/foo/bar?op=CREATE&overwrite=true&user.name=root"
        },
    )
    with gdaltest.config_options({"WEBHDFS_USERNAME": "root"}):
        with webserver.install_http_handler(handler):
            with gdal.quiet_errors():
                f = gdal.VSIFOpenL(webhdfs_base_connection + "/foo/bar", "wb")
                assert f is None

    # Errors during POST

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add(
        "PUT",
        "/webhdfs/v1/foo/bar?op=CREATE&overwrite=true&user.name=root",
        307,
        {
            "Location": webhdfs_redirected_url
            + "/webhdfs/v1/foo/bar?op=CREATE&overwrite=true&user.name=root"
        },
    )
    handler.add(
        "PUT",
        "/redirected/webhdfs/v1/foo/bar?op=CREATE&overwrite=true&user.name=root",
        201,
    )

    with gdaltest.config_options(
        {"WEBHDFS_USERNAME": "root", "WEBHDFS_DATANODE_HOST": "localhost"}
    ):
        with webserver.install_http_handler(handler):
            f = gdal.VSIFOpenL(webhdfs_base_connection + "/foo/bar", "wb")
            assert f is not None

    assert gdal.VSIFWriteL("foobar", 1, 6, f) == 6

    handler = webserver.SequentialHandler()
    handler.add(
        "POST",
        "/webhdfs/v1/foo/bar?op=APPEND&user.name=root",
        307,
        {
            "Location": webhdfs_redirected_url
            + "/webhdfs/v1/foo/bar?op=APPEND&user.name=root"
        },
    )
    handler.add("POST", "/redirected/webhdfs/v1/foo/bar?op=APPEND&user.name=root", 400)

    with gdal.quiet_errors():
        with webserver.install_http_handler(handler):
            assert gdal.VSIFCloseL(f) != 0


###############################################################################
# Test Unlink()


@gdaltest.disable_exceptions()
def test_vsiwebhdfs_unlink(webhdfs_base_connection):

    gdal.VSICurlClearCache()

    # Success
    handler = webserver.SequentialHandler()
    handler.add("DELETE", "/webhdfs/v1/foo/bar?op=DELETE", 200, {}, '{"boolean":true}')
    with webserver.install_http_handler(handler):
        ret = gdal.Unlink(webhdfs_base_connection + "/foo/bar")
    assert ret == 0

    gdal.VSICurlClearCache()

    # With permissions

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add(
        "DELETE",
        "/webhdfs/v1/foo/bar?op=DELETE&user.name=root&delegation=token",
        200,
        {},
        '{"boolean":true}',
    )
    with gdaltest.config_options(
        {"WEBHDFS_USERNAME": "root", "WEBHDFS_DELEGATION": "token"}
    ):
        with webserver.install_http_handler(handler):
            ret = gdal.Unlink(webhdfs_base_connection + "/foo/bar")
        assert ret == 0

    # Failure
    handler = webserver.SequentialHandler()
    handler.add("DELETE", "/webhdfs/v1/foo/bar?op=DELETE", 200, {}, '{"boolean":false}')
    with webserver.install_http_handler(handler):
        with gdal.quiet_errors():
            ret = gdal.Unlink(webhdfs_base_connection + "/foo/bar")
    assert ret == -1

    gdal.VSICurlClearCache()

    # Failure
    handler = webserver.SequentialHandler()
    handler.add("DELETE", "/webhdfs/v1/foo/bar?op=DELETE", 404, {})
    with webserver.install_http_handler(handler):
        with gdal.quiet_errors():
            ret = gdal.Unlink(webhdfs_base_connection + "/foo/bar")
    assert ret == -1


###############################################################################
# Test Mkdir() / Rmdir()


@gdaltest.disable_exceptions()
def test_vsiwebhdfs_mkdir_rmdir(webhdfs_base_connection):

    gdal.VSICurlClearCache()

    # Invalid name
    ret = gdal.Mkdir("/vsiwebhdfs", 0)
    assert ret != 0

    # Valid
    handler = webserver.SequentialHandler()
    handler.add("PUT", "/webhdfs/v1/foo/dir?op=MKDIRS", 200, {}, '{"boolean":true}')
    with webserver.install_http_handler(handler):
        ret = gdal.Mkdir(webhdfs_base_connection + "/foo/dir", 0)
    assert ret == 0

    # Valid with all options
    handler = webserver.SequentialHandler()
    handler.add(
        "PUT",
        "/webhdfs/v1/foo/dir?op=MKDIRS&user.name=root&delegation=token&permission=755",
        200,
        {},
        '{"boolean":true}',
    )
    with gdaltest.config_options(
        {"WEBHDFS_USERNAME": "root", "WEBHDFS_DELEGATION": "token"}
    ):
        with webserver.install_http_handler(handler):
            ret = gdal.Mkdir(webhdfs_base_connection + "/foo/dir/", 493)  # 0755
        assert ret == 0

    # Error
    handler = webserver.SequentialHandler()
    handler.add("PUT", "/webhdfs/v1/foo/dir_error?op=MKDIRS", 404)
    with webserver.install_http_handler(handler):
        ret = gdal.Mkdir(webhdfs_base_connection + "/foo/dir_error", 0)
    assert ret != 0

    # Root name is invalid
    ret = gdal.Mkdir(webhdfs_base_connection + "/", 0)
    assert ret != 0

    # Invalid name
    ret = gdal.Rmdir("/vsiwebhdfs")
    assert ret != 0

    gdal.VSICurlClearCache()

    # Valid
    handler = webserver.SequentialHandler()
    handler.add("DELETE", "/webhdfs/v1/foo/dir?op=DELETE", 200, {}, '{"boolean":true}')
    with webserver.install_http_handler(handler):
        ret = gdal.Rmdir(webhdfs_base_connection + "/foo/dir")
    assert ret == 0

    # Error
    handler = webserver.SequentialHandler()
    handler.add("DELETE", "/webhdfs/v1/foo/dir_error?op=DELETE", 404)
    with webserver.install_http_handler(handler):
        ret = gdal.Rmdir(webhdfs_base_connection + "/foo/dir_error")
    assert ret != 0


###############################################################################
# Nominal cases (require valid credentials)


def test_vsiwebhdfs_extra_1():

    webhdfs_url = gdal.GetConfigOption("WEBHDFS_URL")
    if webhdfs_url is None:
        pytest.skip("Missing WEBHDFS_URL")

    if webhdfs_url.endswith("/webhdfs/v1") or webhdfs_url.endswith("/webhdfs/v1/"):
        path = "/vsiwebhdfs/" + webhdfs_url
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

        unique_id = "vsiwebhdfs_test"
        subpath = path + "/" + unique_id
        ret = gdal.Mkdir(subpath, 0)
        assert ret >= 0, "Mkdir(%s) should not return an error" % subpath

        readdir = gdal.ReadDir(path)
        assert unique_id in readdir, "ReadDir(%s) should contain %s" % (path, unique_id)

        # ret = gdal.Mkdir(subpath, 0)
        # if ret == 0:
        #    gdaltest.post_reason('fail')
        #    print('Mkdir(%s) repeated should return an error' % subpath)
        #    return 'fail'

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

    f = open_for_read("/vsiwebhdfs/" + webhdfs_url)
    assert f is not None
    ret = gdal.VSIFReadL(1, 1, f)
    gdal.VSIFCloseL(f)

    assert len(ret) == 1
