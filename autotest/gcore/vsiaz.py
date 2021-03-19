#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test /vsiaz
# Author:   Even Rouault <even dot rouault at spatialys dot com>
#
###############################################################################
# Copyright (c) 2017 Even Rouault <even dot rouault at spatialys dot com>
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

    az_vars = {}
    for var in ('AZURE_STORAGE_CONNECTION_STRING', 'AZURE_STORAGE_ACCOUNT',
                'AZURE_STORAGE_ACCESS_KEY', 'AZURE_SAS', 'AZURE_NO_SIGN_REQUEST'):
        az_vars[var] = gdal.GetConfigOption(var)
        if az_vars[var] is not None:
            gdal.SetConfigOption(var, "")

    assert gdal.GetSignedURL('/vsiaz/foo/bar') is None

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
# Test with a fake Azure Blob server


def test_vsiaz_fake_basic():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    signed_url = gdal.GetSignedURL('/vsiaz/az_fake_bucket/resource', ['START_DATE=20180213T123456'])
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
        f = open_for_read('/vsiaz/az_fake_bucket/resource')
        assert f is not None
        data = gdal.VSIFReadL(1, 4, f).decode('ascii')
        gdal.VSIFCloseL(f)

    assert data == 'foo'

    def method(request):

        request.protocol_version = 'HTTP/1.1'
        h = request.headers
        if 'Authorization' not in h or \
           h['Authorization'] != 'SharedKey myaccount:bdrimjEtVnI51+rmJtfZddVn/u3N6MbtdEDnoyBByTo=' or \
           'x-ms-date' not in h or h['x-ms-date'] != 'my_timestamp' or \
           'Accept-Encoding' not in h or h['Accept-Encoding'] != 'gzip':
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
        f = open_for_read('/vsiaz_streaming/az_fake_bucket/resource')
        assert f is not None
        data = gdal.VSIFReadL(1, 4, f).decode('ascii')
        gdal.VSIFCloseL(f)

        assert data == 'foo'

    handler = webserver.SequentialHandler()
    handler.add('HEAD', '/azure/blob/myaccount/az_fake_bucket/resource2.bin', 200,
                {'Content-Length': '1000000'})
    with webserver.install_http_handler(handler):
        stat_res = gdal.VSIStatL('/vsiaz/az_fake_bucket/resource2.bin')
        if stat_res is None or stat_res.size != 1000000:
            if stat_res is not None:
                print(stat_res.size)
            else:
                print(stat_res)
            pytest.fail()

    handler = webserver.SequentialHandler()
    handler.add('HEAD', '/azure/blob/myaccount/az_fake_bucket/resource2.bin', 200,
                {'Content-Length': 1000000})
    with webserver.install_http_handler(handler):
        stat_res = gdal.VSIStatL('/vsiaz_streaming/az_fake_bucket/resource2.bin')
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
    handler.add('GET', '/azure/blob/myaccount/az_fake_bucket2?comp=list&delimiter=%2F&prefix=a_dir%20with_space%2F&restype=container', 200,
                {'Content-type': 'application/xml'},
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
                """)
    handler.add('GET', '/azure/blob/myaccount/az_fake_bucket2?comp=list&delimiter=%2F&marker=bla&prefix=a_dir%20with_space%2F&restype=container', 200,
                {'Content-type': 'application/xml'},
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
                """)

    with webserver.install_http_handler(handler):
        f = open_for_read('/vsiaz/az_fake_bucket2/a_dir with_space/resource3 with_space.bin')
    if f is None:

        if gdaltest.is_travis_branch('trusty'):
            pytest.skip('Skipped on trusty branch, but should be investigated')

        pytest.fail()
    gdal.VSIFCloseL(f)

    dir_contents = gdal.ReadDir('/vsiaz/az_fake_bucket2/a_dir with_space')
    assert dir_contents == ['resource3 with_space.bin', 'resource4.bin', 'subdir']
    assert gdal.VSIStatL('/vsiaz/az_fake_bucket2/a_dir with_space/resource3 with_space.bin').size == 123456
    assert gdal.VSIStatL('/vsiaz/az_fake_bucket2/a_dir with_space/resource3 with_space.bin').mtime == 1

    # ReadDir on something known to be a file shouldn't cause network access
    dir_contents = gdal.ReadDir('/vsiaz/az_fake_bucket2/a_dir with_space/resource3 with_space.bin')
    assert dir_contents is None

    # Test error on ReadDir()
    handler = webserver.SequentialHandler()
    handler.add('GET', '/azure/blob/myaccount/az_fake_bucket2?comp=list&delimiter=%2F&prefix=error_test%2F&restype=container', 500)
    with webserver.install_http_handler(handler):
        dir_contents = gdal.ReadDir('/vsiaz/az_fake_bucket2/error_test/')
    assert dir_contents is None

    # List containers (empty result)
    handler = webserver.SequentialHandler()
    handler.add('GET', '/azure/blob/myaccount/?comp=list', 200, {'Content-type': 'application/xml'},
                """<?xml version="1.0" encoding="UTF-8"?>
        <EnumerationResults ServiceEndpoint="https://myaccount.blob.core.windows.net">
            <Containers/>
            </EnumerationResults>
        """)
    with webserver.install_http_handler(handler):
        dir_contents = gdal.ReadDir('/vsiaz/')
    assert dir_contents == ['.']

    gdal.VSICurlClearCache()

    # List containers
    handler = webserver.SequentialHandler()
    handler.add('GET', '/azure/blob/myaccount/?comp=list', 200, {'Content-type': 'application/xml'},
                """<?xml version="1.0" encoding="UTF-8"?>
        <EnumerationResults>
            <Containers>
                <Container>
                    <Name>mycontainer1</Name>
                </Container>
            </Containers>
            <NextMarker>bla</NextMarker>
            </EnumerationResults>
        """)
    handler.add('GET', '/azure/blob/myaccount/?comp=list&marker=bla', 200, {'Content-type': 'application/xml'},
                """<?xml version="1.0" encoding="UTF-8"?>
        <EnumerationResults>
            <Containers>
                <Container>
                    <Name>mycontainer2</Name>
                </Container>
            </Containers>
        </EnumerationResults>
        """)
    with webserver.install_http_handler(handler):
        dir_contents = gdal.ReadDir('/vsiaz/')
    assert dir_contents == ['mycontainer1', 'mycontainer2']

###############################################################################
# Test AZURE_SAS option with fake server


def test_vsiaz_sas_fake():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    with gdaltest.config_options({ 'AZURE_STORAGE_ACCOUNT': 'test', 'AZURE_SAS': 'sig=sas', 'CPL_AZURE_ENDPOINT' : '127.0.0.1:%d' % gdaltest.webserver_port, 'CPL_AZURE_USE_HTTPS': 'NO', 'AZURE_STORAGE_CONNECTION_STRING': ''}):

        handler = webserver.SequentialHandler()

        handler.add('GET', '/azure/blob/test/test?comp=list&delimiter=%2F&restype=container&sig=sas', 200,
                {'Content-type': 'application/xml'},
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
                """)

        with webserver.install_http_handler(handler):
            assert 'foo.bin' in gdal.ReadDir('/vsiaz/test')

        assert gdal.VSIStatL('/vsiaz/test/foo.bin').size == 456789

###############################################################################
# Test write


def test_vsiaz_fake_write():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    # Test creation of BlockBob
    f = gdal.VSIFOpenExL('/vsiaz/test_copy/file.tif', 'wb', 0, ['Content-Encoding=bar'])
    assert f is not None

    handler = webserver.SequentialHandler()

    def method(request):
        h = request.headers
        if 'Authorization' not in h or \
           h['Authorization'] != 'SharedKey myaccount:jqNjm+2wmGAetpQPL2X9UWIrvmbYiOV59pQtyXD35nM=' or \
           'Expect' not in h or h['Expect'] != '100-continue' or \
           'Content-Length' not in h or h['Content-Length'] != '40000' or \
           'x-ms-date' not in h or h['x-ms-date'] != 'my_timestamp' or \
           'x-ms-blob-type' not in h or h['x-ms-blob-type'] != 'BlockBlob' or \
           'Content-Type' not in h or h['Content-Type'] != 'image/tiff' or \
           'Content-Encoding' not in h or h['Content-Encoding'] != 'bar':
            sys.stderr.write('Bad headers: %s\n' % str(h))
            request.send_response(403)
            return

        request.protocol_version = 'HTTP/1.1'
        request.wfile.write('HTTP/1.1 100 Continue\r\n\r\n'.encode('ascii'))
        content = request.rfile.read(40000).decode('ascii')
        if len(content) != 40000:
            sys.stderr.write('Bad headers: %s\n' % str(request.headers))
            request.send_response(403)
            request.send_header('Content-Length', 0)
            request.end_headers()
            return
        request.send_response(201)
        request.send_header('Content-Length', 0)
        request.end_headers()

    handler.add('PUT', '/azure/blob/myaccount/test_copy/file.tif', custom_method=method)
    with webserver.install_http_handler(handler):
        ret = gdal.VSIFWriteL('x' * 35000, 1, 35000, f)
        ret += gdal.VSIFWriteL('x' * 5000, 1, 5000, f)
        if ret != 40000:
            gdal.VSIFCloseL(f)
            pytest.fail(ret)
        gdal.VSIFCloseL(f)

    # Simulate illegal read
    f = gdal.VSIFOpenL('/vsiaz/test_copy/file.tif', 'wb')
    assert f is not None
    with gdaltest.error_handler():
        ret = gdal.VSIFReadL(1, 1, f)
    assert not ret
    gdal.VSIFCloseL(f)

    # Simulate illegal seek
    f = gdal.VSIFOpenL('/vsiaz/test_copy/file.tif', 'wb')
    assert f is not None
    with gdaltest.error_handler():
        ret = gdal.VSIFSeekL(f, 1, 0)
    assert ret != 0
    gdal.VSIFCloseL(f)

    # Simulate failure when putting BlockBob
    f = gdal.VSIFOpenL('/vsiaz/test_copy/file.tif', 'wb')
    assert f is not None

    handler = webserver.SequentialHandler()

    def method(request):
        request.protocol_version = 'HTTP/1.1'
        request.send_response(403)
        request.send_header('Content-Length', 0)
        request.end_headers()

    handler.add('PUT', '/azure/blob/myaccount/test_copy/file.tif', custom_method=method)

    if gdal.VSIFSeekL(f, 0, 0) != 0:
        gdal.VSIFCloseL(f)
        pytest.fail()

    gdal.VSIFWriteL('x' * 35000, 1, 35000, f)

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
        with gdaltest.error_handler():
            ret = gdal.VSIFCloseL(f)
        if ret == 0:
            gdal.VSIFCloseL(f)
            pytest.fail(ret)

    # Simulate creation of BlockBob over an existing blob of incompatible type
    f = gdal.VSIFOpenL('/vsiaz/test_copy/file.tif', 'wb')
    assert f is not None

    handler = webserver.SequentialHandler()
    handler.add('PUT', '/azure/blob/myaccount/test_copy/file.tif', 409)
    handler.add('DELETE', '/azure/blob/myaccount/test_copy/file.tif', 202)
    handler.add('PUT', '/azure/blob/myaccount/test_copy/file.tif', 201)
    with webserver.install_http_handler(handler):
        gdal.VSIFCloseL(f)

    # Test creation of AppendBlob
    gdal.SetConfigOption('VSIAZ_CHUNK_SIZE_BYTES', '10')
    f = gdal.VSIFOpenL('/vsiaz/test_copy/file.tif', 'wb')
    gdal.SetConfigOption('VSIAZ_CHUNK_SIZE_BYTES', None)
    assert f is not None

    handler = webserver.SequentialHandler()

    def method(request):
        h = request.headers
        if 'Authorization' not in h or \
           h['Authorization'] != 'SharedKey myaccount:zmFZkO5IZCidFB/aAtr3oUaT2xg//F2SjyIgWMUoV5g=' or \
           'Content-Length' not in h or h['Content-Length'] != '0' or \
           'x-ms-date' not in h or h['x-ms-date'] != 'my_timestamp' or \
           'x-ms-blob-type' not in h or h['x-ms-blob-type'] != 'AppendBlob':
            sys.stderr.write('Bad headers: %s\n' % str(h))
            request.send_response(403)
            return

        request.protocol_version = 'HTTP/1.1'
        request.send_response(201)
        request.send_header('Content-Length', 0)
        request.end_headers()

    handler.add('PUT', '/azure/blob/myaccount/test_copy/file.tif', custom_method=method)

    def method(request):
        h = request.headers
        if 'Content-Length' not in h or h['Content-Length'] != '10' or \
           'x-ms-date' not in h or h['x-ms-date'] != 'my_timestamp' or \
           'x-ms-blob-type' not in h or h['x-ms-blob-type'] != 'AppendBlob':
            sys.stderr.write('Bad headers: %s\n' % str(h))
            request.send_response(403)
            return

        request.protocol_version = 'HTTP/1.1'
        content = request.rfile.read(10).decode('ascii')
        if content != '0123456789':
            sys.stderr.write('Bad headers: %s\n' % str(request.headers))
            request.send_response(403)
            request.send_header('Content-Length', 0)
            request.end_headers()
            return
        request.send_response(201)
        request.send_header('Content-Length', 0)
        request.end_headers()

    handler.add('PUT', '/azure/blob/myaccount/test_copy/file.tif?comp=appendblock', custom_method=method)

    def method(request):
        h = request.headers
        if 'Content-Length' not in h or h['Content-Length'] != '6' or \
           'x-ms-date' not in h or h['x-ms-date'] != 'my_timestamp' or \
           'x-ms-blob-type' not in h or h['x-ms-blob-type'] != 'AppendBlob':
            sys.stderr.write('Bad headers: %s\n' % str(h))
            request.send_response(403)
            return

        request.protocol_version = 'HTTP/1.1'
        content = request.rfile.read(6).decode('ascii')
        if content != 'abcdef':
            sys.stderr.write('Bad headers: %s\n' % str(request.headers))
            request.send_response(403)
            request.send_header('Content-Length', 0)
            request.end_headers()
            return
        request.send_response(201)
        request.send_header('Content-Length', 0)
        request.end_headers()

    handler.add('PUT', '/azure/blob/myaccount/test_copy/file.tif?comp=appendblock', custom_method=method)

    with webserver.install_http_handler(handler):
        ret = gdal.VSIFWriteL('0123456789abcdef', 1, 16, f)
        if ret != 16:
            gdal.VSIFCloseL(f)
            pytest.fail(ret)
        gdal.VSIFCloseL(f)

    # Test failed creation of AppendBlob
    gdal.SetConfigOption('VSIAZ_CHUNK_SIZE_BYTES', '10')
    f = gdal.VSIFOpenL('/vsiaz/test_copy/file.tif', 'wb')
    gdal.SetConfigOption('VSIAZ_CHUNK_SIZE_BYTES', None)
    assert f is not None

    handler = webserver.SequentialHandler()

    def method(request):
        request.protocol_version = 'HTTP/1.1'
        request.send_response(403)
        request.send_header('Content-Length', 0)
        request.end_headers()

    handler.add('PUT', '/azure/blob/myaccount/test_copy/file.tif', custom_method=method)

    with webserver.install_http_handler(handler):
        with gdaltest.error_handler():
            ret = gdal.VSIFWriteL('0123456789abcdef', 1, 16, f)
        if ret != 0:
            gdal.VSIFCloseL(f)
            pytest.fail(ret)
        gdal.VSIFCloseL(f)

    # Test failed writing of a block of an AppendBlob
    gdal.SetConfigOption('VSIAZ_CHUNK_SIZE_BYTES', '10')
    f = gdal.VSIFOpenL('/vsiaz/test_copy/file.tif', 'wb')
    gdal.SetConfigOption('VSIAZ_CHUNK_SIZE_BYTES', None)
    assert f is not None

    handler = webserver.SequentialHandler()
    handler.add('PUT', '/azure/blob/myaccount/test_copy/file.tif', 201)
    handler.add('PUT', '/azure/blob/myaccount/test_copy/file.tif?comp=appendblock', 403)
    with webserver.install_http_handler(handler):
        with gdaltest.error_handler():
            ret = gdal.VSIFWriteL('0123456789abcdef', 1, 16, f)
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

    # Test creation of BlockBob
    f = gdal.VSIFOpenL('/vsiaz/test_copy/file.bin', 'wb')
    assert f is not None

    with gdaltest.config_options({'GDAL_HTTP_MAX_RETRY': '2',
                                  'GDAL_HTTP_RETRY_DELAY': '0.01'}):

        handler = webserver.SequentialHandler()

        def method(request):
            request.protocol_version = 'HTTP/1.1'
            request.wfile.write('HTTP/1.1 100 Continue\r\n\r\n'.encode('ascii'))
            content = request.rfile.read(3).decode('ascii')
            if len(content) != 3:
                sys.stderr.write('Bad headers: %s\n' % str(request.headers))
                request.send_response(403)
                request.send_header('Content-Length', 0)
                request.end_headers()
                return
            request.send_response(201)
            request.send_header('Content-Length', 0)
            request.end_headers()

        handler.add('PUT', '/azure/blob/myaccount/test_copy/file.bin', 502)
        handler.add('PUT', '/azure/blob/myaccount/test_copy/file.bin', custom_method=method)
        with gdaltest.error_handler():
            with webserver.install_http_handler(handler):
                assert gdal.VSIFWriteL('foo', 1, 3, f) == 3
                gdal.VSIFCloseL(f)

###############################################################################
# Test write with retry


def test_vsiaz_write_appendblob_retry():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    with gdaltest.config_options({'GDAL_HTTP_MAX_RETRY': '2',
                                  'GDAL_HTTP_RETRY_DELAY': '0.01',
                                  'VSIAZ_CHUNK_SIZE_BYTES': '10'}):

        f = gdal.VSIFOpenL('/vsiaz/test_copy/file.bin', 'wb')
        assert f is not None

        handler = webserver.SequentialHandler()
        handler.add('PUT', '/azure/blob/myaccount/test_copy/file.bin', 502)
        handler.add('PUT', '/azure/blob/myaccount/test_copy/file.bin', 201)
        handler.add('PUT', '/azure/blob/myaccount/test_copy/file.bin?comp=appendblock', 502)
        handler.add('PUT', '/azure/blob/myaccount/test_copy/file.bin?comp=appendblock', 201)
        handler.add('PUT', '/azure/blob/myaccount/test_copy/file.bin?comp=appendblock', 502)
        handler.add('PUT', '/azure/blob/myaccount/test_copy/file.bin?comp=appendblock', 201)

        with gdaltest.error_handler():
            with webserver.install_http_handler(handler):
                assert gdal.VSIFWriteL('0123456789abcdef', 1, 16, f) == 16
                gdal.VSIFCloseL(f)

###############################################################################
# Test Unlink()


def test_vsiaz_fake_unlink():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    # Success
    handler = webserver.SequentialHandler()
    handler.add('HEAD', '/azure/blob/myaccount/az_bucket_test_unlink/myfile', 200, {'Content-Length': '1'})
    handler.add('DELETE', '/azure/blob/myaccount/az_bucket_test_unlink/myfile', 202, {'Connection': 'close'})
    with webserver.install_http_handler(handler):
        ret = gdal.Unlink('/vsiaz/az_bucket_test_unlink/myfile')
    assert ret == 0

    # Failure
    handler = webserver.SequentialHandler()
    handler.add('HEAD', '/azure/blob/myaccount/az_bucket_test_unlink/myfile', 200, {'Content-Length': '1'})
    handler.add('DELETE', '/azure/blob/myaccount/az_bucket_test_unlink/myfile', 400, {'Connection': 'close'})
    with webserver.install_http_handler(handler):
        with gdaltest.error_handler():
            ret = gdal.Unlink('/vsiaz/az_bucket_test_unlink/myfile')
    assert ret == -1

###############################################################################
# Test Mkdir() / Rmdir()


def test_vsiaz_fake_mkdir_rmdir():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    # Invalid name
    ret = gdal.Mkdir('/vsiaz', 0)
    assert ret != 0

    handler = webserver.SequentialHandler()
    handler.add('HEAD', '/azure/blob/myaccount/az_bucket_test_mkdir/dir/', 404, {'Connection': 'close'})
    handler.add('GET', '/azure/blob/myaccount/az_bucket_test_mkdir?comp=list&delimiter=%2F&maxresults=1&prefix=dir%2F&restype=container', 200, {'Connection': 'close'})
    handler.add('PUT', '/azure/blob/myaccount/az_bucket_test_mkdir/dir/.gdal_marker_for_dir', 201)
    with webserver.install_http_handler(handler):
        ret = gdal.Mkdir('/vsiaz/az_bucket_test_mkdir/dir', 0)
    assert ret == 0

    # Try creating already existing directory
    handler = webserver.SequentialHandler()
    handler.add('HEAD', '/azure/blob/myaccount/az_bucket_test_mkdir/dir/', 404)
    handler.add('GET', '/azure/blob/myaccount/az_bucket_test_mkdir?comp=list&delimiter=%2F&maxresults=1&prefix=dir%2F&restype=container',
                200,
                {'Connection': 'close', 'Content-type': 'application/xml'},
                """<?xml version="1.0" encoding="UTF-8"?>
                    <EnumerationResults>
                        <Prefix>dir/</Prefix>
                        <Blobs>
                          <Blob>
                            <Name>dir/.gdal_marker_for_dir</Name>
                          </Blob>
                        </Blobs>
                    </EnumerationResults>
                """)
    with webserver.install_http_handler(handler):
        ret = gdal.Mkdir('/vsiaz/az_bucket_test_mkdir/dir', 0)
    assert ret != 0

    # Invalid name
    ret = gdal.Rmdir('/vsiaz')
    assert ret != 0

    # Not a directory
    handler = webserver.SequentialHandler()
    handler.add('HEAD', '/azure/blob/myaccount/az_bucket_test_mkdir/it_is_a_file/', 404)
    handler.add('GET', '/azure/blob/myaccount/az_bucket_test_mkdir?comp=list&delimiter=%2F&maxresults=1&prefix=it_is_a_file%2F&restype=container',
                200,
                {'Connection': 'close', 'Content-type': 'application/xml'},
                """<?xml version="1.0" encoding="UTF-8"?>
                    <EnumerationResults>
                        <Prefix>az_bucket_test_mkdir/</Prefix>
                        <Blobs>
                          <Blob>
                            <Name>az_bucket_test_mkdir/it_is_a_file</Name>
                          </Blob>
                        </Blobs>
                    </EnumerationResults>
                """)
    handler.add('GET', '/azure/blob/myaccount/az_bucket_test_mkdir?comp=list&delimiter=%2F&maxresults=1&prefix=it_is_a_file%2F&restype=container', 200)
    with webserver.install_http_handler(handler):
        ret = gdal.Rmdir('/vsiaz/az_bucket_test_mkdir/it_is_a_file')
    assert ret != 0

    # Valid
    handler = webserver.SequentialHandler()
    handler.add('GET', '/azure/blob/myaccount/az_bucket_test_mkdir?comp=list&delimiter=%2F&maxresults=1&prefix=dir%2F&restype=container',
                200,
                {'Connection': 'close', 'Content-type': 'application/xml'},
                """<?xml version="1.0" encoding="UTF-8"?>
                    <EnumerationResults>
                        <Prefix>dir/</Prefix>
                        <Blobs>
                          <Blob>
                            <Name>dir/.gdal_marker_for_dir</Name>
                          </Blob>
                        </Blobs>
                    </EnumerationResults>
                """)
    handler.add('DELETE', '/azure/blob/myaccount/az_bucket_test_mkdir/dir/.gdal_marker_for_dir', 202)
    with webserver.install_http_handler(handler):
        ret = gdal.Rmdir('/vsiaz/az_bucket_test_mkdir/dir')
    assert ret == 0

    # Try deleting already deleted directory
    # --> do not consider this as an error because Azure directories are removed
    # as soon as the last object in it is removed. So when directories are created
    # without .gdal_marker_for_dir they will disappear without explicit removal
    handler = webserver.SequentialHandler()
    handler.add('HEAD', '/azure/blob/myaccount/az_bucket_test_mkdir/dir/', 404)
    handler.add('GET', '/azure/blob/myaccount/az_bucket_test_mkdir?comp=list&delimiter=%2F&maxresults=1&prefix=dir%2F&restype=container', 200)
    with webserver.install_http_handler(handler):
        ret = gdal.Rmdir('/vsiaz/az_bucket_test_mkdir/dir')
    assert ret == 0

    # Try deleting non-empty directory
    handler = webserver.SequentialHandler()
    handler.add('HEAD', '/azure/blob/myaccount/az_bucket_test_mkdir/dir_nonempty/', 404)
    handler.add('GET', '/azure/blob/myaccount/az_bucket_test_mkdir?comp=list&delimiter=%2F&maxresults=1&prefix=dir_nonempty%2F&restype=container',
                200,
                {'Connection': 'close', 'Content-type': 'application/xml'},
                """<?xml version="1.0" encoding="UTF-8"?>
                    <EnumerationResults>
                        <Prefix>dir_nonempty/</Prefix>
                        <Blobs>
                          <Blob>
                            <Name>dir_nonempty/foo</Name>
                          </Blob>
                        </Blobs>
                    </EnumerationResults>
                """)
    handler.add('GET', '/azure/blob/myaccount/az_bucket_test_mkdir?comp=list&delimiter=%2F&maxresults=1&prefix=dir_nonempty%2F&restype=container',
                200,
                {'Connection': 'close', 'Content-type': 'application/xml'},
                """<?xml version="1.0" encoding="UTF-8"?>
                    <EnumerationResults>
                        <Prefix>dir_nonempty/</Prefix>
                        <Blobs>
                          <Blob>
                            <Name>dir_nonempty/foo</Name>
                          </Blob>
                        </Blobs>
                    </EnumerationResults>
                """)
    with webserver.install_http_handler(handler):
        ret = gdal.Rmdir('/vsiaz/az_bucket_test_mkdir/dir_nonempty')
    assert ret != 0

###############################################################################


def test_vsiaz_fake_test_BlobEndpointInConnectionString():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    with gdaltest.config_option('AZURE_STORAGE_CONNECTION_STRING',
                                'DefaultEndpointsProtocol=http;AccountName=myaccount;AccountKey=MY_ACCOUNT_KEY;BlobEndpoint=http://127.0.0.1:%d/myaccount' % gdaltest.webserver_port):

        signed_url = gdal.GetSignedURL('/vsiaz/az_fake_bucket/resource')
        assert 'http://127.0.0.1:%d/myaccount/az_fake_bucket/resource' % gdaltest.webserver_port in signed_url

###############################################################################
# Test rename

def test_vsiaz_fake_rename():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()
    handler = webserver.SequentialHandler()
    handler.add('HEAD', '/azure/blob/myaccount/test/source.txt', 200,
                {'Content-Length': '3'})
    handler.add('HEAD', '/azure/blob/myaccount/test/target.txt', 404)
    handler.add('GET', '/azure/blob/myaccount/test?comp=list&delimiter=%2F&maxresults=1&prefix=target.txt%2F&restype=container', 200)

    def method(request):
        if request.headers['Content-Length'] != '0':
            sys.stderr.write('Did not get expected headers: %s\n' % str(request.headers))
            request.send_response(400)
            return
        expected = 'http://127.0.0.1:%d/azure/blob/myaccount/test/source.txt' % gdaltest.webserver_port
        if request.headers['x-ms-copy-source'] != expected:
            sys.stderr.write('Did not get expected headers: %s\n' % str(request.headers))
            request.send_response(400)
            return

        request.send_response(202)
        request.send_header('Content-Length', 0)
        request.end_headers()

    handler.add('PUT', '/azure/blob/myaccount/test/target.txt', custom_method=method)
    handler.add('DELETE', '/azure/blob/myaccount/test/source.txt', 202)

    with webserver.install_http_handler(handler):
        assert gdal.Rename( '/vsiaz/test/source.txt', '/vsiaz/test/target.txt') == 0


###############################################################################
# Test OpenDir() with a fake server


def test_vsiaz_opendir():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    # Unlimited depth
    handler = webserver.SequentialHandler()
    handler.add('GET', '/azure/blob/myaccount/opendir?comp=list&restype=container', 200, {'Content-type': 'application/xml'},
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
                    </EnumerationResults>""")
    with webserver.install_http_handler(handler):
        d = gdal.OpenDir('/vsiaz/opendir')
    assert d is not None

    entry = gdal.GetNextDirEntry(d)
    assert entry.name == 'test.txt'
    assert entry.size == 40
    assert entry.mode == 32768
    assert entry.mtime == 1

    entry = gdal.GetNextDirEntry(d)
    assert entry.name == 'subdir/'
    assert entry.mode == 16384

    entry = gdal.GetNextDirEntry(d)
    assert entry.name == 'subdir/test.txt'
    assert entry.size == 4
    assert entry.mode == 32768

    entry = gdal.GetNextDirEntry(d)
    assert entry is None

    gdal.CloseDir(d)


###############################################################################
# Test RmdirRecursive() with a fake server


def test_vsiaz_rmdirrecursive():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    handler = webserver.SequentialHandler()
    handler.add('GET', '/azure/blob/myaccount/rmdirrec?comp=list&prefix=subdir%2F&restype=container', 200, {'Content-type': 'application/xml'},
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
                    </EnumerationResults>""")
    handler.add('DELETE', '/azure/blob/myaccount/rmdirrec/subdir/test.txt', 202)
    handler.add('DELETE', '/azure/blob/myaccount/rmdirrec/subdir/subdir2/test.txt', 202)
    handler.add('HEAD', '/azure/blob/myaccount/rmdirrec/subdir/subdir2/', 404)
    handler.add('GET', '/azure/blob/myaccount/rmdirrec?comp=list&delimiter=%2F&maxresults=1&prefix=subdir%2Fsubdir2%2F&restype=container', 200)
    handler.add('HEAD', '/azure/blob/myaccount/rmdirrec/subdir/', 404)
    handler.add('GET', '/azure/blob/myaccount/rmdirrec?comp=list&delimiter=%2F&maxresults=1&prefix=subdir%2F&restype=container', 200)
    with webserver.install_http_handler(handler):
        assert gdal.RmdirRecursive('/vsiaz/rmdirrec/subdir') == 0


###############################################################################
# Test Sync() and multithreaded download and CHUNK_SIZE


def test_vsiaz_fake_sync_multithreaded_upload_chunk_size():

    if gdaltest.is_github_workflow_mac():
        pytest.xfail('Failure. See https://github.com/rouault/gdal/runs/1329425333?check_suite_focus=true')

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    gdal.Mkdir('/vsimem/test', 0)
    gdal.FileFromMemBuffer('/vsimem/test/foo', 'foo\n')

    tab = [ -1 ]
    handler = webserver.SequentialHandler()
    handler.add('GET', '/azure/blob/myaccount/test_bucket?comp=list&prefix=test%2F&restype=container', 200,
                {'Content-type': 'application/xml'},
                """<?xml version="1.0" encoding="UTF-8"?>
                    <EnumerationResults>
                        <Prefix></Prefix>
                        <Blobs/>
                    </EnumerationResults>
                """)
    handler.add('HEAD', '/azure/blob/myaccount/test_bucket/test', 404)
    handler.add('GET', '/azure/blob/myaccount/test_bucket?comp=list&delimiter=%2F&maxresults=1&prefix=test%2F&restype=container', 200,
                {'Content-type': 'application/xml'},
                """<?xml version="1.0" encoding="UTF-8"?>
                    <EnumerationResults>
                        <Prefix>test/</Prefix>
                        <Blobs/>
                    </EnumerationResults>
                """)
    handler.add('HEAD', '/azure/blob/myaccount/test_bucket', 404)
    handler.add('GET', '/azure/blob/myaccount/test_bucket?comp=list&delimiter=%2F&maxresults=1&restype=container', 200,
                {'Content-type': 'application/xml'},
                """<?xml version="1.0" encoding="UTF-8"?>
                    <EnumerationResults>
                        <Prefix></Prefix>
                        <Blobs>
                            <BlobPrefix>
                                <Name>something</Name>
                            </BlobPrefix>
                        </Blobs>
                    </EnumerationResults>
                """)
    handler.add('HEAD', '/azure/blob/myaccount/test_bucket/test/', 404)
    handler.add('GET', '/azure/blob/myaccount/test_bucket?comp=list&delimiter=%2F&maxresults=1&prefix=test%2F&restype=container', 200,
                {'Content-type': 'application/xml'},
                """<?xml version="1.0" encoding="UTF-8"?>
                    <EnumerationResults>
                        <Prefix>test/</Prefix>
                        <Blobs/>
                    </EnumerationResults>
                """)
    handler.add('PUT', '/azure/blob/myaccount/test_bucket/test/.gdal_marker_for_dir', 201)

    # Simulate an existing blob of another type
    handler.add('PUT', '/azure/blob/myaccount/test_bucket/test/foo?blockid=000000000001&comp=block',
                409,
                expected_headers={'Content-Length': '3'})

    handler.add('DELETE', '/azure/blob/myaccount/test_bucket/test/foo', 202)

    handler.add('PUT', '/azure/blob/myaccount/test_bucket/test/foo?blockid=000000000001&comp=block',
                201,
                expected_headers={'Content-Length': '3'})

    handler.add('PUT', '/azure/blob/myaccount/test_bucket/test/foo?blockid=000000000002&comp=block',
                201,
                expected_headers={'Content-Length': '1'})

    def method(request):
        h = request.headers
        if 'Content-Length' not in h or h['Content-Length'] != '124':
            sys.stderr.write('Bad headers: %s\n' % str(h))
            request.send_response(403)
            return

        request.protocol_version = 'HTTP/1.1'
        request.wfile.write('HTTP/1.1 100 Continue\r\n\r\n'.encode('ascii'))
        content = request.rfile.read(124).decode('ascii')
        if content != """<?xml version="1.0" encoding="utf-8"?>
<BlockList>
<Latest>000000000001</Latest>
<Latest>000000000002</Latest>
</BlockList>
""":
            sys.stderr.write('Bad content: %s\n' % str(content))
            request.send_response(403)
            request.send_header('Content-Length', 0)
            request.end_headers()
            return
        request.send_response(201)
        request.send_header('Content-Length', 0)
        request.end_headers()

    handler.add('PUT', '/azure/blob/myaccount/test_bucket/test/foo?comp=blocklist',
                custom_method = method)

    def cbk(pct, _, tab):
        assert pct >= tab[0]
        tab[0] = pct
        return True

    with gdaltest.config_option('VSIS3_SIMULATE_THREADING', 'YES'):
        with webserver.install_http_handler(handler):
            assert gdal.Sync('/vsimem/test',
                             '/vsiaz/test_bucket',
                             options=['NUM_THREADS=1', 'CHUNK_SIZE=3'],
                             callback=cbk, callback_data=tab)
    assert tab[0] == 1.0

    gdal.RmdirRecursive('/vsimem/test')


###############################################################################
# Test Sync() and multithreaded download of a single file


def test_vsiaz_fake_sync_multithreaded_upload_single_file():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    gdal.Mkdir('/vsimem/test', 0)
    gdal.FileFromMemBuffer('/vsimem/test/foo', 'foo\n')

    handler = webserver.SequentialHandler()
    handler.add('HEAD', '/azure/blob/myaccount/test_bucket', 404)
    handler.add('GET', '/azure/blob/myaccount/test_bucket?comp=list&delimiter=%2F&maxresults=1&restype=container', 200,
            {'Content-type': 'application/xml'},
            """<?xml version="1.0" encoding="UTF-8"?>
                <EnumerationResults>
                    <Prefix></Prefix>
                    <Blobs>
                        <BlobPrefix>
                            <Name>something</Name>
                        </BlobPrefix>
                    </Blobs>
                </EnumerationResults>
            """)
    handler.add('HEAD', '/azure/blob/myaccount/test_bucket/foo', 404)
    handler.add('GET', '/azure/blob/myaccount/test_bucket?comp=list&delimiter=%2F&maxresults=1&prefix=foo%2F&restype=container', 200)

    handler.add('PUT', '/azure/blob/myaccount/test_bucket/foo?blockid=000000000001&comp=block',
                201,
                expected_headers={'Content-Length': '3'})

    handler.add('PUT', '/azure/blob/myaccount/test_bucket/foo?blockid=000000000002&comp=block',
                201,
                expected_headers={'Content-Length': '1'})

    def method(request):
        h = request.headers
        if 'Content-Length' not in h or h['Content-Length'] != '124':
            sys.stderr.write('Bad headers: %s\n' % str(h))
            request.send_response(403)
            return

        request.protocol_version = 'HTTP/1.1'
        request.wfile.write('HTTP/1.1 100 Continue\r\n\r\n'.encode('ascii'))
        content = request.rfile.read(124).decode('ascii')
        if content != """<?xml version="1.0" encoding="utf-8"?>
<BlockList>
<Latest>000000000001</Latest>
<Latest>000000000002</Latest>
</BlockList>
""":
            sys.stderr.write('Bad content: %s\n' % str(content))
            request.send_response(403)
            request.send_header('Content-Length', 0)
            request.end_headers()
            return
        request.send_response(201)
        request.send_header('Content-Length', 0)
        request.end_headers()

    handler.add('PUT', '/azure/blob/myaccount/test_bucket/foo?comp=blocklist',
                custom_method = method)

    with gdaltest.config_option('VSIS3_SIMULATE_THREADING', 'YES'):
        with webserver.install_http_handler(handler):
            assert gdal.Sync('/vsimem/test/foo',
                             '/vsiaz/test_bucket',
                             options=['NUM_THREADS=1', 'CHUNK_SIZE=3'])

    gdal.RmdirRecursive('/vsimem/test')

###############################################################################
# Read credentials from simulated Azure VM


def test_vsiaz_read_credentials_simulated_azure_vm():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    with gdaltest.config_options({'AZURE_STORAGE_CONNECTION_STRING' : '',
                                  'AZURE_STORAGE_ACCOUNT': 'myaccount',
                                  'CPL_AZURE_ENDPOINT' : '127.0.0.1:%d' % gdaltest.webserver_port,
                                  'CPL_AZURE_USE_HTTPS': 'NO',
                                  'CPL_AZURE_VM_API_ROOT_URL': 'http://localhost:%d' % gdaltest.webserver_port}):

        handler = webserver.SequentialHandler()
        handler.add('GET', '/metadata/identity/oauth2/token?api-version=2018-02-01&resource=https%3A%2F%2Fstorage.azure.com%2F', 200, {},
                    """{
                    "access_token": "my_bearer",
                    "expires_on": "99999999999",
                    }""",
                    expected_headers={'Metadata': 'true'})

        handler.add('GET', '/azure/blob/myaccount/az_fake_bucket/resource', 200,
                    {'Content-Length': 3},
                    'foo',
                    expected_headers={'Authorization': 'Bearer my_bearer', 'x-ms-version': '2019-12-12'})
        with webserver.install_http_handler(handler):
            f = open_for_read('/vsiaz/az_fake_bucket/resource')
            assert f is not None
            data = gdal.VSIFReadL(1, 4, f).decode('ascii')
            gdal.VSIFCloseL(f)

        assert data == 'foo'

    # Set a fake URL to check that credentials re-use works
    with gdaltest.config_options({'AZURE_STORAGE_CONNECTION_STRING' : '',
                                  'AZURE_STORAGE_ACCOUNT': 'myaccount',
                                  'CPL_AZURE_ENDPOINT' : '127.0.0.1:%d' % gdaltest.webserver_port,
                                  'CPL_AZURE_USE_HTTPS': 'NO',
                                  'CPL_AZURE_VM_API_ROOT_URL': 'invalid'}):

        handler = webserver.SequentialHandler()
        handler.add('GET', '/azure/blob/myaccount/az_fake_bucket/bar', 200,
                    {'Content-Length': 3},
                    'bar',
                    expected_headers={'Authorization': 'Bearer my_bearer', 'x-ms-version': '2019-12-12'})
        with webserver.install_http_handler(handler):
            f = open_for_read('/vsiaz/az_fake_bucket/bar')
            assert f is not None
            data = gdal.VSIFReadL(1, 4, f).decode('ascii')
            gdal.VSIFCloseL(f)

        assert data == 'bar'

###############################################################################
# Read credentials from simulated Azure VM with expiration


def test_vsiaz_read_credentials_simulated_azure_vm_expiration():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    with gdaltest.config_options({'AZURE_STORAGE_CONNECTION_STRING' : '',
                                  'AZURE_STORAGE_ACCOUNT': 'myaccount',
                                  'CPL_AZURE_ENDPOINT' : '127.0.0.1:%d' % gdaltest.webserver_port,
                                  'CPL_AZURE_USE_HTTPS': 'NO',
                                  'CPL_AZURE_VM_API_ROOT_URL': 'http://localhost:%d' % gdaltest.webserver_port}):

        handler = webserver.SequentialHandler()
        handler.add('GET', '/metadata/identity/oauth2/token?api-version=2018-02-01&resource=https%3A%2F%2Fstorage.azure.com%2F', 200, {},
                    """{
                    "access_token": "my_bearer",
                    "expires_on": "1000",
                    }""",
                    expected_headers={'Metadata': 'true'})
        # Credentials requested again since they are expired
        handler.add('GET', '/metadata/identity/oauth2/token?api-version=2018-02-01&resource=https%3A%2F%2Fstorage.azure.com%2F', 200, {},
                    """{
                    "access_token": "my_bearer",
                    "expires_on": "1000",
                    }""",
                    expected_headers={'Metadata': 'true'})
        handler.add('GET', '/azure/blob/myaccount/az_fake_bucket/resource', 200,
                    {'Content-Length': 3},
                    'foo',
                    expected_headers={'Authorization': 'Bearer my_bearer', 'x-ms-version': '2019-12-12'})
        with webserver.install_http_handler(handler):
            f = open_for_read('/vsiaz/az_fake_bucket/resource')
            assert f is not None
            data = gdal.VSIFReadL(1, 4, f).decode('ascii')
            gdal.VSIFCloseL(f)

        assert data == 'foo'


###############################################################################
# Test GetFileMetadata () / SetFileMetadata()


def test_vsiaz_fake_metadata():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add('HEAD', '/azure/blob/myaccount/test/foo.bin', 200, {'Content-Length': '3', 'x-ms-foo': 'bar'})
    with webserver.install_http_handler(handler):
        md = gdal.GetFileMetadata('/vsiaz/test/foo.bin', 'HEADERS')
        assert 'x-ms-foo' in md
        assert md['x-ms-foo'] == 'bar'

    handler = webserver.SequentialHandler()
    handler.add('GET', '/azure/blob/myaccount/test/foo.bin?comp=metadata', 200, {'x-ms-meta-foo': 'bar'})
    with webserver.install_http_handler(handler):
        md = gdal.GetFileMetadata('/vsiaz/test/foo.bin', 'METADATA')
        assert 'x-ms-meta-foo' in md
        assert md['x-ms-meta-foo'] == 'bar'

    handler = webserver.SequentialHandler()
    handler.add('GET', '/azure/blob/myaccount/test/foo.bin?comp=tags', 200, {},
                """<Tags><TagSet><Tag><Key>foo</Key><Value>bar</Value></Tag></TagSet></Tags>""")
    with webserver.install_http_handler(handler):
        md = gdal.GetFileMetadata('/vsiaz/test/foo.bin', 'TAGS')
        assert 'foo' in md
        assert md['foo'] == 'bar'

    # Error case
    handler = webserver.SequentialHandler()
    handler.add('GET', '/azure/blob/myaccount/test/foo.bin?comp=metadata', 404)
    with webserver.install_http_handler(handler):
        assert gdal.GetFileMetadata('/vsiaz/test/foo.bin', 'METADATA') == {}

    # SetMetadata()
    handler = webserver.SequentialHandler()
    handler.add('PUT', '/azure/blob/myaccount/test/foo.bin?comp=properties', 200, expected_headers={'x-ms-foo': 'bar'})
    with webserver.install_http_handler(handler):
        assert gdal.SetFileMetadata('/vsiaz/test/foo.bin', {'x-ms-foo': 'bar'}, 'PROPERTIES')

    handler = webserver.SequentialHandler()
    handler.add('PUT', '/azure/blob/myaccount/test/foo.bin?comp=metadata', 200, expected_headers={'x-ms-meta-foo': 'bar'})
    with webserver.install_http_handler(handler):
        assert gdal.SetFileMetadata('/vsiaz/test/foo.bin', {'x-ms-meta-foo': 'bar'}, 'METADATA')

    handler = webserver.SequentialHandler()
    handler.add('PUT', '/azure/blob/myaccount/test/foo.bin?comp=tags', 204, expected_body=b'')
    with webserver.install_http_handler(handler):
        assert gdal.SetFileMetadata('/vsiaz/test/foo.bin', {'FOO': 'BAR'}, 'TAGS')

    # Error case
    handler = webserver.SequentialHandler()
    handler.add('PUT', '/azure/blob/myaccount/test/foo.bin?comp=metadata', 404)
    with webserver.install_http_handler(handler):
        assert not gdal.SetFileMetadata('/vsiaz/test/foo.bin', {'x-ms-meta-foo': 'bar'}, 'METADATA')
