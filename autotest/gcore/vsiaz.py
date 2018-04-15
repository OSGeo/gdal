#!/usr/bin/env python
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

import stat
import sys
from osgeo import gdal

sys.path.append('../pymod')

import gdaltest
import webserver


def open_for_read(uri):
    """
    Opens a test file for reading.
    """
    return gdal.VSIFOpenExL(uri, 'rb', 1)

###############################################################################


def vsiaz_init():

    gdaltest.az_vars = {}
    for var in ('AZURE_STORAGE_CONNECTION_STRING', 'AZURE_STORAGE_ACCOUNT',
                'AZURE_STORAGE_ACCESS_KEY'):
        gdaltest.az_vars[var] = gdal.GetConfigOption(var)
        if gdaltest.az_vars[var] is not None:
            gdal.SetConfigOption(var, "")

    if gdal.GetSignedURL('/vsiaz/foo/bar') is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Error cases


def vsiaz_real_server_errors():

    if not gdaltest.built_against_curl():
        return 'skip'

    # Missing AZURE_STORAGE_ACCOUNT
    gdal.ErrorReset()
    with gdaltest.error_handler():
        f = open_for_read('/vsiaz/foo/bar')
    if f is not None or gdal.VSIGetLastErrorMsg().find('AZURE_STORAGE_ACCOUNT') < 0:
        gdaltest.post_reason('fail')
        print(gdal.VSIGetLastErrorMsg())
        return 'fail'

    gdal.ErrorReset()
    with gdaltest.error_handler():
        f = open_for_read('/vsiaz_streaming/foo/bar')
    if f is not None or gdal.VSIGetLastErrorMsg().find('AZURE_STORAGE_ACCOUNT') < 0:
        gdaltest.post_reason('fail')
        print(gdal.VSIGetLastErrorMsg())
        return 'fail'

    # Invalid AZURE_STORAGE_CONNECTION_STRING
    gdal.SetConfigOption('AZURE_STORAGE_CONNECTION_STRING', 'invalid')
    gdal.ErrorReset()
    with gdaltest.error_handler():
        f = open_for_read('/vsiaz/foo/bar')
    if f is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.SetConfigOption('AZURE_STORAGE_CONNECTION_STRING', '')

    gdal.SetConfigOption('AZURE_STORAGE_ACCOUNT', 'AZURE_STORAGE_ACCOUNT')

    # Missing AZURE_STORAGE_ACCESS_KEY
    gdal.ErrorReset()
    with gdaltest.error_handler():
        f = open_for_read('/vsiaz/foo/bar')
    if f is not None or gdal.VSIGetLastErrorMsg().find('AZURE_STORAGE_ACCESS_KEY') < 0:
        gdaltest.post_reason('fail')
        print(gdal.VSIGetLastErrorMsg())
        return 'fail'

    gdal.SetConfigOption('AZURE_STORAGE_ACCESS_KEY', 'AZURE_STORAGE_ACCESS_KEY')

    gdal.ErrorReset()
    with gdaltest.error_handler():
        f = open_for_read('/vsiaz/foo/bar.baz')
    if f is not None:
        if f is not None:
            gdal.VSIFCloseL(f)
        if gdal.GetConfigOption('APPVEYOR') is not None:
            return 'success'
        gdaltest.post_reason('fail')
        print(gdal.VSIGetLastErrorMsg())
        return 'fail'

    gdal.ErrorReset()
    with gdaltest.error_handler():
        f = open_for_read('/vsiaz_streaming/foo/bar.baz')
    if f is not None:
        gdaltest.post_reason('fail')
        print(gdal.VSIGetLastErrorMsg())
        return 'fail'

    return 'success'

###############################################################################


def vsiaz_start_webserver():

    gdaltest.webserver_process = None
    gdaltest.webserver_port = 0

    if not gdaltest.built_against_curl():
        return 'skip'

    (gdaltest.webserver_process, gdaltest.webserver_port) = webserver.launch(handler=webserver.DispatcherHttpHandler)
    if gdaltest.webserver_port == 0:
        return 'skip'

    gdal.SetConfigOption('AZURE_STORAGE_CONNECTION_STRING',
                         'DefaultEndpointsProtocol=http;AccountName=myaccount;AccountKey=MY_ACCOUNT_KEY;EndpointSuffix=127.0.0.1:%d' % gdaltest.webserver_port)
    gdal.SetConfigOption('AZURE_STORAGE_ACCOUNT', '')
    gdal.SetConfigOption('AZURE_STORAGE_ACCESS_KEY', '')
    gdal.SetConfigOption('CPL_AZURE_TIMESTAMP', 'my_timestamp')

    return 'success'

###############################################################################
# Test with a fake Azure Blob server


def vsiaz_fake_basic():

    if gdaltest.webserver_port == 0:
        return 'skip'

    signed_url = gdal.GetSignedURL('/vsiaz/az_fake_bucket/resource', ['START_DATE=20180213T123456'])
    if signed_url not in ('http://127.0.0.1:8080/azure/blob/myaccount/az_fake_bucket/resource?se=2018-02-13T13%3A34%3A56Z&sig=9Jc4yBFlSRZSSxf059OohN6pYRrjuHWJWSEuryczN%2FM%3D&sp=r&sr=c&st=2018-02-13T12%3A34%3A56Z&sv=2012-02-12',
                          'http://127.0.0.1:8081/azure/blob/myaccount/az_fake_bucket/resource?se=2018-02-13T13%3A34%3A56Z&sig=9Jc4yBFlSRZSSxf059OohN6pYRrjuHWJWSEuryczN%2FM%3D&sp=r&sr=c&st=2018-02-13T12%3A34%3A56Z&sv=2012-02-12'):
        gdaltest.post_reason('fail')
        print(signed_url)
        return 'fail'

    def method(request):

        request.protocol_version = 'HTTP/1.1'
        h = request.headers
        if 'Authorization' not in h or \
           h['Authorization'] != 'SharedKey myaccount:zKb0EXnM/RinBjcUE9EU+qfRIGaIItoUElSWc+FE24E=' or \
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
        if f is None:
            gdaltest.post_reason('fail')
            return 'fail'
        data = gdal.VSIFReadL(1, 4, f).decode('ascii')
        gdal.VSIFCloseL(f)

    if data != 'foo':
        gdaltest.post_reason('fail')
        print(data)
        return 'fail'

    def method(request):

        request.protocol_version = 'HTTP/1.1'
        h = request.headers
        if 'Authorization' not in h or \
           h['Authorization'] != 'SharedKey myaccount:8d6IEeOsl7qGpKAxaTTxx2xMNpvqWq8DGlFE67lsmQ4=' or \
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
        if f is None:
            gdaltest.post_reason('fail')
            return 'fail'
        data = gdal.VSIFReadL(1, 4, f).decode('ascii')
        gdal.VSIFCloseL(f)

        if data != 'foo':
            gdaltest.post_reason('fail')
            print(data)
            return 'fail'

    handler = webserver.SequentialHandler()
    handler.add('HEAD', '/azure/blob/myaccount/az_fake_bucket/resource2.bin', 200,
                {'Content-Length': '1000000'})
    with webserver.install_http_handler(handler):
        stat_res = gdal.VSIStatL('/vsiaz/az_fake_bucket/resource2.bin')
        if stat_res is None or stat_res.size != 1000000:
            gdaltest.post_reason('fail')
            if stat_res is not None:
                print(stat_res.size)
            else:
                print(stat_res)
            return 'fail'

    handler = webserver.SequentialHandler()
    handler.add('HEAD', '/azure/blob/myaccount/az_fake_bucket/resource2.bin', 200,
                {'Content-Length': 1000000})
    with webserver.install_http_handler(handler):
        stat_res = gdal.VSIStatL('/vsiaz_streaming/az_fake_bucket/resource2.bin')
        if stat_res is None or stat_res.size != 1000000:
            gdaltest.post_reason('fail')
            if stat_res is not None:
                print(stat_res.size)
            else:
                print(stat_res)
            return 'fail'

    return 'success'

###############################################################################
# Test ReadDir() with a fake Azure Blob server


def vsiaz_fake_readdir():

    if gdaltest.webserver_port == 0:
        return 'skip'

    handler = webserver.SequentialHandler()
    handler.add('GET', '/azure/blob/myaccount/az_fake_bucket2?comp=list&delimiter=%2F&prefix=a_dir%2F&restype=container', 200,
                {'Content-type': 'application/xml'},
                """<?xml version="1.0" encoding="UTF-8"?>
                    <EnumerationResults>
                        <Prefix>a_dir/</Prefix>
                        <NextMarker>bla</NextMarker>
                        <Blobs>
                          <Blob>
                            <Name>a_dir/resource3.bin</Name>
                            <Properties>
                              <Last-Modified>01 Jan 1970 00:00:01</Last-Modified>
                              <Content-Length>123456</Content-Length>
                            </Properties>
                          </Blob>
                        </Blobs>
                    </EnumerationResults>
                """)
    handler.add('GET', '/azure/blob/myaccount/az_fake_bucket2?comp=list&delimiter=%2F&marker=bla&prefix=a_dir%2F&restype=container', 200,
                {'Content-type': 'application/xml'},
                """<?xml version="1.0" encoding="UTF-8"?>
                    <EnumerationResults>
                        <Prefix>a_dir/</Prefix>
                        <Blobs>
                          <Blob>
                            <Name>a_dir/resource4.bin</Name>
                            <Properties>
                              <Last-Modified>16 Oct 2016 12:34:56</Last-Modified>
                              <Content-Length>456789</Content-Length>
                            </Properties>
                          </Blob>
                          <BlobPrefix>
                            <Name>a_dir/subdir/</Name>
                          </BlobPrefix>
                        </Blobs>
                    </EnumerationResults>
                """)

    with webserver.install_http_handler(handler):
        f = open_for_read('/vsiaz/az_fake_bucket2/a_dir/resource3.bin')
    if f is None:

        if gdaltest.is_travis_branch('trusty'):
            print('Skipped on trusty branch, but should be investigated')
            return 'skip'

        gdaltest.post_reason('fail')
        return 'fail'
    gdal.VSIFCloseL(f)

    dir_contents = gdal.ReadDir('/vsiaz/az_fake_bucket2/a_dir')
    if dir_contents != ['resource3.bin', 'resource4.bin', 'subdir']:
        gdaltest.post_reason('fail')
        print(dir_contents)
        return 'fail'
    if gdal.VSIStatL('/vsiaz/az_fake_bucket2/a_dir/resource3.bin').size != 123456:
        gdaltest.post_reason('fail')
        print(gdal.VSIStatL('/vsiaz/az_fake_bucket2/a_dir/resource3.bin').size)
        return 'fail'
    if gdal.VSIStatL('/vsiaz/az_fake_bucket2/a_dir/resource3.bin').mtime != 1:
        gdaltest.post_reason('fail')
        return 'fail'

    # ReadDir on something known to be a file shouldn't cause network access
    dir_contents = gdal.ReadDir('/vsiaz/az_fake_bucket2/a_dir/resource3.bin')
    if dir_contents is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Test error on ReadDir()
    handler = webserver.SequentialHandler()
    handler.add('GET', '/azure/blob/myaccount/az_fake_bucket2?comp=list&delimiter=%2F&prefix=error_test%2F&restype=container', 500)
    with webserver.install_http_handler(handler):
        dir_contents = gdal.ReadDir('/vsiaz/az_fake_bucket2/error_test/')
    if dir_contents is not None:
        gdaltest.post_reason('fail')
        print(dir_contents)
        return 'fail'

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
    if dir_contents != ['.']:
        gdaltest.post_reason('fail')
        print(dir_contents)
        return 'fail'

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
    if dir_contents != ['mycontainer1', 'mycontainer2']:
        gdaltest.post_reason('fail')
        print(dir_contents)
        return 'fail'

    return 'success'

###############################################################################
# Test write


def vsiaz_fake_write():

    if gdaltest.webserver_port == 0:
        return 'skip'

    gdal.VSICurlClearCache()

    # Test creation of BlockBob
    f = gdal.VSIFOpenL('/vsiaz/test_copy/file.bin', 'wb')
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'

    handler = webserver.SequentialHandler()

    def method(request):
        h = request.headers
        if 'Authorization' not in h or \
           h['Authorization'] != 'SharedKey myaccount:AigkrY7q66WCrx3JRKBte56k7kxV2cxB/ZyGNubxk5I=' or \
           'Expect' not in h or h['Expect'] != '100-continue' or \
           'Content-Length' not in h or h['Content-Length'] != '40000' or \
           'x-ms-date' not in h or h['x-ms-date'] != 'my_timestamp' or \
           'x-ms-blob-type' not in h or h['x-ms-blob-type'] != 'BlockBlob':
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

    handler.add('PUT', '/azure/blob/myaccount/test_copy/file.bin', custom_method=method)
    with webserver.install_http_handler(handler):
        ret = gdal.VSIFWriteL('x' * 35000, 1, 35000, f)
        ret += gdal.VSIFWriteL('x' * 5000, 1, 5000, f)
        if ret != 40000:
            gdaltest.post_reason('fail')
            print(ret)
            gdal.VSIFCloseL(f)
            return 'fail'
        gdal.VSIFCloseL(f)

    # Simulate illegal read
    f = gdal.VSIFOpenL('/vsiaz/test_copy/file.bin', 'wb')
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'
    with gdaltest.error_handler():
        ret = gdal.VSIFReadL(1, 1, f)
    if len(ret) != 0:
        gdaltest.post_reason('fail')
        print(ret)
        return 'fail'
    gdal.VSIFCloseL(f)

    # Simulate illegal seek
    f = gdal.VSIFOpenL('/vsiaz/test_copy/file.bin', 'wb')
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'
    with gdaltest.error_handler():
        ret = gdal.VSIFSeekL(f, 1, 0)
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.VSIFCloseL(f)

    # Simulate failure when putting BlockBob
    f = gdal.VSIFOpenL('/vsiaz/test_copy/file.bin', 'wb')
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'

    handler = webserver.SequentialHandler()

    def method(request):
        request.protocol_version = 'HTTP/1.1'
        request.send_response(403)
        request.send_header('Content-Length', 0)
        request.end_headers()

    handler.add('PUT', '/azure/blob/myaccount/test_copy/file.bin', custom_method=method)

    if gdal.VSIFSeekL(f, 0, 0) != 0:
        gdaltest.post_reason('fail')
        gdal.VSIFCloseL(f)
        return 'fail'

    gdal.VSIFWriteL('x' * 35000, 1, 35000, f)

    if gdal.VSIFTellL(f) != 35000:
        gdaltest.post_reason('fail')
        gdal.VSIFCloseL(f)
        return 'fail'

    if gdal.VSIFSeekL(f, 35000, 0) != 0:
        gdaltest.post_reason('fail')
        gdal.VSIFCloseL(f)
        return 'fail'

    if gdal.VSIFSeekL(f, 0, 1) != 0:
        gdaltest.post_reason('fail')
        gdal.VSIFCloseL(f)
        return 'fail'
    if gdal.VSIFSeekL(f, 0, 2) != 0:
        gdaltest.post_reason('fail')
        gdal.VSIFCloseL(f)
        return 'fail'

    if gdal.VSIFEofL(f) != 0:
        gdaltest.post_reason('fail')
        gdal.VSIFCloseL(f)
        return 'fail'

    with webserver.install_http_handler(handler):
        with gdaltest.error_handler():
            ret = gdal.VSIFCloseL(f)
        if ret == 0:
            gdaltest.post_reason('fail')
            print(ret)
            gdal.VSIFCloseL(f)
            return 'fail'

    # Simulate creation of BlockBob over an existing blob of incompatible type
    f = gdal.VSIFOpenL('/vsiaz/test_copy/file.bin', 'wb')
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'

    handler = webserver.SequentialHandler()
    handler.add('PUT', '/azure/blob/myaccount/test_copy/file.bin', 409)
    handler.add('DELETE', '/azure/blob/myaccount/test_copy/file.bin', 202)
    handler.add('PUT', '/azure/blob/myaccount/test_copy/file.bin', 201)
    with webserver.install_http_handler(handler):
        gdal.VSIFCloseL(f)

    # Test creation of AppendBlob
    gdal.SetConfigOption('VSIAZ_CHUNK_SIZE_BYTES', '10')
    f = gdal.VSIFOpenL('/vsiaz/test_copy/file.bin', 'wb')
    gdal.SetConfigOption('VSIAZ_CHUNK_SIZE_BYTES', None)
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'

    handler = webserver.SequentialHandler()

    def method(request):
        h = request.headers
        if 'Authorization' not in h or \
           h['Authorization'] != 'SharedKey myaccount:KimVui3ptY9D5ftLlsI7CNOgK36CNAEzsXqcuHskdEY=' or \
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

    handler.add('PUT', '/azure/blob/myaccount/test_copy/file.bin', custom_method=method)

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

    handler.add('PUT', '/azure/blob/myaccount/test_copy/file.bin?comp=appendblock', custom_method=method)

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

    handler.add('PUT', '/azure/blob/myaccount/test_copy/file.bin?comp=appendblock', custom_method=method)

    with webserver.install_http_handler(handler):
        ret = gdal.VSIFWriteL('0123456789abcdef', 1, 16, f)
        if ret != 16:
            gdaltest.post_reason('fail')
            print(ret)
            gdal.VSIFCloseL(f)
            return 'fail'
        gdal.VSIFCloseL(f)

    # Test failed creation of AppendBlob
    gdal.SetConfigOption('VSIAZ_CHUNK_SIZE_BYTES', '10')
    f = gdal.VSIFOpenL('/vsiaz/test_copy/file.bin', 'wb')
    gdal.SetConfigOption('VSIAZ_CHUNK_SIZE_BYTES', None)
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'

    handler = webserver.SequentialHandler()

    def method(request):
        request.protocol_version = 'HTTP/1.1'
        request.send_response(403)
        request.send_header('Content-Length', 0)
        request.end_headers()

    handler.add('PUT', '/azure/blob/myaccount/test_copy/file.bin', custom_method=method)

    with webserver.install_http_handler(handler):
        with gdaltest.error_handler():
            ret = gdal.VSIFWriteL('0123456789abcdef', 1, 16, f)
        if ret != 0:
            gdaltest.post_reason('fail')
            print(ret)
            gdal.VSIFCloseL(f)
            return 'fail'
        gdal.VSIFCloseL(f)

    # Test failed writing of a block of an AppendBlob
    gdal.SetConfigOption('VSIAZ_CHUNK_SIZE_BYTES', '10')
    f = gdal.VSIFOpenL('/vsiaz/test_copy/file.bin', 'wb')
    gdal.SetConfigOption('VSIAZ_CHUNK_SIZE_BYTES', None)
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'

    handler = webserver.SequentialHandler()
    handler.add('PUT', '/azure/blob/myaccount/test_copy/file.bin', 201)
    handler.add('PUT', '/azure/blob/myaccount/test_copy/file.bin?comp=appendblock', 403)
    with webserver.install_http_handler(handler):
        with gdaltest.error_handler():
            ret = gdal.VSIFWriteL('0123456789abcdef', 1, 16, f)
        if ret != 0:
            gdaltest.post_reason('fail')
            print(ret)
            gdal.VSIFCloseL(f)
            return 'fail'
        gdal.VSIFCloseL(f)

    return 'success'

###############################################################################
# Test Unlink()


def vsiaz_fake_unlink():

    if gdaltest.webserver_port == 0:
        return 'skip'

    # Success
    handler = webserver.SequentialHandler()
    handler.add('HEAD', '/azure/blob/myaccount/az_bucket_test_unlink/myfile', 200, {'Content-Length': '1'})
    handler.add('DELETE', '/azure/blob/myaccount/az_bucket_test_unlink/myfile', 202, {'Connection': 'close'})
    with webserver.install_http_handler(handler):
        ret = gdal.Unlink('/vsiaz/az_bucket_test_unlink/myfile')
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    # Failure
    handler = webserver.SequentialHandler()
    handler.add('HEAD', '/azure/blob/myaccount/az_bucket_test_unlink/myfile', 200, {'Content-Length': '1'})
    handler.add('DELETE', '/azure/blob/myaccount/az_bucket_test_unlink/myfile', 400, {'Connection': 'close'})
    with webserver.install_http_handler(handler):
        with gdaltest.error_handler():
            ret = gdal.Unlink('/vsiaz/az_bucket_test_unlink/myfile')
    if ret != -1:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Test Mkdir() / Rmdir()


def vsiaz_fake_mkdir_rmdir():

    if gdaltest.webserver_port == 0:
        return 'skip'

    # Invalid name
    ret = gdal.Mkdir('/vsiaz', 0)
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    handler = webserver.SequentialHandler()
    handler.add('HEAD', '/azure/blob/myaccount/az_bucket_test_mkdir/dir/', 404, {'Connection': 'close'})
    handler.add('GET', '/azure/blob/myaccount/az_bucket_test_mkdir?comp=list&delimiter=%2F&maxresults=1&prefix=dir%2F&restype=container', 200, {'Connection': 'close'})
    handler.add('PUT', '/azure/blob/myaccount/az_bucket_test_mkdir/dir/.gdal_marker_for_dir', 201)
    with webserver.install_http_handler(handler):
        ret = gdal.Mkdir('/vsiaz/az_bucket_test_mkdir/dir', 0)
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'

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
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    # Invalid name
    ret = gdal.Rmdir('/vsiaz')
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    # Not a directory
    handler = webserver.SequentialHandler()
    handler.add('HEAD', '/azure/blob/myaccount/az_bucket_test_mkdir/it_is_a_file/', 404)
    handler.add('GET', '/azure/blob/myaccount/az_bucket_test_mkdir?comp=list&delimiter=%2F&maxresults=1&prefix=it_is_a_file%2F&restype=container',
                200,
                {'Connection': 'close', 'Content-type': 'application/xml'},
                """<?xml version="1.0" encoding="UTF-8"?>
                    <EnumerationResults>
                        <Prefix>it_is_a_file/</Prefix>
                    </EnumerationResults>
                """)
    with webserver.install_http_handler(handler):
        ret = gdal.Rmdir('/vsiaz/az_bucket_test_mkdir/it_is_a_file')
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

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
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    # Try deleting already deleted directory
    handler = webserver.SequentialHandler()
    handler.add('HEAD', '/azure/blob/myaccount/az_bucket_test_mkdir/dir/', 404)
    handler.add('GET', '/azure/blob/myaccount/az_bucket_test_mkdir?comp=list&delimiter=%2F&maxresults=1&prefix=dir%2F&restype=container', 200)
    with webserver.install_http_handler(handler):
        ret = gdal.Rmdir('/vsiaz/az_bucket_test_mkdir/dir')
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

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
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################


def vsiaz_stop_webserver():

    if gdaltest.webserver_port == 0:
        return 'skip'

    # Clearcache needed to close all connections, since the Python server
    # can only handle one connection at a time
    gdal.VSICurlClearCache()

    webserver.server_stop(gdaltest.webserver_process, gdaltest.webserver_port)

    return 'success'

###############################################################################
# Nominal cases (require valid credentials)


def vsiaz_extra_1():

    if not gdaltest.built_against_curl():
        return 'skip'

    az_resource = gdal.GetConfigOption('AZ_RESOURCE')
    if az_resource is None:
        print('Missing AZ_RESOURCE for running gdaltest_list_extra')
        return 'skip'

    if az_resource.find('/') < 0:
        path = '/vsiaz/' + az_resource
        statres = gdal.VSIStatL(path)
        if statres is None or not stat.S_ISDIR(statres.mode):
            gdaltest.post_reason('fail')
            print('%s is not a valid bucket' % path)
            return 'fail'

        readdir = gdal.ReadDir(path)
        if readdir is None:
            gdaltest.post_reason('fail')
            print('ReadDir() should not return empty list')
            return 'fail'
        for filename in readdir:
            if filename != '.':
                subpath = path + '/' + filename
                if gdal.VSIStatL(subpath) is None:
                    gdaltest.post_reason('fail')
                    print('Stat(%s) should not return an error' % subpath)
                    return 'fail'

        unique_id = 'vsiaz_test'
        subpath = path + '/' + unique_id
        ret = gdal.Mkdir(subpath, 0)
        if ret < 0:
            gdaltest.post_reason('fail')
            print('Mkdir(%s) should not return an error' % subpath)
            return 'fail'

        readdir = gdal.ReadDir(path)
        if unique_id not in readdir:
            gdaltest.post_reason('fail')
            print('ReadDir(%s) should contain %s' % (path, unique_id))
            print(readdir)
            return 'fail'

        ret = gdal.Mkdir(subpath, 0)
        if ret == 0:
            gdaltest.post_reason('fail')
            print('Mkdir(%s) repeated should return an error' % subpath)
            return 'fail'

        ret = gdal.Rmdir(subpath)
        if ret < 0:
            gdaltest.post_reason('fail')
            print('Rmdir(%s) should not return an error' % subpath)
            return 'fail'

        readdir = gdal.ReadDir(path)
        if unique_id in readdir:
            gdaltest.post_reason('fail')
            print('ReadDir(%s) should not contain %s' % (path, unique_id))
            print(readdir)
            return 'fail'

        ret = gdal.Rmdir(subpath)
        if ret == 0:
            gdaltest.post_reason('fail')
            print('Rmdir(%s) repeated should return an error' % subpath)
            return 'fail'

        ret = gdal.Mkdir(subpath, 0)
        if ret < 0:
            gdaltest.post_reason('fail')
            print('Mkdir(%s) should not return an error' % subpath)
            return 'fail'

        f = gdal.VSIFOpenL(subpath + '/test.txt', 'wb')
        if f is None:
            gdaltest.post_reason('fail')
            return 'fail'
        gdal.VSIFWriteL('hello', 1, 5, f)
        gdal.VSIFCloseL(f)

        ret = gdal.Rmdir(subpath)
        if ret == 0:
            gdaltest.post_reason('fail')
            print('Rmdir(%s) on non empty directory should return an error' % subpath)
            return 'fail'

        f = gdal.VSIFOpenL(subpath + '/test.txt', 'rb')
        if f is None:
            gdaltest.post_reason('fail')
            return 'fail'
        data = gdal.VSIFReadL(1, 5, f).decode('utf-8')
        if data != 'hello':
            gdaltest.post_reason('fail')
            print(data)
            return 'fail'
        gdal.VSIFCloseL(f)

        ret = gdal.Unlink(subpath + '/test.txt')
        if ret < 0:
            gdaltest.post_reason('fail')
            print('Unlink(%s) should not return an error' % (subpath + '/test.txt'))
            return 'fail'

        ret = gdal.Rmdir(subpath)
        if ret < 0:
            gdaltest.post_reason('fail')
            print('Rmdir(%s) should not return an error' % subpath)
            return 'fail'

        return 'success'

    f = open_for_read('/vsiaz/' + az_resource)
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'
    ret = gdal.VSIFReadL(1, 1, f)
    gdal.VSIFCloseL(f)

    if len(ret) != 1:
        gdaltest.post_reason('fail')
        print(ret)
        return 'fail'

    # Same with /vsiaz_streaming/
    f = open_for_read('/vsiaz_streaming/' + az_resource)
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'
    ret = gdal.VSIFReadL(1, 1, f)
    gdal.VSIFCloseL(f)

    if len(ret) != 1:
        gdaltest.post_reason('fail')
        print(ret)
        return 'fail'

    if False:  # we actually try to read at read() time and bSetError = false
        # Invalid bucket : "The specified bucket does not exist"
        gdal.ErrorReset()
        f = open_for_read('/vsiaz/not_existing_bucket/foo')
        with gdaltest.error_handler():
            gdal.VSIFReadL(1, 1, f)
        gdal.VSIFCloseL(f)
        if gdal.VSIGetLastErrorMsg() == '':
            gdaltest.post_reason('fail')
            print(gdal.VSIGetLastErrorMsg())
            return 'fail'

    # Invalid resource
    gdal.ErrorReset()
    f = open_for_read('/vsiaz_streaming/' + az_resource + '/invalid_resource.baz')
    if f is not None:
        gdaltest.post_reason('fail')
        print(gdal.VSIGetLastErrorMsg())
        return 'fail'

    # Test GetSignedURL()
    signed_url = gdal.GetSignedURL('/vsiaz/' + az_resource)
    f = open_for_read('/vsicurl_streaming/' + signed_url)
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'
    ret = gdal.VSIFReadL(1, 1, f)
    gdal.VSIFCloseL(f)

    if len(ret) != 1:
        gdaltest.post_reason('fail')
        print(ret)
        return 'fail'

    return 'success'

###############################################################################


def vsiaz_cleanup():

    for var in gdaltest.az_vars:
        gdal.SetConfigOption(var, gdaltest.az_vars[var])

    return 'success'


gdaltest_list = [vsiaz_init,
                 vsiaz_real_server_errors,
                 vsiaz_start_webserver,
                 vsiaz_fake_basic,
                 vsiaz_fake_readdir,
                 vsiaz_fake_write,
                 vsiaz_fake_unlink,
                 vsiaz_fake_mkdir_rmdir,
                 vsiaz_stop_webserver,
                 vsiaz_cleanup]

# gdaltest_list = [ vsiaz_init, vsiaz_start_webserver, vsiaz_fake_mkdir_rmdir, vsiaz_stop_webserver, vsiaz_cleanup ]

gdaltest_list_extra = [vsiaz_extra_1]

if __name__ == '__main__':

    gdaltest.setup_run('vsiaz')

    if gdal.GetConfigOption('RUN_MANUAL_ONLY', None):
        gdaltest.run_tests(gdaltest_list_extra)
    else:
        gdaltest.run_tests(gdaltest_list + gdaltest_list_extra + [vsiaz_cleanup])

    gdaltest.summarize()
