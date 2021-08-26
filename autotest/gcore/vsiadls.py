#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test /vsiadls
# Author:   Even Rouault <even dot rouault at spatialys dot com>
#
###############################################################################
# Copyright (c) 2020 Even Rouault <even dot rouault at spatialys dot com>
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
from osgeo import gdal


import gdaltest
import webserver
import pytest

pytestmark = pytest.mark.skipif(not gdaltest.built_against_curl(), reason="GDAL not built against curl")


def open_for_read(uri):
    """
    Opens a test file for reading.
    """
    return gdal.VSIFOpenExL(uri, 'rb', 1)


###############################################################################
@pytest.fixture(autouse=True, scope='module')
def startup_and_cleanup():

    # Unset all env vars that could influence the tests
    az_vars = {}
    for var in ('AZURE_STORAGE_CONNECTION_STRING', 'AZURE_STORAGE_ACCOUNT',
                'AZURE_STORAGE_ACCESS_KEY', 'AZURE_SAS', 'AZURE_NO_SIGN_REQUEST'):
        az_vars[var] = gdal.GetConfigOption(var)
        if az_vars[var] is not None:
            gdal.SetConfigOption(var, "")

    assert gdal.GetSignedURL('/vsiadls/foo/bar') is None

    gdaltest.webserver_process = None
    gdaltest.webserver_port = 0

    if not gdaltest.built_against_curl():
        pytest.skip()

    (gdaltest.webserver_process, gdaltest.webserver_port) = webserver.launch(handler=webserver.DispatcherHttpHandler)
    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.SetConfigOption('AZURE_STORAGE_CONNECTION_STRING',
                         'DefaultEndpointsProtocol=http;AccountName=myaccount;AccountKey=MY_ACCOUNT_KEY;EndpointSuffix=127.0.0.1:%d' % gdaltest.webserver_port)
    gdal.SetConfigOption('AZURE_STORAGE_ACCOUNT', '')
    gdal.SetConfigOption('AZURE_STORAGE_ACCESS_KEY', '')
    gdal.SetConfigOption('CPL_AZURE_TIMESTAMP', 'my_timestamp')

    yield

    # Clearcache needed to close all connections, since the Python server
    # can only handle one connection at a time
    gdal.VSICurlClearCache()

    webserver.server_stop(gdaltest.webserver_process, gdaltest.webserver_port)

    for var in az_vars:
        gdal.SetConfigOption(var, az_vars[var])


###############################################################################
# Basic authentication tests

def test_vsiadls_fake_basic():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    signed_url = gdal.GetSignedURL('/vsiadls/az_fake_bucket/resource', ['START_DATE=20180213T123456'])
    assert (signed_url in ('http://127.0.0.1:8080/azure/blob/myaccount/az_fake_bucket/resource?se=2018-02-13T13%3A34%3A56Z&sig=9Jc4yBFlSRZSSxf059OohN6pYRrjuHWJWSEuryczN%2FM%3D&sp=r&sr=c&st=2018-02-13T12%3A34%3A56Z&sv=2012-02-12',
                          'http://127.0.0.1:8081/azure/blob/myaccount/az_fake_bucket/resource?se=2018-02-13T13%3A34%3A56Z&sig=9Jc4yBFlSRZSSxf059OohN6pYRrjuHWJWSEuryczN%2FM%3D&sp=r&sr=c&st=2018-02-13T12%3A34%3A56Z&sv=2012-02-12'))

    def method(request):

        request.protocol_version = 'HTTP/1.1'
        h = request.headers
        if 'Authorization' not in h or \
           h['Authorization'] != 'SharedKey myaccount:C0sSaBzGbvadfuuMMjQiHCXCUzsGWj3uuE+UO8dDl0U=' or \
           'x-ms-date' not in h or h['x-ms-date'] != 'my_timestamp':
            sys.stderr.write('Bad headers: %s\n' % str(h))
            request.send_response(403)
            return
        request.send_response(200)
        request.send_header('Content-type', 'text/plain')
        request.send_header('Content-Length', 3)
        request.send_header('Connection', 'close')
        request.end_headers()
        request.wfile.write("""foo""".encode('ascii'))

    handler = webserver.SequentialHandler()
    handler.add('GET', '/azure/blob/myaccount/az_fake_bucket/resource', custom_method=method)
    with webserver.install_http_handler(handler):
        f = open_for_read('/vsiadls/az_fake_bucket/resource')
        assert f is not None
        data = gdal.VSIFReadL(1, 4, f).decode('ascii')
        gdal.VSIFCloseL(f)

    assert data == 'foo'

    handler = webserver.SequentialHandler()
    handler.add('HEAD', '/azure/blob/myaccount/az_fake_bucket/resource2.bin', 200,
                {'Content-Length': '1000000'})
    with webserver.install_http_handler(handler):
        stat_res = gdal.VSIStatL('/vsiadls/az_fake_bucket/resource2.bin')
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
    handler.add('GET', '/azure/blob/myaccount/az_fake_bucket2?directory=a_dir%20with_space&recursive=false&resource=filesystem', 200,
                {'Content-type': 'application/json;charset=utf-8', 'x-ms-continuation': 'contmarker'},
                """
                {"paths":[{"name":"a_dir with_space/resource3 with_space.bin","contentLength":"123456","lastModified": "Mon, 01 Jan 1970 00:00:01"}]}
                """)
    handler.add('GET', '/azure/blob/myaccount/az_fake_bucket2?continuation=contmarker&directory=a_dir%20with_space&recursive=false&resource=filesystem', 200,
                {'Content-type': 'application/json;charset=utf-8'},
                """
                {"paths":[{"name":"a_dir with_space/resource4.bin","contentLength":"456789","lastModified": "16 Oct 2016 12:34:56"},
                          {"name":"a_dir with_space/subdir","isDirectory":"true"}]}
                """
                )

    with webserver.install_http_handler(handler):
        f = open_for_read('/vsiadls/az_fake_bucket2/a_dir with_space/resource3 with_space.bin')
    if f is None:
        pytest.fail()
    gdal.VSIFCloseL(f)

    dir_contents = gdal.ReadDir('/vsiadls/az_fake_bucket2/a_dir with_space')
    assert dir_contents == ['resource3 with_space.bin', 'resource4.bin', 'subdir']
    assert gdal.VSIStatL('/vsiadls/az_fake_bucket2/a_dir with_space/resource3 with_space.bin').size == 123456
    assert gdal.VSIStatL('/vsiadls/az_fake_bucket2/a_dir with_space/resource3 with_space.bin').mtime == 1

    # ReadDir on something known to be a file shouldn't cause network access
    dir_contents = gdal.ReadDir('/vsiadls/az_fake_bucket2/a_dir with_space/resource3 with_space.bin')
    assert dir_contents is None

    # Test error on ReadDir()
    handler = webserver.SequentialHandler()
    handler.add('GET', '/azure/blob/myaccount/az_fake_bucket2?directory=error_test&recursive=false&resource=filesystem', 500)
    with webserver.install_http_handler(handler):
        dir_contents = gdal.ReadDir('/vsiadls/az_fake_bucket2/error_test/')
    assert dir_contents is None

    # List containers (empty result)
    handler = webserver.SequentialHandler()
    handler.add('GET', '/azure/blob/myaccount/?resource=account', 200, {'Content-type': 'application/json'},
                """{ "filesystems": [] }""")
    with webserver.install_http_handler(handler):
        dir_contents = gdal.ReadDir('/vsiadls/')
    assert dir_contents == ['.']

    gdal.VSICurlClearCache()

    # List containers
    handler = webserver.SequentialHandler()
    handler.add('GET', '/azure/blob/myaccount/?resource=account', 200, {'Content-type': 'application/json', 'x-ms-continuation': 'contmarker'},
                """{ "filesystems": [{ "name": "mycontainer1"}] }""")
    handler.add('GET', '/azure/blob/myaccount/?continuation=contmarker&resource=account', 200, {'Content-type': 'application/json'},
                """{ "filesystems": [{ "name": "mycontainer2"}] }""")
    with webserver.install_http_handler(handler):
        dir_contents = gdal.ReadDir('/vsiadls/')
    assert dir_contents == ['mycontainer1', 'mycontainer2']

###############################################################################
# Test OpenDir() with a fake server


def test_vsiadls_opendir():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    # Unlimited depth from root
    handler = webserver.SequentialHandler()
    handler.add('GET', '/azure/blob/myaccount/?resource=account', 200, {'Content-type': 'application/json', 'x-ms-continuation': 'contmarker_root'},
                """{ "filesystems": [{ "name": "fs1"}, { "name": "fs2"} ]}""")
    with webserver.install_http_handler(handler):
        d = gdal.OpenDir('/vsiadls/')
    assert d is not None

    handler = webserver.SequentialHandler()
    handler.add('GET', '/azure/blob/myaccount/fs1?recursive=true&resource=filesystem', 200,
                {'Content-type': 'application/json;charset=utf-8', 'x-ms-continuation': 'contmarker_within_fs'},
                """
                {"paths":[{"name":"foo.txt","contentLength":"123456","lastModified": "Mon, 01 Jan 1970 00:00:01"}]}
                """)
    with webserver.install_http_handler(handler):
        entry = gdal.GetNextDirEntry(d)
    assert entry.name == 'fs1'
    assert entry.mode == 16384

    handler = webserver.SequentialHandler()
    with webserver.install_http_handler(handler):
        entry = gdal.GetNextDirEntry(d)
    assert entry.name == 'fs1/foo.txt'

    handler = webserver.SequentialHandler()
    handler.add('GET', '/azure/blob/myaccount/fs1?continuation=contmarker_within_fs&recursive=true&resource=filesystem', 200,
                {'Content-type': 'application/json;charset=utf-8', 'x-ms-continuation': ''},
                """
                {"paths":[{"name":"bar.txt","contentLength":"123456","lastModified": "Mon, 01 Jan 1970 00:00:01"}]}
                """)
    with webserver.install_http_handler(handler):
        entry = gdal.GetNextDirEntry(d)
    assert entry.name == 'fs1/bar.txt'

    handler = webserver.SequentialHandler()
    handler.add('GET', '/azure/blob/myaccount/fs2?recursive=true&resource=filesystem', 200,
                {'Content-type': 'application/json;charset=utf-8', 'x-ms-continuation': ''},
                """
                {"paths":[]}
                """)
    with webserver.install_http_handler(handler):
        entry = gdal.GetNextDirEntry(d)
    assert entry.name == 'fs2'

    handler = webserver.SequentialHandler()
    handler.add('GET', '/azure/blob/myaccount/?continuation=contmarker_root&resource=account', 200, {'Content-type': 'application/json'},
                """{ "filesystems": [{ "name": "fs3"}] }""")
    handler.add('GET', '/azure/blob/myaccount/fs3?recursive=true&resource=filesystem', 200,
                {'Content-type': 'application/json;charset=utf-8', 'x-ms-continuation': ''},
                """
                {"paths":[{"name":"baz.txt","contentLength":"123456","lastModified": "Mon, 01 Jan 1970 00:00:01"}]}
                """)
    with webserver.install_http_handler(handler):
        entry = gdal.GetNextDirEntry(d)
    assert entry.name == 'fs3'

    handler = webserver.SequentialHandler()
    with webserver.install_http_handler(handler):
        entry = gdal.GetNextDirEntry(d)
    assert entry.name == 'fs3/baz.txt'

    entry = gdal.GetNextDirEntry(d)
    assert entry is None

    gdal.CloseDir(d)

    # Prefix filtering on subdir
    handler = webserver.SequentialHandler()
    handler.add('GET', '/azure/blob/myaccount/fs1?directory=sub_dir&recursive=true&resource=filesystem', 200,
                {'Content-type': 'application/json;charset=utf-8'},
                """
                {"paths":[{"name":"foo.txt","contentLength":"123456","lastModified": "Mon, 01 Jan 1970 00:00:01"},
                          {"name":"my_prefix_test.txt","contentLength":"40","lastModified": "Mon, 01 Jan 1970 00:00:01"}]}
                """)
    with webserver.install_http_handler(handler):
        d = gdal.OpenDir('/vsiadls/fs1/sub_dir', -1, ['PREFIX=my_prefix'])
    assert d is not None

    handler = webserver.SequentialHandler()
    with webserver.install_http_handler(handler):
        entry = gdal.GetNextDirEntry(d)
    assert entry.name == 'my_prefix_test.txt'
    assert entry.size == 40
    assert entry.mode == 32768
    assert entry.mtime == 1

    entry = gdal.GetNextDirEntry(d)
    assert entry is None

    gdal.CloseDir(d)

###############################################################################
# Test write


def test_vsiadls_fake_write():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    # Error case in initial PUT
    handler = webserver.SequentialHandler()
    handler.add('PUT', '/azure/blob/myaccount/test_copy/file.bin?resource=file', 400)
    with webserver.install_http_handler(handler):
        f = gdal.VSIFOpenL('/vsiadls/test_copy/file.bin', 'wb')
        assert f is None

    # Empty file
    handler = webserver.SequentialHandler()
    handler.add('PUT', '/azure/blob/myaccount/test_copy/file.bin?resource=file', 201)
    handler.add('PATCH', '/azure/blob/myaccount/test_copy/file.bin?action=flush&close=true&position=0', 200)
    with webserver.install_http_handler(handler):
        f = gdal.VSIFOpenL('/vsiadls/test_copy/file.bin', 'wb')
        assert f is not None
        assert gdal.VSIFCloseL(f) == 0

    # Small file
    handler = webserver.SequentialHandler()
    handler.add('PUT', '/azure/blob/myaccount/test_copy/file.bin?resource=file', 201)
    handler.add('PATCH', '/azure/blob/myaccount/test_copy/file.bin?action=append&position=0', 202, expected_body = b'foo')
    handler.add('PATCH', '/azure/blob/myaccount/test_copy/file.bin?action=flush&close=true&position=3', 200)
    with webserver.install_http_handler(handler):
        f = gdal.VSIFOpenL('/vsiadls/test_copy/file.bin', 'wb')
        assert f is not None
        assert gdal.VSIFWriteL('foo', 1, 3, f) == 3
        assert gdal.VSIFCloseL(f) == 0

    # Error case in PATCH append
    handler = webserver.SequentialHandler()
    handler.add('PUT', '/azure/blob/myaccount/test_copy/file.bin?resource=file', 201)
    handler.add('PATCH', '/azure/blob/myaccount/test_copy/file.bin?action=append&position=0', 400, expected_body = b'foo')
    with webserver.install_http_handler(handler):
        f = gdal.VSIFOpenL('/vsiadls/test_copy/file.bin', 'wb')
        assert f is not None
        assert gdal.VSIFWriteL('foo', 1, 3, f) == 3
        assert gdal.VSIFCloseL(f) != 0

    # Error case in PATCH close
    handler = webserver.SequentialHandler()
    handler.add('PUT', '/azure/blob/myaccount/test_copy/file.bin?resource=file', 201)
    handler.add('PATCH', '/azure/blob/myaccount/test_copy/file.bin?action=append&position=0', 202, expected_body = b'foo')
    handler.add('PATCH', '/azure/blob/myaccount/test_copy/file.bin?action=flush&close=true&position=3', 400)
    with webserver.install_http_handler(handler):
        f = gdal.VSIFOpenL('/vsiadls/test_copy/file.bin', 'wb')
        assert f is not None
        assert gdal.VSIFWriteL('foo', 1, 3, f) == 3
        assert gdal.VSIFCloseL(f) != 0

    # Chunked output
    with gdaltest.config_option('VSIAZ_CHUNK_SIZE_BYTES', '10'):
        handler = webserver.SequentialHandler()
        handler.add('PUT', '/azure/blob/myaccount/test_copy/file.bin?resource=file', 201)
        handler.add('PATCH', '/azure/blob/myaccount/test_copy/file.bin?action=append&position=0', 202, expected_body = b'0123456789')
        handler.add('PATCH', '/azure/blob/myaccount/test_copy/file.bin?action=append&position=10', 202, expected_body = b'abcd')
        handler.add('PATCH', '/azure/blob/myaccount/test_copy/file.bin?action=flush&close=true&position=14', 200)
        with webserver.install_http_handler(handler):
            f = gdal.VSIFOpenL('/vsiadls/test_copy/file.bin', 'wb')
            assert f is not None
            assert gdal.VSIFWriteL('0123456789abcd', 1, 14, f) == 14
            assert gdal.VSIFCloseL(f) == 0

    # Chunked output with last chunk being the chunk size
    with gdaltest.config_option('VSIAZ_CHUNK_SIZE_BYTES', '5'):
        handler = webserver.SequentialHandler()
        handler.add('PUT', '/azure/blob/myaccount/test_copy/file.bin?resource=file', 201)
        handler.add('PATCH', '/azure/blob/myaccount/test_copy/file.bin?action=append&position=0', 202, expected_body = b'01234')
        handler.add('PATCH', '/azure/blob/myaccount/test_copy/file.bin?action=append&position=5', 202, expected_body = b'56789')
        handler.add('PATCH', '/azure/blob/myaccount/test_copy/file.bin?action=flush&close=true&position=10', 200)
        with webserver.install_http_handler(handler):
            f = gdal.VSIFOpenL('/vsiadls/test_copy/file.bin', 'wb')
            assert f is not None
            assert gdal.VSIFWriteL('0123456789', 1, 10, f) == 10
            assert gdal.VSIFCloseL(f) == 0

###############################################################################
# Test Unlink()


def test_vsiadls_fake_unlink():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    # Success
    handler = webserver.SequentialHandler()
    handler.add('HEAD', '/azure/blob/myaccount/az_bucket_test_unlink/myfile', 200, {'Content-Length': '1'})
    handler.add('DELETE', '/azure/blob/myaccount/az_bucket_test_unlink/myfile', 200, {'Connection': 'close'})
    with webserver.install_http_handler(handler):
        ret = gdal.Unlink('/vsiadls/az_bucket_test_unlink/myfile')
    assert ret == 0

    # Failure
    handler = webserver.SequentialHandler()
    handler.add('HEAD', '/azure/blob/myaccount/az_bucket_test_unlink/myfile', 200, {'Content-Length': '1'})
    handler.add('DELETE', '/azure/blob/myaccount/az_bucket_test_unlink/myfile', 400, {'Connection': 'close'})
    with webserver.install_http_handler(handler):
        with gdaltest.error_handler():
            ret = gdal.Unlink('/vsiadls/az_bucket_test_unlink/myfile')
    assert ret == -1

###############################################################################
# Test Mkdir() / Rmdir() / RmdirRecursive()


def test_vsiadls_fake_mkdir_rmdir():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    # Invalid name
    ret = gdal.Mkdir('/vsiadls', 0)
    assert ret != 0

    handler = webserver.SequentialHandler()
    handler.add('HEAD', '/azure/blob/myaccount/az_bucket_test_mkdir/dir', 404, {'Connection': 'close'})
    handler.add('PUT', '/azure/blob/myaccount/az_bucket_test_mkdir/dir?resource=directory',  201)
    with webserver.install_http_handler(handler):
        ret = gdal.Mkdir('/vsiadls/az_bucket_test_mkdir/dir', 0)
    assert ret == 0

    # Try creating already existing directory
    handler = webserver.SequentialHandler()
    handler.add('HEAD', '/azure/blob/myaccount/az_bucket_test_mkdir/dir', 200, {'x-ms-permissions': 'rwxrwxrwx', 'x-ms-resource-type': 'directory' } )
    with webserver.install_http_handler(handler):
        ret = gdal.Mkdir('/vsiadls/az_bucket_test_mkdir/dir', 0)
    assert ret != 0

    # Invalid name
    ret = gdal.Rmdir('/vsiadls')
    assert ret != 0

    # Not a directory
    handler = webserver.SequentialHandler()
    handler.add('HEAD', '/azure/blob/myaccount/az_bucket_test_mkdir/it_is_a_file', 200, {'x-ms-permissions': 'rwxrwxrwx', 'x-ms-resource-type': 'file' } )
    with webserver.install_http_handler(handler):
        ret = gdal.Rmdir('/vsiadls/az_bucket_test_mkdir/it_is_a_file')
    assert ret != 0

    # Valid
    handler = webserver.SequentialHandler()
    handler.add('DELETE', '/azure/blob/myaccount/az_bucket_test_mkdir/dir?recursive=false', 200)
    with webserver.install_http_handler(handler):
        ret = gdal.Rmdir('/vsiadls/az_bucket_test_mkdir/dir')
    assert ret == 0

    # Try deleting already deleted directory
    handler = webserver.SequentialHandler()
    handler.add('HEAD', '/azure/blob/myaccount/az_bucket_test_mkdir/dir', 404 )
    with webserver.install_http_handler(handler):
        ret = gdal.Rmdir('/vsiadls/az_bucket_test_mkdir/dir')
    assert ret != 0

    # RmdirRecursive
    handler = webserver.SequentialHandler()
    handler.add('HEAD', '/azure/blob/myaccount/az_bucket_test_mkdir/dir_rec', 200, {'x-ms-permissions': 'rwxrwxrwx', 'x-ms-resource-type': 'directory' } )
    handler.add('DELETE', '/azure/blob/myaccount/az_bucket_test_mkdir/dir_rec?recursive=true', 200)
    with webserver.install_http_handler(handler):
        ret = gdal.RmdirRecursive('/vsiadls/az_bucket_test_mkdir/dir_rec')
    assert ret == 0

###############################################################################
# Test rename

def test_vsiadls_fake_rename():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()
    handler = webserver.SequentialHandler()
    handler.add('HEAD', '/azure/blob/myaccount/test/source.txt', 200, {'Content-Length': '3', 'x-ms-permissions': 'rwxrwxrwx', 'x-ms-resource-type': 'file'})
    handler.add('PUT', '/azure/blob/myaccount/test/target.txt', 201, expected_headers = {'x-ms-rename-source' : '/test/source.txt'})
    handler.add('HEAD', '/azure/blob/myaccount/test/source.txt', 404)
    with webserver.install_http_handler(handler):
        assert gdal.Rename( '/vsiadls/test/source.txt', '/vsiadls/test/target.txt') == 0
        assert gdal.VSIStatL('/vsiadls/test/source.txt') is None


###############################################################################
# Test Sync() with source and target on /vsiadls/
# Note: this is using Azure blob object copying since ADLS has no equivalent

def test_vsiadls_fake_sync_copyobject():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add('HEAD', '/azure/blob/myaccount/test_bucket/src.txt', 200, {'Content-Length': '3', 'x-ms-permissions': 'rwxrwxrwx', 'x-ms-resource-type': 'file'})
    handler.add('HEAD', '/azure/blob/myaccount/test_bucket/dst.txt', 404)
    handler.add('PUT', '/azure/blob/myaccount/test_bucket/dst.txt', 202,
                expected_headers={'x-ms-copy-source': 'http://127.0.0.1:%d/azure/blob/myaccount/test_bucket/src.txt' % gdaltest.webserver_port})
    with webserver.install_http_handler(handler):
        assert gdal.Sync('/vsiadls/test_bucket/src.txt',
                         '/vsiadls/test_bucket/dst.txt')

    gdal.VSICurlClearCache()

    # Error case
    handler = webserver.SequentialHandler()
    handler.add('HEAD', '/azure/blob/myaccount/test_bucket/src.txt', 200, {'Content-Length': '3', 'x-ms-permissions': 'rwxrwxrwx', 'x-ms-resource-type': 'file'})
    handler.add('HEAD', '/azure/blob/myaccount/test_bucket/dst.txt', 404)
    handler.add('PUT', '/azure/blob/myaccount/test_bucket/dst.txt', 400)
    with webserver.install_http_handler(handler):
        with gdaltest.error_handler():
            assert not gdal.Sync('/vsiadls/test_bucket/src.txt',
                                 '/vsiadls/test_bucket/dst.txt')


###############################################################################
# Test Sync() and multithreaded download of a single file


def test_vsiadls_fake_sync_multithreaded_upload_single_file():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    gdal.Mkdir('/vsimem/test', 0)
    gdal.FileFromMemBuffer('/vsimem/test/foo', 'foo\n')

    handler = webserver.SequentialHandler()
    handler.add('HEAD', '/azure/blob/myaccount/test_bucket?resource=filesystem', 200)
    handler.add('HEAD', '/azure/blob/myaccount/test_bucket/foo', 404)
    handler.add('PUT', '/azure/blob/myaccount/test_bucket/foo?resource=file', 201)
    handler.add('PATCH', '/azure/blob/myaccount/test_bucket/foo?action=append&position=0', 202, expected_body = b'foo')
    handler.add('PATCH', '/azure/blob/myaccount/test_bucket/foo?action=append&position=3', 202, expected_body = b'\n')
    handler.add('PATCH', '/azure/blob/myaccount/test_bucket/foo?action=flush&close=true&position=4', 200)

    with gdaltest.config_option('VSIS3_SIMULATE_THREADING', 'YES'):
        with webserver.install_http_handler(handler):
            assert gdal.Sync('/vsimem/test/foo',
                             '/vsiadls/test_bucket',
                             options=['NUM_THREADS=1', 'CHUNK_SIZE=3'])

    gdal.RmdirRecursive('/vsimem/test')


###############################################################################
# Test GetFileMetadata () / SetFileMetadata()


def test_vsiadls_fake_metadata():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add('HEAD', '/azure/blob/myaccount/test/foo.bin', 200, {'Content-Length': '3', 'x-ms-permissions': 'rwxrwxrwx', 'x-ms-resource-type': 'file'})
    with webserver.install_http_handler(handler):
        md = gdal.GetFileMetadata('/vsiadls/test/foo.bin', 'HEADERS')
        assert 'x-ms-permissions' in md

    handler = webserver.SequentialHandler()
    handler.add('HEAD', '/azure/blob/myaccount/test/foo.bin?action=getStatus', 200, {'foo': 'bar'})
    with webserver.install_http_handler(handler):
        md = gdal.GetFileMetadata('/vsiadls/test/foo.bin', 'STATUS')
        assert 'foo' in md

    handler = webserver.SequentialHandler()
    handler.add('HEAD', '/azure/blob/myaccount/test/foo.bin?action=getAccessControl', 200, {'x-ms-acl': 'some_acl'})
    with webserver.install_http_handler(handler):
        md = gdal.GetFileMetadata('/vsiadls/test/foo.bin', 'ACL')
        assert 'x-ms-acl' in md

    # Error case
    handler = webserver.SequentialHandler()
    handler.add('HEAD', '/azure/blob/myaccount/test/foo.bin?action=getStatus', 404)
    with webserver.install_http_handler(handler):
        assert gdal.GetFileMetadata('/vsiadls/test/foo.bin', 'STATUS') == {}

    # SetMetadata()
    handler = webserver.SequentialHandler()
    handler.add('PATCH', '/azure/blob/myaccount/test/foo.bin?action=setProperties', 200, expected_headers={'x-ms-properties': 'foo=bar'})
    with webserver.install_http_handler(handler):
        assert gdal.SetFileMetadata('/vsiadls/test/foo.bin', {'x-ms-properties': 'foo=bar'}, 'PROPERTIES')

    handler = webserver.SequentialHandler()
    handler.add('PATCH', '/azure/blob/myaccount/test/foo.bin?action=setAccessControl', 200, expected_headers={'x-ms-acl': 'foo'})
    with webserver.install_http_handler(handler):
        assert gdal.SetFileMetadata('/vsiadls/test/foo.bin', {'x-ms-acl': 'foo'}, 'ACL')

    handler = webserver.SequentialHandler()
    handler.add('PATCH', '/azure/blob/myaccount/test/foo.bin?action=setAccessControlRecursive&mode=set', 200, expected_headers={'x-ms-acl': 'foo'})
    with webserver.install_http_handler(handler):
        assert gdal.SetFileMetadata('/vsiadls/test/foo.bin', {'x-ms-acl': 'foo'}, 'ACL', ['RECURSIVE=TRUE', 'MODE=set'])

    # Error case
    handler = webserver.SequentialHandler()
    handler.add('PATCH', '/azure/blob/myaccount/test/foo.bin?action=setProperties', 404)
    with webserver.install_http_handler(handler):
        assert not gdal.SetFileMetadata('/vsiadls/test/foo.bin', {'x-ms-properties': 'foo=bar'}, 'PROPERTIES')
