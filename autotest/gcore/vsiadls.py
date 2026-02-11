#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test /vsiadls
# Author:   Even Rouault <even dot rouault at spatialys dot com>
#
###############################################################################
# Copyright (c) 2020 Even Rouault <even dot rouault at spatialys dot com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import sys

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


###############################################################################
@pytest.fixture(autouse=True, scope="module")
def startup_and_cleanup():

    with gdaltest.config_options(
        {
            "AZURE_STORAGE_CONNECTION_STRING": None,
            "AZURE_STORAGE_ACCOUNT": None,
            "AZURE_STORAGE_ACCESS_KEY": None,
            "AZURE_STORAGE_SAS_TOKEN": None,
            "AZURE_NO_SIGN_REQUEST": None,
            "AZURE_CONFIG_DIR": "",
            "AZURE_STORAGE_ACCESS_TOKEN": "",
            "AZURE_FEDERATED_TOKEN_FILE": "",
            "CPL_AZURE_VM_API_ROOT_URL": "disabled",
        }
    ):
        assert gdal.GetSignedURL("/vsiadls/foo/bar") is None

    gdaltest.webserver_process = None
    gdaltest.webserver_port = 0

    gdaltest.webserver_process, gdaltest.webserver_port = webserver.launch(
        handler=webserver.DispatcherHttpHandler
    )
    if gdaltest.webserver_port == 0:
        pytest.skip()

    with gdal.config_options(
        {
            "AZURE_CONFIG_DIR": "",
            "AZURE_NO_SIGN_REQUEST": None,
            "AZURE_STORAGE_ACCOUNT": None,
            "AZURE_STORAGE_ACCESS_KEY": None,
            "AZURE_STORAGE_CONNECTION_STRING": "DefaultEndpointsProtocol=http;AccountName=myaccount;AccountKey=MY_ACCOUNT_KEY;BlobEndpoint=http://127.0.0.1:%d/azure/blob/myaccount"
            % gdaltest.webserver_port,
            "AZURE_STORAGE_SAS_TOKEN": None,
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
# Basic authentication tests


def test_vsiadls_fake_basic():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    signed_url = gdal.GetSignedURL(
        "/vsiadls/az_fake_bucket/resource", ["START_DATE=20180213T123456"]
    )
    assert (
        "/azure/blob/myaccount/az_fake_bucket/resource?se=2018-02-13T13%3A34%3A56Z&sig=j0cUaaHtf2SW2usSsiN79DYx%2Fo1vWwq4lLYZSC5%2Bv7I%3D&sp=r&spr=https&sr=b&st=2018-02-13T12%3A34%3A56Z&sv=2020-12-06"
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
        f = open_for_read("/vsiadls/az_fake_bucket/resource")
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
        stat_res = gdal.VSIStatL("/vsiadls/az_fake_bucket/resource2.bin")
        if stat_res is None or stat_res.size != 1000000:
            if stat_res is not None:
                print(stat_res.size)
            else:
                print(stat_res)
            pytest.fail()


###############################################################################
# Test ReadDir()


def test_vsiadls_fake_readdir():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/azure/blob/myaccount/az_fake_bucket2?directory=a_dir%20with_space&maxresults=1000&recursive=false&resource=filesystem",
        200,
        {
            "Content-type": "application/json;charset=utf-8",
            "x-ms-continuation": "contmarker",
        },
        """
                {"paths":[{"name":"a_dir with_space/resource3 with_space.bin","contentLength":"123456","lastModified": "Mon, 01 Jan 1970 00:00:01"}]}
                """,
    )
    handler.add(
        "GET",
        "/azure/blob/myaccount/az_fake_bucket2?continuation=contmarker&directory=a_dir%20with_space&maxresults=1000&recursive=false&resource=filesystem",
        200,
        {"Content-type": "application/json;charset=utf-8"},
        """
                {"paths":[{"name":"a_dir with_space/resource4.bin","contentLength":"456789","lastModified": "16 Oct 2016 12:34:56"},
                          {"name":"a_dir with_space/subdir","isDirectory":"true"}]}
                """,
    )

    with webserver.install_http_handler(handler):
        f = open_for_read(
            "/vsiadls/az_fake_bucket2/a_dir with_space/resource3 with_space.bin"
        )
    if f is None:
        pytest.fail()
    gdal.VSIFCloseL(f)

    dir_contents = gdal.ReadDir("/vsiadls/az_fake_bucket2/a_dir with_space")
    assert dir_contents == ["resource3 with_space.bin", "resource4.bin", "subdir"]
    assert (
        gdal.VSIStatL(
            "/vsiadls/az_fake_bucket2/a_dir with_space/resource3 with_space.bin"
        ).size
        == 123456
    )
    assert (
        gdal.VSIStatL(
            "/vsiadls/az_fake_bucket2/a_dir with_space/resource3 with_space.bin"
        ).mtime
        == 1
    )

    # ReadDir on something known to be a file shouldn't cause network access
    dir_contents = gdal.ReadDir(
        "/vsiadls/az_fake_bucket2/a_dir with_space/resource3 with_space.bin"
    )
    assert dir_contents is None

    # Test error on ReadDir()
    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/azure/blob/myaccount/az_fake_bucket2?directory=error_test&recursive=false&resource=filesystem",
        500,
    )
    with webserver.install_http_handler(handler):
        dir_contents = gdal.ReadDir("/vsiadls/az_fake_bucket2/error_test/")
    assert dir_contents is None

    # List containers (empty result)
    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/azure/blob/myaccount/?resource=account",
        200,
        {"Content-type": "application/json"},
        """{ "filesystems": [] }""",
    )
    with webserver.install_http_handler(handler):
        dir_contents = gdal.ReadDir("/vsiadls/")
    assert dir_contents == ["."]

    gdal.VSICurlClearCache()

    # List containers
    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/azure/blob/myaccount/?resource=account",
        200,
        {"Content-type": "application/json", "x-ms-continuation": "contmarker"},
        """{ "filesystems": [{ "name": "mycontainer1"}] }""",
    )
    handler.add(
        "GET",
        "/azure/blob/myaccount/?continuation=contmarker&resource=account",
        200,
        {"Content-type": "application/json"},
        """{ "filesystems": [{ "name": "mycontainer2"}] }""",
    )
    with webserver.install_http_handler(handler):
        dir_contents = gdal.ReadDir("/vsiadls/")
    assert dir_contents == ["mycontainer1", "mycontainer2"]


###############################################################################
# Test OpenDir() with a fake server


def test_vsiadls_opendir():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    # Unlimited depth from root
    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/azure/blob/myaccount/?resource=account",
        200,
        {"Content-type": "application/json", "x-ms-continuation": "contmarker_root"},
        """{ "filesystems": [{ "name": "fs1"}, { "name": "fs2"} ]}""",
    )
    with webserver.install_http_handler(handler):
        d = gdal.OpenDir("/vsiadls/")
    assert d is not None

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/azure/blob/myaccount/fs1?recursive=true&resource=filesystem",
        200,
        {
            "Content-type": "application/json;charset=utf-8",
            "x-ms-continuation": "contmarker_within_fs",
        },
        """
                {"paths":[{"name":"foo.txt","contentLength":"123456","lastModified": "Mon, 01 Jan 1970 00:00:01"}]}
                """,
    )
    with webserver.install_http_handler(handler):
        entry = gdal.GetNextDirEntry(d)
    assert entry.name == "fs1"
    assert entry.mode == 16384

    handler = webserver.SequentialHandler()
    with webserver.install_http_handler(handler):
        entry = gdal.GetNextDirEntry(d)
    assert entry.name == "fs1/foo.txt"

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/azure/blob/myaccount/fs1?continuation=contmarker_within_fs&recursive=true&resource=filesystem",
        200,
        {"Content-type": "application/json;charset=utf-8", "x-ms-continuation": ""},
        """
                {"paths":[{"name":"bar.txt","contentLength":"123456","lastModified": "Mon, 01 Jan 1970 00:00:01"}]}
                """,
    )
    with webserver.install_http_handler(handler):
        entry = gdal.GetNextDirEntry(d)
    assert entry.name == "fs1/bar.txt"

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/azure/blob/myaccount/fs2?recursive=true&resource=filesystem",
        200,
        {"Content-type": "application/json;charset=utf-8", "x-ms-continuation": ""},
        """
                {"paths":[]}
                """,
    )
    with webserver.install_http_handler(handler):
        entry = gdal.GetNextDirEntry(d)
    assert entry.name == "fs2"

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/azure/blob/myaccount/?continuation=contmarker_root&resource=account",
        200,
        {"Content-type": "application/json"},
        """{ "filesystems": [{ "name": "fs3"}] }""",
    )
    handler.add(
        "GET",
        "/azure/blob/myaccount/fs3?recursive=true&resource=filesystem",
        200,
        {"Content-type": "application/json;charset=utf-8", "x-ms-continuation": ""},
        """
                {"paths":[{"name":"baz.txt","contentLength":"123456","lastModified": "Mon, 01 Jan 1970 00:00:01"}]}
                """,
    )
    with webserver.install_http_handler(handler):
        entry = gdal.GetNextDirEntry(d)
    assert entry.name == "fs3"

    handler = webserver.SequentialHandler()
    with webserver.install_http_handler(handler):
        entry = gdal.GetNextDirEntry(d)
    assert entry.name == "fs3/baz.txt"

    entry = gdal.GetNextDirEntry(d)
    assert entry is None

    gdal.CloseDir(d)

    # Prefix filtering on subdir
    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/azure/blob/myaccount/fs1?directory=sub_dir&recursive=true&resource=filesystem",
        200,
        {"Content-type": "application/json;charset=utf-8"},
        """
                {"paths":[{"name":"sub_dir/foo.txt","contentLength":"123456","lastModified": "Mon, 01 Jan 1970 00:00:01"},
                          {"name":"sub_dir/my_prefix_test.txt","contentLength":"40","lastModified": "Mon, 01 Jan 1970 00:00:01"}]}
                """,
    )
    with webserver.install_http_handler(handler):
        d = gdal.OpenDir("/vsiadls/fs1/sub_dir", -1, ["PREFIX=my_prefix"])
    assert d is not None

    handler = webserver.SequentialHandler()
    with webserver.install_http_handler(handler):
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
        "/vsiadls/fs1/sub_dir/my_prefix_test.txt",
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
            "/vsiadls/fs1/sub_dir/i_do_not_exist.txt",
            gdal.VSI_STAT_EXISTS_FLAG
            | gdal.VSI_STAT_NATURE_FLAG
            | gdal.VSI_STAT_SIZE_FLAG
            | gdal.VSI_STAT_CACHE_ONLY,
        )
        is None
    )


###############################################################################
# Test write


def test_vsiadls_fake_write():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    # Error case in initial PUT
    handler = webserver.SequentialHandler()
    handler.add("PUT", "/azure/blob/myaccount/test_copy/file.bin?resource=file", 400)
    with webserver.install_http_handler(handler):
        f = gdal.VSIFOpenL("/vsiadls/test_copy/file.bin", "wb")
        assert f is None

    # Empty file
    handler = webserver.SequentialHandler()
    handler.add("PUT", "/azure/blob/myaccount/test_copy/file.bin?resource=file", 201)
    handler.add(
        "PATCH",
        "/azure/blob/myaccount/test_copy/file.bin?action=flush&close=true&position=0",
        200,
    )
    with webserver.install_http_handler(handler):
        f = gdal.VSIFOpenL("/vsiadls/test_copy/file.bin", "wb")
        assert f is not None
        assert gdal.VSIFCloseL(f) == 0

    # Small file
    handler = webserver.SequentialHandler()
    handler.add(
        "PUT",
        "/azure/blob/myaccount/test_copy/file.bin?resource=file",
        201,
        expected_headers={"x-ms-client-request-id": "REQUEST_ID"},
    )
    handler.add(
        "PATCH",
        "/azure/blob/myaccount/test_copy/file.bin?action=append&position=0",
        202,
        expected_body=b"foo",
    )
    handler.add(
        "PATCH",
        "/azure/blob/myaccount/test_copy/file.bin?action=flush&close=true&position=3",
        200,
    )
    with webserver.install_http_handler(handler):
        f = gdal.VSIFOpenExL(
            "/vsiadls/test_copy/file.bin",
            "wb",
            0,
            ["x-ms-client-request-id=REQUEST_ID"],
        )
        assert f is not None
        assert gdal.VSIFWriteL("foo", 1, 3, f) == 3
        assert gdal.VSIFCloseL(f) == 0

    # Error case in PATCH append
    handler = webserver.SequentialHandler()
    handler.add("PUT", "/azure/blob/myaccount/test_copy/file.bin?resource=file", 201)
    handler.add(
        "PATCH",
        "/azure/blob/myaccount/test_copy/file.bin?action=append&position=0",
        400,
        expected_body=b"foo",
    )
    with webserver.install_http_handler(handler):
        f = gdal.VSIFOpenL("/vsiadls/test_copy/file.bin", "wb")
        assert f is not None
        assert gdal.VSIFWriteL("foo", 1, 3, f) == 3
        assert gdal.VSIFCloseL(f) != 0

    # Error case in PATCH close
    handler = webserver.SequentialHandler()
    handler.add("PUT", "/azure/blob/myaccount/test_copy/file.bin?resource=file", 201)
    handler.add(
        "PATCH",
        "/azure/blob/myaccount/test_copy/file.bin?action=append&position=0",
        202,
        expected_body=b"foo",
    )
    handler.add(
        "PATCH",
        "/azure/blob/myaccount/test_copy/file.bin?action=flush&close=true&position=3",
        400,
    )
    with webserver.install_http_handler(handler):
        f = gdal.VSIFOpenL("/vsiadls/test_copy/file.bin", "wb")
        assert f is not None
        assert gdal.VSIFWriteL("foo", 1, 3, f) == 3
        assert gdal.VSIFCloseL(f) != 0

    # Chunked output
    with gdaltest.config_option("VSIAZ_CHUNK_SIZE_BYTES", "10"):
        handler = webserver.SequentialHandler()
        handler.add(
            "PUT", "/azure/blob/myaccount/test_copy/file.bin?resource=file", 201
        )
        handler.add(
            "PATCH",
            "/azure/blob/myaccount/test_copy/file.bin?action=append&position=0",
            202,
            expected_body=b"0123456789",
        )
        handler.add(
            "PATCH",
            "/azure/blob/myaccount/test_copy/file.bin?action=append&position=10",
            202,
            expected_body=b"abcd",
        )
        handler.add(
            "PATCH",
            "/azure/blob/myaccount/test_copy/file.bin?action=flush&close=true&position=14",
            200,
        )
        with webserver.install_http_handler(handler):
            f = gdal.VSIFOpenL("/vsiadls/test_copy/file.bin", "wb")
            assert f is not None
            assert gdal.VSIFWriteL("0123456789abcd", 1, 14, f) == 14
            assert gdal.VSIFCloseL(f) == 0

    # Chunked output with last chunk being the chunk size
    with gdaltest.config_option("VSIAZ_CHUNK_SIZE_BYTES", "5"):
        handler = webserver.SequentialHandler()
        handler.add(
            "PUT", "/azure/blob/myaccount/test_copy/file.bin?resource=file", 201
        )
        handler.add(
            "PATCH",
            "/azure/blob/myaccount/test_copy/file.bin?action=append&position=0",
            202,
            expected_body=b"01234",
        )
        handler.add(
            "PATCH",
            "/azure/blob/myaccount/test_copy/file.bin?action=append&position=5",
            202,
            expected_body=b"56789",
        )
        handler.add(
            "PATCH",
            "/azure/blob/myaccount/test_copy/file.bin?action=flush&close=true&position=10",
            200,
        )
        with webserver.install_http_handler(handler):
            f = gdal.VSIFOpenL("/vsiadls/test_copy/file.bin", "wb")
            assert f is not None
            assert gdal.VSIFWriteL("0123456789", 1, 10, f) == 10
            assert gdal.VSIFCloseL(f) == 0


###############################################################################
# Test Unlink()


def test_vsiadls_fake_unlink():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

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
        200,
        {"Connection": "close"},
    )
    with webserver.install_http_handler(handler):
        ret = gdal.Unlink("/vsiadls/az_bucket_test_unlink/myfile")
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
            ret = gdal.Unlink("/vsiadls/az_bucket_test_unlink/myfile")
    assert ret == -1


###############################################################################
# Test Mkdir() / Rmdir() / RmdirRecursive()


def test_vsiadls_fake_mkdir_rmdir():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    # Invalid name
    ret = gdal.Mkdir("/vsiadls", 0)
    assert ret != 0

    handler = webserver.SequentialHandler()
    handler.add(
        "HEAD",
        "/azure/blob/myaccount/az_bucket_test_mkdir/dir",
        404,
        {"Connection": "close"},
    )
    handler.add(
        "PUT", "/azure/blob/myaccount/az_bucket_test_mkdir/dir?resource=directory", 201
    )
    with webserver.install_http_handler(handler):
        ret = gdal.Mkdir("/vsiadls/az_bucket_test_mkdir/dir", 0)
    assert ret == 0

    # Try creating already existing directory
    handler = webserver.SequentialHandler()
    handler.add(
        "HEAD",
        "/azure/blob/myaccount/az_bucket_test_mkdir/dir",
        200,
        {"x-ms-permissions": "rwxrwxrwx", "x-ms-resource-type": "directory"},
    )
    with webserver.install_http_handler(handler):
        ret = gdal.Mkdir("/vsiadls/az_bucket_test_mkdir/dir", 0)
    assert ret != 0

    # Invalid name
    ret = gdal.Rmdir("/vsiadls")
    assert ret != 0

    # Not a directory
    handler = webserver.SequentialHandler()
    handler.add(
        "HEAD",
        "/azure/blob/myaccount/az_bucket_test_mkdir/it_is_a_file",
        200,
        {"x-ms-permissions": "rwxrwxrwx", "x-ms-resource-type": "file"},
    )
    with webserver.install_http_handler(handler):
        ret = gdal.Rmdir("/vsiadls/az_bucket_test_mkdir/it_is_a_file")
    assert ret != 0

    # Valid
    handler = webserver.SequentialHandler()
    handler.add(
        "DELETE", "/azure/blob/myaccount/az_bucket_test_mkdir/dir?recursive=false", 200
    )
    with webserver.install_http_handler(handler):
        ret = gdal.Rmdir("/vsiadls/az_bucket_test_mkdir/dir")
    assert ret == 0

    # Try deleting already deleted directory
    handler = webserver.SequentialHandler()
    handler.add("HEAD", "/azure/blob/myaccount/az_bucket_test_mkdir/dir", 404)
    with webserver.install_http_handler(handler):
        ret = gdal.Rmdir("/vsiadls/az_bucket_test_mkdir/dir")
    assert ret != 0

    # RmdirRecursive
    handler = webserver.SequentialHandler()
    handler.add(
        "HEAD",
        "/azure/blob/myaccount/az_bucket_test_mkdir/dir_rec",
        200,
        {"x-ms-permissions": "rwxrwxrwx", "x-ms-resource-type": "directory"},
    )
    handler.add(
        "DELETE",
        "/azure/blob/myaccount/az_bucket_test_mkdir/dir_rec?recursive=true",
        200,
    )
    with webserver.install_http_handler(handler):
        ret = gdal.RmdirRecursive("/vsiadls/az_bucket_test_mkdir/dir_rec")
    assert ret == 0


###############################################################################
# Test rename


def test_vsiadls_fake_rename():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()
    handler = webserver.SequentialHandler()
    handler.add(
        "HEAD",
        "/azure/blob/myaccount/test/source.txt",
        200,
        {
            "Content-Length": "3",
            "x-ms-permissions": "rwxrwxrwx",
            "x-ms-resource-type": "file",
        },
    )
    handler.add(
        "PUT",
        "/azure/blob/myaccount/test/target.txt",
        201,
        expected_headers={"x-ms-rename-source": "/test/source.txt"},
    )
    handler.add("HEAD", "/azure/blob/myaccount/test/source.txt", 404)
    with webserver.install_http_handler(handler):
        assert gdal.Rename("/vsiadls/test/source.txt", "/vsiadls/test/target.txt") == 0
        assert gdal.VSIStatL("/vsiadls/test/source.txt") is None


###############################################################################
# Test Sync() with source and target on /vsiadls/
# Note: this is using Azure blob object copying since ADLS has no equivalent


def test_vsiadls_fake_sync_copyobject():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add(
        "HEAD",
        "/azure/blob/myaccount/test_bucket/src.txt",
        200,
        {
            "Content-Length": "3",
            "x-ms-permissions": "rwxrwxrwx",
            "x-ms-resource-type": "file",
            "Last-Modified": "Mon, 04 Sep 2023 22:00:00 GMT",
        },
    )
    handler.add("HEAD", "/azure/blob/myaccount/test_bucket/dst.txt", 404)
    handler.add(
        "PUT",
        "/azure/blob/myaccount/test_bucket/dst.txt",
        202,
        expected_headers={
            "x-ms-copy-source": "http://127.0.0.1:%d/azure/blob/myaccount/test_bucket/src.txt"
            % gdaltest.webserver_port
        },
    )
    with webserver.install_http_handler(handler):
        assert gdal.Sync("/vsiadls/test_bucket/src.txt", "/vsiadls/test_bucket/dst.txt")


###############################################################################
# Test sync again, but this is a no-op because of the date time


def test_vsiadls_fake_sync_no_op_because_of_data_time():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add(
        "HEAD",
        "/azure/blob/myaccount/test_bucket/src.txt",
        200,
        {
            "Content-Length": "3",
            "x-ms-permissions": "rwxrwxrwx",
            "x-ms-resource-type": "file",
            "Last-Modified": "Mon, 04 Sep 2023 22:00:00 GMT",
        },
    )
    handler.add(
        "HEAD",
        "/azure/blob/myaccount/test_bucket/dst.txt",
        200,
        {
            "Content-Length": "3",
            "x-ms-permissions": "rwxrwxrwx",
            "x-ms-resource-type": "file",
            "Last-Modified": "Mon, 04 Sep 2023 23:00:00 GMT",
        },
    )
    with webserver.install_http_handler(handler):
        assert gdal.Sync("/vsiadls/test_bucket/src.txt", "/vsiadls/test_bucket/dst.txt")


###############################################################################
# Test sync again, but overwrite because of the date time


def test_vsiadls_fake_sync_overwite_because_of_data_time():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add(
        "HEAD",
        "/azure/blob/myaccount/test_bucket/src.txt",
        200,
        {
            "Content-Length": "3",
            "x-ms-permissions": "rwxrwxrwx",
            "x-ms-resource-type": "file",
            "Last-Modified": "Mon, 04 Sep 2023 22:00:00 GMT",
        },
    )
    handler.add(
        "HEAD",
        "/azure/blob/myaccount/test_bucket/dst.txt",
        200,
        {
            "Content-Length": "3",
            "x-ms-permissions": "rwxrwxrwx",
            "x-ms-resource-type": "file",
            "Last-Modified": "Mon, 04 Sep 1980 23:00:00 GMT",
        },
    )
    handler.add(
        "PUT",
        "/azure/blob/myaccount/test_bucket/dst.txt",
        202,
        expected_headers={
            "x-ms-copy-source": "http://127.0.0.1:%d/azure/blob/myaccount/test_bucket/src.txt"
            % gdaltest.webserver_port
        },
    )
    with webserver.install_http_handler(handler):
        assert gdal.Sync("/vsiadls/test_bucket/src.txt", "/vsiadls/test_bucket/dst.txt")


###############################################################################
# Test sync again on directories, but this is a no-op because of the date time


def test_vsiadls_fake_sync_on_dir_no_op_because_of_data_time():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/azure/blob/myaccount/test_bucket?recursive=true&resource=filesystem",
        200,
        {
            "Content-type": "application/json;charset=utf-8",
            "x-ms-continuation": "",
        },
        """
                {"paths":[{"name":"test.bin","contentLength":"3","lastModified": "Mon, 01 Jan 2010 00:00:01"}]}
                """,
    )
    handler.add("HEAD", "/azure/blob/myaccount/test_bucket?resource=filesystem", 200)
    handler.add(
        "GET",
        "/azure/blob/myaccount/test_bucket2?recursive=true&resource=filesystem",
        200,
        {
            "Content-type": "application/json;charset=utf-8",
            "x-ms-continuation": "",
        },
        """
                {"paths":[{"name":"test.bin","contentLength":"3","lastModified": "Mon, 01 Jan 2020 00:00:01"}]}
                """,
    )

    with webserver.install_http_handler(handler):
        assert gdal.Sync("/vsiadls/test_bucket/", "/vsiadls/test_bucket2/")


###############################################################################
# Test sync again on directories, but overwrite because of the date time


def test_vsiadls_fake_sync_on_dir_overwrite_because_of_data_time():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/azure/blob/myaccount/test_bucket?recursive=true&resource=filesystem",
        200,
        {
            "Content-type": "application/json;charset=utf-8",
            "x-ms-continuation": "",
        },
        """
                {"paths":[{"name":"test.bin","contentLength":"3","lastModified": "Mon, 01 Jan 2010 00:00:01"}]}
                """,
    )
    handler.add("HEAD", "/azure/blob/myaccount/test_bucket?resource=filesystem", 200)
    handler.add(
        "GET",
        "/azure/blob/myaccount/test_bucket2?recursive=true&resource=filesystem",
        200,
        {
            "Content-type": "application/json;charset=utf-8",
            "x-ms-continuation": "",
        },
        """
                {"paths":[{"name":"test.bin","contentLength":"3","lastModified": "Mon, 01 Jan 2000 00:00:01"}]}
                """,
    )
    handler.add(
        "PUT",
        "/azure/blob/myaccount/test_bucket2/test.bin",
        202,
        expected_headers={
            "x-ms-copy-source": "http://127.0.0.1:%d/azure/blob/myaccount/test_bucket/test.bin"
            % gdaltest.webserver_port
        },
    )
    with webserver.install_http_handler(handler):
        assert gdal.Sync("/vsiadls/test_bucket/", "/vsiadls/test_bucket2/")


###############################################################################
# Test Sync() error


def test_vsiadls_fake_sync_error_case():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add(
        "HEAD",
        "/azure/blob/myaccount/test_bucket/src.txt",
        200,
        {
            "Content-Length": "3",
            "x-ms-permissions": "rwxrwxrwx",
            "x-ms-resource-type": "file",
        },
    )
    handler.add("HEAD", "/azure/blob/myaccount/test_bucket/dst.txt", 404)
    handler.add("PUT", "/azure/blob/myaccount/test_bucket/dst.txt", 400)
    with webserver.install_http_handler(handler):
        with gdal.quiet_errors():
            assert not gdal.Sync(
                "/vsiadls/test_bucket/src.txt", "/vsiadls/test_bucket/dst.txt"
            )


###############################################################################
# Test Sync() and multithreaded download of a single file


def test_vsiadls_fake_sync_multithreaded_upload_single_file(tmp_vsimem):

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    gdal.Mkdir(tmp_vsimem / "test", 0)
    gdal.FileFromMemBuffer(tmp_vsimem / "test/foo", "foo\n")

    handler = webserver.SequentialHandler()
    handler.add("HEAD", "/azure/blob/myaccount/test_bucket?resource=filesystem", 200)
    handler.add("HEAD", "/azure/blob/myaccount/test_bucket/foo", 404)
    handler.add("PUT", "/azure/blob/myaccount/test_bucket/foo?resource=file", 201)
    handler.add(
        "PATCH",
        "/azure/blob/myaccount/test_bucket/foo?action=append&position=0",
        202,
        expected_body=b"foo",
    )
    handler.add(
        "PATCH",
        "/azure/blob/myaccount/test_bucket/foo?action=append&position=3",
        202,
        expected_body=b"\n",
    )
    handler.add(
        "PATCH",
        "/azure/blob/myaccount/test_bucket/foo?action=flush&close=true&position=4",
        200,
    )

    with gdaltest.config_option("VSIS3_SIMULATE_THREADING", "YES"):
        with webserver.install_http_handler(handler):
            assert gdal.Sync(
                tmp_vsimem / "test/foo",
                "/vsiadls/test_bucket",
                options=["NUM_THREADS=1", "CHUNK_SIZE=3"],
            )


###############################################################################
# Test GetFileMetadata () / SetFileMetadata()


def test_vsiadls_fake_metadata():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add(
        "HEAD",
        "/azure/blob/myaccount/test/foo.bin",
        200,
        {
            "Content-Length": "3",
            "x-ms-permissions": "rwxrwxrwx",
            "x-ms-resource-type": "file",
        },
    )
    with webserver.install_http_handler(handler):
        md = gdal.GetFileMetadata("/vsiadls/test/foo.bin", "HEADERS")
        assert "x-ms-permissions" in md

    handler = webserver.SequentialHandler()
    handler.add(
        "HEAD",
        "/azure/blob/myaccount/test/foo.bin?action=getStatus",
        200,
        {"foo": "bar"},
    )
    with webserver.install_http_handler(handler):
        md = gdal.GetFileMetadata("/vsiadls/test/foo.bin", "STATUS")
        assert "foo" in md

    handler = webserver.SequentialHandler()
    handler.add(
        "HEAD",
        "/azure/blob/myaccount/test/foo.bin?action=getAccessControl",
        200,
        {"x-ms-acl": "some_acl"},
    )
    with webserver.install_http_handler(handler):
        md = gdal.GetFileMetadata("/vsiadls/test/foo.bin", "ACL")
        assert "x-ms-acl" in md

    # Error case
    handler = webserver.SequentialHandler()
    handler.add("HEAD", "/azure/blob/myaccount/test/foo.bin?action=getStatus", 404)
    with webserver.install_http_handler(handler):
        assert gdal.GetFileMetadata("/vsiadls/test/foo.bin", "STATUS") == {}

    # SetMetadata()
    handler = webserver.SequentialHandler()
    handler.add(
        "PATCH",
        "/azure/blob/myaccount/test/foo.bin?action=setProperties",
        200,
        expected_headers={"x-ms-properties": "foo=bar"},
    )
    with webserver.install_http_handler(handler):
        assert gdal.SetFileMetadata(
            "/vsiadls/test/foo.bin", {"x-ms-properties": "foo=bar"}, "PROPERTIES"
        )

    handler = webserver.SequentialHandler()
    handler.add(
        "PATCH",
        "/azure/blob/myaccount/test/foo.bin?action=setAccessControl",
        200,
        expected_headers={"x-ms-acl": "foo"},
    )
    with webserver.install_http_handler(handler):
        assert gdal.SetFileMetadata("/vsiadls/test/foo.bin", {"x-ms-acl": "foo"}, "ACL")

    handler = webserver.SequentialHandler()
    handler.add(
        "PATCH",
        "/azure/blob/myaccount/test/foo.bin?action=setAccessControlRecursive&mode=set",
        200,
        expected_headers={"x-ms-acl": "foo"},
    )
    with webserver.install_http_handler(handler):
        assert gdal.SetFileMetadata(
            "/vsiadls/test/foo.bin",
            {"x-ms-acl": "foo"},
            "ACL",
            ["RECURSIVE=TRUE", "MODE=set"],
        )

    # Error case
    handler = webserver.SequentialHandler()
    handler.add("PATCH", "/azure/blob/myaccount/test/foo.bin?action=setProperties", 404)
    with webserver.install_http_handler(handler):
        assert not gdal.SetFileMetadata(
            "/vsiadls/test/foo.bin", {"x-ms-properties": "foo=bar"}, "PROPERTIES"
        )


###############################################################################
# Test VSIMultipartUploadXXXX()


def test_vsiadls_MultipartUpload():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    # Test MultipartUploadGetCapabilities()
    info = gdal.MultipartUploadGetCapabilities("/vsiadls/")
    assert info.non_sequential_upload_supported
    assert info.parallel_upload_supported
    assert not info.abort_supported
    assert info.min_part_size == 0
    assert info.max_part_size >= 1024
    assert info.max_part_count > 0

    # Test MultipartUploadAddPart()
    handler = webserver.SequentialHandler()
    handler.add(
        "PATCH",
        "/azure/blob/myaccount/test_multipartupload/test.bin?action=append&position=123456",
        202,
        expected_body=b"foo",
    )
    with webserver.install_http_handler(handler):
        part_id = gdal.MultipartUploadAddPart(
            "/vsiadls/test_multipartupload/test.bin", "my_upload_id", 1, 123456, b"foo"
        )
    assert part_id == "dummy"

    # Test MultipartUploadEnd()
    handler = webserver.SequentialHandler()
    handler.add(
        "PATCH",
        "/azure/blob/myaccount/test_multipartupload/test.bin?action=flush&close=true&position=3",
        200,
    )
    with webserver.install_http_handler(handler):
        assert gdal.MultipartUploadEnd(
            "/vsiadls/test_multipartupload/test.bin", "my_upload_id", ["dummy"], 3
        )

    # Test unsupported MultipartUploadAbort()
    with gdal.ExceptionMgr(useExceptions=True):
        with pytest.raises(
            Exception,
            match=r"MultipartUploadAbort\(\) not supported by this file system",
        ):
            gdal.MultipartUploadAbort("/vsiadls/foo/bar", "upload_id")
